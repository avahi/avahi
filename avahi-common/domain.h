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

/** \file domain.h Domain name handling functions */


#include <inttypes.h>
#include <sys/types.h>

#include <avahi-common/cdecl.h>

AVAHI_C_DECL_BEGIN

/** Normalize a domain name into canonical form. This drops trailing
 * dots and removes useless backslash escapes. avahi_free() the
 * result! */
char *avahi_normalize_name(const char *s);

/** Return the local host name. avahi_free() the result! */
char *avahi_get_host_name(void); 

/** Return 1 when the specified domain names are equal, 0 otherwise */
int avahi_domain_equal(const char *a, const char *b);

/** Do a binary comparison of to specified domain names, return -1, 0, or 1, depending on the order. */
int avahi_binary_domain_cmp(const char *a, const char *b);

/** Read the first label from the textual domain name *name, unescape
 * it and write it to dest, *name is changed to point to the next label*/
char *avahi_unescape_label(const char **name, char *dest, size_t size);

/** Escape the domain name in *src and write it to *ret_name */
char *avahi_escape_label(const uint8_t* src, size_t src_length, char **ret_name, size_t *ret_size);

/** Return some kind of hash value for a string */
unsigned avahi_strhash(const char *p);

/** Return some kind of hash value for a domain */
unsigned avahi_domain_hash(const char *s);

/** Return 1 when the specified string contains a valid service type, 0 otherwise */
int avahi_valid_service_type(const char *t);

/** Return 1 when the specified string contains a valid domain name, 0 otherwise */
int avahi_valid_domain_name(const char *t);

/** Return 1 when the specified string contains a valid service name, 0 otherwise */
int avahi_valid_service_name(const char *t);

/** Return 1 when the specified string contains a valid non-FQDN host name (i.e. without dots), 0 otherwise */
int avahi_valid_host_name(const char *t);

/** Change every character in the string to upper case (ASCII), return a pointer to the string */
char *avahi_strup(char *s);

/** Change every character in the string to lower case (ASCII), return a pointer to the string */
char *avahi_strdown(char *s);

AVAHI_C_DECL_END

#endif
