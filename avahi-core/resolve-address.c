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
#include <avahi-common/error.h>

#include "browse.h"

#define TIMEOUT_MSEC 1000

struct AvahiSAddressResolver {
    AvahiServer *server;
    AvahiAddress address;
    
    AvahiSRecordBrowser *record_browser;

    AvahiSAddressResolverCallback callback;
    void* userdata;

    AvahiRecord *ptr_record;
    AvahiIfIndex interface;
    AvahiProtocol protocol;

    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiSAddressResolver, resolver);
};

static void finish(AvahiSAddressResolver *r, AvahiResolverEvent event) {
    assert(r);
    
    if (r->time_event) {
        avahi_time_event_free(r->time_event);
        r->time_event = NULL;
    }

    if (event == AVAHI_RESOLVER_TIMEOUT)
        r->callback(r, r->interface, r->protocol, AVAHI_RESOLVER_TIMEOUT, &r->address, NULL, r->userdata);
    else {

        assert(event == AVAHI_RESOLVER_FOUND);
        assert(r->ptr_record);

        r->callback(r, r->interface, r->protocol, AVAHI_RESOLVER_FOUND, &r->address, r->ptr_record->data.ptr.name, r->userdata);
    }
}

static void time_event_callback(AvahiTimeEvent *e, void *userdata) {
    AvahiSAddressResolver *r = userdata;
    
    assert(e);
    assert(r);

    finish(r, AVAHI_RESOLVER_TIMEOUT);
}

static void start_timeout(AvahiSAddressResolver *r) {
    struct timeval tv;
    assert(r);

    if (r->time_event)
        return;

    avahi_elapse_time(&tv, TIMEOUT_MSEC, 0);
    r->time_event = avahi_time_event_new(r->server->time_event_queue, &tv, time_event_callback, r);
}

static void record_browser_callback(AvahiSRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, void* userdata) {
    AvahiSAddressResolver *r = userdata;

    assert(rr);
    assert(record);
    assert(r);

    assert(record->key->type == AVAHI_DNS_TYPE_PTR);

    if (event == AVAHI_BROWSER_NEW) {

        if (r->interface > 0 && interface != r->interface)
            return;
        
        if (r->protocol != AVAHI_PROTO_UNSPEC && protocol != r->protocol)
            return;
        
        if (r->interface <= 0)
            r->interface = interface;
        
        if (r->protocol == AVAHI_PROTO_UNSPEC)
            r->protocol = protocol;
        
        if (!r->ptr_record) {
            r->ptr_record = avahi_record_ref(record);

            finish(r, AVAHI_RESOLVER_FOUND);
        }

    } else {
        
        assert(event == AVAHI_BROWSER_REMOVE);
        
        if (r->ptr_record && avahi_record_equal_no_ttl(record, r->ptr_record)) {
            avahi_record_unref(r->ptr_record);
            r->ptr_record = NULL;

            /** Look for a replacement */
            avahi_s_record_browser_restart(r->record_browser);
            start_timeout(r);
        }
    }
}

AvahiSAddressResolver *avahi_s_address_resolver_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const AvahiAddress *address, AvahiSAddressResolverCallback callback, void* userdata) {
    AvahiSAddressResolver *r;
    AvahiKey *k;
    char *n;

    assert(server);
    assert(address);
    assert(callback);

    assert(address->proto == AVAHI_PROTO_INET || address->proto == AVAHI_PROTO_INET6);

    if (address->proto == AVAHI_PROTO_INET)
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

    if (!(r = avahi_new(AvahiSAddressResolver, 1))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        avahi_key_unref(k);
        return NULL;
    }
    
    r->server = server;
    r->address = *address;
    r->callback = callback;
    r->userdata = userdata;
    r->ptr_record = NULL;
    r->interface = interface;
    r->protocol = protocol;

    r->record_browser = NULL;
    AVAHI_LLIST_PREPEND(AvahiSAddressResolver, resolver, server->address_resolvers, r);

    r->time_event = NULL;
    start_timeout(r);
    
    r->record_browser = avahi_s_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
    avahi_key_unref(k);

    if (!r->record_browser) {
        avahi_s_address_resolver_free(r);
        return NULL;
    }
    
    return r;
}

void avahi_s_address_resolver_free(AvahiSAddressResolver *r) {
    assert(r);

    AVAHI_LLIST_REMOVE(AvahiSAddressResolver, resolver, r->server->address_resolvers, r);

    if (r->record_browser)
        avahi_s_record_browser_free(r->record_browser);

    if (r->time_event)
        avahi_time_event_free(r->time_event);

    if (r->ptr_record)
        avahi_record_unref(r->ptr_record);
    
    avahi_free(r);
}
