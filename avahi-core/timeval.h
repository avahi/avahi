#ifndef footimevalhfoo
#define footimevalhfoo

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
#include <sys/time.h>

#include <avahi-common/cdecl.h>

AVAHI_C_DECL_BEGIN

typedef int64_t AvahiUsec;

int avahi_timeval_compare(const struct timeval *a, const struct timeval *b);
AvahiUsec avahi_timeval_diff(const struct timeval *a, const struct timeval *b);
struct timeval* avahi_timeval_add(struct timeval *a, AvahiUsec usec);

AvahiUsec avahi_age(const struct timeval *a);
struct timeval *avahi_elapse_time(struct timeval *tv, unsigned msec, unsigned jitter);

AVAHI_C_DECL_END

#endif
