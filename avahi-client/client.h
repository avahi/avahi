#ifndef fooclienthfoo
#define fooclienthfoo

/* $Id: core.h 98 2005-06-04 18:56:52Z lathiat $ */

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

/** \file client.h Definitions and functions for the client API over D-Bus */

AVAHI_C_DECL_BEGIN

typedef struct _AvahiClientPriv AvahiClientPriv;

typedef struct _AvahiClient {
	int serverid;
    AvahiClientPriv *priv;
} AvahiClient;

/** Creates a new client instance */
AvahiClient* avahi_client_new ();

/** Get the version of the server */
char* avahi_client_get_version_string (AvahiClient*);

/** Get host name */
char* avahi_client_get_host_name (AvahiClient*);

/** Get domain name */
char* avahi_client_get_domain_name (AvahiClient*);

/** Get FQDN domain name */
char* avahi_client_get_host_name_fqdn (AvahiClient*);

/** Get alternative host name for a host name that is taken */
char* avahi_client_get_alternative_host_name (AvahiClient*, char*);

/** Get alternative service name for a service name that is taken */
char* avahi_client_get_alternative_service_name (AvahiClient*, char*);

AVAHI_C_DECL_END

#endif
