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

struct AvahiSimplePoll {
    AvahiPoll api;

    struct pollfd* pollfds;
    int n_pollfds, max_pollfds, rebuild_pollfds;

    struct timeval wakeup;
    int use_wakeup;

    int req_cleanup;
    
    int quit;

    int n_watches;
    AVAHI_LLIST_HEAD(AvahiWatch, watches);
};

static AvahiWatch* watch_new(AvahiPoll *api, int fd, AvahiWatchEvent event, AvahiWatchCallback callback, void *userdata) {
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
    w->pollfd.fd = fd;
    w->pollfd.events = event;
    w->callback = callback;
    w->userdata = userdata;
    w->dead = 0;

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
    w->simple_poll->req_cleanup = 1;
}

static void set_wakeup_time(AvahiPoll *api, const struct timeval *tv) {
    AvahiSimplePoll *s;

    assert(api);
    s = api->userdata;

    if (tv) {
        s->wakeup = *tv;
        s->use_wakeup = 1;
    } else
        s->use_wakeup = 0;
}

static void destroy_watch(AvahiWatch *w) {
    assert(w);

    remove_pollfd(w);
    AVAHI_LLIST_REMOVE(AvahiWatch, watches, w->simple_poll->watches, w);

    if (!w->dead)
        w->simple_poll->n_watches --;
    
    avahi_free(w);
}

static void cleanup(AvahiSimplePoll *s, int all) {
    AvahiWatch *w, *next;
    assert(s);

    for (w = s->watches; w; w = next) {
        next = w->watches_next;

        if (all || w->dead)
            destroy_watch(w);
    }

    s->req_cleanup = 0;
}

AvahiSimplePoll *avahi_simple_poll_new(void) {
    AvahiSimplePoll *s;

    if (!(s = avahi_new(AvahiSimplePoll, 1)))
        return NULL;
    
    s->api.userdata = s;
    s->api.watch_new = watch_new;
    s->api.watch_free = watch_free;
    s->api.watch_update = watch_update;
    s->api.set_wakeup_time = set_wakeup_time;
    s->pollfds = NULL;
    s->max_pollfds = s->n_pollfds = 0;
    s->use_wakeup = 0;
    s->rebuild_pollfds = 0;
    s->quit = 0;
    s->n_watches = 0;
    s->req_cleanup = 0;

    AVAHI_LLIST_HEAD_INIT(AvahiWatch, s->watches);

    return s;
}

void avahi_simple_poll_free(AvahiSimplePoll *s) {
    assert(s);

    cleanup(s, 1);
    
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
    
    s->rebuild_pollfds = 0;

    return 0;
}

int avahi_simple_poll_iterate(AvahiSimplePoll *s, int block) {
    int timeout, r, ret = 0;
    assert(s);

    if (s->quit)
        return 1;

    if (s->req_cleanup)
        cleanup(s, 0);
    
    if (s->rebuild_pollfds)
        if (rebuild(s) < 0)
            return -1;

    if (block) {
        if (s->use_wakeup) {
            struct timeval now;
            AvahiUsec usec;

            gettimeofday(&now, NULL);

            usec = avahi_timeval_diff(&s->wakeup, &now);
            
            timeout = usec <= 0 ? 0 : (int) (usec / 1000);
        } else
            timeout = -1;
    } else
        timeout = 0;

    if ((r = poll(s->pollfds, s->n_pollfds, timeout)) < 0)
        return -1;

    else if (r > 0) {
        AvahiWatch *w;

        for (w = s->watches; w; w = w->watches_next) {

            if (w->dead)
                continue;

            assert(w->idx >= 0);
            assert(w->idx < s->n_pollfds);

            if (s->pollfds[w->idx].revents > 0)
                w->callback(w, w->pollfd.fd, s->pollfds[w->idx].revents, w->userdata);

            if (s->quit) {
                ret = 1;
                goto finish;
            }
        }
    }

    ret = 0;

finish:

    if (s->req_cleanup)
        cleanup(s, 0);
    
    return ret;
}

void avahi_simple_poll_quit(AvahiSimplePoll *w) {
    assert(w);

    w->quit = 1;
}

AvahiPoll* avahi_simple_poll_get(AvahiSimplePoll *s) {
    assert(s);
    
    return &s->api;
}
