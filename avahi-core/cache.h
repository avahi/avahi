#ifndef foocachehfoo
#define foocachehfoo

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

#include <glib.h>

typedef struct AvahiCache AvahiCache;

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
    gboolean cache_flush;
    
    AvahiAddress origin;

    AvahiCacheEntryState state;
    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiCacheEntry, by_key);
    AVAHI_LLIST_FIELDS(AvahiCacheEntry, entry);
};

struct AvahiCache {
    AvahiServer *server;
    
    AvahiInterface *interface;
    
    GHashTable *hash_table;

    AVAHI_LLIST_HEAD(AvahiCacheEntry, entries);
};

AvahiCache *avahi_cache_new(AvahiServer *server, AvahiInterface *interface);
void avahi_cache_free(AvahiCache *c);

AvahiCacheEntry *avahi_cache_lookup_key(AvahiCache *c, AvahiKey *k);
AvahiCacheEntry *avahi_cache_lookup_record(AvahiCache *c, AvahiRecord *r);

void avahi_cache_update(AvahiCache *c, AvahiRecord *r, gboolean cache_flush, const AvahiAddress *a);

void avahi_cache_dump(AvahiCache *c, FILE *f);

typedef gpointer AvahiCacheWalkCallback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, gpointer userdata);
gpointer avahi_cache_walk(AvahiCache *c, AvahiKey *pattern, AvahiCacheWalkCallback cb, gpointer userdata);

gboolean avahi_cache_entry_half_ttl(AvahiCache *c, AvahiCacheEntry *e);

void avahi_cache_flush(AvahiCache *c);

#endif
