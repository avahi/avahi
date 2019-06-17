#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <avahi-common/alternative.h>
#include <avahi-common/malloc.h>
#include <avahi-common/timeval.h>
#include <avahi-common/simple-watch.h>

#include <avahi-core/core.h>
#include <avahi-core/dns-srv-rr.h>
#include <avahi-core/log.h>
#include <avahi-core/lookup.h>
#include <avahi-core/publish.h>
#include <avahi-core/socket.h>

AvahiServer *server;
const uint8_t *data;
static size_t size = sizeof(data);

static int reuseaddr(int fd) {
    int yes = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("SO_REUSEADDR failed: %s", strerror(errno));
        return -1;
    }

#ifdef SO_REUSEPORT
    yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("SO_REUSEPORT failed: %s", strerror(errno));
        if (errno != ENOPROTOOPT)
            return -1;
    }
#endif

    return 0;
}

static void send_server_message_ipv4(int same_port) {
    assert (same_port == 1 || same_port == 0);

    int sock = -1;
    unsigned char set = 1;
    struct sockaddr_in local;
    struct sockaddr_in destAddr;
    memset(&local, 0, sizeof(local));
    memset(&destAddr, 0, sizeof(destAddr));

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    unsigned short port = get_mdns_port() * same_port; //0 for random port
    local.sin_family = AF_INET;
    local.sin_port = htons(port);


    destAddr.sin_port = htons(get_mdns_port());
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = inet_addr(AVAHI_IPV4_MCAST_GROUP);

    if (sock < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        exit(1);
    }
    reuseaddr(sock);

    if (bind(sock, (struct sockaddr *) &local, sizeof(local)) < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        return;
    }

    if (setsockopt(sock, IPPROTO_IP, IP_PKTINFO, &set, sizeof(set)) < 0) {
        avahi_log_warn("IP_PKTINFO failed: %s", strerror(errno));
    }

    ssize_t s;
    if ((s = sendto(sock, data, size, 0, (struct sockaddr *) &destAddr, sizeof(destAddr))) < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        exit(1);
    }

    close(sock);

}

static void send_server_message_ipv6(int same_port) {

    assert (same_port == 1 || same_port == 0);

    int sock = -1;
    int port = get_mdns_port() * same_port;

    struct sockaddr_in6 local;
    struct sockaddr_in6 destAddr;
    memset(&local, 0, sizeof(local));
    memset(&destAddr, 0, sizeof(local));

    local.sin6_family = AF_INET6;
    local.sin6_port = htons(port);

    sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    destAddr.sin6_family = AF_INET6;
    destAddr.sin6_port = htons(get_mdns_port());
    inet_pton(AF_INET6, AVAHI_IPV6_MCAST_GROUP, &destAddr.sin6_addr);

    if (sock < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        exit(1);
    }

    reuseaddr(sock);
    if (bind(sock, (struct sockaddr *) &local, sizeof(local)) < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        return;
    }

    ssize_t s;
    if ((s = sendto(sock, data, size, 0, (struct sockaddr *) &destAddr, sizeof(destAddr))) < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        exit(1);
    }

    close(sock);
}

static void send_server_message(int same_port) {
    send_server_message_ipv4(same_port); //Send IPv4 Packet
    send_server_message_ipv6(same_port); //Send IPv6 Packet
}

static void quit_timeout_callback(AVAHI_GCC_UNUSED AvahiTimeout *timeout, void *userdata) {
    AvahiSimplePoll *simple_poll = userdata;
    avahi_simple_poll_quit(simple_poll);
}

static void server_callback(AvahiServer *s,
                            AvahiServerState state,
                            AVAHI_GCC_UNUSED AVAHI_GCC_UNUSED void *userdata) {
    server = s;

    if (state == AVAHI_SERVER_RUNNING) {
        send_server_message(1);
        send_server_message(0);
    } else if (state == AVAHI_SERVER_COLLISION) {
        /*Handle Collision of Server Interface*/
        char *n;
        n = avahi_alternative_host_name(avahi_server_get_host_name(s));
        avahi_server_set_host_name(s, n);
        avahi_free(n);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    data = Data;
    size = Size;

    const AvahiPoll *poll_api;
    AvahiServerConfig config;
    AvahiSimplePoll *simple_poll;
    int error;

    simple_poll = avahi_simple_poll_new();
    poll_api = avahi_simple_poll_get(simple_poll);

    avahi_server_config_init(&config);
    config.disallow_other_stacks = 0;
    config.enable_reflector = 1;

    server = avahi_server_unreg_running(poll_api, &config, server_callback, NULL, &error);

    struct timeval tv;
    avahi_elapse_time(&tv, 5, 0);
    poll_api->timeout_new(poll_api, &tv, quit_timeout_callback, simple_poll);

    avahi_simple_poll_loop(simple_poll);

    avahi_server_config_free(&config);

    if (server)
        avahi_server_free(server);

    if (simple_poll)
        avahi_simple_poll_free(simple_poll);

    return 0;
}
