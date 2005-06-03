#ifndef fooaddresshfoo
#define fooaddresshfoo

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

#include <sys/socket.h>
#include <glib.h>

typedef struct {
    guint32 address;
} AvahiIPv4Address;

typedef struct {
    guint8 address[16];
} AvahiIPv6Address;

typedef struct {
    guchar family;

    union {
        AvahiIPv6Address ipv6;
        AvahiIPv4Address ipv4;
        guint8 data[1];
    } data;
} AvahiAddress;

guint avahi_address_get_size(const AvahiAddress *a);
gint avahi_address_cmp(const AvahiAddress *a, const AvahiAddress *b);

gchar *avahi_address_snprint(char *ret_s, guint length, const AvahiAddress *a);

AvahiAddress *avahi_address_parse(const char *s, guchar family, AvahiAddress *ret_addr);

AvahiAddress *avahi_address_from_sockaddr(const struct sockaddr* sa, AvahiAddress *ret_addr);
guint16 avahi_port_from_sockaddr(const struct sockaddr* sa);

gchar* avahi_reverse_lookup_name_ipv4(const AvahiIPv4Address *a);
gchar* avahi_reverse_lookup_name_ipv6_arpa(const AvahiIPv6Address *a);
gchar* avahi_reverse_lookup_name_ipv6_int(const AvahiIPv6Address *a);

gboolean avahi_address_is_ipv4_in_ipv6(const AvahiAddress *a);

#endif
