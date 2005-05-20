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

#include "resolve.h"
#include "util.h"

static void elapse(AvahiTimeEvent *e, void *userdata) {
    AvahiRecordResolver *s = userdata;
    GTimeVal tv;
/*     gchar *t; */
    
    g_assert(s);

    avahi_server_post_query(s->server, s->interface, s->protocol, s->key);

    if (s->n_query++ <= 8)
        s->sec_delay *= 2;

/*     g_message("%i. Continuous querying for %s", s->n_query, t = avahi_key_to_string(s->key)); */
/*     g_free(t); */
    
    avahi_elapse_time(&tv, s->sec_delay*1000, 0);
    avahi_time_event_queue_update(s->server->time_event_queue, s->time_event, &tv);
}

struct cbdata {
    AvahiRecordResolver *record_resolver;
    AvahiInterface *interface;
};

static gpointer scan_cache_callback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, gpointer userdata) {
    struct cbdata *cbdata = userdata;

    g_assert(c);
    g_assert(pattern);
    g_assert(e);
    g_assert(cbdata);

    cbdata->record_resolver->callback(
        cbdata->record_resolver,
        cbdata->interface->hardware->index,
        cbdata->interface->protocol,
        AVAHI_BROWSER_NEW,
        e->record,
        cbdata->record_resolver->userdata);

    return NULL;
}

static void scan_interface_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, gpointer userdata) {
    AvahiRecordResolver *s = userdata;
    struct cbdata cbdata = { s, i };

    g_assert(m);
    g_assert(i);
    g_assert(s);

    avahi_cache_walk(i->cache, s->key, scan_cache_callback, &cbdata);
}

AvahiRecordResolver *avahi_record_resolver_new(AvahiServer *server, gint interface, guchar protocol, AvahiKey *key, AvahiRecordResolverCallback callback, gpointer userdata) {
    AvahiRecordResolver *s, *t;
    GTimeVal tv;

    g_assert(server);
    g_assert(key);
    g_assert(callback);

    g_assert(!avahi_key_is_pattern(key));
    
    s = g_new(AvahiRecordResolver, 1);
    s->server = server;
    s->key = avahi_key_ref(key);
    s->interface = interface;
    s->protocol = protocol;
    s->callback = callback;
    s->userdata = userdata;
    s->n_query = 1;
    s->sec_delay = 1;

    avahi_server_post_query(s->server, s->interface, s->protocol, s->key);
    
    avahi_elapse_time(&tv, s->sec_delay*1000, 0);
    s->time_event = avahi_time_event_queue_add(server->time_event_queue, &tv, elapse, s);

    AVAHI_LLIST_PREPEND(AvahiRecordResolver, resolver, server->record_resolvers, s);

    /* Add the new entry to the record_resolver hash table */
    t = g_hash_table_lookup(server->record_resolver_hashtable, key);
    AVAHI_LLIST_PREPEND(AvahiRecordResolver, by_key, t, s);
    g_hash_table_replace(server->record_resolver_hashtable, key, t);

    /* Scan the caches */
    avahi_interface_monitor_walk(s->server->monitor, s->interface, s->protocol, scan_interface_callback, s);
    
    return s;
}

void avahi_record_resolver_free(AvahiRecordResolver *s) {
    AvahiRecordResolver *t;
    
    g_assert(s);

    AVAHI_LLIST_REMOVE(AvahiRecordResolver, resolver, s->server->record_resolvers, s);

    t = g_hash_table_lookup(s->server->record_resolver_hashtable, s->key);
    AVAHI_LLIST_REMOVE(AvahiRecordResolver, by_key, t, s);
    if (t)
        g_hash_table_replace(s->server->record_resolver_hashtable, t->key, t);
    else
        g_hash_table_remove(s->server->record_resolver_hashtable, s->key);
    
    avahi_time_event_queue_remove(s->server->time_event_queue, s->time_event);
    avahi_key_unref(s->key);

    
    g_free(s);
}

void avahi_resolver_notify(AvahiServer *server, AvahiInterface *i, AvahiRecord *record, AvahiBrowserEvent event) {
    AvahiRecordResolver *s;
    
    g_assert(server);
    g_assert(record);

    for (s = g_hash_table_lookup(server->record_resolver_hashtable, record->key); s; s = s->by_key_next)
        if (avahi_interface_match(i, s->interface, s->protocol))
            s->callback(s, i->hardware->index, i->protocol, event, record, s->userdata);
}

gboolean avahi_is_subscribed(AvahiServer *server, AvahiKey *k) {
    g_assert(server);
    g_assert(k);

    return !!g_hash_table_lookup(server->record_resolver_hashtable, k);
}
