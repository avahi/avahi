#ifndef foodefshfoo
#define foodefshfoo

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

AVAHI_C_DECL_BEGIN

/** States of an entry group object */
typedef enum {
    AVAHI_ENTRY_GROUP_UNCOMMITED = -1,   /**< The group has not yet been commited, the user must still call avahi_entry_group_commit() */
    AVAHI_ENTRY_GROUP_REGISTERING = 0,   /**< The entries of the group are currently being registered */
    AVAHI_ENTRY_GROUP_ESTABLISHED,       /**< The entries have successfully been established */
    AVAHI_ENTRY_GROUP_COLLISION          /**< A name collision for one of the entries in the group has been detected, the entries have been withdrawn */ 
} AvahiEntryGroupState;

/** The type of domain to browse for */
typedef enum {
    AVAHI_DOMAIN_BROWSER_REGISTER,          /**< Browse for a list of available registering domains */
    AVAHI_DOMAIN_BROWSER_REGISTER_DEFAULT,  /**< Browse for the default registering domain */
    AVAHI_DOMAIN_BROWSER_BROWSE,            /**< Browse for a list of available browsing domains */
    AVAHI_DOMAIN_BROWSER_BROWSE_DEFAULT,    /**< Browse for the default browsing domain */
    AVAHI_DOMAIN_BROWSER_BROWSE_LEGACY,     /**< Legacy browse domain - see DNS-SD spec for more information */
    AVAHI_DOMAIN_BROWSER_MAX
} AvahiDomainBrowserType;

/** Type of callback event when browsing */
typedef enum {
    AVAHI_BROWSER_NEW = 0,            /**< The object is new on the network */
    AVAHI_BROWSER_REMOVE = -1         /**< The object has been removed from the network */
} AvahiBrowserEvent;

/** Type of callback event when resolving */
typedef enum {
    AVAHI_RESOLVER_FOUND = 0,         /**< RR found, resolving successful */
    AVAHI_RESOLVER_TIMEOUT = -1       /**< Noone responded within the timeout, resolving failed */
} AvahiResolverEvent;

/** States of a server object */
typedef enum {
    AVAHI_SERVER_INVALID = -1,     /**< Invalid state (initial) */ 
    AVAHI_SERVER_REGISTERING = 0,  /**< Host RRs are being registered */
    AVAHI_SERVER_RUNNING,          /**< All host RRs have been established */
    AVAHI_SERVER_COLLISION         /**< There is a collision with a host RR. All host RRs have been withdrawn, the user should set a new host name via avahi_server_set_host_name() */
} AvahiServerState;

AVAHI_C_DECL_END

#endif
