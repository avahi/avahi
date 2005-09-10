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

/** \file defs.h Some common definitions */

#include <avahi-common/cdecl.h>

/** \mainpage
 *  
 * \section choose_api Choosing an API
 *
 * Avahi provides three programming APIs for integration of
 * mDNS/DNS-SD features into your progams:
 *
 * \li avahi-core: an API for embedding a complete mDNS/DNS-SD stack
 * into your software. This is intended for developers of embedded
 * ampliances only. We dissuade from using this API in normal desktop
 * applications since it is not a good idea to run multiple mDNS
 * stacks simultaneously on the same host.
 * \li The DBUS API: an extensive DBUS interface for browsing and
 * registering mDNS/DNS-SD services using avahi-daemon. We recommend
 * to use this API for software written in any language but
 * C. (i.e. python)
 * \li avahi-client: a simplifying C wrapper around the DBUS API. We
 * recommend to use this API in C or C++ progams. The DBUS internals
 * are hidden completely.
 * 
 * All three APIs are very similar, however avahi-core is the most powerful.
 *
 * \section error_reporting Error Reporting
 *
 * Some notes on the Avahi error handling:
 *
 * - Error codes are negative integers and defined as AVAHI_ERR_xx
 * - If a function returns some kind of non-negative integer value
 * on success, a failure is indicated by returning the error code
 * directly.
 * - If a function returns a pointer of some kind on success, a
 * failure is indicated by returning NULL
 * - The last error number may be retrieved by calling
 * avahi_server_errno() (for the server API) or avahi_client_errno()
 * (for the client API)
 * - Just like the libc errno variable the Avahi errno is NOT reset
 * to AVAHI_OK if a function call succeeds.
 * - You may convert a numeric error code into a human readable
 * string using avahi_strerror()
 * - The constructor functions avahi_server_new() and
 * avahi_client_new() return the error code in a call-by-reference
 * argument
 *
 * \section event_loop Event Loop Abstraction
 *
 * Avahi uses for both avahi-client and avahi-core a simple event loop
 * abstraction layer.A table AvahiPoll which contains function
 * pointers for user defined timeout and I/O condition event source
 * implementations, needs to be passed to avahi_server_new() and
 * avahi_client_new(). An adapter for this abstraction layer is
 * available for the GLib main loop in the object AvahiGLibPoll. A
 * simple stand-alone implementation is available under the name
 * AvahiSimplePoll.
 *
 * \section good_publish How to Register Services
 *
 * - Subscribe to server state changes. Pass a callback function
 * pointer to avahi_client_new()/avahi_server_new(). It will be called
 * whenever the server state changes.
 * - Only register your services when the server is in state
 * AVAHI_SERVER_RUNNING. If you register your services in other server
 * states they might not be accessible since the local host name is
 * not established.
 * - Remove your services when the server enters
 * AVAHI_SERVER_COLLISION state. Your services may no be reachable
 * anymore since the local host name is no longer established.
 * - When registering services, use the following algorithm:
 *   - Create a new entry group (i.e. avahi_entry_group_new())
 *   - Add your service(s)/additional host names (i.e. avahi_entry_group_add_service())
 *   - Commit the entry group (i.e. avahi_entry_group_commit())
 * - Subscribe to entry group state changes.
 * - If the entry group enters AVAHI_ENTRY_GROUP_COLLISION state the
 * services of the entry group are automatically removed from the
 * server. You may immediately add your services back to the entry
 * group (but with new names, perhaps using
 * avahi_alternative_service_name()) and commit again. Please do not
 * free the entry group and create a new one. This would inhibit some
 * traffic limiting algorithms in mDNS.
 * - When you need to modify your services, reset the entry group
 * (i.e. avahi_entry_group_reset()) and add them back. Please do not
 * free the entry group and create a new one. This would inhibit some
 * traffic limiting algorithms in mDNS.
 *
 * The linked functions belong to avahi-client. They all have counterparts in the DBUS API and avahi-core.
 *
 * \section good_browse How to Browse for Services
 *
 * - For normal applications you need to call avahi_service_browser_new()
 * for the service type you want to browse for. Use
 * avahi_client_resolve_service() to acquire service data for a a service
 * name.
 * - You can use avahi_domain_browser() to get a list of announced
 * browsing domains. Please note that not all domains whith services
 * on the LAN are mandatorily announced.
 * - Network monitor software may use avahi_service_type_browser_new()
 * to browse for the list of available service types on the LAN. This
 * API should NOT be used in normal software since it increases
 * traffic heavily.
 * - There is no need to subscribe to server state changes.
 *  
 * The linked functions belong to avahi-client. They all have counterparts in the DBUS API and avahi-core.
 *  
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** States of an entry group object */
typedef enum {
    AVAHI_ENTRY_GROUP_UNCOMMITED,    /**< The group has not yet been commited, the user must still call avahi_entry_group_commit() */
    AVAHI_ENTRY_GROUP_REGISTERING,   /**< The entries of the group are currently being registered */
    AVAHI_ENTRY_GROUP_ESTABLISHED,   /**< The entries have successfully been established */
    AVAHI_ENTRY_GROUP_COLLISION      /**< A name collision for one of the entries in the group has been detected, the entries have been withdrawn */ 
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
    AVAHI_BROWSER_NEW,            /**< The object is new on the network */
    AVAHI_BROWSER_REMOVE          /**< The object has been removed from the network */
} AvahiBrowserEvent;

/** Type of callback event when resolving */
typedef enum {
    AVAHI_RESOLVER_FOUND,         /**< RR found, resolving successful */
    AVAHI_RESOLVER_TIMEOUT        /**< Noone responded within the timeout, resolving failed */
} AvahiResolverEvent;

/** States of a server object */
typedef enum {
    AVAHI_SERVER_INVALID,          /**< Invalid state (initial) */ 
    AVAHI_SERVER_REGISTERING,      /**< Host RRs are being registered */
    AVAHI_SERVER_RUNNING,          /**< All host RRs have been established */
    AVAHI_SERVER_COLLISION         /**< There is a collision with a host RR. All host RRs have been withdrawn, the user should set a new host name via avahi_server_set_host_name() */
} AvahiServerState;

/** For every service a special TXT item is implicitly added, which
 * contains a random cookie which is private to the local daemon. This
 * can be used by clients to determine if two services on two
 * different subnets are effectively the same. */
#define AVAHI_SERVICE_COOKIE "org.freedesktop.Avahi.cookie"

/** In invalid cookie as special value */
#define AVAHI_SERVICE_COOKIE_INVALID (0)

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
