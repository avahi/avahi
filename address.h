#ifndef fooaddresshfoo
#define fooaddresshfoo

#include <glib.h>

#include <sys/socket.h>

typedef struct {
    guint32 address;
} AvahiIPv4Address;

typedef struct {
    guint8 address[16];
} AvahiIPv6Address;

typedef struct {
    guchar family;

    union {
        AvahiIPv6Address ipv6;
        AvahiIPv4Address ipv4;
        guint8 data[0];
    } data;
} AvahiAddress;

guint avahi_address_get_size(const AvahiAddress *a);
gint avahi_address_cmp(const AvahiAddress *a, const AvahiAddress *b);

gchar *avahi_address_snprint(char *ret_s, guint length, const AvahiAddress *a);

AvahiAddress *avahi_address_parse(const char *s, guchar family, AvahiAddress *ret_addr);

AvahiAddress *avahi_address_from_sockaddr(const struct sockaddr* sa, AvahiAddress *ret_addr);
guint16 avahi_port_from_sockaddr(const struct sockaddr* sa);

gchar* avahi_reverse_lookup_name_ipv4(const AvahiIPv4Address *a);
gchar* avahi_reverse_lookup_name_ipv6_arpa(const AvahiIPv6Address *a);
gchar* avahi_reverse_lookup_name_ipv6_int(const AvahiIPv6Address *a);

#endif
