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

#include "browse.h"
#include "util.h"

struct AvahiDomainBrowser {
    AvahiServer *server;
    gchar *domain_name;
    
    AvahiRecordBrowser *record_browser;

    AvahiDomainBrowserCallback callback;
    gpointer userdata;

    AVAHI_LLIST_FIELDS(AvahiDomainBrowser, browser);
};

static void record_browser_callback(AvahiRecordBrowser*rr, gint interface, guchar protocol, AvahiBrowserEvent event, AvahiRecord *record, gpointer userdata) {
    AvahiDomainBrowser *b = userdata;
    gchar *n;

    g_assert(rr);
    g_assert(record);
    g_assert(b);

    g_assert(record->key->type == AVAHI_DNS_TYPE_PTR);

    n = avahi_normalize_name(record->data.ptr.name);
    b->callback(b, interface, protocol, event, n, b->userdata);
    g_free(n);
}

AvahiDomainBrowser *avahi_domain_browser_new(AvahiServer *server, gint interface, guchar protocol, const gchar *domain, AvahiDomainBrowserType type, AvahiDomainBrowserCallback callback, gpointer userdata) {
    AvahiDomainBrowser *b;
    AvahiKey *k;
    gchar *n = NULL;
    
    g_assert(server);
    g_assert(callback);

    b = g_new(AvahiDomainBrowser, 1);
    b->server = server;
    b->domain_name = avahi_normalize_name(domain ? domain : "local.");
    b->callback = callback;
    b->userdata = userdata;

    switch (type) {
        case AVAHI_DOMAIN_BROWSER_BROWSE:
            n = g_strdup_printf("_browse._dns-sd._udp.%s", b->domain_name);
            break;
        case AVAHI_DOMAIN_BROWSER_BROWSE_DEFAULT:
            n = g_strdup_printf("_default._browse._dns-sd._udp.%s", b->domain_name);
            break;
        case AVAHI_DOMAIN_BROWSER_REGISTER:
            n = g_strdup_printf("_register._dns-sd._udp.%s", b->domain_name);
            break;
        case AVAHI_DOMAIN_BROWSER_REGISTER_DEFAULT:
            n = g_strdup_printf("_default._register._dns-sd._udp.%s", b->domain_name);
            break;
    }

    g_assert(n);

    k = avahi_key_new(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    g_free(n);
    
    b->record_browser = avahi_record_browser_new(server, interface, protocol, k, record_browser_callback, b);
    avahi_key_unref(k);

    AVAHI_LLIST_PREPEND(AvahiDomainBrowser, browser, server->domain_browsers, b);
    
    return b;
}

void avahi_domain_browser_free(AvahiDomainBrowser *b) {
    g_assert(b);

    AVAHI_LLIST_REMOVE(AvahiDomainBrowser, browser, b->server->domain_browsers, b);

    avahi_record_browser_free(b->record_browser);
    g_free(b->domain_name);
    g_free(b);
}
