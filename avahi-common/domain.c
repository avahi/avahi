/* $Id$ */

/***
  This file is part of avahi.
 
  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.
 
  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

#include "domain.h"
#include "malloc.h"
#include "error.h"

char *avahi_get_host_name(void) {
#ifdef HOST_NAME_MAX
    char t[HOST_NAME_MAX];
#else
    char t[256];
#endif
    gethostname(t, sizeof(t));
    t[sizeof(t)-1] = 0;
    return avahi_normalize_name(t);
}

static char *unescape_uneeded(const char *src, char *ret_dest, size_t size) {
    int escaped = 0;
    
    assert(src);
    assert(ret_dest);
    assert(size > 0);
    
    for (; *src; src++) {

        if (!escaped && *src == '\\')
            escaped = 1;
        else if (escaped && (*src == '.' || *src == '\\')) {

            if ((size -= 2) <= 1) break;
            
            *(ret_dest++) = '\\';
            *(ret_dest++) = *src;
            escaped = 0;
        } else {
            if (--size <= 1) break;

            *(ret_dest++) = *src;
            escaped = 0;
        }

    }

    *ret_dest = 0;
    
    return ret_dest;
}

char *avahi_normalize_name(const char *s) {
    char tmp[256];
    size_t l;
    
    assert(s);

    unescape_uneeded(s, tmp, sizeof(tmp));

    l = strlen(tmp);

    while (l > 0 && tmp[l-1] == '.')
        tmp[--l] = 0;

    return avahi_strdup(tmp);
}


/* Read the first label from string *name, unescape "\" and write it to dest */
char *avahi_unescape_label(const char **name, char *dest, size_t size) {
    unsigned i = 0;
    char *d;
    
    assert(dest);
    assert(size > 0);
    assert(name);

    if (!**name)
        return NULL;

    d = dest;
    
    for (;;) {
        if (i >= size)
            return NULL;

        if (**name == '.') {
            (*name)++;
            break;
        }
        
        if (**name == 0)
            break;
        
        if (**name == '\\') {
            (*name) ++;
            
            if (**name == 0)
                break;
        }
        
        *(d++) = *((*name) ++);
        i++;
    }

    assert(i < size);

    *d = 0;

    return dest;
}

/* Escape "\" and ".", append \0 */
char *avahi_escape_label(const uint8_t* src, size_t src_length, char **ret_name, size_t *ret_size) {
    char *r;

    assert(src);
    assert(ret_name);
    assert(*ret_name);
    assert(ret_size);
    assert(*ret_size > 0);

    r = *ret_name;

    while (src_length > 0) {
        if (*src == '.' || *src == '\\') {
            if (*ret_size < 3)
                return NULL;
            
            *((*ret_name) ++) = '\\';
            (*ret_size) --;
        }

        if (*ret_size < 2)
            return NULL;
        
        *((*ret_name)++) = *src;
        (*ret_size) --;

        src_length --;
        src++;
    }

    **ret_name = 0;

    return r;
}

int avahi_domain_equal(const char *a, const char *b) {
    assert(a);
    assert(b);

    if (a == b)
        return 1;
    
    for (;;) {
        char ca[65], cb[65], *pa, *pb;

        pa = avahi_unescape_label(&a, ca, sizeof(ca));
        pb = avahi_unescape_label(&b, cb, sizeof(cb));

        if (!pa && !pb)
            return 1;
        else if ((pa && !pb) || (!pa && pb))
            return 0;

        if (strcasecmp(pa, pb))
            return 0;
    }

    return 1;
}

int avahi_binary_domain_cmp(const char *a, const char *b) {
    assert(a);
    assert(b);

    if (a == b)
        return 0;

    for (;;) {
        char ca[65], cb[65], *pa, *pb;
        int r;

        pa = avahi_unescape_label(&a, ca, sizeof(ca));
        pb = avahi_unescape_label(&b, cb, sizeof(cb));

        if (!pa && !pb)
            return 0;
        else if (pa && !pb)
            return 1;
        else if (!pa && pb)
            return -1;
        
        if ((r = strcmp(pa, pb)))
            return r;
    }
}

int avahi_is_valid_service_type(const char *t) {
    const char *p;
    assert(t);

    if (strlen(t) < 5)
        return 0;
    
    if (*t != '_')
        return 0;

    if (!(p = strchr(t, '.')))
        return 0;

    if (p - t > 63 || p - t < 2)
        return 0;

    if (*(++p) != '_')
        return 0;

    if (strchr(p, '.'))
        return 0;

    if (strlen(p) > 63 || strlen(p) < 2)
        return 0;
    
    return 1;
}

int avahi_is_valid_domain_name(const char *t) {
    const char *p, *dp;
    int dot = 0;
        
    assert(t);

    if (*t == 0)
        return 0;

    /* Domains may not start with a dot */
    if (*t == '.')
        return 0;

    dp = t; 

    for (p = t; *p; p++) {

        if (*p == '.') {
            if (dot) /* Two subsequent dots */
                return 0;

            if (p - dp > 63)
                return 0;

            dot = 1;
            dp = p + 1;
        } else
            dot = 0;

    }

    if (p - dp > 63)
        return 0;

    /* A trailing dot IS allowed */
    
    return 1;
}

int avahi_is_valid_service_name(const char *t) {
    assert(t);

    if (*t == 0)
        return 0;

    if (strlen(t) > 63)
        return 0;

    return 1;
}

int avahi_is_valid_host_name(const char *t) {
    assert(t);

    if (*t == 0)
        return 0;

    if (strlen(t) > 63)
        return 0;

    if (strchr(t, '.'))
        return 0;

    return 1;
}

unsigned avahi_domain_hash(const char *s) {
    unsigned hash = 0;
    
    for (;;) {
        char c[65], *p;

        if (!avahi_unescape_label(&s, c, sizeof(c)))
            return hash;

        if (!c[0])
            continue;

        for (p = c; *p; p++)
            hash = 31 * hash + tolower(*p);
    }
}

int avahi_domain_ends_with(const char *domain, const char *suffix) {
    assert(domain);
    assert(suffix);

    assert(avahi_is_valid_domain_name(domain));
    assert(avahi_is_valid_domain_name(suffix));

    for (;;) {
        char dummy[64];
        
        if (avahi_domain_equal(domain, suffix))
            return 1;

        if (!(avahi_unescape_label(&domain, dummy, sizeof(dummy))))
            return 0;
    } 
}

static void escape_service_name(char *d, size_t size, const char *s) {
    assert(d);
    assert(size);
    assert(s);

    while (*s && size >= 2) {
        if (*s == '.' || *s == '\\') {
            if (size < 3)
                break;

            *(d++) = '\\';
            size--;
        }
            
        *(d++) = *(s++);
        size--;
    }

    assert(size > 0);
    *(d++) = 0;
}


int avahi_service_name_snprint(char *p, size_t size, const char *name, const char *type, const char *domain) {
    char *t = NULL, *d = NULL;
    char ename[64];
    int ret;
    
    assert(p);

    if ((name && !avahi_is_valid_service_name(name))) {
        ret = AVAHI_ERR_INVALID_SERVICE_NAME;
        goto fail;
    }

    if (!avahi_is_valid_service_type(type)) {
        ret = AVAHI_ERR_INVALID_SERVICE_TYPE;
        goto fail;
    }
        
    if (!avahi_is_valid_domain_name(domain)) {
        ret = AVAHI_ERR_INVALID_DOMAIN_NAME;
        goto fail;
    }
        
    if (name)
        escape_service_name(ename, sizeof(ename), name);
    
    if (!(d = avahi_normalize_name(domain)) ||
        !(t = avahi_normalize_name(type))) {
        ret = AVAHI_ERR_NO_MEMORY;
        goto fail;
    }

    snprintf(p, size, "%s%s%s.%s", name ? ename : "", name ? "." : "", t, d);

    ret = AVAHI_OK;
    
fail:

    avahi_free(t);
    avahi_free(d);

    return ret;
}
