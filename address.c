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

    return memcmp(a, b, flx_address_get_size(a));
}

gchar *flx_address_snprint(char *s, guint length, const flxAddress *a) {
    g_assert(s);
    g_assert(length);
    g_assert(a);
    return (gchar*) inet_ntop(a->family, a->data, s, length);
}

gchar* flx_reverse_lookup_name(const flxAddress *a) {
    g_assert(a);

    if (a->family == AF_INET) {
        guint32 n = ntohl(a->ipv4.address);
        return g_strdup_printf("%u.%u.%u.%u.in-addr.arpa", n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF, n >> 24);
    } else if (a->family == AF_INET6) {
        
        return g_strdup_printf("%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.ip6.int",
                               a->ipv6.address[15] & 0xF,
                               a->ipv6.address[15] >> 4,
                               a->ipv6.address[14] & 0xF,
                               a->ipv6.address[14] >> 4,
                               a->ipv6.address[13] & 0xF,
                               a->ipv6.address[13] >> 4,
                               a->ipv6.address[12] & 0xF,
                               a->ipv6.address[12] >> 4,
                               a->ipv6.address[11] & 0xF,
                               a->ipv6.address[11] >> 4,
                               a->ipv6.address[10] & 0xF,
                               a->ipv6.address[10] >> 4,
                               a->ipv6.address[9] & 0xF,
                               a->ipv6.address[9] >> 4,
                               a->ipv6.address[8] & 0xF,
                               a->ipv6.address[8] >> 4,
                               a->ipv6.address[7] & 0xF,
                               a->ipv6.address[7] >> 4,
                               a->ipv6.address[6] & 0xF,
                               a->ipv6.address[6] >> 4,
                               a->ipv6.address[5] & 0xF,
                               a->ipv6.address[5] >> 4,
                               a->ipv6.address[4] & 0xF,
                               a->ipv6.address[4] >> 4,
                               a->ipv6.address[3] & 0xF,
                               a->ipv6.address[3] >> 4,
                               a->ipv6.address[2] & 0xF,
                               a->ipv6.address[2] >> 4,
                               a->ipv6.address[1] & 0xF,
                               a->ipv6.address[1] >> 4,
                               a->ipv6.address[0] & 0xF,
                               a->ipv6.address[0] >> 4);
    }

    return NULL;
}
