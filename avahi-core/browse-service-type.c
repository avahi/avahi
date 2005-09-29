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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <avahi-common/domain.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "browse.h"
#include "log.h"

struct AvahiSServiceTypeBrowser {
    AvahiServer *server;
    char *domain_name;
    
    AvahiSRecordBrowser *record_browser;

    AvahiSServiceTypeBrowserCallback callback;
    void* userdata;

    AVAHI_LLIST_FIELDS(AvahiSServiceTypeBrowser, browser);
};

static void record_browser_callback(
    AvahiSRecordBrowser*rr,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    AvahiRecord *record,
    AvahiLookupResultFlags flags,
    void* userdata) {
    
    AvahiSServiceTypeBrowser *b = userdata;
    char *n = NULL, *c = NULL;

    assert(rr);
    assert(b);

    if (record) {
        char *e;
        
        assert(record->key->type == AVAHI_DNS_TYPE_PTR);
        
        n = avahi_normalize_name(record->data.ptr.name);
        
        if (*n != '_')
            goto fail;
        
        for (c = e = n; *c == '_';) {
            c += strcspn(c, ".");
            
            if (*c == 0)
                goto fail;
            
            assert(*c == '.');
            e = c;
            c++;
        }
        
        *e = 0;
        
        if (!avahi_domain_equal(c, b->domain_name))
            goto fail;
    }
    
    b->callback(b, interface, protocol, event, n, c, flags, b->userdata);
    avahi_free(n);
    
    return;

fail:
    avahi_log_warn("Invalid service type '%s'", n);
    avahi_free(n);
}

AvahiSServiceTypeBrowser *avahi_s_service_type_browser_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiLookupFlags flags,
    AvahiSServiceTypeBrowserCallback callback,
    void* userdata) {
    
    AvahiSServiceTypeBrowser *b;
    AvahiKey *k;
    char *n = NULL;
    
    assert(server);
    assert(callback);

    if (domain && !avahi_is_valid_domain_name(domain)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_DOMAIN_NAME);
        return NULL;
    }

    if (!domain)
        domain = server->domain_name;

    if (!AVAHI_FLAGS_VALID(flags, AVAHI_LOOKUP_USE_WIDE_AREA|AVAHI_LOOKUP_USE_MULTICAST)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_FLAGS);
        return NULL;
    }

    if (!(b = avahi_new(AvahiSServiceTypeBrowser, 1))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }
    
    b->server = server;
    b->domain_name = avahi_normalize_name(domain);
    b->callback = callback;
    b->userdata = userdata;

    AVAHI_LLIST_PREPEND(AvahiSServiceTypeBrowser, browser, server->service_type_browsers, b);

    n = avahi_strdup_printf("_services._dns-sd._udp.%s", b->domain_name);
    k = avahi_key_new(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    avahi_free(n);
    
    b->record_browser = avahi_s_record_browser_new(server, interface, protocol, k, flags, record_browser_callback, b);
    avahi_key_unref(k);

    if (!b->record_browser)
        return NULL;
    
    return b;
}

void avahi_s_service_type_browser_free(AvahiSServiceTypeBrowser *b) {
    assert(b);

    AVAHI_LLIST_REMOVE(AvahiSServiceTypeBrowser, browser, b->server->service_type_browsers, b);

    if (b->record_browser)
        avahi_s_record_browser_free(b->record_browser);
    
    avahi_free(b->domain_name);
    avahi_free(b);
}


