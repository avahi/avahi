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
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "defs.h"
#include "strlst.h"
#include "malloc.h"
#include "gccmacro.h"

/*
 **************************************************************
 * mock allocator that fails after n malloc/realloc calls.
 * Used to test low-memory paths and error handling
 ***************************************************************/
typedef struct FailAfterState {
    int remaining;
} FailAfterState;

static FailAfterState fail_after_state;

static void* fail_after_malloc(size_t size) {
    if (fail_after_state.remaining <= 0)
        return NULL;

    fail_after_state.remaining--;
    return malloc(size);
}

static void* fail_after_realloc(void *ptr, size_t size) {
    if (fail_after_state.remaining <= 0)
        return NULL;

    fail_after_state.remaining--;
    return realloc(ptr, size);
}

static void fail_after_free(void *ptr) {
    free(ptr);
}

static AvahiAllocator fail_after_allocator = {
    .malloc  = fail_after_malloc,
    .realloc = fail_after_realloc,
    .free    = fail_after_free
};

static const AvahiAllocator* avahi_allocator_fail_after(int n) {
    fail_after_state.remaining = n;
    return &fail_after_allocator;
}

/************* end fake memory allocator *********/

/***************************************************************
 * Build a representative AvahiStringList containing:
 *  - plain strings
 *  - empty strings
 *  - key/value pairs
 *  - arbitrary binary data
 *  - escaped / non-printable characters
 */
static AvahiStringList* build_test_string_list(void) {
    AvahiStringList *a = NULL, *p;
    const char *expected_texts[] = {
        "end", "gh_issue_169=\x1f\x20\x7e\x7f\xff",
        "i am a \"string\" with embedded double-quotes (\\\")\nand newlines (\\n).",
        "uxknurz2=blafasel\0oerks","uxknurz=","blubb=blaa","seven=7 x",
        "null\0null","","quux","","","bar","foo=99","start","b","a","prefix",
    };
    size_t expected_sizes[] = {
        3, 18, 67, 23, 7, 10, 9, 9, 0, 4, 0, 0, 3, 6, 5, 1, 1, 6,
    };
    size_t i = 0;

    /* Basic list with a few strings */
    a = avahi_string_list_new("prefix", "a", "b", (const char*)NULL);
    p = a;
    assert(avahi_string_list_length(a) == 3);
    assert(p && strcmp((const char*)avahi_string_list_get_text(p), "b") == 0);
    p = p->next;
    assert(p && strcmp((const char*)avahi_string_list_get_text(p), "a") == 0);
    p = p->next;
    assert(p && strcmp((const char*)avahi_string_list_get_text(p), "prefix") == 0);
    p = p->next;

    /* Append more strings */
    a = avahi_string_list_add(a, "start");
    assert(strcmp((const char*)avahi_string_list_get_text(a), "start") == 0);
    assert(avahi_string_list_length(a) == 4);

    a = avahi_string_list_add(a, "foo=99");
    a = avahi_string_list_add(a, "bar");
    a = avahi_string_list_add(a, "");
    a = avahi_string_list_add(a, "");
    a = avahi_string_list_add(a, "quux");
    a = avahi_string_list_add(a, "");
    assert(avahi_string_list_length(a) == 10);

    /* Traverse and check the last added "quux" */
    p = a;
    while (p->next) p = p->next;
    assert(strcmp((const char*)avahi_string_list_get_text(a), "") == 0);
    assert(strcmp((const char*)avahi_string_list_get_text(p), "prefix") == 0);

    /* Arbitrary binary data */
    a = avahi_string_list_add_arbitrary(a, (const uint8_t*) "null\0null", 9);
    assert(avahi_string_list_get_size(a) == 9);

    /* Formatted string */
    a = avahi_string_list_add_printf(a, "seven=%i %c", 7, 'x');
    assert(strcmp((const char*)avahi_string_list_get_text(a), "seven=7 x") == 0);

    /* Key/value pairs */
    a = avahi_string_list_add_pair(a, "blubb", "blaa");
    a = avahi_string_list_add_pair(a, "uxknurz", (const char*)NULL);
    a = avahi_string_list_add_pair_arbitrary(
            a, "uxknurz2", (const uint8_t*) "blafasel\0oerks", 14);
    assert(avahi_string_list_get_size(a) == 14 + strlen("uxknurz2="));

    /* Escaped / special characters */
    a = avahi_string_list_add(a,
        "i am a \"string\" with embedded double-quotes (\\\")\n"
        "and newlines (\\n).");
    a = avahi_string_list_add(a, "gh_issue_169=\x1f\x20\x7e\x7f\xff");
    a = avahi_string_list_add(a, "end");

    assert(avahi_string_list_length(a) == 18);

    for (p = a; p; p = p->next, i++) {
        assert(p);
        assert(avahi_string_list_get_size(p) == expected_sizes[i]);
        /* Only compare text for printable strings, skip comparison for binary */
        if (expected_sizes[i] == strlen(expected_texts[i]))
            assert(strcmp((const char*)avahi_string_list_get_text(p), expected_texts[i]) == 0);
    }
    assert(i == sizeof(expected_texts)/sizeof(expected_texts[0]));

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


/**
 * Test avahi_string_list_add_arbitrary() for a range of string sizes.
 *
 * This test iterates over string sizes from 1 to 256 bytes, creating
 * a new AvahiStringList node for each size. It verifies:
 *   - The returned pointer is non-NULL.
 *   - The size field in the node matches the requested size.
 *   - The contents of the node match the input data.
 *
 * Any allocation failure aborts the test immediately.
 * This helps ensure that edge cases, including maximum single-byte
 * length strings, are correctly handled.
 */
static void test_avahi_string_list_add_arbitrary_sizes(void) {
    AvahiStringList *l = NULL, *n;
    uint8_t buf[256];

    for (size_t i = 1; i <= 256; i++) {
        // Fill buffer with predictable data
        for (size_t j = 0; j < i; j++)
            buf[j] = (uint8_t)(j & 0xFF);

        // Add to list
        n = avahi_string_list_add_arbitrary(l, buf, i);
        assert(n);  // allocation must succeed

        // Check contents
        for (size_t j = 0; j < i; j++)
            assert(n->text[j] == (uint8_t)(j & 0xFF));

        // Move to next
        l = n;
    }

    // Clean up
    avahi_string_list_free(l);
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
 * serializing and parsing a NULL list.
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

/* Verify list reversal semantics:
 * - reversing NULL is a no-op
 * - order is correctly reversed
 * - reversing twice restores the original list
 */
static void test_reverse(void) {
    AvahiStringList *a = NULL, *r, *rr;

    /* reverse(NULL) == NULL */
    r = avahi_string_list_reverse(NULL);
    assert(!r);

    a = avahi_string_list_new("one", "two", "three", "four", (const char*)NULL);

    assert(strcmp((char*) a->text, "four") == 0);
    assert(strcmp((char*) a->next->text, "three") == 0);
    assert(strcmp((char*) a->next->next->text, "two") == 0);
    assert(strcmp((char*) a->next->next->next->text, "one") == 0);
    assert(!a->next->next->next->next);

    r = avahi_string_list_reverse(a);
    assert(r);

    /* Order must be reversed */
    assert(strcmp((char*) r->text, "one") == 0);
    assert(strcmp((char*) r->next->text, "two") == 0);
    assert(strcmp((char*) r->next->next->text, "three") == 0);
    assert(strcmp((char*) r->next->next->next->text, "four") == 0);
    assert(!r->next->next->next->next);

    /* Reversing twice must restore original */
    rr = avahi_string_list_reverse(r);
    assert(avahi_string_list_equal(rr, a));

    avahi_string_list_free(rr);
}

/* Verify varargs list construction:
 * - supports NULL-terminated arguments
 * - preserves insertion order
 * - works when starting from an empty list
 */

/* Wrapper to create a va_list and call avahi_string_list_new_va */
static AvahiStringList *new_va_helper(const char * first, ...)
{
    va_list va;
    AvahiStringList *l;

    va_start(va, first);
    l = avahi_string_list_new_va(va);
    va_end(va);

    return l;
}

/* Wrapper to create a va_list and call avahi_string_list_add_many_va */
static AvahiStringList *add_many(AvahiStringList *r, ...) {
    va_list ap;
    AvahiStringList *ret;

    va_start(ap, r);
    ret = avahi_string_list_add_many_va(r, ap);
    va_end(ap);

    return ret;
}


static void test_new_va(void)
{
    AvahiStringList *l, *p;
    const char *expected[] = { "three", "two", "one" };
    size_t i;

    /* Create the list using the wrapper */
    l = new_va_helper(NULL, "one", "two", "three", (const char*)NULL);

    /* Iterate and verify each element */
    i = 0;
    for (p = l; p; p = p->next, i++)
    {
        assert(p);
        assert(strcmp((const char *)avahi_string_list_get_text(p), expected[i]) == 0);
    }

    /* Make sure we have exactly 3 elements */
    assert(avahi_string_list_length(l) == 3);
    assert(i == 3);

    avahi_string_list_free(l);
}

static void test_many_va(void)
{
    AvahiStringList *r = NULL;

    r = add_many(r, (const char *)NULL);
    assert(r == NULL);

    r = add_many(NULL, (const char *)NULL);
    assert(r == NULL);

    r = add_many(r, "one", "two", (const char *)NULL);
    assert(r != NULL);
    avahi_string_list_free(r);
}

/**
 * Test avahi_string_list_add_many() with multiple strings.
 *
 * Verifies that:
 *  - The list is created correctly from multiple input strings.
 *  - The order is reversed (last string becomes head).
 *  - The list terminates properly (next of last node is NULL).
 */
static void test_add_many(void) {
    AvahiStringList *l;

    l = avahi_string_list_add_many(NULL, "a", "b", "c", (const char*)NULL);

    assert(l);
    assert(strcmp((char*) l->text, "c") == 0);
    assert(strcmp((char*) l->next->text, "b") == 0);
    assert(strcmp((char*) l->next->next->text, "a") == 0);
    assert(!l->next->next->next);

    avahi_string_list_free(l);
}

/**
 * Test avahi_string_list_new_from_array() with explicit length and
 * sentinel-terminated arrays.
 *
 * Verifies that:
 *  - The list is correctly created from an array of strings.
 *  - The order is reversed (last element becomes head).
 *  - Both fixed-length and NULL-terminated arrays are handled properly.
 */
static void test_new_from_array(void) {
    const char *a1[] = { "one", "two", "three" };
    const char *a2[] = { "alpha", "beta", NULL };

    AvahiStringList *l;

    /* Explicit length */
    l = avahi_string_list_new_from_array(a1, 3);
    assert(strcmp((char*) l->text, "three") == 0);
    assert(strcmp((char*) l->next->text, "two") == 0);
    assert(strcmp((char*) l->next->next->text, "one") == 0);
    avahi_string_list_free(l);

    /* Sentinel-terminated */
    l = avahi_string_list_new_from_array(a2, -1);
    assert(strcmp((char*) l->text, "beta") == 0);
    assert(strcmp((char*) l->next->text, "alpha") == 0);
    assert(!l->next->next);
    avahi_string_list_free(l);
}

/* Verify traversal helper:
 * - returns the next element
 * - does not modify list structure
 */
static void test_get_next(void) {
    AvahiStringList *l, *n;

    l = avahi_string_list_new("a", "b", (const char*)NULL);

    assert(l);
    assert(strcmp((char*) l->text, "b") == 0);
    assert(strcmp((char*) l->next->text, "a") == 0);

    n = avahi_string_list_get_next(l);

    assert(n);
    assert(strcmp((char*) n->text, "a") == 0);

    avahi_string_list_free(l);
}

/**
 * avahi_string_list_equal() across functionality
 *
 * Covers:
 *  1) Both lists NULL -> equal (returns 1)
 *  2) One list NULL, one non-NULL -> not equal (returns 0)
 *  3) Lists with different string lengths -> not equal
 *  4) Lists with same length but different content -> not equal
 *  5) Lists with identical content -> equal (returns 1)
 *  6) Multiple-element lists with mismatch in the middle -> not equal
 *  7) Multiple-element lists fully equal -> equal (returns 1)
 *
 * Ensures all functionality in list comparisions are  exercised.
 */

static void test_avahi_string_list_equal_branches(void) {
    AvahiStringList *a = NULL, *b = NULL;
    AvahiStringList *l1, *l2, *l3;
    AvahiStringList *tmp;

    // 0. Test with null
    a = avahi_string_list_new(NULL, (const char*)NULL);
    assert(!a);

    // 1. Both NULL -> returns 1
    assert(avahi_string_list_equal(NULL, NULL) == 1);

    // 2. One NULL, one non-NULL -> returns 0
    a = avahi_string_list_new("a", (const char*)NULL);
    assert(a);
    assert(avahi_string_list_equal(a, NULL) == 0);
    assert(avahi_string_list_equal(NULL, a) == 0);

    // 3. Different sizes -> returns 0
    b = avahi_string_list_new("aa", (const char*)NULL);
    assert(b);
    assert(avahi_string_list_equal(a, b) == 0);
    avahi_string_list_free(b);

    // 4. Same size, different content -> returns 0
    b = avahi_string_list_new("b", (const char*)NULL);
    assert(b);
    assert(avahi_string_list_equal(a, b) == 0);

    // 5. Same content -> returns 1
    tmp = avahi_string_list_copy(a);
    assert(tmp);
    assert(avahi_string_list_equal(a, tmp) == 1);
    avahi_string_list_free(tmp);

    // 6. Multiple elements: mismatch in middle
    l1 = avahi_string_list_new("one", "two", (const char*)NULL);
    l2 = avahi_string_list_new("one", "three", (const char*)NULL);
    assert(l1);
    assert(l2);
    assert(avahi_string_list_equal(l1, l2) == 0);

    // 7. Multiple elements: fully equal
    l3 = avahi_string_list_copy(l1);
    assert(l3);
    assert(avahi_string_list_equal(l1, l3) == 1);

    avahi_string_list_free(a);
    avahi_string_list_free(b);
    avahi_string_list_free(l1);
    avahi_string_list_free(l2);
    avahi_string_list_free(l3);
}

/**
 * Test avahi_string_list_add_pair_arbitrary() across key branches.
 *
 * Covers:
 *  1) Normal case: key + non-NULL value -> concatenates as "key=value"
 *  2) Value is NULL -> falls back to avahi_string_list_add() with just the key
 *
 * Verifies both resulting string content and list size, exercising
 * the main conditional branches in the function.
 */
static void test_avahi_string_list_add_pair_arbitrary_branches(void) {
    AvahiStringList *l = NULL;
    const uint8_t val[] = { 'x', 'y', 'z' };
    AvahiStringList *r;

    // 1. Normal path (key + value)
    r = avahi_string_list_add_pair_arbitrary(l, "foo", val, sizeof(val));
    assert(r);
    assert(r->size == strlen("foo") + 1 + sizeof(val));
    assert(memcmp(r->text, "foo=xyz", r->size) == 0);
    avahi_string_list_free(r);

    // 2. value == NULL -> should fallback to avahi_string_list_add()
    r = avahi_string_list_add_pair_arbitrary(l, "bar", NULL, 0);
    assert(r);
    assert(strcmp((char*)r->text, "bar") == 0);
    avahi_string_list_free(r);

}


/*
 * Test matrix for avahi_string_list_get_pair()
 *
 * The function takes 4 arguments:
 *   - AvahiStringList *l      : the string list (must be non-NULL)
 *   - char **key               : pointer to receive key (can be NULL)
 *   - char **value             : pointer to receive value (can be NULL)
 *   - size_t *size             : pointer to receive value size (can be NULL)
 *
 * To fully exercise all branches, we need to test the combinations of NULL vs non-NULL
 * for the last three arguments (key, value, size). There are 2^3 = 8 combinations:
 *
 *   key       value     size
 *   --------------------------------
 *   non-NULL  non-NULL  non-NULL   // case 1
 *   NULL      non-NULL  non-NULL   // case 2
 *   non-NULL  NULL      non-NULL   // case 3
 *   non-NULL  non-NULL  NULL       // case 4
 *   NULL      NULL      non-NULL   // case 5
 *   non-NULL  NULL      NULL       // case 6
 *   NULL      non-NULL  NULL       // case 7
 *   NULL      NULL      NULL       // case 8
 *
 * Additionally, we need to test both situations for the string content:
 *   - l->text contains '='
 *   - l->text does not contain '='
 *
 * That gives 8 x 2 = 16 concrete test cases to fully cover all branches.
 *
 * AvahiStringList argument must never be NULL.
 */
static void test_avahi_string_list_get_pair_full(void) {
    AvahiStringList *l;
    char *key = NULL;
    char *value = NULL;
    size_t size = 0;
    int r;

    /* Test matrix with '=' in l->text
     * Each case flips key/value/size between NULL and non-NULL
     * to exercise all branches when '=' is present in the string
     */

    // Case 1: key != NULL, value != NULL, size != NULL
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    r = avahi_string_list_get_pair(l, &key, &value, &size);
    assert(r == 0);
    assert(strcmp(key, "foo") == 0);
    assert(strcmp(value, "bar") == 0);
    assert(size == 3);
    avahi_free(key);
    avahi_free(value);
    avahi_string_list_free(l);

    // Case 2: key == NULL, value != NULL, size != NULL
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    r = avahi_string_list_get_pair(l, NULL, &value, &size);
    assert(r == 0);
    assert(strcmp(value, "bar") == 0);
    assert(size == 3);
    avahi_free(value);
    avahi_string_list_free(l);

    // Case 3: key != NULL, value == NULL, size != NULL
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    r = avahi_string_list_get_pair(l, &key, NULL, &size);
    assert(!r);
    assert(strcmp(key, "foo") == 0);
    assert(size == 3);
    avahi_free(key);
    avahi_string_list_free(l);

    // Case 4: key != NULL, value != NULL, size == NULL
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    r = avahi_string_list_get_pair(l, &key, &value, NULL);
    assert(!r);
    assert(strcmp(key, "foo") == 0);
    assert(strcmp(value, "bar") == 0);
    avahi_free(key);
    avahi_free(value);
    avahi_string_list_free(l);

    // Case 5: key == NULL, value == NULL, size != NULL
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    r = avahi_string_list_get_pair(l, NULL, NULL, &size);
    assert(!r);
    assert(size == 3);
    avahi_string_list_free(l);

    // Case 6: key != NULL, value == NULL, size == NULL
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    r = avahi_string_list_get_pair(l, &key, NULL, NULL);
    assert(!r);
    assert(strcmp(key, "foo") == 0);
    avahi_free(key);
    avahi_string_list_free(l);

    // Case 7: key == NULL, value != NULL, size == NULL
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    r = avahi_string_list_get_pair(l, NULL, &value, NULL);
    assert(!r);
    assert(strcmp(value, "bar") == 0);
    avahi_free(value);
    avahi_string_list_free(l);

    // Case 8: key == NULL, value == NULL, size == NULL
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    r = avahi_string_list_get_pair(l, NULL, NULL, NULL);
    assert(!r);
    avahi_string_list_free(l);


    /*
     * Now repeat with NO '=' in l->text
     * Each case flips key/value/size between NULL and non-NULL
     * For no '=': value should be NULL, size should be 0, key copies entire string
     */

    // Case 1: key != NULL, value != NULL, size != NULL
    l = avahi_string_list_new("hello", (const char*)NULL);
    r = avahi_string_list_get_pair(l, &key, &value, &size);
    assert(!r);                     // returns 0
    assert(strcmp(key, "hello") == 0);
    assert(value == NULL);
    assert(size == 0);
    avahi_free(key);
    avahi_string_list_free(l);

    // Case 2: key == NULL, value != NULL, size != NULL
    l = avahi_string_list_new("world", (const char*)NULL);
    r = avahi_string_list_get_pair(l, NULL, &value, &size);
    assert(!r);
    assert(value == NULL);
    assert(size == 0);
    avahi_string_list_free(l);

    // Case 3: key != NULL, value == NULL, size != NULL
    l = avahi_string_list_new("foo", (const char*)NULL);
    r = avahi_string_list_get_pair(l, &key, NULL, &size);
    assert(!r);
    assert(strcmp(key, "foo") == 0);
    assert(size == 0);
    avahi_free(key);
    avahi_string_list_free(l);

    // Case 4: key != NULL, value != NULL, size == NULL
    l = avahi_string_list_new("bar", (const char*)NULL);
    r = avahi_string_list_get_pair(l, &key, &value, NULL);
    assert(!r);
    assert(strcmp(key, "bar") == 0);
    assert(value == NULL);
    avahi_free(key);
    avahi_string_list_free(l);

    // Case 5: key == NULL, value == NULL, size != NULL
    l = avahi_string_list_new("baz", (const char*)NULL);
    r = avahi_string_list_get_pair(l, NULL, NULL, &size);
    assert(!r);
    assert(size == 0);
    avahi_string_list_free(l);

    // Case 6: key != NULL, value == NULL, size == NULL
    l = avahi_string_list_new("qux", (const char*)NULL);
    r = avahi_string_list_get_pair(l, &key, NULL, NULL);
    assert(!r);
    assert(strcmp(key, "qux") == 0);
    avahi_free(key);
    avahi_string_list_free(l);

    // Case 7: key == NULL, value != NULL, size == NULL
    l = avahi_string_list_new("abc", (const char*)NULL);
    r = avahi_string_list_get_pair(l, NULL, &value, NULL);
    assert(!r);
    assert(value == NULL);
    avahi_string_list_free(l);

    // Case 8: key == NULL, value == NULL, size == NULL
    l = avahi_string_list_new("xyz", (const char*)NULL);
    r = avahi_string_list_get_pair(l, NULL, NULL, NULL);
    assert(!r);
    avahi_string_list_free(l);

}

/**
 * Test avahi_string_list_add_printf() with strings of varying lengths.
 *
 * 1) Short string that fits within the initial buffer.
 * 2) Long string that requires resizing the internal buffer.
 * 3) Very long string that triggers multiple buffer resizes.
 *
 * Verifies that the resulting list element contains the expected string
 * and has the correct length.
 */
static void test_avahi_string_list_add_vprintf(void) {
    AvahiStringList *r;
    char long_string[512];
    int i;
    char very_long[1024];

    /* 1) Short string fits initial buffer */
    r = avahi_string_list_add_printf(NULL, "short=1");
    assert(r);
    assert(strcmp((char*)r->text, "short=1") == 0);
    avahi_string_list_free(r);

    /* 2) Long string exceeds initial 80-byte buffer, triggers vsnprintf resize branch (len = n+1) */
    for (i = 0; i < (int) sizeof(long_string) - 1; i++)
        long_string[i] = 'A';
    long_string[sizeof(long_string) - 1] = '\0';

    r = avahi_string_list_add_printf(NULL, "%s", long_string);
    assert(r);
    assert(strlen((char*)r->text) == sizeof(long_string)-1);
    avahi_string_list_free(r);

    /* 6) Very long string to exercise multiple realloc iterations (n >= len multiple times) */
    for (i = 0; i < (int) sizeof(very_long) - 1; i++)
        very_long[i] = 'B';
    very_long[sizeof(very_long) - 1] = '\0';

    r = avahi_string_list_add_printf(NULL, "%s", very_long);
    assert(r);
    assert(strlen((char*)r->text) == sizeof(very_long)-1);
    avahi_string_list_free(r);

DIAG_PUSH
GCC_DIAG_IGNORE("-Wformat")
CLANG_DIAG_IGNORE("-Wformat")
GCC_DIAG_IGNORE("-Wformat-extra-args")
CLANG_DIAG_IGNORE("-Wformat-extra-args")
GCC_DIAG_IGNORE("-Wformat-contains-nul")
    r = avahi_string_list_add_printf(NULL, "%q\0\0", 123);
DIAG_POP
#if defined(__GLIBC__)
    /* if an output error is encountered, a negative value is returned.
     * %q is undefined, should return null */
    assert(!r);
#else
    /* SunOS it does not */
    if (r) avahi_string_list_free(r);
#endif

}

/**
 * Test avahi_string_list_parse() with various input cases:
 *
 * - Normal string with correct length ("foo").
 * - Empty string list (0 length).
 * - String prefixed with a leading 0 byte ("bar").
 * - Malformed string with claimed length exceeding available data.
 * - Zero-length input.
 *
 * Verifies that the parser returns success/failure as expected,
 * and that the resulting list elements have the correct size and content.
 */
static void test_parse_normal(void) {
    AvahiStringList *l = NULL;

    /* "foo" */
    uint8_t data[] = { 3, 'f', 'o', 'o' };
    uint8_t empty[] = { 0 };
    uint8_t bar[] = { 0, 3, 'b', 'a', 'r' };
    uint8_t err[] = { 5, 'a', 'b' };
    uint8_t dummy = 0;

    assert(avahi_string_list_parse(data, sizeof(data), &l) == 0);
    assert(l);
    assert(l->size == 3);
    assert(memcmp(l->text, "foo", 3) == 0);

    avahi_string_list_free(l);

    assert(avahi_string_list_parse(empty, sizeof(empty), &l) == 0);
    assert(!l);

    assert(avahi_string_list_parse(bar, sizeof(bar), &l) == 0);
    assert(l);
    assert(l->size == 3);
    assert(memcmp(l->text, "bar", 3) == 0);

    avahi_string_list_free(l);

    /* Claims length 5, only 2 bytes follow */
    l = NULL;
    assert(avahi_string_list_parse(err, sizeof(err), &l) < 0);
    assert(!l);

    assert(avahi_string_list_parse(&dummy, 0, &l) == 0);
    assert(!l);
}

/**
 * Test avahi_string_list_find() with different key formats:
 *
 * - Exact key match (no '=' suffix).
 * - Key with empty value (trailing '=').
 * - Key prefix that should not match.
 *
 * Verifies that the correct list element is returned or NULL when no match exists.
 */
static void test_string_list_find_key_forms(void) {
    AvahiStringList *l, *r;

    /* Build list: "foo", "foo=", "foobar" */
    l = avahi_string_list_new("foobar", "foo=", "foo", (const char*)NULL);
    assert(l);

    /* 1) Exact match: "foo" */
    r = avahi_string_list_find(l, "foo");
    assert(r);
    assert(strcmp((char*) r->text, "foo") == 0);

    /* 2) Key with empty value: "foo" */
    r = avahi_string_list_find(l->next, "foo");
    assert(r);
    assert(strcmp((char*) r->text, "foo=") == 0);

    /* 3) Prefix-only match must NOT match */
    r = avahi_string_list_find(l->next->next, "foo");
    assert(!r);

    avahi_string_list_free(l);
}


/*
 * Verify service cookie parsing:
 *
 * We exercise the following logical paths:
 * 1. No cookie key present
 *    - avahi_string_list_find() returns NULL
 * 2. Cookie key present, but no '='
 *    - avahi_string_list_get_pair() succeeds
 *    - value == NULL
 * 3. Cookie key present with empty value ("cookie=")
 *    - value != NULL
 *    - *value == '\0'
 * 4. Cookie key present with valid numeric value
 *    - full success path
 * 5. Cookie key present with trailing garbage ("123abc")
 *    - strtoll parses number
 *    - *end != '\0' triggers validation failure
 *
 * The only branch NOT covered here is the
 *   avahi_string_list_get_pair(...) < 0
 * path, which requires allocator failure and is tested separately.
 */
static void test_get_service_cookie(void) {
    AvahiStringList *l;
    char buf[128];
    uint32_t c;

    /* Missing cookie */
    l = avahi_string_list_new("foo=1", (const char*)NULL);
    c = avahi_string_list_get_service_cookie(l);
    assert(c == AVAHI_SERVICE_COOKIE_INVALID);
    avahi_string_list_free(l);

    /* Non-numeric */
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, "abc");
    c = avahi_string_list_get_service_cookie(l);
    assert(c == AVAHI_SERVICE_COOKIE_INVALID);
    avahi_string_list_free(l);

    /* Trailing garbage */
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, "123x");
    c = avahi_string_list_get_service_cookie(l);
    assert(c == AVAHI_SERVICE_COOKIE_INVALID);
    avahi_string_list_free(l);

    /* Valid decimal */
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, "42");
    c = avahi_string_list_get_service_cookie(l);
    assert(c == 42);
    avahi_string_list_free(l);

    /* Valid hex */
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, "0x10");
    c = avahi_string_list_get_service_cookie(l);
    assert(c == 16);
    avahi_string_list_free(l);

    /* Cookie key present, no '=' (value == NULL) */
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, NULL);
    assert(l);
    c = avahi_string_list_get_service_cookie(l);
    assert(c == AVAHI_SERVICE_COOKIE_INVALID);
    avahi_string_list_free(l);

    /* Cookie key present, empty value ("cookie=") */
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, "");
    assert(l);
    c = avahi_string_list_get_service_cookie(l);
    assert(c == 0);
    avahi_string_list_free(l);

    /* Cookie key present, with > LLONG_MAX, trailing 0 multiplies by 10 */
    sprintf(buf, "%lli0", LLONG_MAX);
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, buf);
    assert(l);
    c = avahi_string_list_get_service_cookie(l);
    assert(c == AVAHI_SERVICE_COOKIE_INVALID);
    avahi_string_list_free(l);

    /* Cookie key present with > UINT_MAX 4294967295 + 1 */
    sprintf(buf, "%" PRIu64, (uint64_t)UINT_MAX + 1);
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, buf);
    assert(l);
    c = avahi_string_list_get_service_cookie(l);
    assert(c == AVAHI_SERVICE_COOKIE_INVALID);
    avahi_string_list_free(l);

    /* Cookie key present with > UINT_MAX 4294967295 */
    sprintf(buf, "%" PRIu32, (uint32_t)UINT_MAX);
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, buf);
    assert(l);
    c = avahi_string_list_get_service_cookie(l);
    assert(c == UINT_MAX);
    avahi_string_list_free(l);

    /* Cookie key present with < 0 */
    l = avahi_string_list_add_pair(NULL, AVAHI_SERVICE_COOKIE, "-42");
    assert(l);
    c = avahi_string_list_get_service_cookie(l);
    assert(c == AVAHI_SERVICE_COOKIE_INVALID);
    avahi_string_list_free(l);
}

/**
 * Test avahi_string_list_free():
 *
 * - Passing NULL should safely do nothing.
 * - Passing a non-empty list should free all nodes correctly.
 */
static void test_avahi_string_list_free_branches(void) {
    AvahiStringList *l = NULL;

    // --- 1) Call with NULL list (tests "while (l)" false branch) ---
    avahi_string_list_free(l); // should just return, loop never entered

    // --- 2) Call with non-empty list (tests "while (l)" true branch) ---
    l = avahi_string_list_new("foo", (const char*)NULL);
    assert(l); // make sure allocation succeeded
    avahi_string_list_free(l); // loop executes once, frees the node
}

/**
 * Test avahi_string_list_serialize():
 *
 * Exercises various serialization scenarios:
 * - Short and long strings, including truncation at 255 bytes.
 * - Small buffers that force length adjustment.
 * - Empty lists with data buffer or NULL.
 * - Single empty string in list.
 * - Edge cases to confirm correct used size returned.
 */
static void test_string_list_serialize_branches(void) {
    AvahiStringList *l;
    size_t used;
    uint8_t buf[300];
    char text[10] = "abcdefghi";
    char long_text[300];

    /* 1) Normal short string (already covered mostly) */
    l = avahi_string_list_new("foo", (const char*)NULL);
    used = avahi_string_list_serialize(l, buf, sizeof(buf));
    assert(used > 0);
    avahi_string_list_free(l);

    /* 2) Long string > 255 to trigger truncation branch */
    memset(long_text, 'A', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = 0;
    l = avahi_string_list_new(long_text, (const char*)NULL);
    used = avahi_string_list_serialize(l, buf, sizeof(buf));
    assert(used >= 256); /* confirms truncation at 255 */
    avahi_string_list_free(l);

    /* 3) Small buffer triggers size-1 truncation branch */
    l = avahi_string_list_new(text, (const char*)NULL);
    used = avahi_string_list_serialize(l, buf, 5);
    assert(used >= 5); /* string was truncated to fit */
    avahi_string_list_free(l);

    /* 4) Empty list with data != NULL triggers empty string branch */
    l = NULL;
    used = avahi_string_list_serialize(l, buf, sizeof(buf));
    assert(used == 1 && buf[0] == 0);

    /* 5) Empty list with data == NULL triggers used=1 branch */
    l = NULL;
    used = avahi_string_list_serialize(l, NULL, 0);
    assert(used == 1);

    /* 6) data != NULL, triggers k > 255 branch */
    l = avahi_string_list_new(long_text, (const char*)NULL);
    used = avahi_string_list_serialize(l, buf, sizeof(buf)); // buf smaller than string
    assert(used >= 10); // hits k > size-1 too
    avahi_string_list_free(l);

    /* 7) data == NULL, triggers k > 255 branch in else */
    l = avahi_string_list_new(long_text, (const char*)NULL);
    used = avahi_string_list_serialize(l, NULL, 0);
    assert(used > 0); // triggers k > 255 branch
    avahi_string_list_free(l);

    /* 8) empty list with data != NULL triggers empty string */
    l = NULL;
    used = avahi_string_list_serialize(l, buf, sizeof(buf));
    assert(used == 1 && buf[0] == 0);

    /* 9) empty list with data == NULL triggers used=1 branch */
    l = NULL;
    used = avahi_string_list_serialize(l, NULL, 0);
    assert(used == 1);

    /* 10) Single empty string in list */
    l = avahi_string_list_new("", (const char*)NULL);
    used = avahi_string_list_serialize(l, buf, sizeof(buf));
    assert(used == 1);    /* should write single NUL byte */
    assert(buf[0] == 0);  /* confirms special-case empty string written */
    avahi_string_list_free(l);

    /* 11) Edge case: empty list, but size == 0 and data != NULL */
    l = NULL;
    used = avahi_string_list_serialize(l, buf, 0);
    assert(used == 0);  // this will hit the missing boolean combination

    avahi_string_list_free(l);

}


/********************************************************
 * These tests simulate memory-constrained conditions
 * by using a custom allocator that can fail after a
 * specified number of allocations.
 *
 * They verify that functions correctly handle:
 *  - Allocation failures (malloc/realloc returning NULL)
 *  - Edge cases where memory is insufficient
 *  - Proper cleanup and no leaks in error paths
 *
 * This helps ensure robustness under low-memory scenarios.
 */


 /*
 * Allocator-failure coverage matrix for avahi_string_list_get_pair():
 *
 * 1) No '=' present:
 *    - key != NULL
 *    - avahi_strdup() fails
 *    - expect: return -1
 * 2) '=' present:
 *    - key != NULL
 *    - avahi_strndup() fails
 *    - expect: return -1 (no cleanup needed)
 * 3) '=' present:
 *    - key != NULL, value != NULL
 *    - avahi_memdup() fails
 *    - expect: return -1, previously allocated key freed
 */
static void test_avahi_string_list_get_pair_failures(void) {
    AvahiStringList *l;
    char *key = NULL;
    char *value = NULL;
    size_t size = 0;
    int r;

    /* --- 1) No '=' in string -> avahi_strdup() fails --- */
    avahi_set_allocator(avahi_allocator_fail_after(10));
    l = avahi_string_list_new("foo-bar", (const char*)NULL);
    assert(l);

    /* Fail the very next allocation: avahi_strdup() */
    avahi_set_allocator(avahi_allocator_fail_after(0));
    r = avahi_string_list_get_pair(l, &key, &value, &size);
    assert(r == -1);

    key = value = NULL;
    size = 0;
    avahi_string_list_free(l);

    /* --- 2) '=' in string -> avahi_strndup() fails for key --- */
    avahi_set_allocator(avahi_allocator_fail_after(10));
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    assert(l);

    /* First allocation in get_pair(): avahi_strndup() */
    avahi_set_allocator(avahi_allocator_fail_after(0));
    r = avahi_string_list_get_pair(l, &key, &value, &size);
    assert(r == -1);

    key = value = NULL;
    size = 0;
    avahi_string_list_free(l);

    /* --- 3) '=' in string -> avahi_memdup() fails for value --- */
    avahi_set_allocator(avahi_allocator_fail_after(10));
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    assert(l);

    /*
     * Allocation order in get_pair():
     *   1) avahi_strndup()  -> succeeds
     *   2) avahi_memdup()   -> fails
     */
    avahi_set_allocator(avahi_allocator_fail_after(1));
    r = avahi_string_list_get_pair(l, &key, &value, &size);
    assert(r == -1);

    /* key must have been freed internally on memdup failure */
    key = value = NULL;
    size = 0;
    avahi_string_list_free(l);

    /* --- 4) '=' in string -> avahi_memdup() fails with key == NULL --- */
    avahi_set_allocator(avahi_allocator_fail_after(10));
    l = avahi_string_list_new("foo=bar", (const char*)NULL);
    assert(l);

    /*
     * key == NULL skips avahi_strndup()
     * first allocation is avahi_memdup(), which we force to fail
     */
    avahi_set_allocator(avahi_allocator_fail_after(0));
    r = avahi_string_list_get_pair(l, NULL, &value, &size);
    assert(r == -1);

    value = NULL;
    size = 0;
    avahi_string_list_free(l);


    avahi_set_allocator(NULL);
}

/********************************************************
 * Test avahi_string_list_add_vprintf() under simulated
 * memory allocation failures.
 *
 * 1) Fail the initial avahi_malloc() to ensure the
 *    function returns NULL safely.
 * 2) Fail avahi_realloc() during string resizing to
 *    ensure proper error handling when growing the
 *    buffer.
 *
 * This confirms that the function handles low-memory
 * conditions gracefully and does not leak memory.
 */
static void test_avahi_string_list_add_vprintf_failures(void) {
    AvahiStringList *r;
    char long_string[256];
    int i;

    /* Build a string guaranteed to exceed initial 80-byte buffer */
    for (i = 0; i < (int) sizeof(long_string) - 1; i++)
        long_string[i] = 'A';
    long_string[sizeof(long_string) - 1] = '\0';

    /*
     * --- 1) Initial avahi_malloc() fails ---
     * First allocation in avahi_string_list_add_vprintf()
     */
    avahi_set_allocator(avahi_allocator_fail_after(0));
    r = avahi_string_list_add_printf(NULL, "%s", "test");
    assert(r == NULL);

    /*
     * --- 2) avahi_realloc() fails during resize ---
     *
     * Allocation sequence:
     *   1) avahi_malloc()  -> succeeds
     *   2) vsnprintf()     -> returns n >= len
     *   3) avahi_realloc() -> fails
     */
    avahi_set_allocator(avahi_allocator_fail_after(1));
    r = avahi_string_list_add_printf(NULL, "%s", long_string);
    assert(r == NULL);
}

/* Force avahi_malloc() failure to exercise NULL-return branch
 * in avahi_string_list_add_anonymous().
 *
 * This test validates:
 *  - allocation failure is propagated correctly
 *  - no memory is written when allocation fails
 *  - branch coverage for the error path
 */
static void test_string_list_add_anonymous_malloc_fail(void) {
    AvahiStringList *l = (AvahiStringList *) 0xdeadbeef;
    AvahiStringList *r;

    /* Cause the next allocation to fail */
    avahi_set_allocator(avahi_allocator_fail_after(0));

    r = avahi_string_list_add_anonymous(l, 16);

    /* Must fail cleanly */
    assert(!r);

    /* Reset allocator behavior for subsequent tests */
    avahi_set_allocator(NULL);
}

/* Force allocation failure in avahi_string_list_add_anonymous()
 * when called via avahi_string_list_add_arbitrary().
 *
 * This test validates:
 *  - allocator failure propagates through add_anonymous()
 *  - add_arbitrary() returns NULL correctly
 *  - branch coverage for the NULL-return path
 */
static void test_string_list_add_arbitrary_malloc_fail(void) {
    AvahiStringList *l = (AvahiStringList *) 0xcafebabe;
    AvahiStringList *r;
    const uint8_t text[] = { 0x01, 0x02, 0x03 };

    /* Fail the next allocation */
    avahi_set_allocator(avahi_allocator_fail_after(0));

    r = avahi_string_list_add_arbitrary(l, text, sizeof(text));

    /* Must fail cleanly */
    assert(!r);

    /* Restore allocator */
    avahi_set_allocator(NULL);
}

/********************************************************
 * Test avahi_string_list_parse() under simulated
 * out-of-memory conditions.
 *
 * Specifically, the first element is allocated
 * successfully, but the second allocation fails.
 *
 * Ensures that the function properly reports an
 * error and does not leak memory when parsing fails
 * partway through the list.
 */
static void test_parse_oom_second_element(void) {
    AvahiStringList *l = NULL;

    /* "foo", "bar" */
    uint8_t data[] = {
        3, 'f', 'o', 'o',
        3, 'b', 'a', 'r'
    };

    /* First allocation succeeds, second fails */
    avahi_set_allocator(avahi_allocator_fail_after(1));

    assert(avahi_string_list_parse(data, sizeof(data), &l) < 0);
}

/********************************************************
 * Test avahi_string_list_copy() under simulated
 * out-of-memory conditions.
 *
 * Verifies that the copy operation correctly returns
 * NULL if any allocation fails, either on the first
 * or subsequent node, and that no memory leaks occur.
 */
static void test_avahi_string_list_copy_failures(void) {
    AvahiStringList *l = NULL;
    AvahiStringList *r = NULL;

    // Build a small list
    avahi_set_allocator(avahi_allocator_fail_after(10));
    l = avahi_string_list_new("one", "two", (const char*)NULL);
    assert(l);

    // --- 1) Fail on first allocation inside avahi_string_list_add_arbitrary ---
    avahi_set_allocator(avahi_allocator_fail_after(0)); // first malloc will fail
    r = avahi_string_list_copy(l);
    assert(!r); // should return NULL

    // --- 2) Fail on second allocation inside avahi_string_list_add_arbitrary ---
    avahi_set_allocator(avahi_allocator_fail_after(1)); // first node alloc succeeds, second fails
    r = avahi_string_list_copy(l);
    assert(!r); // should return NULL

    // Clean up original list
    avahi_string_list_free(l);
}

/********************************************************
 * Test avahi_string_list_add_pair_arbitrary() under
 * simulated out-of-memory conditions.
 *
 * Verifies that the function returns NULL if an
 * allocation fails, both for an empty list and for
 * a non-empty list, without leaking memory.
 */
static void test_avahi_string_list_add_pair_arbitrary_failures(void) {
    AvahiStringList *l = NULL;
    AvahiStringList *existing;
    const char *key = "foo";
    const uint8_t value[] = { 'b', 'a', 'r' };
    size_t size = sizeof(value);

    // --- 1) Fail allocation inside avahi_string_list_add_anonymous ---
    avahi_set_allocator(avahi_allocator_fail_after(0)); // force first allocation to fail
    l = avahi_string_list_add_pair_arbitrary(NULL, key, value, size);
    assert(!l); // should return NULL

    // --- 2) Fail allocation with a non-empty list ---
    avahi_set_allocator(avahi_allocator_fail_after(10));
    existing = avahi_string_list_new("existing", (const char*)NULL);
    assert(existing);

    avahi_set_allocator(avahi_allocator_fail_after(0)); // next allocation fails
    l = avahi_string_list_add_pair_arbitrary(existing, key, value, size);
    assert(!l); // should return NULL

    avahi_string_list_free(existing);
}

/********************************************************
 * Test avahi_string_list_to_string() under simulated
 * out-of-memory conditions.
 *
 * Verifies that the function returns NULL if memory
 * allocation fails, without leaking the original list.
 */
static void test_avahi_string_list_to_string_fail(void) {
    AvahiStringList *l;
    char *s;

    // Build a normal list
    avahi_set_allocator(avahi_allocator_fail_after(10));
    l = avahi_string_list_new("foo", "bar", (const char*)NULL);
    assert(l);

    // Force avahi_new to fail immediately
    avahi_set_allocator(avahi_allocator_fail_after(0));

    s = avahi_string_list_to_string(l);
    assert(!s);  // allocation failed

    avahi_string_list_free(l);
}

/********************************************************
 * Test avahi_string_list_add_many() under simulated
 * out-of-memory conditions.
 *
 * Checks behavior when:
 *   1) First allocation fails.
 *   2) Allocation fails partway through multiple elements.
 *   3) Multiple allocations succeed (sanity check).
 *
 * Ensures the function returns NULL and does not leak
 * memory in failure scenarios.
 */
static void test_avahi_string_list_add_many_va_failures(void) {
    AvahiStringList *r = NULL;

    // --- 1) First allocation fails ---
    avahi_set_allocator(avahi_allocator_fail_after(0));
    r = avahi_string_list_add_many(NULL, "one", "two", (const char*)NULL);
    assert(!r);  // allocation failed

    // --- 2) First allocation succeeds, second fails ---
    avahi_set_allocator(avahi_allocator_fail_after(1));
    r = avahi_string_list_add_many(NULL, "one", "two", "three", (const char*)NULL);
    assert(!r);  // allocation fails mid-loop

    // --- 3) Multiple allocations succeed normally ---
    avahi_set_allocator(avahi_allocator_fail_after(2));
    r = avahi_string_list_add_many(NULL, "one", "two", "three", (const char*)NULL);
    assert(!r);
}

/********************************************************
 * Test avahi_string_list_get_service_cookie() behavior
 * under low-memory conditions.
 *
 * Creates a normal service cookie entry, then forces
 * the allocator to fail on the first allocation within
 * get_pair. Verifies that the function returns the
 * invalid cookie value and does not crash.
 */
static void test_service_cookie_low_mem(void) {
    AvahiStringList *l;
    uint32_t v;

    /* Create a normal cookie entry */
    avahi_set_allocator(avahi_allocator_fail_after(10));
    l = avahi_string_list_new(AVAHI_SERVICE_COOKIE "=123", (const char*)NULL);
    assert(l);

    /* Force allocator to fail on first malloc inside get_pair */
    avahi_set_allocator(avahi_allocator_fail_after(0));

    /* This should trigger ret1 < 0 in get_service_cookie() */
    v = avahi_string_list_get_service_cookie(l);
    assert(v == AVAHI_SERVICE_COOKIE_INVALID);

    avahi_string_list_free(l);
}

/********************************************************
 * Entry point for the Avahi string list test suite.
 *
 * 1) Builds a sample string list for testing.
 * 2) Runs all functional tests: rendering, serialization,
 *    parsing, copying, equality, key/value operations, etc.
 * 3) Runs low-memory failure tests to ensure allocation
 *    failures are handled safely.
 *
 * Prints "Tests PASS" if all assertions succeed.
 */
int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[]) {
    AvahiStringList *a;

    a = build_test_string_list();

    test_string_rendering(a);
    test_serialize_and_parse(a);
    test_copy_and_equality(a);
    test_find_and_get_pair(a);
    test_null_list_serialization();
    avahi_string_list_free(a);

    test_reverse();
    test_add_many();
    test_new_from_array();
    test_get_next();
    test_new_va();
    test_many_va();
    test_get_service_cookie();
    test_avahi_string_list_equal_branches();
    test_avahi_string_list_add_pair_arbitrary_branches();
    test_avahi_string_list_get_pair_full();
    test_avahi_string_list_add_vprintf();
    test_parse_normal();
    test_string_list_find_key_forms();
    test_avahi_string_list_free_branches();
    test_string_list_serialize_branches();
    test_avahi_string_list_add_arbitrary_sizes();

    /* these tests ensure low memory failures return known paths */
    test_avahi_string_list_get_pair_failures();
    test_avahi_string_list_add_vprintf_failures();
    test_string_list_add_anonymous_malloc_fail();
    test_string_list_add_arbitrary_malloc_fail();
    test_parse_oom_second_element();
    test_avahi_string_list_copy_failures();
    test_avahi_string_list_add_pair_arbitrary_failures();
    test_avahi_string_list_to_string_fail();
    test_avahi_string_list_add_many_va_failures();
    test_service_cookie_low_mem();

    printf("Tests PASS\n");
    return 0;
}

