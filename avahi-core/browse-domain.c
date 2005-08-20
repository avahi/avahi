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

#include <avahi-common/domain.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "browse.h"

struct AvahiSDomainBrowser {
    AvahiServer *server;
    char *domain_name;
    
    AvahiSRecordBrowser *record_browser;

    AvahiSDomainBrowserCallback callback;
    void* userdata;

    AVAHI_LLIST_FIELDS(AvahiSDomainBrowser, browser);
};

static void record_browser_callback(AvahiSRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, void* userdata) {
    AvahiSDomainBrowser *b = userdata;
    char *n;

    assert(rr);
    assert(record);
    assert(b);

    assert(record->key->type == AVAHI_DNS_TYPE_PTR);

    n = avahi_normalize_name(record->data.ptr.name);
    b->callback(b, interface, protocol, event, n, b->userdata);
    avahi_free(n);
}

AvahiSDomainBrowser *avahi_s_domain_browser_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const char *domain, AvahiDomainBrowserType type, AvahiSDomainBrowserCallback callback, void* userdata) {
    AvahiSDomainBrowser *b;
    AvahiKey *k;
    char *n = NULL;
    
    assert(server);
    assert(callback);
    assert(type >= AVAHI_DOMAIN_BROWSER_BROWSE && type <= AVAHI_DOMAIN_BROWSER_BROWSE_LEGACY);

    if (domain && !avahi_is_valid_domain_name(domain)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_DOMAIN_NAME);
        return NULL;
    }

    if (!(b = avahi_new(AvahiSDomainBrowser, 1))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }
    
    b->server = server;
    b->domain_name = avahi_normalize_name(domain ? domain : "local");
    b->callback = callback;
    b->userdata = userdata;

    AVAHI_LLIST_PREPEND(AvahiSDomainBrowser, browser, server->domain_browsers, b);

    switch (type) {
        case AVAHI_DOMAIN_BROWSER_BROWSE:
            n = avahi_strdup_printf("b._dns-sd._udp.%s", b->domain_name);
            break;
        case AVAHI_DOMAIN_BROWSER_BROWSE_DEFAULT:
            n = avahi_strdup_printf("db._dns-sd._udp.%s", b->domain_name);
            break;
        case AVAHI_DOMAIN_BROWSER_REGISTER:
            n = avahi_strdup_printf("r._dns-sd._udp.%s", b->domain_name);
            break;
        case AVAHI_DOMAIN_BROWSER_REGISTER_DEFAULT:
            n = avahi_strdup_printf("dr._dns-sd._udp.%s", b->domain_name);
            break;
        case AVAHI_DOMAIN_BROWSER_BROWSE_LEGACY:
            n = avahi_strdup_printf("lb._dns-sd._udp.%s", b->domain_name);
            break;

        case AVAHI_DOMAIN_BROWSER_MAX:
            assert(0);
            break;
    }

    assert(n);

    k = avahi_key_new(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    avahi_free(n);
    
    b->record_browser = avahi_s_record_browser_new(server, interface, protocol, k, record_browser_callback, b);
    avahi_key_unref(k);

    if (!b->record_browser) {
        avahi_s_domain_browser_free(b);
        return NULL;
    }
    
    return b;
}

void avahi_s_domain_browser_free(AvahiSDomainBrowser *b) {
    assert(b);

    AVAHI_LLIST_REMOVE(AvahiSDomainBrowser, browser, b->server->domain_browsers, b);

    if (b->record_browser)
        avahi_s_record_browser_free(b->record_browser);
    
    avahi_free(b->domain_name);
    avahi_free(b);
}
