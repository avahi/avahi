#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dns.h"

flxDnsPacket* flx_dns_packet_new(guint max_size) {
    flxDnsPacket *p;

    if (max_size <= 0)
        max_size = FLX_DNS_PACKET_MAX_SIZE;
    else if (max_size < FLX_DNS_PACKET_HEADER_SIZE)
        max_size = FLX_DNS_PACKET_HEADER_SIZE;
    
    p = g_malloc(sizeof(flxDnsPacket) + max_size);
    p->size = p->rindex = FLX_DNS_PACKET_HEADER_SIZE;
    p->max_size = max_size;

    memset(FLX_DNS_PACKET_DATA(p), 0, p->size);
    return p;
}

flxDnsPacket* flx_dns_packet_new_query(guint max_size) {
    flxDnsPacket *p;

    p = flx_dns_packet_new(max_size);
    flx_dns_packet_set_field(p, DNS_FIELD_FLAGS, DNS_FLAGS(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    return p;
}

flxDnsPacket* flx_dns_packet_new_response(guint max_size) {
    flxDnsPacket *p;

    p = flx_dns_packet_new(max_size);
    flx_dns_packet_set_field(p, DNS_FIELD_FLAGS, DNS_FLAGS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    return p;
}

void flx_dns_packet_free(flxDnsPacket *p) {
    g_assert(p);
    g_free(p);
}

void flx_dns_packet_set_field(flxDnsPacket *p, guint index, guint16 v) {
    g_assert(p);
    g_assert(index < FLX_DNS_PACKET_HEADER_SIZE);
    
    ((guint16*) FLX_DNS_PACKET_DATA(p))[index] = g_htons(v);
}

guint16 flx_dns_packet_get_field(flxDnsPacket *p, guint index) {
    g_assert(p);
    g_assert(index < FLX_DNS_PACKET_HEADER_SIZE);

    return g_ntohs(((guint16*) FLX_DNS_PACKET_DATA(p))[index]);
}

guint8* flx_dns_packet_append_name(flxDnsPacket *p, const gchar *name) {
    guint8 *d, *f = NULL;
    guint saved_size;
    
    g_assert(p);
    g_assert(name);

    saved_size = p->size;

    for (;;) {
        guint n = strcspn(name, ".");
        if (!n || n > 63)
            goto fail;
        
        if (!(d = flx_dns_packet_extend(p, n+1)))
            goto fail;
            
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

    if (!(d = flx_dns_packet_extend(p, 1)))
        goto fail;
    
    d[0] = 0;

    return f;

fail:
    p->size = saved_size;
    return NULL;
}

guint8* flx_dns_packet_append_uint16(flxDnsPacket *p, guint16 v) {
    guint8 *d;
    g_assert(p);
    
    if (!(d = flx_dns_packet_extend(p, sizeof(guint16))))
        return NULL;
    
    *((guint16*) d) = g_htons(v);
    return d;
}

guint8 *flx_dns_packet_append_uint32(flxDnsPacket *p, guint32 v) {
    guint8 *d;
    g_assert(p);

    if (!(d = flx_dns_packet_extend(p, sizeof(guint32))))
        return NULL;
    
    *((guint32*) d) = g_htonl(v);

    return d;
}

guint8 *flx_dns_packet_append_bytes(flxDnsPacket  *p, gconstpointer b, guint l) {
    guint8* d;

    g_assert(p);
    g_assert(b);
    g_assert(l);

    if (!(d = flx_dns_packet_extend(p, l)))
        return NULL;

    memcpy(d, b, l);
    return d;
}

guint8 *flx_dns_packet_extend(flxDnsPacket *p, guint l) {
    guint8 *d;
    
    g_assert(p);

    if (p->size+l > p->max_size)
        return NULL;
    
    d = FLX_DNS_PACKET_DATA(p) + p->size;
    p->size += l;
    
    return d;
}

guint8 *flx_dns_packet_append_name_compressed(flxDnsPacket *p, const gchar *name, guint8 *prev) {
    guint16 *d;
    signed long k;
    g_assert(p);

    if (!prev)
        return flx_dns_packet_append_name(p, name);
    
    k = prev - FLX_DNS_PACKET_DATA(p);
    if (k < 0 || k >= 0x4000 || (guint) k >= p->size)
        return flx_dns_packet_append_name(p, name);

    if (!(d = (guint16*) flx_dns_packet_extend(p, sizeof(guint16))))
        return NULL;
    
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

        n = FLX_DNS_PACKET_DATA(p)[index];

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

            memcpy(ret_name, FLX_DNS_PACKET_DATA(p) + index, n);
            index += n;
            ret_name += n;
            l -= n;
            
            if (!compressed)
                ret += n;
        } else if ((n & 0xC0) == 0xC0) {
            /* Compressed label */

            if (index+2 > p->size)
                return -1;

            index = ((guint) (FLX_DNS_PACKET_DATA(p)[index] & ~0xC0)) << 8 | FLX_DNS_PACKET_DATA(p)[index+1];

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

    *ret_v = g_ntohs(*((guint16*) (FLX_DNS_PACKET_DATA(p) + p->rindex)));
    p->rindex += sizeof(guint16);

    return 0;
}

gint flx_dns_packet_consume_uint32(flxDnsPacket *p, guint32 *ret_v) {
    g_assert(p);
    g_assert(ret_v);

    if (p->rindex + sizeof(guint32) > p->size)
        return -1;

    *ret_v = g_ntohl(*((guint32*) (FLX_DNS_PACKET_DATA(p) + p->rindex)));
    p->rindex += sizeof(guint32);
    
    return 0;
}

gint flx_dns_packet_consume_bytes(flxDnsPacket *p, gpointer ret_data, guint l) {
    g_assert(p);
    g_assert(ret_data);
    g_assert(l > 0);
    
    if (p->rindex + l > p->size)
        return -1;

    memcpy(ret_data, FLX_DNS_PACKET_DATA(p) + p->rindex, l);
    p->rindex += l;

    return 0;
}

gconstpointer flx_dns_packet_get_rptr(flxDnsPacket *p) {
    g_assert(p);
    
    if (p->rindex >= p->size)
        return NULL;

    return FLX_DNS_PACKET_DATA(p) + p->rindex;
}

gint flx_dns_packet_skip(flxDnsPacket *p, guint length) {
    g_assert(p);

    if (p->rindex + length > p->size)
        return -1;

    p->rindex += length;
    return 0;
}

flxRecord* flx_dns_packet_consume_record(flxDnsPacket *p, gboolean *ret_cache_flush) {
    gchar name[257], buf[257+6];
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
        flx_dns_packet_consume_uint16(p, &rdlength) < 0)
        return NULL;

    switch (type) {
        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME:
            if (flx_dns_packet_consume_name(p, buf, sizeof(buf)) < 0)
                return NULL;
            
            data = buf;
            rdlength = strlen(buf);
            break;

        case FLX_DNS_TYPE_SRV: {
            const guint8 *t = flx_dns_packet_get_rptr(p);

            if (flx_dns_packet_skip(p, 6) < 0)
                return NULL;
            
            memcpy(buf, t, 6);

            if (flx_dns_packet_consume_name(p, buf+6, sizeof(buf)-6) < 0)
                return NULL;

            data = buf;
            rdlength = 6 + strlen(buf+6);
            break;
        }
            
        default:

            if (rdlength > 0) {

                if (!(data = flx_dns_packet_get_rptr(p)) ||
                    flx_dns_packet_skip(p, rdlength) < 0)
                    return NULL;
            } else
                data = NULL;

            break;
    }

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
    guint8 *t;

    g_assert(p);
    g_assert(r);

    if (!(t = flx_dns_packet_append_name(p, r->key->name)) ||
        !flx_dns_packet_append_uint16(p, r->key->type) ||
        !flx_dns_packet_append_uint16(p, cache_flush ? (r->key->class | MDNS_CACHE_FLUSH) : (r->key->class &~ MDNS_CACHE_FLUSH)) ||
        !flx_dns_packet_append_uint32(p, r->ttl))
        return NULL;

    switch (r->key->type) {
        
        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME: {
            char ptr_name[257];

            g_assert((size_t) r->size+1 <= sizeof(ptr_name));
            memcpy(ptr_name, r->data, r->size);
            ptr_name[r->size] = 0;
            
            if (!flx_dns_packet_append_uint16(p, strlen(ptr_name)+1) ||
                !flx_dns_packet_append_name(p, ptr_name))
                return NULL;

            break;
        }

        case FLX_DNS_TYPE_SRV: {
            char name[257];

            g_assert(r->size >= 6 && (size_t) r->size-6+1 <= sizeof(name));
            memcpy(name, r->data+6, r->size-6);
            name[r->size-6] = 0;

            if (!flx_dns_packet_append_uint16(p, strlen(name+6)+1+6) ||
                !flx_dns_packet_append_bytes(p, r->data, 6) ||
                !flx_dns_packet_append_name(p, name))
                return NULL;

            break;
        }

        default:
            if (!flx_dns_packet_append_uint16(p, r->size) ||
                (r->size != 0 && !flx_dns_packet_append_bytes(p, r->data, r->size)))
                return NULL;
    }

    return t;
}
