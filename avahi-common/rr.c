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

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "util.h"
#include "rr.h"

AvahiKey *avahi_key_new(const gchar *name, guint16 class, guint16 type) {
    AvahiKey *k;
    g_assert(name);

    k = g_new(AvahiKey, 1);
    k->ref = 1;
    k->name = avahi_normalize_name(name);    
    k->clazz = class;
    k->type = type;

/*     g_message("%p %% ref=1", k); */
    
    return k;
}

AvahiKey *avahi_key_ref(AvahiKey *k) {
    g_assert(k);
    g_assert(k->ref >= 1);

    k->ref++;

/*     g_message("%p ++ ref=%i", k, k->ref); */

    return k;
}

void avahi_key_unref(AvahiKey *k) {
    g_assert(k);
    g_assert(k->ref >= 1);

/*     g_message("%p -- ref=%i", k, k->ref-1); */
    
    if ((--k->ref) <= 0) {
        g_free(k->name);
        g_free(k);
    }
}

AvahiRecord *avahi_record_new(AvahiKey *k, guint32 ttl) {
    AvahiRecord *r;
    
    g_assert(k);
    
    r = g_new(AvahiRecord, 1);
    r->ref = 1;
    r->key = avahi_key_ref(k);

    memset(&r->data, 0, sizeof(r->data));

    r->ttl = ttl != (guint32) -1 ? ttl : AVAHI_DEFAULT_TTL;

    return r;
}

AvahiRecord *avahi_record_new_full(const gchar *name, guint16 class, guint16 type, guint32 ttl) {
    AvahiRecord *r;
    AvahiKey *k;

    g_assert(name);
    
    k = avahi_key_new(name, class, type);
    r = avahi_record_new(k, ttl);
    avahi_key_unref(k);

    return r;
}

AvahiRecord *avahi_record_ref(AvahiRecord *r) {
    g_assert(r);
    g_assert(r->ref >= 1);

    r->ref++;
    return r;
}

void avahi_record_unref(AvahiRecord *r) {
    g_assert(r);
    g_assert(r->ref >= 1);

    if ((--r->ref) <= 0) {
        switch (r->key->type) {

            case AVAHI_DNS_TYPE_SRV:
                g_free(r->data.srv.name);
                break;

            case AVAHI_DNS_TYPE_PTR:
            case AVAHI_DNS_TYPE_CNAME:
                g_free(r->data.ptr.name);
                break;

            case AVAHI_DNS_TYPE_HINFO:
                g_free(r->data.hinfo.cpu);
                g_free(r->data.hinfo.os);
                break;

            case AVAHI_DNS_TYPE_TXT:
                avahi_string_list_free(r->data.txt.string_list);
                break;

            case AVAHI_DNS_TYPE_A:
            case AVAHI_DNS_TYPE_AAAA:
                break;
            
            default:
                g_free(r->data.generic.data);
        }
        
        avahi_key_unref(r->key);
        g_free(r);
    }
}

const gchar *avahi_dns_class_to_string(guint16 class) {
    if (class & AVAHI_DNS_CACHE_FLUSH) 
        return "FLUSH";

    switch (class) {
        case AVAHI_DNS_CLASS_IN:
            return "IN";
        case AVAHI_DNS_CLASS_ANY:
            return "ANY";
        default:
            return NULL;
    }
}

const gchar *avahi_dns_type_to_string(guint16 type) {
    switch (type) {
        case AVAHI_DNS_TYPE_CNAME:
            return "CNAME";
        case AVAHI_DNS_TYPE_A:
            return "A";
        case AVAHI_DNS_TYPE_AAAA:
            return "AAAA";
        case AVAHI_DNS_TYPE_PTR:
            return "PTR";
        case AVAHI_DNS_TYPE_HINFO:
            return "HINFO";
        case AVAHI_DNS_TYPE_TXT:
            return "TXT";
        case AVAHI_DNS_TYPE_SRV:
            return "SRV";
        case AVAHI_DNS_TYPE_ANY:
            return "ANY";
        default:
            return NULL;
    }
}

gchar *avahi_key_to_string(const AvahiKey *k) {
    g_assert(k);
    g_assert(k->ref >= 1);
    
    return g_strdup_printf("%s\t%s\t%s",
                           k->name,
                           avahi_dns_class_to_string(k->clazz),
                           avahi_dns_type_to_string(k->type));
}

gchar *avahi_record_to_string(const AvahiRecord *r) {
    gchar *p, *s;
    char buf[257], *t = NULL, *d = NULL;

    g_assert(r);
    g_assert(r->ref >= 1);
    
    switch (r->key->type) {
        case AVAHI_DNS_TYPE_A:
            inet_ntop(AF_INET, &r->data.a.address.address, t = buf, sizeof(buf));
            break;
            
        case AVAHI_DNS_TYPE_AAAA:
            inet_ntop(AF_INET6, &r->data.aaaa.address.address, t = buf, sizeof(buf));
            break;
            
        case AVAHI_DNS_TYPE_PTR:
        case AVAHI_DNS_TYPE_CNAME :

            t = r->data.ptr.name;
            break;

        case AVAHI_DNS_TYPE_TXT:
            t = d = avahi_string_list_to_string(r->data.txt.string_list);
            break;

        case AVAHI_DNS_TYPE_HINFO:

            snprintf(t = buf, sizeof(buf), "\"%s\" \"%s\"", r->data.hinfo.cpu, r->data.hinfo.os);
            break;

        case AVAHI_DNS_TYPE_SRV:

            snprintf(t = buf, sizeof(buf), "%u %u %u %s",
                     r->data.srv.priority,
                     r->data.srv.weight,
                     r->data.srv.port,
                     r->data.srv.name);

            break;
    }

    p = avahi_key_to_string(r->key);
    s = g_strdup_printf("%s %s ; ttl=%u", p, t ? t : "<unparsable>", r->ttl);
    g_free(p);
    g_free(d);
    
    return s;
}

gboolean avahi_key_equal(const AvahiKey *a, const AvahiKey *b) {
    g_assert(a);
    g_assert(b);

    if (a == b)
        return TRUE;
    
/*     g_message("equal: %p %p", a, b); */
    
    return avahi_domain_equal(a->name, b->name) &&
        a->type == b->type &&
        a->clazz == b->clazz;
}

gboolean avahi_key_pattern_match(const AvahiKey *pattern, const AvahiKey *k) {
    g_assert(pattern);
    g_assert(k);

/*     g_message("equal: %p %p", a, b); */

    g_assert(!avahi_key_is_pattern(k));

    if (pattern == k)
        return TRUE;
    
    return avahi_domain_equal(pattern->name, k->name) &&
        (pattern->type == k->type || pattern->type == AVAHI_DNS_TYPE_ANY) &&
        (pattern->clazz == k->clazz || pattern->clazz == AVAHI_DNS_CLASS_ANY);
}

gboolean avahi_key_is_pattern(const AvahiKey *k) {
    g_assert(k);

    return
        k->type == AVAHI_DNS_TYPE_ANY ||
        k->clazz == AVAHI_DNS_CLASS_ANY;
}

guint avahi_key_hash(const AvahiKey *k) {
    g_assert(k);

    return
        avahi_domain_hash(k->name) + 
        k->type +
        k->clazz;
}

static gboolean rdata_equal(const AvahiRecord *a, const AvahiRecord *b) {
    g_assert(a);
    g_assert(b);
    g_assert(a->key->type == b->key->type);

/*     t = avahi_record_to_string(a); */
/*     g_message("comparing %s", t); */
/*     g_free(t); */

/*     t = avahi_record_to_string(b); */
/*     g_message("and %s", t); */
/*     g_free(t); */

    
    switch (a->key->type) {
        case AVAHI_DNS_TYPE_SRV:
            return
                a->data.srv.priority == b->data.srv.priority &&
                a->data.srv.weight == b->data.srv.weight &&
                a->data.srv.port == b->data.srv.port &&
                avahi_domain_equal(a->data.srv.name, b->data.srv.name);

        case AVAHI_DNS_TYPE_PTR:
        case AVAHI_DNS_TYPE_CNAME:
            return avahi_domain_equal(a->data.ptr.name, b->data.ptr.name);

        case AVAHI_DNS_TYPE_HINFO:
            return
                !strcmp(a->data.hinfo.cpu, b->data.hinfo.cpu) &&
                !strcmp(a->data.hinfo.os, b->data.hinfo.os);

        case AVAHI_DNS_TYPE_TXT:
            return avahi_string_list_equal(a->data.txt.string_list, b->data.txt.string_list);

        case AVAHI_DNS_TYPE_A:
            return memcmp(&a->data.a.address, &b->data.a.address, sizeof(AvahiIPv4Address)) == 0;

        case AVAHI_DNS_TYPE_AAAA:
            return memcmp(&a->data.aaaa.address, &b->data.aaaa.address, sizeof(AvahiIPv6Address)) == 0;

        default:
            return a->data.generic.size == b->data.generic.size &&
                (a->data.generic.size == 0 || memcmp(a->data.generic.data, b->data.generic.data, a->data.generic.size) == 0);
    }
    
}

gboolean avahi_record_equal_no_ttl(const AvahiRecord *a, const AvahiRecord *b) {
    g_assert(a);
    g_assert(b);

    if (a == b)
        return TRUE;

    return
        avahi_key_equal(a->key, b->key) &&
        rdata_equal(a, b);
}


AvahiRecord *avahi_record_copy(AvahiRecord *r) {
    AvahiRecord *copy;

    copy = g_new(AvahiRecord, 1);
    copy->ref = 1;
    copy->key = avahi_key_ref(r->key);
    copy->ttl = r->ttl;

    switch (r->key->type) {
        case AVAHI_DNS_TYPE_PTR:
        case AVAHI_DNS_TYPE_CNAME:
            copy->data.ptr.name = g_strdup(r->data.ptr.name);
            break;

        case AVAHI_DNS_TYPE_SRV:
            copy->data.srv.priority = r->data.srv.priority;
            copy->data.srv.weight = r->data.srv.weight;
            copy->data.srv.port = r->data.srv.port;
            copy->data.srv.name = g_strdup(r->data.srv.name);
            break;

        case AVAHI_DNS_TYPE_HINFO:
            copy->data.hinfo.os = g_strdup(r->data.hinfo.os);
            copy->data.hinfo.cpu = g_strdup(r->data.hinfo.cpu);
            break;

        case AVAHI_DNS_TYPE_TXT:
            copy->data.txt.string_list = avahi_string_list_copy(r->data.txt.string_list);
            break;

        case AVAHI_DNS_TYPE_A:
            copy->data.a.address = r->data.a.address;
            break;

        case AVAHI_DNS_TYPE_AAAA:
            copy->data.aaaa.address = r->data.aaaa.address;
            break;

        default:
            copy->data.generic.data = g_memdup(r->data.generic.data, r->data.generic.size);
            copy->data.generic.size = r->data.generic.size;
            break;
                
    }

    return copy;
}


guint avahi_key_get_estimate_size(AvahiKey *k) {
    g_assert(k);

    return strlen(k->name)+1+4;
}

guint avahi_record_get_estimate_size(AvahiRecord *r) {
    guint n;
    g_assert(r);

    n = avahi_key_get_estimate_size(r->key) + 4 + 2;

    switch (r->key->type) {
        case AVAHI_DNS_TYPE_PTR:
        case AVAHI_DNS_TYPE_CNAME:
            n += strlen(r->data.ptr.name) + 1;
            break;

        case AVAHI_DNS_TYPE_SRV:
            n += 6 + strlen(r->data.srv.name) + 1;
            break;

        case AVAHI_DNS_TYPE_HINFO:
            n += strlen(r->data.hinfo.os) + 1 + strlen(r->data.hinfo.cpu) + 1;
            break;

        case AVAHI_DNS_TYPE_TXT:
            n += avahi_string_list_serialize(r->data.txt.string_list, NULL, 0);
            break;

        case AVAHI_DNS_TYPE_A:
            n += sizeof(AvahiIPv4Address);
            break;

        case AVAHI_DNS_TYPE_AAAA:
            n += sizeof(AvahiIPv6Address);
            break;

        default:
            n += r->data.generic.size;
    }

    return n;
}

static gint lexicographical_memcmp(gconstpointer a, size_t al, gconstpointer b, size_t bl) {
    size_t c;
    gint ret;
    
    g_assert(a);
    g_assert(b);

    c = al < bl ? al : bl;
    if ((ret = memcmp(a, b, c)))
        return ret;

    if (al == bl)
        return 0;
    else
        return al == c ? 1 : -1;
}

static gint uint16_cmp(guint16 a, guint16 b) {
    return a == b ? 0 : (a < b ? -1 : 1);
}

gint avahi_record_lexicographical_compare(AvahiRecord *a, AvahiRecord *b) {
    gint r;
/*      gchar *t1, *t2; */

    g_assert(a);
    g_assert(b);

/*     t1 = avahi_record_to_string(a); */
/*     t2 = avahi_record_to_string(b); */
/*     g_message("lexicocmp: %s %s", t1, t2); */
/*     g_free(t1); */
/*     g_free(t2); */

    if (a == b)
        return 0;

    if ((r = uint16_cmp(a->key->clazz, b->key->clazz)) ||
        (r = uint16_cmp(a->key->type, b->key->type)))
        return r;

    switch (a->key->type) {

        case AVAHI_DNS_TYPE_PTR:
        case AVAHI_DNS_TYPE_CNAME:
            return avahi_binary_domain_cmp(a->data.ptr.name, b->data.ptr.name);

        case AVAHI_DNS_TYPE_SRV: {
            if ((r = uint16_cmp(a->data.srv.priority, b->data.srv.priority)) == 0 &&
                (r = uint16_cmp(a->data.srv.weight, b->data.srv.weight)) == 0 &&
                (r = uint16_cmp(a->data.srv.port, b->data.srv.port)) == 0)
                r = avahi_binary_domain_cmp(a->data.srv.name, b->data.srv.name);
            
            return r;
        }

        case AVAHI_DNS_TYPE_HINFO: {

            if ((r = strcmp(a->data.hinfo.cpu, b->data.hinfo.cpu)) ||
                (r = strcmp(a->data.hinfo.os, b->data.hinfo.os)))
                return r;

            return 0;

        }

        case AVAHI_DNS_TYPE_TXT: {

            guint8 *ma, *mb;
            guint asize, bsize;

            ma = g_new(guint8, asize = avahi_string_list_serialize(a->data.txt.string_list, NULL, 0));
            mb = g_new(guint8, bsize = avahi_string_list_serialize(b->data.txt.string_list, NULL, 0));
            avahi_string_list_serialize(a->data.txt.string_list, ma, asize);
            avahi_string_list_serialize(b->data.txt.string_list, mb, bsize);

            if (asize && bsize)
                r = lexicographical_memcmp(ma, asize, mb, bsize);
            else if (asize && !bsize)
                r = 1;
            else if (!asize && bsize)
                r = -1;
            else
                r = 0;
            
            g_free(ma);
            g_free(mb);

            return r;
        }
        
        case AVAHI_DNS_TYPE_A:
            return memcmp(&a->data.a.address, &b->data.a.address, sizeof(AvahiIPv4Address));

        case AVAHI_DNS_TYPE_AAAA:
            return memcmp(&a->data.aaaa.address, &b->data.aaaa.address, sizeof(AvahiIPv6Address));

        default:
            return lexicographical_memcmp(a->data.generic.data, a->data.generic.size,
                                          b->data.generic.data, b->data.generic.size);
    }
    
}

gboolean avahi_record_is_goodbye(AvahiRecord *r) {
    g_assert(r);

    return r->ttl == 0;
}
