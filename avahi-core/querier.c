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

#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>

#include "querier.h"
#include "log.h"

struct AvahiQuerier {
    AvahiInterface *interface;

    AvahiKey *key;
    int n_used;

    unsigned sec_delay;

    AvahiTimeEvent *time_event;

    struct timeval creation_time;
    
    AVAHI_LLIST_FIELDS(AvahiQuerier, queriers);
};

void avahi_querier_free(AvahiQuerier *q) {
    assert(q);

    AVAHI_LLIST_REMOVE(AvahiQuerier, queriers, q->interface->queriers, q);
    avahi_hashmap_remove(q->interface->queriers_by_key, q->key);

    avahi_key_unref(q->key);
    avahi_time_event_free(q->time_event);
    
    avahi_free(q);
}

static void querier_elapse_callback(AVAHI_GCC_UNUSED AvahiTimeEvent *e, void *userdata) {
    AvahiQuerier *q = userdata;
    struct timeval tv;
    
    assert(q);

    avahi_interface_post_query(q->interface, q->key, 0);

    q->sec_delay *= 2;
    
    if (q->sec_delay >= 60*60)  /* 1h */
        q->sec_delay = 60*60;
    
    avahi_elapse_time(&tv, q->sec_delay*1000, 0);
    avahi_time_event_update(q->time_event, &tv);
}

void avahi_querier_add(AvahiInterface *i, AvahiKey *key, struct timeval *ret_ctime) {
    AvahiQuerier *q;
    struct timeval tv;
    
    assert(i);
    assert(key);
    
    if ((q = avahi_hashmap_lookup(i->queriers_by_key, key))) {
        /* Someone is already browsing for records of this RR key */
        q->n_used++;

        /* Return the creation time */
        if (ret_ctime)
            *ret_ctime = q->creation_time;
        return;
    }

    /* No one is browsing for this RR key, so we add a new querier */
    if (!(q = avahi_new(AvahiQuerier, 1)))
        return; /* OOM */
    
    q->key = avahi_key_ref(key);
    q->interface = i;
    q->n_used = 1;
    q->sec_delay = 1;
    gettimeofday(&q->creation_time, NULL);

    /* Do the initial query */
    avahi_interface_post_query(i, key, 0);

    /* Schedule next queries */
    q->time_event = avahi_time_event_new(i->monitor->server->time_event_queue, avahi_elapse_time(&tv, q->sec_delay*1000, 0), querier_elapse_callback, q);

    AVAHI_LLIST_PREPEND(AvahiQuerier, queriers, i->queriers, q);
    avahi_hashmap_insert(i->queriers_by_key, q->key, q);

    /* Return the creation time */
    if (ret_ctime)
        *ret_ctime = q->creation_time;
}

void avahi_querier_remove(AvahiInterface *i, AvahiKey *key) {
    AvahiQuerier *q;

    if (!(q = avahi_hashmap_lookup(i->queriers_by_key, key))) {
        /* The was no querier for this RR key */
        avahi_log_warn(__FILE__": querier_remove() called but no querier to remove");
        return;
    }

    assert(q->n_used >= 1);

    if ((--q->n_used) <= 0)
        avahi_querier_free(q);
}

static void remove_querier_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void* userdata) {
    assert(m);
    assert(i);
    assert(userdata);

    if (i->announcing)
        avahi_querier_remove(i, (AvahiKey*) userdata);
}

void avahi_querier_remove_for_all(AvahiServer *s, AvahiIfIndex idx, AvahiProtocol protocol, AvahiKey *key) {
    assert(s);
    assert(key);
    
    avahi_interface_monitor_walk(s->monitor, idx, protocol, remove_querier_callback, key);
}

struct cbdata {
    AvahiKey *key;
    struct timeval *ret_ctime;
};

static void add_querier_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void* userdata) {
    struct cbdata *cbdata = userdata;
    
    assert(m);
    assert(i);
    assert(cbdata);

    if (i->announcing) {
        struct timeval tv;
        avahi_querier_add(i, cbdata->key, &tv);

        if (cbdata->ret_ctime && avahi_timeval_compare(&tv, cbdata->ret_ctime) > 0)
            *cbdata->ret_ctime = tv;
    }
}

void avahi_querier_add_for_all(AvahiServer *s, AvahiIfIndex idx, AvahiProtocol protocol, AvahiKey *key, struct timeval *ret_ctime) {
    struct cbdata cbdata;
    
    assert(s);
    assert(key);

    cbdata.key = key;
    cbdata.ret_ctime = ret_ctime;

    if (ret_ctime)
        ret_ctime->tv_sec = ret_ctime->tv_usec = 0;
    
    avahi_interface_monitor_walk(s->monitor, idx, protocol, add_querier_callback, &cbdata);
}

int avahi_querier_exists(AvahiInterface *i, AvahiKey *key) {
    assert(i);
    assert(key);

    if (avahi_hashmap_lookup(i->queriers_by_key, key))
        return 1;

    return 0;
}

void avahi_querier_free_all(AvahiInterface *i) {
    assert(i);

    while (i->queriers) 
        avahi_querier_free(i->queriers);
}
