#ifndef foocachehfoo
#define foocachehfoo

#include <glib.h>

struct _flxCache;
typedef struct _flxCache flxCache;

#include "prioq.h"
#include "server.h"
#include "llist.h"
#include "timeeventq.h"

typedef enum {
    FLX_CACHE_VALID,
    FLX_CACHE_EXPIRY1,
    FLX_CACHE_EXPIRY2,
    FLX_CACHE_EXPIRY3,
    FLX_CACHE_FINAL
} flxCacheEntryState;

typedef struct flxCacheEntry flxCacheEntry;

struct flxCacheEntry {
    flxCache *cache;
    flxRecord *record;
    GTimeVal timestamp;
    GTimeVal expiry;
    
    flxAddress origin;

    flxCacheEntryState state;
    flxTimeEvent *time_event;

    FLX_LLIST_FIELDS(flxCacheEntry, by_key);
    FLX_LLIST_FIELDS(flxCacheEntry, entry);
};

struct _flxCache {
    flxServer *server;
    
    flxInterface *interface;
    
    GHashTable *hash_table;

    FLX_LLIST_HEAD(flxCacheEntry, entries);
};

flxCache *flx_cache_new(flxServer *server, flxInterface *interface);
void flx_cache_free(flxCache *c);

flxCacheEntry *flx_cache_lookup_key(flxCache *c, flxKey *k);
flxCacheEntry *flx_cache_lookup_record(flxCache *c, flxRecord *r);

void flx_cache_update(flxCache *c, flxRecord *r, gboolean unique, const flxAddress *a);

void flx_cache_drop_record(flxCache *c,  flxRecord *r);

void flx_cache_dump(flxCache *c, FILE *f);

typedef gpointer flxCacheWalkCallback(flxCache *c, flxKey *pattern, flxCacheEntry *e, gpointer userdata);
gpointer flx_cache_walk(flxCache *c, flxKey *pattern, flxCacheWalkCallback cb, gpointer userdata);

gboolean flx_cache_entry_half_ttl(flxCache *c, flxCacheEntry *e);

#endif
