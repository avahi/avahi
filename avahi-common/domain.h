#ifndef foodomainhfoo
#define foodimainhfoo

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

#include <inttypes.h>
#include <sys/types.h>

#include <avahi-common/cdecl.h>

AVAHI_C_DECL_BEGIN

char *avahi_normalize_name(const char *s); /* avahi_free() the result! */
char *avahi_get_host_name(void); /* avahi_free() the result! */

int avahi_domain_equal(const char *a, const char *b);
int avahi_binary_domain_cmp(const char *a, const char *b);

/* Read the first label from the textual domain name *name, unescape
 * it and write it to dest, *name is changed to point to the next label*/
char *avahi_unescape_label(const char **name, char *dest, size_t size);

/* Escape the domain name in *src and write it to *ret_name */
char *avahi_escape_label(const uint8_t* src, size_t src_length, char **ret_name, size_t *ret_size);

unsigned avahi_strhash(const char *p);
unsigned avahi_domain_hash(const char *s);

int avahi_valid_service_type(const char *t);
int avahi_valid_domain_name(const char *t);
int avahi_valid_service_name(const char *t);
int avahi_valid_host_name(const char *t);

char *avahi_strup(char *s);
char *avahi_strdown(char *s);

AVAHI_C_DECL_END

#endif
