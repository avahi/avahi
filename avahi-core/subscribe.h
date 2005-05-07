#ifndef foosubscribehfoo
#define foosubscribehfoo

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
#include "subscribe.h"
#include "timeeventq.h"
#include "server.h"

struct _AvahiSubscription {
    AvahiServer *server;
    AvahiKey *key;
    gint interface;
    guchar protocol;
    gint n_query;
    guint sec_delay;

    AvahiTimeEvent *time_event;

    AvahiSubscriptionCallback callback;
    gpointer userdata;

    AVAHI_LLIST_FIELDS(AvahiSubscription, subscriptions);
    AVAHI_LLIST_FIELDS(AvahiSubscription, by_key);
};

void avahi_subscription_notify(AvahiServer *s, AvahiInterface *i, AvahiRecord *record, AvahiSubscriptionEvent event);

gboolean avahi_is_subscribed(AvahiServer *s, AvahiKey *k);

#endif
