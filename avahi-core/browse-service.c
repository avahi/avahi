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

#include "browse.h"
#include "util.h"
#include "log.h"

struct AvahiServiceBrowser {
    AvahiServer *server;
    gchar *domain_name;
    gchar *service_type;
    
    AvahiRecordBrowser *record_browser;

    AvahiServiceBrowserCallback callback;
    gpointer userdata;

    AVAHI_LLIST_FIELDS(AvahiServiceBrowser, browser);
};

static void record_browser_callback(AvahiRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, gpointer userdata) {
    AvahiServiceBrowser *b = userdata;
    gchar *n, *e, *c, *s;
    gchar service[128];

    g_assert(rr);
    g_assert(record);
    g_assert(b);

    g_assert(record->key->type == AVAHI_DNS_TYPE_PTR);

    c = n = avahi_normalize_name(record->data.ptr.name);

    if (!(avahi_unescape_label((const gchar**) &c, service, sizeof(service))))
        goto fail;

    for (s = e = c; *c == '_';) {
        c += strcspn(c, ".");

        if (*c == 0)
            goto fail;

        g_assert(*c == '.');
        e = c;
        c++;
    }

    *e = 0;

    if (!avahi_domain_equal(c, b->domain_name))
        goto fail;
    
    b->callback(b, interface, protocol, event, service, s, c, b->userdata);
    g_free(n);

    return;

fail:
    avahi_log_warn("Invalid service '%s'", n);
    g_free(n);
}

AvahiServiceBrowser *avahi_service_browser_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const gchar *service_type, const gchar *domain, AvahiServiceBrowserCallback callback, gpointer userdata) {
    AvahiServiceBrowser *b;
    AvahiKey *k;
    gchar *n = NULL;
    
    g_assert(server);
    g_assert(callback);
    g_assert(service_type);

    if (!avahi_valid_service_type(service_type)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_SERVICE_TYPE);
        return NULL;
    }

    if (domain && !avahi_valid_domain_name(domain)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_DOMAIN_NAME);
        return NULL;
    }

    b = g_new(AvahiServiceBrowser, 1);
    b->server = server;
    b->domain_name = avahi_normalize_name(domain ? domain : "local");
    b->service_type = avahi_normalize_name(service_type);
    b->callback = callback;
    b->userdata = userdata;
    AVAHI_LLIST_PREPEND(AvahiServiceBrowser, browser, server->service_browsers, b);

    n = g_strdup_printf("%s.%s", b->service_type, b->domain_name);
    k = avahi_key_new(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    g_free(n);
    
    b->record_browser = avahi_record_browser_new(server, interface, protocol, k, record_browser_callback, b);

    avahi_key_unref(k);

    if (!b->record_browser) {
        avahi_service_browser_free(b);
        return NULL;
    }
    
    return b;
}

void avahi_service_browser_free(AvahiServiceBrowser *b) {
    g_assert(b);

    AVAHI_LLIST_REMOVE(AvahiServiceBrowser, browser, b->server->service_browsers, b);

    if (b->record_browser)
        avahi_record_browser_free(b->record_browser);
    
    g_free(b->domain_name);
    g_free(b->service_type);
    g_free(b);
}
