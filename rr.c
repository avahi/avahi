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

flxRecord *flx_record_new(flxKey *k, gconstpointer data, guint16 size, guint32 ttl) {
    flxRecord *r;
    
    g_assert(k);
    g_assert(size == 0 || data);
    
    r = g_new(flxRecord, 1);
    r->ref = 1;
    r->key = flx_key_ref(k);
    r->data = size > 0 ? g_memdup(data, size) : NULL;
    r->size = size;
    r->ttl = ttl;

    return r;
}

flxRecord *flx_record_new_full(const gchar *name, guint16 class, guint16 type, gconstpointer data, guint16 size, guint32 ttl) {
    flxRecord *r;
    flxKey *k;

    g_assert(name);
    g_assert(size == 0 || data);
    
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


gchar *flx_key_to_string(flxKey *k) {
    return g_strdup_printf("%s\t%s\t%s",
                           k->name,
                           flx_dns_class_to_string(k->class),
                           flx_dns_type_to_string(k->type));
}

gchar *flx_record_to_string(flxRecord *r) {
    gchar *p, *s;
    char t[257] = "<unparsable>";

    switch (r->key->type) {
        case FLX_DNS_TYPE_A:
            inet_ntop(AF_INET, r->data, t, sizeof(t));
            break;
            
        case FLX_DNS_TYPE_AAAA:
            inet_ntop(AF_INET6, r->data, t, sizeof(t));
            break;
            
        case FLX_DNS_TYPE_PTR: {
            size_t l;
        
            l = r->size;
            if (l > sizeof(t)-1)
                l = sizeof(t)-1;
            
            memcpy(t, r->data, l);
            t[l] = 0;
            break;
        }

        case FLX_DNS_TYPE_TXT: {

            if (r->size == 0)
                t[0] = 0;
            else {
                guchar l = ((guchar*) r->data)[0];

                if ((size_t) l+1 <= r->size) {
                    memcpy(t, r->data+1, ((guchar*) r->data)[0]);
                    t[((guchar*) r->data)[0]] = 0;
                }
            }
            break;
        }

        case FLX_DNS_TYPE_HINFO: {
            gchar *s2;
            gchar hi1[256], hi2[256];
            guchar len;

            if ((size_t) (len = ((guchar*) r->data)[0]) + 2 <= r->size) {
                guchar len2;
                memcpy(hi1, (gchar*) r->data +1, len);
                hi1[len] = 0;

                if ((size_t) (len2 = ((guchar*) r->data)[len+1]) + len + 2 <= r->size) {
                    memcpy(hi2, (gchar*) r->data+len+2, len2);
                    hi2[len2] = 0;
                    snprintf(t, sizeof(t), "'%s' '%s'", hi1, hi2);
                }
                
            }

            break;
        }

        case FLX_DNS_TYPE_SRV: {
            char k[257];
            size_t l;

            l = r->size-6;
            if (l > sizeof(k)-1)
                l = sizeof(k)-1;
            
            memcpy(k, r->data+6, l);
            k[l] = 0;
            
            snprintf(t, sizeof(t), "%u %u %u %s",
                     ntohs(((guint16*) r->data)[0]),
                     ntohs(((guint16*) r->data)[1]),
                     ntohs(((guint16*) r->data)[2]),
                     k);
            break;
        }
    }

    p = flx_key_to_string(r->key);
    s = g_strdup_printf("%s %s ; ttl=%u", p, t, r->ttl);
    g_free(p);
    
    return s;
}

gboolean flx_key_equal(const flxKey *a, const flxKey *b) {
    g_assert(a);
    g_assert(b);

/*     g_message("equal: %p %p", a, b); */
    
    return strcmp(a->name, b->name) == 0 && a->type == b->type && a->class == b->class;
}

guint flx_key_hash(const flxKey *k) {
    g_assert(k);

    return g_str_hash(k->name) + k->type + k->class;
}

gboolean flx_record_equal_no_ttl(const flxRecord *a, const flxRecord *b) {
    g_assert(a);
    g_assert(b);

    return flx_key_equal(a->key, b->key) &&
/*        a->ttl == b->ttl && */
        a->size == b->size &&
        (a->size == 0 || memcmp(a->data, b->data, a->size) == 0);
}
