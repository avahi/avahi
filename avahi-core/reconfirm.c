/***
  Copyright (c) 2017-2019 Nate Karstens, Garmin International, Inc. <nate.karstens@garmin.com>

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>

#include "core.h"
#include "internal.h"
#include "lookup.h"
#include "iface.h"
#include "cache.h"
#include "log.h"
#include "reconfirm.h"

static void* lookup_reconfirm_record(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, void* userdata) {
    assert(c);
    assert(pattern);
    assert(e);

    if (avahi_record_equal_no_ttl(e->record, userdata))
        return e;

    return NULL;
}

static void record_browser_callback(
    AvahiSRecordBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    AvahiRecord *record,
    AvahiLookupResultFlags flags,
    void* userdata) {

    AvahiRecord *reconfirm_record = userdata;
    AvahiInterface *i;
    AvahiCacheEntry *e;

    switch (event) {
        case AVAHI_BROWSER_NEW:
            assert(flags & AVAHI_LOOKUP_RESULT_CACHED);

            if (!(avahi_record_equal_no_ttl(record, reconfirm_record)))
                return;

            if (!(i = avahi_interface_monitor_get_interface(b->server->monitor, interface, protocol))) {
                b->server->error = AVAHI_ERR_INVALID_INTERFACE;
                return;
            }

            if (!(e = avahi_cache_walk(i->cache, record->key, lookup_reconfirm_record, record))) {
                b->server->error = AVAHI_ERR_INVALID_RECORD;
                return;
            }

            avahi_cache_entry_reconfirm(e);

            break;

        case AVAHI_BROWSER_REMOVE:
            break;

        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            avahi_s_record_browser_free(b);
            avahi_record_unref(reconfirm_record);
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_FAILURE:
            break;
    }
}

int avahi_record_reconfirm(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiRecord *r) {

    assert(server);
    assert(r);

    if (!(avahi_s_record_browser_new(server, interface, protocol, r->key, 0, record_browser_callback, avahi_record_ref(r)))) {
        avahi_record_unref(r);
        return 0;
    }

    return 1;
}
