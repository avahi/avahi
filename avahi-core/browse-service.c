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

struct AvahiSServiceBrowser {
    AvahiServer *server;
    char *domain_name;
    char *service_type;
    
    AvahiSRecordBrowser *record_browser;

    AvahiSServiceBrowserCallback callback;
    void* userdata;

    AVAHI_LLIST_FIELDS(AvahiSServiceBrowser, browser);
};

static void record_browser_callback(AvahiSRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, void* userdata) {
    AvahiSServiceBrowser *b = userdata;
    char *n, *e, *c, *s;
    char service[128];

    assert(rr);
    assert(record);
    assert(b);

    assert(record->key->type == AVAHI_DNS_TYPE_PTR);

    c = n = avahi_normalize_name(record->data.ptr.name);

    if (!(avahi_unescape_label((const char**) &c, service, sizeof(service))))
        goto fail;

    for (s = e = c; *c == '_';) {
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
    
    b->callback(b, interface, protocol, event, service, s, c, b->userdata);
    avahi_free(n);

    return;

fail:
    avahi_log_warn("Invalid service '%s'", n);
    avahi_free(n);
}

AvahiSServiceBrowser *avahi_s_service_browser_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const char *service_type, const char *domain, AvahiSServiceBrowserCallback callback, void* userdata) {
    AvahiSServiceBrowser *b;
    AvahiKey *k;
    char *n = NULL;
    
    assert(server);
    assert(callback);
    assert(service_type);

    if (!avahi_is_valid_service_type(service_type)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_SERVICE_TYPE);
        return NULL;
    }

    if (domain && !avahi_is_valid_domain_name(domain)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_DOMAIN_NAME);
        return NULL;
    }

    if (!(b = avahi_new(AvahiSServiceBrowser, 1))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }
    
    b->server = server;
    b->domain_name = avahi_normalize_name(domain ? domain : "local");
    b->service_type = avahi_normalize_name(service_type);
    b->callback = callback;
    b->userdata = userdata;
    AVAHI_LLIST_PREPEND(AvahiSServiceBrowser, browser, server->service_browsers, b);

    n = avahi_strdup_printf("%s.%s", b->service_type, b->domain_name);
    k = avahi_key_new(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    avahi_free(n);
    
    b->record_browser = avahi_s_record_browser_new(server, interface, protocol, k, record_browser_callback, b);

    avahi_key_unref(k);

    if (!b->record_browser) {
        avahi_s_service_browser_free(b);
        return NULL;
    }
    
    return b;
}

void avahi_s_service_browser_free(AvahiSServiceBrowser *b) {
    assert(b);

    AVAHI_LLIST_REMOVE(AvahiSServiceBrowser, browser, b->server->service_browsers, b);

    if (b->record_browser)
        avahi_s_record_browser_free(b->record_browser);
    
    avahi_free(b->domain_name);
    avahi_free(b->service_type);
    avahi_free(b);
}
