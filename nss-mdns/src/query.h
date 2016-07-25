#ifndef fooqueryhfoo
#define fooqueryhfoo

/* $Id$ */

/***
  This file is part of nss-mdns.
 
  nss-mdns is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
 
  nss-mdns is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with nss-mdns; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <inttypes.h>

typedef struct {
    uint32_t address;
} ipv4_address_t;

typedef struct {
    uint8_t address[16];
} ipv6_address_t;

int mdns_open_socket(void);

int mdns_query_name(int fd,
               const char *name,
               void (*ipv4_func)(const ipv4_address_t *ipv4, void *userdata),
               void (*ipv6_func)(const ipv6_address_t *ipv6, void *userdata),
               void *userdata);

#ifndef NSS_IPV6_ONLY
int mdns_query_ipv4(int fd,
               const ipv4_address_t *ipv4,
               void (*name_func)(const char *name, void *userdata),
               void *userdata);
#endif
#ifndef NSS_IPV4_ONLY
int mdns_query_ipv6(int fd,
               const ipv6_address_t *ipv6,
               void (*name_func)(const char *name, void *userdata),
               void *userdata);
#endif

#endif
