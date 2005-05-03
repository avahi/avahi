#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "util.h"
#include "rr.h"
#include "dns.h"

flxKey *flx_key_new(const gchar *name, guint16 class, guint16 type) {
    flxKey *k;
    g_assert(name);

    k = g_new(flxKey, 1);
    k->ref = 1;
    k->name = flx_normalize_name(name);    
    k->class = class;
    k->type = type;

/*     g_message("%p %% ref=1", k); */
    
    return k;
}

flxKey *flx_key_ref(flxKey *k) {
    g_assert(k);
    g_assert(k->ref >= 1);

    k->ref++;

/*     g_message("%p ++ ref=%i", k, k->ref); */

    return k;
}

void flx_key_unref(flxKey *k) {
    g_assert(k);
    g_assert(k->ref >= 1);

/*     g_message("%p -- ref=%i", k, k->ref-1); */
    
    if ((--k->ref) <= 0) {
        g_free(k->name);
        g_free(k);
    }
}

flxRecord *flx_record_new(flxKey *k) {
    flxRecord *r;
    
    g_assert(k);
    
    r = g_new(flxRecord, 1);
    r->ref = 1;
    r->key = flx_key_ref(k);

    memset(&r->data, 0, sizeof(r->data));

    r->ttl = FLX_DEFAULT_TTL;

    return r;
}

flxRecord *flx_record_new_full(const gchar *name, guint16 class, guint16 type) {
    flxRecord *r;
    flxKey *k;

    g_assert(name);
    
    k = flx_key_new(name, class, type);
    r = flx_record_new(k);
    flx_key_unref(k);

    return r;
}

flxRecord *flx_record_ref(flxRecord *r) {
    g_assert(r);
    g_assert(r->ref >= 1);

    r->ref++;
    return r;
}

void flx_record_unref(flxRecord *r) {
    g_assert(r);
    g_assert(r->ref >= 1);

    if ((--r->ref) <= 0) {
        switch (r->key->type) {

            case FLX_DNS_TYPE_SRV:
                g_free(r->data.srv.name);
                break;

            case FLX_DNS_TYPE_PTR:
            case FLX_DNS_TYPE_CNAME:
                g_free(r->data.ptr.name);
                break;

            case FLX_DNS_TYPE_HINFO:
                g_free(r->data.hinfo.cpu);
                g_free(r->data.hinfo.os);
                break;

            case FLX_DNS_TYPE_TXT:
                flx_string_list_free(r->data.txt.string_list);
                break;

            case FLX_DNS_TYPE_A:
            case FLX_DNS_TYPE_AAAA:
                break;
            
            default:
                g_free(r->data.generic.data);
        }
        
        flx_key_unref(r->key);
        g_free(r);
    }
}

const gchar *flx_dns_class_to_string(guint16 class) {
    if (class & FLX_DNS_CACHE_FLUSH) 
        return "FLUSH";
    
    if (class == FLX_DNS_CLASS_IN)
        return "IN";

    return NULL;
}

const gchar *flx_dns_type_to_string(guint16 type) {
    switch (type) {
        case FLX_DNS_TYPE_CNAME:
            return "CNAME";
        case FLX_DNS_TYPE_A:
            return "A";
        case FLX_DNS_TYPE_AAAA:
            return "AAAA";
        case FLX_DNS_TYPE_PTR:
            return "PTR";
        case FLX_DNS_TYPE_HINFO:
            return "HINFO";
        case FLX_DNS_TYPE_TXT:
            return "TXT";
        case FLX_DNS_TYPE_SRV:
            return "SRV";
        case FLX_DNS_TYPE_ANY:
            return "ANY";
        default:
            return NULL;
    }
}


gchar *flx_key_to_string(const flxKey *k) {
    return g_strdup_printf("%s\t%s\t%s",
                           k->name,
                           flx_dns_class_to_string(k->class),
                           flx_dns_type_to_string(k->type));
}

gchar *flx_record_to_string(const flxRecord *r) {
    gchar *p, *s;
    char buf[257], *t = NULL, *d = NULL;

    switch (r->key->type) {
        case FLX_DNS_TYPE_A:
            inet_ntop(AF_INET, &r->data.a.address.address, t = buf, sizeof(buf));
            break;
            
        case FLX_DNS_TYPE_AAAA:
            inet_ntop(AF_INET6, &r->data.aaaa.address.address, t = buf, sizeof(buf));
            break;
            
        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME :

            t = r->data.ptr.name;
            break;

        case FLX_DNS_TYPE_TXT:
            t = d = flx_string_list_to_string(r->data.txt.string_list);
            break;

        case FLX_DNS_TYPE_HINFO:

            snprintf(t = buf, sizeof(buf), "\"%s\" \"%s\"", r->data.hinfo.cpu, r->data.hinfo.os);
            break;

        case FLX_DNS_TYPE_SRV:

            snprintf(t = buf, sizeof(buf), "%u %u %u %s",
                     r->data.srv.priority,
                     r->data.srv.weight,
                     r->data.srv.port,
                     r->data.srv.name);

            break;
    }

    p = flx_key_to_string(r->key);
    s = g_strdup_printf("%s %s ; ttl=%u", p, t ? t : "<unparsable>", r->ttl);
    g_free(p);
    g_free(d);
    
    return s;
}

gboolean flx_key_equal(const flxKey *a, const flxKey *b) {
    g_assert(a);
    g_assert(b);

/*     g_message("equal: %p %p", a, b); */
    
    return flx_domain_equal(a->name, b->name) &&
        a->type == b->type &&
        a->class == b->class;
}

gboolean flx_key_pattern_match(const flxKey *pattern, const flxKey *k) {
    g_assert(pattern);
    g_assert(k);

/*     g_message("equal: %p %p", a, b); */

    g_assert(!flx_key_is_pattern(k));
    
    return flx_domain_equal(pattern->name, k->name) &&
        (pattern->type == k->type || pattern->type == FLX_DNS_TYPE_ANY) &&
        pattern->class == k->class;
}

gboolean flx_key_is_pattern(const flxKey *k) {
    g_assert(k);

    return k->type == FLX_DNS_TYPE_ANY;
}


guint flx_key_hash(const flxKey *k) {
    g_assert(k);

    return flx_domain_hash(k->name) + k->type + k->class;
}

static gboolean rdata_equal(const flxRecord *a, const flxRecord *b) {
    g_assert(a);
    g_assert(b);
    g_assert(a->key->type == b->key->type);

/*     t = flx_record_to_string(a); */
/*     g_message("comparing %s", t); */
/*     g_free(t); */

/*     t = flx_record_to_string(b); */
/*     g_message("and %s", t); */
/*     g_free(t); */

    
    switch (a->key->type) {
        case FLX_DNS_TYPE_SRV:
            return
                a->data.srv.priority == b->data.srv.priority &&
                a->data.srv.weight == b->data.srv.weight &&
                a->data.srv.port == b->data.srv.port &&
                flx_domain_equal(a->data.srv.name, b->data.srv.name);

        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME:
            return flx_domain_equal(a->data.ptr.name, b->data.ptr.name);

        case FLX_DNS_TYPE_HINFO:
            return
                !strcmp(a->data.hinfo.cpu, b->data.hinfo.cpu) &&
                !strcmp(a->data.hinfo.os, b->data.hinfo.os);

        case FLX_DNS_TYPE_TXT:
            return flx_string_list_equal(a->data.txt.string_list, b->data.txt.string_list);

        case FLX_DNS_TYPE_A:
            return memcmp(&a->data.a.address, &b->data.a.address, sizeof(flxIPv4Address)) == 0;

        case FLX_DNS_TYPE_AAAA:
            return memcmp(&a->data.aaaa.address, &b->data.aaaa.address, sizeof(flxIPv6Address)) == 0;

        default:
            return a->data.generic.size == b->data.generic.size &&
                (a->data.generic.size == 0 || memcmp(a->data.generic.data, b->data.generic.data, a->data.generic.size) == 0);
    }
    
}

gboolean flx_record_equal_no_ttl(const flxRecord *a, const flxRecord *b) {
    g_assert(a);
    g_assert(b);

    return
        flx_key_equal(a->key, b->key) &&
        rdata_equal(a, b);
}


flxRecord *flx_record_copy(flxRecord *r) {
    flxRecord *copy;

    copy = g_new(flxRecord, 1);
    copy->ref = 1;
    copy->key = flx_key_ref(r->key);
    copy->ttl = r->ttl;

    switch (r->key->type) {
        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME:
            copy->data.ptr.name = g_strdup(r->data.ptr.name);
            break;

        case FLX_DNS_TYPE_SRV:
            copy->data.srv.priority = r->data.srv.priority;
            copy->data.srv.weight = r->data.srv.weight;
            copy->data.srv.port = r->data.srv.port;
            copy->data.srv.name = g_strdup(r->data.srv.name);
            break;

        case FLX_DNS_TYPE_HINFO:
            copy->data.hinfo.os = g_strdup(r->data.hinfo.os);
            copy->data.hinfo.cpu = g_strdup(r->data.hinfo.cpu);
            break;

        case FLX_DNS_TYPE_TXT:
            copy->data.txt.string_list = flx_string_list_copy(r->data.txt.string_list);
            break;

        case FLX_DNS_TYPE_A:
            copy->data.a.address = r->data.a.address;
            break;

        case FLX_DNS_TYPE_AAAA:
            copy->data.aaaa.address = r->data.aaaa.address;
            break;

        default:
            copy->data.generic.data = g_memdup(r->data.generic.data, r->data.generic.size);
            copy->data.generic.size = r->data.generic.size;
            break;
                
    }

    return copy;
}


guint flx_key_get_estimate_size(flxKey *k) {
    g_assert(k);

    return strlen(k->name)+1+4;
}

guint flx_record_get_estimate_size(flxRecord *r) {
    guint n;
    g_assert(r);

    n = flx_key_get_estimate_size(r->key) + 4 + 2;

    switch (r->key->type) {
        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME:
            n += strlen(r->data.ptr.name) + 1;
            break;

        case FLX_DNS_TYPE_SRV:
            n += 6 + strlen(r->data.srv.name) + 1;
            break;

        case FLX_DNS_TYPE_HINFO:
            n += strlen(r->data.hinfo.os) + 1 + strlen(r->data.hinfo.cpu) + 1;
            break;

        case FLX_DNS_TYPE_TXT:
            n += flx_string_list_serialize(r->data.txt.string_list, NULL, 0);
            break;

        case FLX_DNS_TYPE_A:
            n += sizeof(flxIPv4Address);
            break;

        case FLX_DNS_TYPE_AAAA:
            n += sizeof(flxIPv6Address);
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
    if ((ret = memcmp(a, b, c)) != 0)
        return ret;

    if (al == bl)
        return 0;
    else
        return al == c ? 1 : -1;
}

static gint uint16_cmp(guint16 a, guint16 b) {
    return a == b ? 0 : (a < b ? a : b);
}

static gint lexicographical_domain_cmp(const gchar *a, const gchar *b) {
    g_assert(a);
    g_assert(b);
    

    for (;;) {
        gchar t1[64];
        gchar t2[64];
        size_t al, bl;
        gint r;

        if (!a && !b)
            return 0;

        if (a && !b)
            return 1;

        if (b && !a)
            return -1;
        
        flx_unescape_label(t1, sizeof(t1), &a);
        flx_unescape_label(t2, sizeof(t2), &b);

        al = strlen(t1);
        bl = strlen(t2);
        
        if (al != bl) 
            return al < bl ? -1 : 1;

        if ((r =  strcmp(t1, t2)) != 0)
            return r;
    }
}

gint flx_record_lexicographical_compare(flxRecord *a, flxRecord *b) {
    g_assert(a);
    g_assert(b);

    if (a->key->class < b->key->class)
        return -1;
    else if (a->key->class > b->key->class)
        return 1;

    if (a->key->type < b->key->type)
        return -1;
    else if (a->key->type > b->key->type)
        return 1;

    switch (a->key->type) {

        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME:
            return lexicographical_domain_cmp(a->data.ptr.name, b->data.ptr.name);

        case FLX_DNS_TYPE_SRV: {
            gint r;
            if ((r = uint16_cmp(a->data.srv.priority, b->data.srv.priority)) != 0 ||
                (r = uint16_cmp(a->data.srv.weight, b->data.srv.weight)) != 0 ||
                (r = uint16_cmp(a->data.srv.port, b->data.srv.port)) != 0)
                r = lexicographical_domain_cmp(a->data.srv.name, b->data.srv.name);
            
            return r;
        }

        case FLX_DNS_TYPE_HINFO: {
            size_t al = strlen(a->data.hinfo.cpu), bl = strlen(b->data.hinfo.cpu);
            gint r;

            if (al != bl)
                return al < bl ? -1 : 1;

            if ((r = strcmp(a->data.hinfo.cpu, b->data.hinfo.cpu)) != 0)
                return r;

            al = strlen(a->data.hinfo.os), bl = strlen(b->data.hinfo.os);

            if (al != bl)
                return al < bl ? -1 : 1;

            if ((r = strcmp(a->data.hinfo.os, b->data.hinfo.os)) != 0)
                return r;

            return 0;

        }

        case FLX_DNS_TYPE_TXT: {

            guint8 *ma, *mb;
            guint asize, bsize;
            gint r;

            ma = g_new(guint8, asize = flx_string_list_serialize(a->data.txt.string_list, NULL, 0));
            mb = g_new(guint8, bsize = flx_string_list_serialize(b->data.txt.string_list, NULL, 0));
            flx_string_list_serialize(a->data.txt.string_list, ma, asize);
            flx_string_list_serialize(a->data.txt.string_list, mb, bsize);

            r = lexicographical_memcmp(ma, asize, mb, bsize);
            g_free(ma);
            g_free(mb);

            return r;
        }
        
        case FLX_DNS_TYPE_A:
            return memcmp(&a->data.a.address, &b->data.a.address, sizeof(flxIPv4Address));

        case FLX_DNS_TYPE_AAAA:
            return memcmp(&a->data.aaaa.address, &b->data.aaaa.address, sizeof(flxIPv6Address));

        default:
            return lexicographical_memcmp(a->data.generic.data, a->data.generic.size,
                                          b->data.generic.data, b->data.generic.size);
    }
    
}
