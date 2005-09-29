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

/** \example client-publish-service.c Example how to register a DNS-SD
 * service using the client interface to avahi-daemon. It behaves like a network
 * printer registering both an IPP and a BSD LPR service. */

/** \example client-browse-services.c Example how to browse for DNS-SD
 * services using the client interface to avahi-daemon. */

/** \example glib-integration.c Example of how to integrate
 * avahi use with GLIB/GTK applications */
 

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** A connection context */
typedef struct AvahiClient AvahiClient;

/** An entry group object */
typedef struct AvahiEntryGroup AvahiEntryGroup;

/** A domain browser object */
typedef struct AvahiDomainBrowser AvahiDomainBrowser;

/** A service browser object */
typedef struct AvahiServiceBrowser AvahiServiceBrowser;

/** A service type browser object */
typedef struct AvahiServiceTypeBrowser AvahiServiceTypeBrowser;

/** A service resolver object */
typedef struct AvahiServiceResolver AvahiServiceResolver;

/** A service resolver object */
typedef struct AvahiHostNameResolver AvahiHostNameResolver;

/** An address resolver object */
typedef struct AvahiAddressResolver AvahiAddressResolver;

/** States of a client object, a superset of AvahiServerState */
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
typedef void (*AvahiDomainBrowserCallback) (AvahiDomainBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *domain, AvahiLookupResultFlags flags, void *userdata);

/** The function prototype for the callback of an AvahiServiceBrowser */
typedef void (*AvahiServiceBrowserCallback) (AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AvahiLookupResultFlags flags, void *userdata);

/** The function prototype for the callback of an AvahiServiceTypeBrowser */
typedef void (*AvahiServiceTypeBrowserCallback) (AvahiServiceTypeBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *type, const char *domain, AvahiLookupResultFlags flags, void *userdata);

/** The function prototype for the callback of an AvahiServiceResolver */
typedef void (*AvahiServiceResolverCallback) (
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *a,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags, 
    void *userdata);

/** The function prototype for the callback of an AvahiHostNameResolver */
typedef void (*AvahiHostNameResolverCallback) (
    AvahiHostNameResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const AvahiAddress *a,
    AvahiLookupResultFlags flags, 
    void *userdata);

/** The function prototype for the callback of an AvahiAddressResolver */
typedef void (*AvahiAddressResolverCallback) (
    AvahiAddressResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    AvahiProtocol aprotocol,
    const AvahiAddress *a,
    const char *name,
    AvahiLookupResultFlags flags, 
    void *userdata);

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
    AvahiPublishFlags flags,
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
    AvahiPublishFlags flags,
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
    AvahiPublishFlags flags,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    va_list va);

/** Browse for domains on the local network */
AvahiDomainBrowser* avahi_domain_browser_new (
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiDomainBrowserType btype,
    AvahiLookupFlags flags,
    AvahiDomainBrowserCallback callback,
    void *userdata);

/** Get the parent client of an AvahiDomainBrowser object */
AvahiClient* avahi_domain_browser_get_client (AvahiDomainBrowser *);

/** Cleans up and frees an AvahiDomainBrowser object */
int avahi_domain_browser_free (AvahiDomainBrowser *);

/** Browse for service types on the local network */
AvahiServiceTypeBrowser* avahi_service_type_browser_new (
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiLookupFlags flags,
    AvahiServiceTypeBrowserCallback callback,
    void *userdata);

/** Get the parent client of an AvahiServiceTypeBrowser object */
AvahiClient* avahi_service_type_browser_get_client (AvahiServiceTypeBrowser *);

/** Cleans up and frees an AvahiServiceTypeBrowser object */
int avahi_service_type_browser_free (AvahiServiceTypeBrowser *);

/** Browse for services of a type on the local network */
AvahiServiceBrowser* avahi_service_browser_new (
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *type,
    const char *domain,
    AvahiLookupFlags flags,
    AvahiServiceBrowserCallback callback,
    void *userdata);

/** Get the parent client of an AvahiServiceBrowser object */
AvahiClient* avahi_service_browser_get_client (AvahiServiceBrowser *);

/* Cleans up and frees an AvahiServiceBrowser object */
int avahi_service_browser_free (AvahiServiceBrowser *);

/** Create a new service resolver object */
AvahiServiceResolver * avahi_service_resolver_new(
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    AvahiProtocol aprotocol,
    AvahiLookupFlags flags,
    AvahiServiceResolverCallback callback,
    void *userdata);

/** Get the parent client of an AvahiServiceResolver object */
AvahiClient* avahi_service_resolver_get_client (AvahiServiceResolver *);

/** Free a service resolver object */
int avahi_service_resolver_free(AvahiServiceResolver *r);

/** Create a new hostname resolver object */
AvahiHostNameResolver * avahi_host_name_resolver_new(
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    AvahiProtocol aprotocol,
    AvahiLookupFlags flags,
    AvahiHostNameResolverCallback callback,
    void *userdata);

/** Get the parent client of an AvahiHostNameResolver object */
AvahiClient* avahi_host_name_resolver_get_client (AvahiHostNameResolver *);

/** Free a hostname resolver object */
int avahi_host_name_resolver_free(AvahiHostNameResolver *r);

/** Create a new address resolver object from an address string.  Set aprotocol to AF_UNSPEC for protocol detection. */
AvahiAddressResolver * avahi_address_resolver_new(
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *address,
    AvahiLookupFlags flags,
    AvahiAddressResolverCallback callback,
    void *userdata);

/** Create a new address resolver object from an AvahiAddress object */
AvahiAddressResolver* avahi_address_resolver_new_a(
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const AvahiAddress *a,
    AvahiLookupFlags flags,
    AvahiAddressResolverCallback callback,
    void *userdata);

/** Get the parent client of an AvahiAddressResolver object */
AvahiClient* avahi_address_resolver_get_client (AvahiAddressResolver *);

/** Free a AvahiAddressResolver resolver object */
int avahi_address_resolver_free(AvahiAddressResolver *r);

/** Return the local service cookie. returns AVAHI_SERVICE_COOKIE_INVALID on failure. */
uint32_t avahi_client_get_local_service_cookie(AvahiClient *client);

/** Return 1 if the specified service is a registered locally, negative on failure, 0 otherwise. */
int avahi_client_is_service_local(AvahiClient *client, AvahiIfIndex interface, AvahiProtocol protocol, const char *name, const char *type, const char *domain);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
