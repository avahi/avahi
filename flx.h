#ifndef fooflxhfoo
#define fooflxhfoo

#include <stdio.h>
#include <glib.h>

#include "address.h"

struct _flxServer;
typedef struct _flxServer flxServer;

typedef struct  {
    gchar *name;
    guint16 type;
    guint16 class;
    gpointer data;
    guint16 size;
    guint32 ttl;
} flxRecord;

typedef struct {
    gchar *name;
    guint16 type;
    guint16 class;
} flxQuery;

flxServer *flx_server_new(GMainContext *c);
void flx_server_free(flxServer* s);

gint flx_server_get_next_id(flxServer *s);

void flx_server_add_rr(flxServer *s, gint id, gint interface, guchar protocol, const flxRecord *rr);
void flx_server_add(flxServer *s, gint id, gint interface, guchar protocol, const gchar *name, guint16 type, gconstpointer data, guint size);
void flx_server_add_address(flxServer *s, gint id, gint interface, guchar protocol, const gchar *name, flxAddress *a);

void flx_server_remove(flxServer *s, gint id);

const flxRecord *flx_server_iterate(flxServer *s, gint id, void **state);

flxRecord *flx_record_copy_normalize(flxRecord *ret_dest, const flxRecord*src);

void flx_server_dump(flxServer *s, FILE *f);

struct _flxLocalAddrSource;
typedef struct _flxLocalAddrSource flxLocalAddrSource;

flxLocalAddrSource *flx_local_addr_source_new(flxServer *s);
void flx_local_addr_source_free(flxLocalAddrSource *l);

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

#endif
