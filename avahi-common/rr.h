#ifndef foorrhfoo
#define foorrhfoo

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

#include <avahi-common/strlst.h>
#include <avahi-common/address.h>

enum {
    AVAHI_DNS_TYPE_A = 0x01,
    AVAHI_DNS_TYPE_NS = 0x02,
    AVAHI_DNS_TYPE_CNAME = 0x05,
    AVAHI_DNS_TYPE_SOA = 0x06,
    AVAHI_DNS_TYPE_PTR = 0x0C,
    AVAHI_DNS_TYPE_HINFO = 0x0D,
    AVAHI_DNS_TYPE_MX = 0x0F,
    AVAHI_DNS_TYPE_TXT = 0x10,
    AVAHI_DNS_TYPE_AAAA = 0x1C,
    AVAHI_DNS_TYPE_SRV = 0x21,
    AVAHI_DNS_TYPE_ANY = 0xFF
};

enum {
    AVAHI_DNS_CLASS_IN = 0x01,
    AVAHI_DNS_CLASS_ANY = 0xFF,
    AVAHI_DNS_CACHE_FLUSH = 0x8000,
    AVAHI_DNS_UNICAST_RESPONSE = 0x8000
};

#define AVAHI_DEFAULT_TTL (120*60)

typedef struct {
    guint ref;
    gchar *name;
    guint16 class;
    guint16 type;
} AvahiKey;

typedef struct  {
    guint ref;
    AvahiKey *key;
    
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
            AvahiStringList *string_list;
        } txt;

        struct {
            AvahiIPv4Address address;
        } a;

        struct {
            AvahiIPv6Address address;
        } aaaa;

    } data;
    
} AvahiRecord;

AvahiKey *avahi_key_new(const gchar *name, guint16 class, guint16 type);
AvahiKey *avahi_key_ref(AvahiKey *k);
void avahi_key_unref(AvahiKey *k);

gboolean avahi_key_equal(const AvahiKey *a, const AvahiKey *b);  /* Treat AVAHI_DNS_CLASS_ANY like any other type */
gboolean avahi_key_pattern_match(const AvahiKey *pattern, const AvahiKey *k); /* If pattern.type is AVAHI_DNS_CLASS_ANY, k.type is ignored */

gboolean avahi_key_is_pattern(const AvahiKey *k);

guint avahi_key_hash(const AvahiKey *k);

AvahiRecord *avahi_record_new(AvahiKey *k);
AvahiRecord *avahi_record_new_full(const gchar *name, guint16 class, guint16 type);
AvahiRecord *avahi_record_ref(AvahiRecord *r);
void avahi_record_unref(AvahiRecord *r);

const gchar *avahi_dns_class_to_string(guint16 class);
const gchar *avahi_dns_type_to_string(guint16 type);

gchar *avahi_key_to_string(const AvahiKey *k); /* g_free() the result! */
gchar *avahi_record_to_string(const AvahiRecord *r);  /* g_free() the result! */

gboolean avahi_record_equal_no_ttl(const AvahiRecord *a, const AvahiRecord *b);

AvahiRecord *avahi_record_copy(AvahiRecord *r);

/* returns a maximum estimate for the space that is needed to store
 * this key in a DNS packet */
guint avahi_key_get_estimate_size(AvahiKey *k);

/* ditto */
guint avahi_record_get_estimate_size(AvahiRecord *r);

gint avahi_record_lexicographical_compare(AvahiRecord *a, AvahiRecord *b);

gboolean avahi_record_is_goodbye(AvahiRecord *r);

#endif
