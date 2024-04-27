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
#include "avahi-core/rr-util.h"

void log_function(AvahiLogLevel level, const char *txt) {}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    AvahiDnsPacket *p = NULL;
    AvahiRecord *r = NULL, *rs = NULL, *c = NULL;
    uint8_t rdata[AVAHI_DNS_RDATA_MAX];
    size_t rdata_size;
    int ret;

    avahi_set_log_function(log_function);

    if (!(p = avahi_dns_packet_new(size + AVAHI_DNS_PACKET_EXTRA_SIZE)))
        goto finish;

    memcpy(AVAHI_DNS_PACKET_DATA(p), data, size);
    p->size = size;

    if (!(r = avahi_dns_packet_consume_record(p, NULL)))
        goto finish;

    ret = avahi_record_is_valid(r);
    assert(ret);

    avahi_record_is_goodbye(r);

    avahi_record_is_link_local_address(r);

    avahi_record_get_estimate_size(r);

    avahi_free(avahi_record_to_string(r));

    rdata_size = avahi_rdata_serialize(r, rdata, sizeof(rdata));
    assert(rdata_size != (size_t) -1);

    if (!(rs = avahi_record_new_full(r->key->name, r->key->clazz, r->key->type, r->ttl)))
        goto finish;

    if (avahi_rdata_parse(rs, rdata, rdata_size) < 0)
        goto finish;

    ret = avahi_record_is_valid(rs);
    assert(ret);

    ret = avahi_record_equal_no_ttl(r, rs);
    assert(ret);

    ret = avahi_record_lexicographical_compare(r, rs);
    assert(ret == 0);

    if (!(c = avahi_record_copy(r)))
        goto finish;

    ret = avahi_record_equal_no_ttl(r, c);
    assert(ret);

    ret = avahi_record_lexicographical_compare(r, c);
    assert(ret == 0);

    avahi_record_unref(c);
    if (!(c = avahi_dns_packet_consume_record(p, NULL)))
        goto finish;

    avahi_record_equal_no_ttl(r, c);
    avahi_record_lexicographical_compare(r, c);

    avahi_dns_packet_free(p);
    if (!(p = avahi_dns_packet_new(size + AVAHI_DNS_PACKET_EXTRA_SIZE)))
        goto finish;

    if (!avahi_dns_packet_append_record(p, r, 0, 0))
        goto finish;

finish:
    if (c)
        avahi_record_unref(c);
    if (rs)
        avahi_record_unref(rs);
    if (r)
        avahi_record_unref(r);
    if (p)
        avahi_dns_packet_free(p);

    return 0;
}
