#ifndef fooresolvehfoo
#define fooresolvehfoo

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

#include "llist.h"
#include "core.h"
#include "resolve.h"
#include "timeeventq.h"
#include "server.h"

void avahi_browser_cleanup(AvahiServer *server);
void avahi_browser_notify(AvahiServer *s, AvahiInterface *i, AvahiRecord *record, AvahiBrowserEvent event);

gboolean avahi_is_subscribed(AvahiServer *s, AvahiInterface *i, AvahiKey *k);

void avahi_record_browser_destroy(AvahiRecordBrowser *b);

#endif
