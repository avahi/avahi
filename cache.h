#ifndef foocachehfoo
#define foocachehfoo

#include <glib.h>

struct _flxCache;
typedef struct _flxCache flxCache;

#include "prioq.h"
#include "server.h"
#include "llist.h"

typedef enum {
    FLX_CACHE_VALID,
    FLX_CACHE_EXPIRY1,
    FLX_CACHE_EXPIRY2,
    FLX_CACHE_EXPIRY3
        
} flxCacheEntryState;

typedef struct flxCacheEntry flxCacheEntry;

struct flxCacheEntry {
    flxRecord *record;
    GTimeVal timestamp;
    GTimeVal expiry;
    
    flxAddress origin;

    flxCacheEntryState state;

    FLX_LLIST_FIELDS(flxCacheEntry, by_name);

    flxPrioQueueNode *node;
    
};

struct _flxCache {
    flxServer *server;
    
    flxInterface *interface;
    
    GHashTable *hash_table;
};

flxCache *flx_cache_new(flxServer *server, flxInterface *interface);
void flx_cache_free(flxCache *c);

flxCacheEntry *flx_cache_lookup_key(flxCache *c, flxKey *k);
flxCacheEntry *flx_cache_lookup_record(flxCache *c, flxRecord *r);

flxCacheEntry *flx_cache_update(flxCache *c, flxRecord *r, gboolean unique, const flxAddress *a);

void flx_cache_drop_key(flxCache *c, flxKey *k);
void flx_cache_drop_record(flxCache *c,  flxRecord *r);

void flx_cache_dump(flxCache *c, FILE *f);

#endif
