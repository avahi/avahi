#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "util.h"

gchar *flx_get_host_name(void) {
    char t[256];
    gethostname(t, sizeof(t));
    return g_strndup(t, sizeof(t));
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
    g_assert(flx_timeval_compare(a, b) >= 0);

    return (a->tv_sec - b->tv_sec)*1000000 + a->tv_usec - b->tv_usec;
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
