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

#include <stdlib.h>

#include <avahi-common/domain.h>
#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "browse.h"
#include "log.h"

#define TIMEOUT_MSEC 1000

struct AvahiSHostNameResolver {
    AvahiServer *server;
    char *host_name;
    
    AvahiSRecordBrowser *record_browser_a;
    AvahiSRecordBrowser *record_browser_aaaa;

    AvahiSHostNameResolverCallback callback;
    void* userdata;

    AvahiRecord *address_record;
    AvahiIfIndex interface;
    AvahiProtocol protocol;

    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiSHostNameResolver, resolver);
};

static void finish(AvahiSHostNameResolver *r, AvahiResolverEvent event) {
    assert(r);

    if (r->time_event) {
        avahi_time_event_free(r->time_event);
        r->time_event = NULL;
    }

    if (event == AVAHI_RESOLVER_TIMEOUT)
        r->callback(r, r->interface, r->protocol, AVAHI_RESOLVER_TIMEOUT, r->host_name, NULL, r->userdata);
    else {
        AvahiAddress a;
    
        assert(event == AVAHI_RESOLVER_FOUND);
        assert(r->address_record);
    
        switch (r->address_record->key->type) {
            case AVAHI_DNS_TYPE_A:
                a.family = AVAHI_PROTO_INET;
                a.data.ipv4 = r->address_record->data.a.address;
                break;
                
            case AVAHI_DNS_TYPE_AAAA:
                a.family = AVAHI_PROTO_INET6;
                a.data.ipv6 = r->address_record->data.aaaa.address;
                break;
                
            default:
                abort();
        }

        r->callback(r, r->interface, r->protocol, AVAHI_RESOLVER_FOUND, r->address_record->key->name, &a, r->userdata);
    }
}

static void time_event_callback(AvahiTimeEvent *e, void *userdata) {
    AvahiSHostNameResolver *r = userdata;
    
    assert(e);
    assert(r);

    finish(r, AVAHI_RESOLVER_TIMEOUT);
}

static void start_timeout(AvahiSHostNameResolver *r) {
    struct timeval tv;
    assert(r);

    if (r->time_event)
        return;

    avahi_elapse_time(&tv, TIMEOUT_MSEC, 0);
    r->time_event = avahi_time_event_new(r->server->time_event_queue, &tv, time_event_callback, r);
}

static void record_browser_callback(AvahiSRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, void* userdata) {
    AvahiSHostNameResolver *r = userdata;

    assert(rr);
    assert(record);
    assert(r);

    assert(record->key->type == AVAHI_DNS_TYPE_A || record->key->type == AVAHI_DNS_TYPE_AAAA);
    
    if (event == AVAHI_BROWSER_NEW) {

        if (r->interface > 0 && interface != r->interface)
            return;
        
        if (r->protocol != AVAHI_PROTO_UNSPEC && protocol != r->protocol)
            return;
        
        if (r->interface <= 0)
            r->interface = interface;
        
        if (r->protocol == AVAHI_PROTO_UNSPEC)
            r->protocol = protocol;
        
        if (!r->address_record) {
            r->address_record = avahi_record_ref(record);
            
            finish(r, AVAHI_RESOLVER_FOUND);
        }
    } else {

        assert(event == AVAHI_BROWSER_REMOVE);

        if (avahi_record_equal_no_ttl(record, r->address_record)) {
            avahi_record_unref(r->address_record);
            r->address_record = NULL;

            /** Look for a replacement */
            if (r->record_browser_aaaa)
                avahi_s_record_browser_restart(r->record_browser_aaaa);
            if (r->record_browser_a)
                avahi_s_record_browser_restart(r->record_browser_a);

            start_timeout(r);
        }
    }
}

AvahiSHostNameResolver *avahi_s_host_name_resolver_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const char *host_name, AvahiProtocol aprotocol, AvahiSHostNameResolverCallback callback, void* userdata) {
    AvahiSHostNameResolver *r;
    AvahiKey *k;
    
    assert(server);
    assert(host_name);
    assert(callback);

    assert(aprotocol == AVAHI_PROTO_UNSPEC || aprotocol == AVAHI_PROTO_INET || aprotocol == AVAHI_PROTO_INET6);

    if (!avahi_is_valid_domain_name(host_name)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_HOST_NAME);
        return NULL;
    }
    
    if (!(r = avahi_new(AvahiSHostNameResolver, 1))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }
    
    r->server = server;
    r->host_name = avahi_normalize_name(host_name);
    r->callback = callback;
    r->userdata = userdata;
    r->address_record = NULL;
    r->interface = interface;
    r->protocol = protocol;

    r->record_browser_a = r->record_browser_aaaa = NULL;

    r->time_event = NULL;
    start_timeout(r);

    AVAHI_LLIST_PREPEND(AvahiSHostNameResolver, resolver, server->host_name_resolvers, r);

    r->record_browser_aaaa = r->record_browser_a = NULL;
    
    if (aprotocol == AVAHI_PROTO_INET || aprotocol == AVAHI_PROTO_UNSPEC) {
        k = avahi_key_new(host_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A);
        r->record_browser_a = avahi_s_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
        avahi_key_unref(k);

        if (!r->record_browser_a)
            goto fail;
    } 

    if (aprotocol == AVAHI_PROTO_INET6 || aprotocol == AVAHI_PROTO_UNSPEC) {
        k = avahi_key_new(host_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA);
        r->record_browser_aaaa = avahi_s_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
        avahi_key_unref(k);

        if (!r->record_browser_aaaa)
            goto fail;
    }

    assert(r->record_browser_aaaa || r->record_browser_a);

    return r;

fail:
    avahi_s_host_name_resolver_free(r);
    return NULL;
}

void avahi_s_host_name_resolver_free(AvahiSHostNameResolver *r) {
    assert(r);

    AVAHI_LLIST_REMOVE(AvahiSHostNameResolver, resolver, r->server->host_name_resolvers, r);

    if (r->record_browser_a)
        avahi_s_record_browser_free(r->record_browser_a);

    if (r->record_browser_aaaa)
        avahi_s_record_browser_free(r->record_browser_aaaa);

    if (r->time_event)
        avahi_time_event_free(r->time_event);

    if (r->address_record)
        avahi_record_unref(r->address_record);
    
    avahi_free(r->host_name);
    avahi_free(r);
}
