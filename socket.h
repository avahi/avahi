#ifndef foosockethfoo
#define foosockethfoo

#include <netinet/in.h>

#include "dns.h"

#define FLX_MDNS_PORT 5353



gint flx_open_socket_ipv4(void);
gint flx_open_socket_ipv6(void);

gint flx_send_dns_packet_ipv4(gint fd, gint iface, flxDnsPacket *p);
gint flx_send_dns_packet_ipv6(gint fd, gint iface, flxDnsPacket *p);

flxDnsPacket *flx_recv_dns_packet_ipv4(gint fd, struct sockaddr_in*ret_sa, gint *ret_iface, guint8 *ret_ttl);
flxDnsPacket *flx_recv_dns_packet_ipv6(gint fd, struct sockaddr_in6*ret_sa, gint *ret_iface, guint8 *ret_ttl);

int flx_mdns_mcast_join_ipv4(int index, int fd);
int flx_mdns_mcast_join_ipv6(int index, int fd);

int flx_mdns_mcast_leave_ipv4(int index, int fd);
int flx_mdns_mcast_leave_ipv6(int index, int fd);

#endif
