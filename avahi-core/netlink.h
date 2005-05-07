#ifndef foonetlinkhfoo
#define foonetlinkhfoo

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

#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>

#include <glib.h>

struct _AvahiNetlink;
typedef struct _AvahiNetlink AvahiNetlink;

AvahiNetlink *avahi_netlink_new(GMainContext *c, gint priority, guint32 groups, void (*cb) (AvahiNetlink *n, struct nlmsghdr *m, gpointer userdata), gpointer userdata);
void avahi_netlink_free(AvahiNetlink *n);

int avahi_netlink_send(AvahiNetlink *n, struct nlmsghdr *m, guint *ret_seq);

gboolean avahi_netlink_work(AvahiNetlink *n, gboolean block);

#endif
