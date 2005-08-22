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
#include <stdio.h>
#include <stdlib.h>

#include <avahi-common/domain.h>
#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "browse.h"

#define TIMEOUT_MSEC 1000

struct AvahiSServiceResolver {
    AvahiServer *server;
    char *service_name;
    char *service_type;
    char *domain_name;
    AvahiProtocol address_protocol;

    AvahiIfIndex interface;
    AvahiProtocol protocol;

    AvahiSRecordBrowser *record_browser_srv;
    AvahiSRecordBrowser *record_browser_txt;
    AvahiSRecordBrowser *record_browser_a;
    AvahiSRecordBrowser *record_browser_aaaa;

    AvahiRecord *srv_record, *txt_record, *address_record;
    
    AvahiSServiceResolverCallback callback;
    void* userdata;

    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiSServiceResolver, resolver);
};

static void finish(AvahiSServiceResolver *r, AvahiResolverEvent event) {
    assert(r);

    if (r->time_event) {
        avahi_time_event_free(r->time_event);
        r->time_event = NULL;
    }

    if (event == AVAHI_RESOLVER_TIMEOUT)
        r->callback(r, r->interface, r->protocol, event, r->service_name, r->service_type, r->domain_name, NULL, NULL, 0, NULL, r->userdata);
    else {
        AvahiAddress a;
        char sn[256], st[256];
        size_t i;

        assert(event == AVAHI_RESOLVER_FOUND);
        
        assert(r->srv_record);
        assert(r->txt_record);
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
                assert(0);
        }

        snprintf(sn, sizeof(sn), r->service_name);
        snprintf(st, sizeof(st), r->service_type);

        if ((i = strlen(sn)) > 0 && sn[i-1] == '.')
            sn[i-1] = 0;

        if ((i = strlen(st)) > 0 && st[i-1] == '.')
            st[i-1] = 0;

        r->callback(r, r->interface, r->protocol, event, sn, st, r->domain_name, r->srv_record->data.srv.name, &a, r->srv_record->data.srv.port, r->txt_record->data.txt.string_list, r->userdata);
    }
}

static void time_event_callback(AvahiTimeEvent *e, void *userdata) {
    AvahiSServiceResolver *r = userdata;
    
    assert(e);
    assert(r);

    finish(r, AVAHI_RESOLVER_TIMEOUT);
}

static void start_timeout(AvahiSServiceResolver *r) {
    struct timeval tv;
    assert(r);

    if (r->time_event)
        return;
    
    avahi_elapse_time(&tv, TIMEOUT_MSEC, 0);
    r->time_event = avahi_time_event_new(r->server->time_event_queue, &tv, time_event_callback, r);
}

static void record_browser_callback(AvahiSRecordBrowser*rr, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, AvahiRecord *record, void* userdata) {
    AvahiSServiceResolver *r = userdata;

    assert(rr);
    assert(record);
    assert(r);

    if (event == AVAHI_BROWSER_NEW) {
        int changed = 0;

        if (r->interface > 0 && interface != r->interface)
            return;
        
        if (r->protocol != AVAHI_PROTO_UNSPEC && protocol != r->protocol)
            return;
        
        if (r->interface <= 0)
            r->interface = interface;
        
        if (r->protocol == AVAHI_PROTO_UNSPEC)
            r->protocol = protocol;
        
        switch (record->key->type) {
            case AVAHI_DNS_TYPE_SRV:
                if (!r->srv_record) {
                    r->srv_record = avahi_record_ref(record);
                    changed = 1;
                    
                    assert(!r->record_browser_a && !r->record_browser_aaaa);
                    
                    if (r->address_protocol == AVAHI_PROTO_INET || r->address_protocol == AVAHI_PROTO_UNSPEC) {
                        AvahiKey *k = avahi_key_new(r->srv_record->data.srv.name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A);
                        r->record_browser_a = avahi_s_record_browser_new(r->server, r->interface, r->protocol, k, record_browser_callback, r);
                        avahi_key_unref(k);
                    } 
                    
                    if (r->address_protocol == AVAHI_PROTO_INET6 || r->address_protocol == AVAHI_PROTO_UNSPEC) {
                        AvahiKey *k = avahi_key_new(r->srv_record->data.srv.name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA);
                        r->record_browser_aaaa = avahi_s_record_browser_new(r->server, r->interface, r->protocol, k, record_browser_callback, r);
                        avahi_key_unref(k);
                    }
                }
                break;
                
            case AVAHI_DNS_TYPE_TXT:
                if (!r->txt_record) {
                    r->txt_record = avahi_record_ref(record);
                    changed = 1;
                }
                break;
                
            case AVAHI_DNS_TYPE_A:
            case AVAHI_DNS_TYPE_AAAA:
                if (!r->address_record) {
                    r->address_record = avahi_record_ref(record);
                    changed = 1;
                }
                break;
                
            default:
                abort();
        }


        if (changed && r->txt_record && r->srv_record && r->address_record)
            finish(r, AVAHI_RESOLVER_FOUND);

    } else {
        assert(event == AVAHI_BROWSER_REMOVE);

        
        switch (record->key->type) {
            case AVAHI_DNS_TYPE_SRV:

                if (r->srv_record && avahi_record_equal_no_ttl(record, r->srv_record)) {
                    avahi_record_unref(r->srv_record);
                    r->srv_record = NULL;

                    /** Look for a replacement */
                    avahi_s_record_browser_restart(r->record_browser_srv);
                    start_timeout(r);
                }
                
                break;

            case AVAHI_DNS_TYPE_TXT:

                if (r->txt_record && avahi_record_equal_no_ttl(record, r->txt_record)) {
                    avahi_record_unref(r->txt_record);
                    r->txt_record = NULL;

                    /** Look for a replacement */
                    avahi_s_record_browser_restart(r->record_browser_txt);
                    start_timeout(r);
                }
                break;

            case AVAHI_DNS_TYPE_A:
            case AVAHI_DNS_TYPE_AAAA:

                if (r->address_record && avahi_record_equal_no_ttl(record, r->address_record)) {
                    avahi_record_unref(r->address_record);
                    r->address_record = NULL;

                    /** Look for a replacement */
                    if (r->record_browser_aaaa)
                        avahi_s_record_browser_restart(r->record_browser_aaaa);
                    if (r->record_browser_a)
                        avahi_s_record_browser_restart(r->record_browser_a);
                    start_timeout(r);
                }
                break;

            default:
                abort();
        }
    }
}

AvahiSServiceResolver *avahi_s_service_resolver_new(AvahiServer *server, AvahiIfIndex interface, AvahiProtocol protocol, const char *name, const char *type, const char *domain, AvahiProtocol aprotocol, AvahiSServiceResolverCallback callback, void* userdata) {
    AvahiSServiceResolver *r;
    AvahiKey *k;
    char t[256], *n;
    size_t l;
    
    assert(server);
    assert(name);
    assert(type);
    assert(callback);

    assert(aprotocol == AVAHI_PROTO_UNSPEC || aprotocol == AVAHI_PROTO_INET || aprotocol == AVAHI_PROTO_INET6);

    if (!avahi_is_valid_service_name(name)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_SERVICE_NAME);
        return NULL;
    }

    if (!avahi_is_valid_service_type(type)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_SERVICE_TYPE);
        return NULL;
    }

    if (!avahi_is_valid_domain_name(domain)) {
        avahi_server_set_errno(server, AVAHI_ERR_INVALID_DOMAIN_NAME);
        return NULL;
    }
    
    if (!(r = avahi_new(AvahiSServiceResolver, 1))) {
        avahi_server_set_errno(server, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }
    
    r->server = server;
    r->service_name = avahi_strdup(name);
    r->service_type = avahi_normalize_name(type);
    r->domain_name = avahi_normalize_name(domain);
    r->callback = callback;
    r->userdata = userdata;
    r->address_protocol = aprotocol;
    r->srv_record = r->txt_record = r->address_record = NULL;
    r->interface = interface;
    r->protocol = protocol;
    
    n = t;
    l = sizeof(t);
    avahi_escape_label((const uint8_t*) name, strlen(name), &n, &l);
    snprintf(n, l, ".%s.%s", r->service_type, r->domain_name);

    r->time_event = NULL;
    start_timeout(r);
    
    AVAHI_LLIST_PREPEND(AvahiSServiceResolver, resolver, server->service_resolvers, r);

    r->record_browser_a = r->record_browser_aaaa = r->record_browser_srv = r->record_browser_txt = NULL;
    
    k = avahi_key_new(t, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_SRV);
    r->record_browser_srv = avahi_s_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
    avahi_key_unref(k);

    if (!r->record_browser_srv) {
        avahi_s_service_resolver_free(r);
        return NULL;
    }
    
    k = avahi_key_new(t, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT);
    r->record_browser_txt = avahi_s_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
    avahi_key_unref(k);

    if (!r->record_browser_txt) {
        avahi_s_service_resolver_free(r);
        return NULL;
    }

    return r;
}

void avahi_s_service_resolver_free(AvahiSServiceResolver *r) {
    assert(r);

    AVAHI_LLIST_REMOVE(AvahiSServiceResolver, resolver, r->server->service_resolvers, r);

    if (r->time_event)
        avahi_time_event_free(r->time_event);
    
    if (r->record_browser_srv)
        avahi_s_record_browser_free(r->record_browser_srv);
    if (r->record_browser_txt)
        avahi_s_record_browser_free(r->record_browser_txt);
    if (r->record_browser_a)
        avahi_s_record_browser_free(r->record_browser_a);
    if (r->record_browser_aaaa)
        avahi_s_record_browser_free(r->record_browser_aaaa);

    if (r->srv_record)
        avahi_record_unref(r->srv_record);
    if (r->txt_record)
        avahi_record_unref(r->txt_record);
    if (r->address_record)
        avahi_record_unref(r->address_record);
    
    avahi_free(r->service_name);
    avahi_free(r->service_type);
    avahi_free(r->domain_name);
    avahi_free(r);
}
