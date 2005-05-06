#ifndef foocachehfoo
#define foocachehfoo

#include <glib.h>

struct _AvahiCache;
typedef struct _AvahiCache AvahiCache;

#include "prioq.h"
#include "server.h"
#include "llist.h"
#include "timeeventq.h"

typedef enum {
    AVAHI_CACHE_VALID,
    AVAHI_CACHE_EXPIRY1,
    AVAHI_CACHE_EXPIRY2,
    AVAHI_CACHE_EXPIRY3,
    AVAHI_CACHE_FINAL
} AvahiCacheEntryState;

typedef struct AvahiCacheEntry AvahiCacheEntry;

struct AvahiCacheEntry {
    AvahiCache *cache;
    AvahiRecord *record;
    GTimeVal timestamp;
    GTimeVal expiry;
    
    AvahiAddress origin;

    AvahiCacheEntryState state;
    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiCacheEntry, by_key);
    AVAHI_LLIST_FIELDS(AvahiCacheEntry, entry);
};

struct _AvahiCache {
    AvahiServer *server;
    
    AvahiInterface *interface;
    
    GHashTable *hash_table;

    AVAHI_LLIST_HEAD(AvahiCacheEntry, entries);
};

AvahiCache *avahi_cache_new(AvahiServer *server, AvahiInterface *interface);
void avahi_cache_free(AvahiCache *c);

AvahiCacheEntry *avahi_cache_lookup_key(AvahiCache *c, AvahiKey *k);
AvahiCacheEntry *avahi_cache_lookup_record(AvahiCache *c, AvahiRecord *r);

void avahi_cache_update(AvahiCache *c, AvahiRecord *r, gboolean unique, const AvahiAddress *a);

void avahi_cache_drop_record(AvahiCache *c,  AvahiRecord *r);

void avahi_cache_dump(AvahiCache *c, FILE *f);

typedef gpointer AvahiCacheWalkCallback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, gpointer userdata);
gpointer avahi_cache_walk(AvahiCache *c, AvahiKey *pattern, AvahiCacheWalkCallback cb, gpointer userdata);

gboolean avahi_cache_entry_half_ttl(AvahiCache *c, AvahiCacheEntry *e);

#endif
