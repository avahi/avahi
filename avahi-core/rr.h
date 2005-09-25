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

/** \file rr.h Functions and definitions for manipulating DNS resource record (RR) data. */

#include <avahi-common/strlst.h>
#include <avahi-common/address.h>
#include <avahi-common/cdecl.h>

#include <inttypes.h>
#include <sys/types.h>

AVAHI_C_DECL_BEGIN

/** DNS record types, see RFC 1035 */
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
    AVAHI_DNS_TYPE_ANY = 0xFF /**< Special query type for requesting all records */
};

/** DNS record classes, see RFC 1035 */
enum {
    AVAHI_DNS_CLASS_IN = 0x01,          /**< Probably the only class we will ever use */
    AVAHI_DNS_CLASS_ANY = 0xFF,         /**< Special query type for requesting all records */
    AVAHI_DNS_CACHE_FLUSH = 0x8000,     /**< Not really a class but a bit which may be set in response packets, see mDNS spec for more information */
    AVAHI_DNS_UNICAST_RESPONSE = 0x8000 /**< Not really a class but a bit which may be set in query packets, see mDNS spec for more information */
};

/** The default TTL for RRs which contain a host name of some kind. */
#define AVAHI_DEFAULT_TTL_HOST_NAME (120)

/** The default TTL for all other records. */
#define AVAHI_DEFAULT_TTL (75*60)

/** Encapsulates a DNS query key consisting of class, type and
    name. Use avahi_key_ref()/avahi_key_unref() for manipulating the
    reference counter. The structure is intended to be treated as "immutable", no
    changes should be imposed after creation */
typedef struct {
    int ref;           /**< Reference counter */
    char *name;        /**< Record name */
    uint16_t clazz;    /**< Record class, one of the AVAHI_DNS_CLASS_xxx constants */
    uint16_t type;     /**< Record type, one of the AVAHI_DNS_TYPE_xxx constants */
} AvahiKey;

/** Encapsulates a DNS resource record. The structure is intended to
 * be treated as "immutable", no changes should be imposed after
 * creation. */
typedef struct  {
    int ref;         /**< Reference counter */
    AvahiKey *key;   /**< Reference to the query key of this record */
    
    uint32_t ttl;     /**< DNS TTL of this record */

    union {
        
        struct {
            void* data;
            uint16_t size;
        } generic; /**< Generic record data for unknown types */
        
        struct {
            uint16_t priority;
            uint16_t weight;
            uint16_t port;
            char *name;
        } srv; /**< Data for SRV records */

        struct {
            char *name;
        } ptr; /**< Data for PTR an CNAME records */

        struct {
            char *cpu;
            char *os;
        } hinfo; /**< Data for HINFO records */

        struct {
            AvahiStringList *string_list;
        } txt; /**< Data for TXT records */

        struct {
            AvahiIPv4Address address;
        } a; /**< Data for A records */

        struct {
            AvahiIPv6Address address;
        } aaaa; /**< Data for AAAA records */

    } data; /**< Record data */
    
} AvahiRecord;

/** Create a new AvahiKey object. The reference counter will be set to 1. */
AvahiKey *avahi_key_new(const char *name, uint16_t clazz, uint16_t type);

/** Creaze new AvahiKey object based on an existing key but replaceing the type by CNAME */
AvahiKey *avahi_key_new_cname(AvahiKey *key);

/** Increase the reference counter of an AvahiKey object by one */
AvahiKey *avahi_key_ref(AvahiKey *k);

/** Decrease the reference counter of an AvahiKey object by one */
void avahi_key_unref(AvahiKey *k);

/** Check whether two AvahiKey object contain the same
 * data. AVAHI_DNS_CLASS_ANY/AVAHI_DNS_TYPE_ANY are treated like any
 * other class/type. */
int avahi_key_equal(const AvahiKey *a, const AvahiKey *b); 

/** Match a key to a key pattern. The pattern has a type of
AVAHI_DNS_CLASS_ANY, the classes are taken to be equal. Same for the
type. If the pattern has neither class nor type with ANY constants,
this function is identical to avahi_key_equal(). In contrast to
avahi_equal() this function is not commutative. */
int avahi_key_pattern_match(const AvahiKey *pattern, const AvahiKey *k);

/** Check whether a key is a pattern key, i.e. the class/type has a
 * value of AVAHI_DNS_CLASS_ANY/AVAHI_DNS_TYPE_ANY */
int avahi_key_is_pattern(const AvahiKey *k);

/** Return a numeric hash value for a key for usage in hash tables. */
unsigned avahi_key_hash(const AvahiKey *k);

/** Create a new record object. Record data should be filled in right after creation. The reference counter is set to 1. */
AvahiRecord *avahi_record_new(AvahiKey *k, uint32_t ttl);

/** Create a new record object. Record data should be filled in right after creation. The reference counter is set to 1. */
AvahiRecord *avahi_record_new_full(const char *name, uint16_t clazz, uint16_t type, uint32_t ttl);

/** Increase the reference counter of an AvahiRecord by one. */
AvahiRecord *avahi_record_ref(AvahiRecord *r);

/** Decrease the reference counter of an AvahiRecord by one. */
void avahi_record_unref(AvahiRecord *r);

/** Return a textual representation of the specified DNS class. The
 * returned pointer points to a read only internal string. */
const char *avahi_dns_class_to_string(uint16_t clazz);

/** Return a textual representation of the specified DNS class. The
 * returned pointer points to a read only internal string. */
const char *avahi_dns_type_to_string(uint16_t type);

/** Create a textual representation of the specified key. avahi_free() the
 * result! */
char *avahi_key_to_string(const AvahiKey *k);

/** Create a textual representation of the specified record, similar
 * in style to BIND zone file data. avahi_free() the result! */
char *avahi_record_to_string(const AvahiRecord *r); 

/** Check whether two records are equal (regardless of the TTL */
int avahi_record_equal_no_ttl(const AvahiRecord *a, const AvahiRecord *b);

/** Make a deep copy of an AvahiRecord object */
AvahiRecord *avahi_record_copy(AvahiRecord *r);

/** Returns a maximum estimate for the space that is needed to store
 * this key in a DNS packet. */
size_t avahi_key_get_estimate_size(AvahiKey *k);

/** Returns a maximum estimate for the space that is needed to store
 * the record in a DNS packet. */
size_t avahi_record_get_estimate_size(AvahiRecord *r);

/** Do a mDNS spec conforming lexicographical comparison of the two
 * records. Return a negative value if a < b, a positive if a > b,
 * zero if equal. */
int avahi_record_lexicographical_compare(AvahiRecord *a, AvahiRecord *b);

/** Return 1 if the specified record is an mDNS goodbye record. i.e. TTL is zero. */
int avahi_record_is_goodbye(AvahiRecord *r);

/** Check whether the specified key is valid */
int avahi_key_is_valid(AvahiKey *k);

/** Check whether the specified record is valid */
int avahi_record_is_valid(AvahiRecord *r);

AVAHI_C_DECL_END

#endif
