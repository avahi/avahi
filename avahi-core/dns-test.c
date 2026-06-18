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

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <avahi-common/domain.h>
#include <avahi-common/defs.h>
#include <avahi-common/malloc.h>

#include "dns.h"
#include "log.h"
#include "rr-util.h"
#include "util.h"

/* Build an NSEC record with a realistic mDNS type bitmap (window block 0
 * advertising A (type 1) and AAAA (type 28), as used in RFC 6762). */
static AvahiRecord *make_nsec(const char *owner, const char *next_domain_name) {
    static const uint8_t bitmap[] = { 0x00, 0x04, 0x40, 0x00, 0x00, 0x08 };
    AvahiRecord *r;

    r = avahi_record_new_full(owner, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_NSEC, AVAHI_DEFAULT_TTL);
    assert(r);

    r->data.nsec.next_domain_name = avahi_strdup(next_domain_name);
    assert(r->data.nsec.next_domain_name);
    r->data.nsec.type_bitmap = avahi_memdup(bitmap, sizeof(bitmap));
    assert(r->data.nsec.type_bitmap);
    r->data.nsec.type_bitmap_size = sizeof(bitmap);

    return r;
}

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[]) {
    char t[AVAHI_DOMAIN_NAME_MAX], *m;
    const char *a, *b, *c, *d;
    AvahiDnsPacket *p;
    AvahiRecord *r, *r2, *r3;
    uint8_t rdata[AVAHI_DNS_RDATA_MAX];
    size_t l;
    int res;
    uint8_t *resp;

    p = avahi_dns_packet_new(0);

    resp = avahi_dns_packet_append_name(p, a = "Ahello.hello.hello.de.");
    assert(resp);
    resp = avahi_dns_packet_append_name(p, b = "Bthis is a test.hello.de.");
    assert(resp);
    resp = avahi_dns_packet_append_name(p, c = "Cthis\\.is\\.a\\.test\\.with\\.dots.hello.de.");
    assert(resp);
    resp = avahi_dns_packet_append_name(p, d = "Dthis\\\\is another test.hello.de.");
    assert(resp);

    avahi_hexdump(AVAHI_DNS_PACKET_DATA(p), p->size);

    res = avahi_dns_packet_consume_name(p, t, sizeof(t));
    assert(res == 0);
    avahi_log_debug(">%s<", t);
    assert(avahi_domain_equal(a, t));

    res = avahi_dns_packet_consume_name(p, t, sizeof(t));
    assert(res == 0);
    avahi_log_debug(">%s<", t);
    assert(avahi_domain_equal(b, t));

    res = avahi_dns_packet_consume_name(p, t, sizeof(t));
    assert(res == 0);
    avahi_log_debug(">%s<", t);
    assert(avahi_domain_equal(c, t));

    res = avahi_dns_packet_consume_name(p, t, sizeof(t));
    assert(res == 0);
    avahi_log_debug(">%s<", t);
    assert(avahi_domain_equal(d, t));

    avahi_dns_packet_free(p);

    /* RDATA PARSING AND SERIALIZATION */

    /* Create an AvahiRecord with some usful data */
    r = avahi_record_new_full("foobar.local", AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_HINFO, AVAHI_DEFAULT_TTL);
    assert(r);
    r->data.hinfo.cpu = avahi_strdup("FOO");
    r->data.hinfo.os = avahi_strdup("BAR");

    /* Serialize it into a blob */
    l = avahi_rdata_serialize(r, rdata, sizeof(rdata));
    assert(l != (size_t) -1);

    /* Print it */
    avahi_hexdump(rdata, l);

    /* Create a new record and fill in the data from the blob */
    r2 = avahi_record_new(r->key, AVAHI_DEFAULT_TTL);
    assert(r2);
    res = avahi_rdata_parse(r2, rdata, l);
    assert(res >= 0);

    /* Compare both versions */
    assert(avahi_record_equal_no_ttl(r, r2));

    /* Free the records */
    avahi_record_unref(r);
    avahi_record_unref(r2);

    r = avahi_record_new_full("foobar", 77, 77, AVAHI_DEFAULT_TTL);
    assert(r);

    r->data.generic.data = avahi_memdup("HALLO", r->data.generic.size = 5);
    assert(r->data.generic.data);

    m = avahi_record_to_string(r);
    assert(m);

    avahi_log_debug(">%s<", m);

    avahi_free(m);
    avahi_record_unref(r);

    r = avahi_record_new_full("test", 77, 77, AVAHI_DEFAULT_TTL);
    assert(r);

    r2 = avahi_record_new_full("test", 77, 77, AVAHI_DEFAULT_TTL);
    assert(r2);

    // "" == ""
    res = avahi_record_lexicographical_compare(r, r2);
    assert(res == 0);

    r->data.generic.data = avahi_memdup("WAT", r->data.generic.size = 3);
    assert(r->data.generic.data);

    // "WAT" > ""
    res = avahi_record_lexicographical_compare(r, r2);
    assert(res == 1);

    // "" < "WAT"
    res = avahi_record_lexicographical_compare(r2, r);
    assert(res == -1);

    r2->data.generic.data = avahi_memdup("WA", r2->data.generic.size = 2);
    assert(r2->data.generic.data);

    // "WAT" > "WA"
    res = avahi_record_lexicographical_compare(r, r2);
    assert(res == 1);

    // "WA" < "WAT"
    res = avahi_record_lexicographical_compare(r2, r);
    assert(res == -1);

    avahi_record_unref(r);

    r = avahi_record_new_full("test", 77, 77, AVAHI_DEFAULT_TTL);
    assert(r);

    r->data.generic.data = avahi_memdup("WA", r->data.generic.size = 2);
    assert(r->data.generic.data);

    // "WA" == "WA"
    res = avahi_record_lexicographical_compare(r, r2);
    assert(res == 0);

    avahi_record_unref(r);
    avahi_record_unref(r2);

    /* rdata larger than the 16 bit rdlength field must be rejected, not
     * truncated mod 65536 and parsed as a shorter record */
    r = avahi_record_new_full("test", 77, 77, AVAHI_DEFAULT_TTL);
    assert(r);

    m = avahi_malloc0(AVAHI_DNS_RDATA_MAX + 1);
    assert(m);

    res = avahi_rdata_parse(r, m, AVAHI_DNS_RDATA_MAX + 1);
    assert(res < 0);

    res = avahi_rdata_parse(r, m, AVAHI_DNS_RDATA_MAX);
    assert(res >= 0);

    avahi_free(m);
    avahi_record_unref(r);

    /* NSEC: isolated rdata round-trip (Next Domain Name + type bitmap) */
    r = make_nsec("host.local", "host.local");

    l = avahi_rdata_serialize(r, rdata, sizeof(rdata));
    assert(l != (size_t) -1);

    avahi_hexdump(rdata, l);

    r2 = avahi_record_new(r->key, AVAHI_DEFAULT_TTL);
    assert(r2);
    res = avahi_rdata_parse(r2, rdata, l);
    assert(res >= 0);

    assert(avahi_record_equal_no_ttl(r, r2));
    assert(avahi_record_lexicographical_compare(r, r2) == 0);

    m = avahi_record_to_string(r);
    assert(m);
    avahi_log_debug(">%s<", m);
    avahi_free(m);

    avahi_record_unref(r);
    avahi_record_unref(r2);

    /* NSEC: empty type bitmap. type_bitmap is NULL with size 0, which must
     * be handled by equal/copy/compare without dereferencing the NULL. */
    r = avahi_record_new_full("empty.local", AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_NSEC, AVAHI_DEFAULT_TTL);
    assert(r);
    r->data.nsec.next_domain_name = avahi_strdup("empty.local");
    assert(r->data.nsec.next_domain_name);
    r->data.nsec.type_bitmap = NULL;
    r->data.nsec.type_bitmap_size = 0;

    l = avahi_rdata_serialize(r, rdata, sizeof(rdata));
    assert(l != (size_t) -1);

    r2 = avahi_record_new(r->key, AVAHI_DEFAULT_TTL);
    assert(r2);
    res = avahi_rdata_parse(r2, rdata, l);
    assert(res >= 0);

    assert(avahi_record_equal_no_ttl(r, r2));
    assert(avahi_record_lexicographical_compare(r, r2) == 0); /* must not deref NULL bitmap */

    r3 = avahi_record_copy(r);
    assert(r3);
    assert(avahi_record_equal_no_ttl(r, r3));
    assert(avahi_record_lexicographical_compare(r, r3) == 0);

    /* An empty bitmap must compare as smaller than a non-empty one. */
    {
        AvahiRecord *nonempty = make_nsec("empty.local", "empty.local");
        assert(avahi_record_lexicographical_compare(r, nonempty) < 0);
        assert(avahi_record_lexicographical_compare(nonempty, r) > 0);
        avahi_record_unref(nonempty);
    }

    avahi_record_unref(r);
    avahi_record_unref(r2);
    avahi_record_unref(r3);

    /* NSEC: compression regression test.
     *
     * In mDNS the NSEC Next Domain Name is usually the record's own owner
     * name and is therefore name-compressed on the wire (RFC 6762 18.14).
     * Avahi used to treat NSEC as opaque rdata and froze the original
     * packet's compression pointer into the blob, then re-emitted those
     * exact bytes when reflecting the record into a new packet -- where the
     * same offset points at unrelated data. This test reflects an NSEC
     * record through two packets with DIFFERENT layouts and checks the Next
     * Domain Name survives intact. */
    {
        AvahiRecord *nsec, *prefix, *parsed, *skip;
        AvahiDnsPacket *p1, *p2;

        nsec = make_nsec("host.local", "host.local");

        /* Packet 1: the owner name is appended first, so append_record
         * compresses the Next Domain Name down to a pointer back to it. */
        p1 = avahi_dns_packet_new(0);
        assert(p1);
        resp = avahi_dns_packet_append_record(p1, nsec, 0, 0);
        assert(resp);

        parsed = avahi_dns_packet_consume_record(p1, NULL);
        assert(parsed);
        assert(avahi_record_equal_no_ttl(nsec, parsed));

        /* Packet 2: prepend an unrelated record so "host.local" lands at a
         * different offset, then reflect the parsed NSEC into it. With the
         * old code the frozen pointer would now decode to garbage. */
        p2 = avahi_dns_packet_new(0);
        assert(p2);

        prefix = avahi_record_new_full("a.much.longer.unrelated.name.local", AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR, AVAHI_DEFAULT_TTL);
        assert(prefix);
        prefix->data.ptr.name = avahi_strdup("target.local");
        assert(prefix->data.ptr.name);

        resp = avahi_dns_packet_append_record(p2, prefix, 0, 0);
        assert(resp);
        resp = avahi_dns_packet_append_record(p2, parsed, 0, 0);
        assert(resp);

        avahi_record_unref(parsed);

        skip = avahi_dns_packet_consume_record(p2, NULL);
        assert(skip);
        avahi_record_unref(skip);

        parsed = avahi_dns_packet_consume_record(p2, NULL);
        assert(parsed);
        assert(avahi_record_equal_no_ttl(nsec, parsed));
        assert(avahi_domain_equal(parsed->data.nsec.next_domain_name, "host.local"));

        avahi_record_unref(parsed);
        avahi_record_unref(prefix);
        avahi_record_unref(nsec);
        avahi_dns_packet_free(p1);
        avahi_dns_packet_free(p2);
    }

    /* NSEC: a record whose Next Domain Name arrives compressed but whose
     * bitmap is so large that the name + bitmap would overflow the 16-bit
     * rdata limit once the name is written back uncompressed must be
     * rejected at parse time. Otherwise avahi_rdata_serialize() would fail
     * on a record we had already accepted, and the record could not be
     * reflected. On the wire the name is a 2-byte pointer, so the record
     * fits in rdlength; only after decompression does it overflow. */
    {
        AvahiRecord *nsec, *parsed;
        AvahiDnsPacket *p1;
        size_t huge;

        /* "host.local" is 12 bytes uncompressed on the wire; pick a bitmap
         * that keeps the compressed record within rdlength (2 + bitmap <=
         * 0xFFFF) but pushes the decompressed form (12 + bitmap) over it. */
        huge = AVAHI_DNS_RDATA_MAX - 5;

        nsec = avahi_record_new_full("host.local", AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_NSEC, AVAHI_DEFAULT_TTL);
        assert(nsec);
        nsec->data.nsec.next_domain_name = avahi_strdup("host.local");
        assert(nsec->data.nsec.next_domain_name);
        nsec->data.nsec.type_bitmap = avahi_malloc0(huge);
        assert(nsec->data.nsec.type_bitmap);
        nsec->data.nsec.type_bitmap_size = (uint16_t) huge;

        p1 = avahi_dns_packet_new(0);
        assert(p1);

        /* append_record compresses the Next Domain Name to a 2-byte pointer
         * back to the owner name, so the record fits on the wire. */
        resp = avahi_dns_packet_append_record(p1, nsec, 0, 0);
        assert(resp);

        /* Consuming it must fail: the decompressed record cannot be
         * re-serialized within the rdata limit. */
        parsed = avahi_dns_packet_consume_record(p1, NULL);
        assert(!parsed);

        avahi_dns_packet_free(p1);
        avahi_record_unref(nsec);
    }

    return 0;
}
