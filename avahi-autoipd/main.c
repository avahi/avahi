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

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <net/if.h>

#include <avahi-common/malloc.h>
#include <avahi-common/timeval.h>

#include <libdaemon/dfork.h>
#include <libdaemon/dsignal.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>
#include <libdaemon/dexec.h>

#ifndef __linux__
#error "avahi-autoipd is only available on Linux for now"
#endif

/* An implementation of RFC 3927 */

/* Constants from the RFC */
#define PROBE_WAIT 1
#define PROBE_NUM 3
#define PROBE_MIN 1
#define PROBE_MAX 2
#define ANNOUNCE_WAIT 2
#define ANNOUNCE_NUM 2
#define ANNOUNCE_INTERVAL 2
#define MAX_CONFLICTS 10
#define RATE_LIMIT_INTERVAL 60
#define DEFEND_INTERVAL 10

#define IPV4LL_NETWORK 0xA9FE0000L
#define IPV4LL_NETMASK 0xFFFF0000L
#define IPV4LL_HOSTMASK 0x0000FFFFL

#define ETHER_ADDRLEN 6
#define ARP_PACKET_SIZE (8+4+4+2*ETHER_ADDRLEN)

typedef enum State {
    STATE_INVALID,
    STATE_WAITING_PROBE,
    STATE_PROBING,
    STATE_WAITING_ANNOUNCE,
    STATE_ANNOUNCING,
    STATE_RUNNING,
    STATE_SLEEPING,
    STATE_MAX
} State;

typedef enum ArpOperation {
    ARP_REQUEST = 1,
    ARP_RESPONSE = 2
} ArpOperation;

typedef struct ArpPacketInfo {
    ArpOperation operation;

    uint32_t sender_ip_address, target_ip_address;
    uint8_t sender_hw_address[ETHER_ADDRLEN], target_hw_address[ETHER_ADDRLEN];
} ArpPacketInfo;

static State state = STATE_INVALID;
static int n_iteration = 0;
static int n_conflict = 0;

#define RANDOM_DEVICE "/dev/urandom"

static void init_rand_seed(void) {
    int fd;
    unsigned seed = 0;

    /* Try to initialize seed from /dev/urandom, to make it a little
     * less predictable, and to make sure that multiple machines
     * booted at the same time choose different random seeds.  */
    if ((fd = open(RANDOM_DEVICE, O_RDONLY)) >= 0) {
        read(fd, &seed, sizeof(seed));
        close(fd);
    }

    /* If the initialization failed by some reason, we add the time to the seed */
    seed ^= (unsigned) time(NULL);

    srand(seed);
}

static uint32_t pick_addr(uint32_t old_addr) {
    uint32_t addr;

    do {
        unsigned r = (unsigned) rand();

        /* Reduce to 16 bits */
        while (r > 0xFFFF)
            r = (r >> 16) ^ (r & 0xFFFF);
        
        addr = htonl(IPV4LL_NETWORK | (uint32_t) r);

    } while (addr == old_addr);

    return addr;
}

static void* packet_new(const ArpPacketInfo *info, size_t *packet_len) {
    uint8_t *r;

    assert(info);
    assert(packet_len);
    assert(info->operation == ARP_REQUEST || info->operation == ARP_RESPONSE);

    *packet_len = ARP_PACKET_SIZE;
    r = avahi_new0(uint8_t, *packet_len);
    
    r[1] = 1; /* HTYPE */
    r[2] = 8; /* PTYPE */
    r[4] = ETHER_ADDRLEN; /* HLEN */
    r[5] = 4; /* PLEN */
    r[7] = (uint8_t) info->operation;

    memcpy(r+8, info->sender_hw_address, ETHER_ADDRLEN);
    memcpy(r+14, &info->sender_ip_address, 4);
    memcpy(r+18, info->target_hw_address, ETHER_ADDRLEN);
    memcpy(r+24, &info->target_ip_address, 4);

    return r;
}

static void *packet_new_probe(uint32_t ip_address, const uint8_t*hw_address, size_t *packet_len) {
    ArpPacketInfo info;
    
    memset(&info, 0, sizeof(info));
    info.operation = ARP_REQUEST;
    memcpy(info.sender_hw_address, hw_address, ETHER_ADDRLEN);
    info.target_ip_address = ip_address;

    return packet_new(&info, packet_len);
}

static void *packet_new_announcement(uint32_t ip_address, const uint8_t* hw_address, size_t *packet_len) {
    ArpPacketInfo info;

    memset(&info, 0, sizeof(info));
    info.operation = ARP_REQUEST;
    memcpy(info.sender_hw_address, hw_address, ETHER_ADDRLEN);
    info.target_ip_address = ip_address;
    info.sender_ip_address = ip_address;

    return packet_new(&info, packet_len);
}

static int packet_parse(const void *data, size_t packet_len, ArpPacketInfo *info) {
    const uint8_t *p = data;
    
    assert(data);

    if (packet_len < ARP_PACKET_SIZE)
        return -1;

    /* Check HTYPE and PTYPE */
    if (p[0] != 0 || p[1] != 1 || p[2] != 8 || p[3] != 0)
        return -1;

    /* Check HLEN, PLEN, OPERATION */
    if (p[4] != ETHER_ADDRLEN || p[5] != 4 || p[6] != 0 || (p[7] != 1 && p[7] != 2))
        return -1;
    
    info->operation = p[7];
    memcpy(info->sender_hw_address, p+8, ETHER_ADDRLEN);
    memcpy(&info->sender_ip_address, p+14, 4);
    memcpy(info->target_hw_address, p+18, ETHER_ADDRLEN);
    memcpy(&info->target_ip_address, p+24, 4);

    return 0;
}

static void set_state(State st, int reset_counter) {
    const char* const state_table[] = {
        [STATE_INVALID] = "INVALID",
        [STATE_WAITING_PROBE] = "WAITING_PROBE",
        [STATE_PROBING] = "PROBING",
        [STATE_WAITING_ANNOUNCE] = "WAITING_ANNOUNCE", 
        [STATE_ANNOUNCING] = "ANNOUNCING",
        [STATE_RUNNING] = "RUNNING",
        [STATE_SLEEPING] = "SLEEPING"
    };
    
    assert(st < STATE_MAX);

    if (st == state && !reset_counter) {
        n_iteration++;
        daemon_log(LOG_DEBUG, "State iteration %s-%i", state_table[state], n_iteration);
    } else {
        daemon_log(LOG_DEBUG, "State transition %s-%i -> %s-0", state_table[state], n_iteration, state_table[st]);
        state = st;
        n_iteration = 0;
    }
}

static int add_address(int iface, uint32_t addr) {
    char buf[64];

    daemon_log(LOG_INFO, "Selected address %s", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));
    return 0;
}

static int remove_address(int iface, uint32_t addr) {
    char buf[64];
    
    daemon_log(LOG_INFO, "Removing address %s", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));
    return 0;
}

static int open_socket(int iface, uint8_t *hw_address) {
    int fd = -1;
    struct sockaddr_ll sa;
    socklen_t sa_len;
    
    if ((fd = socket(PF_PACKET, SOCK_DGRAM, 0)) < 0) {
        daemon_log(LOG_ERR, "socket() failed: %s", strerror(errno));
        goto fail;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ARP);
    sa.sll_ifindex = iface;

    if (bind(fd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
        daemon_log(LOG_ERR, "bind() failed: %s", strerror(errno));
        goto fail;
    }

    sa_len = sizeof(sa);
    if (getsockname(fd, (struct sockaddr*) &sa, &sa_len) < 0) {
        daemon_log(LOG_ERR, "getsockname() failed: %s", strerror(errno));
        goto fail;
    }

    if (sa.sll_halen != ETHER_ADDRLEN) {
        daemon_log(LOG_ERR, "getsockname() returned invalid hardware address.");
        goto fail;
    }

    memcpy(hw_address, sa.sll_addr, ETHER_ADDRLEN);

    return fd;
    
fail:
    if (fd >= 0)
        close(fd);

    return -1;
}

static int send_packet(int fd, int iface, void *packet, size_t packet_len) {
    struct sockaddr_ll sa;
    
    assert(fd >= 0);
    assert(packet);
    assert(packet_len > 0);

    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ARP);
    sa.sll_ifindex = iface;
    sa.sll_halen = ETHER_ADDRLEN;
    memset(sa.sll_addr, 0xFF, ETHER_ADDRLEN);

    if (sendto(fd, packet, packet_len, 0, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
        daemon_log(LOG_ERR, "sendto() failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static int recv_packet(int fd, void **packet, size_t *packet_len) {
    int s;
    struct sockaddr_ll sa;
    socklen_t sa_len;
    
    assert(fd >= 0);
    assert(packet);
    assert(packet_len);

    *packet = NULL;

    if (ioctl(fd, FIONREAD, &s) < 0) {
        daemon_log(LOG_ERR, "FIONREAD failed: %s", strerror(errno));
        goto fail;
    }

    assert(s > 0);

    *packet_len = (size_t) s;
    *packet = avahi_new(uint8_t, s);

    sa_len = sizeof(sa);
    if (recvfrom(fd, *packet, s, 0, (struct sockaddr*) &sa, &sa_len) < 0) {
        daemon_log(LOG_ERR, "recvfrom() failed: %s", strerror(errno));
        goto fail;
    }
    
    return 0;
    
fail:
    if (*packet)
        avahi_free(*packet);

    return -1;
}
 
static int is_ll_address(uint32_t addr) {
    return (ntohl(addr) & IPV4LL_NETMASK) == IPV4LL_NETWORK;
}

static struct timeval *elapse_time(struct timeval *tv, unsigned msec, unsigned jitter) {
    assert(tv);

    gettimeofday(tv, NULL);

    if (msec)
        avahi_timeval_add(tv, (AvahiUsec) msec*1000);

    if (jitter)
        avahi_timeval_add(tv, (AvahiUsec) (jitter*1000.0*rand()/(RAND_MAX+1.0)));
        
    return tv;
}

static int loop(int iface, uint32_t addr) {
    int fd = -1, ret = -1;
    struct timeval next_wakeup;
    int next_wakeup_valid = 0;
    char buf[64];
    void *in_packet = NULL;
    size_t in_packet_len;
    void *out_packet = NULL;
    size_t out_packet_len;
    uint8_t hw_address[ETHER_ADDRLEN];
    struct pollfd pollfds[1];

    enum {
        EVENT_NULL,
        EVENT_PACKET,
        EVENT_TIMEOUT
    } event = EVENT_NULL;
    
    if ((fd = open_socket(iface, hw_address)) < 0)
        goto fail;

    if (addr && !is_ll_address(addr)) {
        daemon_log(LOG_WARNING, "Requested address %s is not from IPv4LL range 169.254/16, ignoring.", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));
        addr = 0;
    }

    if (!addr) {
        int i;
        uint32_t a = 1;

        for (i = 0; i < ETHER_ADDRLEN; i++)
            a += hw_address[i]*i;

        addr = htonl(IPV4LL_NETWORK | (uint32_t) a);
    }

    daemon_log(LOG_INFO, "Starting with address %s", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));

    memset(pollfds, 0, sizeof(pollfds));
    pollfds[0].fd = fd;
    pollfds[0].events = POLLIN;
    
    for (;;) {
        int r, timeout;
        AvahiUsec usec;

        if (state == STATE_INVALID) {

            /* First, wait a random time */
            set_state(STATE_WAITING_PROBE, 1);

            elapse_time(&next_wakeup, 0, PROBE_WAIT*1000);
            next_wakeup_valid = 1;

        } else if ((state == STATE_WAITING_PROBE && event == EVENT_TIMEOUT) ||
                   (state == STATE_PROBING && event == EVENT_TIMEOUT && n_iteration < PROBE_NUM-2)) {

            /* Send a probe */
            out_packet = packet_new_probe(addr, hw_address, &out_packet_len);
            set_state(STATE_PROBING, 0);

            elapse_time(&next_wakeup, PROBE_MIN*1000, (PROBE_MAX-PROBE_MIN)*1000);
            next_wakeup_valid = 1;
            
        } else if (state == STATE_PROBING && event == EVENT_TIMEOUT && n_iteration >= PROBE_NUM-2) {

            /* Send the last probe */
            out_packet = packet_new_probe(addr, hw_address, &out_packet_len);
            set_state(STATE_WAITING_ANNOUNCE, 1);

            elapse_time(&next_wakeup, ANNOUNCE_WAIT*1000, 0);
            next_wakeup_valid = 1;
            
        } else if ((state == STATE_WAITING_ANNOUNCE && event == EVENT_TIMEOUT) ||
                   (state == STATE_ANNOUNCING && event == EVENT_TIMEOUT && n_iteration < ANNOUNCE_NUM-1)) {

            /* Send announcement packet */
            out_packet = packet_new_announcement(addr, hw_address, &out_packet_len);
            set_state(STATE_ANNOUNCING, 0);

            elapse_time(&next_wakeup, ANNOUNCE_INTERVAL*1000, 0);
            next_wakeup_valid = 1;
            
            if (n_iteration == 0) {
                add_address(iface, addr);
                n_conflict = 0;
            }

        } else if ((state == STATE_ANNOUNCING && event == EVENT_TIMEOUT && n_iteration >= ANNOUNCE_NUM-1)) {

            daemon_log(LOG_INFO, "Successfully claimed IP address %s", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));
            set_state(STATE_RUNNING, 0);
            
        } else if (event == EVENT_PACKET) {
            ArpPacketInfo info;

            assert(in_packet);
            
            if (packet_parse(in_packet, in_packet_len, &info) < 0)
                daemon_log(LOG_WARNING, "Failed to parse incoming ARP packet.");
            else {
                int conflict = 0;

                if (info.sender_ip_address == addr) {
                    /* Normal conflict */
                    conflict = 1;
                    daemon_log(LOG_INFO, "Recieved conflicting normal ARP packet.");
                } else if (state == STATE_WAITING_PROBE || state == STATE_PROBING || state == STATE_WAITING_ANNOUNCE) {
                    /* Probe conflict */
                    conflict = info.target_ip_address == addr && memcmp(hw_address, info.sender_hw_address, ETHER_ADDRLEN);
                    daemon_log(LOG_INFO, "Recieved conflicting probe ARP packet.");
                }

                if (conflict) {
                    
                    if (state == STATE_RUNNING || state == STATE_ANNOUNCING)
                        remove_address(iface, addr);
                    
                    /* Pick a new address */
                    addr = pick_addr(addr);

                    daemon_log(LOG_INFO, "Trying address %s", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));

                    set_state(STATE_WAITING_PROBE, 1);

                    n_conflict++;

                    if (n_conflict >= MAX_CONFLICTS) {
                        daemon_log(LOG_WARNING, "Got too many conflicts, rate limiting new probes.");
                        elapse_time(&next_wakeup, RATE_LIMIT_INTERVAL*1000, PROBE_WAIT*1000);
                    } else
                        elapse_time(&next_wakeup, 0, PROBE_WAIT*1000);

                    next_wakeup_valid = 1;
                } else
                    daemon_log(LOG_DEBUG, "Ignoring ARP packet.");
            }
        }
        
        if (out_packet) {
            daemon_log(LOG_DEBUG, "sending...");
            
            if (send_packet(fd, iface, out_packet, out_packet_len) < 0)
                goto fail;
            
            avahi_free(out_packet);
            out_packet = NULL;
        }

        if (in_packet) {
            avahi_free(in_packet);
            in_packet = NULL;
        }

        timeout = -1;
        
        if (next_wakeup_valid) {
            usec = avahi_age(&next_wakeup);
            timeout = usec < 0 ? (int) (-usec/1000) : 0;
        }

        daemon_log(LOG_DEBUG, "sleeping %ims", timeout);
                    
        while ((r = poll(pollfds, 1, timeout)) < 0 && errno == EINTR)
            ;

        if (r < 0) {
            daemon_log(LOG_ERR, "poll() failed: %s", strerror(r));
            break;
        } else if (r == 0) {
            event = EVENT_TIMEOUT;
            next_wakeup_valid = 0;
        } else {
            assert(pollfds[0].revents == POLLIN);

            if (recv_packet(fd, &in_packet, &in_packet_len) < 0)
                goto fail;

            if (in_packet)
                event = EVENT_PACKET;
        }
    }

    ret = 0;
    
fail:

    avahi_free(out_packet);
    avahi_free(in_packet);
    
    if (fd >= 0)
        close(fd);
    
    return ret;
}

static int get_ifindex(const char *name) {
    int fd = -1;
    struct ifreq ifreq;

    if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        daemon_log(LOG_ERR, "socket() failed: %s", strerror(errno));
        goto fail;
    }

    memset(&ifreq, 0, sizeof(ifreq));
    strncpy(ifreq.ifr_name, name, IFNAMSIZ-1);
    ifreq.ifr_name[IFNAMSIZ-1] = 0;

    if (ioctl(fd, SIOCGIFINDEX, &ifreq) < 0) {
        daemon_log(LOG_ERR, "SIOCGIFINDEX failed: %s", strerror(errno));
        goto fail;
    }

    return ifreq.ifr_ifindex;

fail:

    if (fd >= 0)
        close(fd);
    
    return -1;
}

int main(int argc, char*argv[]) {
    int ret = 1;
    int ifindex;
    uint32_t addr = 0;

    init_rand_seed();

    if ((ifindex = get_ifindex(argc >= 2 ? argv[1] : "eth0")) < 0)
        goto fail;

    if (argc >= 3)
        addr = inet_addr(argv[2]);
    
    if (loop(ifindex, addr) < 0)
        goto fail;
    
    ret = 0;

    
fail:
    
    return ret;
}

/* TODO:

- netlink
- man page
- user script
- chroot/drop privs/caps
- daemonize
- defend
- signals
- store last used address
- cmdline

*/
