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

#include <stdio.h>
#include <glib.h>

/** An mDNS responder object */
typedef struct AvahiServer AvahiServer;

/** A locally registered DNS resource record */
typedef struct AvahiEntry AvahiEntry;

/** A group of locally registered DNS RRs */
typedef struct AvahiEntryGroup AvahiEntryGroup;

#include <avahi-core/address.h>
#include <avahi-core/rr.h>

/** States of a server object */
typedef enum {
    AVAHI_SERVER_INVALID = -1,     /**< Invalid state (initial) */ 
    AVAHI_SERVER_REGISTERING = 0,  /**< Host RRs are being registered */
    AVAHI_SERVER_RUNNING,          /**< All host RRs have been established */
    AVAHI_SERVER_COLLISION,        /**< There is a collision with a host RR. All host RRs have been withdrawn, the user should set a new host name via avahi_server_set_host_name() */
    AVAHI_SERVER_SLEEPING          /**< The host or domain name has changed and the server waits for old entries to be expired */
} AvahiServerState;

/** Flags for server entries */
typedef enum {
    AVAHI_ENTRY_NULL = 0,          /**< No special flags */
    AVAHI_ENTRY_UNIQUE = 1,        /**< The RRset is intended to be unique */
    AVAHI_ENTRY_NOPROBE = 2,       /**< Though the RRset is intended to be unique no probes shall be sent */
    AVAHI_ENTRY_NOANNOUNCE = 4     /**< Do not announce this RR to other hosts */
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
    gboolean register_hinfo;               /**< Register a HINFO record for the host containing the local OS and CPU type */
    gboolean register_addresses;           /**< Register A, AAAA and PTR records for all local IP addresses */
    gboolean register_workstation;         /**< Register a _workstation._tcp service */
    gboolean check_response_ttl;           /**< If enabled the server ignores all incoming responses with IP TTL != 255 */
    gboolean announce_domain;              /**< Announce the local domain for browsing */
    gboolean use_iff_running;              /**< Require IFF_RUNNING on local network interfaces. This is the official way to check for link beat. Unfortunately this doesn't work with all drivers. So bettere leave this off. */
    gboolean enable_reflector;             /**< Reflect incoming mDNS traffic to all local networks. This allows mDNS based network browsing beyond ethernet borders */
    gboolean ipv_reflect;                  /**< if enable_reflector is TRUE, enable/disable reflecting between IPv4 and IPv6 */
} AvahiServerConfig;

/** Allocate a new mDNS responder object. */
AvahiServer *avahi_server_new(
    GMainContext *c,               /**< The GLIB main loop context to attach to */
    const AvahiServerConfig *sc,   /**< If non-NULL a pointer to a configuration structure for the server. The server makes an internal deep copy of this structure, so you may free it using avahi_server_config_done() immediately after calling this function. */
    AvahiServerCallback callback,  /**< A callback which is called whenever the state of the server changes */
    gpointer userdata              /**< An opaque pointer which is passed to the callback function */);

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

void avahi_server_set_host_name(AvahiServer *s, const gchar *host_name);
void avahi_server_set_domain_name(AvahiServer *s, const gchar *domain_name);

gpointer avahi_server_get_data(AvahiServer *s);
void avahi_server_set_data(AvahiServer *s, gpointer userdata);

AvahiServerState avahi_server_get_state(AvahiServer *s);

const AvahiRecord *avahi_server_iterate(AvahiServer *s, AvahiEntryGroup *g, void **state);
void avahi_server_dump(AvahiServer *s, FILE *f);

AvahiEntryGroup *avahi_entry_group_new(AvahiServer *s, AvahiEntryGroupCallback callback, gpointer userdata);
void avahi_entry_group_free(AvahiEntryGroup *g);
void avahi_entry_group_commit(AvahiEntryGroup *g);
AvahiEntryGroupState avahi_entry_group_get_state(AvahiEntryGroup *g);
void avahi_entry_group_set_data(AvahiEntryGroup *g, gpointer userdata);
gpointer avahi_entry_group_get_data(AvahiEntryGroup *g);

void avahi_server_add(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    AvahiRecord *r);

void avahi_server_add_ptr(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    const gchar *dest);

void avahi_server_add_address(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    AvahiAddress *a);

void avahi_server_add_text(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    ... /* text records, terminated by NULL */);

void avahi_server_add_text_va(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    va_list va);

void avahi_server_add_text_strlst(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    AvahiStringList *strlst);

void avahi_server_add_service(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    ...  /* text records, terminated by NULL */);

void avahi_server_add_service_va(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    va_list va);

void avahi_server_add_service_strlst(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    AvahiStringList *strlst);

typedef enum {
    AVAHI_BROWSER_NEW = 0,
    AVAHI_BROWSER_REMOVE = -1
} AvahiBrowserEvent;

typedef enum {
    AVAHI_RESOLVER_FOUND = 0,
    AVAHI_RESOLVER_TIMEOUT = -1
} AvahiResolverEvent;


typedef struct AvahiRecordBrowser AvahiRecordBrowser;
typedef void (*AvahiRecordBrowserCallback)(AvahiRecordBrowser *b, gint interface, guchar protocol, AvahiBrowserEvent event, AvahiRecord *record, gpointer userdata);
AvahiRecordBrowser *avahi_record_browser_new(AvahiServer *server, gint interface, guchar protocol, AvahiKey *key, AvahiRecordBrowserCallback callback, gpointer userdata);
void avahi_record_browser_free(AvahiRecordBrowser *b);

typedef struct AvahiHostNameResolver AvahiHostNameResolver;
typedef void (*AvahiHostNameResolverCallback)(AvahiHostNameResolver *r, gint interface, guchar protocol, AvahiResolverEvent event, const gchar *host_name, const AvahiAddress *a, gpointer userdata);
AvahiHostNameResolver *avahi_host_name_resolver_new(AvahiServer *server, gint interface, guchar protocol, const gchar *host_name, guchar aprotocol, AvahiHostNameResolverCallback calback, gpointer userdata);
void avahi_host_name_resolver_free(AvahiHostNameResolver *r);

typedef struct AvahiAddressResolver AvahiAddressResolver;
typedef void (*AvahiAddressResolverCallback)(AvahiAddressResolver *r, gint interface, guchar protocol, AvahiResolverEvent event, const AvahiAddress *a, const gchar *host_name, gpointer userdata);
AvahiAddressResolver *avahi_address_resolver_new(AvahiServer *server, gint interface, guchar protocol, const AvahiAddress *address, AvahiAddressResolverCallback calback, gpointer userdata);
void avahi_address_resolver_free(AvahiAddressResolver *r);

typedef enum {
    AVAHI_DOMAIN_BROWSER_REGISTER,
    AVAHI_DOMAIN_BROWSER_REGISTER_DEFAULT,
    AVAHI_DOMAIN_BROWSER_BROWSE,
    AVAHI_DOMAIN_BROWSER_BROWSE_DEFAULT
} AvahiDomainBrowserType;

typedef struct AvahiDomainBrowser AvahiDomainBrowser;
typedef void (*AvahiDomainBrowserCallback)(AvahiDomainBrowser *b, gint interface, guchar protocol, AvahiBrowserEvent event, const gchar *domain, gpointer userdata);
AvahiDomainBrowser *avahi_domain_browser_new(AvahiServer *server, gint interface, guchar protocol, const gchar *domain, AvahiDomainBrowserType type, AvahiDomainBrowserCallback callback, gpointer userdata);
void avahi_domain_browser_free(AvahiDomainBrowser *b);

typedef struct AvahiServiceTypeBrowser AvahiServiceTypeBrowser;
typedef void (*AvahiServiceTypeBrowserCallback)(AvahiServiceTypeBrowser *b, gint interface, guchar protocol, AvahiBrowserEvent event, const gchar *type, const gchar *domain, gpointer userdata);
AvahiServiceTypeBrowser *avahi_service_type_browser_new(AvahiServer *server, gint interface, guchar protocol, const gchar *domain, AvahiServiceTypeBrowserCallback callback, gpointer userdata);
void avahi_service_type_browser_free(AvahiServiceTypeBrowser *b);

typedef struct AvahiServiceBrowser AvahiServiceBrowser;
typedef void (*AvahiServiceBrowserCallback)(AvahiServiceBrowser *b, gint interface, guchar protocol, AvahiBrowserEvent event, const gchar *name, const gchar *type, const gchar *domain, gpointer userdata);
AvahiServiceBrowser *avahi_service_browser_new(AvahiServer *server, gint interface, guchar protocol, const gchar *service_type, const gchar *domain, AvahiServiceBrowserCallback callback, gpointer userdata);
void avahi_service_browser_free(AvahiServiceBrowser *b);

typedef struct AvahiServiceResolver AvahiServiceResolver;
typedef void (*AvahiServiceResolverCallback)(AvahiServiceResolver *r, gint interface, guchar protocol, AvahiResolverEvent event, const gchar *name, const gchar *type, const gchar *domain, const gchar *host_name, const AvahiAddress *a, guint16 port, AvahiStringList *txt, gpointer userdata);
AvahiServiceResolver *avahi_service_resolver_new(AvahiServer *server, gint interface, guchar protocol, const gchar *name, const gchar *type, const gchar *domain, guchar aprotocol, AvahiServiceResolverCallback calback, gpointer userdata);
void avahi_service_resolver_free(AvahiServiceResolver *r);

#endif
