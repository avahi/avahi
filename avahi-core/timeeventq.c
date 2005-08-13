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

#include <assert.h>
#include <stdlib.h>

#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>

#include "timeeventq.h"
#include "log.h"

struct AvahiTimeEvent {
    AvahiTimeEventQueue *queue;
    AvahiPrioQueueNode *node;
    struct timeval expiry;
    struct timeval last_run;
    AvahiTimeEventCallback callback;
    void* userdata;
};

struct AvahiTimeEventQueue {
    AvahiPoll *poll_api;
    AvahiPrioQueue *prioq;
};

static int compare(const void* _a, const void* _b) {
    const AvahiTimeEvent *a = _a,  *b = _b;
    int ret;

    if ((ret = avahi_timeval_compare(&a->expiry, &b->expiry)) != 0)
        return ret;

    /* If both exevents are scheduled for the same time, put the entry
     * that has been run earlier the last time first. */
    return avahi_timeval_compare(&a->last_run, &b->last_run);
}

static void expiration_event(AvahiPoll *poll_api, void *userdata);

static void update_wakeup(AvahiTimeEventQueue *q) {
    assert(q);

    if (q->prioq->root) {
        AvahiTimeEvent *e = q->prioq->root->data;
        q->poll_api->set_wakeup(q->poll_api, &e->expiry, expiration_event, q);
    } else
        q->poll_api->set_wakeup(q->poll_api, NULL, NULL, NULL);
}

void expiration_event(AvahiPoll *poll_api, void *userdata) {
    struct timeval now;
    AvahiTimeEventQueue *q = userdata;
    AvahiTimeEvent *e;

    gettimeofday(&now, NULL);
    
    if ((e = avahi_time_event_queue_root(q))) {

        /* Check if expired */
        if (avahi_timeval_compare(&now, &e->expiry) >= 0) {

            /* Make sure to move the entry away from the front */
            e->last_run = now;
            avahi_prio_queue_shuffle(q->prioq, e->node);

            /* Run it */
            assert(e->callback);
            e->callback(e, e->userdata);
        }
    }

    update_wakeup(q);
}

static void fix_expiry_time(AvahiTimeEvent *e) {
    struct timeval now;
    assert(e);

    gettimeofday(&now, NULL);

    if (avahi_timeval_compare(&now, &e->expiry) > 0)
        e->expiry = now;
}

AvahiTimeEventQueue* avahi_time_event_queue_new(AvahiPoll *poll_api) {
    AvahiTimeEventQueue *q;

    if (!(q = avahi_new(AvahiTimeEventQueue, 1))) {
        avahi_log_error(__FILE__": Out of memory");
        goto oom;
    }

    if (!(q->prioq = avahi_prio_queue_new(compare)))
        goto oom;

    q->poll_api = poll_api;
    return q;

oom:

    if (q)
        avahi_free(q);
    
    return NULL;
}

void avahi_time_event_queue_free(AvahiTimeEventQueue *q) {
    assert(q);

    while (q->prioq->root)
        avahi_time_event_free(q->prioq->root->data);
    avahi_prio_queue_free(q->prioq);

    avahi_free(q);
}

AvahiTimeEvent* avahi_time_event_new(
    AvahiTimeEventQueue *q,
    const struct timeval *timeval,
    AvahiTimeEventCallback callback,
    void* userdata) {
    
    AvahiTimeEvent *e;
    
    assert(q);
    assert(callback);
    assert(userdata);

    if (!(e = avahi_new(AvahiTimeEvent, 1))) {
        avahi_log_error(__FILE__": Out of memory");
        return NULL; /* OOM */
    }
    
    e->queue = q;
    e->callback = callback;
    e->userdata = userdata;

    if (timeval)
        e->expiry = *timeval;
    else {
        e->expiry.tv_sec = 0;
        e->expiry.tv_usec = 0;
    }
    
    fix_expiry_time(e);
    
    e->last_run.tv_sec = 0;
    e->last_run.tv_usec = 0;

    if (!(e->node = avahi_prio_queue_put(q->prioq, e))) {
        avahi_free(e);
        return NULL;
    }

    update_wakeup(q);
    return e;
}

void avahi_time_event_free(AvahiTimeEvent *e) {
    AvahiTimeEventQueue *q;
    assert(e);

    q = e->queue;

    avahi_prio_queue_remove(q->prioq, e->node);
    avahi_free(e);

    update_wakeup(q);
}

void avahi_time_event_update(AvahiTimeEvent *e, const struct timeval *timeval) {
    assert(e);
    assert(timeval);

    e->expiry = *timeval;
    fix_expiry_time(e);
    avahi_prio_queue_shuffle(e->queue->prioq, e->node);
    
    update_wakeup(e->queue);
}

AvahiTimeEvent* avahi_time_event_queue_root(AvahiTimeEventQueue *q) {
    assert(q);

    return q->prioq->root ? q->prioq->root->data : NULL;
}

AvahiTimeEvent* avahi_time_event_next(AvahiTimeEvent *e) {
    assert(e);

    return e->node->next->data;
}


