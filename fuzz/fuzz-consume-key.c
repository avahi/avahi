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

#include <stdint.h>
#include <string.h>

#include "avahi-common/malloc.h"
#include "avahi-core/dns.h"
#include "avahi-core/log.h"

void log_function(AvahiLogLevel level, const char *txt) {}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    avahi_set_log_function(log_function);
    AvahiDnsPacket* packet = avahi_dns_packet_new(size + AVAHI_DNS_PACKET_EXTRA_SIZE);

    if (size + AVAHI_DNS_PACKET_EXTRA_SIZE > AVAHI_DNS_PACKET_SIZE_MAX)
        size = packet->max_size - AVAHI_DNS_PACKET_EXTRA_SIZE;

    memcpy(AVAHI_DNS_PACKET_DATA(packet), data, size);
    packet->size = size;
    AvahiKey* key = avahi_dns_packet_consume_key(packet, NULL);
    if (key) {
        avahi_key_is_valid(key);
        char *s = avahi_key_to_string(key);
        avahi_free(s);
        avahi_key_unref(key);
    }
    avahi_dns_packet_free(packet);

    return 0;
}
