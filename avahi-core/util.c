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

#include "util.h"

gchar *avahi_get_host_name(void) {
#ifdef HOST_NAME_MAX
    char t[HOST_NAME_MAX];
#else
    char t[256];
#endif
    gethostname(t, sizeof(t));
    t[sizeof(t)-1] = 0;
    return avahi_normalize_name(t);
}

static gchar *unescape_uneeded(const gchar *src, gchar *ret_dest, size_t size) {
    gboolean escaped = FALSE;
    
    g_assert(src);
    g_assert(ret_dest);
    g_assert(size > 0);
    
    for (; *src; src++) {

        if (!escaped && *src == '\\')
            escaped = TRUE;
        else if (escaped && (*src == '.' || *src == '\\')) {

            if ((size -= 2) <= 1) break;
            
            *(ret_dest++) = '\\';
            *(ret_dest++) = *src;

            escaped = FALSE;
        } else {
            if (--size <= 1) break;

            *(ret_dest++) = *src;
            escaped = FALSE;
        }

    }

    *ret_dest = 0;
    
    return ret_dest;
}

gchar *avahi_normalize_name(const gchar *s) {
    gchar tmp[256];
    gchar *n, *t;
    guint l;
    g_assert(s);

    unescape_uneeded(s, tmp, sizeof(tmp));

    n = g_utf8_normalize(tmp, -1, G_NORMALIZE_DEFAULT);

    if ((l = strlen(n)) == 0) {
        g_free(n);
        return g_strdup(".");
    }

    if (n[l-1] == '.')
        return n;

    t = g_strdup_printf("%s.", n);
    g_free(n);
    return t;
}

gint avahi_timeval_compare(const GTimeVal *a, const GTimeVal *b) {
    g_assert(a);
    g_assert(b);

    if (a->tv_sec < b->tv_sec)
        return -1;

    if (a->tv_sec > b->tv_sec)
        return 1;

    if (a->tv_usec < b->tv_usec)
        return -1;

    if (a->tv_usec > b->tv_usec)
        return 1;

    return 0;
}

glong avahi_timeval_diff(const GTimeVal *a, const GTimeVal *b) {
    g_assert(a);
    g_assert(b);

    if (avahi_timeval_compare(a, b) < 0)
        return avahi_timeval_diff(b, a);

    return ((glong) a->tv_sec - b->tv_sec)*1000000 + a->tv_usec - b->tv_usec;
}


gint avahi_set_cloexec(gint fd) {
    gint n;

    g_assert(fd >= 0);
    
    if ((n = fcntl(fd, F_GETFD)) < 0)
        return -1;

    if (n & FD_CLOEXEC)
        return 0;

    return fcntl(fd, F_SETFD, n|FD_CLOEXEC);
}

gint avahi_set_nonblock(gint fd) {
    gint n;

    g_assert(fd >= 0);

    if ((n = fcntl(fd, F_GETFL)) < 0)
        return -1;

    if (n & O_NONBLOCK)
        return 0;

    return fcntl(fd, F_SETFL, n|O_NONBLOCK);
}

gint avahi_wait_for_write(gint fd) {
    fd_set fds;
    gint r;
    
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    
    if ((r = select(fd+1, NULL, &fds, NULL, NULL)) < 0) {
        g_message("select() failed: %s", strerror(errno));

        return -1;
    }
    
    g_assert(r > 0);

    return 0;
}

GTimeVal *avahi_elapse_time(GTimeVal *tv, guint msec, guint jitter) {
    g_assert(tv);

    g_get_current_time(tv);

    if (msec)
        g_time_val_add(tv, msec*1000);

    if (jitter)
        g_time_val_add(tv, g_random_int_range(0, jitter) * 1000);
        
    return tv;
}

gint avahi_age(const GTimeVal *a) {
    GTimeVal now;
    
    g_assert(a);

    g_get_current_time(&now);

    return avahi_timeval_diff(&now, a);
}

/* Read the first label from string *name, unescape "\" and write it to dest */
gchar *avahi_unescape_label(const gchar **name, gchar *dest, guint size) {
    guint i = 0;
    gchar *d;
    
    g_assert(dest);
    g_assert(size > 0);
    g_assert(name);

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

    g_assert(i < size);

    *d = 0;

    return dest;
}

/* Escape "\" and ".", append \0 */
gchar *avahi_escape_label(const guint8* src, guint src_length, gchar **ret_name, guint *ret_size) {
    gchar *r;

    g_assert(src);
    g_assert(ret_name);
    g_assert(*ret_name);
    g_assert(ret_size);
    g_assert(*ret_size > 0);

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

static gint utf8_strcasecmp(const gchar *a, const gchar *b) {
    gchar *ta, *tb;
    gint r;
    
    g_assert(a);
    g_assert(b);

    ta = g_utf8_casefold(a, -1);
    tb = g_utf8_casefold(b, -1);
    r = g_utf8_collate(ta, tb);
    g_free(ta);
    g_free(tb);

    return r;
}

gboolean avahi_domain_equal(const gchar *a, const gchar *b) {
    g_assert(a);
    g_assert(b);

    if (a == b)
        return TRUE;
    
    for (;;) {
        gchar ca[65], cb[65], *pa, *pb;

        pa = avahi_unescape_label(&a, ca, sizeof(ca));
        pb = avahi_unescape_label(&b, cb, sizeof(cb));

        if (!pa && !pb)
            return TRUE;
        else if ((pa && !pb) || (!pa && pb))
            return FALSE;

        if (utf8_strcasecmp(pa, pb))
            return FALSE;
    }

    return TRUE;
}

gint avahi_binary_domain_cmp(const gchar *a, const gchar *b) {
    g_assert(a);
    g_assert(b);

    if (a == b)
        return 0;

    for (;;) {
        gchar ca[65], cb[65], *pa, *pb;
        gint r;

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

void avahi_hexdump(gconstpointer p, guint size) {
    const guint8 *c = p;
    g_assert(p);

    printf("Dumping %u bytes from %p:\n", size, p);
    
    while (size > 0) {
        guint i;

        for (i = 0; i < 16; i++) { 
            if (i < size)
                printf("%02x ", c[i]);
            else
                printf("   ");
        }

        for (i = 0; i < 16; i++) {
            if (i < size)
                printf("%c", c[i] >= 32 && c[i] < 127 ? c[i] : '.');
            else
                printf(" ");
        }
        
        printf("\n");

        c += 16;

        if (size <= 16)
            break;
        
        size -= 16;
    }
}

gint avahi_domain_hash(const gchar *s) {
    guint hash = 0;
    
    for (;;) {
        gchar c[65], *n, *m;

        if (!avahi_unescape_label(&s, c, sizeof(c)))
            return hash;

        if (!c[0])
            continue;
        
        n = g_utf8_normalize(c, -1, G_NORMALIZE_DEFAULT);
        m = g_utf8_strdown(n, -1);

        hash += g_str_hash(m);

        g_free(m);
        g_free(n);
    }
}
