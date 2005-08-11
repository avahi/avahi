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

#include <inttypes.h>
#include <stdarg.h>
#include <sys/time.h>

#include <avahi-common/cdecl.h>

AVAHI_C_DECL_BEGIN

typedef int64_t AvahiUsec;

char *avahi_normalize_name(const char *s); /* avahi_free() the result! */
char *avahi_get_host_name(void); /* avahi_free() the result! */

int avahi_timeval_compare(const struct timeval *a, const struct timeval *b);
AvahiUsec avahi_timeval_diff(const struct timeval *a, const struct timeval *b);
struct timeval* avahi_timeval_add(struct timeval *a, AvahiUsec usec);

AvahiUsec avahi_age(const struct timeval *a);
struct timeval *avahi_elapse_time(struct timeval *tv, unsigned msec, unsigned jitter);

int avahi_set_cloexec(int fd);
int avahi_set_nonblock(int fd);
int avahi_wait_for_write(int fd);

int avahi_domain_equal(const char *a, const char *b);
int avahi_binary_domain_cmp(const char *a, const char *b);

void avahi_hexdump(const void *p, size_t size);

/* Read the first label from the textual domain name *name, unescape
 * it and write it to dest, *name is changed to point to the next label*/
char *avahi_unescape_label(const char **name, char *dest, size_t size);

/* Escape the domain name in *src and write it to *ret_name */
char *avahi_escape_label(const uint8_t* src, size_t src_length, char **ret_name, size_t *ret_size);

unsigned avahi_strhash(const char *p);
unsigned avahi_domain_hash(const char *s);

char *avahi_format_mac_address(const uint8_t* mac, size_t size);

int avahi_valid_service_type(const char *t);
int avahi_valid_domain_name(const char *t);
int avahi_valid_service_name(const char *t);
int avahi_valid_host_name(const char *t);

char *avahi_strdown(char *s);
char *avahi_strup(char *s);

AVAHI_C_DECL_END

#endif
