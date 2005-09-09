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
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

#include "strlst.h"
#include "malloc.h"

AvahiStringList*avahi_string_list_add_anonymous(AvahiStringList *l, size_t size) {
    AvahiStringList *n;

    if (!(n = avahi_malloc(sizeof(AvahiStringList) + size)))
        return NULL;
    
    n->next = l;
    n->size = size;

    /* NUL terminate strings, just to make sure */
    n->text[size] = 0;

    return n;
}

AvahiStringList *avahi_string_list_add_arbitrary(AvahiStringList *l, const uint8_t*text, size_t size) {
    AvahiStringList *n;

    assert(text);

    if (!(n = avahi_string_list_add_anonymous(l, size)))
        return NULL;

    if (size > 0)
        memcpy(n->text, text, size);

    return n;
}

AvahiStringList *avahi_string_list_add(AvahiStringList *l, const char *text) {
    assert(text);

    return avahi_string_list_add_arbitrary(l, (const uint8_t*) text, strlen(text));
}

AvahiStringList *avahi_string_list_parse(const void* data, size_t size) {
    AvahiStringList *r = NULL;
    const uint8_t *c;
    
    assert(data);

    c = data;
    for (;;) {
        size_t k;
        
        if (size < 1)
            break;

        k = *(c++);

        if (k > 0) /* Ignore empty strings */
            r = avahi_string_list_add_arbitrary(r, c, k);
        c += k;

        size -= 1 + k;
    }

    return r;
}

void avahi_string_list_free(AvahiStringList *l) {
    AvahiStringList *n;

    while (l) {
        n = l->next;
        avahi_free(l);
        l = n;
    }
}

AvahiStringList* avahi_string_list_reverse(AvahiStringList *l) {
    AvahiStringList *r = NULL, *n;

    while (l) {
        n = l->next;
        l->next = r;
        r = l;
        l = n;
    }

    return r;
}

char* avahi_string_list_to_string(AvahiStringList *l) {
    AvahiStringList *n;
    size_t s = 0;
    char *t, *e;

    for (n = l; n; n = n->next) {
        if (n != l)
            s ++;

        s += n->size+2;
    }

    if (!(t = e = avahi_new(char, s+1)))
        return NULL;

    l = avahi_string_list_reverse(l);
    
    for (n = l; n; n = n->next) {
        if (n != l)
            *(e++) = ' ';

        *(e++) = '"';
        strncpy(e, (char*) n->text, n->size);
        e[n->size] = 0;
        e = strchr(e, 0);
        *(e++) = '"';

        assert(e);
    }

    l = avahi_string_list_reverse(l);
    
    *e = 0;

    return t;
}

size_t avahi_string_list_serialize(AvahiStringList *l, void *data, size_t size) {
    size_t used = 0;

    if (data) {
    
        if (l) {
            uint8_t *c;
            AvahiStringList *n;
        
            l = avahi_string_list_reverse(l);
            c = data;
            
            for (n = l; n; n = n->next) {
                size_t k;
                if (size < 1)
                    break;
                
                k = n->size;
                if (k > 255)
                    k = 255;
                
                if (k > size-1)
                    k = size-1;
                
                *(c++) = k;
                memcpy(c, n->text, k);
                c += k;
                
                used += 1+ k;
            }
        
            l = avahi_string_list_reverse(l);
            
        } else {

            /* Empty lists are treated specially. To comply with
             * section 6.1 of the DNS-SD spec, we return a single
             * empty string (i.e. a NUL byte)*/

            if (size > 0) {
                *(uint8_t*) data = 0;
                used = 1;
            }
            
        }
            
    } else {
        AvahiStringList *n;

        if (!l)
            used = 1;
        else {

            for (n = l; n; n = n->next) {
                size_t k;
                
                k = n->size;
                if (k > 255)
                    k = 255;
                
                used += 1+k;
            }
        }
    }

    return used;
}

int avahi_string_list_equal(const AvahiStringList *a, const AvahiStringList *b) {

    for (;;) {
        if (!a && !b)
            return 1;

        if (!a || !b)
            return 0;

        if (a->size != b->size)
            return 0;

        if (a->size != 0 && memcmp(a->text, b->text, a->size) != 0)
            return 0;

        a = a->next;
        b = b->next;
    }
}

AvahiStringList *avahi_string_list_add_many(AvahiStringList *r, ...) {
    va_list va;

    va_start(va, r);
    r = avahi_string_list_add_many_va(r, va);
    va_end(va);
    
    return r;
}

AvahiStringList *avahi_string_list_add_many_va(AvahiStringList *r, va_list va) {
    const char *txt;

    while ((txt = va_arg(va, const char*)))
        r = avahi_string_list_add(r, txt);

    return r;
}

AvahiStringList *avahi_string_list_new(const char *txt, ...) {
    va_list va;
    AvahiStringList *r = NULL;

    if (txt) {
        r = avahi_string_list_add(r, txt);

        va_start(va, txt);
        r = avahi_string_list_add_many_va(r, va);
        va_end(va);
    }

    return r;
}

AvahiStringList *avahi_string_list_new_va(va_list va) {
    return avahi_string_list_add_many_va(NULL, va);
}

AvahiStringList *avahi_string_list_copy(const AvahiStringList *l) {
    AvahiStringList *r = NULL;

    for (; l; l = l->next)
        r = avahi_string_list_add_arbitrary(r, l->text, l->size);

    return avahi_string_list_reverse(r);
}

AvahiStringList *avahi_string_list_new_from_array(const char *array[], int length) {
    AvahiStringList *r = NULL;
    int i;

    assert(array);

    for (i = 0; length >= 0 ? i < length : !!array[i]; i++)
        r = avahi_string_list_add(r, array[i]);

    return r;
}

unsigned avahi_string_list_length(const AvahiStringList *l) {
    unsigned n = 0;

    for (; l; l = l->next)
        n++;

    return n;
}

AvahiStringList *avahi_string_list_add_vprintf(AvahiStringList *l, const char *format, va_list va) {
    size_t len = 80;
    AvahiStringList *r;
    
    assert(format);

    if (!(r = avahi_malloc(sizeof(AvahiStringList) + len)))
        return NULL;

    for (;;) {
        int n;
        AvahiStringList *nr;
        
        n = vsnprintf((char*) r->text, len+1, format, va);

        if (n >= 0 && n < (int) len)
            break;

        if (n >= 0)
            len = n+1;
        else
            len *= 2;

        if (!(nr = avahi_realloc(r, sizeof(AvahiStringList) + len))) {
            avahi_free(r);
            return NULL;
        }

        r = nr;
    }

    
    r->next = l;
    r->size = strlen((char*) r->text); 

    return r;
}

AvahiStringList *avahi_string_list_add_printf(AvahiStringList *l, const char *format, ...) {
    va_list va;
    
    assert(format);

    va_start(va, format);
    l  = avahi_string_list_add_vprintf(l, format, va);
    va_end(va);

    return l;    
}

AvahiStringList *avahi_string_list_find(AvahiStringList *l, const char *key) {
    size_t n;
    
    assert(key);
    n = strlen(key);

    for (; l; l = l->next) {
        if (strcasecmp((char*) l->text, key) == 0)
            return l;

        if (strncasecmp((char*) l->text, key, n) == 0 && l->text[n] == '=')
            return l;
    }

    return NULL;
}

AvahiStringList *avahi_string_list_add_pair(AvahiStringList *l, const char *key, const char *value) {
    assert(key);

    if (value)
        return avahi_string_list_add_printf(l, "%s=%s", key, value);
    else
        return avahi_string_list_add(l, key);
}

AvahiStringList *avahi_string_list_add_pair_arbitrary(AvahiStringList *l, const char *key, const uint8_t *value, size_t size) {
    size_t n;
    assert(key);

    if (!value)
        return avahi_string_list_add(l, key);

    n = strlen(key);
    
    if (!(l = avahi_string_list_add_anonymous(l, n + 1 + size)))
        return NULL;

    memcpy(l->text, key, n);
    l->text[n] = '=';
    memcpy(l->text + n + 1, value, size);

    return l;
}


int avahi_string_list_get_pair(AvahiStringList *l, char **key, char **value, size_t *size) {
    char *e;
    
    assert(l);
    assert(key);

    if (!(e = memchr(l->text, '=', l->size))) {
        
        if (!(*key = avahi_strdup((char*) l->text)))
            return -1;

        if (value)
            *value = NULL;

        if (size)
            *size = 0;

    } else {
        size_t n;

        if (!(*key = avahi_strndup((char*) l->text, e - (char *) l->text)))
            return -1;

        e++; /* Advance after '=' */
        
        n = l->size - (e - (char*) l->text);
        
        if (value) {

            if (!(*value = avahi_memdup(e, n+1))) {
                avahi_free(*key);
                return -1;
            }

            (*value)[n] = 0;
        }

        if (size)
            *size = n;
    }

    return 0;
}

AvahiStringList *avahi_string_list_get_next(AvahiStringList *l) {
    assert(l);
    return l->next;
}

uint8_t *avahi_string_list_get_text(AvahiStringList *l) {
    assert(l);
    return l->text;
}

size_t avahi_string_list_get_size(AvahiStringList *l) {
    assert(l);
    return l->size;
}
