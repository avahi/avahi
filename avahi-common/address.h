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

/** \file address.h Definitions and functions to manipulate IP addresses. */

#include <sys/socket.h>
#include <inttypes.h>

#include <avahi-common/cdecl.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** Protocol family specification, takes the values AVAHI_PROTO_INET, AVAHI_PROTO_INET6, AVAHI_PROTO_UNSPEC */
typedef int AvahiProtocol;

/** Numeric network interface index. Takes OS dependent values and the special constant AVAHI_IF_UNSPEC  */
typedef int AvahiIfIndex;

/** Values for AvahiProtocol */
enum {
    AVAHI_PROTO_INET = 0,     /**< IPv4 */
    AVAHI_PROTO_INET6 = 1,   /**< IPv6 */
    AVAHI_PROTO_UNSPEC = -1  /**< Unspecified/all protocol(s) */
};

/** Special values for AvahiIfIndex */
enum {
    AVAHI_IF_UNSPEC = -1       /**< Unspecified/all interface(s) */
};

/** Return TRUE if the specified interface index is valid */
#define AVAHI_IF_VALID(ifindex) (((ifindex) >= 0) || ((ifindex) == AVAHI_PROTO_UNSPEC))

/** Return TRUE if the specified protocol is valid */
#define AVAHI_PROTO_VALID(protocol) (((protocol) == AVAHI_PROTO_INET) || ((protocol) == AVAHI_PROTO_INET6) || ((protocol) == AVAHI_PROTO_UNSPEC))

/** An IPv4 address */
typedef struct {
    uint32_t address; /**< Address data in network byte order. */
} AvahiIPv4Address;


/** An IPv6 address */
typedef struct {
    uint8_t address[16]; /**< Address data */
} AvahiIPv6Address;

/** Protocol (address family) independent address structure */
typedef struct {
    AvahiProtocol proto; /**< Address family */

    union {
        AvahiIPv6Address ipv6;  /** Address when IPv6 */
        AvahiIPv4Address ipv4;  /** Address when IPv4 */
        uint8_t data[1];         /** Type independant data field */
    } data;
} AvahiAddress;

/** Return the address data size of the specified address. (4 for IPv4, 16 for IPv6) */
size_t avahi_address_get_size(const AvahiAddress *a);

/** Compare two addresses. Returns 0 when equal, a negative value when a < b, a positive value when a > b. */
int avahi_address_cmp(const AvahiAddress *a, const AvahiAddress *b);

/** Convert the specified address *a to a human readable character string */
char *avahi_address_snprint(char *ret_s, size_t length, const AvahiAddress *a);

/** Convert the specifeid human readable character string to an
 * address structure. Set af to AVAHI_UNSPEC for automatic address
 * family detection. */
AvahiAddress *avahi_address_parse(const char *s, AvahiProtocol af, AvahiAddress *ret_addr);

/** Make an address structture of a sockaddr structure */
AvahiAddress *avahi_address_from_sockaddr(const struct sockaddr* sa, AvahiAddress *ret_addr);

/** Return the port number of a sockaddr structure (either IPv4 or IPv6) */
uint16_t avahi_port_from_sockaddr(const struct sockaddr* sa);

/** Generate the DNS reverse lookup name for an IPv4 address. avahi_free() the result! */
char* avahi_reverse_lookup_name_ipv4(const AvahiIPv4Address *a);

/** Generate the modern DNS reverse lookup name for an IPv6 address, ending in ipv6.arpa. avahi_free() the result! */
char* avahi_reverse_lookup_name_ipv6_arpa(const AvahiIPv6Address *a);

/** Generate the historic DNS reverse lookup name for an IPv6 address, ending in ipv6.int. avahi_free() the result! */
char* avahi_reverse_lookup_name_ipv6_int(const AvahiIPv6Address *a);

/** Check whether the specified IPv6 address is in fact an
 * encapsulated IPv4 address, returns 1 if yes, 0 otherwise */
int avahi_address_is_ipv4_in_ipv6(const AvahiAddress *a);

/** Map AVAHI_PROTO_xxx constants to Unix AF_xxx constants */
int avahi_proto_to_af(AvahiProtocol proto);

/** Map Unix AF_xxx constants to AVAHI_PROTO_xxx constants */
AvahiProtocol avahi_af_to_proto(int af);

/** Return a textual representation of the specified protocol number. i.e. "IPv4", "IPv6" or "UNSPEC" */
const char* avahi_proto_to_string(AvahiProtocol proto);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
