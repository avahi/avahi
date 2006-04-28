/* $Id$ */

/***
    This file is part of nss-mdns.
 
    nss-mdns is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2 of the License,
    or (at your option) any later version.
 
    nss-mdns is distributed in the hope that it will be useful, but1
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
#include <stdlib.h>

#include "query.h"

#ifdef ENABLE_AVAHI
#include "avahi.h"
#endif

#if defined(NSS_IPV4_ONLY) && ! defined(MDNS_MINIMAL)
#define _nss_mdns_gethostbyname2_r _nss_mdns4_gethostbyname2_r
#define _nss_mdns_gethostbyname_r  _nss_mdns4_gethostbyname_r
#define _nss_mdns_gethostbyaddr_r  _nss_mdns4_gethostbyaddr_r
#elif defined(NSS_IPV4_ONLY) && defined(MDNS_MINIMAL)
#define _nss_mdns_gethostbyname2_r _nss_mdns4_minimal_gethostbyname2_r
#define _nss_mdns_gethostbyname_r  _nss_mdns4_minimal_gethostbyname_r
#define _nss_mdns_gethostbyaddr_r  _nss_mdns4_minimal_gethostbyaddr_r
#elif defined(NSS_IPV6_ONLY) && ! defined(MDNS_MINIMAL)
#define _nss_mdns_gethostbyname2_r _nss_mdns6_gethostbyname2_r
#define _nss_mdns_gethostbyname_r  _nss_mdns6_gethostbyname_r
#define _nss_mdns_gethostbyaddr_r  _nss_mdns6_gethostbyaddr_r
#elif defined(NSS_IPV6_ONLY) && defined(MDNS_MINIMAL)
#define _nss_mdns_gethostbyname2_r _nss_mdns6_minimal_gethostbyname2_r
#define _nss_mdns_gethostbyname_r  _nss_mdns6_minimal_gethostbyname_r
#define _nss_mdns_gethostbyaddr_r  _nss_mdns6_minimal_gethostbyaddr_r
#elif defined(MDNS_MINIMAL)
#define _nss_mdns_gethostbyname2_r _nss_mdns_minimal_gethostbyname2_r
#define _nss_mdns_gethostbyname_r  _nss_mdns_minimal_gethostbyname_r
#define _nss_mdns_gethostbyaddr_r  _nss_mdns_minimal_gethostbyaddr_r
#endif

/* Maximum number of entries to return */
#define MAX_ENTRIES 16

/* The resolv.conf page states that they only support 6 domains */
#define MAX_SEARCH_DOMAINS 6

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
#ifndef MDNS_MINIMAL
    FILE *f;
#endif
    
    assert(name);

#ifndef MDNS_MINIMAL
    if ((f = fopen(MDNS_ALLOW_FILE, "r"))) {
        int valid = 0;
        
        
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
#endif

    return ends_with(name, ".local") || ends_with(name, ".local."); 
}

static char **alloc_domains(unsigned ndomains) {
    char **domains;

    if (!(domains = malloc(sizeof(char*) * ndomains)))
        return NULL;

    /* initialize them all to 0 */
    memset(domains, 0, sizeof(char*) * ndomains);
    return domains;
}

static void free_domains(char **domains) {
    char **p;

    if (!domains)
        return;

    for(p = domains; *p; p++) 
        free(*p);

    free(domains);
}

static char** parse_domains(const char *domains_in) {
    /* leave room for the NULL terminator */
    char **domains_out;
    const char *start = domains_in;
    unsigned domain = 0;

    if (!(domains_out = alloc_domains(MAX_SEARCH_DOMAINS+1)))
        return NULL;

    while (domain < MAX_SEARCH_DOMAINS) {
        const char *end;
        char *tmp;
        size_t domain_len;
        
        end = start + strcspn(start, " \t\r\n");
        domain_len = (end - start);

        if (!(tmp = malloc(domain_len + 1)))
            break;
        
        memcpy(tmp, start, domain_len);
        tmp[domain_len] = '\0';

        domains_out[domain++] = tmp;

        end += strspn(end," \t\r\n");

        if (!*end)
            break;
        
        start = end;
    }

    return domains_out;
}

static char** get_search_domains(void) {
    FILE *f = 0;
    char **domains = NULL;

    /* according to the resolv.conf man page (in Linux) the LOCALDOMAIN
       environment variable should override the settings in the resolv.conf file */
    char *line = getenv("LOCALDOMAIN");
    if (line && *line != 0)
        return parse_domains(line);
    
    if (!(f = fopen(RESOLV_CONF_FILE, "r")))
        return NULL;

    while (!feof(f)) {
        char *start = NULL;
        char ln[512];
	  
        if (!fgets(ln, sizeof(ln), f))
            break;

        start = ln + strspn(ln, " \t\r\n");
    
        if (strncmp(start, "search", 6) && strncmp(start, "domain", 6))
            continue;
        
        if (start[6] != ' ' && start[6] != '\t')
            continue;

        /* scan to the end of the keyword ('search' or 'domain' currently) */
        start += strcspn(start, " \t\r\n");

        /* find the begining of the first domain in the list */
        start += strspn(start, " \t\r\n");

        /* the resolv.conf manpage also states that 'search' and 'domain' are mutually exclusive
           and that the last one wins. */
        free_domains(domains);
        domains = parse_domains(start);
    }

    fclose(f);

    return domains;
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
    int i;
    size_t address_length, l, idx, astart;
    void (*ipv4_func)(const ipv4_address_t *ipv4, void *userdata);
    void (*ipv6_func)(const ipv6_address_t *ipv6, void *userdata);
    int name_allowed;

#ifdef ENABLE_AVAHI
    int avahi_works = 1;
    uint8_t data[128];
#endif

#ifdef ENABLE_LEGACY
    int fd = -1;
#endif

/*     DEBUG_TRAP; */

    if (af == AF_UNSPEC)
#ifdef NSS_IPV6_ONLY
        af = AF_INET6;
#else
        af = AF_INET;
#endif

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

    address_length = af == AF_INET ? sizeof(ipv4_address_t) : sizeof(ipv6_address_t);
    if (buflen <
        sizeof(char*)+    /* alias names */
        strlen(name)+1)  {   /* official name */
        
        *errnop = ERANGE;
        *h_errnop = NO_RECOVERY;
        status = NSS_STATUS_TRYAGAIN;

        goto finish;
    }
    
    u.count = 0;
    u.data_len = 0;

#ifdef NSS_IPV6_ONLY
    ipv4_func = NULL;
#else
    ipv4_func = af == AF_INET ? ipv4_callback : NULL;
#endif    

#ifdef NSS_IPV4_ONLY
    ipv6_func = NULL;
#else
    ipv6_func = af == AF_INET6 ? ipv6_callback : NULL;
#endif

    name_allowed = verify_name_allowed(name);
    
#ifdef ENABLE_AVAHI

    if (avahi_works && name_allowed) {
        int r;

        if ((r = avahi_resolve_name(af, name, data)) < 0)
            avahi_works = 0;
        else if (r == 0) {
            if (af == AF_INET && ipv4_func)
                ipv4_func((ipv4_address_t*) data, &u);
            if (af == AF_INET6 && ipv6_func)
                ipv6_func((ipv6_address_t*)data, &u);
        } else
            status = NSS_STATUS_NOTFOUND;
    }

    if (u.count == 0 && avahi_works && !ends_with(name, ".")) {
        char **domains;

        if ((domains = get_search_domains())) {
            char **p;
            
            /* Try to concatenate host names */
	    for (p = domains; *p; p++) {
                int fullnamesize;
                char *fullname;
                
	        fullnamesize = strlen(name) + strlen(*p) + 2;

                if (!(fullname = malloc(fullnamesize)))
                    break;
                
		snprintf(fullname, fullnamesize, "%s.%s", name, *p);

                if (verify_name_allowed(fullname)) {
                    int r;

                    r = avahi_resolve_name(af, fullname, data);
                    free(fullname);
                    
                    if (r < 0) {
                        /* Lookup failed */
                        avahi_works = 0;
                        break;
                    } else if (r == 0) {
                        /* Lookup succeeded */
                        if (af == AF_INET && ipv4_func)
                            ipv4_func((ipv4_address_t*) data, &u);
                        if (af == AF_INET6 && ipv6_func)
                            ipv6_func((ipv6_address_t*)data, &u);
                        break;
                    } else
                        /* Lookup suceeded, but nothing found */
                        status = NSS_STATUS_NOTFOUND;
                    
		} else
                    free(fullname);
	    }
            
	    free_domains(domains);
	}
    }
#endif

#if defined(ENABLE_LEGACY) && defined(ENABLE_AVAHI)
    if (u.count == 0 && !avahi_works) 
#endif

#if defined(ENABLE_LEGACY)
    {
        if ((fd = mdns_open_socket()) < 0) {
            *errnop = errno;
            *h_errnop = NO_RECOVERY;
            goto finish;
        }

        if (name_allowed) {
            /* Ignore return value */
            mdns_query_name(fd, name, ipv4_func, ipv6_func, &u);

            if (!u.count)
                status = NSS_STATUS_NOTFOUND;
        }

        if (u.count == 0 && !ends_with(name, ".")) {
            char **domains;
            
            /* Try the search domains if the user did not use a traling '.' */
            
            if ((domains = get_search_domains())) {
                char **p;
                
                for (p = domains; *p; p++) {
                    int fullnamesize = 0;
                    char *fullname = NULL;
                    
                    fullnamesize = strlen(name) + strlen(*p) + 2;
                    if (!(fullname = malloc(fullnamesize)))
                        break;
                    
                    snprintf(fullname, fullnamesize, "%s.%s", name, *p);
                    
                    if (verify_name_allowed(fullname)) {
                        
                        /* Ignore return value */
                        mdns_query_name(fd, fullname, ipv4_func, ipv6_func, &u);
                        
                        if (u.count > 0) {
                            /* We found something, so let's quit */
                            free(fullname);
                            break;
                        } else
                            status = NSS_STATUS_NOTFOUND;

                    }
                    
                    free(fullname);
                }
                
                free_domains(domains);
	    }
        }
    }
#endif

    if (u.count == 0) {
        *errnop = ETIMEDOUT;
        *h_errnop = HOST_NOT_FOUND;
        goto finish;
    }
    
    /* Alias names */
    *((char**) buffer) = NULL;
    result->h_aliases = (char**) buffer;
    idx = sizeof(char*);
    
    /* Official name */
    strcpy(buffer+idx, name); 
    result->h_name = buffer+idx;
    idx += strlen(name)+1;
    
    result->h_addrtype = af;
    result->h_length = address_length;
    
    if (idx%sizeof(char*))
        idx+=(sizeof(char*)-idx%sizeof(char*)); /* Align on 32 bit boundary */
    
    /* Check if there's enough space for the addresses */
    if (buflen < idx+u.data_len+sizeof(char*)*(u.count+1)) {
        *errnop = ERANGE;
        *h_errnop = NO_RECOVERY;
        status = NSS_STATUS_TRYAGAIN;
        goto finish;
    }

    /* Addresses */
    astart = idx;
    l = u.count*address_length;
    memcpy(buffer+astart, &u.data, l);
    idx += l;

    /* Address array */
    for (i = 0; i < u.count; i++)
        ((char**) (buffer+idx))[i] = buffer+astart+address_length*i;
    ((char**) (buffer+idx))[i] = NULL;

    result->h_addr_list = (char**) (buffer+idx);

    status = NSS_STATUS_SUCCESS;
    
finish:
#ifdef ENABLE_LEGACY
    if (fd >= 0)
        close(fd);
#endif

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
        AF_UNSPEC,
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
    int r;
    size_t address_length, idx, astart;
    
#ifdef ENABLE_AVAHI
    char t[256];
#endif
#ifdef ENABLE_LEGACY
    int fd = -1;
#endif

    *errnop = EINVAL;
    *h_errnop = NO_RECOVERY;

    u.count = 0;
    u.data_len = 0;

    /* Check for address types */
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

    /* Check for buffer space */
    if (buflen <
        sizeof(char*)+      /* alias names */
        address_length) {   /* address */
        
        *errnop = ERANGE;
        *h_errnop = NO_RECOVERY;
        status = NSS_STATUS_TRYAGAIN;

        goto finish;
    }

#ifdef MDNS_MINIMAL

    /* Only query for 169.254.0.0/16 IPv4 in minimal mode */
    if ((af == AF_INET && ((ntohl(*(const uint32_t*)  addr) & 0xFFFF0000UL) != 0xA9FE0000UL)) ||
        (af == AF_INET6 && !(((const uint8_t*) addr)[0] == 0xFE && (((const uint8_t*) addr)[1] >> 6) == 2))) {

        *errnop = EINVAL;
        *h_errnop = NO_RECOVERY;

        goto finish;
    }
#endif
    
#ifdef ENABLE_AVAHI
    /* Lookup using Avahi */
    if ((r = avahi_resolve_address(af, addr, t, sizeof(t))) == 0) {
        name_callback(t, &u);
    } else if (r > 0) {
        *errnop = ETIMEDOUT;
        *h_errnop = HOST_NOT_FOUND;
        status = NSS_STATUS_NOTFOUND;
        goto finish;
    } 
#endif

#if defined(ENABLE_AVAHI) && defined(ENABLE_LEGACY)
    else
#endif
    
#ifdef ENABLE_LEGACY
     /* Lookup using legacy mDNS queries */   
     {
        if ((fd = mdns_open_socket()) < 0) {
            *errnop = errno;
            *h_errnop = NO_RECOVERY;
            goto finish;
        }

	r = -1;

#if ! defined(NSS_IPV6_ONLY) && ! defined(NSS_IPV4_ONLY)
        if (af == AF_INET)
#endif
#ifndef NSS_IPV6_ONLY
            r = mdns_query_ipv4(fd, (const ipv4_address_t*) addr, name_callback, &u);
#endif
#if ! defined(NSS_IPV6_ONLY) && ! defined(NSS_IPV4_ONLY)
        else
#endif
#ifndef NSS_IPV4_ONLY
            r = mdns_query_ipv6(fd, (const ipv6_address_t*) addr, name_callback, &u);
#endif
        if (r < 0) {
            *errnop = ETIMEDOUT;
            *h_errnop = HOST_NOT_FOUND;
            status = NSS_STATUS_NOTFOUND;
            goto finish;
        }
    }
#endif /* ENABLE_LEGACY */

    /* Alias names */
    *((char**) buffer) = NULL;
    result->h_aliases = (char**) buffer;
    idx = sizeof(char*);

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
    strcpy(buffer+idx, u.data.name[0]); 
    result->h_name = buffer+idx;
    idx += strlen(u.data.name[0])+1;
    
    result->h_addrtype = af;
    result->h_length = address_length;

    /* Address */
    astart = idx;
    memcpy(buffer+astart, addr, address_length);
    idx += address_length;

    /* Address array */
    ((char**) (buffer+idx))[0] = buffer+astart;
    ((char**) (buffer+idx))[1] = NULL;
    result->h_addr_list = (char**) (buffer+idx);

    status = NSS_STATUS_SUCCESS;
    
finish:
#ifdef ENABLE_LEGACY
    if (fd >= 0)
        close(fd);
#endif

    return status;
}

