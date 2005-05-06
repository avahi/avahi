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

typedef struct _AvahiStringList AvahiStringList;

struct _AvahiStringList {
    AvahiStringList *next;
    guint size;
    guint8 text[1];
};

AvahiStringList *avahi_string_list_new(const gchar *txt, ...);
AvahiStringList *avahi_string_list_new_va(va_list va);

void avahi_string_list_free(AvahiStringList *l);

AvahiStringList *avahi_string_list_add(AvahiStringList *l, const gchar *text);
AvahiStringList *avahi_string_list_add_arbitrary(AvahiStringList *l, const guint8 *text, guint size);
AvahiStringList *avahi_string_list_add_many(AvahiStringList *r, ...);
AvahiStringList *avahi_string_list_add_many_va(AvahiStringList *r, va_list va);

gchar* avahi_string_list_to_string(AvahiStringList *l);

guint avahi_string_list_serialize(AvahiStringList *l, gpointer data, guint size);
AvahiStringList *avahi_string_list_parse(gconstpointer data, guint size);

gboolean avahi_string_list_equal(AvahiStringList *a, AvahiStringList *b);

AvahiStringList *avahi_string_list_copy(AvahiStringList *l);

#endif

