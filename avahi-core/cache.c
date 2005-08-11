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

#include <string.h>

#include "util.h"
#include "cache.h"
#include "log.h"

#define AVAHI_MAX_CACHE_ENTRIES 200

static void remove_entry(AvahiCache *c, AvahiCacheEntry *e) {
    AvahiCacheEntry *t;

    g_assert(c);
    g_assert(e);

/*     avahi_log_debug("removing from cache: %p %p", c, e); */

    /* Remove from hash table */
    t = g_hash_table_lookup(c->hash_table, e->record->key);
    AVAHI_LLIST_REMOVE(AvahiCacheEntry, by_key, t, e);
    if (t)
        g_hash_table_replace(c->hash_table, t->record->key, t);
    else
        g_hash_table_remove(c->hash_table, e->record->key);

    /* Remove from linked list */
    AVAHI_LLIST_REMOVE(AvahiCacheEntry, entry, c->entries, e);
        
    if (e->time_event)
        avahi_time_event_queue_remove(c->server->time_event_queue, e->time_event);

    avahi_browser_notify(c->server, c->interface, e->record, AVAHI_BROWSER_REMOVE);

    avahi_record_unref(e->record);
    
    g_free(e);

    g_assert(c->n_entries-- >= 1);
}

AvahiCache *avahi_cache_new(AvahiServer *server, AvahiInterface *iface) {
    AvahiCache *c;
    g_assert(server);

    c = g_new(AvahiCache, 1);
    c->server = server;
    c->interface = iface;
    c->hash_table = g_hash_table_new((GHashFunc) avahi_key_hash, (GEqualFunc) avahi_key_equal);

    AVAHI_LLIST_HEAD_INIT(AvahiCacheEntry, c->entries);
    c->n_entries = 0;
    
    return c;
}

void avahi_cache_free(AvahiCache *c) {
    g_assert(c);

    while (c->entries)
        remove_entry(c, c->entries);
    g_assert(c->n_entries == 0);
    
    g_hash_table_destroy(c->hash_table);
    
    g_free(c);
}

AvahiCacheEntry *avahi_cache_lookup_key(AvahiCache *c, AvahiKey *k) {
    g_assert(c);
    g_assert(k);

    g_assert(!avahi_key_is_pattern(k));
    
    return g_hash_table_lookup(c->hash_table, k);
}

gpointer avahi_cache_walk(AvahiCache *c, AvahiKey *pattern, AvahiCacheWalkCallback cb, gpointer userdata) {
    gpointer ret;
    
    g_assert(c);
    g_assert(pattern);
    g_assert(cb);
    
    if (avahi_key_is_pattern(pattern)) {
        AvahiCacheEntry *e, *n;
        
        for (e = c->entries; e; e = n) {
            n = e->entry_next;
            
            if (avahi_key_pattern_match(pattern, e->record->key))
                if ((ret = cb(c, pattern, e, userdata)))
                    return ret;
        }
        
    } else {
        AvahiCacheEntry *e, *n;

        for (e = avahi_cache_lookup_key(c, pattern); e; e = n) {
            n = e->by_key_next;
                
            if ((ret = cb(c, pattern, e, userdata)))
                return ret;
        }
    }

    return NULL;
}

static gpointer lookup_record_callback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, void *userdata) {
    g_assert(c);
    g_assert(pattern);
    g_assert(e);

    if (avahi_record_equal_no_ttl(e->record, userdata))
        return e;

    return NULL;
}

AvahiCacheEntry *avahi_cache_lookup_record(AvahiCache *c, AvahiRecord *r) {
    g_assert(c);
    g_assert(r);

    return avahi_cache_walk(c, r->key, lookup_record_callback, r);
}

static void next_expiry(AvahiCache *c, AvahiCacheEntry *e, guint percent);

static void elapse_func(AvahiTimeEvent *t, void *userdata) {
    AvahiCacheEntry *e = userdata;
/*     gchar *txt; */
    
    g_assert(t);
    g_assert(e);

/*     txt = avahi_record_to_string(e->record); */

    if (e->state == AVAHI_CACHE_FINAL) {
        remove_entry(e->cache, e);
/*         avahi_log_debug("Removing entry from cache due to expiration (%s)", txt); */
    } else {
        guint percent = 0;
    
        switch (e->state) {
            case AVAHI_CACHE_VALID:
                e->state = AVAHI_CACHE_EXPIRY1;
                percent = 85;
                break;
                
            case AVAHI_CACHE_EXPIRY1:
                e->state = AVAHI_CACHE_EXPIRY2;
                percent = 90;
                break;
            case AVAHI_CACHE_EXPIRY2:
                e->state = AVAHI_CACHE_EXPIRY3;
                percent = 95;
                break;
                
            case AVAHI_CACHE_EXPIRY3:
                e->state = AVAHI_CACHE_FINAL;
                percent = 100;
                break;

            default:
                ;
        }

        g_assert(percent > 0);

        /* Request a cache update, if we are subscribed to this entry */
        if (avahi_is_subscribed(e->cache->server, e->cache->interface, e->record->key)) {
/*             avahi_log_debug("Requesting cache entry update at %i%% for %s.", percent, txt);   */
            avahi_interface_post_query(e->cache->interface, e->record->key, TRUE);
        }

        /* Check again later */
        next_expiry(e->cache, e, percent);
    }

/*     g_free(txt); */
}

static void update_time_event(AvahiCache *c, AvahiCacheEntry *e) {
    g_assert(c);
    g_assert(e);
    
    if (e->time_event)
        avahi_time_event_queue_update(c->server->time_event_queue, e->time_event, &e->expiry);
    else
        e->time_event = avahi_time_event_queue_add(c->server->time_event_queue, &e->expiry, elapse_func, e);
}

static void next_expiry(AvahiCache *c, AvahiCacheEntry *e, guint percent) {
    g_assert(c);
    g_assert(e);
    g_assert(percent > 0 && percent <= 100);
    AvahiUsec usec;
/*     gchar *txt; */

    usec = ((AvahiUsec) e->record->ttl) * 10000;

    /* 2% jitter */
    usec = (AvahiUsec) g_random_double_range((gdouble) (usec*percent), (gdouble) (usec*(percent+2)));
/*     g_message("next expiry: %lli (%s)", usec / 1000000, txt = avahi_record_to_string(e->record)); */
/*     g_free(txt); */
    
    e->expiry = e->timestamp;
    avahi_timeval_add(&e->expiry, usec);
    
/*     g_message("wake up in +%lu seconds", e->expiry.tv_sec - e->timestamp.tv_sec); */
    
    update_time_event(c, e);
}

static void expire_in_one_second(AvahiCache *c, AvahiCacheEntry *e) {
    g_assert(c);
    g_assert(e);
    
    e->state = AVAHI_CACHE_FINAL;
    gettimeofday(&e->expiry, NULL);
    avahi_timeval_add(&e->expiry, 1000000); /* 1s */
    update_time_event(c, e);
}

void avahi_cache_update(AvahiCache *c, AvahiRecord *r, gboolean cache_flush, const AvahiAddress *a) {
/*     gchar *txt; */
    
    g_assert(c);
    g_assert(r && r->ref >= 1);

/*     txt = avahi_record_to_string(r); */

    if (r->ttl == 0) {
        /* This is a goodbye request */

        AvahiCacheEntry *e;

        if ((e = avahi_cache_lookup_record(c, r)))
            expire_in_one_second(c, e);

    } else {
        AvahiCacheEntry *e = NULL, *first;
        struct timeval now;

        gettimeofday(&now, NULL);

        /* This is an update request */

        if ((first = avahi_cache_lookup_key(c, r->key))) {
            
            if (cache_flush) {

                /* For unique entries drop all entries older than one second */
                for (e = first; e; e = e->by_key_next) {
                    AvahiUsec t;

                    t = avahi_timeval_diff(&now, &e->timestamp);

                    if (t > 1000000)
                        expire_in_one_second(c, e);
                }
            }
                
            /* Look for exactly the same entry */
            for (e = first; e; e = e->by_key_next)
                if (avahi_record_equal_no_ttl(e->record, r))
                    break;
        }
    
        if (e) {
            
/*             avahi_log_debug("found matching cache entry");  */

            /* We need to update the hash table key if we replace the
             * record */
            if (e->by_key_prev == NULL)
                g_hash_table_replace(c->hash_table, r->key, e);
            
            /* Update the record */
            avahi_record_unref(e->record);
            e->record = avahi_record_ref(r);

/*             avahi_log_debug("cache: updating %s", txt);   */
            
        } else {
            /* No entry found, therefore we create a new one */
            
/*             avahi_log_debug("cache: couldn't find matching cache entry for %s", txt);   */

            if (c->n_entries >= AVAHI_MAX_CACHE_ENTRIES)
                return;

            c->n_entries++;
            
            e = g_new(AvahiCacheEntry, 1);
            e->cache = c;
            e->time_event = NULL;
            e->record = avahi_record_ref(r);

            /* Append to hash table */
            AVAHI_LLIST_PREPEND(AvahiCacheEntry, by_key, first, e);
            g_hash_table_replace(c->hash_table, e->record->key, first);

            /* Append to linked list */
            AVAHI_LLIST_PREPEND(AvahiCacheEntry, entry, c->entries, e);

            /* Notify subscribers */
            avahi_browser_notify(c->server, c->interface, e->record, AVAHI_BROWSER_NEW);
        } 
        
        e->origin = *a;
        e->timestamp = now;
        next_expiry(c, e, 80);
        e->state = AVAHI_CACHE_VALID;
        e->cache_flush = cache_flush;
    }

/*     g_free(txt);  */
}

struct dump_data {
    AvahiDumpCallback callback;
    gpointer userdata;
};

static void dump_callback(gpointer key, gpointer data, gpointer userdata) {
    AvahiCacheEntry *e = data;
    AvahiKey *k = key;
    struct dump_data *dump_data = userdata;

    g_assert(k);
    g_assert(e);
    g_assert(data);

    for (; e; e = e->by_key_next) {
        gchar *t = avahi_record_to_string(e->record);
        dump_data->callback(t, dump_data->userdata);
        g_free(t);
    }
}

void avahi_cache_dump(AvahiCache *c, AvahiDumpCallback callback, gpointer userdata) {
    struct dump_data data;

    g_assert(c);
    g_assert(callback);

    callback(";;; CACHE DUMP FOLLOWS ;;;", userdata);

    data.callback = callback;
    data.userdata = userdata;

    g_hash_table_foreach(c->hash_table, dump_callback, &data);
}

gboolean avahi_cache_entry_half_ttl(AvahiCache *c, AvahiCacheEntry *e) {
    struct timeval now;
    AvahiUsec age;
    
    g_assert(c);
    g_assert(e);

    gettimeofday(&now, NULL);

    age = avahi_timeval_diff(&now, &e->timestamp)/1000000;

/*     avahi_log_debug("age: %lli, ttl/2: %u", age, e->record->ttl);  */
    
    return age >= e->record->ttl/2;
}

void avahi_cache_flush(AvahiCache *c) {
    g_assert(c);

    while (c->entries)
        remove_entry(c, c->entries);
}
