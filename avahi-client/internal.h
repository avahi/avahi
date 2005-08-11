#ifndef foointernalhfoo
#define foointernalhfoo

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct _AvahiClient
{
    DBusConnection *bus;
    int errno;
    AvahiClientCallback callback;
    void *user_data;
    AVAHI_LLIST_HEAD(AvahiEntryGroup, groups);
};

struct _AvahiEntryGroup {
    char *path;
    AvahiClient *client;
    AvahiEntryGroupCallback callback;
    void *user_data;
    AVAHI_LLIST_FIELDS(AvahiEntryGroup, groups);
};

int avahi_client_set_errno (AvahiClient *client, int errno);

void avahi_entry_group_state_change (AvahiEntryGroup *group, int state);

#endif
