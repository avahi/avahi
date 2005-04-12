#include <glib.h>
#include <stdio.h>

#include "strlst.h"

int main(int argc, char *argv[]) {
    gchar *t;
    guint8 data[1024];
    flxStringList *a = NULL, *b;
    guint size, n;

    a = flx_string_list_add(a, "foo");
    a = flx_string_list_add(a, "bar");
    a = flx_string_list_add(a, "baz");

    t = flx_string_list_to_string(a);
    printf("--%s--\n", t);
    g_free(t);

    size = flx_string_list_serialize(a, data, sizeof(data));

    printf("%u\n", size);

    for (t = (gchar*) data, n = 0; n < size; n++, t++) {
        if (*t <= 32)
            printf("(%u)", *t);
        else
            printf("%c", *t);
    }

    printf("\n");
    
    b = flx_string_list_parse(data, size);

    g_assert(flx_string_list_equal(a, b));
    
    t = flx_string_list_to_string(b);
    printf("--%s--\n", t);
    g_free(t);


    flx_string_list_free(a);
    flx_string_list_free(b);
    
    return 0;
}
