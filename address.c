#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include "address.h"

guint flx_address_get_size(const flxAddress *a) {
    g_assert(a);

    if (a->family == AF_INET)
        return 4;
    else if (a->family == AF_INET6)
        return 16;

    return 0;
}

gint flx_address_cmp(const flxAddress *a, const flxAddress *b) {
    g_assert(a);
    g_assert(b);
    
    if (a->family != b->family)
        return -1;

    return memcmp(a->data.data, b->data.data, flx_address_get_size(a));
}

gchar *flx_address_snprint(char *s, guint length, const flxAddress *a) {
    g_assert(s);
    g_assert(length);
    g_assert(a);
    return (gchar*) inet_ntop(a->family, a->data.data, s, length);
}

gchar* flx_reverse_lookup_name_ipv4(const flxIPv4Address *a) {
    guint32 n = ntohl(a->address);
    g_assert(a);

    return g_strdup_printf("%u.%u.%u.%u.in-addr.arpa", n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF, n >> 24);
}

static gchar *reverse_lookup_name_ipv6(const flxIPv6Address *a, const gchar *suffix) {
    
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

gchar *flx_reverse_lookup_name_ipv6_arpa(const flxIPv6Address *a) {
    return reverse_lookup_name_ipv6(a, "ip6.arpa");
}

gchar *flx_reverse_lookup_name_ipv6_int(const flxIPv6Address *a) {
    return reverse_lookup_name_ipv6(a, "ip6.int");
}

flxAddress *flx_address_parse(const char *s, guchar family, flxAddress *ret_addr) {
    g_assert(ret_addr);
    g_assert(s);

    if (inet_pton(family, s, ret_addr->data.data) < 0)
        return NULL;

    ret_addr->family = family;
    
    return ret_addr;
}

flxAddress *flx_address_from_sockaddr(const struct sockaddr* sa, flxAddress *ret_addr) {
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
