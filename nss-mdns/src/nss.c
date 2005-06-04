/* $Id$ */

/***
  This file is part of nss-mdns.
 
  nss-mdns is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.
 
  nss-mdns is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public License
  along with nss-mdns; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <netdb.h>
#include <sys/socket.h>
#include <nss.h>
#include <stdio.h>

#include "query.h"

#define MAX_ENTRIES 16

#ifdef NSS_IPV4_ONLY
#define _nss_mdns_gethostbyname2_r _nss_mdns4_gethostbyname2_r
#define _nss_mdns_gethostbyname_r _nss_mdns4_gethostbyname_r
#define _nss_mdns_gethostbyaddr_r _nss_mdns4_gethostbyaddr_r
#elif NSS_IPV6_ONLY
#define _nss_mdns_gethostbyname2_r _nss_mdns6_gethostbyname2_r
#define _nss_mdns_gethostbyname_r _nss_mdns6_gethostbyname_r
#define _nss_mdns_gethostbyaddr_r _nss_mdns6_gethostbyaddr_r
#endif

struct userdata {
    int count;
    int data_len; /* only valid when doing reverse lookup */
    union  {
        ipv4_address_t ipv4[MAX_ENTRIES];
        ipv6_address_t ipv6[MAX_ENTRIES];
        char *name[MAX_ENTRIES];
    } data;
};

#ifndef NSS_IPV6_ONLY
static void ipv4_callback(const ipv4_address_t *ipv4, void *userdata) {
    struct userdata *u = userdata;
    assert(ipv4 && userdata);

    if (u->count >= MAX_ENTRIES)
        return;

    u->data.ipv4[u->count++] = *ipv4;
    u->data_len += sizeof(ipv4_address_t);
}
#endif

#ifndef NSS_IPV4_ONLY
static void ipv6_callback(const ipv6_address_t *ipv6, void *userdata) {
    struct userdata *u = userdata;
    assert(ipv6 && userdata);

    if (u->count >= MAX_ENTRIES)
        return;

    u->data.ipv6[u->count++] = *ipv6;
    u->data_len += sizeof(ipv6_address_t);
}
#endif

static void name_callback(const char*name, void *userdata) {
    struct userdata *u = userdata;
    assert(name && userdata);

    if (u->count >= MAX_ENTRIES)
        return;

    u->data.name[u->count++] = strdup(name);
    u->data_len += strlen(name)+1;
}

static int ends_with(const char *name, const char* suffix) {
    size_t ln, ls;
    assert(name);
    assert(suffix);

    if ((ls = strlen(suffix)) > (ln = strlen(name)))
        return 0;

    return strcasecmp(name+ln-ls, suffix) == 0;
}

static int verify_name_allowed(const char *name) {
    FILE *f;
    int valid = 0;
    
    assert(name);

    if (!(f = fopen(MDNS_ALLOW_FILE, "r")))
        return ends_with(name, ".local") || ends_with(name, ".local."); 

    while (!feof(f)) {
        char ln[128], ln2[128], *t;
        
        if (!fgets(ln, sizeof(ln), f))
            break;

        ln[strcspn(ln, "#\t\n\r ")] = 0;

        if (ln[0] == 0)
            continue;

        if (strcmp(ln, "*") == 0) {
            valid = 1;
            break;
        }

        if (ln[0] != '.')
            snprintf(t = ln2, sizeof(ln2), ".%s", ln);
        else
            t = ln;

        if (ends_with(name, t)) {
            valid = 1;
            break;
        }
    }

    fclose(f);

    return valid;
}

enum nss_status _nss_mdns_gethostbyname2_r(
    const char *name,
    int af,
    struct hostent * result,
    char *buffer,
    size_t buflen,
    int *errnop,
    int *h_errnop) {

    struct userdata u;
    enum nss_status status = NSS_STATUS_UNAVAIL;
    int fd = -1, r, i;
    size_t address_length, l, index, astart;
    void (*ipv4_func)(const ipv4_address_t *ipv4, void *userdata);
    void (*ipv6_func)(const ipv6_address_t *ipv6, void *userdata);

/*     DEBUG_TRAP; */

#ifdef NSS_IPV4_ONLY
    if (af != AF_INET) 
#elif NSS_IPV6_ONLY
    if (af != AF_INET6)
#else        
    if (af != AF_INET && af != AF_INET6)
#endif        
    {    
        *errnop = EINVAL;
        *h_errnop = NO_RECOVERY;
        goto finish;
    }

    if (! verify_name_allowed(name)) {
        *errnop = ENOENT;
        *h_errnop = HOST_NOT_FOUND;
        status = NSS_STATUS_NOTFOUND;
        goto finish;
    }

    address_length = af == AF_INET ? sizeof(ipv4_address_t) : sizeof(ipv6_address_t);
    if (buflen <
        sizeof(char*)+    /* alias names */
        strlen(name)+1)  {   /* official name */
        
        *errnop = ERANGE;
        *h_errnop = NO_RECOVERY;
        status = NSS_STATUS_TRYAGAIN;

        goto finish;
    }
    
    if ((fd = mdns_open_socket()) < 0) {

        *errnop = errno;
        *h_errnop = NO_RECOVERY;
        goto finish;
    }

    u.count = 0;
    u.data_len = 0;

#ifndef NSS_IPV6_ONLY
    ipv4_func = af == AF_INET ? ipv4_callback : NULL;
#else
    ipv4_func = NULL;
#endif    

#ifndef NSS_IPV4_ONLY
    ipv6_func = af == AF_INET6 ? ipv6_callback : NULL;
#else
    ipv6_func = NULL;
#endif
    
    if ((r = mdns_query_name(fd, name, ipv4_func, ipv6_func, &u)) < 0) {
        *errnop = ETIMEDOUT;
        *h_errnop = HOST_NOT_FOUND;
        goto finish;
    }

    /* Alias names */
    *((char**) buffer) = NULL;
    result->h_aliases = (char**) buffer;
    index = sizeof(char*);

    /* Official name */
    strcpy(buffer+index, name); 
    result->h_name = buffer+index;
    index += strlen(name)+1;
    
    result->h_addrtype = af;
    result->h_length = address_length;

    /* Check if there's enough space for the addresses */
    if (buflen < index+u.data_len+sizeof(char*)*(u.count+1)) {
        *errnop = ERANGE;
        *h_errnop = NO_RECOVERY;
        status = NSS_STATUS_TRYAGAIN;
        goto finish;
    }

    /* Addresses */
    astart = index;
    l = u.count*address_length;
    memcpy(buffer+astart, &u.data, l);
    index += l;

    /* Address array */
    for (i = 0; i < u.count; i++)
        ((char**) (buffer+index))[i] = buffer+astart+address_length*i;
    ((char**) (buffer+index))[i] = NULL;

    result->h_addr_list = (char**) (buffer+index);

    status = NSS_STATUS_SUCCESS;
    
finish:
    if (fd >= 0)
        close(fd);
    
    return status;
}

enum nss_status _nss_mdns_gethostbyname_r (
    const char *name,
    struct hostent *result,
    char *buffer,
    size_t buflen,
    int *errnop,
    int *h_errnop) {

    return _nss_mdns_gethostbyname2_r(
        name,
#ifdef NSS_IPV6_ONLY
        AF_INET6,
#else
        AF_INET,
#endif        
        result,
        buffer,
        buflen,
        errnop,
        h_errnop);
}

enum nss_status _nss_mdns_gethostbyaddr_r(
    const void* addr,
    int len,
    int af,
    struct hostent *result,
    char *buffer,
    size_t buflen,
    int *errnop,
    int *h_errnop) {
    
    struct userdata u;
    enum nss_status status = NSS_STATUS_UNAVAIL;
    int fd = -1, r;
    size_t address_length, index, astart;

    *errnop = EINVAL;
    *h_errnop = NO_RECOVERY;

    u.count = 0;
    u.data_len = 0;
    
    address_length = af == AF_INET ? sizeof(ipv4_address_t) : sizeof(ipv6_address_t);

    if (len != (int) address_length ||
#ifdef NSS_IPV4_ONLY
        af != AF_INET
#elif NSS_IPV6_ONLY
        af != AF_INET6
#else        
        (af != AF_INET && af != AF_INET6)
#endif
        ) {
        *errnop = EINVAL;
        *h_errnop = NO_RECOVERY;
        goto finish;
    }

    if (buflen <
        sizeof(char*)+      /* alias names */
        address_length) {   /* address */
        
        *errnop = ERANGE;
        *h_errnop = NO_RECOVERY;
        status = NSS_STATUS_TRYAGAIN;

        goto finish;
    }
    
    if ((fd = mdns_open_socket()) < 0) {

        *errnop = errno;
        *h_errnop = NO_RECOVERY;
        goto finish;
    }

#if ! defined(NSS_IPV6_ONLY) && ! defined(NSS_IPV4_ONLY)
    if (af == AF_INET)
#endif
#ifndef NSS_IPV6_ONLY        
        r = mdns_query_ipv4(fd, (ipv4_address_t*) addr, name_callback, &u);
#endif
#if ! defined(NSS_IPV6_ONLY) && ! defined(NSS_IPV4_ONLY)
    else
#endif
#ifndef NSS_IPV4_ONLY        
        r = mdns_query_ipv6(fd, (ipv6_address_t*) addr, name_callback, &u);
#endif
    
    if (r < 0) {
        *errnop = ETIMEDOUT;
        *h_errnop = HOST_NOT_FOUND;
        goto finish;
    }

    /* Alias names */
    *((char**) buffer) = NULL;
    result->h_aliases = (char**) buffer;
    index = sizeof(char*);

    assert(u.count > 0 && u.data.name[0]);
    if (buflen <
        strlen(u.data.name[0])+1+ /* official names */
        sizeof(char*)+ /* alias names */
        address_length+  /* address */
        sizeof(void*)*2) {  /* address list */

        *errnop = ERANGE;
        *h_errnop = NO_RECOVERY;
        status = NSS_STATUS_TRYAGAIN;
        goto finish;
    }
    
    /* Official name */
    strcpy(buffer+index, u.data.name[0]); 
    result->h_name = buffer+index;
    index += strlen(u.data.name[0])+1;
    
    result->h_addrtype = af;
    result->h_length = address_length;

    /* Address */
    astart = index;
    memcpy(buffer+astart, addr, address_length);
    index += address_length;

    /* Address array */
    ((char**) (buffer+index))[0] = buffer+astart;
    ((char**) (buffer+index))[1] = NULL;
    result->h_addr_list = (char**) (buffer+index);

    status = NSS_STATUS_SUCCESS;
    
finish:
    if (fd >= 0)
        close(fd);
    
    return status;
}

