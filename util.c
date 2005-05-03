#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

#include "util.h"

gchar *flx_get_host_name(void) {
#ifdef HOST_NAME_MAX
    char t[HOST_NAME_MAX];
#else
    char t[256];
#endif
    gethostname(t, sizeof(t));
    t[sizeof(t)-1] = 0;
    return flx_normalize_name(t);
}

gchar *flx_normalize_name(const gchar *s) {
    size_t l;
    g_assert(s);

    l = strlen(s);

    if (!l)
        return g_strdup(".");

    if (s[l-1] == '.')
        return g_strdup(s);
    
    return g_strdup_printf("%s.", s);
}

gint flx_timeval_compare(const GTimeVal *a, const GTimeVal *b) {
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

glong flx_timeval_diff(const GTimeVal *a, const GTimeVal *b) {
    g_assert(a);
    g_assert(b);

    if (flx_timeval_compare(a, b) < 0)
        return flx_timeval_diff(b, a);

    return ((glong) a->tv_sec - b->tv_sec)*1000000 + a->tv_usec - b->tv_usec;
}


gint flx_set_cloexec(gint fd) {
    gint n;

    g_assert(fd >= 0);
    
    if ((n = fcntl(fd, F_GETFD)) < 0)
        return -1;

    if (n & FD_CLOEXEC)
        return 0;

    return fcntl(fd, F_SETFD, n|FD_CLOEXEC);
}

gint flx_set_nonblock(gint fd) {
    gint n;

    g_assert(fd >= 0);

    if ((n = fcntl(fd, F_GETFL)) < 0)
        return -1;

    if (n & O_NONBLOCK)
        return 0;

    return fcntl(fd, F_SETFL, n|O_NONBLOCK);
}

gint flx_wait_for_write(gint fd) {
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

GTimeVal *flx_elapse_time(GTimeVal *tv, guint msec, guint jitter) {
    g_assert(tv);

    g_get_current_time(tv);

    if (msec)
        g_time_val_add(tv, msec*1000);

    if (jitter)
        g_time_val_add(tv, g_random_int_range(0, jitter) * 1000);
        
    return tv;
}

gint flx_age(const GTimeVal *a) {
    GTimeVal now;
    
    g_assert(a);

    g_get_current_time(&now);

    return flx_timeval_diff(&now, a);
}

gboolean flx_domain_cmp(const gchar *a, const gchar *b) {
    int escaped_a = 0, escaped_b = 0;
    g_assert(a);
    g_assert(b);

    for (;;) {
        /* Check for escape characters "\" */
        if ((escaped_a = *a == '\\'))
            a ++;

        if ((escaped_b = *b == '\\'))
            b++;

        /* Check for string end */
        if (*a == 0 && *b == 0)
            return 0;
        
        if (*a == 0 && !escaped_b && *b == '.' && *(b+1) == 0)
            return 0;

        if (!escaped_a && *a == '.' && *(a+1) == 0 && *b == 0)
            return 0;

        /* Compare characters */
        if (escaped_a == escaped_b && *a != *b)
            return *a < *b ? -1 : 1;

        /* Next characters */
        a++;
        b++;
        
    }
}

gboolean flx_domain_equal(const gchar *a, const gchar *b) {
    return flx_domain_cmp(a, b) == 0;
}

guint flx_domain_hash(const gchar *p) {
    char t[256];
    strncpy(t, p, sizeof(t)-1);
    t[sizeof(t)-1] = 0;

    return g_int_hash(t);
}

void flx_hexdump(gconstpointer p, guint size) {
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
