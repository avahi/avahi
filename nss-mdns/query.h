#ifndef fooqueryhfoo
#define fooqueryhfoo

#include <inttypes.h>

typedef struct {
    uint32_t address;
} ipv4_address_t;

typedef struct {
    uint8_t address[16];
} ipv6_address_t;

int mdns_open_socket(void);

int mdns_query_name(int fd,
               const char *name,
               void (*ipv4_func)(const ipv4_address_t *ipv4, void *userdata),
               void (*ipv6_func)(const ipv6_address_t *ipv6, void *userdata),
               void *userdata);

int mdns_query_ipv4(int fd,
               const ipv4_address_t *ipv4,
               void (*name_func)(const char *name, void *userdata),
               void *userdata);

int mdns_query_ipv6(int fd,
               const ipv6_address_t *ipv6,
               void (*name_func)(const char *name, void *userdata),
               void *userdata);

#endif
