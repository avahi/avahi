#ifndef foorrhfoo
#define foorrhfoo

#include <glib.h>

#include "strlst.h"
#include "address.h"

enum {
    FLX_DNS_TYPE_A = 0x01,
    FLX_DNS_TYPE_NS = 0x02,
    FLX_DNS_TYPE_CNAME = 0x05,
    FLX_DNS_TYPE_SOA = 0x06,
    FLX_DNS_TYPE_PTR = 0x0C,
    FLX_DNS_TYPE_HINFO = 0x0D,
    FLX_DNS_TYPE_MX = 0x0F,
    FLX_DNS_TYPE_TXT = 0x10,
    FLX_DNS_TYPE_AAAA = 0x1C,
    FLX_DNS_TYPE_SRV = 0x21,
    FLX_DNS_TYPE_ANY = 0xFF
};

enum {
    FLX_DNS_CLASS_IN = 0x01
};

#define FLX_DEFAULT_TTL (120*60)

typedef struct {
    guint ref;
    gchar *name;
    guint16 class;
    guint16 type;
} flxKey;

typedef struct  {
    guint ref;
    flxKey *key;
    
    guint32 ttl;

    union {
        struct {
            gpointer data;
            guint16 size;
        } generic;

        struct {
            guint16 priority;
            guint16 weight;
            guint16 port;
            gchar *name;
        } srv;

        struct {
            gchar *name;
        } ptr; /* and cname */

        struct {
            gchar *cpu;
            gchar *os;
        } hinfo;

        struct {
            flxStringList *string_list;
        } txt;

        struct {
            flxIPv4Address address;
        } a;

        struct {
            flxIPv6Address address;
        } aaaa;

    } data;
    
} flxRecord;

flxKey *flx_key_new(const gchar *name, guint16 class, guint16 type);
flxKey *flx_key_ref(flxKey *k);
void flx_key_unref(flxKey *k);

gboolean flx_key_equal(const flxKey *a, const flxKey *b);  /* Treat FLX_DNS_CLASS_ANY like any other type */
gboolean flx_key_pattern_match(const flxKey *pattern, const flxKey *k); /* If pattern.type is FLX_DNS_CLASS_ANY, k.type is ignored */

gboolean flx_key_is_pattern(const flxKey *k);

guint flx_key_hash(const flxKey *k);

flxRecord *flx_record_new(flxKey *k);
flxRecord *flx_record_new_full(const gchar *name, guint16 class, guint16 type);
flxRecord *flx_record_ref(flxRecord *r);
void flx_record_unref(flxRecord *r);

const gchar *flx_dns_class_to_string(guint16 class);
const gchar *flx_dns_type_to_string(guint16 type);

gchar *flx_key_to_string(const flxKey *k); /* g_free() the result! */
gchar *flx_record_to_string(const flxRecord *r);  /* g_free() the result! */

gboolean flx_record_equal_no_ttl(const flxRecord *a, const flxRecord *b);

flxRecord *flx_record_copy(flxRecord *r);

#endif
