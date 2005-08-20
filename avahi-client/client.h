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
#include <avahi-common/watch.h>
#include <avahi-common/gccmacro.h>

/** \file client.h Definitions and functions for the client API over D-Bus */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

typedef struct AvahiClient AvahiClient;
typedef struct AvahiEntryGroup AvahiEntryGroup;
typedef struct AvahiDomainBrowser AvahiDomainBrowser;
typedef struct AvahiServiceBrowser AvahiServiceBrowser;
typedef struct AvahiServiceTypeBrowser AvahiServiceTypeBrowser;

/** States of a client object, note that AvahiServerStates are also emitted */
typedef enum {
    AVAHI_CLIENT_S_INVALID = AVAHI_SERVER_INVALID,
    AVAHI_CLIENT_S_REGISTERING = AVAHI_SERVER_REGISTERING,
    AVAHI_CLIENT_S_RUNNING = AVAHI_SERVER_RUNNING,
    AVAHI_CLIENT_S_COLLISION = AVAHI_SERVER_COLLISION,
    AVAHI_CLIENT_DISCONNECTED = 100 /**< Lost DBUS connection to the Avahi daemon */
} AvahiClientState;

/** The function prototype for the callback of an AvahiClient */
typedef void (*AvahiClientCallback) (AvahiClient *s, AvahiClientState state, void* userdata);

/** The function prototype for the callback of an AvahiEntryGroup */
typedef void (*AvahiEntryGroupCallback) (AvahiEntryGroup *g, AvahiEntryGroupState state, void* userdata);

/** The function prototype for the callback of an AvahiDomainBrowser */
typedef void (*AvahiDomainBrowserCallback) (AvahiDomainBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *domain, void *userdata);

/** The function prototype for the callback of an AvahiServiceBrowser */
typedef void (*AvahiServiceBrowserCallback) (AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, void *userdata);

/** The function prototype for the callback of an AvahiServiceTypeBrowser */
typedef void (*AvahiServiceTypeBrowserCallback) (AvahiServiceTypeBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *type, const char *domain, void *userdata);

/** Creates a new client instance */
AvahiClient* avahi_client_new (const AvahiPoll *poll_api, AvahiClientCallback callback, void *userdata, int *error);

/** Free a client instance */
void avahi_client_free(AvahiClient *client);

/** Get the version of the server */
const char* avahi_client_get_version_string (AvahiClient*);

/** Get host name */
const char* avahi_client_get_host_name (AvahiClient*);

/** Get domain name */
const char* avahi_client_get_domain_name (AvahiClient*);

/** Get FQDN domain name */
const char* avahi_client_get_host_name_fqdn (AvahiClient*);

/** Get state */
AvahiClientState avahi_client_get_state(AvahiClient *client); 

/** Get the last error number */
int avahi_client_errno (AvahiClient*);

/** Create a new AvahiEntryGroup object */
AvahiEntryGroup* avahi_entry_group_new (AvahiClient*, AvahiEntryGroupCallback callback, void *userdata);

/** Clean up and free an AvahiEntryGroup object */
int avahi_entry_group_free (AvahiEntryGroup *);

/** Commit an AvahiEntryGroup */
int avahi_entry_group_commit (AvahiEntryGroup*);

/** Reset an AvahiEntryGroup */
int avahi_entry_group_reset (AvahiEntryGroup*);

/** Get an AvahiEntryGroup's state */
int avahi_entry_group_get_state (AvahiEntryGroup*);

/** Check if an AvahiEntryGroup is empty */
int avahi_entry_group_is_empty (AvahiEntryGroup*);

/** Get an AvahiEntryGroup's owning client instance */
AvahiClient* avahi_entry_group_get_client (AvahiEntryGroup*);

/** Add a service, takes a variable NULL terminated list of text records */
int avahi_entry_group_add_service(
    AvahiEntryGroup *group,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    ...) AVAHI_GCC_SENTINEL;

/** Add a service, takes an AvahiStringList for text records */
int avahi_entry_group_add_service_strlst(
    AvahiEntryGroup *group,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    AvahiStringList *txt);

/** Add a service, takes a NULL terminated va_list for text records */
int avahi_entry_group_add_service_va(
    AvahiEntryGroup *group,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    va_list va);

/** Get the D-Bus path of an AvahiEntryGroup object, for debugging purposes only. */
const char* avahi_entry_group_get_dbus_path (AvahiEntryGroup *);

/** Browse for domains on the local network */
AvahiDomainBrowser* avahi_domain_browser_new (AvahiClient *client,
                                              AvahiIfIndex interface,
                                              AvahiProtocol protocol,
                                              const char *domain,
                                              AvahiDomainBrowserType btype,
                                              AvahiDomainBrowserCallback callback,
                                              void *userdata);

/** Get the D-Bus path of an AvahiDomainBrowser object, for debugging purposes only. */
const char* avahi_domain_browser_get_dbus_path (AvahiDomainBrowser *);

/** Cleans up and frees an AvahiDomainBrowser object */
int avahi_domain_browser_free (AvahiDomainBrowser *);

/** Browse for service types on the local network */
AvahiServiceTypeBrowser* avahi_service_type_browser_new (
                AvahiClient *client,
                AvahiIfIndex interface,
                AvahiProtocol protocol,
                const char *domain,
                AvahiServiceTypeBrowserCallback callback,
                void *userdata);

/** Get the D-Bus path of an AvahiServiceTypeBrowser object, for debugging purposes only. */
const char* avahi_service_type_browser_get_dbus_path(AvahiServiceTypeBrowser *);

/** Cleans up and frees an AvahiServiceTypeBrowser object */
int avahi_service_type_browser_free (AvahiServiceTypeBrowser *);

/** Browse for services of a type on the local network */
AvahiServiceBrowser* avahi_service_browser_new (
                AvahiClient *client,
                AvahiIfIndex interface,
                AvahiProtocol protocol,
                const char *type,
                const char *domain,
                AvahiServiceBrowserCallback callback,
                void *userdata);

/** Get the D-Bus path of an AvahiServiceBrowser object, for debugging purposes only. */
const char* avahi_service_browser_get_dbus_path (AvahiServiceBrowser *);

/* Cleans up and frees an AvahiServiceBrowser object */
int avahi_service_browser_free (AvahiServiceBrowser *);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
