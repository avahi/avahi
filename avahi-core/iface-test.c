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
#include <string.h>

#include <avahi-common/address.h>

#include "iface.h"

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

    return 0;
}
