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

#include "strlst.h"

AvahiStringList *avahi_string_list_add_arbitrary(AvahiStringList *l, const guint8*text, guint size) {
    AvahiStringList *n;

    g_assert(text);

    n = g_malloc(sizeof(AvahiStringList) + size);
    n->next = l;
    memcpy(n->text, text, n->size = size);
    
    return n;
}

AvahiStringList *avahi_string_list_add(AvahiStringList *l, const gchar *text) {
    g_assert(text);

    return avahi_string_list_add_arbitrary(l, (const guint8*) text, strlen(text));
}

AvahiStringList *avahi_string_list_parse(gconstpointer data, guint size) {
    AvahiStringList *r = NULL;
    const guint8 *c;
    g_assert(data);

    c = data;
    for (;;) {
        guint k;
        
        if (size < 1)
            break;

        k = *(c++);
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
        g_free(l);
        l = n;
    }
}

static AvahiStringList* string_list_reverse(AvahiStringList *l) {
    AvahiStringList *r = NULL, *n;

    while (l) {
        n = l->next;
        l->next = r;
        r = l;
        l = n;
    }

    return r;
}

gchar* avahi_string_list_to_string(AvahiStringList *l) {
    AvahiStringList *n;
    guint s = 0;
    gchar *t, *e;

    l = string_list_reverse(l);
    
    for (n = l; n; n = n->next) {
        if (n != l)
            s ++;

        s += n->size+3;
    }

    t = e = g_new(gchar, s);

    for (n = l; n; n = n->next) {
        if (n != l)
            *(e++) = ' ';

        *(e++) = '"';
        strncpy(e, (gchar*) n->text, n->size);
        e[n->size] = 0;
        e = strchr(e, 0);
        *(e++) = '"';
    }

    l = string_list_reverse(l);

    *e = 0;

    return t;
}

guint avahi_string_list_serialize(AvahiStringList *l, gpointer data, guint size) {
    guint used = 0;

    if (data) {
        guint8 *c;
        AvahiStringList *n;
    
        g_assert(data);
        
        l = string_list_reverse(l);
        c = data;
        
        for (n = l; n; n = n->next) {
            guint k;
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
        
        l = string_list_reverse(l);
    } else {
        AvahiStringList *n;

        for (n = l; n; n = n->next) {
            guint k;
        
            k = n->size;
            if (k > 255)
                k = 255;
            
            used += 1+k;
        }
    }

    return used;
}

gboolean avahi_string_list_equal(AvahiStringList *a, AvahiStringList *b) {

    for (;;) {
        if (!a && !b)
            return TRUE;

        if (!a || !b)
            return FALSE;

        if (a->size != b->size)
            return FALSE;

        if (a->size != 0 && memcmp(a->text, b->text, a->size) != 0)
            return FALSE;

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
    const gchar *txt;

    while ((txt = va_arg(va, const gchar*)))
        r = avahi_string_list_add(r, txt);

    return r;
}


AvahiStringList *avahi_string_list_new(const gchar *txt, ...) {
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

AvahiStringList *avahi_string_list_copy(AvahiStringList *l) {
    AvahiStringList *r = NULL;

    for (; l; l = l->next)
        r = avahi_string_list_add_arbitrary(r, l->text, l->size);

    return string_list_reverse(r);
}
