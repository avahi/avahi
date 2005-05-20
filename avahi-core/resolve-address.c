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

struct AvahiAddressResolver {
    AvahiServer *server;
    AvahiAddress address;
    
    AvahiRecordResolver *record_resolver;

    AvahiAddressResolverCallback callback;
    gpointer userdata;

    AVAHI_LLIST_FIELDS(AvahiAddressResolver, resolver);
};

static void record_resolver_callback(AvahiRecordResolver*rr, gint interface, guchar protocol, AvahiBrowserEvent event, AvahiRecord *record, gpointer userdata) {
    AvahiAddressResolver *r = userdata;

    g_assert(rr);
    g_assert(record);
    g_assert(r);

    r->callback(r, interface, protocol, event, &r->address, record->data.ptr.name, r->userdata);
}

AvahiAddressResolver *avahi_address_resolver_new(AvahiServer *server, gint interface, guchar protocol, const AvahiAddress *address, AvahiAddressResolverCallback callback, gpointer userdata) {
    AvahiAddressResolver *r;
    AvahiKey *k;
    gchar *n;

    g_assert(server);
    g_assert(address);
    g_assert(callback);

    g_assert(address->family == AF_INET || address->family == AF_INET6);

    r = g_new(AvahiAddressResolver, 1);
    r->server = server;
    r->address = *address;
    r->callback = callback;
    r->userdata = userdata;

    if (address->family == AF_INET)
        n = avahi_reverse_lookup_name_ipv4(&address->data.ipv4);
    else 
        n = avahi_reverse_lookup_name_ipv6_arpa(&address->data.ipv6);
    
    k = avahi_key_new(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    g_free(n);
    
    r->record_resolver = avahi_record_resolver_new(server, interface, protocol, k, record_resolver_callback, r);
    avahi_key_unref(k);

    AVAHI_LLIST_PREPEND(AvahiAddressResolver, resolver, server->address_resolvers, r);
    
    return r;
}

void avahi_address_resolver_free(AvahiAddressResolver *r) {
    g_assert(r);

    AVAHI_LLIST_REMOVE(AvahiAddressResolver, resolver, r->server->address_resolvers, r);
    avahi_record_resolver_free(r->record_resolver);
    g_free(r);
}
