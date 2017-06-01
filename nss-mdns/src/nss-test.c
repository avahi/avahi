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

#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    struct hostent *he;
    in_addr_t **a;
    const char *arg= argc > 1 ? argv[1] : "cocaine.local";
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
        char txt[256];
        fprintf(stderr, " %s", inet_ntop(he->h_addrtype, *a, txt, sizeof(txt)));
    }
    fprintf(stderr, "\n");
    
    return 0;
}
