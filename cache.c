#include <string.h>

#include "cache.h"

static void remove_entry(flxCache *c, flxCacheEntry *e, gboolean remove_from_hash_table) {
    g_assert(c);
    g_assert(e);

    g_message("remvoin from cache: %p %p", c, e);
    
    if (remove_from_hash_table) {
        flxCacheEntry *t;
        t = g_hash_table_lookup(c->hash_table, e->record->key);
        FLX_LLIST_REMOVE(flxCacheEntry, by_name, t, e);
        if (t)
            g_hash_table_replace(c->hash_table, t->record->key, t);
        else
            g_hash_table_remove(c->hash_table, e->record->key);
    }
        
    flx_record_unref(e->record);

    if (e->time_event)
        flx_time_event_queue_remove(c->server->time_event_queue, e->time_event);
    
    g_free(e);
}

flxCache *flx_cache_new(flxServer *server, flxInterface *iface) {
    flxCache *c;
    g_assert(server);

    c = g_new(flxCache, 1);
    c->server = server;
    c->interface = iface;
    c->hash_table = g_hash_table_new((GHashFunc) flx_key_hash, (GEqualFunc) flx_key_equal);

    return c;
}

gboolean remove_func(gpointer key, gpointer value, gpointer user_data) {
    flxCacheEntry *e, *next;

    for (e = value; e; e = next) {
        next = e->by_name_next;
        remove_entry(user_data, e, FALSE);
    }
    
    return TRUE;
}

void flx_cache_free(flxCache *c) {
    g_assert(c);

    g_hash_table_foreach_remove(c->hash_table, remove_func, c);
    g_hash_table_destroy(c->hash_table);
    
    g_free(c);
}

flxCacheEntry *flx_cache_lookup_key(flxCache *c, flxKey *k) {
    g_assert(c);
    g_assert(k);

    return g_hash_table_lookup(c->hash_table, k);
}

flxCacheEntry *flx_cache_lookup_record(flxCache *c, flxRecord *r) {
    flxCacheEntry *e;
    g_assert(c);
    g_assert(r);

    for (e = flx_cache_lookup_key(c, r->key); e; e = e->by_name_next)
        if (e->record->size == r->size && !memcmp(e->record->data, r->data, r->size))
            return e;

    return NULL;
}

static void next_expiry(flxCache *c, flxCacheEntry *e, guint percent);

static void elapse_func(flxTimeEvent *t, void *userdata) {
    flxCacheEntry *e = userdata;
    
    g_assert(t);
    g_assert(e);

    if (e->state == FLX_CACHE_FINAL) {
        remove_entry(e->cache, e, TRUE);
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

        /* Request a cache update */
        flx_interface_post_query(e->cache->interface, e->record->key);

        /* Check again later */
        next_expiry(e->cache, e, percent);
    }
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

    if (e->time_event)
        flx_time_event_queue_update(c->server->time_event_queue, e->time_event, &e->expiry);
    else
        e->time_event = flx_time_event_queue_add(c->server->time_event_queue, &e->expiry, elapse_func, e);
}

flxCacheEntry *flx_cache_update(flxCache *c, flxRecord *r, gboolean unique, const flxAddress *a) {
    flxCacheEntry *e, *t;
    gchar *txt;
    
    g_assert(c);
    g_assert(r && r->ref >= 1);

    g_message("cache update: %s", (txt = flx_record_to_string(r)));
    g_free(txt);

    if ((t = e = flx_cache_lookup_key(c, r->key))) {

/*         g_message("found prev cache entry"); */

        if (unique) {
            /* Drop all entries but the first which we replace */
            while (e->by_name_next)
                remove_entry(c, e->by_name_next, TRUE);

        } else {
            /* Look for exactly the same entry */
            for (; e; e = e->by_name_next)
                if (flx_record_equal(e->record, r))
                    break;
        }
    }
    
    if (e) {

/*         g_message("found matching cache entry"); */

        /* We are the first in the linked list so let's replace the hash table key with the new one */
        if (e->by_name_prev == NULL)
            g_hash_table_replace(c->hash_table, r->key, e);
        
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
        FLX_LLIST_PREPEND(flxCacheEntry, by_name, t, e);
        g_hash_table_replace(c->hash_table, e->record->key, t);
    } 

    e->origin = *a;
    g_get_current_time(&e->timestamp);
    next_expiry(c, e, 80);
    e->state = FLX_CACHE_VALID;

    return e;
}

void flx_cache_drop_key(flxCache *c, flxKey *k) {
    flxCacheEntry *e;
    
    g_assert(c);
    g_assert(k);

    while ((e = flx_cache_lookup_key(c, k)))
        remove_entry(c, e, TRUE);
}

void flx_cache_drop_record(flxCache *c, flxRecord *r) {
    flxCacheEntry *e;
    
    g_assert(c);
    g_assert(r);

    if ((e = flx_cache_lookup_record(c, r))) 
        remove_entry(c, e, TRUE);
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
