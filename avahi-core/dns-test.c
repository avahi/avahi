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
 * advertising A (type 1) and AAAA (type 28), as used in RFC 6762). The
 * generic representation exposes no NSEC fields to set, so the record is
 * built from its wire rdata. */
static AvahiRecord *make_nsec(const char *owner, const char *next_domain_name) {
    static const uint8_t bitmap[] = { 0x00, 0x04, 0x40, 0x00, 0x00, 0x08 };
    uint8_t rdata[AVAHI_DOMAIN_NAME_MAX + sizeof(bitmap)];
    AvahiDnsPacket np;
    size_t n;
    AvahiRecord *r;

    np.data = rdata;
    np.max_size = sizeof(rdata);
    np.size = np.rindex = 0;
    np.name_table = NULL;

    avahi_dns_packet_append_name(&np, next_domain_name);
    if (np.name_table)
        avahi_hashmap_free(np.name_table);

    n = np.size;
    memcpy(rdata + n, bitmap, sizeof(bitmap));
    n += sizeof(bitmap);

    r = avahi_record_new_full(owner, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_NSEC, AVAHI_DEFAULT_TTL);
    assert(avahi_rdata_parse(r, rdata, n) >= 0);

    return r;
}

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[]) {
    char t[AVAHI_DOMAIN_NAME_MAX], *m;
    const char *a, *b, *c, *d;
    AvahiDnsPacket *p;
    AvahiRecord *r, *r2;
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

    /* NSEC: empty type bitmap. The record is just the Next Domain Name; it
     * must round-trip and compare as smaller than a non-empty NSEC. */
    {
        uint8_t rd[64];
        AvahiDnsPacket np;
        AvahiRecord *nr, *nr2, *nonempty;

        np.data = rd;
        np.max_size = sizeof(rd);
        np.size = np.rindex = 0;
        np.name_table = NULL;
        avahi_dns_packet_append_name(&np, "empty.local");
        if (np.name_table)
            avahi_hashmap_free(np.name_table);

        nr = avahi_record_new_full("empty.local", AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_NSEC, AVAHI_DEFAULT_TTL);
        assert(avahi_rdata_parse(nr, rd, np.size) >= 0);

        l = avahi_rdata_serialize(nr, rdata, sizeof(rdata));
        assert(l == np.size && memcmp(rdata, rd, np.size) == 0);

        nr2 = avahi_record_copy(nr);
        assert(avahi_record_equal_no_ttl(nr, nr2));
        assert(avahi_record_lexicographical_compare(nr, nr2) == 0);

        nonempty = make_nsec("empty.local", "empty.local");
        assert(avahi_record_lexicographical_compare(nr, nonempty) < 0);
        assert(avahi_record_lexicographical_compare(nonempty, nr) > 0);

        avahi_record_unref(nr);
        avahi_record_unref(nr2);
        avahi_record_unref(nonempty);
    }

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
        resp = avahi_dns_packet_append_record(p1, nsec, 0, 0);
        assert(resp);

        parsed = avahi_dns_packet_consume_record(p1, NULL);
        assert(parsed);
        assert(avahi_record_equal_no_ttl(nsec, parsed));

        /* Packet 2: prepend an unrelated record so "host.local" lands at a
         * different offset, then reflect the parsed NSEC into it. With the
         * old code the frozen pointer would now decode to garbage. */
        p2 = avahi_dns_packet_new(0);

        prefix = avahi_record_new_full("a.much.longer.unrelated.name.local", AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR, AVAHI_DEFAULT_TTL);
        prefix->data.ptr.name = avahi_strdup("target.local");

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

        avahi_record_unref(parsed);
        avahi_record_unref(prefix);
        avahi_record_unref(nsec);
        avahi_dns_packet_free(p1);
        avahi_dns_packet_free(p2);
    }

    /* NSEC: a record whose Next Domain Name arrives compressed but whose
     * bitmap is so large that the name + bitmap would overflow the 16-bit
     * rdata limit once the name is written back uncompressed must be
     * rejected at parse time. On the wire the name is a 2-byte pointer, so
     * the record fits in rdlength; only after decompression does it
     * overflow, and a record we accept must always be re-serializable. */
    {
        static const uint8_t owner[] = { 0x04,'h','o','s','t', 0x05,'l','o','c','a','l', 0x00 };
        size_t bitmap_size = AVAHI_DNS_RDATA_MAX - 5;
        size_t rdlen = 2 + bitmap_size;
        size_t total = AVAHI_DNS_PACKET_HEADER_SIZE + sizeof(owner) + 10 + rdlen;
        uint8_t *pkt;
        size_t off;
        AvahiDnsPacket *p1;
        AvahiRecord *parsed;

        pkt = avahi_malloc0(total);

        off = AVAHI_DNS_PACKET_HEADER_SIZE;
        memcpy(pkt + off, owner, sizeof(owner)); off += sizeof(owner);
        pkt[off++] = 0x00; pkt[off++] = 0x2f;                 /* type NSEC */
        pkt[off++] = 0x00; pkt[off++] = 0x01;                 /* class IN */
        pkt[off++] = 0; pkt[off++] = 0; pkt[off++] = 0x11; pkt[off++] = 0x94; /* ttl */
        pkt[off++] = (uint8_t) (rdlen >> 8); pkt[off++] = (uint8_t) rdlen;    /* rdlength */
        pkt[off++] = 0xc0; pkt[off++] = 0x0c;                 /* Next Domain Name -> owner @12 */
        /* remaining bitmap_size bytes stay zero */

        p1 = avahi_dns_packet_new(0);
        memcpy(AVAHI_DNS_PACKET_DATA(p1), pkt, total);
        p1->size = total;

        parsed = avahi_dns_packet_consume_record(p1, NULL);
        assert(!parsed);

        avahi_dns_packet_free(p1);
        avahi_free(pkt);
    }

    /* NSEC: a Next Domain Name we can parse but cannot re-escape -- here a
     * label holding a NUL byte, which avahi_unescape_label() rejects -- must
     * be refused at parse time. The name lives in the generic blob, so it is
     * not covered by avahi_record_is_valid() the way PTR/SRV names are;
     * without an explicit check the record would parse yet fail to
     * re-serialize when reflected. */
    {
        static const uint8_t owner[] = { 0x04,'h','o','s','t', 0x05,'l','o','c','a','l', 0x00 };
        /* Next Domain Name = one label holding a single 0x00 byte (01 00 00),
         * followed by a small type bitmap. */
        static const uint8_t rd[] = { 0x01, 0x00, 0x00, 0x00, 0x04, 0x40, 0x00, 0x00, 0x08 };
        size_t total = AVAHI_DNS_PACKET_HEADER_SIZE + sizeof(owner) + 10 + sizeof(rd);
        uint8_t *pkt;
        size_t off;
        AvahiDnsPacket *p1;
        AvahiRecord *parsed;

        pkt = avahi_malloc0(total);

        off = AVAHI_DNS_PACKET_HEADER_SIZE;
        memcpy(pkt + off, owner, sizeof(owner)); off += sizeof(owner);
        pkt[off++] = 0x00; pkt[off++] = 0x2f;                 /* type NSEC */
        pkt[off++] = 0x00; pkt[off++] = 0x01;                 /* class IN */
        pkt[off++] = 0; pkt[off++] = 0; pkt[off++] = 0x11; pkt[off++] = 0x94; /* ttl */
        pkt[off++] = 0x00; pkt[off++] = (uint8_t) sizeof(rd); /* rdlength */
        memcpy(pkt + off, rd, sizeof(rd)); off += sizeof(rd);

        p1 = avahi_dns_packet_new(0);
        memcpy(AVAHI_DNS_PACKET_DATA(p1), pkt, total);
        p1->size = total;

        parsed = avahi_dns_packet_consume_record(p1, NULL);
        assert(!parsed);

        avahi_dns_packet_free(p1);
        avahi_free(pkt);
    }

    return 0;
}
