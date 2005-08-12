/* $Id$ */

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

#include <netinet/in.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <avahi-common/domain.h>
#include "dns.h"

AvahiDnsPacket* avahi_dns_packet_new(guint mtu) {
    AvahiDnsPacket *p;
    guint max_size;

    if (mtu <= 0)
        max_size = AVAHI_DNS_PACKET_MAX_SIZE;
    else if (mtu >= AVAHI_DNS_PACKET_EXTRA_SIZE)
        max_size = mtu - AVAHI_DNS_PACKET_EXTRA_SIZE;
    else
        max_size = 0;

    if (max_size < AVAHI_DNS_PACKET_HEADER_SIZE)
        max_size = AVAHI_DNS_PACKET_HEADER_SIZE;
    
    p = g_malloc(sizeof(AvahiDnsPacket) + max_size);
    p->size = p->rindex = AVAHI_DNS_PACKET_HEADER_SIZE;
    p->max_size = max_size;
    p->name_table = NULL;

    memset(AVAHI_DNS_PACKET_DATA(p), 0, p->size);
    return p;
}

AvahiDnsPacket* avahi_dns_packet_new_query(guint mtu) {
    AvahiDnsPacket *p;

    p = avahi_dns_packet_new(mtu);
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_FLAGS, AVAHI_DNS_FLAGS(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    return p;
}

AvahiDnsPacket* avahi_dns_packet_new_response(guint mtu, gboolean aa) {
    AvahiDnsPacket *p;

    p = avahi_dns_packet_new(mtu);
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_FLAGS, AVAHI_DNS_FLAGS(1, 0, aa, 0, 0, 0, 0, 0, 0, 0));
    return p;
}

AvahiDnsPacket* avahi_dns_packet_new_reply(AvahiDnsPacket* p, guint mtu, gboolean copy_queries, gboolean aa) {
    AvahiDnsPacket *r;
    g_assert(p);

    r = avahi_dns_packet_new_response(mtu, aa);

    if (copy_queries) {
        guint n, saved_rindex;

        saved_rindex = p->rindex;
        p->rindex = AVAHI_DNS_PACKET_HEADER_SIZE;
        
        for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT); n > 0; n--) {
            AvahiKey *k;
            gboolean unicast_response;

            if ((k = avahi_dns_packet_consume_key(p, &unicast_response))) {
                avahi_dns_packet_append_key(r, k, unicast_response);
                avahi_key_unref(k);
            }
        }

        p->rindex = saved_rindex;

        avahi_dns_packet_set_field(r, AVAHI_DNS_FIELD_QDCOUNT, avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT));
    }

    avahi_dns_packet_set_field(r, AVAHI_DNS_FIELD_ID, avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ID));

    avahi_dns_packet_set_field(r, AVAHI_DNS_FIELD_FLAGS,
                               (avahi_dns_packet_get_field(r, AVAHI_DNS_FIELD_FLAGS) & ~AVAHI_DNS_FLAG_OPCODE) |
                               (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_FLAGS) & AVAHI_DNS_FLAG_OPCODE));

    return r;
} 


void avahi_dns_packet_free(AvahiDnsPacket *p) {
    g_assert(p);

    if (p->name_table)
        g_hash_table_destroy(p->name_table);
    
    g_free(p);
}

void avahi_dns_packet_set_field(AvahiDnsPacket *p, guint idx, guint16 v) {
    g_assert(p);
    g_assert(idx < AVAHI_DNS_PACKET_HEADER_SIZE);
    
    ((guint16*) AVAHI_DNS_PACKET_DATA(p))[idx] = g_htons(v);
}

guint16 avahi_dns_packet_get_field(AvahiDnsPacket *p, guint idx) {
    g_assert(p);
    g_assert(idx < AVAHI_DNS_PACKET_HEADER_SIZE);

    return g_ntohs(((guint16*) AVAHI_DNS_PACKET_DATA(p))[idx]);
}

void avahi_dns_packet_inc_field(AvahiDnsPacket *p, guint idx) {
    g_assert(p);
    g_assert(idx < AVAHI_DNS_PACKET_HEADER_SIZE);

    avahi_dns_packet_set_field(p, idx, avahi_dns_packet_get_field(p, idx) + 1);
}   

guint8* avahi_dns_packet_append_name(AvahiDnsPacket *p, const gchar *name) {
    guint8 *d, *saved_ptr = NULL;
    guint saved_size;
    
    g_assert(p);
    g_assert(name);

    saved_size = p->size;
    saved_ptr = avahi_dns_packet_extend(p, 0);
    
    while (*name) {
        guint8* prev;
        const gchar *pname;
        gchar label[64];

        /* Check whether we can compress this name. */

        if (p->name_table && (prev = g_hash_table_lookup(p->name_table, name))) {
            guint idx;
            
            g_assert(prev >= AVAHI_DNS_PACKET_DATA(p));
            idx = (guint) (prev - AVAHI_DNS_PACKET_DATA(p));

            g_assert(idx < p->size);

            if (idx < 0x4000) {
                guint16 *t;
                if (!(t = (guint16*) avahi_dns_packet_extend(p, sizeof(guint16))))
                    return NULL;

                *t = g_htons((0xC000 | idx));
                return saved_ptr;
            }
        }

        pname = name;
        
        if (!(avahi_unescape_label(&name, label, sizeof(label))))
            goto fail;

        if (!(d = avahi_dns_packet_append_string(p, label)))
            goto fail;

        if (!p->name_table)
            /* This works only for normalized domain names */
            p->name_table = g_hash_table_new_full((GHashFunc) g_str_hash, (GEqualFunc) g_str_equal, g_free, NULL);

        g_hash_table_insert(p->name_table, g_strdup(pname), d);
    }

    if (!(d = avahi_dns_packet_extend(p, 1)))
        goto fail;
    
    *d = 0;

    return saved_ptr;

fail:
    p->size = saved_size;
    return NULL;
}

guint8* avahi_dns_packet_append_uint16(AvahiDnsPacket *p, guint16 v) {
    guint8 *d;
    g_assert(p);
    
    if (!(d = avahi_dns_packet_extend(p, sizeof(guint16))))
        return NULL;
    
    *((guint16*) d) = g_htons(v);
    return d;
}

guint8 *avahi_dns_packet_append_uint32(AvahiDnsPacket *p, guint32 v) {
    guint8 *d;
    g_assert(p);

    if (!(d = avahi_dns_packet_extend(p, sizeof(guint32))))
        return NULL;
    
    *((guint32*) d) = g_htonl(v);

    return d;
}

guint8 *avahi_dns_packet_append_bytes(AvahiDnsPacket  *p, gconstpointer b, guint l) {
    guint8* d;

    g_assert(p);
    g_assert(b);
    g_assert(l);

    if (!(d = avahi_dns_packet_extend(p, l)))
        return NULL;

    memcpy(d, b, l);
    return d;
}

guint8* avahi_dns_packet_append_string(AvahiDnsPacket *p, const gchar *s) {
    guint8* d;
    guint k;
    
    g_assert(p);
    g_assert(s);

    if ((k = strlen(s)) >= 255)
        k = 255;
    
    if (!(d = avahi_dns_packet_extend(p, k+1)))
        return NULL;

    *d = (guint8) k;
    memcpy(d+1, s, k);

    return d;
}

guint8 *avahi_dns_packet_extend(AvahiDnsPacket *p, guint l) {
    guint8 *d;
    
    g_assert(p);

    if (p->size+l > p->max_size)
        return NULL;
    
    d = AVAHI_DNS_PACKET_DATA(p) + p->size;
    p->size += l;
    
    return d;
}

gint avahi_dns_packet_check_valid(AvahiDnsPacket *p) {
    guint16 flags;
    g_assert(p);

    if (p->size < 12)
        return -1;

    flags = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_FLAGS);

    if (flags & AVAHI_DNS_FLAG_OPCODE || flags & AVAHI_DNS_FLAG_RCODE)
        return -1;

    return 0;
}

gint avahi_dns_packet_is_query(AvahiDnsPacket *p) {
    g_assert(p);
    
    return !(avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_FLAGS) & AVAHI_DNS_FLAG_QR);
}

static gint consume_labels(AvahiDnsPacket *p, guint idx, gchar *ret_name, guint l) {
    gint ret = 0;
    int compressed = 0;
    int first_label = 1;
    g_assert(p && ret_name && l);
    
    for (;;) {
        guint8 n;

        if (idx+1 > p->size)
            return -1;

        n = AVAHI_DNS_PACKET_DATA(p)[idx];

        if (!n) {
            idx++;
            if (!compressed)
                ret++;

            if (l < 1)
                return -1;
            *ret_name = 0;
            
            return ret;
            
        } else if (n <= 63) {
            /* Uncompressed label */
            idx++;
            if (!compressed)
                ret++;
        
            if (idx + n > p->size)
                return -1;

            if ((guint) n + 1 > l)
                return -1;

            if (!first_label) {
                *(ret_name++) = '.';
                l--;
            } else
                first_label = 0;

            if (!(avahi_escape_label(AVAHI_DNS_PACKET_DATA(p) + idx, n, &ret_name, &l)))
                return -1;

            idx += n;
            
            if (!compressed)
                ret += n;
        } else if ((n & 0xC0) == 0xC0) {
            /* Compressed label */

            if (idx+2 > p->size)
                return -1;

            idx = ((guint) (AVAHI_DNS_PACKET_DATA(p)[idx] & ~0xC0)) << 8 | AVAHI_DNS_PACKET_DATA(p)[idx+1];

            if (!compressed)
                ret += 2;
            
            compressed = 1;
        } else
            return -1;
    }
}

gint avahi_dns_packet_consume_name(AvahiDnsPacket *p, gchar *ret_name, guint l) {
    gint r;
    
    if ((r = consume_labels(p, p->rindex, ret_name, l)) < 0)
        return -1;

    p->rindex += r;
    return 0;
}

gint avahi_dns_packet_consume_uint16(AvahiDnsPacket *p, guint16 *ret_v) {
    g_assert(p);
    g_assert(ret_v);

    if (p->rindex + sizeof(guint16) > p->size)
        return -1;

    *ret_v = g_ntohs(*((guint16*) (AVAHI_DNS_PACKET_DATA(p) + p->rindex)));
    p->rindex += sizeof(guint16);

    return 0;
}

gint avahi_dns_packet_consume_uint32(AvahiDnsPacket *p, guint32 *ret_v) {
    g_assert(p);
    g_assert(ret_v);

    if (p->rindex + sizeof(guint32) > p->size)
        return -1;

    *ret_v = g_ntohl(*((guint32*) (AVAHI_DNS_PACKET_DATA(p) + p->rindex)));
    p->rindex += sizeof(guint32);
    
    return 0;
}

gint avahi_dns_packet_consume_bytes(AvahiDnsPacket *p, gpointer ret_data, guint l) {
    g_assert(p);
    g_assert(ret_data);
    g_assert(l > 0);
    
    if (p->rindex + l > p->size)
        return -1;

    memcpy(ret_data, AVAHI_DNS_PACKET_DATA(p) + p->rindex, l);
    p->rindex += l;

    return 0;
}

gint avahi_dns_packet_consume_string(AvahiDnsPacket *p, gchar *ret_string, guint l) {
    guint k;
    
    g_assert(p);
    g_assert(ret_string);
    g_assert(l > 0);

    if (p->rindex >= p->size)
        return -1;

    k = AVAHI_DNS_PACKET_DATA(p)[p->rindex];

    if (p->rindex+1+k > p->size)
        return -1;

    if (l > k+1)
        l = k+1;

    memcpy(ret_string, AVAHI_DNS_PACKET_DATA(p)+p->rindex+1, l-1);
    ret_string[l-1] = 0;

    
    p->rindex += 1+k;

    return 0;
}

gconstpointer avahi_dns_packet_get_rptr(AvahiDnsPacket *p) {
    g_assert(p);
    
    if (p->rindex > p->size)
        return NULL;

    return AVAHI_DNS_PACKET_DATA(p) + p->rindex;
}

gint avahi_dns_packet_skip(AvahiDnsPacket *p, guint length) {
    g_assert(p);

    if (p->rindex + length > p->size)
        return -1;

    p->rindex += length;
    return 0;
}

AvahiRecord* avahi_dns_packet_consume_record(AvahiDnsPacket *p, gboolean *ret_cache_flush) {
    gchar name[257], buf[257];
    guint16 type, class;
    guint32 ttl;
    guint16 rdlength;
    AvahiRecord *r = NULL;
    gconstpointer start;

    g_assert(p);
    g_assert(ret_cache_flush);

/*     avahi_log_debug("consume_record()"); */

    if (avahi_dns_packet_consume_name(p, name, sizeof(name)) < 0 ||
        avahi_dns_packet_consume_uint16(p, &type) < 0 ||
        avahi_dns_packet_consume_uint16(p, &class) < 0 ||
        avahi_dns_packet_consume_uint32(p, &ttl) < 0 ||
        avahi_dns_packet_consume_uint16(p, &rdlength) < 0 ||
        p->rindex + rdlength > p->size)
        goto fail;

/*     avahi_log_debug("name = %s, rdlength = %u", name, rdlength); */

    *ret_cache_flush = !!(class & AVAHI_DNS_CACHE_FLUSH);
    class &= ~AVAHI_DNS_CACHE_FLUSH;
    
    start = avahi_dns_packet_get_rptr(p);
    
    r = avahi_record_new_full(name, class, type, ttl);
    
    switch (type) {
        case AVAHI_DNS_TYPE_PTR:
        case AVAHI_DNS_TYPE_CNAME:

/*             avahi_log_debug("ptr"); */
            
            if (avahi_dns_packet_consume_name(p, buf, sizeof(buf)) < 0)
                goto fail;

            r->data.ptr.name = g_strdup(buf);
            break;

            
        case AVAHI_DNS_TYPE_SRV:

/*             avahi_log_debug("srv"); */
            
            if (avahi_dns_packet_consume_uint16(p, &r->data.srv.priority) < 0 ||
                avahi_dns_packet_consume_uint16(p, &r->data.srv.weight) < 0 ||
                avahi_dns_packet_consume_uint16(p, &r->data.srv.port) < 0 ||
                avahi_dns_packet_consume_name(p, buf, sizeof(buf)) < 0)
                goto fail;
            
            r->data.srv.name = g_strdup(buf);
            break;

        case AVAHI_DNS_TYPE_HINFO:
            
/*             avahi_log_debug("hinfo"); */

            if (avahi_dns_packet_consume_string(p, buf, sizeof(buf)) < 0)
                goto fail;

            r->data.hinfo.cpu = g_strdup(buf);

            if (avahi_dns_packet_consume_string(p, buf, sizeof(buf)) < 0)
                goto fail;

            r->data.hinfo.os = g_strdup(buf);
            break;

        case AVAHI_DNS_TYPE_TXT:

/*             avahi_log_debug("txt"); */

            if (rdlength > 0) {
                r->data.txt.string_list = avahi_string_list_parse(avahi_dns_packet_get_rptr(p), rdlength);
                
                if (avahi_dns_packet_skip(p, rdlength) < 0)
                    goto fail;
            } else
                r->data.txt.string_list = NULL;
            
            break;

        case AVAHI_DNS_TYPE_A:

/*             avahi_log_debug("A"); */

            if (avahi_dns_packet_consume_bytes(p, &r->data.a.address, sizeof(AvahiIPv4Address)) < 0)
                goto fail;
            
            break;

        case AVAHI_DNS_TYPE_AAAA:

/*             avahi_log_debug("aaaa"); */
            
            if (avahi_dns_packet_consume_bytes(p, &r->data.aaaa.address, sizeof(AvahiIPv6Address)) < 0)
                goto fail;
            
            break;
            
        default:

/*             avahi_log_debug("generic"); */
            
            if (rdlength > 0) {

                r->data.generic.data = g_memdup(avahi_dns_packet_get_rptr(p), rdlength);
                
                if (avahi_dns_packet_skip(p, rdlength) < 0)
                    goto fail;
            }

            break;
    }

/*     avahi_log_debug("%i == %u ?", (guint8*) avahi_dns_packet_get_rptr(p) - (guint8*) start, rdlength); */
    
    /* Check if we read enough data */
    if ((const guint8*) avahi_dns_packet_get_rptr(p) - (const guint8*) start != rdlength)
        goto fail;

    return r;

fail:
    if (r)
        avahi_record_unref(r);

    return NULL;
}

AvahiKey* avahi_dns_packet_consume_key(AvahiDnsPacket *p, gboolean *ret_unicast_response) {
    gchar name[256];
    guint16 type, class;

    g_assert(p);
    g_assert(ret_unicast_response);

    if (avahi_dns_packet_consume_name(p, name, sizeof(name)) < 0 ||
        avahi_dns_packet_consume_uint16(p, &type) < 0 ||
        avahi_dns_packet_consume_uint16(p, &class) < 0)
        return NULL;

    *ret_unicast_response = !!(class & AVAHI_DNS_UNICAST_RESPONSE);
    class &= ~AVAHI_DNS_UNICAST_RESPONSE;

    return avahi_key_new(name, class, type);
}

guint8* avahi_dns_packet_append_key(AvahiDnsPacket *p, AvahiKey *k, gboolean unicast_response) {
    guint8 *t;
    guint size;
    
    g_assert(p);
    g_assert(k);

    size = p->size;
    
    if (!(t = avahi_dns_packet_append_name(p, k->name)) ||
        !avahi_dns_packet_append_uint16(p, k->type) ||
        !avahi_dns_packet_append_uint16(p, k->clazz | (unicast_response ? AVAHI_DNS_UNICAST_RESPONSE : 0))) {
        p->size = size;
        return NULL;
    }

    return t;
}

guint8* avahi_dns_packet_append_record(AvahiDnsPacket *p, AvahiRecord *r, gboolean cache_flush, guint max_ttl) {
    guint8 *t, *l, *start;
    guint size;

    g_assert(p);
    g_assert(r);

    size = p->size;

    if (!(t = avahi_dns_packet_append_name(p, r->key->name)) ||
        !avahi_dns_packet_append_uint16(p, r->key->type) ||
        !avahi_dns_packet_append_uint16(p, cache_flush ? (r->key->clazz | AVAHI_DNS_CACHE_FLUSH) : (r->key->clazz &~ AVAHI_DNS_CACHE_FLUSH)) ||
        !avahi_dns_packet_append_uint32(p, (max_ttl && r->ttl > max_ttl) ? max_ttl : r->ttl) ||
        !(l = avahi_dns_packet_append_uint16(p, 0)))
        goto fail;

    start = avahi_dns_packet_extend(p, 0);

    switch (r->key->type) {
        
        case AVAHI_DNS_TYPE_PTR:
        case AVAHI_DNS_TYPE_CNAME :

            if (!(avahi_dns_packet_append_name(p, r->data.ptr.name)))
                goto fail;
            
            break;

        case AVAHI_DNS_TYPE_SRV:

            if (!avahi_dns_packet_append_uint16(p, r->data.srv.priority) ||
                !avahi_dns_packet_append_uint16(p, r->data.srv.weight) ||
                !avahi_dns_packet_append_uint16(p, r->data.srv.port) ||
                !avahi_dns_packet_append_name(p, r->data.srv.name))
                goto fail;

            break;

        case AVAHI_DNS_TYPE_HINFO:
            if (!avahi_dns_packet_append_string(p, r->data.hinfo.cpu) ||
                !avahi_dns_packet_append_string(p, r->data.hinfo.os))
                goto fail;

            break;

        case AVAHI_DNS_TYPE_TXT: {

            guint8 *data;
            guint n;

            n = avahi_string_list_serialize(r->data.txt.string_list, NULL, 0);

/*             avahi_log_debug("appending string: %u %p", n, r->data.txt.string_list); */

            if (!(data = avahi_dns_packet_extend(p, n)))
                goto fail;

            avahi_string_list_serialize(r->data.txt.string_list, data, n);
            break;
        }


        case AVAHI_DNS_TYPE_A:

            if (!avahi_dns_packet_append_bytes(p, &r->data.a.address, sizeof(r->data.a.address)))
                goto fail;
            
            break;

        case AVAHI_DNS_TYPE_AAAA:
            
            if (!avahi_dns_packet_append_bytes(p, &r->data.aaaa.address, sizeof(r->data.aaaa.address)))
                goto fail;
            
            break;
            
        default:

            if (r->data.generic.size &&
                avahi_dns_packet_append_bytes(p, r->data.generic.data, r->data.generic.size))
                goto fail;

            break;
    }



    
    size = avahi_dns_packet_extend(p, 0) - start;
    g_assert(size <= 0xFFFF);

/*     avahi_log_debug("appended %u", size); */

    * (guint16*) l = g_htons((guint16) size);
    
    return t;


fail:
    p->size = size;
    return NULL;
}

gboolean avahi_dns_packet_is_empty(AvahiDnsPacket *p) {
    g_assert(p);

    return p->size <= AVAHI_DNS_PACKET_HEADER_SIZE;
}

guint avahi_dns_packet_space(AvahiDnsPacket *p) {
    g_assert(p);

    g_assert(p->size <= p->max_size);
    
    return p->max_size - p->size;
}
