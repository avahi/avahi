#include <string.h>

#include "cache.h"

static void remove_entry(flxCache *c, flxCacheEntry *e, gboolean remove_from_hash_table) {
    g_assert(c);
    g_assert(e);

    if (remove_from_hash_table) {
        flxCacheEntry *t;
        t = g_hash_table_lookup(c->hash_table, &e->record->key);
        FLX_LLIST_REMOVE(flxCacheEntry, by_name, t, e);
        if (t)
            g_hash_table_replace(c->hash_table, &t->record->key, t);
        else
            g_hash_table_remove(c->hash_table, &e->record->key);
    }
        
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

flxCacheEntry *flx_cache_update(flxCache *c, flxRecord *r, gboolean unique, const flxAddress *a) {
    flxCacheEntry *e, *t;
    
    g_assert(c);
    g_assert(r);

    if ((t = e = flx_cache_lookup_key(c, r->key))) {

        if (unique) {
            flxCacheEntry *n;
            /* Drop all entries but the first which we replace */

            while (e->by_name_next)
                remove_entry(c, e->by_name_next, TRUE);

            g_free(e->record->data);
            e->record->data = g_memdup(r->data, r->size);
            e->record->size = r->size;
            e->record->ttl = r->ttl;

        } else {
            /* Look for exactly the same entry */

            for (; e; e = e->by_name_next) {
                if (e->record->size == r->size &&
                    !memcmp(e->record->data, r->data, r->size)) {

                    /* We found it, so let's update the TTL */
                    e->record->ttl = r->ttl;
                    break;
                }
            }
        }
    }

    if (!e) {
        /* No entry found, therefore we create a new one */
        
        e = g_new(flxCacheEntry, 1);
        e->node = NULL;

        e->record = flx_record_ref(r);
        FLX_LLIST_PREPEND(flxCacheEntry, by_name, t, e);
        g_hash_table_replace(c->hash_table, e->record->key, e);
    } 

    e->origin = *a;
    
    g_get_current_time(&e->timestamp);
    e->expiry = e->timestamp;
    g_time_val_add(&e->expiry, e->record->ttl * 1000000);

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

    gchar *s, *t;

    s = flx_key_to_string(k);
    t = flx_record_to_string(e->record);

    fprintf((FILE*) userdata, "%s %s\n", s, t);
    
    g_free(s);
    g_free(t);
}

void flx_cache_dump(flxCache *c, FILE *f) {
    g_assert(c);
    g_assert(f);

    fprintf(f, ";;; CACHE DUMP FOLLOWS ;;;\n");
    g_hash_table_foreach(c->hash_table, func, f);
}
