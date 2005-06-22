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


#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <assert.h>
#include <unistd.h>

#include "avahi.h"
#include "util.h"

#define AVAHI_SOCKET "/var/run/avahi/socket"

static FILE *open_socket(void) {
    int fd = -1;
    struct sockaddr_un sa;
    FILE *f;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        goto fail;

    set_cloexec(fd);
    
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, AVAHI_SOCKET, sizeof(sa.sun_path)-1);
    sa.sun_path[sizeof(sa.sun_path)-1] = 0;

    if (connect(fd, (struct sockaddr*) &sa, sizeof(sa)) < 0)
        goto fail;

    if (!(f = fdopen(fd, "r+")))
        goto fail;

    return f;
    
fail:
    if (fd >= 0)
        close(fd);

    return NULL;
    
}

int avahi_resolve_name(int af, const char* name, void* data) {
    FILE *f;
    char *e, *p;
    int ret = -1;
    char ln[256];

    assert(af == AF_INET || af == AF_INET6);
    
    if (!(f = open_socket()))
        goto finish;

    fprintf(f, "RESOLVE-HOSTNAME%s %s\n", af == AF_INET ? "-IPV4" : "-IPV6", name);
    fflush(f);

    if (!(fgets(ln, sizeof(ln), f)))
        goto finish;

    if (ln[0] != '+') {
        ret = 1;
        goto finish;
    }

    p = ln+1;
    p += strspn(p, "\t ");
    e = p + strcspn(p, "\n\r\t ");
    *e = 0;

    if (inet_pton(af, p, data) <= 0)
        goto finish;

    ret = 0;
    
finish:

    if (f)
        fclose(f);

    return ret;
}

int avahi_resolve_address(int af, const void *data, char* name, size_t name_len) {
    FILE *f;
    char *e, *p;
    int ret = -1;
    char a[256], ln[256];

    assert(af == AF_INET || af == AF_INET6);
    
    if (!(f = open_socket()))
        goto finish;

    fprintf(f, "RESOLVE-ADDRESS %s\n", inet_ntop(af, data, a, sizeof(a)));
    
    if (!(fgets(ln, sizeof(ln), f)))
        goto finish;

    if (ln[0] != '+') {
        ret = 1;
        goto finish;
    }

    p = ln+1;
    p += strspn(p, "\t ");
    e = p + strcspn(p, "\n\r\t ");
    *e = 0;

    strncpy(name, p, name_len-1);
    name[name_len-1] = 0;

    ret = 0;
     
finish:

    if (f)
        fclose(f);

    return ret;
}
