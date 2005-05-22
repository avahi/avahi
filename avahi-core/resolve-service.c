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

struct AvahiServiceResolver {
    AvahiServer *server;
    gchar *service_name;
    gchar *service_type;
    gchar *domain_name;
    guchar address_protocol;

    gint interface;
    guchar protocol;

    AvahiRecordBrowser *record_browser_srv;
    AvahiRecordBrowser *record_browser_txt;
    AvahiRecordBrowser *record_browser_a;
    AvahiRecordBrowser *record_browser_aaaa;

    AvahiRecord *srv_record, *txt_record, *address_record;
    
    AvahiServiceResolverCallback callback;
    gpointer userdata;

    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiServiceResolver, resolver);
};

static void finish(AvahiServiceResolver *r, AvahiResolverEvent event) {
    g_assert(r);

    if (r->record_browser_a) {
        avahi_record_browser_free(r->record_browser_a);
        r->record_browser_a = NULL;
    }

    if (r->record_browser_aaaa) {
        avahi_record_browser_free(r->record_browser_aaaa);
        r->record_browser_aaaa = NULL;
    }

    if (r->record_browser_srv) {
        avahi_record_browser_free(r->record_browser_srv);
        r->record_browser_srv = NULL;
    }

    if (r->record_browser_txt) {
        avahi_record_browser_free(r->record_browser_txt);
        r->record_browser_txt = NULL;
    }
    
    avahi_time_event_queue_remove(r->server->time_event_queue, r->time_event);
    r->time_event = NULL;

    if (event == AVAHI_RESOLVER_TIMEOUT)
        r->callback(r, r->interface, r->protocol, event, r->service_name, r->service_type, r->domain_name, NULL, NULL, 0, NULL, r->userdata);
    else {
        AvahiAddress a;
        gchar sn[256], st[256];
        size_t i;
        
        g_assert(r->srv_record);
        g_assert(r->txt_record);
        g_assert(r->address_record);
        
        switch (r->address_record->key->type) {
            case AVAHI_DNS_TYPE_A:
                a.family = AF_INET;
                a.data.ipv4 = r->address_record->data.a.address;
                break;
                
            case AVAHI_DNS_TYPE_AAAA:
                a.family = AF_INET6;
                a.data.ipv6 = r->address_record->data.aaaa.address;
                break;
                
            default:
                g_assert(FALSE);
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

static void record_browser_callback(AvahiRecordBrowser*rr, gint interface, guchar protocol, AvahiBrowserEvent event, AvahiRecord *record, gpointer userdata) {
    AvahiServiceResolver *r = userdata;

    g_assert(rr);
    g_assert(record);
    g_assert(r);

    if (!(event == AVAHI_BROWSER_NEW))
        return;

    if (r->interface > 0 && interface != r->interface)
        return;

    if (r->protocol != AF_UNSPEC && protocol != r->protocol)
        return;
    
    if (r->interface <= 0)
        r->interface = interface;

    if (r->protocol == AF_UNSPEC)
        r->protocol = protocol;
    
    switch (record->key->type) {
        case AVAHI_DNS_TYPE_SRV:
            if (!r->srv_record) {
                r->srv_record = avahi_record_ref(record);

                g_assert(!r->record_browser_a && !r->record_browser_aaaa);
                
                if (r->address_protocol == AF_INET || r->address_protocol == AF_UNSPEC) {
                    AvahiKey *k = avahi_key_new(r->srv_record->data.srv.name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A);
                    r->record_browser_a = avahi_record_browser_new(r->server, r->interface, r->protocol, k, record_browser_callback, r);
                    avahi_key_unref(k);
                } 
                
                if (r->address_protocol == AF_INET6 || r->address_protocol == AF_UNSPEC) {
                    AvahiKey *k = avahi_key_new(r->srv_record->data.srv.name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA);
                    r->record_browser_aaaa = avahi_record_browser_new(r->server, r->interface, r->protocol, k, record_browser_callback, r);
                    avahi_key_unref(k);
                }
            }
            break;
            
        case AVAHI_DNS_TYPE_TXT:
            if (!r->txt_record)
                r->txt_record = avahi_record_ref(record);
            break;

        case AVAHI_DNS_TYPE_A:
        case AVAHI_DNS_TYPE_AAAA:
            if (!r->address_record)
                r->address_record = avahi_record_ref(record);
            break;
            
        default:
            g_assert(FALSE);
    }

    if (r->txt_record && r->srv_record && r->address_record)
        finish(r, AVAHI_RESOLVER_FOUND);
}

static void time_event_callback(AvahiTimeEvent *e, void *userdata) {
    AvahiServiceResolver *r = userdata;
    
    g_assert(e);
    g_assert(r);

    finish(r, AVAHI_RESOLVER_TIMEOUT);
}

AvahiServiceResolver *avahi_service_resolver_new(AvahiServer *server, gint interface, guchar protocol, const gchar *name, const gchar *type, const gchar *domain, guchar aprotocol, AvahiServiceResolverCallback callback, gpointer userdata) {
    AvahiServiceResolver *r;
    AvahiKey *k;
    GTimeVal tv;
    gchar t[256], *n;
    size_t l;
    
    g_assert(server);
    g_assert(name);
    g_assert(type);
    g_assert(callback);

    g_assert(aprotocol == AF_UNSPEC || aprotocol == AF_INET || aprotocol == AF_INET6);

    r = g_new(AvahiServiceResolver, 1);
    r->server = server;
    r->service_name = avahi_normalize_name(name);
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
    avahi_escape_label((guint8*) name, strlen(name), &n, &l);
    snprintf(n, l, ".%s%s", r->service_type, r->domain_name);

    g_message("<%s>", t);
    
    k = avahi_key_new(t, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_SRV);
    r->record_browser_srv = avahi_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
    avahi_key_unref(k);

    k = avahi_key_new(t, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT);
    r->record_browser_txt = avahi_record_browser_new(server, interface, protocol, k, record_browser_callback, r);
    avahi_key_unref(k);
    
    r->record_browser_a = r->record_browser_aaaa = NULL;
    
    avahi_elapse_time(&tv, 1000, 0);
    r->time_event = avahi_time_event_queue_add(server->time_event_queue, &tv, time_event_callback, r);
    
    AVAHI_LLIST_PREPEND(AvahiServiceResolver, resolver, server->service_resolvers, r);
    
    return r;
}

void avahi_service_resolver_free(AvahiServiceResolver *r) {
    g_assert(r);

    AVAHI_LLIST_REMOVE(AvahiServiceResolver, resolver, r->server->service_resolvers, r);

    if (r->record_browser_srv)
        avahi_record_browser_free(r->record_browser_srv);
    if (r->record_browser_txt)
        avahi_record_browser_free(r->record_browser_txt);
    if (r->record_browser_a)
        avahi_record_browser_free(r->record_browser_a);
    if (r->record_browser_aaaa)
        avahi_record_browser_free(r->record_browser_aaaa);

    if (r->srv_record)
        avahi_record_unref(r->srv_record);
    if (r->txt_record)
        avahi_record_unref(r->txt_record);
    if (r->address_record)
        avahi_record_unref(r->address_record);
    
    g_free(r->service_name);
    g_free(r->service_type);
    g_free(r->domain_name);
    g_free(r);
}
