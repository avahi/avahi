#include <netinet/in.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dns.h"
#include "util.h"

flxDnsPacket* flx_dns_packet_new(guint max_size) {
    flxDnsPacket *p;

    if (max_size <= 0)
        max_size = FLX_DNS_PACKET_MAX_SIZE;
    else if (max_size < FLX_DNS_PACKET_HEADER_SIZE)
        max_size = FLX_DNS_PACKET_HEADER_SIZE;
    
    p = g_malloc(sizeof(flxDnsPacket) + max_size);
    p->size = p->rindex = FLX_DNS_PACKET_HEADER_SIZE;
    p->max_size = max_size;
    p->name_table = NULL;

    memset(FLX_DNS_PACKET_DATA(p), 0, p->size);
    return p;
}

flxDnsPacket* flx_dns_packet_new_query(guint max_size) {
    flxDnsPacket *p;

    p = flx_dns_packet_new(max_size);
    flx_dns_packet_set_field(p, FLX_DNS_FIELD_FLAGS, FLX_DNS_FLAGS(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    return p;
}

flxDnsPacket* flx_dns_packet_new_response(guint max_size) {
    flxDnsPacket *p;

    p = flx_dns_packet_new(max_size);
    flx_dns_packet_set_field(p, FLX_DNS_FIELD_FLAGS, FLX_DNS_FLAGS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    return p;
}

void flx_dns_packet_free(flxDnsPacket *p) {
    g_assert(p);

    if (p->name_table)
        g_hash_table_destroy(p->name_table);
    
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

/* Read the first label from string *name, unescape "\" and write it to dest */
gchar *flx_unescape_label(gchar *dest, guint size, const gchar **name) {
    guint i = 0;
    gchar *d;
    
    g_assert(dest);
    g_assert(size > 0);
    g_assert(name);
    g_assert(*name);

    d = dest;
    
    for (;;) {
        if (i >= size)
            return NULL;

        if (**name == '.') {
            (*name)++;
            break;
        }
        
        if (**name == 0)
            break;
        
        if (**name == '\\') {
            (*name) ++;
            
            if (**name == 0)
                break;
        }
        
        *(d++) = *((*name) ++);
        i++;
    }

    g_assert(i < size);

    *d = 0;

    return dest;
}

guint8* flx_dns_packet_append_name(flxDnsPacket *p, const gchar *name) {
    guint8 *d, *saved_ptr = NULL;
    guint saved_size;
    
    g_assert(p);
    g_assert(name);

    saved_size = p->size;
    saved_ptr = flx_dns_packet_extend(p, 0);
    
    while (*name) {
        guint n;
        guint8* prev;
        const gchar *pname;
        gchar label[64];

        /* Check whether we can compress this name. */

        if (p->name_table && (prev = g_hash_table_lookup(p->name_table, name))) {
            guint index;
            
            g_assert(prev >= FLX_DNS_PACKET_DATA(p));
            index = (guint) (prev - FLX_DNS_PACKET_DATA(p));

            g_assert(index < p->size);

            if (index < 0x4000) {
                guint16 *t;
                if (!(t = (guint16*) flx_dns_packet_extend(p, sizeof(guint16))))
                    return NULL;

                *t = g_htons((0xC000 | index));
                return saved_ptr;
            }
        }

        pname = name;
        
        if (!(flx_unescape_label(label, sizeof(label), &name)))
            goto fail;

        if (!(d = flx_dns_packet_append_string(p, label)))
            goto fail;

        if (!p->name_table)
            p->name_table = g_hash_table_new_full((GHashFunc) flx_domain_hash, (GEqualFunc) flx_domain_equal, g_free, NULL);

        g_hash_table_insert(p->name_table, g_strdup(pname), d);
    }

    if (!(d = flx_dns_packet_extend(p, 1)))
        goto fail;
    
    *d = 0;

    return saved_ptr;

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

guint8* flx_dns_packet_append_string(flxDnsPacket *p, const gchar *s) {
    guint8* d;
    guint k;
    
    g_assert(p);
    g_assert(s);

    if ((k = strlen(s)) >= 255)
        k = 255;
    
    if (!(d = flx_dns_packet_extend(p, k+1)))
        return NULL;

    *d = (guint8) k;
    memcpy(d+1, s, k);

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

gint flx_dns_packet_check_valid(flxDnsPacket *p) {
    guint16 flags;
    g_assert(p);

    if (p->size < 12)
        return -1;

    flags = flx_dns_packet_get_field(p, FLX_DNS_FIELD_FLAGS);

    if (flags & FLX_DNS_FLAG_OPCODE || flags & FLX_DNS_FLAG_RCODE)
        return -1;

    return 0;
}

gint flx_dns_packet_is_query(flxDnsPacket *p) {
    g_assert(p);
    
    return !(flx_dns_packet_get_field(p, FLX_DNS_FIELD_FLAGS) & FLX_DNS_FLAG_QR);
}

/* Read a label from a DNS packet, escape "\" and ".", append \0 */
static gchar *escape_label(guint8* src, guint src_length, gchar **ret_name, guint *ret_name_length) {
    gchar *r;

    g_assert(src);
    g_assert(ret_name);
    g_assert(*ret_name);
    g_assert(ret_name_length);
    g_assert(*ret_name_length > 0);

    r = *ret_name;

    while (src_length > 0) {
        if (*src == '.' || *src == '\\') {
            if (*ret_name_length < 3)
                return NULL;
            
            *((*ret_name) ++) = '\\';
            (*ret_name_length) --;
        }

        if (*ret_name_length < 2)
            return NULL;
        
        *((*ret_name)++) = *src;
        (*ret_name_length) --;

        src_length --;
        src++;
    }

    **ret_name = 0;

    return r;
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

            if (!(escape_label(FLX_DNS_PACKET_DATA(p) + index, n, &ret_name, &l)))
                return -1;

            index += n;
            
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

gint flx_dns_packet_consume_string(flxDnsPacket *p, gchar *ret_string, guint l) {
    guint k;
    
    g_assert(p);
    g_assert(ret_string);
    g_assert(l > 0);

    if (p->rindex >= p->size)
        return -1;

    k = FLX_DNS_PACKET_DATA(p)[p->rindex];

    if (p->rindex+1+k > p->size)
        return -1;

    if (l > k+1)
        l = k+1;

    memcpy(ret_string, FLX_DNS_PACKET_DATA(p)+p->rindex+1, l-1);
    ret_string[l-1] = 0;

    
    p->rindex += 1+k;

    return 0;
    
}

gconstpointer flx_dns_packet_get_rptr(flxDnsPacket *p) {
    g_assert(p);
    
    if (p->rindex > p->size)
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
    gchar name[257], buf[257];
    guint16 type, class;
    guint32 ttl;
    guint16 rdlength;
    gconstpointer data;
    flxRecord *r = NULL;
    gconstpointer start;

    g_assert(p);
    g_assert(ret_cache_flush);

/*     g_message("consume_record()"); */

    if (flx_dns_packet_consume_name(p, name, sizeof(name)) < 0 ||
        flx_dns_packet_consume_uint16(p, &type) < 0 ||
        flx_dns_packet_consume_uint16(p, &class) < 0 ||
        flx_dns_packet_consume_uint32(p, &ttl) < 0 ||
        flx_dns_packet_consume_uint16(p, &rdlength) < 0 ||
        p->rindex + rdlength > p->size)
        
        goto fail;

/*     g_message("name = %s, rdlength = %u", name, rdlength); */

    *ret_cache_flush = !!(class & FLX_DNS_CACHE_FLUSH);
    class &= ~ FLX_DNS_CACHE_FLUSH;
    
    start = flx_dns_packet_get_rptr(p);
    
    r = flx_record_new_full(name, class, type);
    
    switch (type) {
        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME:

/*             g_message("ptr"); */
            
            if (flx_dns_packet_consume_name(p, buf, sizeof(buf)) < 0)
                goto fail;

            r->data.ptr.name = g_strdup(buf);
            break;

            
        case FLX_DNS_TYPE_SRV:

/*             g_message("srv"); */
            
            if (flx_dns_packet_consume_uint16(p, &r->data.srv.priority) < 0 ||
                flx_dns_packet_consume_uint16(p, &r->data.srv.weight) < 0 ||
                flx_dns_packet_consume_uint16(p, &r->data.srv.port) < 0 ||
                flx_dns_packet_consume_name(p, buf, sizeof(buf)) < 0)
                goto fail;
            
            r->data.srv.name = g_strdup(buf);
            break;

        case FLX_DNS_TYPE_HINFO:
            
/*             g_message("hinfo"); */

            if (flx_dns_packet_consume_string(p, buf, sizeof(buf)) < 0)
                goto fail;

            r->data.hinfo.cpu = g_strdup(buf);

            if (flx_dns_packet_consume_string(p, buf, sizeof(buf)) < 0)
                goto fail;

            r->data.hinfo.os = g_strdup(buf);
            break;

        case FLX_DNS_TYPE_TXT:

/*             g_message("txt"); */

            if (rdlength > 0) {
                r->data.txt.string_list = flx_string_list_parse(flx_dns_packet_get_rptr(p), rdlength);
                
                if (flx_dns_packet_skip(p, rdlength) < 0)
                    goto fail;
            } else
                r->data.txt.string_list = NULL;
            
            break;

        case FLX_DNS_TYPE_A:

/*             g_message("A"); */

            if (flx_dns_packet_consume_bytes(p, &r->data.a.address, sizeof(flxIPv4Address)) < 0)
                goto fail;
            
            break;

        case FLX_DNS_TYPE_AAAA:

/*             g_message("aaaa"); */
            
            if (flx_dns_packet_consume_bytes(p, &r->data.aaaa.address, sizeof(flxIPv6Address)) < 0)
                goto fail;
            
            break;
            
        default:

/*             g_message("generic"); */
            
            if (rdlength > 0) {

                r->data.generic.data = g_memdup(flx_dns_packet_get_rptr(p), rdlength);
                
                if (flx_dns_packet_skip(p, rdlength) < 0)
                    goto fail;
            }

            break;
    }

/*     g_message("%i == %u ?", (guint8*) flx_dns_packet_get_rptr(p) - (guint8*) start, rdlength); */
    
    /* Check if we read enough data */
    if ((guint8*) flx_dns_packet_get_rptr(p) - (guint8*) start != rdlength)
        goto fail;
    
    r->ttl = ttl;

    return r;

fail:
    if (r)
        flx_record_unref(r);

    return NULL;
}

flxKey* flx_dns_packet_consume_key(flxDnsPacket *p) {
    gchar name[256];
    guint16 type, class;

    g_assert(p);

    if (flx_dns_packet_consume_name(p, name, sizeof(name)) < 0 ||
        flx_dns_packet_consume_uint16(p, &type) < 0 ||
        flx_dns_packet_consume_uint16(p, &class) < 0)
        return NULL;

    class &= ~ FLX_DNS_CACHE_FLUSH;

    return flx_key_new(name, class, type);
}

guint8* flx_dns_packet_append_key(flxDnsPacket *p, flxKey *k) {
    guint8 *t;
    guint size;
    
    g_assert(p);
    g_assert(k);

    size = p->size;
    
    if (!(t = flx_dns_packet_append_name(p, k->name)) ||
        !flx_dns_packet_append_uint16(p, k->type) ||
        !flx_dns_packet_append_uint16(p, k->class)) {
        p->size = size;
        return NULL;
    }

    return t;
}

guint8* flx_dns_packet_append_record(flxDnsPacket *p, flxRecord *r, gboolean cache_flush) {
    guint8 *t, *l, *start;
    guint size;

    g_assert(p);
    g_assert(r);

    size = p->size;

    if (!(t = flx_dns_packet_append_name(p, r->key->name)) ||
        !flx_dns_packet_append_uint16(p, r->key->type) ||
        !flx_dns_packet_append_uint16(p, cache_flush ? (r->key->class | FLX_DNS_CACHE_FLUSH) : (r->key->class &~ FLX_DNS_CACHE_FLUSH)) ||
        !flx_dns_packet_append_uint32(p, r->ttl) ||
        !(l = flx_dns_packet_append_uint16(p, 0)))
        goto fail;

    start = flx_dns_packet_extend(p, 0);

    switch (r->key->type) {
        
        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME :

            if (!(flx_dns_packet_append_name(p, r->data.ptr.name)))
                goto fail;
            
            break;

        case FLX_DNS_TYPE_SRV:

            if (!flx_dns_packet_append_uint16(p, r->data.srv.priority) ||
                !flx_dns_packet_append_uint16(p, r->data.srv.weight) ||
                !flx_dns_packet_append_uint16(p, r->data.srv.port) ||
                !flx_dns_packet_append_name(p, r->data.srv.name))
                goto fail;

            break;

        case FLX_DNS_TYPE_HINFO:
            if (!flx_dns_packet_append_string(p, r->data.hinfo.cpu) ||
                !flx_dns_packet_append_string(p, r->data.hinfo.os))
                goto fail;

            break;

        case FLX_DNS_TYPE_TXT: {

            guint8 *data;
            guint size;

            size = flx_string_list_serialize(r->data.txt.string_list, NULL, 0);

/*             g_message("appending string: %u %p", size, r->data.txt.string_list); */

            if (!(data = flx_dns_packet_extend(p, size)))
                goto fail;

            flx_string_list_serialize(r->data.txt.string_list, data, size);
            break;
        }


        case FLX_DNS_TYPE_A:

            if (!flx_dns_packet_append_bytes(p, &r->data.a.address, sizeof(r->data.a.address)))
                goto fail;
            
            break;

        case FLX_DNS_TYPE_AAAA:
            
            if (!flx_dns_packet_append_bytes(p, &r->data.aaaa.address, sizeof(r->data.aaaa.address)))
                goto fail;
            
            break;
            
        default:

            if (r->data.generic.size &&
                flx_dns_packet_append_bytes(p, r->data.generic.data, r->data.generic.size))
                goto fail;

            break;
    }



    
    size = flx_dns_packet_extend(p, 0) - start;
    g_assert(size <= 0xFFFF);

/*     g_message("appended %u", size); */

    * (guint16*) l = g_htons((guint16) size);
    
    return t;


fail:
    p->size = size;
    return NULL;
}

gboolean flx_dns_packet_is_empty(flxDnsPacket *p) {
    g_assert(p);

    return p->size <= FLX_DNS_PACKET_HEADER_SIZE;
}

guint flx_dns_packet_space(flxDnsPacket *p) {
    g_assert(p);

    g_assert(p->size <= p->max_size);
    
    return p->max_size - p->size;
}
