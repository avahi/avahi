#ifndef foocorehfoo
#define foocorehfoo

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

#include <glib.h>

/** \file core.h The Avahi Multicast DNS and DNS Service Discovery implmentation. */

/** \example publish-service.c Example how to register a DNS-SD
 * service using an embedded mDNS stack. It behaves like a network
 * printer registering both an IPP and a BSD LPR service. */

/** \example browse-services.c Example how to browse for DNS-SD
 * services using an embedded mDNS stack. */

#include <avahi-common/cdecl.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** An mDNS responder object */
typedef struct AvahiServer AvahiServer;

/** A locally registered DNS resource record */
typedef struct AvahiEntry AvahiEntry;

/** A group of locally registered DNS RRs */
typedef struct AvahiEntryGroup AvahiEntryGroup;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#include <avahi-common/address.h>
#include <avahi-common/rr.h>
#include <avahi-common/alternative.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** Error codes used by avahi */
enum { 
    AVAHI_OK = 0,                          /**< OK */
    AVAHI_ERR_FAILURE = -1,                /**< Generic error code */
    AVAHI_ERR_BAD_STATE = -2,              /**< Object was in a bad state */
    AVAHI_ERR_INVALID_HOST_NAME = -3,      /**< Invalid host name */
    AVAHI_ERR_INVALID_DOMAIN_NAME = -4,    /**< Invalid domain name */
    AVAHI_ERR_NO_NETWORK = -5,             /**< No suitable network protocol available */
    AVAHI_ERR_INVALID_TTL = -6,            /**< Invalid DNS TTL */
    AVAHI_ERR_IS_PATTERN = -7,             /**< RR key is pattern */
    AVAHI_ERR_LOCAL_COLLISION = -8,        /**< Local name collision */
    AVAHI_ERR_INVALID_RECORD = -9,         /**< Invalid RR */
    AVAHI_ERR_INVALID_SERVICE_NAME = -10,  /**< Invalid service name */
    AVAHI_ERR_INVALID_SERVICE_TYPE = -11,  /**< Invalid service type */
    AVAHI_ERR_INVALID_PORT = -12,          /**< Invalid port number */
    AVAHI_ERR_INVALID_KEY = -13,           /**< Invalid key */
    AVAHI_ERR_INVALID_ADDRESS = -14,       /**< Invalid address */
    AVAHI_ERR_TIMEOUT = -15,               /**< Timeout reached */
    AVAHI_ERR_TOO_MANY_CLIENTS = -16,      /**< Too many clients */
    AVAHI_ERR_TOO_MANY_OBJECTS = -17,      /**< Too many objects */
    AVAHI_ERR_TOO_MANY_ENTRIES = -18,      /**< Too many entries */
    AVAHI_ERR_OS = -19,                    /**< OS error */
    AVAHI_ERR_ACCESS_DENIED = -20,         /**< Access denied */
    AVAHI_ERR_INVALID_OPERATION = -21,     /**< Invalid operation */
    AVAHI_ERR_MAX = -22
};

/** States of a server object */
typedef enum {
    AVAHI_SERVER_INVALID = -1,     /**< Invalid state (initial) */ 
    AVAHI_SERVER_REGISTERING = 0,  /**< Host RRs are being registered */
    AVAHI_SERVER_RUNNING,          /**< All host RRs have been established */
    AVAHI_SERVER_COLLISION         /**< There is a collision with a host RR. All host RRs have been withdrawn, the user should set a new host name via avahi_server_set_host_name() */
} AvahiServerState;

/** Flags for server entries */
typedef enum {
    AVAHI_ENTRY_NULL = 0,          /**< No special flags */
    AVAHI_ENTRY_UNIQUE = 1,        /**< The RRset is intended to be unique */
    AVAHI_ENTRY_NOPROBE = 2,       /**< Though the RRset is intended to be unique no probes shall be sent */
    AVAHI_ENTRY_NOANNOUNCE = 4,    /**< Do not announce this RR to other hosts */
    AVAHI_ENTRY_ALLOWMUTIPLE = 8   /**< Allow multiple local records of this type, even if they are intended to be unique */
} AvahiEntryFlags;

/** States of an entry group object */
typedef enum {
    AVAHI_ENTRY_GROUP_UNCOMMITED = -1,   /**< The group has not yet been commited, the user must still call avahi_entry_group_commit() */
    AVAHI_ENTRY_GROUP_REGISTERING = 0,   /**< The entries of the group are currently being registered */
    AVAHI_ENTRY_GROUP_ESTABLISHED,       /**< The entries have successfully been established */
    AVAHI_ENTRY_GROUP_COLLISION          /**< A name collision for one of the entries in the group has been detected, the entries have been withdrawn */ 
} AvahiEntryGroupState;

/** Prototype for callback functions which are called whenever the state of an AvahiServer object changes */
typedef void (*AvahiServerCallback) (AvahiServer *s, AvahiServerState state, gpointer userdata);

/** Prototype for callback functions which are called whenever the state of an AvahiEntryGroup object changes */
typedef void (*AvahiEntryGroupCallback) (AvahiServer *s, AvahiEntryGroup *g, AvahiEntryGroupState state, gpointer userdata);

/** Stores configuration options for a server instance */
typedef struct AvahiServerConfig {
    gchar *host_name;                      /**< Default host name. If left empty defaults to the result of gethostname(2) of the libc */
    gchar *domain_name;                    /**< Default domain name. If left empty defaults to .local */
    gboolean use_ipv4;                     /**< Enable IPv4 support */
    gboolean use_ipv6;                     /**< Enable IPv6 support */
    gboolean publish_hinfo;                /**< Register a HINFO record for the host containing the local OS and CPU type */
    gboolean publish_addresses;            /**< Register A, AAAA and PTR records for all local IP addresses */
    gboolean publish_workstation;          /**< Register a _workstation._tcp service */
    gboolean publish_domain;               /**< Announce the local domain for browsing */
    gboolean check_response_ttl;           /**< If enabled the server ignores all incoming responses with IP TTL != 255. Newer versions of the RFC do no longer contain this check, so it is disabled by default. */
    gboolean use_iff_running;        /**< Require IFF_RUNNING on local network interfaces. This is the official way to check for link beat. Unfortunately this doesn't work with all drivers. So bettere leave this off. */
    gboolean enable_reflector;             /**< Reflect incoming mDNS traffic to all local networks. This allows mDNS based network browsing beyond ethernet borders */
    gboolean reflect_ipv;                  /**< if enable_reflector is TRUE, enable/disable reflecting between IPv4 and IPv6 */
} AvahiServerConfig;

/** Allocate a new mDNS responder object. */
AvahiServer *avahi_server_new(
    GMainContext *c,               /**< The GLIB main loop context to attach to */
    const AvahiServerConfig *sc,   /**< If non-NULL a pointer to a configuration structure for the server. The server makes an internal deep copy of this structure, so you may free it using avahi_server_config_done() immediately after calling this function. */
    AvahiServerCallback callback,  /**< A callback which is called whenever the state of the server changes */
    gpointer userdata,             /**< An opaque pointer which is passed to the callback function */
    gint *error);

/** Free an mDNS responder object */
void avahi_server_free(AvahiServer* s);

/** Fill in default values for a server configuration structure. If you
 * make use of an AvahiServerConfig structure be sure to initialize
 * it with this function for the sake of upwards library
 * compatibility. This call may allocate strings on the heap. To
 * release this memory make sure to call
 * avahi_server_config_done(). If you want to replace any strings in
 * the structure be sure to free the strings filled in by this
 * function with g_free() first and allocate the replacements with
 * g_malloc() (or g_strdup()).*/
AvahiServerConfig* avahi_server_config_init(
   AvahiServerConfig *c /**< A structure which shall be filled in */ );

/** Make a deep copy of the configuration structure *c to *ret. */
AvahiServerConfig* avahi_server_config_copy(
    AvahiServerConfig *ret /**< destination */,
    const AvahiServerConfig *c /**< source */);

/** Free the data in a server configuration structure. */
void avahi_server_config_free(AvahiServerConfig *c);

/** Return the currently chosen domain name of the server object. The
 * return value points to an internally allocated string. Be sure to
 * make a copy of the string before calling any other library
 * functions. */
const gchar* avahi_server_get_domain_name(AvahiServer *s);

/** Return the currently chosen host name. The return value points to a internally allocated string. */
const gchar* avahi_server_get_host_name(AvahiServer *s);

/** Return the currently chosen host name as a FQDN ("fully qualified
 * domain name", i.e. the concatenation of the host and domain
 * name). The return value points to a internally allocated string. */
const gchar* avahi_server_get_host_name_fqdn(AvahiServer *s);

/** Change the host name of a running mDNS responder. This will drop
all automicatilly generated RRs and readd them with the new
name. Since the responder has to probe for the new RRs this function
takes some time to take effect altough it returns immediately. This
function is intended to be called when a host name conflict is
reported using AvahiServerCallback. The caller should readd all user
defined RRs too since they otherwise continue to point to the outdated
host name..*/
gint avahi_server_set_host_name(AvahiServer *s, const gchar *host_name);

/** Change the domain name of a running mDNS responder. The same rules
 * as with avahi_server_set_host_name() apply. */
gint avahi_server_set_domain_name(AvahiServer *s, const gchar *domain_name);

/** Return the opaque user data pointer attached to a server object */
gpointer avahi_server_get_data(AvahiServer *s);

/** Change the opaque user data pointer attached to a server object */
void avahi_server_set_data(AvahiServer *s, gpointer userdata);

/** Return the current state of the server object */
AvahiServerState avahi_server_get_state(AvahiServer *s);

/** Iterate through all local entries of the server. (when g is NULL)
 * or of a specified entry group. At the first call state should point
 * to a NULL initialized void pointer, That pointer is used to track
 * the current iteration. It is not safe to call any other
 * avahi_server_xxx() function during the iteration. If the last entry
 * has been read, NULL is returned. */
const AvahiRecord *avahi_server_iterate(AvahiServer *s, AvahiEntryGroup *g, void **state);

/** Callback prototype for avahi_server_dump() */
typedef void (*AvahiDumpCallback)(const gchar *text, gpointer userdata);

/** Dump the current server status by calling "callback" for each line.  */
void avahi_server_dump(AvahiServer *s, AvahiDumpCallback callback, gpointer userdata);

/** Create a new entry group. The specified callback function is
 * called whenever the state of the group changes. Use entry group
 * objects to keep track of you RRs. Add new RRs to a group using
 * avahi_server_add_xxx(). Make sure to call avahi_entry_group_commit()
 * to start the registration process for your RRs */
AvahiEntryGroup *avahi_entry_group_new(AvahiServer *s, AvahiEntryGroupCallback callback, gpointer userdata);

/** Free an entry group. All RRs assigned to the group are removed from the server */
void avahi_entry_group_free(AvahiEntryGroup *g);

/** Commit an entry group. This starts the probing and registration process for all RRs in the group */
gint avahi_entry_group_commit(AvahiEntryGroup *g);

/** Remove all entries from the entry group and reset the state to AVAHI_ENTRY_GROUP_UNCOMMITED. */
void avahi_entry_group_reset(AvahiEntryGroup *g);

/** Return TRUE if the entry group is empty, i.e. has no records attached. */
gboolean avahi_entry_group_is_empty(AvahiEntryGroup *g);

/** Return the current state of the specified entry group */
AvahiEntryGroupState avahi_entry_group_get_state(AvahiEntryGroup *g);

/** Change the opaque user data pointer attached to an entry group object */
void avahi_entry_group_set_data(AvahiEntryGroup *g, gpointer userdata);

/** Return the opaque user data pointer currently set for the entry group object */
gpointer avahi_entry_group_get_data(AvahiEntryGroup *g);

/** Add a new resource record to the server. Returns 0 on success, negative otherwise. */
gint avahi_server_add(
    AvahiServer *s,           /**< The server object to add this record to */
    AvahiEntryGroup *g,       /**< An entry group object if this new record shall be attached to one, or NULL. If you plan to remove the record sometime later you a required to pass an entry group object here. */
    AvahiIfIndex interface,   /**< A numeric index of a network interface to attach this record to, or AVAHI_IF_UNSPEC to attach this record to all interfaces */
    AvahiProtocol protocol,   /**< A protocol family to attach this record to. One of the AVAHI_PROTO_xxx constants. Use AVAHI_PROTO_UNSPEC to make this record available on all protocols (wich means on both IPv4 and IPv6). */
    AvahiEntryFlags flags,    /**< Special flags for this record */
    AvahiRecord *r            /**< The record to add. This function increases the reference counter of this object. */   );

/** Add a PTR RR to the server. See avahi_server_add() for more information. */
gint avahi_server_add_ptr(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    guint32 ttl,             /**< DNS TTL for this record */
    const gchar *name,       /**< PTR record name */
    const gchar *dest        /**< pointer destination */  );

/** Add a PTR RR to the server. See avahi_server_add() for more information. */
gint avahi_server_add_txt(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    guint32 ttl,             /**< DNS TTL for this record */
    const gchar *name,       /**< TXT record name */
    ... /**< Text record data, terminated by NULL */);

/** Add a PTR RR to the server. Mostly identical to
 * avahi_server_add_text but takes a va_list instead of a variable
 * number of arguments */
gint avahi_server_add_txt_va(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    guint32 ttl,
    const gchar *name,
    va_list va);

/** Add a PTR RR to the server. Mostly identical to 
 * avahi_server_add_text but takes an AvahiStringList record instead of a variable
 * number of arguments. */
gint avahi_server_add_txt_strlst(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    guint32 ttl,
    const gchar *name,
    AvahiStringList *strlst  /**< TXT decord data as a AvahiString. This routine makes a deep copy of this object. */ );

/** Add an IP address mapping to the server. This will add both the
 * host-name-to-address and the reverse mapping to the server. See
 * avahi_server_add() for more information. If adding one of the RRs
 * fails, the function returns with an error, but it is not defined if
 * the other RR is deleted from the server or not. Therefore, you have
 * to free the AvahiEntryGroup and create a new one before
 * proceeding. */
gint avahi_server_add_address(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    AvahiAddress *a);

/** Add an DNS-SD service to the Server. This will add all required
 * RRs to the server. See avahi_server_add() for more information.  If
 * adding one of the RRs fails, the function returns with an error,
 * but it is not defined if the other RR is deleted from the server or
 * not. Therefore, you have to free the AvahiEntryGroup and create a
 * new one before proceeding. */
gint avahi_server_add_service(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *name,         /**< Service name, e.g. "Lennart's Files" */
    const gchar *type,         /**< DNS-SD type, e.g. "_http._tcp" */
    const gchar *domain,       
    const gchar *host,         /**< Host name where this servcie resides, or NULL if on the local host */
    guint16 port,              /**< Port number of the service */
    ...  /**< Text records, terminated by NULL */);

/** Mostly identical to avahi_server_add_service(), but takes an va_list for the TXT records. */
gint avahi_server_add_service_va(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *name,
    const gchar *type,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    va_list va);

/** Mostly identical to avahi_server_add_service(), but takes an AvahiStringList object for the TXT records.  The AvahiStringList object is copied. */
gint avahi_server_add_service_strlst(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *name,
    const gchar *type,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    AvahiStringList *strlst);

/** The type of DNS server */
typedef enum {
    AVAHI_DNS_SERVER_RESOLVE,         /**< Unicast DNS servers for normal resolves (_domain._udp)*/
    AVAHI_DNS_SERVER_UPDATE           /**< Unicast DNS servers for updates (_dns-update._udp)*/
} AvahiDNSServerType;

/** Publish the specified unicast DNS server address via mDNS. You may
 * browse for records create this way wit
 * avahi_dns_server_browser_new(). */
gint avahi_server_add_dns_server_address(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *domain,
    AvahiDNSServerType type,
    const AvahiAddress *address,
    guint16 port /** should be 53 */);

/** Similar to avahi_server_add_dns_server_address(), but specify a
host name instead of an address. The specified host name should be
resolvable via mDNS */
gint avahi_server_add_dns_server_name(
    AvahiServer *s,
    AvahiEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *domain,
    AvahiDNSServerType type,
    const gchar *name,
    guint16 port /** should be 53 */);

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

/** A browsing object for arbitrary RRs */
typedef struct AvahiRecordBrowser AvahiRecordBrowser;

/** Callback prototype for AvahiRecordBrowser events */
typedef void (*AvahiRecordBrowserCallback)(
    AvahiRecordBrowser *b,       /**< The AvahiRecordBrowser object that is emitting this callback */
    AvahiIfIndex interface,      /**< Logical OS network interface number the record was found on */
    AvahiProtocol protocol,      /**< Protocol number the record was found. */
    AvahiBrowserEvent event,     /**< Browsing event, either AVAHI_BROWSER_NEW or AVAHI_BROWSER_REMOVE */
    AvahiRecord *record,         /**< The record that was found */
    gpointer userdata            /**< Arbitrary user data passed to avahi_record_browser_new() */ );

/** Create a new browsing object for arbitrary RRs */
AvahiRecordBrowser *avahi_record_browser_new(
    AvahiServer *server,                  /**< The server object to which attach this query */
    AvahiIfIndex interface,               /**< Logical OS interface number where to look for the records, or AVAHI_IF_UNSPEC to look on interfaces */
    AvahiProtocol protocol,               /**< Protocol number to use when looking for the record, or AVAHI_PROTO_UNSPEC to look on all protocols */
    AvahiKey *key,                        /**< The search key */
    AvahiRecordBrowserCallback callback,  /**< The callback to call on browsing events */
    gpointer userdata                     /**< Arbitrary use suppliable data which is passed to the callback */);

/** Free an AvahiRecordBrowser object */
void avahi_record_browser_free(AvahiRecordBrowser *b);

/** A host name to IP adddress resolver object */
typedef struct AvahiHostNameResolver AvahiHostNameResolver;

/** Callback prototype for AvahiHostNameResolver events */
typedef void (*AvahiHostNameResolverCallback)(
    AvahiHostNameResolver *r,
    AvahiIfIndex interface,  
    AvahiProtocol protocol,
    AvahiResolverEvent event, /**< Resolving event */
    const gchar *host_name,   /**< Host name which should be resolved. May differ in case from the query */
    const AvahiAddress *a,    /**< The address, or NULL if the host name couldn't be resolved. */
    gpointer userdata);

/** Create an AvahiHostNameResolver object for resolving a host name to an adddress. See AvahiRecordBrowser for more info on the paramters. */
AvahiHostNameResolver *avahi_host_name_resolver_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *host_name,    /**< The host name to look for */
    AvahiProtocol aprotocol,   /**< The address family of the desired address or AVAHI_PROTO_UNSPEC if doesn't matter. */
    AvahiHostNameResolverCallback calback,
    gpointer userdata);

/** Free a AvahiHostNameResolver object */
void avahi_host_name_resolver_free(AvahiHostNameResolver *r);

/** An IP address to host name resolver object ("reverse lookup") */
typedef struct AvahiAddressResolver AvahiAddressResolver;

/** Callback prototype for AvahiAddressResolver events */
typedef void (*AvahiAddressResolverCallback)(
    AvahiAddressResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const AvahiAddress *a,   
    const gchar *host_name,   /**< A host name for the specified address, if one was found, i.e. event == AVAHI_RESOLVER_FOUND */
    gpointer userdata);

/** Create an AvahiAddressResolver object. See AvahiRecordBrowser for more info on the paramters. */
AvahiAddressResolver *avahi_address_resolver_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const AvahiAddress *address,
    AvahiAddressResolverCallback calback,
    gpointer userdata);

/** Free an AvahiAddressResolver object */
void avahi_address_resolver_free(AvahiAddressResolver *r);

/** The type of domain to browse for */
typedef enum {
    AVAHI_DOMAIN_BROWSER_REGISTER,          /**< Browse for a list of available registering domains */
    AVAHI_DOMAIN_BROWSER_REGISTER_DEFAULT,  /**< Browse for the default registering domain */
    AVAHI_DOMAIN_BROWSER_BROWSE,            /**< Browse for a list of available browsing domains */
    AVAHI_DOMAIN_BROWSER_BROWSE_DEFAULT,    /**< Browse for the default browsing domain */
    AVAHI_DOMAIN_BROWSER_BROWSE_LEGACY,     /**< Legacy browse domain - see DNS-SD spec for more information */
    AVAHI_DOMAIN_BROWSER_MAX
} AvahiDomainBrowserType;

/** A local domain browsing object. May be used to enumerate domains used on the local LAN */
typedef struct AvahiDomainBrowser AvahiDomainBrowser;

/** Callback prototype for AvahiDomainBrowser events */
typedef void (*AvahiDomainBrowserCallback)(
    AvahiDomainBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const gchar *domain,
    gpointer userdata);

/** Create a new AvahiDomainBrowser object */
AvahiDomainBrowser *avahi_domain_browser_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *domain,
    AvahiDomainBrowserType type,
    AvahiDomainBrowserCallback callback,
    gpointer userdata);

/** Free an AvahiDomainBrowser object */
void avahi_domain_browser_free(AvahiDomainBrowser *b);

/** A DNS-SD service type browsing object. May be used to enumerate the service types of all available services on the local LAN */
typedef struct AvahiServiceTypeBrowser AvahiServiceTypeBrowser;

/** Callback prototype for AvahiServiceTypeBrowser events */
typedef void (*AvahiServiceTypeBrowserCallback)(
    AvahiServiceTypeBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const gchar *type,
    const gchar *domain,
    gpointer userdata);

/** Create a new AvahiServiceTypeBrowser object. */
AvahiServiceTypeBrowser *avahi_service_type_browser_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *domain,
    AvahiServiceTypeBrowserCallback callback,
    gpointer userdata);

/** Free an AvahiServiceTypeBrowser object */
void avahi_service_type_browser_free(AvahiServiceTypeBrowser *b);

/** A DNS-SD service browser. Use this to enumerate available services of a certain kind on the local LAN. Use AvahiServiceResolver to get specific service data like address and port for a service. */
typedef struct AvahiServiceBrowser AvahiServiceBrowser;

/** Callback prototype for AvahiServiceBrowser events */
typedef void (*AvahiServiceBrowserCallback)(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const gchar *name     /**< Service name, e.g. "Lennart's Files" */,
    const gchar *type     /**< DNS-SD type, e.g. "_http._tcp" */,
    const gchar *domain   /**< Domain of this service, e.g. "local" */,
    gpointer userdata);

/** Create a new AvahiServiceBrowser object. */
AvahiServiceBrowser *avahi_service_browser_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *service_type /** DNS-SD service type, e.g. "_http._tcp" */,
    const gchar *domain,
    AvahiServiceBrowserCallback callback,
    gpointer userdata);

/** Free an AvahiServiceBrowser object */
void avahi_service_browser_free(AvahiServiceBrowser *b);

/** A DNS-SD service resolver.  Use this to retrieve addres, port and TXT data for a DNS-SD service */
typedef struct AvahiServiceResolver AvahiServiceResolver;

/** Callback prototype for AvahiServiceResolver events */
typedef void (*AvahiServiceResolverCallback)(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const gchar *name,       /**< Service name */
    const gchar *type,       /**< Service Type */
    const gchar *domain,
    const gchar *host_name,  /**< Host name of the service */
    const AvahiAddress *a,   /**< The resolved host name */
    guint16 port,            /**< Service name */
    AvahiStringList *txt,    /**< TXT record data */
    gpointer userdata);

/** Create a new AvahiServiceResolver object */
AvahiServiceResolver *avahi_service_resolver_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *name,
    const gchar *type,
    const gchar *domain,
    AvahiProtocol aprotocol,    /**< Address family of the desired service address. Use AVAHI_PROTO_UNSPEC if you don't care */
    AvahiServiceResolverCallback calback,
    gpointer userdata);

/** Free an AvahiServiceResolver object */
void avahi_service_resolver_free(AvahiServiceResolver *r);

/** A domain service browser object. Use this to browse for
 * conventional unicast DNS servers which may be used to resolve
 * conventional domain names */
typedef struct AvahiDNSServerBrowser AvahiDNSServerBrowser;

/** Callback prototype for AvahiDNSServerBrowser events */
typedef void (*AvahiDNSServerBrowserCallback)(
    AvahiDNSServerBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const gchar *host_name,       /**< Host name of the DNS server, probably useless */
    const AvahiAddress *a,        /**< Address of the DNS server */
    guint16 port,                 /**< Port number of the DNS servers, probably 53 */
    gpointer userdata);

/** Create a new AvahiDNSServerBrowser object */
AvahiDNSServerBrowser *avahi_dns_server_browser_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const gchar *domain,
    AvahiDNSServerType type,
    AvahiProtocol aprotocol,  /**< Address protocol for the DNS server */ 
    AvahiDNSServerBrowserCallback callback,
    gpointer userdata);

/** Free an AvahiDNSServerBrowser object */
void avahi_dns_server_browser_free(AvahiDNSServerBrowser *b);

/** Return a human readable error string for the specified error code */
const gchar *avahi_strerror(gint error);

/** Return the last error code */
gint avahi_server_errno(AvahiServer *s);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
