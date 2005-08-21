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

#include "browse.h"
#include "log.h"

struct AvahiSRecordBrowser {
    int dead;
    
    AvahiServer *server;
    AvahiKey *key;
    AvahiIfIndex interface;
    AvahiProtocol protocol;
    unsigned sec_delay;

    AvahiTimeEvent *query_time_event;
    AvahiTimeEvent *scan_time_event;

    AvahiSRecordBrowserCallback callback;
    void* userdata;

    AVAHI_LLIST_FIELDS(AvahiSRecordBrowser, browser);
    AVAHI_LLIST_FIELDS(AvahiSRecordBrowser, by_key);
};

static void elapse_callback(AvahiTimeEvent *e, void *userdata) {
    AvahiSRecordBrowser *s = userdata;
    struct timeval tv;
/*     char *t;  */
    
    assert(s);

    avahi_server_post_query(s->server, s->interface, s->protocol, s->key);

    s->sec_delay *= 2;
    
    if (s->sec_delay >= 60*60)  /* 1h */
        s->sec_delay = 60*60;
    
/*     avahi_log_debug("Continuous querying for %s (%i)", t = avahi_key_to_string(s->key), s->sec_delay);  */
/*     avahi_free(t);  */
    
    avahi_elapse_time(&tv, s->sec_delay*1000, 0);
    avahi_time_event_update(s->query_time_event, &tv);
}

struct cbdata {
    AvahiSRecordBrowser *record_browser;
    AvahiInterface *interface;
};

static void* scan_cache_callback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, void* userdata) {
    struct cbdata *cbdata = userdata;

    assert(c);
    assert(pattern);
    assert(e);
    assert(cbdata);

    if (cbdata->record_browser->dead)
        return NULL;

    cbdata->record_browser->callback(
        cbdata->record_browser,
        cbdata->interface->hardware->index,
        cbdata->interface->protocol,
        AVAHI_BROWSER_NEW,
        e->record,
        cbdata->record_browser->userdata);

    return NULL;
}

static void scan_interface_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void* userdata) {
    AvahiSRecordBrowser *b = userdata;
    struct cbdata cbdata;

    cbdata.record_browser = b;
    cbdata.interface = i;

    assert(m);
    assert(i);
    assert(b);

    if (!b->dead)
        avahi_cache_walk(i->cache, b->key, scan_cache_callback, &cbdata);
}

static void scan_callback(AvahiTimeEvent *e, void *userdata) {
    AvahiSRecordBrowser *b = userdata;
    assert(b);

    /* Scan the caches */
    if (!b->dead)
        avahi_interface_monitor_walk(b->server->monitor, b->interface, b->protocol, scan_interface_callback, b);

    if (b->scan_time_event) {
        avahi_time_event_free(b->scan_time_event);
        b->scan_time_event = NULL;
    }
}

void avahi_s_record_browser_restart(AvahiSRecordBrowser *b) {
    assert(b);

    if (!b->scan_time_event) {
        b->scan_time_event = avahi_time_event_new(b->server->time_event_queue, NULL, scan_callback, b);
        assert(b->scan_time_event);
    }

    avahi_server_post_query(b->server, b->interface, b->protocol, b->key);
}

AvahiSRecordBrowser *avahi_s_record_browser_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, AvahiKey *key, AvahiSRecordBrowserCallback callback, void* userdata) {
    AvahiSRecordBrowser *b, *t;
    struct timeval tv;

    assert(server);
    assert(key);
    assert(callback);

    if (avahi_key_is_pattern(key)) {
        avahi_server_set_errno(server, AVAHI_ERR_IS_PATTERN);
        return NULL;
    }

    if (!avahi_key_is_valid(key)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_KEY);
        return NULL;
    }
    
    if (!(b = avahi_new(AvahiSRecordBrowser, 1))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }
    
    b->dead = 0;
    b->server = server;
    b->key = avahi_key_ref(key);
    b->interface = interface;
    b->protocol = protocol;
    b->callback = callback;
    b->userdata = userdata;
    b->sec_delay = 1;

    avahi_server_post_query(b->server, b->interface, b->protocol, b->key);
    
    avahi_elapse_time(&tv, b->sec_delay*1000, 0);
    b->query_time_event = avahi_time_event_new(server->time_event_queue, &tv, elapse_callback, b);

    AVAHI_LLIST_PREPEND(AvahiSRecordBrowser, browser, server->record_browsers, b);

    /* Add the new entry to the record_browser hash table */
    t = avahi_hashmap_lookup(server->record_browser_hashmap, key);
    AVAHI_LLIST_PREPEND(AvahiSRecordBrowser, by_key, t, b);
    avahi_hashmap_replace(server->record_browser_hashmap, key, t);

    /* The currenlty cached entries are scanned a bit later */
    b->scan_time_event = avahi_time_event_new(server->time_event_queue, NULL, scan_callback, b);
    assert(b->scan_time_event);
    return b;
}

void avahi_s_record_browser_free(AvahiSRecordBrowser *b) {
    assert(b);
    assert(!b->dead);

    b->dead = 1;
    b->server->need_browser_cleanup = 1;

    if (b->query_time_event) {
        avahi_time_event_free(b->query_time_event);
        b->query_time_event = NULL;
    }

    if (b->scan_time_event) {
        avahi_time_event_free(b->scan_time_event);
        b->scan_time_event = NULL;
    }
}

void avahi_s_record_browser_destroy(AvahiSRecordBrowser *b) {
    AvahiSRecordBrowser *t;
    
    assert(b);
    
    AVAHI_LLIST_REMOVE(AvahiSRecordBrowser, browser, b->server->record_browsers, b);

    t = avahi_hashmap_lookup(b->server->record_browser_hashmap, b->key);
    AVAHI_LLIST_REMOVE(AvahiSRecordBrowser, by_key, t, b);
    if (t)
        avahi_hashmap_replace(b->server->record_browser_hashmap, t->key, t);
    else
        avahi_hashmap_remove(b->server->record_browser_hashmap, b->key);

    if (b->query_time_event)
        avahi_time_event_free(b->query_time_event);
    if (b->scan_time_event)
        avahi_time_event_free(b->scan_time_event);

    avahi_key_unref(b->key);
    
    avahi_free(b);
}

void avahi_browser_cleanup(AvahiServer *server) {
    AvahiSRecordBrowser *b;
    AvahiSRecordBrowser *n;
    
    assert(server);

    for (b = server->record_browsers; b; b = n) {
        n = b->browser_next;
        
        if (b->dead)
            avahi_s_record_browser_destroy(b);
    }

    server->need_browser_cleanup = 0;
}

void avahi_browser_notify(AvahiServer *server, AvahiInterface *i, AvahiRecord *record, AvahiBrowserEvent event) {
    AvahiSRecordBrowser *b;
    
    assert(server);
    assert(record);

    for (b = avahi_hashmap_lookup(server->record_browser_hashmap, record->key); b; b = b->by_key_next)
        if (!b->dead && avahi_interface_match(i, b->interface, b->protocol))
                b->callback(b, i->hardware->index, i->protocol, event, record, b->userdata);
}

int avahi_is_subscribed(AvahiServer *server, AvahiInterface *i, AvahiKey *k) {
    AvahiSRecordBrowser *b;
    assert(server);
    assert(k);

    for (b = avahi_hashmap_lookup(server->record_browser_hashmap, k); b; b = b->by_key_next)
        if (!b->dead && avahi_interface_match(i, b->interface, b->protocol))
            return 1;

    return 0;
}

void avahi_browser_new_interface(AvahiServer*s, AvahiInterface *i) {
    AvahiSRecordBrowser *b;
    
    assert(s);
    assert(i);
    
    for (b = s->record_browsers; b; b = b->browser_next)
        if (avahi_interface_match(i, b->interface, b->protocol))
            avahi_interface_post_query(i, b->key, 0);
}
