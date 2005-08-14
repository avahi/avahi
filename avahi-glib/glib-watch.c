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

#include <avahi-common/llist.h>
#include <avahi-common/malloc.h>

#include "glib-watch.h"

struct AvahiWatch {
    AvahiGLibPoll *glib_poll;
    int dead;
    GPollFD pollfd;
    int pollfd_added;
    AvahiWatchCallback callback;
    void *userdata;

    AVAHI_LLIST_FIELDS(AvahiWatch, watches);
};

struct AvahiGLibPoll {
    GSource source;
    AvahiPoll api;
    GMainContext *context;

    struct timeval wakeup;
    AvahiWakeupCallback wakeup_callback;
    void *wakeup_userdata;
    
    int req_cleanup;
    
     AVAHI_LLIST_HEAD(AvahiWatch, watches);
};

static void destroy_watch(AvahiWatch *w) {
    assert(w);

    if (w->pollfd_added)
        g_source_remove_poll(&w->glib_poll->source, &w->pollfd);

    AVAHI_LLIST_REMOVE(AvahiWatch, watches, w->glib_poll->watches, w);

    avahi_free(w);
}

static void cleanup(AvahiGLibPoll *g, int all) {
    AvahiWatch *w, *next;
    assert(g);

    for (w = g->watches; w; w = next) {
        next = w->watches_next;

        if (all || w->dead)
            destroy_watch(w);
    }

    g->req_cleanup = 0;
}

static AvahiWatch* watch_new(const AvahiPoll *api, int fd, AvahiWatchEvent event, AvahiWatchCallback callback, void *userdata) {
    AvahiWatch *w;
    AvahiGLibPoll *g;
    
    assert(api);
    assert(fd >= 0);
    assert(callback);

    g = api->userdata;
    assert(g);

    if (!(w = avahi_new(AvahiWatch, 1)))
        return NULL;
    
    w->glib_poll = g;
    w->pollfd.fd = fd;
    w->pollfd.events =
        (event & AVAHI_WATCH_IN ? G_IO_IN : 0) |
        (event & AVAHI_WATCH_OUT ? G_IO_OUT : 0) |
        (event & AVAHI_WATCH_ERR ? G_IO_ERR : 0) |
        (event & AVAHI_WATCH_HUP ? G_IO_HUP : 0);
        ;
    w->callback = callback;
    w->userdata = userdata;
    w->dead = 0;

    g_source_add_poll(&g->source, &w->pollfd);
    w->pollfd_added = 1;

    AVAHI_LLIST_PREPEND(AvahiWatch, watches, g->watches, w);

    return w;
}

static void watch_update(AvahiWatch *w, AvahiWatchEvent events) {
    assert(w);
    assert(!w->dead);

    w->pollfd.events = events;
}

static void watch_free(AvahiWatch *w) {
    assert(w);
    assert(!w->dead);

    if (w->pollfd_added) {
        g_source_remove_poll(&w->glib_poll->source, &w->pollfd);
        w->pollfd_added = 0;
    }
    
    w->dead = 1;
    w->glib_poll->req_cleanup = 1;
}

static void set_wakeup(const AvahiPoll *api, const struct timeval *tv, AvahiWakeupCallback callback, void *userdata) {
    AvahiGLibPoll *g;

    assert(api);
    g = api->userdata;

    if (callback) {
        if (tv) 
            g->wakeup = *tv;
        else {
            g->wakeup.tv_sec = 0;
            g->wakeup.tv_usec = 0;
        }
            
        g->wakeup_callback = callback;
        g->wakeup_userdata = userdata;
    } else
        g->wakeup_callback = NULL;
}

static void start_wakeup_callback(AvahiGLibPoll *g) {
    AvahiWakeupCallback callback;
    void *userdata;

    assert(g);

    /* Reset the wakeup functions, but allow changing of the two
       values from the callback function */

    callback = g->wakeup_callback;
    userdata = g->wakeup_userdata;
    g->wakeup_callback = NULL;
    g->wakeup_userdata = NULL;

    assert(callback);
    
    callback(&g->api, userdata);
}

static gboolean prepare_func(GSource *source, gint *timeout) {
    AvahiGLibPoll *g = (AvahiGLibPoll*) source;

    g_assert(g);
    g_assert(timeout);

    if (g->req_cleanup)
        cleanup(g, 0);
    
    if (g->wakeup_callback) {
        GTimeVal now;
        struct timeval tvnow;
        AvahiUsec usec;

        g_source_get_current_time(source, &now);
        tvnow.tv_sec = now.tv_sec;
        tvnow.tv_usec = now.tv_usec;
    
        usec = avahi_timeval_diff(&g->wakeup, &tvnow);

        if (usec <= 0)
            return TRUE;

        *timeout = (gint) (usec / 1000);
    }
        
    return FALSE;
}

static gboolean check_func(GSource *source) {
    AvahiGLibPoll *g = (AvahiGLibPoll*) source;
    AvahiWatch *w;

    g_assert(g);

    if (g->wakeup_callback) {
        GTimeVal now;
        struct timeval tvnow;
        g_source_get_current_time(source, &now);
        tvnow.tv_sec = now.tv_sec;
        tvnow.tv_usec = now.tv_usec;
        
        if (avahi_timeval_compare(&g->wakeup, &tvnow) < 0)
            return TRUE;
    }

    for (w = g->watches; w; w = w->watches_next)
        if (w->pollfd.revents > 0)
            return TRUE;
    
    return FALSE;
}

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer userdata) {
    AvahiGLibPoll* g = (AvahiGLibPoll*) source;
    AvahiWatch *w;
    
    g_assert(g);

    if (g->wakeup_callback) {
        GTimeVal now;
        struct timeval tvnow;
        g_source_get_current_time(source, &now);
        tvnow.tv_sec = now.tv_sec;
        tvnow.tv_usec = now.tv_usec;
        
        if (avahi_timeval_compare(&g->wakeup, &tvnow) < 0) {
            start_wakeup_callback(g);
            return TRUE;
        }
    }
    
    for (w = g->watches; w; w = w->watches_next)
        if (w->pollfd.revents > 0) {
            assert(w->callback);
            w->callback(w, w->pollfd.fd, w->pollfd.revents, w->userdata);
            w->pollfd.revents = 0;
            return TRUE;
        }

    return TRUE;
}

AvahiGLibPoll *avahi_glib_poll_new(GMainContext *context) {
    AvahiGLibPoll *g;
    
    static GSourceFuncs source_funcs = {
        prepare_func,
        check_func,
        dispatch_func,
        NULL,
        NULL,
        NULL
    };

    g = (AvahiGLibPoll*) g_source_new(&source_funcs, sizeof(AvahiGLibPoll));
    g_main_context_ref(g->context = context ? context : g_main_context_default());

    g->api.userdata = g;
    g->api.watch_new = watch_new;
    g->api.watch_free = watch_free;
    g->api.watch_update = watch_update;
    g->api.set_wakeup = set_wakeup;

    g->wakeup_callback = NULL;
    g->req_cleanup = 0;
    
    AVAHI_LLIST_HEAD_INIT(AvahiWatch, g->watches);
    
    g_source_attach(&g->source, g->context);

    return g;
}

void avahi_glib_poll_free(AvahiGLibPoll *g) {
    GSource *s = &g->source;
    assert(g);

/*     g_message("BEFORE"); */
    cleanup(g, 1);

/*     g_message("MIDDLE"); */
    g_main_context_unref(g->context);
    g_source_destroy(s);
    g_source_unref(s);
/*     g_message("AFTER"); */
}

const AvahiPoll* avahi_glib_poll_get(AvahiGLibPoll *g) {
    assert(g);

    return &g->api;
}
