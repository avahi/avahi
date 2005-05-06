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

#include "timeeventq.h"
#include "util.h"

static gint compare(gconstpointer _a, gconstpointer _b) {
    const AvahiTimeEvent *a = _a,  *b = _b;

    return avahi_timeval_compare(&a->expiry, &b->expiry);
}

static gboolean prepare_func(GSource *source, gint *timeout) {
    AvahiTimeEventQueue *q = (AvahiTimeEventQueue*) source;
    AvahiTimeEvent *e;
    GTimeVal now;

    g_assert(source);
    g_assert(timeout);

    if (!q->prioq->root) {
        *timeout = -1;
        return FALSE;
    }
    
    e = q->prioq->root->data;
    g_assert(e);

    g_source_get_current_time(source, &now);

    if (avahi_timeval_compare(&now, &e->expiry) >= 0) {
        *timeout = -1;
        return TRUE;
    }

    *timeout = (gint) (avahi_timeval_diff(&e->expiry, &now)/1000);
    
    return FALSE;
}

static gboolean check_func(GSource *source) {
    AvahiTimeEventQueue *q = (AvahiTimeEventQueue*) source;
    AvahiTimeEvent *e;
    GTimeVal now;

    g_assert(source);

    if (!q->prioq->root)
        return FALSE;

    e = q->prioq->root->data;
    g_assert(e);

    g_source_get_current_time(source, &now);
    
    return avahi_timeval_compare(&now, &e->expiry) >= 0;
}

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer user_data) {
    AvahiTimeEventQueue *q = (AvahiTimeEventQueue*) source;
    GTimeVal now;

    g_assert(source);

    g_source_get_current_time(source, &now);

    while (q->prioq->root) {
        AvahiTimeEvent *e = q->prioq->root->data;

        if (avahi_timeval_compare(&now, &e->expiry) < 0)
            break;

        g_assert(e->callback);
        e->callback(e, e->userdata);
    }

    return TRUE;
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

AvahiTimeEvent* avahi_time_event_queue_add(AvahiTimeEventQueue *q, const GTimeVal *timeval, void (*callback)(AvahiTimeEvent *e, void *userdata), void *userdata) {
    AvahiTimeEvent *e;
    
    g_assert(q);
    g_assert(timeval);
    g_assert(callback);
    g_assert(userdata);

    e = g_new(AvahiTimeEvent, 1);
    e->queue = q;
    e->expiry = *timeval;
    e->callback = callback;
    e->userdata = userdata;

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

void avahi_time_event_queue_update(AvahiTimeEventQueue *q, AvahiTimeEvent *e, const GTimeVal *timeval) {
    g_assert(q);
    g_assert(e);
    g_assert(e->queue == q);

    e->expiry = *timeval;

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


