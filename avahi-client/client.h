#ifndef fooclienthfoo
#define fooclienthfoo

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
#include <avahi-common/address.h>
#include <avahi-common/strlst.h>
#include <avahi-common/defs.h>

/** \file client.h Definitions and functions for the client API over D-Bus */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

typedef struct _AvahiClient AvahiClient;

typedef struct _AvahiEntryGroup AvahiEntryGroup;

typedef struct _AvahiDomainBrowser AvahiDomainBrowser;

typedef struct _AvahiServiceTypeBrowser AvahiServiceTypeBrowser;

/** States of a client object, note that AvahiServerStates are also emitted */
typedef enum {
    AVAHI_CLIENT_DISCONNECTED = 100, /**< Lost DBUS connection to the Avahi daemon */
    AVAHI_CLIENT_RECONNECTED  = 101  /**< Regained connection to the daemon, all records need to be re-added */
} AvahiClientState;

/** The function prototype for the callback of an AvahiClient */
typedef void (*AvahiClientCallback) (AvahiClient *s, AvahiClientState state, void* userdata);

/** The function prototype for the callback of an AvahiEntryGroup */
typedef void (*AvahiEntryGroupCallback) (AvahiEntryGroup *g, AvahiEntryGroupState state, void* userdata);

/** The function prototype for the callback of an AvahiDomainBrowser */
typedef void (*AvahiDomainBrowserCallback) (AvahiDomainBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, char *domain, void *user_data);

/** The function prototype for the callback of an AvahiServiceTypeBrowser */
typedef void (*AvahiServiceTypeBrowserCallback) (AvahiServiceTypeBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, char *type, char *domain, void *user_data);

/** Creates a new client instance */
AvahiClient* avahi_client_new (AvahiClientCallback callback, void *user_data);

/** Get the version of the server */
char* avahi_client_get_version_string (AvahiClient*);

/** Get host name */
char* avahi_client_get_host_name (AvahiClient*);

/** Get domain name */
char* avahi_client_get_domain_name (AvahiClient*);

/** Get FQDN domain name */
char* avahi_client_get_host_name_fqdn (AvahiClient*);

/** Create a new AvahiEntryGroup object */
AvahiEntryGroup* avahi_entry_group_new (AvahiClient*, AvahiEntryGroupCallback callback, void *user_data);

/** Commit an AvahiEntryGroup */
int avahi_entry_group_commit (AvahiEntryGroup*);

/** Reset an AvahiEntryGroup */
int avahi_entry_group_reset (AvahiEntryGroup*);

/** Get an AvahiEntryGroup's state */
int avahi_entry_group_state (AvahiEntryGroup*);

/** Check if an AvahiEntryGroup is empty */
int avahi_entry_group_is_empty (AvahiEntryGroup*);

/** Get the last error number */
int avahi_client_errno (AvahiClient*);

/** Get an AvahiEntryGroup's owning client instance */
AvahiClient* avahi_entry_group_get_client (AvahiEntryGroup*);

/** Add a service, takes an AvahiStringList for text records */
int
avahi_entry_group_add_service (AvahiEntryGroup *group,
                               AvahiIfIndex interface,
                               AvahiProtocol protocol,
                               const char *name,
                               const char *type,
                               const char *domain,
                               const char *host,
                               int port,
                               AvahiStringList *txt);

/** Get the D-Bus path of an AvahiEntryGroup object, for debugging purposes only. */
char* avahi_entry_group_path (AvahiEntryGroup *);

/** Get the D-Bus path of an AvahiDomainBrowser object, for debugging purposes only. */
char* avahi_domain_browser_path (AvahiDomainBrowser *);

/** Browse for domains on the local network */
AvahiDomainBrowser* avahi_domain_browser_new (AvahiClient *client,
                                              AvahiIfIndex interface,
                                              AvahiProtocol protocol,
                                              char *domain,
                                              AvahiDomainBrowserType btype,
                                              AvahiDomainBrowserCallback callback,
                                              void *user_data);

/** Get the D-Bus path of an AvahiServiceTypeBrowser object, for debugging purposes only. */
char* avahi_service_type_browser_path (AvahiServiceTypeBrowser *);

/** Browse for service types on the local network */
AvahiServiceTypeBrowser* avahi_service_type_browser_new (
                AvahiClient *client,
                AvahiIfIndex interface,
                AvahiProtocol protocol,
                char *domain,
                AvahiServiceTypeBrowserCallback callback,
                void *user_data);


#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
