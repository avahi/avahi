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

/* Exercises name conflict handling against a scripted peer that shares
 * this process and its event loop with the server under test. The peer
 * has its own multicast socket on one interface, the loopback interface
 * by default, so the test needs no network setup. The server warns
 * about another mDNS stack on the host: that is the peer.
 *
 * A daemon on the host would take part in the test and conflict with
 * the reverse address records of the server. On Linux the test moves
 * into a private network namespace when unprivileged user namespaces
 * are available. Otherwise it skips when something is bound to the
 * mDNS port.
 *
 * Scenarios:
 *
 *   real-conflict   The peer owns the host name and answers probes for
 *                   it. The server must choose another name.
 *
 *   stale-probe     Probes carrying a lexicographically later record for
 *                   the host name keep arriving, but nothing answers the
 *                   server's own probes. The server must keep its name
 *                   (RFC 6762 section 8.2).
 *
 *   stale-response  Two responses with a conflicting record arrive back
 *                   to back while the name is established. The server
 *                   returns to probing on the first and must ignore the
 *                   second, then establish the name again (RFC 6762
 *                   section 8.1).
 *
 *   withdraw        An entry group with two addresses loses the
 *                   records of one address to a conflicting response
 *                   while the records of the other wait for them. The
 *                   withdrawal must not complete the group's
 *                   registration or announce the waiting records.
 *
 *   tiebreak-denial The server publishes two unique records under its
 *                   host name. The peer makes it lose a simultaneous
 *                   probe tiebreak and denies the next probe for the
 *                   name it sees, once. The server must send no probe
 *                   for the name for most of a second after the lost
 *                   tiebreak, and the denial after its re-probe must
 *                   make it choose another name (RFC 6762 section 8.2,
 *                   the PROBING subtest of the Bonjour Conformance
 *                   Test).
 *
 * Exits with 0 when all scenarios pass, 1 on failure and 77 when the
 * test cannot run on this host. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <avahi-common/test-util.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#ifdef __linux__
#include <sched.h>
#endif

#include <avahi-common/alternative.h>
#include <avahi-common/domain.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/timeval.h>
#include <avahi-core/core.h>
#include <avahi-core/log.h>
#include <avahi-core/publish.h>

#include "dns.h"
#include "socket.h"

#define HOST_NAME "conflict-test"
#define HOST_FQDN HOST_NAME ".local"
#define GROUP_NAME "conflict-test-group.local"
#define OTHER_NAME "conflict-test-other.local"
#define PEER_PROBE_INTERVAL_MSEC 50
#define EXIT_SKIP 77

/* The peer shares address and port with the server and hears its own
 * multicast. Its packets carry this ID so that it can drop them on
 * receipt. avahi ignores the ID of queries and responses alike. RFC
 * 6762 section 18.1 wants zero in queries and lets receivers ignore
 * it in responses, so the peer breaks the transmission rule on
 * purpose. */
#define PEER_PACKET_ID 0xbc7

typedef struct Peer {
    int fd;
    AvahiIfIndex iface;
    AvahiIPv4Address address;
    AvahiWatch *watch;
    AvahiTimeout *probe_timeout;

    const char *answer_name;    /* answer queries for this name ... */
    AvahiIPv4Address answer_address; /* ... with this address */
    const char *probe_name;     /* send probes for this name ... */
    AvahiIPv4Address probe_address;  /* ... carrying this address */
    const char *watch_name;     /* count announcements of these two names ... */
    const char *watch_ptr_name; /* ... an A record and a reverse PTR record */

    /* tiebreak-denial: on the first probe for tiebreak_name send a
     * conflicting probe that wins, then deny the next probe for it */
    const char *tiebreak_name;
    AvahiIPv4Address tiebreak_address;
    AvahiIPv4Address denial_address;
    enum { TIEBREAK_OFF, TIEBREAK_WAIT_PROBE, TIEBREAK_WAIT_REPROBE, TIEBREAK_DONE } tiebreak_state;
    struct timeval tiebreak_time;
    unsigned n_early_reprobes;  /* probes for tiebreak_name within 800 ms of the lost tiebreak */

    unsigned n_received;
    unsigned n_answers;
    unsigned n_probes_sent;
    unsigned n_announcements;   /* responses for the watched names with a TTL > 0 */
} Peer;

static AvahiSimplePoll *simple_poll = NULL;
static const AvahiPoll *poll_api = NULL;
static Peer peer;

static AvahiServer *server = NULL;
static unsigned n_collisions = 0;
static unsigned n_running = 0;

static AvahiSEntryGroup *group = NULL;
static int group_reset_requested = 0;
static int group_established_after_reset = 0;
static int group_established_before_reset = 0;

static int failures = 0;

static void check(int condition, const char *scenario, const char *what) {
    if (condition)
        avahi_log_info("PASS %s: %s", scenario, what);
    else {
        avahi_log_error("FAIL %s: %s", scenario, what);
        failures++;
    }
}

/* Peer */

static AvahiRecord *make_a_record(const char *name, const AvahiIPv4Address *a) {
    AvahiRecord *r;

    r = avahi_record_new_full(name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A, AVAHI_DEFAULT_TTL_HOST_NAME);
    must(r);
    r->data.a.address = *a;

    return r;
}

static void peer_send(AvahiDnsPacket *p) {
    int r;

    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ID, PEER_PACKET_ID);
    r = avahi_send_dns_packet_ipv4(peer.fd, peer.iface, p, &peer.address, NULL, 0);
    if (r < 0) {
        avahi_log_error("FAIL peer: sending a packet failed");
        failures++;
    }
    avahi_dns_packet_free(p);
}

static void peer_send_response(const char *name, const AvahiIPv4Address *a) {
    AvahiDnsPacket *p;
    AvahiRecord *r;

    p = avahi_dns_packet_new_response(0, 1);
    must(p);
    r = make_a_record(name, a);
    must(avahi_dns_packet_append_record(p, r, 1, 0) != NULL);
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ANCOUNT, 1);
    avahi_record_unref(r);

    peer_send(p);
}

static void peer_send_probe(const char *name, const AvahiIPv4Address *a) {
    AvahiDnsPacket *p;
    AvahiRecord *r;
    AvahiKey *k;

    p = avahi_dns_packet_new_query(0);
    must(p);
    k = avahi_key_new(name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_ANY);
    must(k);
    must(avahi_dns_packet_append_key(p, k, 0) != NULL);
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_QDCOUNT, 1);
    avahi_key_unref(k);
    r = make_a_record(name, a);
    must(avahi_dns_packet_append_record(p, r, 0, 0) != NULL);
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_NSCOUNT, 1);
    avahi_record_unref(r);

    peer_send(p);
    peer.n_probes_sent++;
}

static void peer_handle_probe(void) {
    struct timeval now;

    switch (peer.tiebreak_state) {
        case TIEBREAK_WAIT_PROBE:
            peer_send_probe(peer.tiebreak_name, &peer.tiebreak_address);
            gettimeofday(&peer.tiebreak_time, NULL);
            peer.tiebreak_state = TIEBREAK_WAIT_REPROBE;
            break;

        case TIEBREAK_WAIT_REPROBE:
            gettimeofday(&now, NULL);
            if (avahi_timeval_diff(&now, &peer.tiebreak_time) < 800000)
                peer.n_early_reprobes++;
            peer_send_response(peer.tiebreak_name, &peer.denial_address);
            peer.tiebreak_state = TIEBREAK_DONE;
            break;

        default:
            break;
    }
}

static void peer_handle_query(AvahiDnsPacket *p) {
    unsigned n;
    int answer = 0, probe = 0;

    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT); n > 0; n--) {
        AvahiKey *k;
        int unicast_response;

        if (!(k = avahi_dns_packet_consume_key(p, &unicast_response)))
            return;

        if (peer.answer_name && avahi_domain_equal(k->name, peer.answer_name))
            answer = 1;

        /* A query with records in the authority section is a probe */
        if (peer.tiebreak_name && avahi_domain_equal(k->name, peer.tiebreak_name) &&
            avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT) > 0)
            probe = 1;

        avahi_key_unref(k);
    }

    if (answer) {
        peer_send_response(peer.answer_name, &peer.answer_address);
        peer.n_answers++;
    }

    if (probe)
        peer_handle_probe();
}

static void peer_handle_response(AvahiDnsPacket *p) {
    unsigned n;

    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT); n > 0; n--) {
        AvahiRecord *r;
        int cache_flush;

        if (!(r = avahi_dns_packet_consume_record(p, &cache_flush)))
            return;

        if (r->ttl > 0 &&
            ((r->key->type == AVAHI_DNS_TYPE_A && peer.watch_name && avahi_domain_equal(r->key->name, peer.watch_name)) ||
             (r->key->type == AVAHI_DNS_TYPE_PTR && peer.watch_ptr_name && avahi_domain_equal(r->key->name, peer.watch_ptr_name))))
            peer.n_announcements++;

        avahi_record_unref(r);
    }
}

static void peer_watch_callback(AVAHI_GCC_UNUSED AvahiWatch *w, int fd, AVAHI_GCC_UNUSED AvahiWatchEvent event, AVAHI_GCC_UNUSED void *userdata) {
    AvahiDnsPacket *p;
    AvahiIPv4Address src, dst;
    uint16_t port;
    AvahiIfIndex iface;
    uint8_t ttl;

    if (!(p = avahi_recv_dns_packet_ipv4(fd, &src, &port, &dst, &iface, &ttl)))
        return;

    peer.n_received++;

    if (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ID) == PEER_PACKET_ID) {
        avahi_dns_packet_free(p);
        return;
    }

    if (avahi_dns_packet_check_valid_multicast(p) >= 0) {
        if (avahi_dns_packet_is_query(p))
            peer_handle_query(p);
        else
            peer_handle_response(p);
    }

    avahi_dns_packet_free(p);
}

static void peer_probe_callback(AvahiTimeout *t, AVAHI_GCC_UNUSED void *userdata) {
    struct timeval tv;

    peer_send_probe(peer.probe_name, &peer.probe_address);
    poll_api->timeout_update(t, avahi_elapse_time(&tv, PEER_PROBE_INTERVAL_MSEC, 0));
}

static void peer_start_probing(const char *name, const AvahiIPv4Address *a) {
    struct timeval tv;

    peer.probe_name = name;
    peer.probe_address = *a;
    peer.n_probes_sent = 0;

    if (peer.probe_timeout)
        poll_api->timeout_update(peer.probe_timeout, avahi_elapse_time(&tv, 0, 0));
    else
        peer.probe_timeout = poll_api->timeout_new(poll_api, avahi_elapse_time(&tv, 0, 0), peer_probe_callback, NULL);
}

static void peer_stop_probing(void) {
    if (peer.probe_timeout) {
        poll_api->timeout_free(peer.probe_timeout);
        peer.probe_timeout = NULL;
    }
}

static void peer_reset(void) {
    peer_stop_probing();
    peer.answer_name = NULL;
    peer.probe_name = NULL;
    peer.watch_name = peer.watch_ptr_name = NULL;
    peer.tiebreak_name = NULL;
    peer.tiebreak_state = TIEBREAK_OFF;
    peer.n_early_reprobes = 0;
    peer.n_received = peer.n_answers = peer.n_probes_sent = peer.n_announcements = 0;
}

static int find_interface(const char *name, char *ret_name, size_t l, AvahiIPv4Address *ret_address) {
    struct ifaddrs *ifa, *i;
    int found = 0;

    if (getifaddrs(&ifa) < 0)
        return 0;

    /* Without a name, pick the first loopback interface that is up and has an IPv4 address */
    for (i = ifa; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !(i->ifa_flags & IFF_UP))
            continue;
        if (name ? strcmp(i->ifa_name, name) != 0 : !(i->ifa_flags & IFF_LOOPBACK))
            continue;

        snprintf(ret_name, l, "%s", i->ifa_name);
        ret_address->address = ((struct sockaddr_in*) i->ifa_addr)->sin_addr.s_addr;
        found = 1;
        break;
    }

    freeifaddrs(ifa);
    return found;
}

#ifdef __linux__
static int write_file(const char *path, const char *text) {
    int fd, r;

    if ((fd = open(path, O_WRONLY)) < 0)
        return -1;

    r = write(fd, text, strlen(text)) == (ssize_t) strlen(text) ? 0 : -1;
    close(fd);
    return r;
}
#endif

/* Move into a private network namespace with a loopback interface that
 * is up, like "unshare -rn" followed by "ip link set lo up". */
static int enter_private_netns(void) {
#ifdef __linux__
    char map[64];
    struct ifreq ifr;
    int fd;
    uid_t uid = getuid();
    gid_t gid = getgid();

    if (unshare(CLONE_NEWUSER | CLONE_NEWNET) < 0)
        return -1;

    /* Map this user to root in the new namespace. Bringing up the
     * interface does not depend on it, so failures are not fatal. */
    snprintf(map, sizeof(map), "0 %u 1", (unsigned) uid);
    if (write_file("/proc/self/setgroups", "deny") == 0 && write_file("/proc/self/uid_map", map) == 0) {
        snprintf(map, sizeof(map), "0 %u 1", (unsigned) gid);
        write_file("/proc/self/gid_map", map);
    }

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "lo", sizeof(ifr.ifr_name) - 1);
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        close(fd);
        return -1;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
#else
    return -1;
#endif
}

/* A bind without SO_REUSEADDR fails when any socket holds the port */
static int mdns_port_in_use(void) {
    struct sockaddr_in sa;
    int fd, in_use;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return 0;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(AVAHI_MDNS_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    in_use = bind(fd, (struct sockaddr*) &sa, sizeof(sa)) < 0 && errno == EADDRINUSE;
    close(fd);
    return in_use;
}

static int peer_open(const char *ifname, const AvahiIPv4Address *a) {
    memset(&peer, 0, sizeof(peer));
    peer.address = *a;

    if ((peer.fd = avahi_open_socket_ipv4(0)) < 0)
        return -1;

    if ((peer.iface = (AvahiIfIndex) if_nametoindex(ifname)) <= 0) {
        close(peer.fd);
        return -1;
    }

    if (avahi_mdns_mcast_join_ipv4(peer.fd, &peer.address, peer.iface, 1) < 0) {
        close(peer.fd);
        return -1;
    }

#ifdef IP_MULTICAST_ALL
    {
        int no = 0;
        /* Only see traffic of the group joined above, not groups other sockets joined */
        setsockopt(peer.fd, IPPROTO_IP, IP_MULTICAST_ALL, &no, sizeof(no));
    }
#endif

    peer.watch = poll_api->watch_new(poll_api, peer.fd, AVAHI_WATCH_IN, peer_watch_callback, NULL);
    must(peer.watch);

    return 0;
}

static void peer_close(void) {
    peer_stop_probing();
    poll_api->watch_free(peer.watch);
    close(peer.fd);
}

/* Server under test */

static void server_callback(AvahiServer *s, AvahiServerState state, AVAHI_GCC_UNUSED void *userdata) {
    switch (state) {
        case AVAHI_SERVER_RUNNING:
            n_running++;
            avahi_log_info("Server running as %s", avahi_server_get_host_name_fqdn(s));
            break;

        case AVAHI_SERVER_COLLISION: {
            char *n = avahi_alternative_host_name(avahi_server_get_host_name(s));
            n_collisions++;
            avahi_log_info("Host name conflict, retrying with %s", n);
            avahi_server_set_host_name(s, n);
            avahi_free(n);
            break;
        }

        case AVAHI_SERVER_FAILURE:
            avahi_log_error("Server failure: %s", avahi_strerror(avahi_server_errno(s)));
            failures++;
            avahi_simple_poll_quit(simple_poll);
            break;

        default:
            break;
    }
}

static void server_start(const char *ifname, int publish_hinfo) {
    AvahiServerConfig config;
    int error;

    n_collisions = n_running = 0;

    avahi_server_config_init(&config);
    config.host_name = avahi_strdup(HOST_NAME);
    config.allow_interfaces = avahi_string_list_new(ifname, NULL);
    config.publish_hinfo = publish_hinfo;
    config.publish_workstation = 0;
    config.publish_domain = 0;
    config.use_ipv6 = 0;

    server = avahi_server_new(poll_api, &config, server_callback, NULL, &error);
    avahi_server_config_free(&config);

    if (!server) {
        avahi_log_error("avahi_server_new() failed: %s", avahi_strerror(error));
        exit(1);
    }
}

static void server_stop(void) {
    avahi_server_free(server);
    server = NULL;
}

/* Run the event loop for the given time, or until something asks it to quit */
static void run_for(unsigned msec) {
    struct timeval deadline;

    avahi_elapse_time(&deadline, msec, 0);

    for (;;) {
        struct timeval now;
        AvahiUsec remaining;

        gettimeofday(&now, NULL);
        remaining = avahi_timeval_diff(&deadline, &now);
        if (remaining <= 0)
            break;

        if (avahi_simple_poll_iterate(simple_poll, (int) (remaining / 1000) + 1) != 0)
            break;
    }
}

static int server_has_host_name(const char *name) {
    return strcmp(avahi_server_get_host_name(server), name) == 0;
}

/* Scenarios */

static void scenario_real_conflict(const char *ifname) {
    const char *name = "real-conflict";
    AvahiAddress a;

    peer_reset();
    avahi_address_parse("127.0.0.99", AVAHI_PROTO_INET, &a);
    peer.answer_name = HOST_FQDN;
    peer.answer_address = a.data.ipv4;

    server_start(ifname, 0);
    run_for(5000);

    check(peer.n_answers > 0, name, "the peer answered a probe");
    check(n_collisions == 1, name, "the server detected exactly one conflict");
    check(!server_has_host_name(HOST_NAME) && n_running >= 1, name, "the server established another name");

    server_stop();
}

static void scenario_stale_probe(const char *ifname) {
    const char *name = "stale-probe";
    AvahiAddress a;

    peer_reset();
    /* Sorts after 127.0.0.1, so the server loses every tiebreak */
    avahi_address_parse("127.0.0.200", AVAHI_PROTO_INET, &a);
    peer_start_probing(HOST_FQDN, &a.data.ipv4);

    server_start(ifname, 0);
    run_for(2000);
    peer_stop_probing();
    run_for(4000);

    check(peer.n_probes_sent > 20, name, "the peer kept probing");
    check(n_collisions == 0, name, "the server did not detect a conflict");
    check(server_has_host_name(HOST_NAME) && n_running == 1, name, "the server established its own name");

    server_stop();
}

static void scenario_stale_response(const char *ifname) {
    const char *name = "stale-response";
    AvahiAddress a;
    unsigned before;

    peer_reset();
    avahi_address_parse("127.0.0.99", AVAHI_PROTO_INET, &a);
    peer.watch_name = HOST_FQDN;

    server_start(ifname, 0);
    run_for(2500);
    check(n_running == 1 && server_has_host_name(HOST_NAME), name, "the server established its name");

    before = peer.n_announcements;
    peer_send_response(HOST_FQDN, &a.data.ipv4);
    run_for(10);
    peer_send_response(HOST_FQDN, &a.data.ipv4);
    run_for(3000);

    check(n_collisions == 0, name, "the server did not detect a conflict");
    check(server_has_host_name(HOST_NAME), name, "the server kept its name");
    check(peer.n_announcements > before, name, "the server announced its name again");

    server_stop();
}

static void group_callback(AVAHI_GCC_UNUSED AvahiServer *s, AVAHI_GCC_UNUSED AvahiSEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) {
    if (state != AVAHI_ENTRY_GROUP_ESTABLISHED)
        return;

    if (group_reset_requested)
        group_established_after_reset = 1;
    else
        group_established_before_reset = 1;
}

static void scenario_withdraw(const char *ifname) {
    const char *name = "withdraw";
    char ptr_name[AVAHI_DOMAIN_NAME_MAX];
    AvahiAddress a, b, c, denial;

    peer_reset();
    group_reset_requested = group_established_after_reset = group_established_before_reset = 0;

    server_start(ifname, 0);
    run_for(2000);

    /* The group gets two addresses. Stale probes for the first one
     * keep its records probing while the records of the second finish
     * and wait for them. A conflicting response for the first address
     * then withdraws the whole group without goodbyes. Entries are
     * prepended to a group, so the records added first are removed
     * last, and a withdrawal that completed the group when the last
     * probing announcer went away would announce them. */
    avahi_address_parse("10.42.0.1", AVAHI_PROTO_INET, &a);
    avahi_address_parse("10.42.0.2", AVAHI_PROTO_INET, &b);
    avahi_address_parse("10.42.0.4", AVAHI_PROTO_INET, &c);
    avahi_address_parse("10.42.0.99", AVAHI_PROTO_INET, &denial);
    group = avahi_s_entry_group_new(server, group_callback, NULL);
    must(group);
    must(avahi_server_add_address(server, group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, OTHER_NAME, &c) >= 0);
    must(avahi_server_add_address(server, group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, GROUP_NAME, &a) >= 0);
    must(avahi_s_entry_group_commit(group) >= 0);
    peer.watch_name = OTHER_NAME;
    avahi_reverse_lookup_name(&c, ptr_name, sizeof(ptr_name));
    peer.watch_ptr_name = ptr_name;
    peer_start_probing(GROUP_NAME, &b.data.ipv4);

    run_for(1500);
    check(!group_established_before_reset, name, "the group was still registering when the stale probes stopped");

    /* Deny the next probe for the first address. The denial arrives
     * after that probe was sent, so it is a conflict, not a stale
     * response. */
    group_reset_requested = 1;
    peer_stop_probing();
    peer.tiebreak_name = GROUP_NAME;
    peer.denial_address = denial.data.ipv4;
    peer.tiebreak_state = TIEBREAK_WAIT_REPROBE;
    run_for(2500);

    check(peer.tiebreak_state == TIEBREAK_DONE, name, "the peer denied the re-probe");
    check(!group_established_after_reset, name, "the withdrawal did not complete the group's registration");
    check(peer.n_announcements == 0, name, "the waiting records of the withdrawn group were never announced");

    avahi_s_entry_group_free(group);
    group = NULL;
    server_stop();
}

static void scenario_tiebreak_denial(const char *ifname) {
    const char *name = "tiebreak-denial";
    AvahiAddress winner, denial;

    peer_reset();
    /* Sorts after 127.0.0.1, so the server loses the tiebreak */
    avahi_address_parse("127.0.0.200", AVAHI_PROTO_INET, &winner);
    avahi_address_parse("127.0.0.99", AVAHI_PROTO_INET, &denial);
    peer.tiebreak_name = HOST_FQDN;
    peer.tiebreak_address = winner.data.ipv4;
    peer.denial_address = denial.data.ipv4;
    peer.tiebreak_state = TIEBREAK_WAIT_PROBE;

    /* The HINFO record is a second unique record under the host name.
     * It must be deferred together with the address record, or its
     * probe draws the denial while the address record has not sent a
     * probe in its new cycle and ignores it. */
    server_start(ifname, 1);
    run_for(5000);

    check(peer.tiebreak_state == TIEBREAK_DONE, name, "the peer sent the conflicting probe and one denial");
    check(peer.n_early_reprobes == 0, name, "the server sent no probe for the name within 800 ms of losing the tiebreak");
    check(n_collisions == 1, name, "the denial of the deferred re-probe made the server choose another name");
    check(!server_has_host_name(HOST_NAME) && n_running >= 1, name, "the server established another name");

    server_stop();
}

int main(int argc, char *argv[]) {
    char ifname[IF_NAMESIZE];
    const char *wanted = NULL;
    AvahiIPv4Address a;
    int c;

    while ((c = getopt(argc, argv, "i:")) >= 0) {
        switch (c) {
            case 'i': wanted = optarg; break;
            default:
                fprintf(stderr, "usage: %s [-i interface]\n", argv[0]);
                return 1;
        }
    }

    if (!wanted && enter_private_netns() == 0)
        avahi_log_info("Running in a private network namespace.");
    else if (mdns_port_in_use()) {
        avahi_log_warn("Another mDNS stack is bound to port %u and no private network namespace is available, skipping.", AVAHI_MDNS_PORT);
        return EXIT_SKIP;
    }

    if (!find_interface(wanted, ifname, sizeof(ifname), &a)) {
        avahi_log_warn("No usable %s interface with an IPv4 address, skipping.", wanted ? wanted : "loopback");
        return EXIT_SKIP;
    }

    simple_poll = avahi_simple_poll_new();
    must(simple_poll);
    poll_api = avahi_simple_poll_get(simple_poll);

    if (peer_open(ifname, &a) < 0) {
        avahi_log_warn("Cannot open an mDNS socket on %s, skipping.", ifname);
        avahi_simple_poll_free(simple_poll);
        return EXIT_SKIP;
    }

    /* The peer must receive its own multicast, otherwise nothing below can work */
    peer_send_response("multicast-self-test.local", &a);
    run_for(1000);
    if (peer.n_received == 0) {
        avahi_log_warn("Multicast on %s does not loop back, skipping.", ifname);
        peer_close();
        avahi_simple_poll_free(simple_poll);
        return EXIT_SKIP;
    }

    avahi_log_info("Running on interface %s", ifname);

    scenario_real_conflict(ifname);
    scenario_stale_probe(ifname);
    scenario_stale_response(ifname);
    scenario_withdraw(ifname);
    scenario_tiebreak_denial(ifname);

    peer_close();
    avahi_simple_poll_free(simple_poll);

    return failures ? 1 : 0;
}
