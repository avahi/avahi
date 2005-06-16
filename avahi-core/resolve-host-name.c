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

struct AvahiHostNameResolver {
    AvahiServer *server;
    gchar *host_name;
    
    AvahiRecordBrowser *record_browser_a;
    AvahiRecordBrowser *record_browser_aaaa;

    AvahiHostNameResolverCallback callback;
    gpointer userdata;

    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiHostNameResolver, resolver);
};

static void finish(AvahiHostNameResolver *r, gint interface, guchar protocol, AvahiResolverEvent event, AvahiRecord *record) {
    AvahiAddress a;
    
    g_assert(r);

    if (r->record_browser_a) {
        avahi_record_browser_free(r->record_browser_a);
        r->record_browser_a = NULL;
    }

    if (r->record_browser_aaaa) {
        avahi_record_browser_free(r->record_browser_aaaa);
        r->record_browser_aaaa = NULL;
    }
 
    avahi_time_event_queue_remove(r->server->time_event_queue, r->time_event);
    r->time_event = NULL;

    if (record) {
        switch (record->key->type) {
            case AVAHI_DNS_TYPE_A:
                a.family = AF_INET;
                a.data.ipv4 = record->data.a.address;
                break;
                
            case AVAHI_DNS_TYPE_AAAA:
                a.family = AF_INET6;
                a.data.ipv6 = record->data.aaaa.address;
                break;
                
            default:
                g_assert(FALSE);
        }
    }

    r->callback(r, interface, protocol, event, record ? record->key->name : r->host_name, record ? &a : NULL, r->userdata);
}

static void record_browser_callback(AvahiRecordBrowser*rr, gint interface, guchar protocol, AvahiBrowserEvent event, AvahiRecord *record, gpointer userdata) {
    AvahiHostNameResolver *r = userdata;

    g_assert(rr);
    g_assert(record);
    g_assert(r);

    if (!(event == AVAHI_BROWSER_NEW))
        return;
    
    g_assert(record->key->type == AVAHI_DNS_TYPE_A || record->key->type == AVAHI_DNS_TYPE_AAAA);
    finish(r, interface, protocol, AVAHI_RESOLVER_FOUND, record);
}

static void time_event_callback(AvahiTimeEvent *e, void *userdata) {
    AvahiHostNameResolver *r = userdata;
    
    g_assert(e);
    g_assert(r);

    finish(r, -1, AF_UNSPEC, AVAHI_RESOLVER_TIMEOUT, NULL);
}

AvahiHostNameResolver *avahi_host_name_resolver_new(AvahiServer *server, gint interface, guchar protocol, const gchar *host_name, guchar aprotocol, AvahiHostNameResolverCallback callback, gpointer userdata) {
    AvahiHostNameResolver *r;
    AvahiKey *k;
    GTimeVal tv;
    
    g_assert(server);
    g_assert(host_name);
    g_assert(callback);

    g_assert(aprotocol == AF_UNSPEC || aprotocol == AF_INET || aprotocol == AF_INET6);

    r = g_new(AvahiHostNameResolver, 1);
    r->server = server;
    r->host_name = avahi_normalize_name(host_name);
    r->callback = callback;
    r->userdata = userdata;

    r->record_browser_a = r->record_browser_aaaa = NULL;
        
    avahi_elapse_time(&tv, 1000, 0);
    r->time_event = avahi_time_event_queue_add(server->time_event_queue, &tv, time_event_callback, r);

    AVAHI_LLIST_PREPEND(AvahiHostNameResolver, resolver, server->host_name_resolvers, r);
    
    if (aprotocol == AF_INET || aprotocol == AF_UNSPEC) {
        k = avahi_key_new(host_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A);
        r->record_browser_a = avahi_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
        avahi_key_unref(k);
    } 

    if (aprotocol == AF_INET6 || aprotocol == AF_UNSPEC) {
        k = avahi_key_new(host_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA);
        r->record_browser_aaaa = avahi_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
        avahi_key_unref(k);
    }
    
    return r;
}

void avahi_host_name_resolver_free(AvahiHostNameResolver *r) {
    g_assert(r);

    AVAHI_LLIST_REMOVE(AvahiHostNameResolver, resolver, r->server->host_name_resolvers, r);

    if (r->record_browser_a)
        avahi_record_browser_free(r->record_browser_a);
    if (r->record_browser_aaaa)
        avahi_record_browser_free(r->record_browser_aaaa);
    
    g_free(r->host_name);
    g_free(r);
}
