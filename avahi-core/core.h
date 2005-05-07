#ifndef foocorehfoo
#define foocorehfoo

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

#include <stdio.h>
#include <glib.h>

typedef struct _AvahiServer AvahiServer;
typedef struct _AvahiEntry AvahiEntry;
typedef struct _AvahiEntryGroup AvahiEntryGroup;

#include <avahi-core/address.h>
#include <avahi-core/rr.h>

typedef enum {
    AVAHI_ENTRY_NULL = 0,
    AVAHI_ENTRY_UNIQUE = 1,
    AVAHI_ENTRY_NOPROBE = 2,
    AVAHI_ENTRY_NOANNOUNCE = 4
} AvahiEntryFlags;

typedef enum {
    AVAHI_ENTRY_GROUP_UNCOMMITED,
    AVAHI_ENTRY_GROUP_REGISTERING,
    AVAHI_ENTRY_GROUP_ESTABLISHED,
    AVAHI_ENTRY_GROUP_COLLISION
} AvahiEntryGroupState;

typedef void (*AvahiEntryGroupCallback) (AvahiServer *s, AvahiEntryGroup *g, AvahiEntryGroupState state, gpointer userdata);

AvahiServer *avahi_server_new(GMainContext *c);
void avahi_server_free(AvahiServer* s);

const AvahiRecord *avahi_server_iterate(AvahiServer *s, AvahiEntryGroup *g, void **state);
void avahi_server_dump(AvahiServer *s, FILE *f);

AvahiEntryGroup *avahi_entry_group_new(AvahiServer *s, AvahiEntryGroupCallback callback, gpointer userdata);
void avahi_entry_group_free(AvahiEntryGroup *g);
void avahi_entry_group_commit(AvahiEntryGroup *g);
AvahiEntryGroupState avahi_entry_group_get_state(AvahiEntryGroup *g);

void avahi_server_add(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    AvahiRecord *r);

void avahi_server_add_ptr(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    const gchar *dest);

void avahi_server_add_address(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    AvahiAddress *a);

void avahi_server_add_text(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    ... /* text records, terminated by NULL */);

void avahi_server_add_text_va(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    va_list va);

void avahi_server_add_text_strlst(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    AvahiStringList *strlst);

void avahi_server_add_service(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    ...  /* text records, terminated by NULL */);

void avahi_server_add_service_va(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    va_list va);

void avahi_server_add_service_strlst(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    AvahiStringList *strlst);

typedef enum {
    AVAHI_SUBSCRIPTION_NEW,
    AVAHI_SUBSCRIPTION_REMOVE,
    AVAHI_SUBSCRIPTION_CHANGE
} AvahiSubscriptionEvent;

typedef struct _AvahiSubscription AvahiSubscription;

typedef void (*AvahiSubscriptionCallback)(AvahiSubscription *s, AvahiRecord *record, gint interface, guchar protocol, AvahiSubscriptionEvent event, gpointer userdata);

AvahiSubscription *avahi_subscription_new(AvahiServer *s, AvahiKey *key, gint interface, guchar protocol, AvahiSubscriptionCallback callback, gpointer userdata);
void avahi_subscription_free(AvahiSubscription *s);

#endif
