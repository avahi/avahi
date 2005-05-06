#ifndef foosockethfoo
#define foosockethfoo

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

#include <netinet/in.h>

#include "dns.h"

#define AVAHI_MDNS_PORT 5353

gint avahi_open_socket_ipv4(void);
gint avahi_open_socket_ipv6(void);

gint avahi_send_dns_packet_ipv4(gint fd, gint iface, AvahiDnsPacket *p);
gint avahi_send_dns_packet_ipv6(gint fd, gint iface, AvahiDnsPacket *p);

AvahiDnsPacket *avahi_recv_dns_packet_ipv4(gint fd, struct sockaddr_in*ret_sa, gint *ret_iface, guint8 *ret_ttl);
AvahiDnsPacket *avahi_recv_dns_packet_ipv6(gint fd, struct sockaddr_in6*ret_sa, gint *ret_iface, guint8 *ret_ttl);

int avahi_mdns_mcast_join_ipv4(int index, int fd);
int avahi_mdns_mcast_join_ipv6(int index, int fd);

int avahi_mdns_mcast_leave_ipv4(int index, int fd);
int avahi_mdns_mcast_leave_ipv6(int index, int fd);

#endif
