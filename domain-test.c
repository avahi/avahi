#include "util.h"

int main(int argc, char *argv[]) {
    gchar *s;
    
    g_message("host name: %s", s = flx_get_host_name());
    g_free(s);

    g_message("%s", s = flx_normalize_name("foo.foo."));
    g_free(s);
    
    g_message("%s", s = flx_normalize_name("foo.foo."));
    g_free(s);


    g_message("%i", flx_domain_equal("\\aaa bbb\\.cccc\\\\.dee.fff.", "aaa\\ bbb\\.cccc\\\\.dee.fff"));

    return 0;
}
