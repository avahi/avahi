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

#include "dns.h"
#include "util.h"

#define MDNS_PORT 5353

static void mdns_mcast_group_ipv4(struct sockaddr_in *ret_sa) {
    g_assert(ret_sa);

    memset(ret_sa, 0, sizeof(struct sockaddr_in));
    
    ret_sa->sin_family = AF_INET;
    ret_sa->sin_port = htons(MDNS_PORT);
    inet_pton(AF_INET, "224.0.0.251", &ret_sa->sin_addr);
}

gint flx_open_socket_ipv4(void) {
    struct ip_mreqn mreq;
    struct sockaddr_in sa, local;
    int fd = -1, ttl, yes;

    mdns_mcast_group_ipv4(&sa);
        
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        g_warning("socket() failed: %s\n", strerror(errno));
        goto fail;
    }
    
    ttl = 255;
    if (setsockopt(fd, SOL_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        g_warning("IP_MULTICAST_TTL failed: %s\n", strerror(errno));
        goto fail;
    }

    ttl = 255;
    if (setsockopt(fd, SOL_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        g_warning("IP_TTL failed: %s\n", strerror(errno));
        goto fail;
    }
    
    yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        g_warning("SO_REUSEADDR failed: %s\n", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IP, IP_MULTICAST_LOOP, &yes, sizeof(yes)) < 0) {
        g_warning("IP_MULTICAST_LOOP failed: %s\n", strerror(errno));
        goto fail;
    }

    
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(MDNS_PORT);
    
    if (bind(fd, (struct sockaddr*) &local, sizeof(local)) < 0) {
        g_warning("bind() failed: %s\n", strerror(errno));
        goto fail;
    }

    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr = sa.sin_addr;
    mreq.imr_address.s_addr = htonl(INADDR_ANY);
    mreq.imr_ifindex = 0;
    
    if (setsockopt(fd, SOL_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        g_warning("IP_ADD_MEMBERSHIP failed: %s\n", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IP, IP_RECVTTL, &yes, sizeof(yes)) < 0) {
        g_warning("IP_RECVTTL failed: %s\n", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IP, IP_PKTINFO, &yes, sizeof(yes)) < 0) {
        g_warning("IP_PKTINFO failed: %s\n", strerror(errno));
        goto fail;
    }
    
    if (flx_set_cloexec(fd) < 0) {
        g_warning("FD_CLOEXEC failed: %s\n", strerror(errno));
        goto fail;
    }
    
    if (flx_set_nonblock(fd) < 0) {
        g_warning("O_NONBLOCK failed: %s\n", strerror(errno));
        goto fail;
    }

    return fd;

fail:
    if (fd >= 0)
        close(fd);

    return -1;
}

static void mdns_mcast_group_ipv6(struct sockaddr_in6 *ret_sa) {
    g_assert(ret_sa);

    memset(ret_sa, 0, sizeof(struct sockaddr_in6));
    
    ret_sa->sin6_family = AF_INET6;
    ret_sa->sin6_port = htons(MDNS_PORT);
    inet_pton(AF_INET6, "ff02::fb", &ret_sa->sin6_addr);
}


gint flx_open_socket_ipv6(void) {
    struct ipv6_mreq mreq;
    struct sockaddr_in6 sa, local;
    int fd = -1, ttl, yes;

    mdns_mcast_group_ipv6(&sa);
        
    if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        g_warning("socket() failed: %s\n", strerror(errno));
        goto fail;
    }
    
    ttl = 255;
    if (setsockopt(fd, SOL_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl)) < 0) {
        g_warning("IPV6_MULTICAST_HOPS failed: %s\n", strerror(errno));
        goto fail;
    }

    ttl = 255;
    if (setsockopt(fd, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl)) < 0) {
        g_warning("IPV6_UNICAST_HOPS failed: %s\n", strerror(errno));
        goto fail;
    }
    
    yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        g_warning("SO_REUSEADDR failed: %s\n", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) < 0) {
        g_warning("IPV6_V6ONLY failed: %s\n", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_MULTICAST_LOOP, &yes, sizeof(yes)) < 0) {
        g_warning("IPV6_MULTICAST_LOOP failed: %s\n", strerror(errno));
        goto fail;
    }

    memset(&local, 0, sizeof(local));
    local.sin6_family = AF_INET6;
    local.sin6_port = htons(MDNS_PORT);
    
    if (bind(fd, (struct sockaddr*) &local, sizeof(local)) < 0) {
        g_warning("bind() failed: %s\n", strerror(errno));
        goto fail;
    }

    memset(&mreq, 0, sizeof(mreq));
    mreq.ipv6mr_multiaddr = sa.sin6_addr;
    mreq.ipv6mr_interface = 0;
    
    if (setsockopt(fd, SOL_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        g_warning("IPV6_ADD_MEMBERSHIP failed: %s\n", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_HOPLIMIT, &yes, sizeof(yes)) < 0) {
        g_warning("IPV6_HOPLIMIT failed: %s\n", strerror(errno));
        goto fail;
    }

    yes = 1;
    if (setsockopt(fd, SOL_IPV6, IPV6_PKTINFO, &yes, sizeof(yes)) < 0) {
        g_warning("IPV6_PKTINFO failed: %s\n", strerror(errno));
        goto fail;
    }
    
    if (flx_set_cloexec(fd) < 0) {
        g_warning("FD_CLOEXEC failed: %s\n", strerror(errno));
        goto fail;
    }
    
    if (flx_set_nonblock(fd) < 0) {
        g_warning("O_NONBLOCK failed: %s\n", strerror(errno));
        goto fail;
    }

    return fd;

fail:
    if (fd >= 0)
        close(fd);

    return -1;
}

static gint sendmsg_loop(gint fd, struct msghdr *msg, gint flags) {
    g_assert(fd >= 0);
    g_assert(msg);

    for (;;) {
    
        if (sendmsg(fd, msg, flags) >= 0)
            break;
        
        if (errno != EAGAIN) {
            g_message("sendmsg() failed: %s\n", strerror(errno));
            return -1;
        }
        
        if (flx_wait_for_write(fd) < 0)
            return -1;
    }

    return 0;
}

gint flx_send_dns_packet_ipv4(gint fd, gint interface, flxDnsPacket *p) {
    struct sockaddr_in sa;
    struct msghdr msg;
    struct iovec io;
    struct cmsghdr *cmsg;
    struct in_pktinfo *pkti;
    uint8_t cmsg_data[sizeof(struct cmsghdr) + sizeof(struct in_pktinfo)];
    int i, n;

    g_assert(fd >= 0);
    g_assert(p);
    g_assert(flx_dns_packet_check_valid(p) >= 0);

    mdns_mcast_group_ipv4(&sa);

    memset(&io, 0, sizeof(io));
    io.iov_base = FLX_DNS_PACKET_DATA(p);
    io.iov_len = p->size;

    memset(cmsg_data, 0, sizeof(cmsg_data));
    cmsg = (struct cmsghdr*) cmsg_data;
    cmsg->cmsg_len = sizeof(cmsg_data);
    cmsg->cmsg_level = IPPROTO_IP;
    cmsg->cmsg_type = IP_PKTINFO;

    pkti = (struct in_pktinfo*) (cmsg_data + sizeof(struct cmsghdr));
    pkti->ipi_ifindex = interface;
    
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_data;
    msg.msg_controllen = sizeof(cmsg_data);
    msg.msg_flags = 0;

    return sendmsg_loop(fd, &msg, MSG_DONTROUTE);
}

gint flx_send_dns_packet_ipv6(gint fd, gint interface, flxDnsPacket *p) {
    struct sockaddr_in6 sa;
    struct msghdr msg;
    struct iovec io;
    struct cmsghdr *cmsg;
    struct in6_pktinfo *pkti;
    uint8_t cmsg_data[sizeof(struct cmsghdr) + sizeof(struct in6_pktinfo)];
    int i, n;

    g_assert(fd >= 0);
    g_assert(p);
    g_assert(flx_dns_packet_check_valid(p) >= 0);

    mdns_mcast_group_ipv6(&sa);

    memset(&io, 0, sizeof(io));
    io.iov_base = FLX_DNS_PACKET_DATA(p);
    io.iov_len = p->size;

    memset(cmsg_data, 0, sizeof(cmsg_data));
    cmsg = (struct cmsghdr*) cmsg_data;
    cmsg->cmsg_len = sizeof(cmsg_data);
    cmsg->cmsg_level = IPPROTO_IPV6;
    cmsg->cmsg_type = IPV6_PKTINFO;

    pkti = (struct in6_pktinfo*) (cmsg_data + sizeof(struct cmsghdr));
    pkti->ipi6_ifindex = interface;
    
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_data;
    msg.msg_controllen = sizeof(cmsg_data);
    msg.msg_flags = 0;

    return sendmsg_loop(fd, &msg, MSG_DONTROUTE);
}

flxDnsPacket* flx_recv_dns_packet_ipv4(gint fd, struct sockaddr_in *ret_sa, gint *ret_iface, guint8* ret_ttl) {
    flxDnsPacket *p= NULL;
    struct msghdr msg;
    struct iovec io;
    uint8_t aux[64];
    ssize_t l;
    struct cmsghdr *cmsg;
    gboolean found_ttl = FALSE, found_iface = FALSE;

    g_assert(fd >= 0);
    g_assert(ret_sa);
    g_assert(ret_iface);
    g_assert(ret_ttl);

    p = flx_dns_packet_new(0);

    io.iov_base = FLX_DNS_PACKET_DATA(p);
    io.iov_len = p->max_size;
    
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = ret_sa;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = aux;
    msg.msg_controllen = sizeof(aux);
    msg.msg_flags = 0;
    
    if ((l = recvmsg(fd, &msg, 0)) < 0)
        goto fail;

    p->size = (size_t) l;
    
    *ret_ttl = 0;
        
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg,cmsg)) {
        if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_TTL) {
            *ret_ttl = *(uint8_t *) CMSG_DATA(cmsg);
            found_ttl = TRUE;
        }
            
        if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_PKTINFO) {
            *ret_iface = ((struct in_pktinfo*) CMSG_DATA(cmsg))->ipi_ifindex;
            found_iface = TRUE;
        }
    }

    g_assert(found_iface);
    g_assert(found_ttl);

    return p;

fail:
    if (p)
        flx_dns_packet_free(p);

    return NULL;
}

flxDnsPacket* flx_recv_dns_packet_ipv6(gint fd, struct sockaddr_in6 *ret_sa, gint *ret_iface, guint8* ret_ttl) {
    flxDnsPacket *p = NULL;
    struct msghdr msg;
    struct iovec io;
    uint8_t aux[64];
    ssize_t l;
    struct cmsghdr *cmsg;
    gboolean found_ttl = FALSE, found_iface = FALSE;

    g_assert(fd >= 0);
    g_assert(ret_sa);
    g_assert(ret_iface);
    g_assert(ret_ttl);

    p = flx_dns_packet_new(0);

    io.iov_base = FLX_DNS_PACKET_DATA(p);
    io.iov_len = p->max_size;
    
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = ret_sa;
    msg.msg_namelen = sizeof(struct sockaddr_in6);
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = aux;
    msg.msg_controllen = sizeof(aux);
    msg.msg_flags = 0;
    
    if ((l = recvmsg(fd, &msg, 0)) < 0)
        goto fail;

    p->size = (size_t) l;
    
    *ret_ttl = 0;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_HOPLIMIT) {
            *ret_ttl = *(uint8_t *) CMSG_DATA(cmsg);
            found_ttl = TRUE;
        }
            
        if (cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
            *ret_iface = ((struct in6_pktinfo*) CMSG_DATA(cmsg))->ipi6_ifindex;
            found_iface = TRUE;
        }
    }

    g_assert(found_iface);
    g_assert(found_ttl);

    return p;

fail:
    if (p)
        flx_dns_packet_free(p);

    return NULL;
}

