#ifndef foorrhfoo
#define foorrhfoo

#include <glib.h>

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
    
    gpointer data;
    guint16 size;
    guint32 ttl;
} flxRecord;

flxKey *flx_key_new(const gchar *name, guint16 class, guint16 type);
flxKey *flx_key_ref(flxKey *k);
void flx_key_unref(flxKey *k);

gboolean flx_key_equal(const flxKey *a, const flxKey *b);
guint flx_key_hash(const flxKey *k);

flxRecord *flx_record_new(flxKey *k, gconstpointer data, guint16 size, guint32 ttl);
flxRecord *flx_record_new_full(const gchar *name, guint16 class, guint16 type, gconstpointer data, guint16 size, guint32 ttl);
flxRecord *flx_record_ref(flxRecord *r);
void flx_record_unref(flxRecord *r);

const gchar *flx_dns_class_to_string(guint16 class);
const gchar *flx_dns_type_to_string(guint16 type);

gchar *flx_key_to_string(flxKey *k); /* g_free() the result! */
gchar *flx_record_to_string(flxRecord *r);  /* g_free() the result! */

#endif
