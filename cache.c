#include <string.h>

#include "util.h"
#include "cache.h"

static void remove_entry(flxCache *c, flxCacheEntry *e) {
    flxCacheEntry *t;

    g_assert(c);
    g_assert(e);

/*     g_message("removing from cache: %p %p", c, e); */

    /* Remove from hash table */
    t = g_hash_table_lookup(c->hash_table, e->record->key);
    FLX_LLIST_REMOVE(flxCacheEntry, by_key, t, e);
    if (t)
        g_hash_table_replace(c->hash_table, t->record->key, t);
    else
        g_hash_table_remove(c->hash_table, e->record->key);

    /* Remove from linked list */
    FLX_LLIST_REMOVE(flxCacheEntry, entry, c->entries, e);
        
    if (e->time_event)
        flx_time_event_queue_remove(c->server->time_event_queue, e->time_event);

    flx_subscription_notify(c->server, c->interface, e->record, FLX_SUBSCRIPTION_REMOVE);

    flx_record_unref(e->record);
    
    g_free(e);
}

flxCache *flx_cache_new(flxServer *server, flxInterface *iface) {
    flxCache *c;
    g_assert(server);

    c = g_new(flxCache, 1);
    c->server = server;
    c->interface = iface;
    c->hash_table = g_hash_table_new((GHashFunc) flx_key_hash, (GEqualFunc) flx_key_equal);

    FLX_LLIST_HEAD_INIT(flxCacheEntry, c->entries);
    
    return c;
}

void flx_cache_free(flxCache *c) {
    g_assert(c);

    while (c->entries)
        remove_entry(c, c->entries);
    
    g_hash_table_destroy(c->hash_table);
    
    g_free(c);
}

flxCacheEntry *flx_cache_lookup_key(flxCache *c, flxKey *k) {
    g_assert(c);
    g_assert(k);

    g_assert(!flx_key_is_pattern(k));
    
    return g_hash_table_lookup(c->hash_table, k);
}

gpointer flx_cache_walk(flxCache *c, flxKey *pattern, flxCacheWalkCallback cb, gpointer userdata) {
    gpointer ret;
    
    g_assert(c);
    g_assert(pattern);
    g_assert(cb);
    
    if (flx_key_is_pattern(pattern)) {
        flxCacheEntry *e, *n;
        
        for (e = c->entries; e; e = n) {
            n = e->entry_next;
            
            if (flx_key_pattern_match(pattern, e->record->key))
                if ((ret = cb(c, pattern, e, userdata)))
                    return ret;
        }
        
    } else {
        flxCacheEntry *e, *n;

        for (e = flx_cache_lookup_key(c, pattern); e; e = n) {
            n = e->by_key_next;
                
            if ((ret = cb(c, pattern, e, userdata)))
                return ret;
        }
    }

    return NULL;
}

static gpointer lookup_record_callback(flxCache *c, flxKey *pattern, flxCacheEntry *e, void *userdata) {
    g_assert(c);
    g_assert(pattern);
    g_assert(e);

    if (flx_record_equal_no_ttl(e->record, userdata))
        return e;

    return NULL;
}

flxCacheEntry *flx_cache_lookup_record(flxCache *c, flxRecord *r) {
    flxCacheEntry *e;
    
    g_assert(c);
    g_assert(r);

    return flx_cache_walk(c, r->key, lookup_record_callback, r);
}

static void next_expiry(flxCache *c, flxCacheEntry *e, guint percent);

static void elapse_func(flxTimeEvent *t, void *userdata) {
    flxCacheEntry *e = userdata;
    
    g_assert(t);
    g_assert(e);

    if (e->state == FLX_CACHE_FINAL) {
        remove_entry(e->cache, e);
        g_message("Removing entry from cache due to expiration");
    } else {
        guint percent = 0;
    
        switch (e->state) {
            case FLX_CACHE_VALID:
                e->state = FLX_CACHE_EXPIRY1;
                percent = 85;
                break;
                
            case FLX_CACHE_EXPIRY1:
                e->state = FLX_CACHE_EXPIRY2;
                percent = 90;
                break;
            case FLX_CACHE_EXPIRY2:
                e->state = FLX_CACHE_EXPIRY3;
                percent = 95;
                break;
                
            case FLX_CACHE_EXPIRY3:
                e->state = FLX_CACHE_FINAL;
                percent = 100;
                break;

            default:
                ;
        }

        g_assert(percent > 0);

        g_message("Requesting cache entry update at %i%%.", percent);

        /* Request a cache update, if we are subscribed to this entry */
        if (flx_is_subscribed(e->cache->server, e->record->key))
            flx_interface_post_query(e->cache->interface, e->record->key, TRUE);

        /* Check again later */
        next_expiry(e->cache, e, percent);
    }
}

static void update_time_event(flxCache *c, flxCacheEntry *e) {
    g_assert(c);
    g_assert(e);
    
    if (e->time_event)
        flx_time_event_queue_update(c->server->time_event_queue, e->time_event, &e->expiry);
    else
        e->time_event = flx_time_event_queue_add(c->server->time_event_queue, &e->expiry, elapse_func, e);
}

static void next_expiry(flxCache *c, flxCacheEntry *e, guint percent) {
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

void flx_cache_update(flxCache *c, flxRecord *r, gboolean unique, const flxAddress *a) {
    flxCacheEntry *e, *t;
    gchar *txt;
    
    g_assert(c);
    g_assert(r && r->ref >= 1);

    g_message("cache update: %s", (txt = flx_record_to_string(r)));
    g_free(txt);

    if (r->ttl == 0) {

        /* This is a goodbye request */

        if ((e = flx_cache_lookup_record(c, r))) {

            e->state = FLX_CACHE_FINAL;
            g_get_current_time(&e->timestamp);
            e->expiry = e->timestamp;
            g_time_val_add(&e->expiry, 1000000); /* 1s */
            update_time_event(c, e);
        }

    } else {

        /* This is an update request */

        if ((t = e = flx_cache_lookup_key(c, r->key))) {
        
            if (unique) {
                
                /* For unique records, remove all entries but one */
                while (e->by_key_next)
                    remove_entry(c, e->by_key_next);
                
            } else {
                
                /* For non-unique record, look for exactly the same entry */
                for (; e; e = e->by_key_next)
                    if (flx_record_equal_no_ttl(e->record, r))
                        break;
            }
        }
    
        if (e) {
            
/*         g_message("found matching cache entry"); */
            
            /* We are the first in the linked list so let's replace the hash table key with the new one */
            if (e->by_key_prev == NULL)
                g_hash_table_replace(c->hash_table, r->key, e);
            
            /* Notify subscribers */
            if (!flx_record_equal_no_ttl(e->record, r))
                flx_subscription_notify(c->server, c->interface, r, FLX_SUBSCRIPTION_CHANGE);    
            
            /* Update the record */
            flx_record_unref(e->record);
            e->record = flx_record_ref(r);
            
        } else {
            /* No entry found, therefore we create a new one */
            
/*         g_message("couldn't find matching cache entry"); */
            
            e = g_new(flxCacheEntry, 1);
            e->cache = c;
            e->time_event = NULL;
            e->record = flx_record_ref(r);

            /* Append to hash table */
            FLX_LLIST_PREPEND(flxCacheEntry, by_key, t, e);
            g_hash_table_replace(c->hash_table, e->record->key, t);

            /* Append to linked list */
            FLX_LLIST_PREPEND(flxCacheEntry, entry, c->entries, e);

            /* Notify subscribers */
            flx_subscription_notify(c->server, c->interface, e->record, FLX_SUBSCRIPTION_NEW);
        } 
        
        e->origin = *a;
        g_get_current_time(&e->timestamp);
        next_expiry(c, e, 80);
        e->state = FLX_CACHE_VALID;
    }
}

static gpointer drop_key_callback(flxCache *c, flxKey *pattern, flxCacheEntry *e, gpointer userdata) {
    g_assert(c);
    g_assert(pattern);
    g_assert(e);

    remove_entry(c, e);
    return NULL;
}

void flx_cache_drop_key(flxCache *c, flxKey *k) {
    g_assert(c);
    g_assert(k);

    flx_cache_walk(c, k, drop_key_callback, NULL);
}

void flx_cache_drop_record(flxCache *c, flxRecord *r) {
    flxCacheEntry *e;
    
    g_assert(c);
    g_assert(r);

    if ((e = flx_cache_lookup_record(c, r))) 
        remove_entry(c, e);
}

static void func(gpointer key, gpointer data, gpointer userdata) {
    flxCacheEntry *e = data;
    flxKey *k = key;
    gchar *t;

    t = flx_record_to_string(e->record);
    fprintf((FILE*) userdata, "%s\n", t);
    g_free(t);
}

void flx_cache_dump(flxCache *c, FILE *f) {
    g_assert(c);
    g_assert(f);

    fprintf(f, ";;; CACHE DUMP FOLLOWS ;;;\n");
    g_hash_table_foreach(c->hash_table, func, f);
}

gboolean flx_cache_entry_half_ttl(flxCache *c, flxCacheEntry *e) {
    GTimeVal now;
    guint age;
    
    g_assert(c);
    g_assert(e);

    g_get_current_time(&now);

    age = flx_timeval_diff(&now, &e->timestamp)/1000000;

    g_message("age: %u, ttl/2: %u", age, e->record->ttl);
    
    return age >= e->record->ttl/2;
}
