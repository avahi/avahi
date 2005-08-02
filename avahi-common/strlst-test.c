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

#include <glib.h>
#include <stdio.h>

#include "strlst.h"

int main(int argc, char *argv[]) {
    gchar *t;
    guint8 data[1024];
    AvahiStringList *a = NULL, *b;
    guint size, n;

    a = avahi_string_list_new("prefix", "a", "b", NULL);
    
    a = avahi_string_list_add(a, "start");
    a = avahi_string_list_add(a, "foo");
    a = avahi_string_list_add(a, "bar");
    a = avahi_string_list_add(a, "quux");
    a = avahi_string_list_add_arbitrary(a, (const guint8*) "null\0null", 9);
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
