#ifndef fooglibwatchhfoo
#define fooglibwatchhfoo

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

#include <glib.h>
#include <avahi-common/watch.h>

AVAHI_C_DECL_BEGIN

typedef struct AvahiGLibPoll AvahiGLibPoll;

typedef void (*AvahiGLibProcessCallback)(AvahiGLibPoll *g, void *userdata);

AvahiGLibPoll *avahi_glib_poll_new(GMainContext *context, AvahiGLibProcessCallback callback, void *userdata);
void avahi_glib_poll_free(AvahiGLibPoll *g);

AvahiPoll* avahi_glib_poll_get(AvahiGLibPoll *g);

AVAHI_C_DECL_END

#endif
