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

#include "timeeventq.h"
#include "timeval.h"

static gint compare(gconstpointer _a, gconstpointer _b) {
    const AvahiTimeEvent *a = _a,  *b = _b;
    gint ret;

    if ((ret = avahi_timeval_compare(&a->expiry, &b->expiry)) != 0)
        return ret;

    /* If both exevents are scheduled for the same time, put the entry
     * that has been run earlier the last time first. */
    return avahi_timeval_compare(&a->last_run, &b->last_run);
}

static void source_get_timeval(GSource *source, struct timeval *tv) {
    GTimeVal gtv;
    
    g_assert(source);
    g_assert(tv);

    g_source_get_current_time(source, &gtv);
    tv->tv_sec = gtv.tv_sec;
    tv->tv_usec = gtv.tv_usec;
}

static gboolean prepare_func(GSource *source, gint *timeout) {
    AvahiTimeEventQueue *q = (AvahiTimeEventQueue*) source;
    AvahiTimeEvent *e;
    struct timeval now;

    g_assert(source);
    g_assert(timeout);

    if (!q->prioq->root) {
        *timeout = -1;
        return FALSE;
    }
    
    e = q->prioq->root->data;
    g_assert(e);

    source_get_timeval(source, &now);

    if (avahi_timeval_compare(&now, &e->expiry) >= 0 &&  /* Time elapsed */
        avahi_timeval_compare(&now, &e->last_run) != 0   /* Not yet run */) {
        *timeout = -1;
        return TRUE;
    }

    *timeout = (gint) (avahi_timeval_diff(&e->expiry, &now)/1000);

    /* Wait at least 1 msec */
    if (*timeout <= 0)
        *timeout = 1;
    
    return FALSE;
}

static gboolean check_func(GSource *source) {
    AvahiTimeEventQueue *q = (AvahiTimeEventQueue*) source;
    AvahiTimeEvent *e;
    struct timeval now;

    g_assert(source);

    if (!q->prioq->root)
        return FALSE;

    e = q->prioq->root->data;
    g_assert(e);

    source_get_timeval(source, &now);
    
    return
        avahi_timeval_compare(&now, &e->expiry) >= 0 && /* Time elapsed */
        avahi_timeval_compare(&now, &e->last_run) != 0;  /* Not yet run */
}

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer user_data) {
    AvahiTimeEventQueue *q = (AvahiTimeEventQueue*) source;
    struct timeval now;

    g_assert(source);

    source_get_timeval(source, &now);

    while (q->prioq->root) {
        AvahiTimeEvent *e = q->prioq->root->data;

        /* Not yet expired */
        if (avahi_timeval_compare(&now, &e->expiry) < 0)
            break;

        /* Already ran */
        if (avahi_timeval_compare(&now, &e->last_run) == 0)
            break;

        /* Make sure to move the entry away from the front */
        e->last_run = now;
        avahi_prio_queue_shuffle(q->prioq, e->node);

        /* Run it */
        g_assert(e->callback);
        e->callback(e, e->userdata);
    }

    return TRUE;
}

static void fix_expiry_time(AvahiTimeEvent *e) {
    struct timeval now;
    g_assert(e);

    source_get_timeval(&e->queue->source, &now);

    if (avahi_timeval_compare(&now, &e->expiry) > 0)
        e->expiry = now;
    
}

AvahiTimeEventQueue* avahi_time_event_queue_new(GMainContext *context, gint priority) {
    AvahiTimeEventQueue *q;

    static GSourceFuncs source_funcs = {
        prepare_func,
        check_func,
        dispatch_func,
        NULL,
        NULL,
        NULL
    };

    q = (AvahiTimeEventQueue*) g_source_new(&source_funcs, sizeof(AvahiTimeEventQueue));
    q->prioq = avahi_prio_queue_new(compare);

    g_source_set_priority((GSource*) q, priority);
    
    g_source_attach(&q->source, context);
    
    return q;
}

void avahi_time_event_queue_free(AvahiTimeEventQueue *q) {
    g_assert(q);

    while (q->prioq->root)
        avahi_time_event_queue_remove(q, q->prioq->root->data);
    avahi_prio_queue_free(q->prioq);

    g_source_destroy(&q->source);
    g_source_unref(&q->source);
}

AvahiTimeEvent* avahi_time_event_queue_add(AvahiTimeEventQueue *q, const struct timeval *timeval, AvahiTimeEventCallback callback, gpointer userdata) {
    AvahiTimeEvent *e;
    
    g_assert(q);
    g_assert(timeval);
    g_assert(callback);
    g_assert(userdata);

    e = g_new(AvahiTimeEvent, 1);
    e->queue = q;
    e->callback = callback;
    e->userdata = userdata;

    e->expiry = *timeval;
    fix_expiry_time(e);
    
    e->last_run.tv_sec = 0;
    e->last_run.tv_usec = 0;

    e->node = avahi_prio_queue_put(q->prioq, e);
    
    return e;
}

void avahi_time_event_queue_remove(AvahiTimeEventQueue *q, AvahiTimeEvent *e) {
    g_assert(q);
    g_assert(e);
    g_assert(e->queue == q);

    avahi_prio_queue_remove(q->prioq, e->node);
    g_free(e);
}

void avahi_time_event_queue_update(AvahiTimeEventQueue *q, AvahiTimeEvent *e, const struct timeval *timeval) {
    g_assert(q);
    g_assert(e);
    g_assert(e->queue == q);
    g_assert(timeval);

    e->expiry = *timeval;
    fix_expiry_time(e);

    avahi_prio_queue_shuffle(q->prioq, e->node);
}

AvahiTimeEvent* avahi_time_event_queue_root(AvahiTimeEventQueue *q) {
    g_assert(q);

    return q->prioq->root ? q->prioq->root->data : NULL;
}

AvahiTimeEvent* avahi_time_event_next(AvahiTimeEvent *e) {
    g_assert(e);

    return e->node->next->data;
}


