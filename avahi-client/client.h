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

/** \file client.h Definitions and functions for the client API over D-Bus */

AVAHI_C_DECL_BEGIN

typedef struct _AvahiClient AvahiClient;

typedef struct _AvahiEntryGroup AvahiEntryGroup;

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

AVAHI_C_DECL_END

#endif
