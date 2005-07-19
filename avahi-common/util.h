#ifndef fooutilhfoo
#define fooutilhfoo

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

AVAHI_C_DECL_BEGIN

typedef gint64 AvahiUsec;

gchar *avahi_normalize_name(const gchar *s); /* g_free() the result! */
gchar *avahi_get_host_name(void); /* g_free() the result! */

gint avahi_timeval_compare(const GTimeVal *a, const GTimeVal *b);
AvahiUsec avahi_timeval_diff(const GTimeVal *a, const GTimeVal *b);
GTimeVal* avahi_timeval_add(GTimeVal *a, AvahiUsec usec);

AvahiUsec avahi_age(const GTimeVal *a);
GTimeVal *avahi_elapse_time(GTimeVal *tv, guint msec, guint jitter);

gint avahi_set_cloexec(gint fd);
gint avahi_set_nonblock(gint fd);
gint avahi_wait_for_write(gint fd);

gboolean avahi_domain_equal(const gchar *a, const gchar *b);
gint avahi_binary_domain_cmp(const gchar *a, const gchar *b);

void avahi_hexdump(gconstpointer p, guint size);

/* Read the first label from the textual domain name *name, unescape
 * it and write it to dest, *name is changed to point to the next label*/
gchar *avahi_unescape_label(const gchar **name, gchar *dest, guint size);

/* Escape the domain name in *src and write it to *ret_name */
gchar *avahi_escape_label(const guint8* src, guint src_length, gchar **ret_name, guint *ret_size);

guint avahi_domain_hash(const gchar *s);

gchar *avahi_format_mac_address(const guint8* mac, guint size);

AVAHI_C_DECL_END

#endif
