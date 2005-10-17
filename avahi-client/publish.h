#ifndef fooclientpublishhfoo
#define fooclientpublishhfoo

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

#include <avahi-client/client.h>

/** \file avahi-client/publish.h Publishing Client API */

/** \example client-publish-service.c Example how to register a DNS-SD
 * service using the client interface to avahi-daemon. It behaves like a network
 * printer registering both an IPP and a BSD LPR service. */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** An entry group object */
typedef struct AvahiEntryGroup AvahiEntryGroup;

/** The function prototype for the callback of an AvahiEntryGroup */
typedef void (*AvahiEntryGroupCallback) (AvahiEntryGroup *g, AvahiEntryGroupState state, void* userdata);

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

/** Add a subtype for a service */
int avahi_entry_group_add_service_subtype(
    AvahiEntryGroup *group,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    const char *type,
    const char *domain,
    const char *subtype);

/** Update a TXT record for an existing service */
int avahi_entry_group_update_service_txt(
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,     
    const char *type,     
    const char *domain,   
    ...) AVAHI_GCC_SENTINEL;

/** Update a TXT record for an existing service */
int avahi_entry_group_update_service_txt_strlst(
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,     
    const char *type,     
    const char *domain,   
    AvahiStringList *strlst);

/** Update a TXT record for an existing service */
int avahi_entry_group_update_service_txt_va(
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,     
    const char *type,     
    const char *domain,   
    va_list va);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
