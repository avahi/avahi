/* $Id$ */

/***
  This file is part of nss-mdns.
 
  nss-mdns is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
 
  nss-mdns is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with nss-mdns; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>

#include "dns.h"

struct dns_packet* dns_packet_new(void) {
    struct dns_packet *p;

    if (!(p = malloc(sizeof(struct dns_packet))))
        return NULL;
    
    p->size = p->rindex = 2*6;
    memset(p->data, 0, p->size);
    return p;
}

void dns_packet_free(struct dns_packet *p) {
    assert(p);
    free(p);
}

void dns_packet_set_field(struct dns_packet *p, unsigned idx, uint16_t v) {
    assert(p);
    assert(idx < 2*6);
    
    ((uint16_t*) p->data)[idx] = htons(v);
}

uint16_t dns_packet_get_field(struct dns_packet *p, unsigned idx) {
    assert(p);
    assert(idx < 2*6);

    return ntohs(((uint16_t*) p->data)[idx]);
}

uint8_t* dns_packet_append_name(struct dns_packet *p, const char *name) {
    uint8_t *d, *f = NULL;
    assert(p);

    for (;;) {
        size_t n = strcspn(name, ".");
        if (!n || n > 63)
            return NULL;
        
        d = dns_packet_extend(p, n+1);
        if (!f)
            f = d;
        
        d[0] = n;
        memcpy(d+1, name, n);

        name += n;

        /* no trailing dot */
        if (!*name)
            break;

        name ++;

        /* trailing dot */
        if (!*name)
            break;
    }

    d = dns_packet_extend(p, 1);
    d[0] = 0;

    return f;
}

uint8_t* dns_packet_append_uint16(struct dns_packet *p, uint16_t v) {
    uint8_t *d;
    assert(p);
    
    d = dns_packet_extend(p, sizeof(uint16_t));
    *((uint16_t*) d) = htons(v);
    
    return d;
}

uint8_t *dns_packet_extend(struct dns_packet *p, size_t l) {
    uint8_t *d;
    assert(p);

    assert(p->size+l <= sizeof(p->data));

    d = p->data + p->size;
    p->size += l;
    
    return d;
}

uint8_t *dns_packet_append_name_compressed(struct dns_packet *p, const char *name, uint8_t *prev) {
    int16_t *d;
    signed long k;
    assert(p);

    if (!prev)
        return dns_packet_append_name(p, name);
    
    k = prev - p->data;
    if (k < 0 || k >= 0x4000 || (size_t) k >= p->size)
        return dns_packet_append_name(p, name);

    d = (int16_t*) dns_packet_extend(p, sizeof(uint16_t));
    *d = htons((0xC000 | k));
    
    return prev;
}

int dns_packet_check_valid(struct dns_packet *p) {
    uint16_t flags;
    assert(p);

    if (p->size < 12)
        return -1;

    flags = dns_packet_get_field(p, DNS_FIELD_FLAGS);

    if (flags & DNS_FLAG_OPCODE || flags & DNS_FLAG_RCODE)
        return -1;

    return 0;
}

int dns_packet_check_valid_response(struct dns_packet *p) {
    uint16_t flags;
    assert(p);
    
    if (dns_packet_check_valid(p) < 0)
        return -1;

    flags = dns_packet_get_field(p, DNS_FIELD_FLAGS);

    if (!(flags & DNS_FLAG_QR))
        return -1;

    return 0;
}

static ssize_t consume_labels(struct dns_packet *p, size_t idx, char *ret_name, size_t l) {
    ssize_t ret = 0;
    int compressed = 0;
    int first_label = 1;
    assert(p && ret_name && l);
    
    for (;;) {
        uint8_t n;

        if (idx+1 > p->size)
            return -1;

        n = p->data[idx];

        if (!n) {
            idx++;
            if (!compressed)
                ret++;

            if (l < 1)
                return -1;
            *ret_name = 0;
            
            return ret;
            
        } else if (n <= 63) {
            /* Uncompressed label */
            idx++;
            if (!compressed)
                ret++;
        
            if (idx + n > p->size)
                return -1;

            if ((size_t) n + 1 > l)
                return -1;

            if (!first_label) {
                *(ret_name++) = '.';
                l--;
            } else
                first_label = 0;

            memcpy(ret_name, p->data + idx, n);
            idx += n;
            ret_name += n;
            l -= n;
            
            if (!compressed)
                ret += n;
        } else if ((n & 0xC0) == 0xC0) {
            /* Compressed label */

            if (idx+2 > p->size)
                return -1;

            idx = ((size_t) (p->data[idx] & ~0xC0)) << 8 | p->data[idx+1];

            if (!compressed)
                ret += 2;
            
            compressed = 1;
        } else
            return -1;
    }
}

int dns_packet_consume_name(struct dns_packet *p, char *ret_name, size_t l) {
    ssize_t r;
    
    if ((r = consume_labels(p, p->rindex, ret_name, l)) < 0)
        return -1;

    p->rindex += r;
    return 0;
}

int dns_packet_consume_uint16(struct dns_packet *p, uint16_t *ret_v) {
    assert(p && ret_v);

    if (p->rindex + sizeof(uint16_t) > p->size)
        return -1;

    *ret_v = ntohs(*((uint16_t*) (p->data + p->rindex)));
    p->rindex += sizeof(uint16_t);

    return 0;
}

int dns_packet_consume_uint32(struct dns_packet *p, uint32_t *ret_v) {
    assert(p && ret_v);

    if (p->rindex + sizeof(uint32_t) > p->size)
        return -1;

    *ret_v = ntohl(*((uint32_t*) (p->data + p->rindex)));
    p->rindex += sizeof(uint32_t);
    
    return 0;
}

int dns_packet_consume_bytes(struct dns_packet *p, void *ret_data, size_t l) {
    assert(p && ret_data && l > 0);
    
    if (p->rindex + l > p->size)
        return -1;

    memcpy(ret_data, p->data + p->rindex, l);
    p->rindex += l;

    return 0;
}

int dns_packet_consume_seek(struct dns_packet *p, size_t length) {
    assert(p);

    if (!length)
        return 0;
    
    if (p->rindex + length > p->size)
        return -1;

    p->rindex += length;
    return 0;
}
