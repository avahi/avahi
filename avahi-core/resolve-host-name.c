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
#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "browse.h"
#include "log.h"

struct AvahiSHostNameResolver {
    AvahiServer *server;
    char *host_name;
    
    AvahiSRecordBrowser *record_browser_a;
    AvahiSRecordBrowser *record_browser_aaaa;

    AvahiSHostNameResolverCallback callback;
    void* userdata;

    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiSHostNameResolver, resolver);
};

static void finish(AvahiSHostNameResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, AvahiRecord *record) {
    AvahiAddress a;
    
    assert(r);

    if (r->record_browser_a) {
        avahi_s_record_browser_free(r->record_browser_a);
        r->record_browser_a = NULL;
    }

    if (r->record_browser_aaaa) {
        avahi_s_record_browser_free(r->record_browser_aaaa);
        r->record_browser_aaaa = NULL;
    }

    if (r->time_event) {
        avahi_time_event_free(r->time_event);
        r->time_event = NULL;
    }

    if (record) {
        switch (record->key->type) {
            case AVAHI_DNS_TYPE_A:
                a.family = AVAHI_PROTO_INET;
                a.data.ipv4 = record->data.a.address;
                break;
                
            case AVAHI_DNS_TYPE_AAAA:
                a.family = AVAHI_PROTO_INET6;
                a.data.ipv6 = record->data.aaaa.address;
                break;
                
            default:
                assert(0);
        }
    }

    r->callback(r, interface, protocol, event, record ? record->key->name : r->host_name, record ? &a : NULL, r->userdata);
}

static void record_browser_callback(AvahiSRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, void* userdata) {
    AvahiSHostNameResolver *r = userdata;

    assert(rr);
    assert(record);
    assert(r);

    if (!(event == AVAHI_BROWSER_NEW))
        return;
    
    assert(record->key->type == AVAHI_DNS_TYPE_A || record->key->type == AVAHI_DNS_TYPE_AAAA);
    finish(r, interface, protocol, AVAHI_RESOLVER_FOUND, record);
}

static void time_event_callback(AvahiTimeEvent *e, void *userdata) {
    AvahiSHostNameResolver *r = userdata;
    
    assert(e);
    assert(r);

    finish(r, -1, AVAHI_PROTO_UNSPEC, AVAHI_RESOLVER_TIMEOUT, NULL);
}

AvahiSHostNameResolver *avahi_s_host_name_resolver_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const char *host_name, AvahiProtocol aprotocol, AvahiSHostNameResolverCallback callback, void* userdata) {
    AvahiSHostNameResolver *r;
    AvahiKey *k;
    struct timeval tv;
    
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

    r->record_browser_a = r->record_browser_aaaa = NULL;
        
    avahi_elapse_time(&tv, 1000, 0);
    r->time_event = avahi_time_event_new(server->time_event_queue, &tv, time_event_callback, r);

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
    
    avahi_free(r->host_name);
    avahi_free(r);
}
