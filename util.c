#include <string.h>
#include <unistd.h>

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

