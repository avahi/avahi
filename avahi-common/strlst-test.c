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

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "strlst.h"
#include "malloc.h"

/*
 * Build a representative AvahiStringList containing:
 *  - plain strings
 *  - empty strings
 *  - key/value pairs
 *  - arbitrary binary data
 *  - escaped / non-printable characters
 *
 * This list is reused across most tests.
 */
static AvahiStringList* build_test_string_list(void) {
    AvahiStringList *a = NULL;

    a = avahi_string_list_new("prefix", "a", "b", NULL);

    a = avahi_string_list_add(a, "start");
    a = avahi_string_list_add(a, "foo=99");
    a = avahi_string_list_add(a, "bar");
    a = avahi_string_list_add(a, "");
    a = avahi_string_list_add(a, "");
    a = avahi_string_list_add(a, "quux");
    a = avahi_string_list_add(a, "");
    a = avahi_string_list_add_arbitrary(a, (const uint8_t*) "null\0null", 9);
    a = avahi_string_list_add_printf(a, "seven=%i %c", 7, 'x');
    a = avahi_string_list_add_pair(a, "blubb", "blaa");
    a = avahi_string_list_add_pair(a, "uxknurz", NULL);
    a = avahi_string_list_add_pair_arbitrary(
            a, "uxknurz2", (const uint8_t*) "blafasel\0oerks", 14);
    a = avahi_string_list_add(
            a,
            "i am a \"string\" with embedded double-quotes (\\\")\n"
            "and newlines (\\n).");
    a = avahi_string_list_add(a, "gh_issue_169=\x1f\x20\x7e\x7f\xff");
    a = avahi_string_list_add(a, "end");

    return a;
}

/*
 * Verify human-readable string rendering and escaping behavior.
 */
static void test_string_rendering(AvahiStringList *a) {
    char *t;

    t = avahi_string_list_to_string(a);
    printf("--%s--\n", t);
    avahi_free(t);

    /* Ensure non-printable bytes are escaped as expected */
    t = avahi_string_list_to_string(a);
    assert(strstr(t, "gh_issue_169=\\031 ~\\127\\255"));
    avahi_free(t);
}

/*
 * Serialize the list to wire format, dump it for debugging,
 * and parse it back. Ensures round-trip correctness.
 */
static void test_serialize_and_parse(AvahiStringList *a) {
    uint8_t data[1024];
    uint8_t *u;
    size_t size, n;
    AvahiStringList *b;
    char *t;

    n = avahi_string_list_serialize(a, NULL, 0);
    size = avahi_string_list_serialize(a, data, sizeof(data));
    assert(size == n);

    printf("%zu\n", size);

    for (u = data, n = 0; n < size; n++, u++) {
        if (*u < 32 || *u >= 127)
            printf("(%u)", *u);
        else
            printf("%c", *u);
    }
    printf("\n");

    assert(avahi_string_list_parse(data, size, &b) == 0);

    printf("equal: %i\n", avahi_string_list_equal(a, b));

    t = avahi_string_list_to_string(b);
    printf("--%s--\n", t);
    avahi_free(t);

    avahi_string_list_free(b);
}

/*
 * Verify copy semantics and equality checks.
 */
static void test_copy_and_equality(AvahiStringList *a) {
    AvahiStringList *b;
    char *t;

    b = avahi_string_list_copy(a);
    assert(avahi_string_list_equal(a, b));

    t = avahi_string_list_to_string(b);
    printf("--%s--\n", t);
    avahi_free(t);

    avahi_string_list_free(b);
}

/*
 * Validate lookup and key/value extraction behavior.
 */
static void test_find_and_get_pair(AvahiStringList *a) {
    AvahiStringList *p;
    char *t, *v;
    int r;

    /* Key with value */
    p = avahi_string_list_find(a, "seven");
    assert(p);

    r = avahi_string_list_get_pair(p, &t, &v, NULL);
    assert(r >= 0);
    assert(t);
    assert(v);

    printf("<%s>=<%s>\n", t, v);
    avahi_free(t);
    avahi_free(v);

    /* Key without value */
    p = avahi_string_list_find(a, "quux");
    assert(p);

    r = avahi_string_list_get_pair(p, &t, &v, NULL);
    assert(r >= 0);
    assert(t);
    assert(!v);

    printf("<%s>=<%s>\n", t, v);
    avahi_free(t);
    avahi_free(v);
}

/*
 * Edge case: serializing and parsing a NULL list.
 */
static void test_null_list_serialization(void) {
    uint8_t data[1024];
    size_t size, n;
    AvahiStringList *a;

    n = avahi_string_list_serialize(NULL, NULL, 0);
    size = avahi_string_list_serialize(NULL, data, sizeof(data));
    assert(size == 1);
    assert(size == n);

    assert(avahi_string_list_parse(data, size, &a) == 0);
    assert(!a);
}

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[]) {
    AvahiStringList *a;

    a = build_test_string_list();

    test_string_rendering(a);
    test_serialize_and_parse(a);
    test_copy_and_equality(a);
    test_find_and_get_pair(a);
    test_null_list_serialization();

    avahi_string_list_free(a);

    return 0;
}

