#ifndef foocachehfoo
#define foocachehfoo

typedef enum {
    FLX_CACHE_VALID,
    FLX_CACHE_EXPIRY1,
    FLX_CACHE_EXPIRY2,
    FLX_CACHE_EXPIRY3
        
} flxCacheEntry;

typedef struct flxCacheEntry {
    GTimeVal timestamp;
    flxRecord rr;
    gint interface;
    flxAddress origin;

    flxCacheEntryState state;
    
} flxCacheEntry;

#endif
