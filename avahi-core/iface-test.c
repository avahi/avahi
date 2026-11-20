/***
  This file is part of avahi.

  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <avahi-common/address.h>
#include <avahi-common/malloc.h>
#include <avahi-common/strlst.h>

#include "iface.h"
#include "internal.h"
#include "log.h"

static int address_on_link(const char *interface_address, unsigned prefix_len, const char *address) {
    AvahiInterface interface;
    AvahiInterfaceAddress interface_address_entry;
    AvahiAddress candidate;

    memset(&interface, 0, sizeof(interface));
    memset(&interface_address_entry, 0, sizeof(interface_address_entry));

    interface.protocol = AVAHI_PROTO_INET;
    interface.addresses = &interface_address_entry;

    interface_address_entry.interface = &interface;
    interface_address_entry.prefix_len = prefix_len;

    avahi_address_parse(interface_address, AVAHI_PROTO_INET, &interface_address_entry.address);
    avahi_address_parse(address, AVAHI_PROTO_INET, &candidate);

    return avahi_interface_address_on_link(&interface, &candidate);
}

static char last_log[1024];
static unsigned n_logs;

static void capture_log(AVAHI_GCC_UNUSED AvahiLogLevel level, const char *txt) {
    snprintf(last_log, sizeof(last_log), "%s", txt);
    n_logs++;
}

static int ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);

    return a >= b && strcmp(s + a - b, suffix) == 0;
}

/* Runs avahi_interface_monitor_log_excluded() over hardware interfaces
 * with the given names. Returns the number of lines logged; the last
 * one is in last_log. */
static unsigned log_excluded(const char *allow, const char *deny, const char * const *names, unsigned n_names) {
    AvahiServer *server;
    AvahiInterfaceMonitor monitor;
    AvahiHwInterface *hw;
    unsigned i;

    /* AvahiServer is too large for the stack under valgrind. */
    server = avahi_new0(AvahiServer, 1);
    assert(server);
    memset(&monitor, 0, sizeof(monitor));
    monitor.server = server;

    if (allow)
        server->config.allow_interfaces = avahi_string_list_new(allow, NULL);
    if (deny)
        server->config.deny_interfaces = avahi_string_list_new(deny, NULL);

    hw = avahi_new0(AvahiHwInterface, n_names);
    assert(hw);
    for (i = 0; i < n_names; i++) {
        hw[i].name = (char*) names[i];
        hw[i].hardware_next = i + 1 < n_names ? &hw[i + 1] : NULL;
    }
    monitor.hw_interfaces = n_names ? hw : NULL;

    last_log[0] = 0;
    n_logs = 0;
    avahi_interface_monitor_log_excluded(&monitor);

    avahi_free(hw);
    avahi_string_list_free(server->config.allow_interfaces);
    avahi_string_list_free(server->config.deny_interfaces);
    avahi_free(server);

    return n_logs;
}

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[]) {
    int r;

    r = address_on_link("192.0.2.1", 0, "203.0.113.1");
    assert(r == 1);

    r = address_on_link("192.0.2.1", 24, "192.0.2.254");
    assert(r == 1);

    r = address_on_link("192.0.2.1", 24, "192.0.3.1");
    assert(r == 0);

    r = address_on_link("192.0.2.10", 31, "192.0.2.11");
    assert(r == 1);

    r = address_on_link("192.0.2.10", 31, "192.0.2.12");
    assert(r == 0);

    r = address_on_link("192.0.2.1", 32, "192.0.2.1");
    assert(r == 1);

    r = address_on_link("192.0.2.1", 32, "192.0.2.2");
    assert(r == 0);

    r = address_on_link("192.0.2.1", 33, "192.0.2.1");
    assert(r == 0);

    assert(avahi_interface_name_match("eth0", "eth0"));
    assert(avahi_interface_name_match("ETH0", "eth0"));
    assert(avahi_interface_name_match("eth0", "ETH0"));
    assert(!avahi_interface_name_match("eth0", "eth1"));
    assert(!avahi_interface_name_match("eth", "eth0"));
    assert(avahi_interface_name_match("veth*", "veth1a2b3c"));
    assert(avahi_interface_name_match("VETH*", "veth1a2b3c"));
    assert(!avahi_interface_name_match("veth*", "eth0"));
    assert(avahi_interface_name_match("br-*", "br-78ca670ae6f2"));
    assert(avahi_interface_name_match("eth?", "eth1"));
    assert(!avahi_interface_name_match("eth?", "eth10"));
    assert(avahi_interface_name_match("wlan[0-3]", "wlan2"));
    assert(!avahi_interface_name_match("wlan[0-3]", "wlan4"));
    assert(avahi_interface_name_match("*", "enp5s0"));
    assert(avahi_interface_name_match("tap\\[1\\]", "tap[1]"));
    assert(!avahi_interface_name_match("tap\\[1\\]", "tap1"));

    {
        /* A pattern too long to lower-case on the stack still matches
         * as a glob, not as a literal string. */
        char long_pattern[400];

        memset(long_pattern, '*', sizeof(long_pattern) - 1);
        long_pattern[sizeof(long_pattern) - 1] = 0;
        assert(avahi_interface_name_match(long_pattern, "veth1a2b3c"));

        memcpy(long_pattern + sizeof(long_pattern) - 6, "vEtH*", 6);
        assert(avahi_interface_name_match(long_pattern, "xvEtH0"));
    }

    avahi_set_log_function(capture_log);

    {
        const char * const names[] = { "enp5s0", "docker0", "veth1a2b3c", "br-78ca670ae6f2" };
        const char *prefix = "Ignoring 200 interfaces excluded by allow-interfaces/deny-interfaces: veth0, veth1, ";
        char *many[200];
        unsigned i;

        /* No allow/deny lists: nothing to report. */
        assert(log_excluded(NULL, NULL, names, 4) == 0);

        /* Nothing excluded: nothing to report. */
        assert(log_excluded(NULL, "wlan*", names, 4) == 0);

        assert(log_excluded(NULL, "veth*", names, 4) == 1);
        assert(strcmp(last_log, "Ignoring 1 interface excluded by allow-interfaces/deny-interfaces: veth1a2b3c.") == 0);

        assert(log_excluded("enp5s0", NULL, names, 4) == 1);
        assert(strcmp(last_log, "Ignoring 3 interfaces excluded by allow-interfaces/deny-interfaces: docker0, veth1a2b3c, br-78ca670ae6f2.") == 0);

        /* Too many names for one line: the rest are counted. */
        for (i = 0; i < 200; i++)
            many[i] = avahi_strdup_printf("veth%u", i);

        assert(log_excluded(NULL, "veth*", (const char * const *) many, 200) == 1);
        assert(strncmp(last_log, prefix, strlen(prefix)) == 0);
        /* The tail must survive the 255-byte cut in avahi_log_ap(). */
        assert(ends_with(last_log, " more)."));

        for (i = 0; i < 200; i++)
            avahi_free(many[i]);
    }

    avahi_set_log_function(NULL);

    return 0;
}
