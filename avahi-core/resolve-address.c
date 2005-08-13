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

#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>

#include "browse.h"

struct AvahiAddressResolver {
    AvahiServer *server;
    AvahiAddress address;
    
    AvahiRecordBrowser *record_browser;

    AvahiAddressResolverCallback callback;
    void* userdata;

    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiAddressResolver, resolver);
};

static void finish(AvahiAddressResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, AvahiRecord *record) {
    assert(r);
    
    avahi_record_browser_free(r->record_browser);
    r->record_browser = NULL;

    if (r->time_event) {
        avahi_time_event_free(r->time_event);
        r->time_event = NULL;
    }

    r->callback(r, interface, protocol, event, &r->address, record ? record->data.ptr.name : NULL, r->userdata);
}

static void record_browser_callback(AvahiRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, void* userdata) {
    AvahiAddressResolver *r = userdata;

    assert(rr);
    assert(record);
    assert(r);

    if (!(event == AVAHI_BROWSER_NEW))
        return;

    assert(record->key->type == AVAHI_DNS_TYPE_PTR);

    finish(r, interface, protocol, AVAHI_RESOLVER_FOUND, record);
}

static void time_event_callback(AvahiTimeEvent *e, void *userdata) {
    AvahiAddressResolver *r = userdata;
    
    assert(e);
    assert(r);

    finish(r, -1, AVAHI_PROTO_UNSPEC, AVAHI_RESOLVER_TIMEOUT, NULL);
}

AvahiAddressResolver *avahi_address_resolver_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const AvahiAddress *address, AvahiAddressResolverCallback callback, void* userdata) {
    AvahiAddressResolver *r;
    AvahiKey *k;
    char *n;
    struct timeval tv;

    assert(server);
    assert(address);
    assert(callback);

    assert(address->family == AVAHI_PROTO_INET || address->family == AVAHI_PROTO_INET6);

    if (address->family == AVAHI_PROTO_INET)
        n = avahi_reverse_lookup_name_ipv4(&address->data.ipv4);
    else 
        n = avahi_reverse_lookup_name_ipv6_arpa(&address->data.ipv6);

    if (!n) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }

    k = avahi_key_new(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    avahi_free(n);

    if (!k) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }

    if (!(r = avahi_new(AvahiAddressResolver, 1))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        avahi_key_unref(k);
        return NULL;
    }
    
    r->server = server;
    r->address = *address;
    r->callback = callback;
    r->userdata = userdata;

    r->record_browser = NULL;
    AVAHI_LLIST_PREPEND(AvahiAddressResolver, resolver, server->address_resolvers, r);

    avahi_elapse_time(&tv, 1000, 0);
    if (!(r->time_event = avahi_time_event_new(server->time_event_queue, &tv, time_event_callback, r))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        avahi_address_resolver_free(r);
        avahi_key_unref(k);
        return NULL;
    }
    
    r->record_browser = avahi_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
    avahi_key_unref(k);

    if (!r->record_browser) {
        avahi_address_resolver_free(r);
        return NULL;
    }
    
    return r;
}

void avahi_address_resolver_free(AvahiAddressResolver *r) {
    assert(r);

    AVAHI_LLIST_REMOVE(AvahiAddressResolver, resolver, r->server->address_resolvers, r);

    if (r->record_browser)
        avahi_record_browser_free(r->record_browser);

    if (r->time_event)
        avahi_time_event_free(r->time_event);
    
    avahi_free(r);
}
