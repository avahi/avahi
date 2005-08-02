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
#include "rr.h"

typedef struct AvahiDNSServerInfo AvahiDNSServerInfo;

struct AvahiDNSServerInfo {
    AvahiDNSServerBrowser *browser;

    AvahiIfIndex interface;
    AvahiProtocol protocol;
    AvahiRecord *srv_record;
    AvahiHostNameResolver *host_name_resolver;
    AvahiAddress address;
    
    AVAHI_LLIST_FIELDS(AvahiDNSServerInfo, info);
};

struct AvahiDNSServerBrowser {
    AvahiServer *server;
    gchar *domain_name;
    
    AvahiRecordBrowser *record_browser;
    AvahiDNSServerBrowserCallback callback;
    gpointer userdata;
    AvahiProtocol aprotocol;

    guint n_info;
    
    AVAHI_LLIST_FIELDS(AvahiDNSServerBrowser, browser);
    AVAHI_LLIST_HEAD(AvahiDNSServerInfo, info);
};

static AvahiDNSServerInfo* get_server_info(AvahiDNSServerBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiRecord *r) {
    AvahiDNSServerInfo *i;
    
    g_assert(b);
    g_assert(r);

    for (i = b->info; i; i = i->info_next)
        if (i->interface == interface &&
            i->protocol == protocol &&
            avahi_record_equal_no_ttl(r, i->srv_record))
            return i;

    return NULL;
}

static void server_info_free(AvahiDNSServerBrowser *b, AvahiDNSServerInfo *i) {
    g_assert(b);
    g_assert(i);

    avahi_record_unref(i->srv_record);
    if (i->host_name_resolver)
        avahi_host_name_resolver_free(i->host_name_resolver);
    
    AVAHI_LLIST_REMOVE(AvahiDNSServerInfo, info, b->info, i);

    g_assert(b->n_info >= 1);
    b->n_info--;
    
    g_free(i);
}

static void host_name_resolver_callback(AvahiHostNameResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const gchar *host_name, const AvahiAddress *a, gpointer userdata) {
    AvahiDNSServerInfo *i = userdata;
    
    g_assert(r);
    g_assert(host_name);
    g_assert(i);

    if (event == AVAHI_RESOLVER_FOUND) {
        i->address = *a;

        i->browser->callback(i->browser, i->interface, i->protocol, AVAHI_BROWSER_NEW, i->srv_record->data.srv.name, &i->address, i->srv_record->data.srv.port, i->browser->userdata);
    }

    avahi_host_name_resolver_free(i->host_name_resolver);
    i->host_name_resolver = NULL;
}

static void record_browser_callback(AvahiRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, gpointer userdata) {
    AvahiDNSServerBrowser *b = userdata;

    g_assert(rr);
    g_assert(record);
    g_assert(b);

    g_assert(record->key->type == AVAHI_DNS_TYPE_SRV);

    if (event == AVAHI_BROWSER_NEW) {
        AvahiDNSServerInfo *i;

        if (get_server_info(b, interface, protocol, record))
            return;

        if (b->n_info >= 10)
            return;
        
        i = g_new(AvahiDNSServerInfo, 1);
        i->browser = b;
        i->interface = interface;
        i->protocol = protocol;
        i->srv_record = avahi_record_ref(record);
        i->host_name_resolver = avahi_host_name_resolver_new(b->server, interface, protocol, record->data.srv.name, b->aprotocol, host_name_resolver_callback, i);
        
        AVAHI_LLIST_PREPEND(AvahiDNSServerInfo, info, b->info, i);

        b->n_info++;
    } else if (event == AVAHI_BROWSER_REMOVE) {
        AvahiDNSServerInfo *i;

        if (!(i = get_server_info(b, interface, protocol, record)))
            return;

        if (!i->host_name_resolver)
            b->callback(b, interface, protocol, event, i->srv_record->data.srv.name, &i->address, i->srv_record->data.srv.port, b->userdata);

        server_info_free(b, i);
    }
}

AvahiDNSServerBrowser *avahi_dns_server_browser_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const gchar *domain, AvahiDNSServerType type, AvahiProtocol aprotocol, AvahiDNSServerBrowserCallback callback, gpointer userdata) {
    AvahiDNSServerBrowser *b;
    AvahiKey *k;
    gchar *n = NULL;
    
    g_assert(server);
    g_assert(callback);
    g_assert(type == AVAHI_DNS_SERVER_RESOLVE || type == AVAHI_DNS_SERVER_UPDATE);

    b = g_new(AvahiDNSServerBrowser, 1);
    b->server = server;
    b->domain_name = avahi_normalize_name(domain ? domain : "local.");
    b->callback = callback;
    b->userdata = userdata;
    b->aprotocol = aprotocol;
    b->n_info = 0;

    AVAHI_LLIST_HEAD_INIT(AvahiDNSServerInfo, b->info);
    
    n = g_strdup_printf("%s.%s",type == AVAHI_DNS_SERVER_RESOLVE ? "_domain._udp" : "_dns-update._udp", b->domain_name);
    k = avahi_key_new(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_SRV);
    g_free(n);
    
    b->record_browser = avahi_record_browser_new(server, interface, protocol, k, record_browser_callback, b);
    avahi_key_unref(k);

    AVAHI_LLIST_PREPEND(AvahiDNSServerBrowser, browser, server->dns_server_browsers, b);
    
    return b;
}

void avahi_dns_server_browser_free(AvahiDNSServerBrowser *b) {
    g_assert(b);

    while (b->info)
        server_info_free(b, b->info);
    
    AVAHI_LLIST_REMOVE(AvahiDNSServerBrowser, browser, b->server->dns_server_browsers, b);

    avahi_record_browser_free(b->record_browser);
    g_free(b->domain_name);
    g_free(b);
}

