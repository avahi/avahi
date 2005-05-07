#ifndef fooannouncehfoo
#define fooannouncehfoo

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

#include <glib.h>

typedef struct AvahiAnnouncement AvahiAnnouncement;

#include "llist.h"
#include "iface.h"
#include "server.h"
#include "timeeventq.h"

typedef enum {
    AVAHI_PROBING,
    AVAHI_WAITING,         /* wait for other records in group */
    AVAHI_ANNOUNCING,
    AVAHI_ESTABLISHED
} AvahiAnnouncementState;

struct AvahiAnnouncement {
    AvahiServer *server;
    AvahiInterface *interface;
    AvahiEntry *entry;

    AvahiTimeEvent *time_event;

    AvahiAnnouncementState state;
    guint n_iteration;
    guint sec_delay;

    AVAHI_LLIST_FIELDS(AvahiAnnouncement, by_interface);
    AVAHI_LLIST_FIELDS(AvahiAnnouncement, by_entry);
};

void avahi_announce_interface(AvahiServer *s, AvahiInterface *i);
void avahi_announce_entry(AvahiServer *s, AvahiEntry *e);
void avahi_announce_group(AvahiServer *s, AvahiEntryGroup *g);

void avahi_entry_group_check_probed(AvahiEntryGroup *g, gboolean immediately);

gboolean avahi_entry_registered(AvahiServer *s, AvahiEntry *e, AvahiInterface *i);
gboolean avahi_entry_registering(AvahiServer *s, AvahiEntry *e, AvahiInterface *i);

void avahi_goodbye_interface(AvahiServer *s, AvahiInterface *i, gboolean send);
void avahi_goodbye_entry(AvahiServer *s, AvahiEntry *e, gboolean send);

void avahi_goodbye_all(AvahiServer *s, gboolean send);

AvahiAnnouncement *avahi_get_announcement(AvahiServer *s, AvahiEntry *e, AvahiInterface *i);

#endif
