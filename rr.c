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

    return k;
}

flxKey *flx_key_ref(flxKey *k) {
    g_assert(k);
    g_assert(k->ref >= 1);

    k->ref++;
    return k;
}

void flx_key_unref(flxKey *k) {
    g_assert(k);
    g_assert(k->ref >= 1);

    if ((--k->ref) <= 0) {
        g_free(k->name);
        g_free(k);
    }
}

flxRecord *flx_record_new(flxKey *k, gconstpointer data, guint16 size, guint32 ttl) {
    flxRecord *r;
    
    g_assert(k);
    g_assert(data);
    g_assert(size > 0);
    g_assert(ttl > 0);

    r = g_new(flxRecord, 1);
    r->ref = 1;
    r->key = flx_key_ref(k);
    r->data = g_memdup(data, size);
    r->size = size;
    r->ttl = ttl;

    return r;
}

flxRecord *flx_record_new_full(const gchar *name, guint16 class, guint16 type, gconstpointer data, guint16 size, guint32 ttl) {
    flxRecord *r;
    flxKey *k;
    
    k = flx_key_new(name, class, type);
    r = flx_record_new(k, data, size, ttl);
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
        flx_key_unref(r->key);
        g_free(r->data);
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
        default:
            return NULL;
    }
}


gchar *flx_key_to_string(flxKey *k) {
    return g_strdup_printf("%s\t%s\t%s",
                           k->name,
                           flx_dns_class_to_string(k->class),
                           flx_dns_type_to_string(k->type));
}

gchar *flx_record_to_string(flxRecord *r) {
    gchar *p, *s;
    char t[256] = "<unparsable>";

    if (r->key->type == FLX_DNS_TYPE_A)
        inet_ntop(AF_INET, r->data, t, sizeof(t));
    else if (r->key->type == FLX_DNS_TYPE_AAAA)
        inet_ntop(AF_INET6, r->data, t, sizeof(t));
    else if (r->key->type == FLX_DNS_TYPE_PTR || r->key->type == FLX_DNS_TYPE_TXT) {
        size_t l;
        
        l = r->size;
        if (l > sizeof(t)-1)
            l = sizeof(t)-1;
        
        memcpy(t, r->data, l);
        t[l] = 0;
    } else if (r->key->type == FLX_DNS_TYPE_HINFO) {
        char *s2;
        
        if ((s2 = memchr(r->data, 0, r->size))) {
            s2++;
            if (memchr(s2, 0, r->size - ((char*) s2 - (char*) r->data)))
                snprintf(t, sizeof(t), "'%s' '%s'", (char*) r->data, s2);
        }
    }

    p = flx_key_to_string(r->key);
    s = g_strdup_printf("%s %s", p, t);
    g_free(p);
    
    return s;
}

gboolean flx_key_equal(const flxKey *a, const flxKey *b) {
    g_assert(a);
    g_assert(b);
    
    return strcmp(a->name, b->name) == 0 && a->type == b->type && a->class == b->class;
}

guint flx_key_hash(const flxKey *k) {
    g_assert(k);

    return g_str_hash(k->name) + k->type + k->class;
}
