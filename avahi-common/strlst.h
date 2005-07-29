#ifndef footxtlisthfoo
#define footxtlisthfoo

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

#include <glib.h>
#include <avahi-common/cdecl.h>

/** \file strlst.h Implementation of a data type to store lists of strings */

AVAHI_C_DECL_BEGIN

/** Linked list of strings that can contain any number of binary
 * characters, including NUL bytes. An empty list is created by
 * assigning a NULL to a pointer to AvahiStringList. The string list
 * is stored in reverse order, so that appending to the string list is
 * effectively a prepending to the linked list.  This object is used
 * primarily for storing DNS TXT record data. */
typedef struct AvahiStringList {
    struct AvahiStringList *next; /**< Pointe to the next linked list element */
    guint size;  /**< Size of text[] */
    guint8 text[1]; /**< Character data */
} AvahiStringList;

/** Create a new string list by taking a variable list of NUL
 * terminated strings. The strings are copied using g_strdup(). The
 * argument list must be terminated by a NULL pointer. */
AvahiStringList *avahi_string_list_new(const gchar *txt, ...);

/** Same as avahi_string_list_new() but pass a va_list structure */
AvahiStringList *avahi_string_list_new_va(va_list va);

/** Create a new string list from a string array. The strings are
 * copied using g_strdup(). length should contain the length of the
 * array, or -1 if the array is NULL terminated*/
AvahiStringList *avahi_string_list_new_from_array(const gchar **array, gint length);

/** Free a string list */
void avahi_string_list_free(AvahiStringList *l);

/** Append a NUL terminated string to the specified string list. The
 * passed string is copied using g_strdup(). Returns the new list
 * start. */
AvahiStringList *avahi_string_list_add(AvahiStringList *l, const gchar *text);

/** Append am arbitrary length byte string to the list. Returns the
 * new list start. */
AvahiStringList *avahi_string_list_add_arbitrary(AvahiStringList *l, const guint8 *text, guint size);

/** Same as avahi_string_list_add(), but takes a variable number of
 * NUL terminated strings. The argument list must be terminated by a
 * NULL pointer. Returns the new list start. */
AvahiStringList *avahi_string_list_add_many(AvahiStringList *r, ...);

/** Same as avahi_string_list_add_many(), but use a va_list
 * structure. Returns the new list start. */
AvahiStringList *avahi_string_list_add_many_va(AvahiStringList *r, va_list va);

/** Convert the string list object to a single character string,
 * seperated by spaces and enclosed in "". g_free() the result! This
 * function doesn't work well with string that contain NUL bytes. */
gchar* avahi_string_list_to_string(AvahiStringList *l);

/** Serialize the string list object in a way that is compatible with
 * the storing of DNS TXT records. Strings longer than 255 bytes are truncated. */
guint avahi_string_list_serialize(AvahiStringList *l, gpointer data, guint size);

/** Inverse of avahi_string_list_serialize() */
AvahiStringList *avahi_string_list_parse(gconstpointer data, guint size);

/** Compare to string lists */
gboolean avahi_string_list_equal(const AvahiStringList *a, const AvahiStringList *b);

/** Copy a string list */
AvahiStringList *avahi_string_list_copy(const AvahiStringList *l);

/** Reverse the string list. */
AvahiStringList* avahi_string_list_reverse(AvahiStringList *l);

/** Return the number of elements in the string list */
guint avahi_string_list_length(const AvahiStringList *l);

AVAHI_C_DECL_END

#endif

