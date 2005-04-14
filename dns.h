#ifndef foodnshfoo
#define foodnshfoo

#include <glib.h>

#include "rr.h"

#define FLX_DNS_PACKET_MAX_SIZE 9000
#define FLX_DNS_PACKET_HEADER_SIZE 12

typedef struct _flxDnsPacket {
    guint size, rindex, max_size;
} flxDnsPacket;


#define FLX_DNS_PACKET_DATA(p) (((guint8*) p) + sizeof(flxDnsPacket))

flxDnsPacket* flx_dns_packet_new(guint size);
flxDnsPacket* flx_dns_packet_new_query(guint size);
flxDnsPacket* flx_dns_packet_new_response(guint size);

void flx_dns_packet_free(flxDnsPacket *p);
void flx_dns_packet_set_field(flxDnsPacket *p, guint index, guint16 v);
guint16 flx_dns_packet_get_field(flxDnsPacket *p, guint index);

guint8 *flx_dns_packet_extend(flxDnsPacket *p, guint l);

guint8 *flx_dns_packet_append_uint16(flxDnsPacket *p, guint16 v);
guint8 *flx_dns_packet_append_uint32(flxDnsPacket *p, guint32 v);
guint8 *flx_dns_packet_append_name(flxDnsPacket *p, const gchar *name);
guint8 *flx_dns_packet_append_name_compressed(flxDnsPacket *p, const gchar *name, guint8 *prev);
guint8 *flx_dns_packet_append_bytes(flxDnsPacket  *p, gconstpointer, guint l);
guint8* flx_dns_packet_append_key(flxDnsPacket *p, flxKey *k);
guint8* flx_dns_packet_append_record(flxDnsPacket *p, flxRecord *r, gboolean cache_flush);
guint8* flx_dns_packet_append_string(flxDnsPacket *p, const gchar *s);

gint flx_dns_packet_is_query(flxDnsPacket *p);
gint flx_dns_packet_check_valid(flxDnsPacket *p);

gint flx_dns_packet_consume_uint16(flxDnsPacket *p, guint16 *ret_v);
gint flx_dns_packet_consume_uint32(flxDnsPacket *p, guint32 *ret_v);
gint flx_dns_packet_consume_name(flxDnsPacket *p, gchar *ret_name, guint l);
gint flx_dns_packet_consume_bytes(flxDnsPacket *p, gpointer ret_data, guint l);
flxKey* flx_dns_packet_consume_key(flxDnsPacket *p);
flxRecord* flx_dns_packet_consume_record(flxDnsPacket *p, gboolean *ret_cache_flush);
gint flx_dns_packet_consume_string(flxDnsPacket *p, gchar *ret_string, guint l);

gconstpointer flx_dns_packet_get_rptr(flxDnsPacket *p);

gint flx_dns_packet_skip(flxDnsPacket *p, guint length);

gboolean flx_dns_packet_is_empty(flxDnsPacket *p);
guint flx_dns_packet_space(flxDnsPacket *p);

#define FLX_DNS_FIELD_ID 0
#define FLX_DNS_FIELD_FLAGS 1
#define FLX_DNS_FIELD_QDCOUNT 2
#define FLX_DNS_FIELD_ANCOUNT 3
#define FLX_DNS_FIELD_NSCOUNT 4
#define FLX_DNS_FIELD_ARCOUNT 5

#define FLX_DNS_FLAG_QR (1 << 15)
#define FLX_DNS_FLAG_OPCODE (15 << 11)
#define FLX_DNS_FLAG_RCODE (15)
#define FLX_DNS_FLAG_TC (1 << 9)

#define FLX_DNS_FLAGS(qr, opcode, aa, tc, rd, ra, z, ad, cd, rcode) \
        (((guint16) !!qr << 15) |  \
         ((guint16) (opcode & 15) << 11) | \
         ((guint16) !!aa << 10) | \
         ((guint16) !!tc << 9) | \
         ((guint16) !!rd << 8) | \
         ((guint16) !!ra << 7) | \
         ((guint16) !!ad << 5) | \
         ((guint16) !!cd << 4) | \
         ((guint16) (rd & 15)))
         

#endif

