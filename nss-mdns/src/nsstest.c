#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    struct hostent *he;
    in_addr_t **a;
    char *arg= argc > 1 ? argv[1] : "whiskey.local";
    uint8_t t[256];
    
    if (inet_pton(AF_INET, arg, &t) > 0) 
        he = gethostbyaddr(t, 4, AF_INET);
    else if (inet_pton(AF_INET6, arg, &t) > 0)
        he = gethostbyaddr(t, 16, AF_INET6);
    else
        he = gethostbyname(arg);

    if (!he) {
        fprintf(stderr, "lookup failed\n");
        return 1;
    }

    fprintf(stderr, "official name: %s\n", he->h_name);

    if (!he->h_aliases || !he->h_aliases[0])
        fprintf(stderr, "no aliases\n");
    else {
        char **h;
        fprintf(stderr, "aliases:");
        for (h = he->h_aliases; *h; h++)
            fprintf(stderr, " %s", *h);
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "addr type: %s\n", he->h_addrtype == AF_INET ? "inet" : (he->h_addrtype == AF_INET6 ? "inet6" : NULL));
    fprintf(stderr, "addr length: %i\n", he->h_length);

    fprintf(stderr, "addresses:");
    for (a = (in_addr_t**) he->h_addr_list; *a;  a++) {
        char t[256];
        fprintf(stderr, " %s", inet_ntop(he->h_addrtype, *a, t, sizeof(t)));
    }
    fprintf(stderr, "\n");
    
    return 0;
}
