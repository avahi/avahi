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
#include <stdio.h>
#include <string.h>

#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>

#include <avahi-common/timeval.h>
#include <avahi-core/core.h>
#include <avahi-core/log.h>
#include <avahi-core/lookup.h>
#include <avahi-core/cache.h>

static AvahiCache *cache = NULL;
static AvahiServer *server = NULL;
static const AvahiPoll *poll_api;

static void (*avahi_test_case_function)(void) = NULL;

static void quit_timeout_callback(AVAHI_GCC_UNUSED AvahiTimeout *timeout, void *userdata) {
    AvahiSimplePoll *simple_poll = userdata;

    avahi_simple_poll_quit(simple_poll);
}

#define avahi_test_cache_flush() avahi_cache_flush(cache)

static void avahi_test_add_a(const char *src, const char *dst, uint32_t ttl) {
    AvahiRecord *record;
    AvahiAddress src_addr;
    AvahiAddress answer_addr;

    avahi_address_parse("192.168.50.1", AVAHI_PROTO_UNSPEC, &src_addr);
    avahi_address_parse(dst, AVAHI_PROTO_UNSPEC, &answer_addr);

    record = avahi_record_new_full(src, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A, ttl);
    record->data.a.address = answer_addr.data.ipv4;

    avahi_cache_update(cache, record, 0, &src_addr);
    avahi_record_unref(record);

    avahi_log_debug("Added A record to cache: %s -> %s", src, dst);
}

static void avahi_test_add_cname(const char *src, const char *dst) {
    AvahiRecord *record;
    AvahiAddress src_addr;

    avahi_address_parse("192.168.50.1", AVAHI_PROTO_UNSPEC, &src_addr);
    record = avahi_record_new_full(src, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_CNAME, AVAHI_DEFAULT_TTL);
    record->data.cname.name = avahi_strdup(dst);

    avahi_cache_update(cache, record, 0, &src_addr);
    avahi_record_unref(record);

    avahi_log_debug("Added CNAME record to cache: %s -> %s", src, dst);
}

static void self_loop(void) {
    avahi_test_add_cname("X.local", "X.local");
}

static void retransmit_cname(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_cache_flush();
    avahi_test_add_cname("X.local", "Y.local");
}

static void one_normal(void) {
    avahi_test_add_cname("X.local", "Y.local");
}

static void one_loop(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_cname("Y.local", "X.local");
}

static void two_normal(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_cname("Y.local", "Z.local");
}

static void two_loop(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_cname("Y.local", "Z.local");
    avahi_test_add_cname("Z.local", "X.local");
}

static void two_loop_inner(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_cname("Y.local", "Z.local");
    avahi_test_add_cname("Z.local", "Y.local");
}

static void two_loop_inner2(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_cname("Y.local", "Z.local");
    avahi_test_add_cname("Y.local", "X.local");
}

static void three_normal(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_cname("Y.local", "Z.local");
    avahi_test_add_cname("Z.local", "A.local");
}

static void three_loop(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_cname("Y.local", "Z.local");
    avahi_test_add_cname("Z.local", "A.local");
    avahi_test_add_cname("A.local", "X.local");
}

static void diamond(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_cname("X.local", "Z.local");
    avahi_test_add_cname("Y.local", "A.local");
    avahi_test_add_cname("Z.local", "A.local");
}

static void cname_answer_diamond(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_cname("X.local", "Z.local");
    avahi_test_add_cname("Y.local", "A.local");
    avahi_test_add_cname("Z.local", "A.local");
    avahi_test_add_a("A.local", "192.168.50.99", 2);
}

static void cname_answer(void) {
    avahi_test_add_cname("X.local", "Y.local");
    avahi_test_add_a("Y.local", "192.168.50.99", 2);
}

static void server_callback(AvahiServer *s, AvahiServerState state, AVAHI_GCC_UNUSED void *userdata) {
    avahi_log_debug("server state: %i", state);

    if (state == AVAHI_SERVER_RUNNING) {
        avahi_log_debug("Server startup complete. Host name is <%s>. Service cookie is %u",
                        avahi_server_get_host_name_fqdn(s), avahi_server_get_local_service_cookie(s));

        server = s;
        assert(avahi_test_case_function);
        avahi_test_case_function();
        avahi_log_debug("Server configuration complete.");
        server = NULL;
    }
}

static const char *resolver_event_to_string(AvahiResolverEvent event) {
    switch (event) {
        case AVAHI_RESOLVER_FOUND:
            return "FOUND";
        case AVAHI_RESOLVER_FAILURE:
            return "FAILURE";
    }
    abort();
}

static void hnr_callback(
        AVAHI_GCC_UNUSED AvahiSHostNameResolver *r,
        AvahiIfIndex iface,
        AvahiProtocol protocol,
        AvahiResolverEvent event,
        const char *hostname,
        const AvahiAddress *a,
        AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
        AVAHI_GCC_UNUSED void *userdata) {
    char t[AVAHI_ADDRESS_STR_MAX];

    if (a)
        avahi_address_snprint(t, sizeof(t), a);

    avahi_log_debug("HNR: (%i.%i) <%s> -> %s [%s]", iface, protocol, hostname, a ? t : "n/a",
                    resolver_event_to_string(event));
}

#define CHECK_TEST_CASE(test_name, function) \
    if (!strcmp(test_case, test_name)) \
        avahi_test_case_function = &function;

static void avahi_test_initialize(char *test_case) {
    assert(test_case);

    CHECK_TEST_CASE("self_loop", self_loop);
    CHECK_TEST_CASE("retransmit_cname", retransmit_cname);
    CHECK_TEST_CASE("one_normal", one_normal);
    CHECK_TEST_CASE("one_loop", one_loop);
    CHECK_TEST_CASE("two_normal", two_normal);
    CHECK_TEST_CASE("two_loop", two_loop);
    CHECK_TEST_CASE("two_loop_inner", two_loop_inner);
    CHECK_TEST_CASE("two_loop_inner2", two_loop_inner2);
    CHECK_TEST_CASE("three_normal", three_normal);
    CHECK_TEST_CASE("three_loop", three_loop);
    CHECK_TEST_CASE("diamond", diamond);
    CHECK_TEST_CASE("cname_answer_diamond", cname_answer_diamond);
    CHECK_TEST_CASE("cname_answer", cname_answer);

    assert(avahi_test_case_function);
}

int main(int argc, char *argv[]) {
    int error;
    struct timeval tv;

    AvahiSHostNameResolver *hnr;
    AvahiServerConfig config;
    AvahiSimplePoll *simple_poll;

    if (argc < 2) {
        printf("Usage: %s test_name", argv[0]);
        return 1;
    }

    avahi_test_initialize(argv[1]);

    simple_poll = avahi_simple_poll_new();
    poll_api = avahi_simple_poll_get(simple_poll);

    avahi_server_config_init(&config);
    server = avahi_server_new(poll_api, &config, server_callback, NULL, &error);
    avahi_server_config_free(&config);

    cache = server->monitor->interfaces->cache;
    avahi_cache_flush(cache);

    hnr = avahi_s_host_name_resolver_prepare(server, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "X.local", AVAHI_PROTO_UNSPEC,
                                             AVAHI_LOOKUP_USE_MULTICAST, hnr_callback, NULL);
    avahi_s_host_name_resolver_start(hnr);

    avahi_elapse_time(&tv, 1000 * 5, 0);
    poll_api->timeout_new(poll_api, &tv, quit_timeout_callback, simple_poll);

    avahi_simple_poll_loop(simple_poll);

    avahi_s_host_name_resolver_free(hnr);

    avahi_log_info("Test complete!");
    if (server) {
        avahi_server_free(server);
    }

    if (simple_poll) {
        avahi_simple_poll_free(simple_poll);
    }

    return 0;
}
