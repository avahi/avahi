#ifndef foosimplewatchhfoo
#define foosimplewatchhfoo

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

#include <avahi-common/cdecl.h>

#include "watch.h"

AVAHI_C_DECL_BEGIN

typedef struct AvahiSimplePoll AvahiSimplePoll;

AvahiSimplePoll *avahi_simple_poll_new(void);
void avahi_simple_poll_free(AvahiSimplePoll *s);

AvahiPoll* avahi_simple_poll_get(AvahiSimplePoll *s);

int avahi_simple_poll_iterate(AvahiSimplePoll *s, int block);

void avahi_simple_poll_quit(AvahiSimplePoll *s);

AVAHI_C_DECL_END

#endif
