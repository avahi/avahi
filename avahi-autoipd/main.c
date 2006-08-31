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
#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>

#include <avahi-common/malloc.h>
#include <avahi-common/timeval.h>

#include <avahi-daemon/setproctitle.h>

#include <libdaemon/dfork.h>
#include <libdaemon/dsignal.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>
#include <libdaemon/dexec.h>

#include "main.h"
#include "iface.h"

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

typedef enum ArpOperation {
    ARP_REQUEST = 1,
    ARP_RESPONSE = 2
} ArpOperation;

typedef struct ArpPacketInfo {
    ArpOperation operation;

    uint32_t sender_ip_address, target_ip_address;
    uint8_t sender_hw_address[ETHER_ADDRLEN], target_hw_address[ETHER_ADDRLEN];
} ArpPacketInfo;

static State state = STATE_START;
static int n_iteration = 0;
static int n_conflict = 0;

static char *interface_name = NULL;
static char *pid_file_name = NULL;
static uint32_t start_address = 0;
static char *argv0 = NULL;
static int daemonize = 0;
static int wait_for_address = 0;
static int use_syslog = 0;
static int debug = 0;
static int modify_proc_title = 1;
static int force_bind = 0;

static enum {
    DAEMON_RUN,
    DAEMON_KILL,
    DAEMON_REFRESH,
    DAEMON_VERSION,
    DAEMON_HELP,
    DAEMON_CHECK
} command = DAEMON_RUN;

typedef enum CalloutEvent {
    CALLOUT_BIND,
    CALLOUT_CONFLICT,
    CALLOUT_UNBIND,
    CALLOUT_STOP,
    CALLOUT_MAX
} CalloutEvent;

#define RANDOM_DEVICE "/dev/urandom"

#define DEBUG(x) do {\
if (debug) { \
    x; \
} \
} while (0)

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

static void set_state(State st, int reset_counter, uint32_t address) {
    const char* const state_table[] = {
        [STATE_START] = "START",
        [STATE_WAITING_PROBE] = "WAITING_PROBE",
        [STATE_PROBING] = "PROBING",
        [STATE_WAITING_ANNOUNCE] = "WAITING_ANNOUNCE", 
        [STATE_ANNOUNCING] = "ANNOUNCING",
        [STATE_RUNNING] = "RUNNING",
        [STATE_SLEEPING] = "SLEEPING"
    };
    char buf[64];
    
    assert(st < STATE_MAX);

    if (st == state && !reset_counter) {
        n_iteration++;
        DEBUG(daemon_log(LOG_DEBUG, "State iteration %s-%i", state_table[state], n_iteration));
    } else {
        DEBUG(daemon_log(LOG_DEBUG, "State transition %s-%i -> %s-0", state_table[state], n_iteration, state_table[st]));
        state = st;
        n_iteration = 0;
    }

    if (modify_proc_title) {
        if (state == STATE_SLEEPING) 
            avahi_set_proc_title(argv0, "%s(%s): sleeping", argv0, interface_name);
        else if (state == STATE_ANNOUNCING)
            avahi_set_proc_title(argv0, "%s(%s): announcing %s", argv0, interface_name, inet_ntop(AF_INET, &address, buf, sizeof(buf)));
        else if (state == STATE_RUNNING)
            avahi_set_proc_title(argv0, "%s(%s): bound %s", argv0, interface_name, inet_ntop(AF_INET, &address, buf, sizeof(buf)));
        else
            avahi_set_proc_title(argv0, "%s(%s): probing %s", argv0, interface_name, inet_ntop(AF_INET, &address, buf, sizeof(buf)));
    }
}

static int interface_up(int iface) {
    int fd = -1;
    struct ifreq ifreq;

    if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        daemon_log(LOG_ERR, "socket() failed: %s", strerror(errno));
        goto fail;
    }

    memset(&ifreq, 0, sizeof(ifreq));
    if (!if_indextoname(iface, ifreq.ifr_name)) {
        daemon_log(LOG_ERR, "if_indextoname() failed: %s", strerror(errno));
        goto fail;
    }
    
    if (ioctl(fd, SIOCGIFFLAGS, &ifreq) < 0) {
        daemon_log(LOG_ERR, "SIOCGIFFLAGS failed: %s", strerror(errno));
        goto fail;
    }

    ifreq.ifr_flags |= IFF_UP;

    if (ioctl(fd, SIOCSIFFLAGS, &ifreq) < 0) {
        daemon_log(LOG_ERR, "SIOCSIFFLAGS failed: %s", strerror(errno));
        goto fail;
    }

    close(fd);

    return 0;
    
fail:
    if (fd >= 0)
        close(fd);
    
    return -1;
}

static int do_callout(CalloutEvent event, int iface, uint32_t addr) {
    char buf[64], ifname[IFNAMSIZ];
    const char * const event_table[CALLOUT_MAX] = {
        [CALLOUT_BIND] = "BIND",
        [CALLOUT_CONFLICT] = "CONFLICT",
        [CALLOUT_UNBIND] = "UNBIND",
        [CALLOUT_STOP] = "STOP"
    };

    daemon_log(LOG_INFO, "Callout %s, address %s on interface %s",
               event_table[event],
               inet_ntop(AF_INET, &addr, buf, sizeof(buf)),
               if_indextoname(iface, ifname));
    
    return 0;
}

static int open_socket(int iface, uint8_t *hw_address) {
    int fd = -1;
    struct sockaddr_ll sa;
    socklen_t sa_len;

    if (interface_up(iface) < 0)
        goto fail;
    
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
    ssize_t r;
    
    assert(fd >= 0);
    assert(packet);
    assert(packet_len);

    *packet = NULL;

    if (ioctl(fd, FIONREAD, &s) < 0) {
        daemon_log(LOG_ERR, "FIONREAD failed: %s", strerror(errno));
        goto fail;
    }

    if (s <= 0)
        s = 4096;

    *packet = avahi_new(uint8_t, s);

    sa_len = sizeof(sa);
    if ((r = recvfrom(fd, *packet, s, 0, (struct sockaddr*) &sa, &sa_len)) < 0) {
        daemon_log(LOG_ERR, "recvfrom() failed: %s", strerror(errno));
        goto fail;
    }
    
    *packet_len = (size_t) r;
    
    return 0;
    
fail:
    if (*packet) {
        avahi_free(*packet);
        *packet = NULL;
    }

    return -1;
}
 
int is_ll_address(uint32_t addr) {
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
    enum {
        FD_ARP,
        FD_IFACE,
        FD_SIGNAL,
        FD_MAX,
    };

    int fd = -1, ret = -1;
    struct timeval next_wakeup;
    int next_wakeup_valid = 0;
    char buf[64];
    void *in_packet = NULL;
    size_t in_packet_len;
    void *out_packet = NULL;
    size_t out_packet_len;
    uint8_t hw_address[ETHER_ADDRLEN];
    struct pollfd pollfds[FD_MAX];
    int iface_fd;
    Event event = EVENT_NULL;
    int retval_sent = !daemonize;
    State st;

    daemon_signal_init(SIGINT, SIGTERM, SIGCHLD, SIGHUP,0);

    if ((fd = open_socket(iface, hw_address)) < 0)
        goto fail;

    if ((iface_fd = iface_init(iface)) < 0)
        goto fail;

    if (force_bind)
        st = STATE_START;
    else if (iface_get_initial_state(&st) < 0)
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

    set_state(st, 1, addr);
    
    daemon_log(LOG_INFO, "Starting with address %s", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));

    if (state == STATE_SLEEPING)
        daemon_log(LOG_INFO, "Routable address already assigned, sleeping.");

    if (!retval_sent && (!wait_for_address || state == STATE_SLEEPING)) {
        daemon_retval_send(0);
        retval_sent = 1;
    }

    memset(pollfds, 0, sizeof(pollfds));
    pollfds[FD_ARP].fd = fd;
    pollfds[FD_ARP].events = POLLIN;
    pollfds[FD_IFACE].fd = iface_fd;
    pollfds[FD_IFACE].events = POLLIN;
    pollfds[FD_SIGNAL].fd = daemon_signal_fd();
    pollfds[FD_SIGNAL].events = POLLIN;
    
    for (;;) {
        int r, timeout;
        AvahiUsec usec;

        if (state == STATE_START) {

            /* First, wait a random time */
            set_state(STATE_WAITING_PROBE, 1, addr);

            elapse_time(&next_wakeup, 0, PROBE_WAIT*1000);
            next_wakeup_valid = 1;

        } else if ((state == STATE_WAITING_PROBE && event == EVENT_TIMEOUT) ||
                   (state == STATE_PROBING && event == EVENT_TIMEOUT && n_iteration < PROBE_NUM-2)) {

            /* Send a probe */
            out_packet = packet_new_probe(addr, hw_address, &out_packet_len);
            set_state(STATE_PROBING, 0, addr);

            elapse_time(&next_wakeup, PROBE_MIN*1000, (PROBE_MAX-PROBE_MIN)*1000);
            next_wakeup_valid = 1;
            
        } else if (state == STATE_PROBING && event == EVENT_TIMEOUT && n_iteration >= PROBE_NUM-2) {

            /* Send the last probe */
            out_packet = packet_new_probe(addr, hw_address, &out_packet_len);
            set_state(STATE_WAITING_ANNOUNCE, 1, addr);

            elapse_time(&next_wakeup, ANNOUNCE_WAIT*1000, 0);
            next_wakeup_valid = 1;
            
        } else if ((state == STATE_WAITING_ANNOUNCE && event == EVENT_TIMEOUT) ||
                   (state == STATE_ANNOUNCING && event == EVENT_TIMEOUT && n_iteration < ANNOUNCE_NUM-1)) {

            /* Send announcement packet */
            out_packet = packet_new_announcement(addr, hw_address, &out_packet_len);
            set_state(STATE_ANNOUNCING, 0, addr);

            elapse_time(&next_wakeup, ANNOUNCE_INTERVAL*1000, 0);
            next_wakeup_valid = 1;
            
            if (n_iteration == 0) {
                do_callout(CALLOUT_BIND, iface, addr);
                n_conflict = 0;

                if (!retval_sent) {
                    daemon_retval_send(0);
                    retval_sent = 1;
                }
            }

        } else if ((state == STATE_ANNOUNCING && event == EVENT_TIMEOUT && n_iteration >= ANNOUNCE_NUM-1)) {

            daemon_log(LOG_INFO, "Successfully claimed IP address %s", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));
            set_state(STATE_RUNNING, 0, addr);

            next_wakeup_valid = 0;
            
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
                        do_callout(CALLOUT_CONFLICT, iface, addr);
                    
                    /* Pick a new address */
                    addr = pick_addr(addr);

                    daemon_log(LOG_INFO, "Trying address %s", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));

                    n_conflict++;

                    set_state(STATE_WAITING_PROBE, 1, addr);
                    
                    if (n_conflict >= MAX_CONFLICTS) {
                        daemon_log(LOG_WARNING, "Got too many conflicts, rate limiting new probes.");
                        elapse_time(&next_wakeup, RATE_LIMIT_INTERVAL*1000, PROBE_WAIT*1000);
                    } else
                        elapse_time(&next_wakeup, 0, PROBE_WAIT*1000);

                    next_wakeup_valid = 1;
                } else
                    DEBUG(daemon_log(LOG_DEBUG, "Ignoring irrelevant ARP packet."));
            }
            
        } else if (event == EVENT_ROUTABLE_ADDR_CONFIGURED) {

            daemon_log(LOG_INFO, "A routable address has been configured.");

            if (state == STATE_RUNNING || state == STATE_ANNOUNCING)
                do_callout(CALLOUT_UNBIND, iface, addr);

            if (!retval_sent) {
                daemon_retval_send(0);
                retval_sent = 1;
            }
            
            set_state(STATE_SLEEPING, 1, addr);
            next_wakeup_valid = 0;
            
        } else if (event == EVENT_ROUTABLE_ADDR_UNCONFIGURED && state == STATE_SLEEPING && !force_bind) {

            daemon_log(LOG_INFO, "No longer a routable address configured, restarting probe process.");

            set_state(STATE_WAITING_PROBE, 1, addr);

            elapse_time(&next_wakeup, 0, PROBE_WAIT*1000);
            next_wakeup_valid = 1;

        } else if (event == EVENT_REFRESH_REQUEST && state == STATE_RUNNING && !force_bind) {

            /* The user requested a reannouncing of the address by a SIGHUP */
            daemon_log(LOG_INFO, "Reannouncing address.");
            
            /* Send announcement packet */
            out_packet = packet_new_announcement(addr, hw_address, &out_packet_len);
            set_state(STATE_ANNOUNCING, 1, addr);

            elapse_time(&next_wakeup, ANNOUNCE_INTERVAL*1000, 0);
            next_wakeup_valid = 1;
        }
        
        if (out_packet) {
            DEBUG(daemon_log(LOG_DEBUG, "sending..."));
            
            if (send_packet(fd, iface, out_packet, out_packet_len) < 0)
                goto fail;
            
            avahi_free(out_packet);
            out_packet = NULL;
        }

        if (in_packet) {
            avahi_free(in_packet);
            in_packet = NULL;
        }

        event = EVENT_NULL;
        timeout = -1;
        
        if (next_wakeup_valid) {
            usec = avahi_age(&next_wakeup);
            timeout = usec < 0 ? (int) (-usec/1000) : 0;
        }

        DEBUG(daemon_log(LOG_DEBUG, "sleeping %ims", timeout));
                    
        while ((r = poll(pollfds, FD_MAX, timeout)) < 0 && errno == EINTR)
            ;

        if (r < 0) {
            daemon_log(LOG_ERR, "poll() failed: %s", strerror(r));
            goto fail;
        } else if (r == 0) {
            event = EVENT_TIMEOUT;
            next_wakeup_valid = 0;
        } else {
            
            
            if (pollfds[FD_ARP].revents) {

                if (pollfds[FD_ARP].revents == POLLERR) {
                    /* The interface is probably down, let's recreate our socket */
                    
                    close(fd);

                    if ((fd = open_socket(iface, hw_address)) < 0)
                        goto fail;

                    pollfds[FD_ARP].fd = fd;
                    
                } else {
                
                    assert(pollfds[FD_ARP].revents == POLLIN);
                    
                    if (recv_packet(fd, &in_packet, &in_packet_len) < 0)
                        goto fail;
                    
                    if (in_packet)
                        event = EVENT_PACKET;
                }
            }

            if (event == EVENT_NULL &&
                pollfds[FD_IFACE].revents) {
                
                assert(pollfds[FD_IFACE].revents == POLLIN);

                if (iface_process(&event) < 0)
                    goto fail;
            }

            if (event == EVENT_NULL &&
                pollfds[FD_SIGNAL].revents) {

                int sig;
                assert(pollfds[FD_SIGNAL].revents == POLLIN);

                if ((sig = daemon_signal_next()) <= 0) {
                    daemon_log(LOG_ERR, "daemon_signal_next() failed");
                    goto fail;
                }

                switch(sig) {
                    case SIGINT:
                    case SIGTERM:
                        daemon_log(LOG_INFO, "Got %s, quitting.", sig == SIGINT ? "SIGINT" : "SIGTERM");
                        ret = 0;
                        goto fail;

                    case SIGCHLD:
                        waitpid(-1, NULL, WNOHANG);
                        break;
                    
                    case SIGHUP:
                        event = EVENT_REFRESH_REQUEST;
                        break;
                }
                
            }
        }
    }

    ret = 0;
    
fail:

    if (state == STATE_RUNNING || state == STATE_ANNOUNCING)
        do_callout(CALLOUT_STOP, iface, addr);

    avahi_free(out_packet);
    avahi_free(in_packet);
    
    if (fd >= 0)
        close(fd);

    if (iface_fd >= 0)
        iface_done();

    if (daemonize && !retval_sent)
        daemon_retval_send(ret);
    
    return ret;
}


static void help(FILE *f, const char *a0) {
    fprintf(f,
            "%s [options] INTERFACE\n"
            "    -h --help           Show this help\n"
            "    -D --daemonize      Daemonize after startup\n"
            "    -s --syslog         Write log messages to syslog(3) instead of STDERR\n"
            "    -k --kill           Kill a running daemon\n"
            "    -r --refresh        Request a running daemon to refresh it's IP address\n"
            "    -c --check          Return 0 if a daemon is already running\n"
            "    -V --version        Show version\n"
            "    -S --start=ADDRESS  Start with this address from the IPv4LL range\n"
            "                        169.254.0.0/16\n"
            "    -w --wait           Wait until an address has been acquired before\n"
            "                        daemonizing\n"
            "       --no-proc-title  Don't modify process title\n"
            "       --force-bind     Assign an IPv4LL address even if routable address\n"
            "                        is already assigned\n"
            "       --debug          Increase verbosity\n",
            a0);
}

static int parse_command_line(int argc, char *argv[]) {
    int c;
    
    enum {
        OPTION_NO_PROC_TITLE = 256,
        OPTION_FORCE_BIND,
        OPTION_DEBUG
    };
    
    static const struct option long_options[] = {
        { "help",          no_argument,       NULL, 'h' },
        { "daemonize",     no_argument,       NULL, 'D' },
        { "syslog",        no_argument,       NULL, 's' },
        { "kill",          no_argument,       NULL, 'k' },
        { "refresh",       no_argument,       NULL, 'r' },
        { "check",         no_argument,       NULL, 'c' },
        { "version",       no_argument,       NULL, 'V' },
        { "start",         required_argument, NULL, 'S' },
        { "wait",          no_argument,       NULL, 'w' },
        { "no-proc-title", no_argument,       NULL, OPTION_NO_PROC_TITLE },
        { "force-bind",    no_argument,       NULL, OPTION_FORCE_BIND },
        { "debug",         no_argument,       NULL, OPTION_DEBUG },
        { NULL, 0, NULL, 0 }
    };

    opterr = 0;
    while ((c = getopt_long(argc, argv, "hDskrcVS:w", long_options, NULL)) >= 0) {

        switch(c) {
            case 's':
                use_syslog = 1;
                break;
            case 'h':
                command = DAEMON_HELP;
                break;
            case 'D':
                daemonize = 1;
                break;
            case 'k':
                command = DAEMON_KILL;
                break;
            case 'V':
                command = DAEMON_VERSION;
                break;
            case 'r':
                command = DAEMON_REFRESH;
                break;
            case 'c':
                command = DAEMON_CHECK;
                break;
            case 'S':
                
                if ((start_address = inet_addr(optarg)) == (uint32_t) -1) {
                    fprintf(stderr, "Failed to parse IP address '%s'.", optarg);
                    return -1;
                }
                break;
            case 'w':
                wait_for_address = 1;
                break;
                
            case OPTION_NO_PROC_TITLE:
                modify_proc_title = 0;
                break;

            case OPTION_DEBUG:
                debug = 1;
                break;

            case OPTION_FORCE_BIND:
                force_bind = 1;
                break;

            default:
                fprintf(stderr, "Invalid command line argument: %c\n", c);
                return -1;
        }
    }

    if (command == DAEMON_RUN ||
        command == DAEMON_KILL ||
        command == DAEMON_REFRESH ||
        command == DAEMON_CHECK) {

        if (optind >= argc) {
            fprintf(stderr, "Missing interface name.\n");
            return -1;
        }

        interface_name = avahi_strdup(argv[optind++]);
    }

    if (optind != argc) {
        fprintf(stderr, "Too many arguments\n");
        return -1;
    }
        
    return 0;
}

static const char* pid_file_proc(void) {
    return pid_file_name;
}

int main(int argc, char*argv[]) {
    int r = 1;
    int wrote_pid_file = 0;
    char *log_ident = NULL;

    avahi_init_proc_title(argc, argv);

    if ((argv0 = strrchr(argv[0], '/')))
        argv0++;
    else
        argv0 = argv[0];

    argv0 = avahi_strdup(argv0);

    daemon_log_ident = argv0;
    
    if (parse_command_line(argc, argv) < 0)
        goto finish;

    daemon_log_ident = log_ident = avahi_strdup_printf("%s(%s)", argv0, interface_name);
    daemon_pid_file_proc = pid_file_proc;
    pid_file_name = avahi_strdup_printf(AVAHI_RUNTIME_DIR"/avahi-autoipd.%s.pid", interface_name);

    if (command == DAEMON_RUN) {
        pid_t pid;
        int ifindex;

        init_rand_seed();
        
        if ((ifindex = if_nametoindex(interface_name)) <= 0) {
            daemon_log(LOG_ERR, "Failed to get index for interface name '%s': %s", interface_name, strerror(errno));
            goto finish;
        }

        if (getuid() != 0) {
            daemon_log(LOG_ERR, "This program is intended to be run as root.");
            goto finish;
        }

        if ((pid = daemon_pid_file_is_running()) >= 0) {
            daemon_log(LOG_ERR, "Daemon already running on PID %u", pid);
            goto finish;
        }

        if (daemonize) {
            daemon_retval_init();
            
            if ((pid = daemon_fork()) < 0)
                goto finish;
            else if (pid != 0) {
                int ret;
                /** Parent **/

                if ((ret = daemon_retval_wait(20)) < 0) {
                    daemon_log(LOG_ERR, "Could not receive return value from daemon process.");
                    goto finish;
                }

                r = ret;
                goto finish;
            }

            /* Child */
        }

        if (use_syslog || daemonize)
            daemon_log_use = DAEMON_LOG_SYSLOG;

        chdir("/");

        if (daemon_pid_file_create() < 0) {
            daemon_log(LOG_ERR, "Failed to create PID file: %s", strerror(errno));

            if (daemonize)
                daemon_retval_send(1);
            goto finish;
        } else
            wrote_pid_file = 1;

        avahi_set_proc_title(argv0, "%s(%s): starting up", argv0, interface_name);
        
        if (loop(ifindex, start_address) < 0)
            goto finish;

        r = 0;
    } else if (command == DAEMON_HELP) {
        help(stdout, argv0);
        
        r = 0;
    } else if (command == DAEMON_VERSION) {
        printf("%s "PACKAGE_VERSION"\n", argv0);
        
        r = 0;
    } else if (command == DAEMON_KILL) {
        if (daemon_pid_file_kill_wait(SIGTERM, 5) < 0) {
            daemon_log(LOG_WARNING, "Failed to kill daemon: %s", strerror(errno));
            goto finish;
        }
        
        r = 0;
    } else if (command == DAEMON_REFRESH) {
        if (daemon_pid_file_kill(SIGHUP) < 0) {
            daemon_log(LOG_WARNING, "Failed to kill daemon: %s", strerror(errno));
            goto finish;
        }

        r = 0;
    } else if (command == DAEMON_CHECK)
        r = (daemon_pid_file_is_running() >= 0) ? 0 : 1;


finish:

    if (daemonize)
        daemon_retval_done();
    
    if (wrote_pid_file)
        daemon_pid_file_remove();

    avahi_free(log_ident);
    avahi_free(pid_file_name);
    avahi_free(argv0);
    avahi_free(interface_name);

    return r;
}

/* TODO:

- chroot/drop privs/caps
- user script
- store last used address
- man page

*/
