#ifndef foodnshfoo
#define foodnshfoo

#include <sys/types.h>
#include <inttypes.h>

struct dns_packet {
    size_t size, rindex;
    uint8_t data[9000];
};

struct dns_packet* dns_packet_new(void);
void dns_packet_free(struct dns_packet *p);
void dns_packet_set_field(struct dns_packet *p, unsigned index, uint16_t v);
uint16_t dns_packet_get_field(struct dns_packet *p, unsigned index);

uint8_t *dns_packet_append_uint16(struct dns_packet *p, uint16_t v);
uint8_t *dns_packet_append_name(struct dns_packet *p, const char *name);
uint8_t *dns_packet_append_name_compressed(struct dns_packet *p, const char *name, uint8_t *prev);
uint8_t *dns_packet_extend(struct dns_packet *p, size_t l);
int dns_packet_check_valid_response(struct dns_packet *p);
int dns_packet_check_valid(struct dns_packet *p);

int dns_packet_consume_name(struct dns_packet *p, char *ret_name, size_t l);
int dns_packet_consume_uint16(struct dns_packet *p, uint16_t *ret_v);
int dns_packet_consume_uint32(struct dns_packet *p, uint32_t *ret_v);
int dns_packet_consume_bytes(struct dns_packet *p, void *ret_data, size_t l);
int dns_packet_consume_seek(struct dns_packet *p, size_t length);

#define DNS_TYPE_A 0x01
#define DNS_TYPE_AAAA 0x1C
#define DNS_TYPE_PTR 0x0C
#define DNS_CLASS_IN 0x01

#define DNS_FIELD_ID 0
#define DNS_FIELD_FLAGS 1
#define DNS_FIELD_QDCOUNT 2
#define DNS_FIELD_ANCOUNT 3
#define DNS_FIELD_NSCOUNT 4
#define DNS_FIELD_ARCOUNT 5

#define DNS_FLAG_QR (1 << 15)
#define DNS_FLAG_OPCODE (15 << 11)
#define DNS_FLAG_RCODE (15)

#define DNS_FLAGS(qr, opcode, aa, tc, rd, ra, z, ad, cd, rcode) \
        (((uint16_t) !!qr << 15) |  \
         ((uint16_t) (opcode & 15) << 11) | \
         ((uint16_t) !!aa << 10) | \
         ((uint16_t) !!tc << 9) | \
         ((uint16_t) !!rd << 8) | \
         ((uint16_t) !!ra << 7) | \
         ((uint16_t) !!ad << 5) | \
         ((uint16_t) !!cd << 4) | \
         ((uint16_t) (rd & 15)))
         

#endif

