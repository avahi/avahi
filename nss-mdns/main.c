#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include "query.h"

static void ipv4_func(const ipv4_address_t *ipv4, void *userdata) {
    fprintf(stderr, "IPV4: %s\n", inet_ntoa(*(struct in_addr*) &ipv4->address));
}

static void ipv6_func(const ipv6_address_t *ipv6, void *userdata) {
}

static void name_func(const char *name, void *userdata) {
    fprintf(stderr, "NAME: %s\n", name);
}

int main(int argc, char *argv[]) {
    int ret = 1, fd = -1;
    ipv4_address_t ipv4;

    if ((fd = mdns_open_socket()) < 0)
        goto finish;

/*     if (mdns_query_name(fd, argc > 1 ? argv[1] : "ecstasy.local", &ipv4_func, &ipv6_func, NULL) < 0) */
/*         goto finish; */

    ipv4.address = inet_addr(argc > 1 ? argv[1] : "192.168.100.1"); 
    
    if (mdns_query_ipv4(fd, &ipv4, name_func, NULL) < 0) 
        goto finish; 
    
    ret = 0;

finish:

    if (fd >= 0)
        close(fd);
    
    return ret;
}
