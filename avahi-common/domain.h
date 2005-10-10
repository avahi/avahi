#ifndef foodomainhfoo
#define foodomainhfoo

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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** The maximum length of a a fully escaped domain name C string. This
 * is calculated like this: RFC1034 mandates maximum length of FQDNs
 * is 255. The maximum label length is 63. To minimize the number of
 * (non-escaped) dots, we comprise our maximum-length domain name of
 * four labels á 63 characters plus three inner dots. Escaping the
 * four labels quadruples their length at maximum. An escaped domain
 * name has the therefore the maximum length of 63*4*4+3=1011. A
 * trailing NUL and perhaps two unnecessary dots leading and trailing
 * the string brings us to 1014. */
#define AVAHI_DOMAIN_NAME_MAX 1014

/** Maxium size of an unescaped label */
#define AVAHI_LABEL_MAX 64

/** Normalize a domain name into canonical form. This drops trailing
 * dots and removes useless backslash escapes. */
char *avahi_normalize_name(const char *s, char *ret_s, size_t size);

/** Normalize a domain name into canonical form. This drops trailing
 * dots and removes useless backslash escapes. avahi_free() the
 * result! */
char *avahi_normalize_name_strdup(const char *s);

/** Return the local host name. */
char *avahi_get_host_name(char *ret_s, size_t size); 

/** Return the local host name. avahi_free() the result! */
char *avahi_get_host_name_strdup(void);

/** Return 1 when the specified domain names are equal, 0 otherwise */
int avahi_domain_equal(const char *a, const char *b);

/** Do a binary comparison of to specified domain names, return -1, 0, or 1, depending on the order. */
int avahi_binary_domain_cmp(const char *a, const char *b);

/** Read the first label from the textual domain name *name, unescape
 * it and write it to dest, *name is changed to point to the next label*/
char *avahi_unescape_label(const char **name, char *dest, size_t size);

/** Escape the domain name in *src and write it to *ret_name */
char *avahi_escape_label(const char* src, size_t src_length, char **ret_name, size_t *ret_size);

/** Return 1 when the specified string contains a valid service type, 0 otherwise */
int avahi_is_valid_service_type(const char *t);

/** Return 1 when the specified string contains a valid domain name, 0 otherwise */
int avahi_is_valid_domain_name(const char *t);

/** Return 1 when the specified string contains a valid service name, 0 otherwise */
int avahi_is_valid_service_name(const char *t);

/** Return 1 when the specified string contains a valid non-FQDN host name (i.e. without dots), 0 otherwise */
int avahi_is_valid_host_name(const char *t);

/** Return some kind of hash value for the domain, useful for using domains as hash table keys. */
unsigned avahi_domain_hash(const char *name);

/** Returns 1 if the the end labels of domain are eqal to suffix */
int avahi_domain_ends_with(const char *domain, const char *suffix);

/** Construct a valid complete service name from a name, a type and a domain */
int avahi_service_name_join(char *p, size_t size, const char *name, const char *type, const char *domain);

/** Split a full service name into name, type and domain */
int avahi_service_name_split(const char *p, char *name, size_t name_size, char *type, size_t type_size, char *domain, size_t domain_size);

/** Just like OpenBSD strlcpy */
char *avahi_strlcpy(char *dest, const char *src, size_t n);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
