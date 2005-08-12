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

#include <inttypes.h>

#include <avahi-common/cdecl.h>
#include <avahi-common/address.h>
#include <avahi-common/strlst.h>
#include <avahi-common/defs.h>

/** \file client.h Definitions and functions for the client API over D-Bus */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

typedef struct _AvahiClient AvahiClient;

typedef struct _AvahiClientEntryGroup AvahiClientEntryGroup;

typedef struct _AvahiClientDomainBrowser AvahiClientDomainBrowser;

typedef struct _AvahiClientServiceTypeBrowser AvahiClientServiceTypeBrowser;

/* Convenience typedefs for slight name differences */
typedef AvahiDomainBrowserType AvahiClientDomainBrowserType;
typedef AvahiEntryGroupState AvahiClientEntryGroupState;

/** States of a client object, note that AvahiServerStates are also emitted */
typedef enum {
    AVAHI_CLIENT_DISCONNECTED = 100, /**< Lost DBUS connection to the Avahi daemon */
    AVAHI_CLIENT_RECONNECTED  = 101  /**< Regained connection to the daemon, all records need to be re-added */
} AvahiClientState;

/** The function prototype for the callback of an AvahiClient */
typedef void (*AvahiClientCallback) (AvahiClient *s,
                                     AvahiClientState state,
                                     void* userdata);

/** The function prototype for the callback of an AvahiClientEntryGroup */
typedef void (*AvahiClientEntryGroupCallback)
                    (AvahiClientEntryGroup *g,
                     AvahiEntryGroupState state,
                     void* userdata);

/** The function prototype for the callback of an AvahiClientDomainBrowser */
typedef void (*AvahiClientDomainBrowserCallback)
                    (AvahiClientDomainBrowser *b,
                     AvahiIfIndex interface,
                     AvahiProtocol protocol,
                     AvahiBrowserEvent event,
                     char *domain,
                     void *user_data);

/** The function prototype for the callback of an AvahiClientServiceTypeBrowser */
typedef void (*AvahiClientServiceTypeBrowserCallback)
                    (AvahiClientServiceTypeBrowser *b, 
                     AvahiIfIndex interface,
                     AvahiProtocol protocol,
                     AvahiBrowserEvent event,
                     char *type,
                     char *domain,
                     void *user_data);

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

/** Create a new AvahiClientEntryGroup object */
AvahiClientEntryGroup* avahi_entry_group_new
                    (AvahiClient*,
                     AvahiClientEntryGroupCallback callback,
                     void *user_data);

/** Commit an AvahiClientEntryGroup */
int avahi_entry_group_commit (AvahiClientEntryGroup*);

/** Reset an AvahiClientEntryGroup */
int avahi_entry_group_reset (AvahiClientEntryGroup*);

/** Get an AvahiClientEntryGroup's state */
int avahi_entry_group_get_state (AvahiClientEntryGroup*);

/** Check if an AvahiClientEntryGroup is empty */
int avahi_entry_group_is_empty (AvahiClientEntryGroup*);

/** Get the last error number */
int avahi_client_errno (AvahiClient*);

/** Get an AvahiClientEntryGroup's owning client instance */
AvahiClient* avahi_entry_group_get_client (AvahiClientEntryGroup*);

/** Add a service, takes an AvahiStringList for text records */
int
avahi_entry_group_add_service (AvahiClientEntryGroup *group,
                               AvahiIfIndex interface,
                               AvahiProtocol protocol,
                               const char *name,
                               const char *type,
                               const char *domain,
                               const char *host,
                               uint16_t port,
                               AvahiStringList *txt);

/** Get the D-Bus path of an AvahiClientEntryGroup object, for debugging purposes only. */
char* avahi_entry_group_path (AvahiClientEntryGroup *);

/** Get the D-Bus path of an AvahiClientDomainBrowser object, for debugging purposes only. */
char* avahi_domain_browser_path (AvahiClientDomainBrowser *);

/** Browse for domains on the local network */
AvahiClientDomainBrowser* avahi_domain_browser_new
                                (AvahiClient *client,
                                AvahiIfIndex interface,
                                AvahiProtocol protocol,
                                char *domain,
                                AvahiDomainBrowserType btype,
                                AvahiClientDomainBrowserCallback callback,
                                void *user_data);

/** Get the D-Bus path of an AvahiClientServiceTypeBrowser object, for debugging purposes only. */
char* avahi_service_type_browser_path (AvahiClientServiceTypeBrowser *);

/** Browse for service types on the local network */
AvahiClientServiceTypeBrowser* avahi_service_type_browser_new (
                AvahiClient *client,
                AvahiIfIndex interface,
                AvahiProtocol protocol,
                char *domain,
                AvahiClientServiceTypeBrowserCallback callback,
                void *user_data);


#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
