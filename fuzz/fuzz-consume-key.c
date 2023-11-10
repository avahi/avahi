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
#include "avahi-core/domain-util.h"
#include "avahi-core/log.h"
#include "avahi-core/rr-util.h"

void log_function(AvahiLogLevel level, const char *txt) {}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    AvahiDnsPacket *p = NULL;
    AvahiKey *k1 = NULL, *k2 = NULL;
    int ret;

    avahi_set_log_function(log_function);

    if (!(p = avahi_dns_packet_new(size + AVAHI_DNS_PACKET_EXTRA_SIZE)))
        goto finish;

    memcpy(AVAHI_DNS_PACKET_DATA(p), data, size);
    p->size = size;

    if (!(k1 = avahi_dns_packet_consume_key(p, NULL)))
        goto finish;

    ret = avahi_key_is_valid(k1);
    assert(ret);

    avahi_key_hash(k1);

    avahi_free(avahi_key_to_string(k1));

    if ((k2 = avahi_key_new_cname(k1)))
        avahi_key_unref(k2);

    if (!(k2 = avahi_dns_packet_consume_key(p, NULL)))
        goto finish;

    ret = avahi_key_is_valid(k2);
    assert(ret);

    if (!avahi_key_is_pattern(k2))
        avahi_key_pattern_match(k1, k2);

finish:
    if (k2)
        avahi_key_unref(k2);
    if (k1)
        avahi_key_unref(k1);
    if (p)
        avahi_dns_packet_free(p);

    return 0;
}
