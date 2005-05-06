#include <glib.h>
#include <stdio.h>

#include "strlst.h"

int main(int argc, char *argv[]) {
    gchar *t;
    guint8 data[1024];
    AvahiStringList *a = NULL, *b;
    guint size, n;

    a = avahi_string_list_add(a, "start");
    a = avahi_string_list_add(a, "foo");
    a = avahi_string_list_add(a, "bar");
    a = avahi_string_list_add(a, "quux");
    a = avahi_string_list_add_arbitrary(a, "null\0null", 9);
    a = avahi_string_list_add(a, "end");

    t = avahi_string_list_to_string(a);
    printf("--%s--\n", t);
    g_free(t);

    size = avahi_string_list_serialize(a, data, sizeof(data));

    printf("%u\n", size);

    for (t = (gchar*) data, n = 0; n < size; n++, t++) {
        if (*t <= 32)
            printf("(%u)", *t);
        else
            printf("%c", *t);
    }

    printf("\n");
    
    b = avahi_string_list_parse(data, size);

    g_assert(avahi_string_list_equal(a, b));
    
    t = avahi_string_list_to_string(b);
    printf("--%s--\n", t);
    g_free(t);

    avahi_string_list_free(b);

    b = avahi_string_list_copy(a);

    g_assert(avahi_string_list_equal(a, b));

    t = avahi_string_list_to_string(b);
    printf("--%s--\n", t);
    g_free(t);
    
    avahi_string_list_free(a);
    avahi_string_list_free(b);
    
    return 0;
}
