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

/** \example glib-integration.c Example of how to integrate
 * avahi use with GLIB/GTK applications */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** A connection context */
typedef struct AvahiClient AvahiClient;

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

/** Return the local service cookie. returns AVAHI_SERVICE_COOKIE_INVALID on failure. */
uint32_t avahi_client_get_local_service_cookie(AvahiClient *client);

/** Return 1 if the specified service is a registered locally, negative on failure, 0 otherwise. */
int avahi_client_is_service_local(AvahiClient *client, AvahiIfIndex interface, AvahiProtocol protocol, const char *name, const char *type, const char *domain);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
