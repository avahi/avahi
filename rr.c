#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "util.h"
#include "rr.h"

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
    char buf[257], *t, *d = NULL;

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
    
    return strcmp(a->name, b->name) == 0 &&
        a->type == b->type &&
        a->class == b->class;
}

gboolean flx_key_pattern_match(const flxKey *pattern, const flxKey *k) {
    g_assert(pattern);
    g_assert(k);

/*     g_message("equal: %p %p", a, b); */

    g_assert(!flx_key_is_pattern(k));
    
    return strcmp(pattern->name, k->name) == 0 &&
        (pattern->type == k->type || pattern->type == FLX_DNS_TYPE_ANY) &&
        pattern->class == k->class;
}

gboolean flx_key_is_pattern(const flxKey *k) {
    g_assert(k);

    return k->type == FLX_DNS_TYPE_ANY;
}


guint flx_key_hash(const flxKey *k) {
    g_assert(k);

    return g_str_hash(k->name) + k->type + k->class;
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
                !strcmp(a->data.srv.name, b->data.srv.name);

        case FLX_DNS_TYPE_PTR:
        case FLX_DNS_TYPE_CNAME:
            return !strcmp(a->data.ptr.name, b->data.ptr.name);

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
