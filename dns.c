#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dns.h"

flxDnsPacket* flx_dns_packet_new(void) {
    flxDnsPacket *p;
    p = g_new(flxDnsPacket, 1);
    p->size = p->rindex = 2*6;
    memset(p->data, 0, p->size);
    return p;
}

void flx_dns_packet_free(flxDnsPacket *p) {
    g_assert(p);
    g_free(p);
}

void flx_dns_packet_set_field(flxDnsPacket *p, guint index, guint16 v) {
    g_assert(p);
    g_assert(index < 2*6);
    
    ((guint16*) p->data)[index] = g_htons(v);
}

guint16 flx_dns_packet_get_field(flxDnsPacket *p, guint index) {
    g_assert(p);
    g_assert(index < 2*6);

    return g_ntohs(((guint16*) p->data)[index]);
}

guint8* flx_dns_packet_append_name(flxDnsPacket *p, const gchar *name) {
    guint8 *d, *f = NULL;
    
    g_assert(p);
    g_assert(name);

    for (;;) {
        guint n = strcspn(name, ".");
        if (!n || n > 63)
            return NULL;
        
        d = flx_dns_packet_extend(p, n+1);
        if (!f)
            f = d;
        d[0] = n;
        memcpy(d+1, name, n);

        name += n;

        /* no trailing dot */
        if (!*name)
            break;

        name ++;

        /* trailing dot */
        if (!*name)
            break;
    }

    d = flx_dns_packet_extend(p, 1);
    d[0] = 0;

    return f;
}

guint8* flx_dns_packet_append_uint16(flxDnsPacket *p, guint16 v) {
    guint8 *d;
    
    g_assert(p);
    
    d = flx_dns_packet_extend(p, sizeof(guint16));
    *((guint16*) d) = g_htons(v);
    
    return d;
}

guint8 *flx_dns_packet_extend(flxDnsPacket *p, guint l) {
    guint8 *d;
    
    g_assert(p);
    g_assert(p->size+l <= sizeof(p->data));

    d = p->data + p->size;
    p->size += l;
    
    return d;
}

guint8 *flx_dns_packet_append_name_compressed(flxDnsPacket *p, const gchar *name, guint8 *prev) {
    guint16 *d;
    signed long k;
    g_assert(p);

    if (!prev)
        return flx_dns_packet_append_name(p, name);
    
    k = prev - p->data;
    if (k < 0 || k >= 0x4000 || (guint) k >= p->size)
        return flx_dns_packet_append_name(p, name);

    d = (guint16*) flx_dns_packet_extend(p, sizeof(guint16));
    *d = g_htons((0xC000 | k));
    
    return prev;
}

gint flx_dns_packet_check_valid(flxDnsPacket *p) {
    guint16 flags;
    g_assert(p);

    if (p->size < 12)
        return -1;

    flags = flx_dns_packet_get_field(p, DNS_FIELD_FLAGS);

    if (flags & DNS_FLAG_OPCODE || flags & DNS_FLAG_RCODE)
        return -1;

    return 0;
}

gint flx_dns_packet_is_query(flxDnsPacket *p) {
    g_assert(p);
    
    return !(flx_dns_packet_get_field(p, DNS_FIELD_FLAGS) & DNS_FLAG_QR);
}

static gint consume_labels(flxDnsPacket *p, guint index, gchar *ret_name, guint l) {
    gint ret = 0;
    int compressed = 0;
    int first_label = 1;
    g_assert(p && ret_name && l);
    
    for (;;) {
        guint8 n;

        if (index+1 > p->size)
            return -1;

        n = p->data[index];

        if (!n) {
            index++;
            if (!compressed)
                ret++;

            if (l < 1)
                return -1;
            *ret_name = 0;
            
            return ret;
            
        } else if (n <= 63) {
            /* Uncompressed label */
            index++;
            if (!compressed)
                ret++;
        
            if (index + n > p->size)
                return -1;

            if ((guint) n + 1 > l)
                return -1;

            if (!first_label) {
                *(ret_name++) = '.';
                l--;
            } else
                first_label = 0;

            memcpy(ret_name, p->data + index, n);
            index += n;
            ret_name += n;
            l -= n;
            
            if (!compressed)
                ret += n;
        } else if ((n & 0xC0) == 0xC0) {
            /* Compressed label */

            if (index+2 > p->size)
                return -1;

            index = ((guint) (p->data[index] & ~0xC0)) << 8 | p->data[index+1];

            if (!compressed)
                ret += 2;
            
            compressed = 1;
        } else
            return -1;
    }
}

gint flx_dns_packet_consume_name(flxDnsPacket *p, gchar *ret_name, guint l) {
    gint r;
    
    if ((r = consume_labels(p, p->rindex, ret_name, l)) < 0)
        return -1;

    p->rindex += r;
    return 0;
}

gint flx_dns_packet_consume_uint16(flxDnsPacket *p, guint16 *ret_v) {
    g_assert(p);
    g_assert(ret_v);

    if (p->rindex + sizeof(guint16) > p->size)
        return -1;

    *ret_v = g_ntohs(*((guint16*) (p->data + p->rindex)));
    p->rindex += sizeof(guint16);

    return 0;
}

gint flx_dns_packet_consume_uint32(flxDnsPacket *p, guint32 *ret_v) {
    g_assert(p);
    g_assert(ret_v);

    if (p->rindex + sizeof(guint32) > p->size)
        return -1;

    *ret_v = g_ntohl(*((guint32*) (p->data + p->rindex)));
    p->rindex += sizeof(guint32);
    
    return 0;
}

gint flx_dns_packet_consume_bytes(flxDnsPacket *p, gpointer ret_data, guint l) {
    g_assert(p);
    g_assert(ret_data);
    g_assert(l > 0);
    
    if (p->rindex + l > p->size)
        return -1;

    memcpy(ret_data, p->data + p->rindex, l);
    p->rindex += l;

    return 0;
}

gconstpointer flx_dns_packet_get_rptr(flxDnsPacket *p) {
    g_assert(p);
    
    if (p->rindex >= p->size)
        return NULL;

    return p->data + p->rindex;
}

gint flx_dns_packet_skip(flxDnsPacket *p, guint length) {
    g_assert(p);

    if (p->rindex + length > p->size)
        return -1;

    p->rindex += length;
    return 0;
}

flxRecord* flx_dns_packet_consume_record(flxDnsPacket *p, gboolean *ret_cache_flush) {
    gchar name[256];
    guint16 type, class;
    guint32 ttl;
    guint16 rdlength;
    gconstpointer data;

    g_assert(p);
    g_assert(ret_cache_flush);

    if (flx_dns_packet_consume_name(p, name, sizeof(name)) < 0 ||
        flx_dns_packet_consume_uint16(p, &type) < 0 ||
        flx_dns_packet_consume_uint16(p, &class) < 0 ||
        flx_dns_packet_consume_uint32(p, &ttl) < 0 ||
        flx_dns_packet_consume_uint16(p, &rdlength) < 0 ||
        !(data = flx_dns_packet_get_rptr(p)) ||
        flx_dns_packet_skip(p, rdlength) < 0)
        return NULL;

    *ret_cache_flush = !!(class & MDNS_CACHE_FLUSH);
    class &= ~ MDNS_CACHE_FLUSH;

    return flx_record_new_full(name, class, type, data, rdlength, ttl);
}

flxKey* flx_dns_packet_consume_key(flxDnsPacket *p) {
    gchar name[256];
    guint16 type, class;

    g_assert(p);

    if (flx_dns_packet_consume_name(p, name, sizeof(name)) < 0 ||
        flx_dns_packet_consume_uint16(p, &type) < 0 ||
        flx_dns_packet_consume_uint16(p, &class) < 0)
        return NULL;

    class &= ~ MDNS_CACHE_FLUSH;

    return flx_key_new(name, class, type);
}

guint8* flx_dns_packet_append_key(flxDnsPacket *p, flxKey *k) {
    guint8 *t;
    
    g_assert(p);
    g_assert(k);

    if (!(t = flx_dns_packet_append_name(p, k->name)) ||
        !flx_dns_packet_append_uint16(p, k->type) ||
        !flx_dns_packet_append_uint16(p, k->class))
        return NULL;

    return t;
}

guint8* flx_dns_packet_append_record(flxDnsPacket *p, flxRecord *r, gboolean cache_flush) {

    
}
