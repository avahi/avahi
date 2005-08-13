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

#include "browse.h"
#include "log.h"

struct AvahiServiceTypeBrowser {
    AvahiServer *server;
    char *domain_name;
    
    AvahiRecordBrowser *record_browser;

    AvahiServiceTypeBrowserCallback callback;
    void* userdata;

    AVAHI_LLIST_FIELDS(AvahiServiceTypeBrowser, browser);
};

static void record_browser_callback(AvahiRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, void* userdata) {
    AvahiServiceTypeBrowser *b = userdata;
    char *n, *e, *c;

    assert(rr);
    assert(record);
    assert(b);

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
    
    b->callback(b, interface, protocol, event, n, c, b->userdata);
    avahi_free(n);

    return;

fail:
    avahi_log_warn("Invalid service type '%s'", n);
    avahi_free(n);
}

AvahiServiceTypeBrowser *avahi_service_type_browser_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const char *domain, AvahiServiceTypeBrowserCallback callback, void* userdata) {
    AvahiServiceTypeBrowser *b;
    AvahiKey *k;
    char *n = NULL;
    
    assert(server);
    assert(callback);

    if (domain && !avahi_is_valid_domain_name(domain)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_DOMAIN_NAME);
        return NULL;
    }

    if (!(b = avahi_new(AvahiServiceTypeBrowser, 1))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }
    
    b->server = server;
    b->domain_name = avahi_normalize_name(domain ? domain : "local");
    b->callback = callback;
    b->userdata = userdata;

    AVAHI_LLIST_PREPEND(AvahiServiceTypeBrowser, browser, server->service_type_browsers, b);

    n = avahi_strdup_printf("_services._dns-sd._udp.%s", b->domain_name);
    k = avahi_key_new(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    avahi_free(n);
    
    b->record_browser = avahi_record_browser_new(server, interface, protocol, k, record_browser_callback, b);
    avahi_key_unref(k);

    if (!b->record_browser)
        return NULL;
    
    return b;
}

void avahi_service_type_browser_free(AvahiServiceTypeBrowser *b) {
    assert(b);

    AVAHI_LLIST_REMOVE(AvahiServiceTypeBrowser, browser, b->server->service_type_browsers, b);

    if (b->record_browser)
        avahi_record_browser_free(b->record_browser);
    
    avahi_free(b->domain_name);
    avahi_free(b);
}


