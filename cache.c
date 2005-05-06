#include <string.h>

#include "util.h"
#include "cache.h"

static void remove_entry(AvahiCache *c, AvahiCacheEntry *e) {
    AvahiCacheEntry *t;

    g_assert(c);
    g_assert(e);

/*     g_message("removing from cache: %p %p", c, e); */

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

    avahi_subscription_notify(c->server, c->interface, e->record, AVAHI_SUBSCRIPTION_REMOVE);

    avahi_record_unref(e->record);
    
    g_free(e);
}

AvahiCache *avahi_cache_new(AvahiServer *server, AvahiInterface *iface) {
    AvahiCache *c;
    g_assert(server);

    c = g_new(AvahiCache, 1);
    c->server = server;
    c->interface = iface;
    c->hash_table = g_hash_table_new((GHashFunc) avahi_key_hash, (GEqualFunc) avahi_key_equal);

    AVAHI_LLIST_HEAD_INIT(AvahiCacheEntry, c->entries);
    
    return c;
}

void avahi_cache_free(AvahiCache *c) {
    g_assert(c);

    while (c->entries)
        remove_entry(c, c->entries);
    
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
    AvahiCacheEntry *e;
    
    g_assert(c);
    g_assert(r);

    return avahi_cache_walk(c, r->key, lookup_record_callback, r);
}

static void next_expiry(AvahiCache *c, AvahiCacheEntry *e, guint percent);

static void elapse_func(AvahiTimeEvent *t, void *userdata) {
    AvahiCacheEntry *e = userdata;
    
    g_assert(t);
    g_assert(e);

    if (e->state == AVAHI_CACHE_FINAL) {
        remove_entry(e->cache, e);
        g_message("Removing entry from cache due to expiration");
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

        g_message("Requesting cache entry update at %i%%.", percent);

        /* Request a cache update, if we are subscribed to this entry */
        if (avahi_is_subscribed(e->cache->server, e->record->key))
            avahi_interface_post_query(e->cache->interface, e->record->key, TRUE);

        /* Check again later */
        next_expiry(e->cache, e, percent);
    }
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
    gulong usec;

    g_assert(c);
    g_assert(e);
    g_assert(percent > 0 && percent <= 100);

    e->expiry = e->timestamp;

    usec = e->record->ttl * 10000;

    /* 2% jitter */
    usec = g_random_int_range(usec*percent, usec*(percent+2));
    
    g_time_val_add(&e->expiry, usec);
    update_time_event(c, e);
}

void avahi_cache_update(AvahiCache *c, AvahiRecord *r, gboolean unique, const AvahiAddress *a) {
    AvahiCacheEntry *e, *t;
    gchar *txt;
    
    g_assert(c);
    g_assert(r && r->ref >= 1);

    g_message("cache update: %s", (txt = avahi_record_to_string(r)));
    g_free(txt);

    if (r->ttl == 0) {

        /* This is a goodbye request */

        if ((e = avahi_cache_lookup_record(c, r))) {

            e->state = AVAHI_CACHE_FINAL;
            g_get_current_time(&e->timestamp);
            e->expiry = e->timestamp;
            g_time_val_add(&e->expiry, 1000000); /* 1s */
            update_time_event(c, e);
        }

    } else {

        /* This is an update request */

        if ((t = e = avahi_cache_lookup_key(c, r->key))) {
        
            if (unique) {
                
                /* For unique records, remove all entries but one */
                while (e->by_key_next)
                    remove_entry(c, e->by_key_next);
                
            } else {
                
                /* For non-unique record, look for exactly the same entry */
                for (; e; e = e->by_key_next)
                    if (avahi_record_equal_no_ttl(e->record, r))
                        break;
            }
        }
    
        if (e) {
            
/*         g_message("found matching cache entry"); */
            
            /* We are the first in the linked list so let's replace the hash table key with the new one */
            if (e->by_key_prev == NULL)
                g_hash_table_replace(c->hash_table, r->key, e);
            
            /* Notify subscribers */
            if (!avahi_record_equal_no_ttl(e->record, r))
                avahi_subscription_notify(c->server, c->interface, r, AVAHI_SUBSCRIPTION_CHANGE);    
            
            /* Update the record */
            avahi_record_unref(e->record);
            e->record = avahi_record_ref(r);
            
        } else {
            /* No entry found, therefore we create a new one */
            
/*         g_message("couldn't find matching cache entry"); */
            
            e = g_new(AvahiCacheEntry, 1);
            e->cache = c;
            e->time_event = NULL;
            e->record = avahi_record_ref(r);

            /* Append to hash table */
            AVAHI_LLIST_PREPEND(AvahiCacheEntry, by_key, t, e);
            g_hash_table_replace(c->hash_table, e->record->key, t);

            /* Append to linked list */
            AVAHI_LLIST_PREPEND(AvahiCacheEntry, entry, c->entries, e);

            /* Notify subscribers */
            avahi_subscription_notify(c->server, c->interface, e->record, AVAHI_SUBSCRIPTION_NEW);
        } 
        
        e->origin = *a;
        g_get_current_time(&e->timestamp);
        next_expiry(c, e, 80);
        e->state = AVAHI_CACHE_VALID;
    }
}

static gpointer drop_key_callback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, gpointer userdata) {
    g_assert(c);
    g_assert(pattern);
    g_assert(e);

    remove_entry(c, e);
    return NULL;
}

void avahi_cache_drop_key(AvahiCache *c, AvahiKey *k) {
    g_assert(c);
    g_assert(k);

    avahi_cache_walk(c, k, drop_key_callback, NULL);
}

void avahi_cache_drop_record(AvahiCache *c, AvahiRecord *r) {
    AvahiCacheEntry *e;
    
    g_assert(c);
    g_assert(r);

    if ((e = avahi_cache_lookup_record(c, r))) 
        remove_entry(c, e);
}

static void func(gpointer key, gpointer data, gpointer userdata) {
    AvahiCacheEntry *e = data;
    AvahiKey *k = key;
    gchar *t;

    t = avahi_record_to_string(e->record);
    fprintf((FILE*) userdata, "%s\n", t);
    g_free(t);
}

void avahi_cache_dump(AvahiCache *c, FILE *f) {
    g_assert(c);
    g_assert(f);

    fprintf(f, ";;; CACHE DUMP FOLLOWS ;;;\n");
    g_hash_table_foreach(c->hash_table, func, f);
}

gboolean avahi_cache_entry_half_ttl(AvahiCache *c, AvahiCacheEntry *e) {
    GTimeVal now;
    guint age;
    
    g_assert(c);
    g_assert(e);

    g_get_current_time(&now);

    age = avahi_timeval_diff(&now, &e->timestamp)/1000000;

    g_message("age: %u, ttl/2: %u", age, e->record->ttl);
    
    return age >= e->record->ttl/2;
}
