#ifndef foodnshfoo
#define foodnshfoo

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

#include <glib.h>

#include "rr.h"

#define AVAHI_DNS_PACKET_MAX_SIZE 9000
#define AVAHI_DNS_PACKET_HEADER_SIZE 12

typedef struct AvahiDnsPacket {
    guint size, rindex, max_size;
    GHashTable *name_table; /* for name compression */
} AvahiDnsPacket;


#define AVAHI_DNS_PACKET_DATA(p) (((guint8*) p) + sizeof(AvahiDnsPacket))

AvahiDnsPacket* avahi_dns_packet_new(guint mtu);
AvahiDnsPacket* avahi_dns_packet_new_query(guint mtu);
AvahiDnsPacket* avahi_dns_packet_new_response(guint mtu, gboolean aa);

AvahiDnsPacket* avahi_dns_packet_new_reply(AvahiDnsPacket* p, guint mtu, gboolean copy_queries, gboolean aa);

void avahi_dns_packet_free(AvahiDnsPacket *p);
void avahi_dns_packet_set_field(AvahiDnsPacket *p, guint index, guint16 v);
guint16 avahi_dns_packet_get_field(AvahiDnsPacket *p, guint index);
void avahi_dns_packet_inc_field(AvahiDnsPacket *p, guint index);

guint8 *avahi_dns_packet_extend(AvahiDnsPacket *p, guint l);

guint8 *avahi_dns_packet_append_uint16(AvahiDnsPacket *p, guint16 v);
guint8 *avahi_dns_packet_append_uint32(AvahiDnsPacket *p, guint32 v);
guint8 *avahi_dns_packet_append_name(AvahiDnsPacket *p, const gchar *name);
guint8 *avahi_dns_packet_append_bytes(AvahiDnsPacket  *p, gconstpointer, guint l);
guint8* avahi_dns_packet_append_key(AvahiDnsPacket *p, AvahiKey *k, gboolean unicast_response);
guint8* avahi_dns_packet_append_record(AvahiDnsPacket *p, AvahiRecord *r, gboolean cache_flush, guint max_ttl);
guint8* avahi_dns_packet_append_string(AvahiDnsPacket *p, const gchar *s);

gint avahi_dns_packet_is_query(AvahiDnsPacket *p);
gint avahi_dns_packet_check_valid(AvahiDnsPacket *p);

gint avahi_dns_packet_consume_uint16(AvahiDnsPacket *p, guint16 *ret_v);
gint avahi_dns_packet_consume_uint32(AvahiDnsPacket *p, guint32 *ret_v);
gint avahi_dns_packet_consume_name(AvahiDnsPacket *p, gchar *ret_name, guint l);
gint avahi_dns_packet_consume_bytes(AvahiDnsPacket *p, gpointer ret_data, guint l);
AvahiKey* avahi_dns_packet_consume_key(AvahiDnsPacket *p, gboolean *ret_unicast_response);
AvahiRecord* avahi_dns_packet_consume_record(AvahiDnsPacket *p, gboolean *ret_cache_flush);
gint avahi_dns_packet_consume_string(AvahiDnsPacket *p, gchar *ret_string, guint l);

gconstpointer avahi_dns_packet_get_rptr(AvahiDnsPacket *p);

gint avahi_dns_packet_skip(AvahiDnsPacket *p, guint length);

gboolean avahi_dns_packet_is_empty(AvahiDnsPacket *p);
guint avahi_dns_packet_space(AvahiDnsPacket *p);

#define AVAHI_DNS_FIELD_ID 0
#define AVAHI_DNS_FIELD_FLAGS 1
#define AVAHI_DNS_FIELD_QDCOUNT 2
#define AVAHI_DNS_FIELD_ANCOUNT 3
#define AVAHI_DNS_FIELD_NSCOUNT 4
#define AVAHI_DNS_FIELD_ARCOUNT 5

#define AVAHI_DNS_FLAG_QR (1 << 15)
#define AVAHI_DNS_FLAG_OPCODE (15 << 11)
#define AVAHI_DNS_FLAG_RCODE (15)
#define AVAHI_DNS_FLAG_TC (1 << 9)
#define AVAHI_DNS_FLAG_AA (1 << 10)

#define AVAHI_DNS_FLAGS(qr, opcode, aa, tc, rd, ra, z, ad, cd, rcode) \
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

