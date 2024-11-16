#ifndef foowideareahfoo
#define foowideareahfoo

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

#include "lookup.h"
#include "browse.h"

typedef struct AvahiWideAreaLookupEngine AvahiWideAreaLookupEngine;
typedef struct AvahiWideAreaLookup AvahiWideAreaLookup;

typedef void (*AvahiWideAreaLookupCallback)(
    AvahiWideAreaLookupEngine *e,
    AvahiBrowserEvent event,
    AvahiLookupResultFlags flags,
    AvahiRecord *r,
    void *userdata);

/** Create new wide area engine.
 *
 * @param s Server configuration.
 */
AvahiWideAreaLookupEngine *avahi_wide_area_engine_new(AvahiServer *s);

/** Release wide area engine resources. */
void avahi_wide_area_engine_free(AvahiWideAreaLookupEngine *e);

unsigned avahi_wide_area_scan_cache(AvahiWideAreaLookupEngine *e, AvahiKey *key, AvahiWideAreaLookupCallback callback, void *userdata);
/** Start wire area cache dump
 *
 * @param e Wide area lookup engine.
 * @param key Owner name and type to scan.
 * @param callback Callback function receiving every scan.
 * @param userdata Optional userdata pointer passed into callback function.
 */
void avahi_wide_area_cache_dump(AvahiWideAreaLookupEngine *e, AvahiDumpCallback callback, void* userdata);
/** Configure used DNS servers for wide area lookups.
 *
 * @param a pointer to array containing n addresses.
 * @param n number of addresses passed.
 */
void avahi_wide_area_set_servers(AvahiWideAreaLookupEngine *e, const AvahiAddress *a, unsigned n);
/** Clear wide area cache. */
void avahi_wide_area_clear_cache(AvahiWideAreaLookupEngine *e);
/** Release all dead lookups on engine e. */
void avahi_wide_area_cleanup(AvahiWideAreaLookupEngine *e);
/** Returns 1 if at least one server is defined. */
int avahi_wide_area_has_servers(AvahiWideAreaLookupEngine *e);

/** Create new wide area lookup.
 *
 * @param e Wide area engine.
 * @param key Domain name to query.
 * @param callback Callback function to receive lookup results.
 * @param userdata Optional pointer passed to callback function.
 */
AvahiWideAreaLookup *avahi_wide_area_lookup_new(AvahiWideAreaLookupEngine *e, AvahiKey *key, AvahiWideAreaLookupCallback callback, void *userdata);

/** Release wide area lookup resources.
 *
 * Marks lookup as dead, but does not yet release all resources.
 */
void avahi_wide_area_lookup_free(AvahiWideAreaLookup *q);



#endif

