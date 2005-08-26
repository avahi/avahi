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

#include <dbus/dbus.h>
#include "client.h"

struct AvahiClient {
    const AvahiPoll *poll_api;
    DBusConnection *bus;
    int error;
    AvahiClientState state;

    /* Cache for some seldom changing server data */
    char *version_string, *host_name, *host_name_fqdn, *domain_name;
    
    AvahiClientCallback callback;
    void *userdata;
    
    AVAHI_LLIST_HEAD(AvahiEntryGroup, groups);
    AVAHI_LLIST_HEAD(AvahiDomainBrowser, domain_browsers);
    AVAHI_LLIST_HEAD(AvahiServiceBrowser, service_browsers);
    AVAHI_LLIST_HEAD(AvahiServiceTypeBrowser, service_type_browsers);
    AVAHI_LLIST_HEAD(AvahiServiceResolver, service_resolvers);
    AVAHI_LLIST_HEAD(AvahiHostNameResolver, host_name_resolvers);
    AVAHI_LLIST_HEAD(AvahiAddressResolver, address_resolvers);
};

struct AvahiEntryGroup {
    char *path;
    AvahiEntryGroupState state;
    AvahiClient *client;
    AvahiEntryGroupCallback callback;
    void *userdata;
    AVAHI_LLIST_FIELDS(AvahiEntryGroup, groups);
};

struct AvahiDomainBrowser {
    char *path;
    AvahiClient *client;
    AvahiDomainBrowserCallback callback;
    void *userdata;
    AVAHI_LLIST_FIELDS(AvahiDomainBrowser, domain_browsers);
};

struct AvahiServiceBrowser {
    char *path;
    AvahiClient *client;
    AvahiServiceBrowserCallback callback;
    void *userdata;
    AVAHI_LLIST_FIELDS(AvahiServiceBrowser, service_browsers);
};

struct AvahiServiceTypeBrowser {
    char *path;
    AvahiClient *client;
    AvahiServiceTypeBrowserCallback callback;
    void *userdata;
    AVAHI_LLIST_FIELDS(AvahiServiceTypeBrowser, service_type_browsers);
};

struct AvahiServiceResolver {
    DBusPendingCall *call;
    AvahiClient *client;
    AvahiServiceResolverCallback callback;
    void *userdata;
    AVAHI_LLIST_FIELDS(AvahiServiceResolver, service_resolvers);
};

struct AvahiHostNameResolver {
    char *path;
    DBusPendingCall *call;
    AvahiClient *client;
    AvahiHostNameResolverCallback callback;
    void *userdata;
    AVAHI_LLIST_FIELDS(AvahiHostNameResolver, host_name_resolvers);
};

struct AvahiAddressResolver {
    char *path;
    DBusPendingCall *call;
    AvahiClient *client;
    AvahiAddressResolverCallback callback;
    void *userdata;
    AVAHI_LLIST_FIELDS(AvahiAddressResolver, address_resolvers);
};

int avahi_client_set_errno (AvahiClient *client, int error);
int avahi_client_set_dbus_error(AvahiClient *client, DBusError *error);

void avahi_entry_group_set_state(AvahiEntryGroup *group, AvahiEntryGroupState state);

DBusHandlerResult avahi_domain_browser_event (AvahiClient *client, AvahiBrowserEvent event, DBusMessage *message);

DBusHandlerResult avahi_service_type_browser_event (AvahiClient *client, AvahiBrowserEvent event, DBusMessage *message);

DBusHandlerResult avahi_service_browser_event (AvahiClient *client, AvahiBrowserEvent event, DBusMessage *message);

#endif
