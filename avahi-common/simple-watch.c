/* $Id$ */

/***
  This file is part of avahi.
 
  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.
 
  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/poll.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <avahi-common/llist.h>
#include <avahi-common/malloc.h>

#include "simple-watch.h"

struct AvahiWatch {
    AvahiSimplePoll *simple_poll;
    int dead;

    int idx;
    struct pollfd pollfd;
    
    AvahiWatchCallback callback;
    void *userdata;

    AVAHI_LLIST_FIELDS(AvahiWatch, watches);
};

struct AvahiTimeout {
    AvahiSimplePoll *simple_poll;
    int dead;

    int enabled;
    struct timeval expiry;
    
    AvahiTimeoutCallback callback;
    void  *userdata;
    
    AVAHI_LLIST_FIELDS(AvahiTimeout, timeouts);
};

struct AvahiSimplePoll {
    AvahiPoll api;
    AvahiPollFunc poll_func;

    struct pollfd* pollfds;
    int n_pollfds, max_pollfds, rebuild_pollfds;

    int watch_req_cleanup, timeout_req_cleanup;
    int quit;
    int events_valid;

    int n_watches;
    AVAHI_LLIST_HEAD(AvahiWatch, watches);
    AVAHI_LLIST_HEAD(AvahiTimeout, timeouts);
};

static AvahiWatch* watch_new(const AvahiPoll *api, int fd, AvahiWatchEvent event, AvahiWatchCallback callback, void *userdata) {
    AvahiWatch *w;
    AvahiSimplePoll *s;
    
    assert(api);
    assert(fd >= 0);
    assert(callback);

    s = api->userdata;
    assert(s);

    if (!(w = avahi_new(AvahiWatch, 1)))
        return NULL;
    
    w->simple_poll = s;
    w->dead = 0;

    w->pollfd.fd = fd;
    w->pollfd.events = event;

    w->callback = callback;
    w->userdata = userdata;

    if (s->n_pollfds < s->max_pollfds) {
        /* If there's space for this pollfd, go on and allocate it */
        w->idx = s->n_pollfds++;
        s->pollfds[w->idx] = w->pollfd;
        
    } else {
        /* Unfortunately there's no place for this pollfd, so request a rebuild of the array */
        w->idx = -1;
        s->rebuild_pollfds = 1;
    }

    AVAHI_LLIST_PREPEND(AvahiWatch, watches, s->watches, w);
    s->n_watches++;

    return w;
}

static void watch_update(AvahiWatch *w, AvahiWatchEvent events) {
    assert(w);
    assert(!w->dead);

    w->pollfd.events = events;

    if (w->idx != -1) {
        assert(w->simple_poll);
        w->simple_poll->pollfds[w->idx] = w->pollfd;
    } else
        w->simple_poll->rebuild_pollfds = 1;
}

static AvahiWatchEvent watch_get_events(AvahiWatch *w) {
    assert(w);
    assert(!w->dead);

    if (w->idx != -1 && w->simple_poll->events_valid)
        return w->simple_poll->pollfds[w->idx].revents;

    return 0;
}

static void remove_pollfd(AvahiWatch *w) {
    assert(w);

    if (w->idx == -1)
        return;
    
    if (w->idx == w->simple_poll->n_pollfds-1) {

        /* This pollfd is at the end of the array, so we can easily cut it */

        assert(w->simple_poll->n_pollfds > 0);
        w->simple_poll->n_pollfds -= 1;
    } else

        /* Unfortunately this pollfd is in the middle of the array, so request a rebuild of it */
        w->simple_poll->rebuild_pollfds = 1;
}

static void watch_free(AvahiWatch *w) {
    assert(w);

    assert(!w->dead);

    remove_pollfd(w);
    
    w->dead = 1;
    w->simple_poll->n_watches --;
    w->simple_poll->watch_req_cleanup = 1;
}

static void destroy_watch(AvahiWatch *w) {
    assert(w);

    remove_pollfd(w);
    AVAHI_LLIST_REMOVE(AvahiWatch, watches, w->simple_poll->watches, w);

    if (!w->dead)
        w->simple_poll->n_watches --;
    
    avahi_free(w);
}

static void cleanup_watches(AvahiSimplePoll *s, int all) {
    AvahiWatch *w, *next;
    assert(s);

    for (w = s->watches; w; w = next) {
        next = w->watches_next;

        if (all || w->dead)
            destroy_watch(w);
    }

    s->timeout_req_cleanup = 0;
}

static AvahiTimeout* timeout_new(const AvahiPoll *api, const struct timeval *tv, AvahiTimeoutCallback callback, void *userdata) {
    AvahiTimeout *t;
    AvahiSimplePoll *s;
    
    assert(api);
    assert(callback);

    s = api->userdata;
    assert(s);

    if (!(t = avahi_new(AvahiTimeout, 1)))
        return NULL;
    
    t->simple_poll = s;
    t->dead = 0;

    if ((t->enabled = !!tv))
        t->expiry = *tv;
        
    t->callback = callback;
    t->userdata = userdata;

    AVAHI_LLIST_PREPEND(AvahiTimeout, timeouts, s->timeouts, t);

    return t;
}

static void timeout_update(AvahiTimeout *t, const struct timeval *tv) {
    assert(t);
    assert(!t->dead);
    
    if ((t->enabled = !!tv))
        t->expiry = *tv;
}

static void timeout_free(AvahiTimeout *t) {
    assert(t);
    assert(!t->dead);

    t->dead = 1;
    t->simple_poll->timeout_req_cleanup = 1;
}


static void destroy_timeout(AvahiTimeout *t) {
    assert(t);

    AVAHI_LLIST_REMOVE(AvahiTimeout, timeouts, t->simple_poll->timeouts, t);

    avahi_free(t);
}

static void cleanup_timeouts(AvahiSimplePoll *s, int all) {
    AvahiTimeout *t, *next;
    assert(s);

    for (t = s->timeouts; t; t = next) {
        next = t->timeouts_next;

        if (all || t->dead)
            destroy_timeout(t);
    }

    s->timeout_req_cleanup = 0;
}

AvahiSimplePoll *avahi_simple_poll_new(void) {
    AvahiSimplePoll *s;

    if (!(s = avahi_new(AvahiSimplePoll, 1)))
        return NULL;
    
    s->api.userdata = s;

    s->api.watch_new = watch_new;
    s->api.watch_free = watch_free;
    s->api.watch_update = watch_update;
    s->api.watch_get_events = watch_get_events;

    s->api.timeout_new = timeout_new;
    s->api.timeout_free = timeout_free;
    s->api.timeout_update = timeout_update;
    
    s->pollfds = NULL;
    s->max_pollfds = s->n_pollfds = 0;
    s->rebuild_pollfds = 0;
    s->quit = 0;
    s->n_watches = 0;
    s->events_valid = 0;
    
    s->watch_req_cleanup = 0;
    s->timeout_req_cleanup = 0;

    avahi_simple_poll_set_func(s, NULL);

    AVAHI_LLIST_HEAD_INIT(AvahiWatch, s->watches);
    AVAHI_LLIST_HEAD_INIT(AvahiTimeout, s->timeouts);

    return s;
}

void avahi_simple_poll_free(AvahiSimplePoll *s) {
    assert(s);

    cleanup_timeouts(s, 1);
    cleanup_watches(s, 1);
    assert(s->n_watches == 0);
    
    avahi_free(s->pollfds);
    avahi_free(s);
}

static int rebuild(AvahiSimplePoll *s) {
    AvahiWatch *w;
    int idx;
    
    assert(s);

    if (s->n_watches > s->max_pollfds) {
        struct pollfd *n;

        s->max_pollfds = s->n_watches + 10;
        
        if (!(n = avahi_realloc(s->pollfds, sizeof(struct pollfd) * s->max_pollfds)))
            return -1;

        s->pollfds = n;
    }

    for (idx = 0, w = s->watches; w; w = w->watches_next) {

        if(w->dead)
            continue;

        assert(w->idx < s->max_pollfds);
        s->pollfds[w->idx = idx++] = w->pollfd;
    }

    s->n_pollfds = idx;
    s->events_valid = 0;
    s->rebuild_pollfds = 0;

    return 0;
}

static AvahiTimeout* find_next_timeout(AvahiSimplePoll *s) {
    AvahiTimeout *t, *n = NULL;
    assert(s);

    for (t = s->timeouts; t; t = t->timeouts_next) {
        
        if (t->dead || !t->enabled)
            continue;
        
        if (!n || avahi_timeval_compare(&t->expiry, &n->expiry) < 0)
            n = t;
    }

    return n;
}

static int start_timeout_callback(AvahiTimeout *t) {
    assert(t);
    assert(!t->dead);
    assert(t->enabled);

    t->enabled = 0;
    t->callback(t, t->userdata);
    return 0;
}

int avahi_simple_poll_iterate(AvahiSimplePoll *s, int timeout) {
    int r;
    AvahiTimeout *next_timeout;
    assert(s);

    /* Cleanup things first */
    if (s->watch_req_cleanup)
        cleanup_watches(s, 0);

    if (s->timeout_req_cleanup)
        cleanup_timeouts(s, 0);

    /* Check whether a quit was requested */
    if (s->quit)
        return 1;

    /* Do we need to rebuild our array of pollfds? */
    if (s->rebuild_pollfds)
        if (rebuild(s) < 0)
            return -1;


    /* Calculate the wakeup time */
    if ((next_timeout = find_next_timeout(s))) {
        struct timeval now;
        int t;
        AvahiUsec usec;

        if (next_timeout->expiry.tv_sec == 0 &&
            next_timeout->expiry.tv_usec == 0) {

            /* Just a shortcut so that we don't need to call gettimeofday() */
            
            /* The events poll() returned in the last call are now no longer valid */
            s->events_valid = 0;
            return start_timeout_callback(next_timeout);
        }

            
        gettimeofday(&now, NULL);
        usec = avahi_timeval_diff(&next_timeout->expiry, &now);

        if (usec <= 0) {
            /* Timeout elapsed */

            /* The events poll() returned in the last call are now no longer valid */
            s->events_valid = 0;
            return start_timeout_callback(next_timeout);
        }

        /* Calculate sleep time. We add 1ms because otherwise we'd
         * wake up too early most of the time */
        t = (int) (usec / 1000) + 1;

        if (timeout < 0 || timeout > t)
            timeout = t;
    }

    if ((r = s->poll_func(s->pollfds, s->n_pollfds, timeout)) < 0)
        return -1;

    /* The pollf events are now valid again */
    s->events_valid = 1;

    /* Check whether the wakeup time has been reached now */
    if ((next_timeout = find_next_timeout(s))) {
        struct timeval now;
        
        gettimeofday(&now, NULL);

        if (avahi_timeval_compare(&next_timeout->expiry, &now) <= 0)
            /* Time elapsed */
            return start_timeout_callback(next_timeout);
    }
    
    if (r > 0) {
        AvahiWatch *w;

        /* Look for some kind of I/O event */

        for (w = s->watches; w; w = w->watches_next) {

            if (w->dead)
                continue;

            assert(w->idx >= 0);
            assert(w->idx < s->n_pollfds);

            if (s->pollfds[w->idx].revents > 0) {
                /* We execute only on callback in every iteration */
                w->callback(w, w->pollfd.fd, s->pollfds[w->idx].revents, w->userdata);
                return 0;
            }
        }
    }

    return 0;
}

void avahi_simple_poll_quit(AvahiSimplePoll *w) {
    assert(w);

    w->quit = 1;
}

const AvahiPoll* avahi_simple_poll_get(AvahiSimplePoll *s) {
    assert(s);
    
    return &s->api;
}

void avahi_simple_poll_set_func(AvahiSimplePoll *s, AvahiPollFunc func) {
    assert(s);

    s->poll_func = func ? func : (AvahiPollFunc) poll;
}
