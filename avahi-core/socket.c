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

#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <assert.h>

#include "dns.h"
#include "fdutil.h"
#include "socket.h"
#include "log.h"

static void mdns_mcast_group_ipv4(struct sockaddr_in *ret_sa) {
    assert(ret_sa);

    memset(ret_sa, 0, sizeof(struct sockaddr_in));
    ret_sa->sin_family = AF_INET;
    ret_sa->sin_port = htons(AVAHI_MDNS_PORT);
    inet_pton(AF_INET, AVAHI_IPV4_MCAST_GROUP, &ret_sa->sin_addr);
}

static void mdns_mcast_group_ipv6(struct sockaddr_in6 *ret_sa) {
    assert(ret_sa);

    memset(ret_sa, 0, sizeof(struct sockaddr_in6));
    ret_sa->sin6_family = AF_INET6;
    ret_sa->sin6_port = htons(AVAHI_MDNS_PORT);
    inet_pton(AF_INET6, AVAHI_IPV6_MCAST_GROUP, &ret_sa->sin6_addr);
}

static void ipv4_address_to_sockaddr(struct sockaddr_in *ret_sa, const AvahiIPv4Address *a, uint16_t port) {
    assert(ret_sa);
    assert(a);
    assert(port > 0);

    memset(ret_sa, 0, sizeof(struct sockaddr_in));
    ret_sa->sin_family = AF_INET;
    ret_sa->sin_port = htons(port);
    memcpy(&ret_sa->sin_addr, a, sizeof(AvahiIPv4Address));
}

static void ipv6_address_to_sockaddr(struct sockaddr_in6 *ret_sa, const AvahiIPv6Address *a, uint16_t port) {
    assert(ret_sa);
    assert(a);
    assert(port > 0);

    memset(ret_sa, 0, sizeof(struct sockaddr_in6));
    ret_sa->sin6_family = AF_INET6;
    ret_sa->sin6_port = htons(port);
    memcpy(&ret_sa->sin6_addr, a, sizeof(AvahiIPv6Address));
}

int avahi_mdns_mcast_join_ipv4(int fd, int idx) {
    struct ip_mreqn mreq; 
    struct sockaddr_in sa;

    mdns_mcast_group_ipv4 (&sa);
 
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr = sa.sin_addr;
    mreq.imr_ifindex = idx;
 
    if (setsockopt(fd, SOL_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        avahi_log_warn("IP_ADD_MEMBERSHIP failed: %s", strerror(errno));
        return -1;
    } 

    return 0;
}

int avahi_mdns_mcast_join_ipv6(int fd, int idx) {
    struct ipv6_mreq mreq6; 
    struct sockaddr_in6 sa6;

    mdns_mcast_group_ipv6 (&sa6);

    memset(&mreq6, 0, sizeof(mreq6));
    mreq6.ipv6mr_multiaddr = sa6.sin6_addr;
    mreq6.ipv6mr_interface = idx;

    if (setsockopt(fd, SOL_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6)) < 0) {
        avahi_log_warn("IPV6_ADD_MEMBERSHIP failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int avahi_mdns_mcast_leave_ipv4(int fd, int idx) {
    struct ip_mreqn mreq; 
    struct sockaddr_in sa;
    
    mdns_mcast_group_ipv4 (&sa);
 
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr = sa.sin_addr;
    mreq.imr_ifindex = idx;
 
    if (setsockopt(fd, SOL_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        avahi_log_warn("IP_DROP_MEMBERSHIP failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int avahi_mdns_mcast_leave_ipv6(int fd, int idx) {
    struct ipv6_mreq mreq6; 
    struct sockaddr_in6 sa6;

    mdns_mcast_group_ipv6 (&sa6);

    memset(&mreq6, 0, sizeof(mreq6));
    mreq6.ipv6mr_multiaddr = sa6.sin6_addr;
    mreq6.ipv6mr_interface = idx;

    if (setsockopt(fd, SOL_IPV6, IPV6_DROP_MEMBERSHIP, &mreq6, sizeof(mreq6)) < 0) {
        avahi_log_warn("IPV6_DROP_MEMBERSHIP failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static int bind_with_warn(int fd, const struct sockaddr *sa, socklen_t l) {
    int yes;
    
    assert(fd >= 0);
    assert(sa);
    assert(l > 0);
    
    if (bind(fd, sa, l) < 0) {

        if (errno != EADDRINUSE) {
            avahi_log_warn("bind() failed: %s", strerror(errno));
            return -1;
        }
            
        avahi_log_warn("*** WARNING: Detected another %s mDNS stack running on this host. This makes mDNS unreliable and is thus not recommended. ***",
                       sa->sa_family == AF_INET ? "IPv4" : "IPv6");

        /* Try again, this time with SO_REUSEADDR set */
        yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            avahi_log_warn("SO_REUSEADDR failed: %s", strerror(errno));
            return -1;
        }

        if (bind(fd, sa, l) < 0) {
            avahi_log_warn("bind() failed: %s", strerror(errno));
            return -1;
        }
    } else {

        /* We enable SO_REUSEADDR afterwards, to make sure that the
         * user may run other mDNS implementations if he really
         * wants. */
        
        yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            avahi_log_warn("SO_REUSEADDR failed: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}

int avahi_open_socket_ipv4(int no_reuse) {
    struct sockaddr_in local;
    int fd = -1, ttl, yes, r;
        
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        goto fail;
    }
    
    ttl = 255;
    if (setsockopt(fd, SOL_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        avahi_log_warn("IP_MULTICAST_TTL failed: %s", strerror(errno));
        goto fail;
    }

    ttl = 255;
    if (setsockopt(fd, SOL_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        avahi_log_warn("IP_TTL failed: %s", strerror(errno));
        goto fail;
    }
    
    yes = 1;
    if (setsockopt(fd, SOL_IP, IP_MULTICAST_LOOP, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IP_MULTICAST_LOOP failed: %s", strerror(errno));
        goto fail;
    }
    
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(AVAHI_MDNS_PORT);

    if (no_reuse)
        r = bind(fd, (struct sockaddr*) &local, sizeof(local));
    else
        r = bind_with_warn(fd, (struct sockaddr*) &local, sizeof(local));

    if (r < 0)
        goto fail;

    yes = 1;
    if (setsockopt(fd, SOL_IP, IP_RECVTTL, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IP_RECVTTL failed: %s", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IP, IP_PKTINFO, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IP_PKTINFO failed: %s", strerror(errno));
        goto fail;
    }
    
    if (avahi_set_cloexec(fd) < 0) {
        avahi_log_warn("FD_CLOEXEC failed: %s", strerror(errno));
        goto fail;
    }
    
    if (avahi_set_nonblock(fd) < 0) {
        avahi_log_warn("O_NONBLOCK failed: %s", strerror(errno));
        goto fail;
    }

    return fd;

fail:
    if (fd >= 0)
        close(fd);

    return -1;
}

int avahi_open_socket_ipv6(int no_reuse) {
    struct sockaddr_in6 sa, local;
    int fd = -1, ttl, yes, r;

    mdns_mcast_group_ipv6(&sa);
        
    if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        goto fail;
    }
    
    ttl = 255;
    if (setsockopt(fd, SOL_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl)) < 0) {
        avahi_log_warn("IPV6_MULTICAST_HOPS failed: %s", strerror(errno));
        goto fail;
    }

    ttl = 255;
    if (setsockopt(fd, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl)) < 0) {
        avahi_log_warn("IPV6_UNICAST_HOPS failed: %s", strerror(errno));
        goto fail;
    }
    
    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IPV6_V6ONLY failed: %s", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_MULTICAST_LOOP, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IPV6_MULTICAST_LOOP failed: %s", strerror(errno));
        goto fail;
    }

    memset(&local, 0, sizeof(local));
    local.sin6_family = AF_INET6;
    local.sin6_port = htons(AVAHI_MDNS_PORT);
    
    if (no_reuse)
        r = bind(fd, (struct sockaddr*) &local, sizeof(local));
    else
        r = bind_with_warn(fd, (struct sockaddr*) &local, sizeof(local));

    if (r < 0)
        goto fail;

    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_HOPLIMIT, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IPV6_HOPLIMIT failed: %s", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_PKTINFO, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IPV6_PKTINFO failed: %s", strerror(errno));
        goto fail;
    }
    
    if (avahi_set_cloexec(fd) < 0) {
        avahi_log_warn("FD_CLOEXEC failed: %s", strerror(errno));
        goto fail;
    }
    
    if (avahi_set_nonblock(fd) < 0) {
        avahi_log_warn("O_NONBLOCK failed: %s", strerror(errno));
        goto fail;
    }

    return fd;

fail:
    if (fd >= 0)
        close(fd);

    return -1;
}

static int sendmsg_loop(int fd, struct msghdr *msg, int flags) {
    assert(fd >= 0);
    assert(msg);

    for (;;) {
    
        if (sendmsg(fd, msg, flags) >= 0)
            break;
        
        if (errno != EAGAIN) {
            avahi_log_debug("sendmsg() failed: %s", strerror(errno));
            return -1;
        }
        
        if (avahi_wait_for_write(fd) < 0)
            return -1;
    }

    return 0;
}

int avahi_send_dns_packet_ipv4(int fd, int interface, AvahiDnsPacket *p, const AvahiIPv4Address *a, uint16_t port) {
    struct sockaddr_in sa;
    struct msghdr msg;
    struct iovec io;
    struct cmsghdr *cmsg;
    uint8_t cmsg_data[sizeof(struct cmsghdr) + sizeof(struct in_pktinfo)];

    assert(fd >= 0);
    assert(p);
    assert(avahi_dns_packet_check_valid(p) >= 0);
    assert(!a || port > 0);

    if (!a)
        mdns_mcast_group_ipv4(&sa);
    else
        ipv4_address_to_sockaddr(&sa, a, port);

    memset(&io, 0, sizeof(io));
    io.iov_base = AVAHI_DNS_PACKET_DATA(p);
    io.iov_len = p->size;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_flags = 0;

    if (interface >= 0) {
        struct in_pktinfo *pkti;
        
        memset(cmsg_data, 0, sizeof(cmsg_data));
        cmsg = (struct cmsghdr*) cmsg_data;
        cmsg->cmsg_len = sizeof(cmsg_data);
        cmsg->cmsg_level = IPPROTO_IP;
        cmsg->cmsg_type = IP_PKTINFO;
        
        pkti = (struct in_pktinfo*) (cmsg_data + sizeof(struct cmsghdr));
        pkti->ipi_ifindex = interface;

        msg.msg_control = cmsg_data;
        msg.msg_controllen = sizeof(cmsg_data);
    } else {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
    }

    return sendmsg_loop(fd, &msg, 0);
}

int avahi_send_dns_packet_ipv6(int fd, int interface, AvahiDnsPacket *p, const AvahiIPv6Address *a, uint16_t port) {
    struct sockaddr_in6 sa;
    struct msghdr msg;
    struct iovec io;
    struct cmsghdr *cmsg;
    uint8_t cmsg_data[sizeof(struct cmsghdr) + sizeof(struct in6_pktinfo)];

    assert(fd >= 0);
    assert(p);
    assert(avahi_dns_packet_check_valid(p) >= 0);

    if (!a)
        mdns_mcast_group_ipv6(&sa);
    else
        ipv6_address_to_sockaddr(&sa, a, port);

    memset(&io, 0, sizeof(io));
    io.iov_base = AVAHI_DNS_PACKET_DATA(p);
    io.iov_len = p->size;

    
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_flags = 0;

    if (interface >= 0) {
        struct in6_pktinfo *pkti;
    
        memset(cmsg_data, 0, sizeof(cmsg_data));
        cmsg = (struct cmsghdr*) cmsg_data;
        cmsg->cmsg_len = sizeof(cmsg_data);
        cmsg->cmsg_level = IPPROTO_IPV6;
        cmsg->cmsg_type = IPV6_PKTINFO;
        
        pkti = (struct in6_pktinfo*) (cmsg_data + sizeof(struct cmsghdr));
        pkti->ipi6_ifindex = interface;
        
        msg.msg_control = cmsg_data;
        msg.msg_controllen = sizeof(cmsg_data);
    } else {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
    }
    
    return sendmsg_loop(fd, &msg, 0);
}

AvahiDnsPacket* avahi_recv_dns_packet_ipv4(int fd, struct sockaddr_in *ret_sa, AvahiIPv4Address *ret_dest_address, int *ret_iface, uint8_t* ret_ttl) {
    AvahiDnsPacket *p= NULL;
    struct msghdr msg;
    struct iovec io;
    uint8_t aux[1024];
    ssize_t l;
    struct cmsghdr *cmsg;
    int found_ttl = 0, found_iface = 0;
    int ms;

    assert(fd >= 0);

    if (ioctl(fd, FIONREAD, &ms) < 0) {
        avahi_log_warn("ioctl(): %s", strerror(errno));
        goto fail;
    }

    p = avahi_dns_packet_new(ms + AVAHI_DNS_PACKET_EXTRA_SIZE);

    io.iov_base = AVAHI_DNS_PACKET_DATA(p);
    io.iov_len = p->max_size;
    
    memset(&msg, 0, sizeof(msg));
    if (ret_sa) {
        msg.msg_name = ret_sa;
        msg.msg_namelen = sizeof(struct sockaddr_in);
    } else {
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
    }
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = aux;
    msg.msg_controllen = sizeof(aux);
    msg.msg_flags = 0;
    
    if ((l = recvmsg(fd, &msg, 0)) < 0) {
        avahi_log_warn("recvmsg(): %s", strerror(errno));
        goto fail;
    }

    if (ret_sa && ret_sa->sin_addr.s_addr == INADDR_ANY) {
        /* Linux 2.4 behaves very strangely sometimes! */

        /*avahi_hexdump(AVAHI_DNS_PACKET_DATA(p), l); */
        goto fail;
    }
    
    assert(!(msg.msg_flags & MSG_CTRUNC));
    assert(!(msg.msg_flags & MSG_TRUNC));
    p->size = (size_t) l;

    if (ret_ttl)
        *ret_ttl = 0;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {

        if (cmsg->cmsg_level == SOL_IP) {
            
            if (cmsg->cmsg_type == IP_TTL) {
                if (ret_ttl)
                    *ret_ttl = (uint8_t) (*(int *) CMSG_DATA(cmsg));
                found_ttl = 1;
            } else if (cmsg->cmsg_type == IP_PKTINFO) {
                struct in_pktinfo *i = (struct in_pktinfo*) CMSG_DATA(cmsg);

                if (ret_iface)
                    *ret_iface = (int) i->ipi_ifindex;

                if (ret_dest_address)
                    ret_dest_address->address = i->ipi_addr.s_addr;
                found_iface = 1;
            }
        }
    }

    assert(found_iface);
    assert(found_ttl);

    return p;

fail:
    if (p)
        avahi_dns_packet_free(p);

    return NULL;
}

AvahiDnsPacket* avahi_recv_dns_packet_ipv6(int fd, struct sockaddr_in6 *ret_sa, AvahiIPv6Address *ret_dest_address, int *ret_iface, uint8_t* ret_ttl) {
    AvahiDnsPacket *p = NULL;
    struct msghdr msg;
    struct iovec io;
    uint8_t aux[64];
    ssize_t l;
    int ms;
    
    struct cmsghdr *cmsg;
    int found_ttl = 0, found_iface = 0;

    assert(fd >= 0);

    if (ioctl(fd, FIONREAD, &ms) < 0) {
        avahi_log_warn("ioctl(): %s", strerror(errno));
        goto fail;
    }
    
    p = avahi_dns_packet_new(ms + AVAHI_DNS_PACKET_EXTRA_SIZE);

    io.iov_base = AVAHI_DNS_PACKET_DATA(p);
    io.iov_len = p->max_size;
    
    memset(&msg, 0, sizeof(msg));
    if (ret_sa) {
        msg.msg_name = ret_sa;
        msg.msg_namelen = sizeof(struct sockaddr_in6);
    } else {
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
    }
    
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = aux;
    msg.msg_controllen = sizeof(aux);
    msg.msg_flags = 0;
    
    if ((l = recvmsg(fd, &msg, 0)) < 0) {
        avahi_log_warn("recvmsg(): %s", strerror(errno));
        goto fail;
    }

    p->size = (size_t) l;

    if (ret_ttl)
        *ret_ttl = 0;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_HOPLIMIT) {

            if (ret_ttl)
                *ret_ttl = (uint8_t) (*(int *) CMSG_DATA(cmsg));
            
            found_ttl = 1;
        }
            
        if (cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
            struct in6_pktinfo *i = (struct in6_pktinfo*) CMSG_DATA(cmsg);

            if (ret_iface)
                *ret_iface = i->ipi6_ifindex;

            if (ret_dest_address)
                memcpy(ret_dest_address->address, i->ipi6_addr.s6_addr, 16);
            found_iface = 1;
        }
    }

    assert(found_iface);
    assert(found_ttl);

    return p;

fail:
    if (p)
        avahi_dns_packet_free(p);

    return NULL;
}

int avahi_open_unicast_socket_ipv4(void) {
    struct sockaddr_in local;
    int fd = -1, yes;
        
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        goto fail;
    }
    
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    
    if (bind(fd, (struct sockaddr*) &local, sizeof(local)) < 0) {
        avahi_log_warn("bind() failed: %s", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IP, IP_RECVTTL, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IP_RECVTTL failed: %s", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IP, IP_PKTINFO, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IP_PKTINFO failed: %s", strerror(errno));
        goto fail;
    }
    
    if (avahi_set_cloexec(fd) < 0) {
        avahi_log_warn("FD_CLOEXEC failed: %s", strerror(errno));
        goto fail;
    }
    
    if (avahi_set_nonblock(fd) < 0) {
        avahi_log_warn("O_NONBLOCK failed: %s", strerror(errno));
        goto fail;
    }

    return fd;

fail:
    if (fd >= 0)
        close(fd);

    return -1;
}

int avahi_open_unicast_socket_ipv6(void) {
    struct sockaddr_in6 local;
    int fd = -1, yes;
        
    if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        goto fail;
    }
    
    memset(&local, 0, sizeof(local));
    local.sin6_family = AF_INET6;
    
    if (bind(fd, (struct sockaddr*) &local, sizeof(local)) < 0) {
        avahi_log_warn("bind() failed: %s", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_HOPLIMIT, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IPV6_HOPLIMIT failed: %s", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_PKTINFO, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IPV6_PKTINFO failed: %s", strerror(errno));
        goto fail;
    }
    
    if (avahi_set_cloexec(fd) < 0) {
        avahi_log_warn("FD_CLOEXEC failed: %s", strerror(errno));
        goto fail;
    }
    
    if (avahi_set_nonblock(fd) < 0) {
        avahi_log_warn("O_NONBLOCK failed: %s", strerror(errno));
        goto fail;
    }

    return fd;

fail:
    if (fd >= 0)
        close(fd);

    return -1;
}
