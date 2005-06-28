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
#include <avahi-common/cdecl.h>

/** \file address.h Defintions and functions to manipulate IP addresses. */

AVAHI_C_DECL_BEGIN

/** Protocol family specification, takes the values AVAHI_INET, AVAHI_INET6, AVAHI_UNSPEC */
typedef guchar AvahiProtocol;

/** Numeric network interface index. Takes OS dependent values and the special constant AVAHI_IF_UNSPEC  */
typedef gint AvahiIfIndex;

/** Values for AvahiProtocol */
enum {
    AVAHI_PROTO_INET = AF_INET,     /**< IPv4 */
    AVAHI_PROTO_INET6 = AF_INET6,   /**< IPv6 */
    AVAHI_PROTO_UNSPEC = AF_UNSPEC  /**< Unspecified/all protocol(s) */
};

/** Special values for AvahiIfIndex */
enum {
    AVAHI_IF_UNSPEC = -1 /**< Unspecifed/all interfaces */
};

/** An IPv4 address */
typedef struct {
    guint32 address; /**< Address data in network byte order. */
} AvahiIPv4Address;

/** An IPv6 address */
typedef struct {
    guint8 address[16]; /**< Address data */
} AvahiIPv6Address;

/** Protocol (address family) independent address structure */
typedef struct {
    AvahiProtocol family; /**< Address family */

    union {
        AvahiIPv6Address ipv6;  /** Address when IPv6 */
        AvahiIPv4Address ipv4;  /** Address when IPv4 */
        guint8 data[1];         /** Type independant data field */
    } data;
} AvahiAddress;

/** Return the address data size of the specified address. (4 for IPv4, 16 for IPv6) */
guint avahi_address_get_size(const AvahiAddress *a);

/** Compare two addresses. Returns 0 when equal, a negative value when a < b, a positive value when a > b. */
gint avahi_address_cmp(const AvahiAddress *a, const AvahiAddress *b);

/** Convert the specified address *a to a human readable character string */
gchar *avahi_address_snprint(char *ret_s, guint length, const AvahiAddress *a);

/** Convert the specifeid human readable character string to an
 * address structure. Set af to AVAHI_UNSPEC for automatic address
 * family detection. */
AvahiAddress *avahi_address_parse(const char *s, AvahiProtocol af, AvahiAddress *ret_addr);

/** Make an address structture of a sockaddr structure */
AvahiAddress *avahi_address_from_sockaddr(const struct sockaddr* sa, AvahiAddress *ret_addr);

/** Return the port number of a sockaddr structure (either IPv4 or IPv6) */
guint16 avahi_port_from_sockaddr(const struct sockaddr* sa);

/** Generate the DNS reverse lookup name for an IPv4 address. g_free() the result! */
gchar* avahi_reverse_lookup_name_ipv4(const AvahiIPv4Address *a);

/** Generate the modern DNS reverse lookup name for an IPv6 address, ending in ipv6.arpa. g_free() the result! */
gchar* avahi_reverse_lookup_name_ipv6_arpa(const AvahiIPv6Address *a);

/** Generate the historic DNS reverse lookup name for an IPv6 address, ending in ipv6.int. g_free() the result! */
gchar* avahi_reverse_lookup_name_ipv6_int(const AvahiIPv6Address *a);

/** Check whether the specified IPv6 address is in fact an
 * encapsulated IPv4 address */
gboolean avahi_address_is_ipv4_in_ipv6(const AvahiAddress *a);

AVAHI_C_DECL_END

#endif
