#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include "address.h"

guint avahi_address_get_size(const AvahiAddress *a) {
    g_assert(a);

    if (a->family == AF_INET)
        return 4;
    else if (a->family == AF_INET6)
        return 16;

    return 0;
}

gint avahi_address_cmp(const AvahiAddress *a, const AvahiAddress *b) {
    g_assert(a);
    g_assert(b);
    
    if (a->family != b->family)
        return -1;

    return memcmp(a->data.data, b->data.data, avahi_address_get_size(a));
}

gchar *avahi_address_snprint(char *s, guint length, const AvahiAddress *a) {
    g_assert(s);
    g_assert(length);
    g_assert(a);
    return (gchar*) inet_ntop(a->family, a->data.data, s, length);
}

gchar* avahi_reverse_lookup_name_ipv4(const AvahiIPv4Address *a) {
    guint32 n = ntohl(a->address);
    g_assert(a);

    return g_strdup_printf("%u.%u.%u.%u.in-addr.arpa", n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF, n >> 24);
}

static gchar *reverse_lookup_name_ipv6(const AvahiIPv6Address *a, const gchar *suffix) {
    
    return g_strdup_printf("%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%s",
                           a->address[15] & 0xF,
                           a->address[15] >> 4,
                           a->address[14] & 0xF,
                           a->address[14] >> 4,
                           a->address[13] & 0xF,
                           a->address[13] >> 4,
                           a->address[12] & 0xF,
                           a->address[12] >> 4,
                           a->address[11] & 0xF,
                           a->address[11] >> 4,
                           a->address[10] & 0xF,
                           a->address[10] >> 4,
                           a->address[9] & 0xF,
                           a->address[9] >> 4,
                           a->address[8] & 0xF,
                           a->address[8] >> 4,
                           a->address[7] & 0xF,
                           a->address[7] >> 4,
                           a->address[6] & 0xF,
                           a->address[6] >> 4,
                           a->address[5] & 0xF,
                           a->address[5] >> 4,
                           a->address[4] & 0xF,
                           a->address[4] >> 4,
                           a->address[3] & 0xF,
                           a->address[3] >> 4,
                           a->address[2] & 0xF,
                           a->address[2] >> 4,
                           a->address[1] & 0xF,
                           a->address[1] >> 4,
                           a->address[0] & 0xF,
                           a->address[0] >> 4,
                           suffix);
}

gchar *avahi_reverse_lookup_name_ipv6_arpa(const AvahiIPv6Address *a) {
    return reverse_lookup_name_ipv6(a, "ip6.arpa");
}

gchar *avahi_reverse_lookup_name_ipv6_int(const AvahiIPv6Address *a) {
    return reverse_lookup_name_ipv6(a, "ip6.int");
}

AvahiAddress *avahi_address_parse(const char *s, guchar family, AvahiAddress *ret_addr) {
    g_assert(ret_addr);
    g_assert(s);

    if (inet_pton(family, s, ret_addr->data.data) < 0)
        return NULL;

    ret_addr->family = family;
    
    return ret_addr;
}

AvahiAddress *avahi_address_from_sockaddr(const struct sockaddr* sa, AvahiAddress *ret_addr) {
    g_assert(sa);
    g_assert(ret_addr);

    g_assert(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);

    ret_addr->family = sa->sa_family;

    if (sa->sa_family == AF_INET)
        memcpy(&ret_addr->data.ipv4, &((struct sockaddr_in*) sa)->sin_addr, sizeof(ret_addr->data.ipv4));
    else
        memcpy(&ret_addr->data.ipv6, &((struct sockaddr_in6*) sa)->sin6_addr, sizeof(ret_addr->data.ipv6));

    return ret_addr;
}

guint16 avahi_port_from_sockaddr(const struct sockaddr* sa) {
    g_assert(sa);

    g_assert(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);

    if (sa->sa_family == AF_INET)
        return ntohs(((struct sockaddr_in*) sa)->sin_port);
    else
        return ntohs(((struct sockaddr_in6*) sa)->sin6_port);
}
