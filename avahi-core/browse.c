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

#include "browse.h"
#include "timeval.h"
#include "log.h"

struct AvahiRecordBrowser {
    gboolean dead;
    
    AvahiServer *server;
    AvahiKey *key;
    AvahiIfIndex interface;
    AvahiProtocol protocol;
    guint sec_delay;

    AvahiTimeEvent *time_event;

    AvahiRecordBrowserCallback callback;
    gpointer userdata;
    guint scan_idle_source;

    AVAHI_LLIST_FIELDS(AvahiRecordBrowser, browser);
    AVAHI_LLIST_FIELDS(AvahiRecordBrowser, by_key);
};

static void elapse(AvahiTimeEvent *e, void *userdata) {
    AvahiRecordBrowser *s = userdata;
    struct timeval tv;
/*     gchar *t;  */
    
    g_assert(s);

    avahi_server_post_query(s->server, s->interface, s->protocol, s->key);

    s->sec_delay *= 2;
    
    if (s->sec_delay >= 60*60)  /* 1h */
        s->sec_delay = 60*60;
    
/*     avahi_log_debug("Continuous querying for %s (%i)", t = avahi_key_to_string(s->key), s->sec_delay);  */
/*     g_free(t);  */
    
    avahi_elapse_time(&tv, s->sec_delay*1000, 0);
    avahi_time_event_queue_update(s->server->time_event_queue, s->time_event, &tv);
}

struct cbdata {
    AvahiRecordBrowser *record_browser;
    AvahiInterface *interface;
};

static gpointer scan_cache_callback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, gpointer userdata) {
    struct cbdata *cbdata = userdata;

    g_assert(c);
    g_assert(pattern);
    g_assert(e);
    g_assert(cbdata);

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

static void scan_interface_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, gpointer userdata) {
    AvahiRecordBrowser *s = userdata;
    struct cbdata cbdata = { s, i };

    g_assert(m);
    g_assert(i);
    g_assert(s);

    avahi_cache_walk(i->cache, s->key, scan_cache_callback, &cbdata);
}

static gboolean scan_idle_callback(gpointer data) {
    AvahiRecordBrowser *b = data;
    g_assert(b);

    /* Scan the caches */
    avahi_interface_monitor_walk(b->server->monitor, b->interface, b->protocol, scan_interface_callback, b);
    b->scan_idle_source = (guint) -1;

    return FALSE;
}

AvahiRecordBrowser *avahi_record_browser_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, AvahiKey *key, AvahiRecordBrowserCallback callback, gpointer userdata) {
    AvahiRecordBrowser *b, *t;
    struct timeval tv;

    g_assert(server);
    g_assert(key);
    g_assert(callback);

    if (avahi_key_is_pattern(key)) {
        avahi_server_set_errno(server, AVAHI_ERR_IS_PATTERN);
        return NULL;
    }

    if (!avahi_key_valid(key)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_KEY);
        return NULL;
    }
    
    b = g_new(AvahiRecordBrowser, 1);
    b->dead = FALSE;
    b->server = server;
    b->key = avahi_key_ref(key);
    b->interface = interface;
    b->protocol = protocol;
    b->callback = callback;
    b->userdata = userdata;
    b->sec_delay = 1;

    avahi_server_post_query(b->server, b->interface, b->protocol, b->key);
    
    avahi_elapse_time(&tv, b->sec_delay*1000, 0);
    b->time_event = avahi_time_event_queue_add(server->time_event_queue, &tv, elapse, b);

    AVAHI_LLIST_PREPEND(AvahiRecordBrowser, browser, server->record_browsers, b);

    /* Add the new entry to the record_browser hash table */
    t = g_hash_table_lookup(server->record_browser_hashtable, key);
    AVAHI_LLIST_PREPEND(AvahiRecordBrowser, by_key, t, b);
    g_hash_table_replace(server->record_browser_hashtable, key, t);

    /* The currenlty cached entries are scanned a bit later */
    b->scan_idle_source = g_idle_add_full(G_PRIORITY_HIGH, scan_idle_callback, b, NULL);
    return b;
}

void avahi_record_browser_free(AvahiRecordBrowser *b) {
    g_assert(b);
    g_assert(!b->dead);

    b->dead = TRUE;
    b->server->need_browser_cleanup = TRUE;

    if (b->time_event) {
        avahi_time_event_queue_remove(b->server->time_event_queue, b->time_event);
        b->time_event = NULL;

        if (b->scan_idle_source != (guint) -1) {
            g_source_remove(b->scan_idle_source);
            b->scan_idle_source = (guint) -1;
        }

    }
}

void avahi_record_browser_destroy(AvahiRecordBrowser *b) {
    AvahiRecordBrowser *t;
    
    g_assert(b);
    
    AVAHI_LLIST_REMOVE(AvahiRecordBrowser, browser, b->server->record_browsers, b);

    t = g_hash_table_lookup(b->server->record_browser_hashtable, b->key);
    AVAHI_LLIST_REMOVE(AvahiRecordBrowser, by_key, t, b);
    if (t)
        g_hash_table_replace(b->server->record_browser_hashtable, t->key, t);
    else
        g_hash_table_remove(b->server->record_browser_hashtable, b->key);

    if (b->time_event)
        avahi_time_event_queue_remove(b->server->time_event_queue, b->time_event);
    avahi_key_unref(b->key);

    if (b->scan_idle_source != (guint) -1)
        g_source_remove(b->scan_idle_source);
    
    g_free(b);
}

void avahi_browser_cleanup(AvahiServer *server) {
    AvahiRecordBrowser *b;
    AvahiRecordBrowser *n;
    
    g_assert(server);

    for (b = server->record_browsers; b; b = n) {
        n = b->browser_next;
        
        if (b->dead)
            avahi_record_browser_destroy(b);
    }

    server->need_browser_cleanup = FALSE;
}

void avahi_browser_notify(AvahiServer *server, AvahiInterface *i, AvahiRecord *record, AvahiBrowserEvent event) {
    AvahiRecordBrowser *b;
    
    g_assert(server);
    g_assert(record);

    for (b = g_hash_table_lookup(server->record_browser_hashtable, record->key); b; b = b->by_key_next)
        if (!b->dead && avahi_interface_match(i, b->interface, b->protocol))
                b->callback(b, i->hardware->index, i->protocol, event, record, b->userdata);
}

gboolean avahi_is_subscribed(AvahiServer *server, AvahiInterface *i, AvahiKey *k) {
    AvahiRecordBrowser *b;
    g_assert(server);
    g_assert(k);

    for (b = g_hash_table_lookup(server->record_browser_hashtable, k); b; b = b->by_key_next)
        if (!b->dead && avahi_interface_match(i, b->interface, b->protocol))
            return TRUE;

    return FALSE;
}

void avahi_browser_new_interface(AvahiServer*s, AvahiInterface *i) {
    AvahiRecordBrowser *b;
    
    g_assert(s);
    g_assert(i);
    
    for (b = s->record_browsers; b; b = b->browser_next)
        if (avahi_interface_match(i, b->interface, b->protocol))
            avahi_interface_post_query(i, b->key, FALSE);
}
