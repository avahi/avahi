#include <string.h>
#include <stdarg.h>

#include "strlst.h"

flxStringList *flx_string_list_add_arbitrary(flxStringList *l, const guint8*text, guint size) {
    flxStringList *n;

    g_assert(text);

    n = g_malloc(sizeof(flxStringList) + size);
    n->next = l;
    memcpy(n->text, text, n->size = size);
    
    return n;
}

flxStringList *flx_string_list_add(flxStringList *l, const gchar *text) {
    g_assert(text);

    return flx_string_list_add_arbitrary(l, (const guint8*) text, strlen(text));
}

flxStringList *flx_string_list_parse(gconstpointer data, guint size) {
    flxStringList *r = NULL;
    const guint8 *c;
    g_assert(data);

    c = data;
    for (;;) {
        guint k;
        
        if (size < 1)
            break;

        k = *(c++);
        r = flx_string_list_add_arbitrary(r, c, k);
        c += k;

        size -= 1 + k;
    }

    return r;
}

void flx_string_list_free(flxStringList *l) {
    flxStringList *n;

    while (l) {
        n = l->next;
        g_free(l);
        l = n;
    }
}

static flxStringList* string_list_reverse(flxStringList *l) {
    flxStringList *r = NULL, *n;

    while (l) {
        n = l->next;
        l->next = r;
        r = l;
        l = n;
    }

    return r;
}

gchar* flx_string_list_to_string(flxStringList *l) {
    flxStringList *n;
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
        strncpy(e, n->text, n->size);
        e[n->size] = 0;
        e = strchr(e, 0);
        *(e++) = '"';
    }

    l = string_list_reverse(l);

    *e = 0;

    return t;
}

guint flx_string_list_serialize(flxStringList *l, gpointer data, guint size) {
    guint used = 0;

    if (data) {
        guint8 *c;
        flxStringList *n;
    
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
        flxStringList *n;

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

gboolean flx_string_list_equal(flxStringList *a, flxStringList *b) {

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

flxStringList *flx_string_list_add_many(flxStringList *r, ...) {
    va_list va;

    va_start(va, r);
    r = flx_string_list_add_many_va(r, va);
    va_end(va);
    
    return r;
}

flxStringList *flx_string_list_add_many_va(flxStringList *r, va_list va) {
    const gchar *txt;

    while ((txt = va_arg(va, const gchar*)))
        r = flx_string_list_add(r, txt);

    return r;
}


flxStringList *flx_string_list_new(const gchar *txt, ...) {
    va_list va;
    flxStringList *r = NULL;

    if (txt) {
        r = flx_string_list_add(r, txt);

        va_start(va, txt);
        r = flx_string_list_add_many_va(r, va);
        va_end(va);
    }

    return r;
}

flxStringList *flx_string_list_new_va(va_list va) {
    return flx_string_list_add_many_va(NULL, va);
}

flxStringList *flx_string_list_copy(flxStringList *l) {
    flxStringList *r = NULL;

    for (; l; l = l->next)
        r = flx_string_list_add_arbitrary(r, l->text, l->size);

    return string_list_reverse(r);
}
