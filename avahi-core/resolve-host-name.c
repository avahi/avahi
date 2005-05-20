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

#include "resolve.h"
#include "util.h"

struct AvahiHostNameResolver {
    AvahiServer *server;
    gchar *host_name;
    
    AvahiRecordResolver *record_resolver_a;
    AvahiRecordResolver *record_resolver_aaaa;

    AvahiHostNameResolverCallback callback;
    gpointer userdata;

    AVAHI_LLIST_FIELDS(AvahiHostNameResolver, resolver);
};

static void record_resolver_callback(AvahiRecordResolver*rr, gint interface, guchar protocol, AvahiBrowserEvent event, AvahiRecord *record, gpointer userdata) {
    AvahiHostNameResolver *r = userdata;
    AvahiAddress a;

    g_assert(rr);
    g_assert(record);
    g_assert(r);

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

    r->callback(r, interface, protocol, event, record->key->name, &a, r->userdata);
}

AvahiHostNameResolver *avahi_host_name_resolver_new(AvahiServer *s, gint interface, guchar protocol, const gchar *host_name, AvahiHostNameResolverCallback callback, gpointer userdata) {
    AvahiHostNameResolver *r;
    AvahiKey *k;

    g_assert(s);
    g_assert(host_name);
    g_assert(callback);

    r = g_new(AvahiHostNameResolver, 1);
    r->server = s;
    r->host_name = avahi_normalize_name(host_name);
    r->callback = callback;
    r->userdata = userdata;

    k = avahi_key_new(host_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A);
    r->record_resolver_a = avahi_record_resolver_new(s, interface, protocol, k, record_resolver_callback, r);
    avahi_key_unref(k);

    k = avahi_key_new(host_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA);
    r->record_resolver_aaaa = avahi_record_resolver_new(s, interface, protocol, k, record_resolver_callback, r);
    avahi_key_unref(k);

    AVAHI_LLIST_PREPEND(AvahiHostNameResolver, resolver, s->host_name_resolvers, r);
    
    return r;
}

void avahi_host_name_resolver_free(AvahiHostNameResolver *r) {
    g_assert(r);

    AVAHI_LLIST_REMOVE(AvahiHostNameResolver, resolver, r->server->host_name_resolvers, r);

    avahi_record_resolver_free(r->record_resolver_a);
    avahi_record_resolver_free(r->record_resolver_aaaa);
    g_free(r->host_name);
    g_free(r);
}
