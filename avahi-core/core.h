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

/** \file core.h The Avahi Multicast DNS and DNS Service Discovery implmentation. */

/** \example core-publish-service.c Example how to register a DNS-SD
 * service using an embedded mDNS stack. It behaves like a network
 * printer registering both an IPP and a BSD LPR service. */

/** \example core-browse-services.c Example how to browse for DNS-SD
 * services using an embedded mDNS stack. */

#include <avahi-common/cdecl.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** An mDNS responder object */
typedef struct AvahiServer AvahiServer;

/** A group of locally registered DNS RRs */
typedef struct AvahiSEntryGroup AvahiSEntryGroup;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#include <avahi-core/rr.h>
#include <avahi-common/address.h>
#include <avahi-common/defs.h>
#include <avahi-common/watch.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** Flags for server entries */
typedef enum {
    AVAHI_ENTRY_NULL = 0,          /**< No special flags */
    AVAHI_ENTRY_UNIQUE = 1,        /**< The RRset is intended to be unique */
    AVAHI_ENTRY_NOPROBE = 2,       /**< Though the RRset is intended to be unique no probes shall be sent */
    AVAHI_ENTRY_NOANNOUNCE = 4,    /**< Do not announce this RR to other hosts */
    AVAHI_ENTRY_ALLOWMUTIPLE = 8   /**< Allow multiple local records of this type, even if they are intended to be unique */
} AvahiEntryFlags;

/** Prototype for callback functions which are called whenever the state of an AvahiServer object changes */
typedef void (*AvahiServerCallback) (AvahiServer *s, AvahiServerState state, void* userdata);

/** Prototype for callback functions which are called whenever the state of an AvahiSEntryGroup object changes */
typedef void (*AvahiSEntryGroupCallback) (AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state, void* userdata);

/** Stores configuration options for a server instance */
typedef struct AvahiServerConfig {
    char *host_name;                      /**< Default host name. If left empty defaults to the result of gethostname(2) of the libc */
    char *domain_name;                    /**< Default domain name. If left empty defaults to .local */
    int use_ipv4;                     /**< Enable IPv4 support */
    int use_ipv6;                     /**< Enable IPv6 support */
    int publish_hinfo;                /**< Register a HINFO record for the host containing the local OS and CPU type */
    int publish_addresses;            /**< Register A, AAAA and PTR records for all local IP addresses */
    int publish_workstation;          /**< Register a _workstation._tcp service */
    int publish_domain;               /**< Announce the local domain for browsing */
    int check_response_ttl;           /**< If enabled the server ignores all incoming responses with IP TTL != 255. Newer versions of the RFC do no longer contain this check, so it is disabled by default. */
    int use_iff_running;              /**< Require IFF_RUNNING on local network interfaces. This is the official way to check for link beat. Unfortunately this doesn't work with all drivers. So bettere leave this off. */
    int enable_reflector;             /**< Reflect incoming mDNS traffic to all local networks. This allows mDNS based network browsing beyond ethernet borders */
    int reflect_ipv;                  /**< if enable_reflector is 1, enable/disable reflecting between IPv4 and IPv6 */
    int add_service_cookie;           /**< Add magic service cookie to all locally generated records implicitly */
} AvahiServerConfig;

/** Allocate a new mDNS responder object. */
AvahiServer *avahi_server_new(
    const AvahiPoll *api,          /**< The main loop adapter */
    const AvahiServerConfig *sc,   /**< If non-NULL a pointer to a configuration structure for the server. The server makes an internal deep copy of this structure, so you may free it using avahi_server_config_done() immediately after calling this function. */
    AvahiServerCallback callback,  /**< A callback which is called whenever the state of the server changes */
    void* userdata,                /**< An opaque pointer which is passed to the callback function */
    int *error);

/** Free an mDNS responder object */
void avahi_server_free(AvahiServer* s);

/** Fill in default values for a server configuration structure. If you
 * make use of an AvahiServerConfig structure be sure to initialize
 * it with this function for the sake of upwards library
 * compatibility. This call may allocate strings on the heap. To
 * release this memory make sure to call
 * avahi_server_config_done(). If you want to replace any strings in
 * the structure be sure to free the strings filled in by this
 * function with avahi_free() first and allocate the replacements with
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
const char* avahi_server_get_domain_name(AvahiServer *s);

/** Return the currently chosen host name. The return value points to a internally allocated string. */
const char* avahi_server_get_host_name(AvahiServer *s);

/** Return the currently chosen host name as a FQDN ("fully qualified
 * domain name", i.e. the concatenation of the host and domain
 * name). The return value points to a internally allocated string. */
const char* avahi_server_get_host_name_fqdn(AvahiServer *s);

/** Change the host name of a running mDNS responder. This will drop
all automicatilly generated RRs and readd them with the new
name. Since the responder has to probe for the new RRs this function
takes some time to take effect altough it returns immediately. This
function is intended to be called when a host name conflict is
reported using AvahiServerCallback. The caller should readd all user
defined RRs too since they otherwise continue to point to the outdated
host name..*/
int avahi_server_set_host_name(AvahiServer *s, const char *host_name);

/** Change the domain name of a running mDNS responder. The same rules
 * as with avahi_server_set_host_name() apply. */
int avahi_server_set_domain_name(AvahiServer *s, const char *domain_name);

/** Return the opaque user data pointer attached to a server object */
void* avahi_server_get_data(AvahiServer *s);

/** Change the opaque user data pointer attached to a server object */
void avahi_server_set_data(AvahiServer *s, void* userdata);

/** Return the current state of the server object */
AvahiServerState avahi_server_get_state(AvahiServer *s);

/** Iterate through all local entries of the server. (when g is NULL)
 * or of a specified entry group. At the first call state should point
 * to a NULL initialized void pointer, That pointer is used to track
 * the current iteration. It is not safe to call any other
 * avahi_server_xxx() function during the iteration. If the last entry
 * has been read, NULL is returned. */
const AvahiRecord *avahi_server_iterate(AvahiServer *s, AvahiSEntryGroup *g, void **state);

/** Callback prototype for avahi_server_dump() */
typedef void (*AvahiDumpCallback)(const char *text, void* userdata);

/** Dump the current server status by calling "callback" for each line.  */
int avahi_server_dump(AvahiServer *s, AvahiDumpCallback callback, void* userdata);

/** Create a new entry group. The specified callback function is
 * called whenever the state of the group changes. Use entry group
 * objects to keep track of you RRs. Add new RRs to a group using
 * avahi_server_add_xxx(). Make sure to call avahi_s_entry_group_commit()
 * to start the registration process for your RRs */
AvahiSEntryGroup *avahi_s_entry_group_new(AvahiServer *s, AvahiSEntryGroupCallback callback, void* userdata);

/** Free an entry group. All RRs assigned to the group are removed from the server */
void avahi_s_entry_group_free(AvahiSEntryGroup *g);

/** Commit an entry group. This starts the probing and registration process for all RRs in the group */
int avahi_s_entry_group_commit(AvahiSEntryGroup *g);

/** Remove all entries from the entry group and reset the state to AVAHI_ENTRY_GROUP_UNCOMMITED. */
void avahi_s_entry_group_reset(AvahiSEntryGroup *g);

/** Return 1 if the entry group is empty, i.e. has no records attached. */
int avahi_s_entry_group_is_empty(AvahiSEntryGroup *g);

/** Return the current state of the specified entry group */
AvahiEntryGroupState avahi_s_entry_group_get_state(AvahiSEntryGroup *g);

/** Change the opaque user data pointer attached to an entry group object */
void avahi_s_entry_group_set_data(AvahiSEntryGroup *g, void* userdata);

/** Return the opaque user data pointer currently set for the entry group object */
void* avahi_s_entry_group_get_data(AvahiSEntryGroup *g);

/** Add a new resource record to the server. Returns 0 on success, negative otherwise. */
int avahi_server_add(
    AvahiServer *s,           /**< The server object to add this record to */
    AvahiSEntryGroup *g,       /**< An entry group object if this new record shall be attached to one, or NULL. If you plan to remove the record sometime later you a required to pass an entry group object here. */
    AvahiIfIndex interface,   /**< A numeric index of a network interface to attach this record to, or AVAHI_IF_UNSPEC to attach this record to all interfaces */
    AvahiProtocol protocol,   /**< A protocol family to attach this record to. One of the AVAHI_PROTO_xxx constants. Use AVAHI_PROTO_UNSPEC to make this record available on all protocols (wich means on both IPv4 and IPv6). */
    AvahiEntryFlags flags,    /**< Special flags for this record */
    AvahiRecord *r            /**< The record to add. This function increases the reference counter of this object. */   );

/** Add a PTR RR to the server. See avahi_server_add() for more information. */
int avahi_server_add_ptr(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    uint32_t ttl,             /**< DNS TTL for this record */
    const char *name,       /**< PTR record name */
    const char *dest        /**< pointer destination */  );

/** Add a PTR RR to the server. See avahi_server_add() for more information. */
int avahi_server_add_txt(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    uint32_t ttl,             /**< DNS TTL for this record */
    const char *name,       /**< TXT record name */
    ... /**< Text record data, terminated by NULL */) AVAHI_GCC_SENTINEL;

/** Add a PTR RR to the server. Mostly identical to
 * avahi_server_add_text but takes a va_list instead of a variable
 * number of arguments */
int avahi_server_add_txt_va(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    uint32_t ttl,
    const char *name,
    va_list va);

/** Add a PTR RR to the server. Mostly identical to 
 * avahi_server_add_text but takes an AvahiStringList record instead of a variable
 * number of arguments. */
int avahi_server_add_txt_strlst(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    uint32_t ttl,
    const char *name,
    AvahiStringList *strlst  /**< TXT decord data as a AvahiString. This routine makes a deep copy of this object. */ );

/** Add an IP address mapping to the server. This will add both the
 * host-name-to-address and the reverse mapping to the server. See
 * avahi_server_add() for more information. If adding one of the RRs
 * fails, the function returns with an error, but it is not defined if
 * the other RR is deleted from the server or not. Therefore, you have
 * to free the AvahiSEntryGroup and create a new one before
 * proceeding. */
int avahi_server_add_address(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    const char *name,
    AvahiAddress *a);

/** Add an DNS-SD service to the Server. This will add all required
 * RRs to the server. See avahi_server_add() for more information.  If
 * adding one of the RRs fails, the function returns with an error,
 * but it is not defined if the other RR is deleted from the server or
 * not. Therefore, you have to free the AvahiSEntryGroup and create a
 * new one before proceeding. */
int avahi_server_add_service(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,         /**< Service name, e.g. "Lennart's Files" */
    const char *type,         /**< DNS-SD type, e.g. "_http._tcp" */
    const char *domain,       
    const char *host,         /**< Host name where this servcie resides, or NULL if on the local host */
    uint16_t port,              /**< Port number of the service */
    ...  /**< Text records, terminated by NULL */) AVAHI_GCC_SENTINEL;

/** Mostly identical to avahi_server_add_service(), but takes an va_list for the TXT records. */
int avahi_server_add_service_va(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    va_list va);

/** Mostly identical to avahi_server_add_service(), but takes an AvahiStringList object for the TXT records.  The AvahiStringList object is copied. */
int avahi_server_add_service_strlst(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    AvahiStringList *strlst);

/** The type of DNS server */
typedef enum {
    AVAHI_DNS_SERVER_RESOLVE,         /**< Unicast DNS servers for normal resolves (_domain._udp)*/
    AVAHI_DNS_SERVER_UPDATE           /**< Unicast DNS servers for updates (_dns-update._udp)*/
} AvahiDNSServerType;

/** Publish the specified unicast DNS server address via mDNS. You may
 * browse for records create this way wit
 * avahi_s_dns_server_browser_new(). */
int avahi_server_add_dns_server_address(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiDNSServerType type,
    const AvahiAddress *address,
    uint16_t port /** should be 53 */);

/** Similar to avahi_server_add_dns_server_address(), but specify a
host name instead of an address. The specified host name should be
resolvable via mDNS */
int avahi_server_add_dns_server_name(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiDNSServerType type,
    const char *name,
    uint16_t port /** should be 53 */);

/** A browsing object for arbitrary RRs */
typedef struct AvahiSRecordBrowser AvahiSRecordBrowser;

/** Callback prototype for AvahiSRecordBrowser events */
typedef void (*AvahiSRecordBrowserCallback)(
    AvahiSRecordBrowser *b,       /**< The AvahiSRecordBrowser object that is emitting this callback */
    AvahiIfIndex interface,      /**< Logical OS network interface number the record was found on */
    AvahiProtocol protocol,      /**< Protocol number the record was found. */
    AvahiBrowserEvent event,     /**< Browsing event, either AVAHI_BROWSER_NEW or AVAHI_BROWSER_REMOVE */
    AvahiRecord *record,         /**< The record that was found */
    void* userdata            /**< Arbitrary user data passed to avahi_s_record_browser_new() */ );

/** Create a new browsing object for arbitrary RRs */
AvahiSRecordBrowser *avahi_s_record_browser_new(
    AvahiServer *server,                  /**< The server object to which attach this query */
    AvahiIfIndex interface,               /**< Logical OS interface number where to look for the records, or AVAHI_IF_UNSPEC to look on interfaces */
    AvahiProtocol protocol,               /**< Protocol number to use when looking for the record, or AVAHI_PROTO_UNSPEC to look on all protocols */
    AvahiKey *key,                        /**< The search key */
    AvahiSRecordBrowserCallback callback,  /**< The callback to call on browsing events */
    void* userdata                     /**< Arbitrary use suppliable data which is passed to the callback */);

/** Free an AvahiSRecordBrowser object */
void avahi_s_record_browser_free(AvahiSRecordBrowser *b);

/** A host name to IP adddress resolver object */
typedef struct AvahiSHostNameResolver AvahiSHostNameResolver;

/** Callback prototype for AvahiSHostNameResolver events */
typedef void (*AvahiSHostNameResolverCallback)(
    AvahiSHostNameResolver *r,
    AvahiIfIndex interface,  
    AvahiProtocol protocol,
    AvahiResolverEvent event, /**< Resolving event */
    const char *host_name,   /**< Host name which should be resolved. May differ in case from the query */
    const AvahiAddress *a,    /**< The address, or NULL if the host name couldn't be resolved. */
    void* userdata);

/** Create an AvahiSHostNameResolver object for resolving a host name to an adddress. See AvahiSRecordBrowser for more info on the paramters. */
AvahiSHostNameResolver *avahi_s_host_name_resolver_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *host_name,    /**< The host name to look for */
    AvahiProtocol aprotocol,   /**< The address family of the desired address or AVAHI_PROTO_UNSPEC if doesn't matter. */
    AvahiSHostNameResolverCallback calback,
    void* userdata);

/** Free a AvahiSHostNameResolver object */
void avahi_s_host_name_resolver_free(AvahiSHostNameResolver *r);

/** An IP address to host name resolver object ("reverse lookup") */
typedef struct AvahiSAddressResolver AvahiSAddressResolver;

/** Callback prototype for AvahiSAddressResolver events */
typedef void (*AvahiSAddressResolverCallback)(
    AvahiSAddressResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const AvahiAddress *a,   
    const char *host_name,   /**< A host name for the specified address, if one was found, i.e. event == AVAHI_RESOLVER_FOUND */
    void* userdata);

/** Create an AvahiSAddressResolver object. See AvahiSRecordBrowser for more info on the paramters. */
AvahiSAddressResolver *avahi_s_address_resolver_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const AvahiAddress *address,
    AvahiSAddressResolverCallback calback,
    void* userdata);

/** Free an AvahiSAddressResolver object */
void avahi_s_address_resolver_free(AvahiSAddressResolver *r);

/** A local domain browsing object. May be used to enumerate domains used on the local LAN */
typedef struct AvahiSDomainBrowser AvahiSDomainBrowser;

/** Callback prototype for AvahiSDomainBrowser events */
typedef void (*AvahiSDomainBrowserCallback)(
    AvahiSDomainBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *domain,
    void* userdata);

/** Create a new AvahiSDomainBrowser object */
AvahiSDomainBrowser *avahi_s_domain_browser_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiDomainBrowserType type,
    AvahiSDomainBrowserCallback callback,
    void* userdata);

/** Free an AvahiSDomainBrowser object */
void avahi_s_domain_browser_free(AvahiSDomainBrowser *b);

/** A DNS-SD service type browsing object. May be used to enumerate the service types of all available services on the local LAN */
typedef struct AvahiSServiceTypeBrowser AvahiSServiceTypeBrowser;

/** Callback prototype for AvahiSServiceTypeBrowser events */
typedef void (*AvahiSServiceTypeBrowserCallback)(
    AvahiSServiceTypeBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *type,
    const char *domain,
    void* userdata);

/** Create a new AvahiSServiceTypeBrowser object. */
AvahiSServiceTypeBrowser *avahi_s_service_type_browser_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiSServiceTypeBrowserCallback callback,
    void* userdata);

/** Free an AvahiSServiceTypeBrowser object */
void avahi_s_service_type_browser_free(AvahiSServiceTypeBrowser *b);

/** A DNS-SD service browser. Use this to enumerate available services of a certain kind on the local LAN. Use AvahiSServiceResolver to get specific service data like address and port for a service. */
typedef struct AvahiSServiceBrowser AvahiSServiceBrowser;

/** Callback prototype for AvahiSServiceBrowser events */
typedef void (*AvahiSServiceBrowserCallback)(
    AvahiSServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name     /**< Service name, e.g. "Lennart's Files" */,
    const char *type     /**< DNS-SD type, e.g. "_http._tcp" */,
    const char *domain   /**< Domain of this service, e.g. "local" */,
    void* userdata);

/** Create a new AvahiSServiceBrowser object. */
AvahiSServiceBrowser *avahi_s_service_browser_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *service_type /** DNS-SD service type, e.g. "_http._tcp" */,
    const char *domain,
    AvahiSServiceBrowserCallback callback,
    void* userdata);

/** Free an AvahiSServiceBrowser object */
void avahi_s_service_browser_free(AvahiSServiceBrowser *b);

/** A DNS-SD service resolver.  Use this to retrieve addres, port and TXT data for a DNS-SD service */
typedef struct AvahiSServiceResolver AvahiSServiceResolver;

/** Callback prototype for AvahiSServiceResolver events */
typedef void (*AvahiSServiceResolverCallback)(
    AvahiSServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,  /**< Is AVAHI_RESOLVER_FOUND when the service was resolved successfully, and everytime it changes. Is AVAHI_RESOLVER_TIMOUT when the service failed to resolve or disappeared. */
    const char *name,       /**< Service name */
    const char *type,       /**< Service Type */
    const char *domain,
    const char *host_name,  /**< Host name of the service */
    const AvahiAddress *a,   /**< The resolved host name */
    uint16_t port,            /**< Service name */
    AvahiStringList *txt,    /**< TXT record data */
    void* userdata);

/** Create a new AvahiSServiceResolver object. The specified callback function will be called with the resolved service data. */
AvahiSServiceResolver *avahi_s_service_resolver_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    AvahiProtocol aprotocol,    /**< Address family of the desired service address. Use AVAHI_PROTO_UNSPEC if you don't care */
    AvahiSServiceResolverCallback calback,
    void* userdata);

/** Free an AvahiSServiceResolver object */
void avahi_s_service_resolver_free(AvahiSServiceResolver *r);

/** A domain service browser object. Use this to browse for
 * conventional unicast DNS servers which may be used to resolve
 * conventional domain names */
typedef struct AvahiSDNSServerBrowser AvahiSDNSServerBrowser;

/** Callback prototype for AvahiSDNSServerBrowser events */
typedef void (*AvahiSDNSServerBrowserCallback)(
    AvahiSDNSServerBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *host_name,       /**< Host name of the DNS server, probably useless */
    const AvahiAddress *a,        /**< Address of the DNS server */
    uint16_t port,                 /**< Port number of the DNS servers, probably 53 */
    void* userdata);

/** Create a new AvahiSDNSServerBrowser object */
AvahiSDNSServerBrowser *avahi_s_dns_server_browser_new(
    AvahiServer *server,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiDNSServerType type,
    AvahiProtocol aprotocol,  /**< Address protocol for the DNS server */ 
    AvahiSDNSServerBrowserCallback callback,
    void* userdata);

/** Free an AvahiSDNSServerBrowser object */
void avahi_s_dns_server_browser_free(AvahiSDNSServerBrowser *b);

/** Return the last error code */
int avahi_server_errno(AvahiServer *s);

/** Return the local service cookie */
uint32_t avahi_server_get_local_service_cookie(AvahiServer *s);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif
