#ifndef fooaddresshfoo
#define fooaddresshfoo

#include <glib.h>

#include <sys/socket.h>

typedef struct {
    guint32 address;
} flxIPv4Address;

typedef struct {
    guint8 address[16];
} flxIPv6Address;

typedef struct {
    guchar family;

    union {
        flxIPv6Address ipv6;
        flxIPv4Address ipv4;
        guint8 data[0];
    } data;
} flxAddress;

guint flx_address_get_size(const flxAddress *a);
gint flx_address_cmp(const flxAddress *a, const flxAddress *b);

gchar *flx_address_snprint(char *ret_s, guint length, const flxAddress *a);

flxAddress *flx_address_parse(const char *s, guchar family, flxAddress *ret_addr);

flxAddress *flx_address_from_sockaddr(const struct sockaddr* sa, flxAddress *ret_addr);
guint16 flx_port_from_sockaddr(const struct sockaddr* sa);

gchar* flx_reverse_lookup_name_ipv4(const flxIPv4Address *a);
gchar* flx_reverse_lookup_name_ipv6_arpa(const flxIPv6Address *a);
gchar* flx_reverse_lookup_name_ipv6_int(const flxIPv6Address *a);

#endif
