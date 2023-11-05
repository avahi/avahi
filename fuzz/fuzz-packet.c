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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "avahi-common/malloc.h"
#include "avahi-core/dns.h"
#include "avahi-core/log.h"

void log_function(AvahiLogLevel level, const char *txt) {}

bool copy_rrs(AvahiDnsPacket *from, AvahiDnsPacket *to, unsigned idx) {
    for (uint16_t n = avahi_dns_packet_get_field(from, idx); n > 0; n--) {
        AvahiRecord *record;
        int cache_flush = 0;
        uint8_t *res;

        if (!(record = avahi_dns_packet_consume_record(from, &cache_flush)))
            return false;

        res = avahi_dns_packet_append_record(to, record, cache_flush, 0);
        avahi_record_unref(record);
        if (!res)
            return false;
        avahi_dns_packet_inc_field(to, idx);
    }
    return true;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    AvahiDnsPacket *p1 = NULL, *p2 = NULL;

    if (size > AVAHI_DNS_PACKET_SIZE_MAX)
        return 0;

    avahi_set_log_function(log_function);

    if (!(p1 = avahi_dns_packet_new(size + AVAHI_DNS_PACKET_EXTRA_SIZE)))
        goto finish;

    memcpy(AVAHI_DNS_PACKET_DATA(p1), data, size);
    p1->size = size;

    if (avahi_dns_packet_check_valid(p1) < 0)
        goto finish;

    if (!(p2 = avahi_dns_packet_new(size + AVAHI_DNS_PACKET_EXTRA_SIZE)))
        goto finish;

    avahi_dns_packet_set_field(p2, AVAHI_DNS_FIELD_ID, avahi_dns_packet_get_field(p1, AVAHI_DNS_FIELD_ID));

    for (uint16_t n = avahi_dns_packet_get_field(p1, AVAHI_DNS_FIELD_QDCOUNT); n > 0; n--) {
        AvahiKey *key;
        int unicast_response = 0;
        uint8_t *res;

        if (!(key = avahi_dns_packet_consume_key(p1, &unicast_response)))
            goto finish;

        res = avahi_dns_packet_append_key(p2, key, unicast_response);
        avahi_key_unref(key);
        if (!res)
            goto finish;
        avahi_dns_packet_inc_field(p2, AVAHI_DNS_FIELD_QDCOUNT);
    }

    if (!copy_rrs(p1, p2, AVAHI_DNS_FIELD_ANCOUNT))
        goto finish;

    if (!copy_rrs(p1, p2, AVAHI_DNS_FIELD_NSCOUNT))
        goto finish;

    if (!copy_rrs(p1, p2, AVAHI_DNS_FIELD_ARCOUNT))
        goto finish;

finish:
    if (p2)
        avahi_dns_packet_free(p2);
    if (p1)
        avahi_dns_packet_free(p1);

    return 0;
}
