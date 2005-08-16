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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>

#include <dbus/dbus.h>

#include <avahi-common/llist.h>
#include <avahi-common/malloc.h>
#include <avahi-common/dbus.h>
#include <avahi-common/dbus-watch-glue.h>
#include <avahi-core/log.h>
#include <avahi-core/core.h>

#include "dbus-protocol.h"
#include "main.h"

typedef struct Server Server;
typedef struct Client Client;
typedef struct EntryGroupInfo EntryGroupInfo;
typedef struct HostNameResolverInfo HostNameResolverInfo;
typedef struct AddressResolverInfo AddressResolverInfo;
typedef struct DomainBrowserInfo DomainBrowserInfo;
typedef struct ServiceTypeBrowserInfo ServiceTypeBrowserInfo;
typedef struct ServiceBrowserInfo ServiceBrowserInfo;
typedef struct ServiceResolverInfo ServiceResolverInfo;

#define MAX_CLIENTS 20
#define MAX_OBJECTS_PER_CLIENT 50
#define MAX_ENTRIES_PER_ENTRY_GROUP 20

/* #define VALGRIND_WORKAROUND */

struct EntryGroupInfo {
    unsigned id;
    Client *client;
    AvahiSEntryGroup *entry_group;
    char *path;

    int n_entries;
    
    AVAHI_LLIST_FIELDS(EntryGroupInfo, entry_groups);
};

struct HostNameResolverInfo {
    Client *client;
    AvahiSHostNameResolver *host_name_resolver;
    DBusMessage *message;

    AVAHI_LLIST_FIELDS(HostNameResolverInfo, host_name_resolvers);
};

struct AddressResolverInfo {
    Client *client;
    AvahiSAddressResolver *address_resolver;
    DBusMessage *message;

    AVAHI_LLIST_FIELDS(AddressResolverInfo, address_resolvers);
};

struct DomainBrowserInfo {
    unsigned id;
    Client *client;
    AvahiSDomainBrowser *domain_browser;
    char *path;

    AVAHI_LLIST_FIELDS(DomainBrowserInfo, domain_browsers);
};

struct ServiceTypeBrowserInfo {
    unsigned id;
    Client *client;
    AvahiSServiceTypeBrowser *service_type_browser;
    char *path;

    AVAHI_LLIST_FIELDS(ServiceTypeBrowserInfo, service_type_browsers);
};

struct ServiceBrowserInfo {
    unsigned id;
    Client *client;
    AvahiSServiceBrowser *service_browser;
    char *path;

    AVAHI_LLIST_FIELDS(ServiceBrowserInfo, service_browsers);
};

struct ServiceResolverInfo {
    Client *client;
    AvahiSServiceResolver *service_resolver;
    DBusMessage *message;

    AVAHI_LLIST_FIELDS(ServiceResolverInfo, service_resolvers);
};

struct Client {
    unsigned id;
    char *name;
    unsigned current_id;
    int n_objects;
    
    AVAHI_LLIST_FIELDS(Client, clients);
    AVAHI_LLIST_HEAD(EntryGroupInfo, entry_groups);
    AVAHI_LLIST_HEAD(HostNameResolverInfo, host_name_resolvers);
    AVAHI_LLIST_HEAD(AddressResolverInfo, address_resolvers);
    AVAHI_LLIST_HEAD(DomainBrowserInfo, domain_browsers);
    AVAHI_LLIST_HEAD(ServiceTypeBrowserInfo, service_type_browsers);
    AVAHI_LLIST_HEAD(ServiceBrowserInfo, service_browsers);
    AVAHI_LLIST_HEAD(ServiceResolverInfo, service_resolvers);
};

struct Server {
    DBusConnection *bus;
    AVAHI_LLIST_HEAD(Client, clients);
    int n_clients;
    unsigned current_id;
};

static Server *server = NULL;

static void entry_group_free(EntryGroupInfo *i) {
    assert(i);

    if (i->entry_group)
        avahi_s_entry_group_free(i->entry_group);
    dbus_connection_unregister_object_path(server->bus, i->path);
    avahi_free(i->path);
    AVAHI_LLIST_REMOVE(EntryGroupInfo, entry_groups, i->client->entry_groups, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);
    
    avahi_free(i);
 }

static void host_name_resolver_free(HostNameResolverInfo *i) {
    assert(i);

    if (i->host_name_resolver)
        avahi_s_host_name_resolver_free(i->host_name_resolver);
    dbus_message_unref(i->message);
    AVAHI_LLIST_REMOVE(HostNameResolverInfo, host_name_resolvers, i->client->host_name_resolvers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void address_resolver_free(AddressResolverInfo *i) {
    assert(i);

    if (i->address_resolver)
        avahi_s_address_resolver_free(i->address_resolver);
    dbus_message_unref(i->message);
    AVAHI_LLIST_REMOVE(AddressResolverInfo, address_resolvers, i->client->address_resolvers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void domain_browser_free(DomainBrowserInfo *i) {
    assert(i);

    if (i->domain_browser)
        avahi_s_domain_browser_free(i->domain_browser);
    dbus_connection_unregister_object_path(server->bus, i->path);
    avahi_free(i->path);
    AVAHI_LLIST_REMOVE(DomainBrowserInfo, domain_browsers, i->client->domain_browsers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void service_type_browser_free(ServiceTypeBrowserInfo *i) {
    assert(i);

    if (i->service_type_browser)
        avahi_s_service_type_browser_free(i->service_type_browser);
    dbus_connection_unregister_object_path(server->bus, i->path);
    avahi_free(i->path);
    AVAHI_LLIST_REMOVE(ServiceTypeBrowserInfo, service_type_browsers, i->client->service_type_browsers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void service_browser_free(ServiceBrowserInfo *i) {
    assert(i);

    if (i->service_browser)
        avahi_s_service_browser_free(i->service_browser);
    dbus_connection_unregister_object_path(server->bus, i->path);
    avahi_free(i->path);
    AVAHI_LLIST_REMOVE(ServiceBrowserInfo, service_browsers, i->client->service_browsers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void service_resolver_free(ServiceResolverInfo *i) {
    assert(i);

    if (i->service_resolver)
        avahi_s_service_resolver_free(i->service_resolver);
    dbus_message_unref(i->message);
    AVAHI_LLIST_REMOVE(ServiceResolverInfo, service_resolvers, i->client->service_resolvers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void client_free(Client *c) {
    
    assert(server);
    assert(c);

    while (c->entry_groups)
        entry_group_free(c->entry_groups);

    while (c->host_name_resolvers)
        host_name_resolver_free(c->host_name_resolvers);

    while (c->address_resolvers)
        address_resolver_free(c->address_resolvers);

    while (c->domain_browsers)
        domain_browser_free(c->domain_browsers);

    while (c->service_type_browsers)
        service_type_browser_free(c->service_type_browsers);

    while (c->service_browsers)
        service_browser_free(c->service_browsers);

    while (c->service_resolvers)
        service_resolver_free(c->service_resolvers);

    assert(c->n_objects == 0);
    
    avahi_free(c->name);
    AVAHI_LLIST_REMOVE(Client, clients, server->clients, c);
    avahi_free(c);

    server->n_clients --;
    assert(server->n_clients >= 0);
}

static Client *client_get(const char *name, int create) {
    Client *client;

    assert(server);
    assert(name);

    for (client = server->clients; client; client = client->clients_next)
        if (!strcmp(name, client->name))
            return client;

    if (!create)
        return NULL;

    if (server->n_clients >= MAX_CLIENTS)
        return NULL;
    
    /* If not existant yet, create a new entry */
    client = avahi_new(Client, 1);
    client->id = server->current_id++;
    client->name = avahi_strdup(name);
    client->current_id = 0;
    client->n_objects = 0;
    AVAHI_LLIST_HEAD_INIT(EntryGroupInfo, client->entry_groups);
    AVAHI_LLIST_HEAD_INIT(HostNameResolverInfo, client->host_name_resolvers);
    AVAHI_LLIST_HEAD_INIT(AddressResolverInfo, client->address_resolvers);
    AVAHI_LLIST_HEAD_INIT(DomainBrowserInfo, client->domain_browsers);
    AVAHI_LLIST_HEAD_INIT(ServiceTypeBrowserInfo, client->service_type_browsers);
    AVAHI_LLIST_HEAD_INIT(ServiceBrowserInfo, client->service_browsers);
    AVAHI_LLIST_HEAD_INIT(ServiceResolverInfo, client->service_resolvers);

    AVAHI_LLIST_PREPEND(Client, clients, server->clients, client);

    server->n_clients++;
    assert(server->n_clients > 0);
    
    return client;
}

static DBusHandlerResult respond_error(DBusConnection *c, DBusMessage *m, int error, const char *text) {
    DBusMessage *reply;

    const char * const table[- AVAHI_ERR_MAX] = {
        NULL, /* OK */
        AVAHI_DBUS_ERR_FAILURE,
        AVAHI_DBUS_ERR_BAD_STATE,
        AVAHI_DBUS_ERR_INVALID_HOST_NAME,
        AVAHI_DBUS_ERR_INVALID_DOMAIN_NAME,
        AVAHI_DBUS_ERR_NO_NETWORK,
        AVAHI_DBUS_ERR_INVALID_TTL,
        AVAHI_DBUS_ERR_IS_PATTERN,
        AVAHI_DBUS_ERR_LOCAL_COLLISION,
        AVAHI_DBUS_ERR_INVALID_RECORD,
        AVAHI_DBUS_ERR_INVALID_SERVICE_NAME,
        AVAHI_DBUS_ERR_INVALID_SERVICE_TYPE,
        AVAHI_DBUS_ERR_INVALID_PORT,
        AVAHI_DBUS_ERR_INVALID_KEY,
        AVAHI_DBUS_ERR_INVALID_ADDRESS,
        AVAHI_DBUS_ERR_TIMEOUT,
        AVAHI_DBUS_ERR_TOO_MANY_CLIENTS,
        AVAHI_DBUS_ERR_TOO_MANY_OBJECTS,
        AVAHI_DBUS_ERR_TOO_MANY_ENTRIES,
        AVAHI_DBUS_ERR_OS,
        AVAHI_DBUS_ERR_ACCESS_DENIED,
        AVAHI_DBUS_ERR_INVALID_OPERATION,
        AVAHI_DBUS_ERR_DBUS_ERROR,
        AVAHI_DBUS_ERR_NOT_CONNECTED,
        AVAHI_DBUS_ERR_NO_MEMORY,
        AVAHI_DBUS_ERR_INVALID_OBJECT,
        AVAHI_DBUS_ERR_NO_DAEMON
    };

    assert(-error > -AVAHI_OK);
    assert(-error < -AVAHI_ERR_MAX);
    
    reply = dbus_message_new_error(m, table[-error], text ? text : avahi_strerror(error));
    dbus_connection_send(c, reply, NULL);
    dbus_message_unref(reply);
    
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult respond_string(DBusConnection *c, DBusMessage *m, const char *text) {
    DBusMessage *reply;

    reply = dbus_message_new_method_return(m);
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &text, DBUS_TYPE_INVALID);
    dbus_connection_send(c, reply, NULL);
    dbus_message_unref(reply);
    
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult respond_int32(DBusConnection *c, DBusMessage *m, int32_t i) {
    DBusMessage *reply;

    reply = dbus_message_new_method_return(m);
    dbus_message_append_args(reply, DBUS_TYPE_INT32, &i, DBUS_TYPE_INVALID);
    dbus_connection_send(c, reply, NULL);
    dbus_message_unref(reply);
    
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult respond_ok(DBusConnection *c, DBusMessage *m) {
    DBusMessage *reply;

    reply = dbus_message_new_method_return(m);
    dbus_connection_send(c, reply, NULL);
    dbus_message_unref(reply);
    
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult respond_path(DBusConnection *c, DBusMessage *m, const char *path) {
    DBusMessage *reply;

    reply = dbus_message_new_method_return(m);
    dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID);
    dbus_connection_send(c, reply, NULL);
    dbus_message_unref(reply);
    
    return DBUS_HANDLER_RESULT_HANDLED;
}

static char *file_get_contents(char *fname) {
    int fd = -1;
    struct stat st;
    ssize_t size;
    char *buf = NULL;
    
    assert(fname);

    if (!(fd = open(fname, O_RDONLY))) {
        avahi_log_error("Failed to open %s: %s", fname, strerror(errno));
        goto fail;
    }

    if (fstat(fd, &st) < 0) {
        avahi_log_error("stat(%s) failed: %s", fname, strerror(errno));
        goto fail;
    }

    if (!(S_ISREG(st.st_mode))) {
        avahi_log_error("Invalid file %s", fname);
        goto fail;
    }

    if (st.st_size > 1024*1024) { /** 1MB */
        avahi_log_error("File too large %s", fname);
        goto fail;
    }

    buf = avahi_new(char, st.st_size+1);

    if ((size = read(fd, buf, st.st_size)) < 0) {
        avahi_log_error("read() failed: %s\n", strerror(errno));
        goto fail;
    }

    buf[size] = 0;

    close(fd);
    return buf;
    
fail:
    if (fd >= 0)
        close(fd);

    if (buf)
        avahi_free(buf);

    return NULL;
        
}

static DBusHandlerResult handle_introspect(DBusConnection *c, DBusMessage *m, const char *fname) {
    char *path, *contents;
    DBusError error;
    
    assert(c);
    assert(m);
    assert(fname);

    dbus_error_init(&error);

    if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
        avahi_log_error("Error parsing Introspect message: %s", error.message);
        goto fail;
    }
    
    path = avahi_strdup_printf("%s/%s", AVAHI_DBUS_INTROSPECTION_DIR, fname);
    contents = file_get_contents(path);
    avahi_free(path);
    
    if (!contents) {
        avahi_log_error("Failed to load introspection data.");
        goto fail;
    }
    
    respond_string(c, m, contents);
    avahi_free(contents);
    
    return DBUS_HANDLER_RESULT_HANDLED;

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

}

static DBusHandlerResult msg_signal_filter_impl(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError error;

    dbus_error_init(&error);

/*     avahi_log_debug("dbus: interface=%s, path=%s, member=%s", */
/*                     dbus_message_get_interface(m), */
/*                     dbus_message_get_path(m), */
/*                     dbus_message_get_member(m)); */

    if (dbus_message_is_signal(m, DBUS_INTERFACE_LOCAL, "Disconnected")) {
        /* No, we shouldn't quit, but until we get somewhere
         * usefull such that we can restore our state, we will */
        avahi_log_warn("Disconnnected from d-bus, terminating...");
        
        raise(SIGQUIT); /* The signal handler will catch this and terminate the process cleanly*/
        
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_signal(m, DBUS_INTERFACE_DBUS, "NameAcquired")) {
        char *name;

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing NameAcquired message");
            goto fail;
        }

/*         avahi_log_info("dbus: name acquired (%s)", name); */
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_signal(m, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
        char *name, *old, *new;

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &old, DBUS_TYPE_STRING, &new, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing NameOwnerChanged message");
            goto fail;
        }

        if (!*new) {
            Client *client;

            if ((client = client_get(name, FALSE))) {
/*                 avahi_log_info("dbus: client %s vanished", name); */
                client_free(client);
            }
        }
    }

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state, void* userdata) {
    EntryGroupInfo *i = userdata;
    DBusMessage *m;
    int32_t t;
    
    assert(s);
    assert(g);
    assert(i);

    m = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "StateChanged");
    t = (int32_t) state;
    dbus_message_append_args(m, DBUS_TYPE_INT32, &t, DBUS_TYPE_INVALID);
    dbus_message_set_destination(m, i->client->name);  
    dbus_connection_send(server->bus, m, NULL);
    dbus_message_unref(m);
}

static DBusHandlerResult msg_entry_group_impl(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError error;
    EntryGroupInfo *i = userdata;

    assert(c);
    assert(m);
    assert(i);
    
    dbus_error_init(&error);

    avahi_log_debug("dbus: interface=%s, path=%s, member=%s",
                    dbus_message_get_interface(m),
                    dbus_message_get_path(m),
                    dbus_message_get_member(m));

    /* Introspection */
    if (dbus_message_is_method_call(m, DBUS_INTERFACE_INTROSPECTABLE, "Introspect"))
        return handle_introspect(c, m, "EntryGroup.introspect");
    
    /* Access control */
    if (strcmp(dbus_message_get_sender(m), i->client->name)) 
        return respond_error(c, m, AVAHI_ERR_ACCESS_DENIED, NULL);
    
    if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "Free")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing EntryGroup::Free message");
            goto fail;
        }

        entry_group_free(i);
        return respond_ok(c, m);
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "Commit")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing EntryGroup::Commit message");
            goto fail;
        }

        avahi_s_entry_group_commit(i->entry_group);
        return respond_ok(c, m);
        
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "Reset")) {
        
        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing EntryGroup::Reset message");
            goto fail;
        }

        avahi_s_entry_group_reset(i->entry_group);
        return respond_ok(c, m);
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "IsEmpty")) {
        DBusMessage *reply;
        int b;
        
        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing EntryGroup::IsEmpty message");
            goto fail;
        }

        b = !!avahi_s_entry_group_is_empty(i->entry_group);

        reply = dbus_message_new_method_return(m);
        dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &b, DBUS_TYPE_INVALID);
        dbus_connection_send(c, reply, NULL);
        dbus_message_unref(reply);
        
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "GetState")) {
        AvahiEntryGroupState state;
        
        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing EntryGroup::GetState message");
            goto fail;
        }

        state = avahi_s_entry_group_get_state(i->entry_group);
        return respond_int32(c, m, (int32_t) state);
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "AddService")) {
        int32_t interface, protocol;
        char *type, *name, *domain, *host;
        uint16_t port;
        AvahiStringList *strlst;
        DBusMessageIter iter, sub;
        int j;
        
        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_STRING, &type,
                DBUS_TYPE_STRING, &domain,
                DBUS_TYPE_STRING, &host,
                DBUS_TYPE_UINT16, &port, 
                DBUS_TYPE_INVALID) || !type || !name) {
            avahi_log_warn("Error parsing EntryGroup::AddService message");
            goto fail;
        }

        dbus_message_iter_init(m, &iter);

        for (j = 0; j < 7; j++)
            dbus_message_iter_next(&iter);
        
        if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY ||
            dbus_message_iter_get_element_type(&iter) != DBUS_TYPE_ARRAY) {
            avahi_log_warn("Error parsing EntryGroup::AddService message 2");
            goto fail;
        }

        strlst = NULL;
        dbus_message_iter_recurse(&iter, &sub);
        
        for (;;) {
            DBusMessageIter sub2;
            int at, n;
            uint8_t *k;

            if ((at = dbus_message_iter_get_arg_type(&sub)) == DBUS_TYPE_INVALID)
                break;

            assert(at == DBUS_TYPE_ARRAY);
            
            if (dbus_message_iter_get_element_type(&sub) != DBUS_TYPE_BYTE) {
                avahi_log_warn("Error parsing EntryGroup::AddService message");
                goto fail;
            }

            dbus_message_iter_recurse(&sub, &sub2);
            dbus_message_iter_get_fixed_array(&sub2, &k, &n);
            strlst = avahi_string_list_add_arbitrary(strlst, k, n);
            
            dbus_message_iter_next(&sub);
        }

        if (i->n_entries >= MAX_ENTRIES_PER_ENTRY_GROUP) {
            avahi_string_list_free(strlst);
            avahi_log_warn("Too many entries per entry group, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_ENTRIES, NULL);
        }

        if (domain && !*domain)
            domain = NULL;

        if (host && !*host)
            host = NULL;

        if (avahi_server_add_service_strlst(avahi_server, i->entry_group, (AvahiIfIndex) interface, (AvahiProtocol) protocol, name, type, domain, host, port, strlst) < 0) {
            avahi_string_list_free(strlst);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        } else
            i->n_entries ++;
        
        avahi_string_list_free(strlst);
        
        return respond_ok(c, m);
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "AddAddress")) {
        int32_t interface, protocol;
        char *name, *address;
        AvahiAddress a;
        
        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_STRING, &address,
                DBUS_TYPE_INVALID) || !name || !address) {
            avahi_log_warn("Error parsing EntryGroup::AddAddress message");
            goto fail;
        }

        if (i->n_entries >= MAX_ENTRIES_PER_ENTRY_GROUP) {
            avahi_log_warn("Too many entries per entry group, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_ENTRIES, NULL);
        }
        
        if (!(avahi_address_parse(address, AVAHI_PROTO_UNSPEC, &a))) {
            return respond_error(c, m, AVAHI_ERR_INVALID_ADDRESS, NULL);
        }

        if (avahi_server_add_address(avahi_server, i->entry_group, (AvahiIfIndex) interface, (AvahiProtocol) protocol, 0, name, &a) < 0)
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        else
            i->n_entries ++;
        
        return respond_ok(c, m);
    }
    
    avahi_log_warn("Missed message %s::%s()", dbus_message_get_interface(m), dbus_message_get_member(m));

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void host_name_resolver_callback(AvahiSHostNameResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *host_name, const AvahiAddress *a, void* userdata) {
    HostNameResolverInfo *i = userdata;
    
    assert(r);
    assert(host_name);
    assert(i);

    if (event == AVAHI_RESOLVER_FOUND) {
        char t[256], *pt = t;
        int32_t i_interface, i_protocol, i_aprotocol;
        DBusMessage *reply;

        assert(a);
        avahi_address_snprint(t, sizeof(t), a);

        i_interface = (int32_t) interface;
        i_protocol = (int32_t) protocol;
        i_aprotocol = (int32_t) a->family;
        
        reply = dbus_message_new_method_return(i->message);
        dbus_message_append_args(
            reply,
            DBUS_TYPE_INT32, &i_interface,
            DBUS_TYPE_INT32, &i_protocol,
            DBUS_TYPE_STRING, &host_name,
            DBUS_TYPE_INT32, &i_aprotocol,
            DBUS_TYPE_STRING, &pt,
            DBUS_TYPE_INVALID);

        dbus_connection_send(server->bus, reply, NULL);
        dbus_message_unref(reply);
    } else {
        assert(event == AVAHI_RESOLVER_TIMEOUT);

        respond_error(server->bus, i->message, AVAHI_ERR_TIMEOUT, NULL);
    }

    host_name_resolver_free(i);
}

static void address_resolver_callback(AvahiSAddressResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const AvahiAddress *address, const char *host_name, void* userdata) {
    AddressResolverInfo *i = userdata;
    
    assert(r);
    assert(address);
    assert(i);

    if (event == AVAHI_RESOLVER_FOUND) {
        char t[256], *pt = t;
        int32_t i_interface, i_protocol, i_aprotocol;
        DBusMessage *reply;

        assert(host_name);
        avahi_address_snprint(t, sizeof(t), address);

        i_interface = (int32_t) interface;
        i_protocol = (int32_t) protocol;
        i_aprotocol = (int32_t) address->family;
        
        reply = dbus_message_new_method_return(i->message);
        dbus_message_append_args(
            reply,
            DBUS_TYPE_INT32, &i_interface,
            DBUS_TYPE_INT32, &i_protocol,
            DBUS_TYPE_INT32, &i_aprotocol,
            DBUS_TYPE_STRING, &pt,
            DBUS_TYPE_STRING, &host_name,
            DBUS_TYPE_INVALID);

        dbus_connection_send(server->bus, reply, NULL);
        dbus_message_unref(reply);
    } else {
        assert(event == AVAHI_RESOLVER_TIMEOUT);
        respond_error(server->bus, i->message, AVAHI_ERR_TIMEOUT, NULL);
    }

    address_resolver_free(i);
}

static DBusHandlerResult msg_domain_browser_impl(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError error;
    DomainBrowserInfo *i = userdata;

    assert(c);
    assert(m);
    assert(i);
    
    dbus_error_init(&error);

    avahi_log_debug("dbus: interface=%s, path=%s, member=%s",
                    dbus_message_get_interface(m),
                    dbus_message_get_path(m),
                    dbus_message_get_member(m));

    /* Introspection */
    if (dbus_message_is_method_call(m, DBUS_INTERFACE_INTROSPECTABLE, "Introspect"))
        return handle_introspect(c, m, "DomainBrowser.introspect");
    
    /* Access control */
    if (strcmp(dbus_message_get_sender(m), i->client->name)) 
        return respond_error(c, m, AVAHI_ERR_ACCESS_DENIED, NULL);
    
    if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_DOMAIN_BROWSER, "Free")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing DomainBrowser::Free message");
            goto fail;
        }

        domain_browser_free(i);
        return respond_ok(c, m);
        
    }
    
    avahi_log_warn("Missed message %s::%s()", dbus_message_get_interface(m), dbus_message_get_member(m));

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void domain_browser_callback(AvahiSDomainBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *domain, void* userdata) {
    DomainBrowserInfo *i = userdata;
    DBusMessage *m;
    int32_t i_interface, i_protocol;
    
    assert(b);
    assert(domain);
    assert(i);

    i_interface = (int32_t) interface;
    i_protocol = (int32_t) protocol;

    m = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_DOMAIN_BROWSER, event == AVAHI_BROWSER_NEW ? "ItemNew" : "ItemRemove");
    dbus_message_append_args(
        m,
        DBUS_TYPE_INT32, &i_interface,
        DBUS_TYPE_INT32, &i_protocol,
        DBUS_TYPE_STRING, &domain,
        DBUS_TYPE_INVALID);
    dbus_message_set_destination(m, i->client->name);   
    dbus_connection_send(server->bus, m, NULL);
    dbus_message_unref(m);
}

static DBusHandlerResult msg_service_type_browser_impl(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError error;
    ServiceTypeBrowserInfo *i = userdata;

    assert(c);
    assert(m);
    assert(i);
    
    dbus_error_init(&error);

    avahi_log_debug("dbus: interface=%s, path=%s, member=%s",
                    dbus_message_get_interface(m),
                    dbus_message_get_path(m),
                    dbus_message_get_member(m));

    /* Introspection */
    if (dbus_message_is_method_call(m, DBUS_INTERFACE_INTROSPECTABLE, "Introspect"))
        return handle_introspect(c, m, "ServiceTypeBrowser.introspect");
    
    /* Access control */
    if (strcmp(dbus_message_get_sender(m), i->client->name)) 
        return respond_error(c, m, AVAHI_ERR_ACCESS_DENIED, NULL);
    
    if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVICE_TYPE_BROWSER, "Free")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing ServiceTypeBrowser::Free message");
            goto fail;
        }

        service_type_browser_free(i);
        return respond_ok(c, m);
        
    }
    
    avahi_log_warn("Missed message %s::%s()", dbus_message_get_interface(m), dbus_message_get_member(m));

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void service_type_browser_callback(AvahiSServiceTypeBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *type, const char *domain, void* userdata) {
    ServiceTypeBrowserInfo *i = userdata;
    DBusMessage *m;
    int32_t i_interface, i_protocol;
    
    assert(b);
    assert(type);
    assert(domain);
    assert(i);

    i_interface = (int32_t) interface;
    i_protocol = (int32_t) protocol;

    m = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_SERVICE_TYPE_BROWSER, event == AVAHI_BROWSER_NEW ? "ItemNew" : "ItemRemove");
    dbus_message_append_args(
        m,
        DBUS_TYPE_INT32, &i_interface,
        DBUS_TYPE_INT32, &i_protocol,
        DBUS_TYPE_STRING, &type,
        DBUS_TYPE_STRING, &domain,
        DBUS_TYPE_INVALID);
    dbus_message_set_destination(m, i->client->name);   
    dbus_connection_send(server->bus, m, NULL);
    dbus_message_unref(m);
}

static DBusHandlerResult msg_service_browser_impl(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError error;
    ServiceBrowserInfo *i = userdata;

    assert(c);
    assert(m);
    assert(i);
    
    dbus_error_init(&error);

    avahi_log_debug("dbus: interface=%s, path=%s, member=%s",
                    dbus_message_get_interface(m),
                    dbus_message_get_path(m),
                    dbus_message_get_member(m));

    /* Introspection */
    if (dbus_message_is_method_call(m, DBUS_INTERFACE_INTROSPECTABLE, "Introspect"))
        return handle_introspect(c, m, "ServiceBrowser.Introspect");
    
    /* Access control */
    if (strcmp(dbus_message_get_sender(m), i->client->name)) 
        return respond_error(c, m, AVAHI_ERR_ACCESS_DENIED, NULL);
    
    if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVICE_BROWSER, "Free")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing ServiceBrowser::Free message");
            goto fail;
        }

        service_browser_free(i);
        return respond_ok(c, m);
        
    }
    
    avahi_log_warn("Missed message %s::%s()", dbus_message_get_interface(m), dbus_message_get_member(m));

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void service_browser_callback(AvahiSServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, void* userdata) {
    ServiceBrowserInfo *i = userdata;
    DBusMessage *m;
    int32_t i_interface, i_protocol;
    
    assert(b);
    assert(name);
    assert(type);
    assert(domain);
    assert(i);

    i_interface = (int32_t) interface;
    i_protocol = (int32_t) protocol;

    m = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_SERVICE_BROWSER, event == AVAHI_BROWSER_NEW ? "ItemNew" : "ItemRemove");
    dbus_message_append_args(
        m,
        DBUS_TYPE_INT32, &i_interface,
        DBUS_TYPE_INT32, &i_protocol,
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_STRING, &type,
        DBUS_TYPE_STRING, &domain,
        DBUS_TYPE_INVALID);
    dbus_message_set_destination(m, i->client->name);   
    dbus_connection_send(server->bus, m, NULL);
    dbus_message_unref(m);
}

static void service_resolver_callback(
    AvahiSServiceResolver *r,
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
    void* userdata) {
    
    ServiceResolverInfo *i = userdata;
    
    assert(r);
    assert(i);

    if (event == AVAHI_RESOLVER_FOUND) {
        char t[256], *pt = t;
        int32_t i_interface, i_protocol, i_aprotocol;
        unsigned n, j;
        AvahiStringList *p;
        DBusMessage *reply;
        DBusMessageIter iter, sub;

        assert(host_name);
        
        assert(a);
        avahi_address_snprint(t, sizeof(t), a);

        i_interface = (int32_t) interface;
        i_protocol = (int32_t) protocol;
        i_aprotocol = (int32_t) a->family;

        reply = dbus_message_new_method_return(i->message);
        dbus_message_append_args(
            reply,
            DBUS_TYPE_INT32, &i_interface,
            DBUS_TYPE_INT32, &i_protocol,
            DBUS_TYPE_STRING, &name,
            DBUS_TYPE_STRING, &type,
            DBUS_TYPE_STRING, &domain,
            DBUS_TYPE_STRING, &host_name,
            DBUS_TYPE_INT32, &i_aprotocol,
            DBUS_TYPE_STRING, &pt,
            DBUS_TYPE_UINT16, &port,
            DBUS_TYPE_INVALID);

        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "ay", &sub);

        for (p = txt, j = n-1; p; p = p->next, j--) {
            DBusMessageIter sub2;
            const uint8_t *data = p->text;
            
            dbus_message_iter_open_container(&sub, DBUS_TYPE_ARRAY, "y", &sub2);
            dbus_message_iter_append_fixed_array(&sub2, DBUS_TYPE_BYTE, &data, p->size); 
            dbus_message_iter_close_container(&sub, &sub2);

        }
        dbus_message_iter_close_container(&iter, &sub);
                
        dbus_connection_send(server->bus, reply, NULL);
        dbus_message_unref(reply);
    } else {
        assert(event == AVAHI_RESOLVER_TIMEOUT);

        respond_error(server->bus, i->message, AVAHI_ERR_TIMEOUT, NULL);
    }

    service_resolver_free(i);
}

static DBusHandlerResult msg_server_impl(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError error;

    dbus_error_init(&error);

    avahi_log_debug("dbus: interface=%s, path=%s, member=%s",
                    dbus_message_get_interface(m),
                    dbus_message_get_path(m),
                    dbus_message_get_member(m));

    if (dbus_message_is_method_call(m, DBUS_INTERFACE_INTROSPECTABLE, "Introspect"))
        return handle_introspect(c, m, "Server.introspect");
        
    else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "GetHostName")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing Server::GetHostName message");
            goto fail;
        }

        return respond_string(c, m, avahi_server_get_host_name(avahi_server));
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "GetDomainName")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing Server::GetDomainName message");
            goto fail;
        }

        return respond_string(c, m, avahi_server_get_domain_name(avahi_server));

    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "GetHostNameFqdn")) {

        if (!(dbus_message_get_args(m, &error, DBUS_TYPE_INVALID))) {
            avahi_log_warn("Error parsing Server::GetHostNameFqdn message");
            goto fail;
        }
    
        return respond_string(c, m, avahi_server_get_host_name_fqdn(avahi_server));
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "GetVersionString")) {

        if (!(dbus_message_get_args(m, &error, DBUS_TYPE_INVALID))) {
            avahi_log_warn("Error parsing Server::GetVersionString message");
            goto fail;
        }
    
        return respond_string(c, m, PACKAGE_STRING);

    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "GetState")) {
        AvahiServerState state;
        
        if (!(dbus_message_get_args(m, &error, DBUS_TYPE_INVALID))) {
            avahi_log_warn("Error parsing Server::GetState message");
            goto fail;
        }
        
        state = avahi_server_get_state(avahi_server);
        return respond_int32(c, m, (int32_t) state);

    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "GetNetworkInterfaceNameByIndex")) {
        int32_t idx;
        int fd;
        struct ifreq ifr;
        
        if (!(dbus_message_get_args(m, &error, DBUS_TYPE_INT32, &idx, DBUS_TYPE_INVALID))) {
            avahi_log_warn("Error parsing Server::GetNetworkInterfaceNameByIndex message");
            goto fail;
        }

#ifdef VALGRIND_WORKAROUND
        return respond_string(c, m, "blah");
#else
        
        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            char txt[256];
            snprintf(txt, sizeof(txt), "OS Error: %s", strerror(errno));
            return respond_error(c, m, AVAHI_ERR_OS, txt);
        }

        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_ifindex = idx;

        if (ioctl(fd, SIOCGIFNAME, &ifr) < 0) {
            char txt[256];
            snprintf(txt, sizeof(txt), "OS Error: %s", strerror(errno));
            close(fd);
            return respond_error(c, m, AVAHI_ERR_OS, txt);
        }

        close(fd);
        
        return respond_string(c, m, ifr.ifr_name);
#endif
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "GetNetworkInterfaceIndexByName")) {
        char *n;
        int fd;
        struct ifreq ifr;
        
        if (!(dbus_message_get_args(m, &error, DBUS_TYPE_STRING, &n, DBUS_TYPE_INVALID)) || !n) {
            avahi_log_warn("Error parsing Server::GetNetworkInterfaceIndexByName message");
            goto fail;
        }

#ifdef VALGRIND_WORKAROUND
        return respond_int32(c, m, 1);
#else
        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            char txt[256];
            snprintf(txt, sizeof(txt), "OS Error: %s", strerror(errno));
            return respond_error(c, m, AVAHI_ERR_OS, txt);
        }

        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", n);

        if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
            char txt[256];
            snprintf(txt, sizeof(txt), "OS Error: %s", strerror(errno));
            close(fd);
            return respond_error(c, m, AVAHI_ERR_OS, txt);
        }

        close(fd);
        
        return respond_int32(c, m, ifr.ifr_ifindex);
#endif

    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "GetAlternativeHostName")) {
        char *n, * t;
        
        if (!(dbus_message_get_args(m, &error, DBUS_TYPE_STRING, &n, DBUS_TYPE_INVALID)) || !n) {
            avahi_log_warn("Error parsing Server::GetAlternativeHostName message");
            goto fail;
        }

        t = avahi_alternative_host_name(n);
        respond_string(c, m, t);
        avahi_free(t);

        return DBUS_HANDLER_RESULT_HANDLED;

    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "GetAlternativeServiceName")) {
        char *n, *t;
        
        if (!(dbus_message_get_args(m, &error, DBUS_TYPE_STRING, &n, DBUS_TYPE_INVALID)) || !n) {
            avahi_log_warn("Error parsing Server::GetAlternativeServiceName message");
            goto fail;
        }

        t = avahi_alternative_service_name(n);
        respond_string(c, m, t);
        avahi_free(t);

        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "EntryGroupNew")) {
        Client *client;
        EntryGroupInfo *i;
        static const DBusObjectPathVTable vtable = {
            NULL,
            msg_entry_group_impl,
            NULL,
            NULL,
            NULL,
            NULL
        };

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing Server::EntryGroupNew message");
            goto fail;
        }

        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn("Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }

        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn("Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        i = avahi_new(EntryGroupInfo, 1);
        i->id = ++client->current_id;
        i->client = client;
        i->path = avahi_strdup_printf("/Client%u/EntryGroup%u", client->id, i->id);
        i->n_entries = 0;
        AVAHI_LLIST_PREPEND(EntryGroupInfo, entry_groups, client->entry_groups, i);
        client->n_objects++;
        
        if (!(i->entry_group = avahi_s_entry_group_new(avahi_server, entry_group_callback, i))) {
            entry_group_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }

        dbus_connection_register_object_path(c, i->path, &vtable, i);
        return respond_path(c, m, i->path);
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "ResolveHostName")) {
        Client *client;
        int32_t interface, protocol, aprotocol;
        char *name;
        HostNameResolverInfo *i;
            
        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INT32, &aprotocol,
                DBUS_TYPE_INVALID) || !name) {
            avahi_log_warn("Error parsing Server::ResolveHostName message");
            goto fail;
        }

        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn("Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }

        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn("Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        i = avahi_new(HostNameResolverInfo, 1);
        i->client = client;
        i->message = dbus_message_ref(m);
        AVAHI_LLIST_PREPEND(HostNameResolverInfo, host_name_resolvers, client->host_name_resolvers, i);
        client->n_objects++;

        if (!(i->host_name_resolver = avahi_s_host_name_resolver_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, name, (AvahiProtocol) aprotocol, host_name_resolver_callback, i))) {
            host_name_resolver_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "ResolveAddress")) {
        Client *client;
        int32_t interface, protocol;
        char *address;
        AddressResolverInfo *i;
        AvahiAddress a;
            
        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &address,
                DBUS_TYPE_INVALID) || !address) {
            avahi_log_warn("Error parsing Server::ResolveAddress message");
            goto fail;
        }

        if (!avahi_address_parse(address, AVAHI_PROTO_UNSPEC, &a))
            return respond_error(c, m, AVAHI_ERR_INVALID_ADDRESS, NULL);

        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn("Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }

        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn("Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        i = avahi_new(AddressResolverInfo, 1);
        i->client = client;
        i->message = dbus_message_ref(m);
        AVAHI_LLIST_PREPEND(AddressResolverInfo, address_resolvers, client->address_resolvers, i);
        client->n_objects++;

        if (!(i->address_resolver = avahi_s_address_resolver_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, &a, address_resolver_callback, i))) {
            address_resolver_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "DomainBrowserNew")) {
        Client *client;
        DomainBrowserInfo *i;
        static const DBusObjectPathVTable vtable = {
            NULL,
            msg_domain_browser_impl,
            NULL,
            NULL,
            NULL,
            NULL
        };
        int32_t interface, protocol, type;
        char *domain;
        

        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &domain,
                DBUS_TYPE_INT32, &type,
                DBUS_TYPE_INVALID) || type < 0 || type >= AVAHI_DOMAIN_BROWSER_MAX) {
            avahi_log_warn("Error parsing Server::DomainBrowserNew message");
            goto fail;
        }

        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn("Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }

        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn("Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        if (!*domain)
            domain = NULL;

        i = avahi_new(DomainBrowserInfo, 1);
        i->id = ++client->current_id;
        i->client = client;
        i->path = avahi_strdup_printf("/Client%u/DomainBrowser%u", client->id, i->id);
        AVAHI_LLIST_PREPEND(DomainBrowserInfo, domain_browsers, client->domain_browsers, i);
        client->n_objects++;

        if (!(i->domain_browser = avahi_s_domain_browser_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, domain, (AvahiDomainBrowserType) type, domain_browser_callback, i))) {
            domain_browser_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        dbus_connection_register_object_path(c, i->path, &vtable, i);
        return respond_path(c, m, i->path);

    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "ServiceTypeBrowserNew")) {
        Client *client;
        ServiceTypeBrowserInfo *i;
        static const DBusObjectPathVTable vtable = {
            NULL,
            msg_service_type_browser_impl,
            NULL,
            NULL,
            NULL,
            NULL
        };
        int32_t interface, protocol;
        char *domain;
        
        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &domain,
                DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing Server::ServiceTypeBrowserNew message");
            goto fail;
        }

        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn("Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }


        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn("Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        if (!*domain)
            domain = NULL;

        i = avahi_new(ServiceTypeBrowserInfo, 1);
        i->id = ++client->current_id;
        i->client = client;
        i->path = avahi_strdup_printf("/Client%u/ServiceTypeBrowser%u", client->id, i->id);
        AVAHI_LLIST_PREPEND(ServiceTypeBrowserInfo, service_type_browsers, client->service_type_browsers, i);
        client->n_objects++;

        if (!(i->service_type_browser = avahi_s_service_type_browser_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, domain, service_type_browser_callback, i))) {
            service_type_browser_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        dbus_connection_register_object_path(c, i->path, &vtable, i);
        return respond_path(c, m, i->path);
        
     } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "ServiceBrowserNew")) {
        Client *client;
        ServiceBrowserInfo *i;
        static const DBusObjectPathVTable vtable = {
            NULL,
            msg_service_browser_impl,
            NULL,
            NULL,
            NULL,
            NULL
        };
        int32_t interface, protocol;
        char *domain, *type;
        
        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &type,
                DBUS_TYPE_STRING, &domain,
                DBUS_TYPE_INVALID) || !type) {
            avahi_log_warn("Error parsing Server::ServiceBrowserNew message");
            goto fail;
        }

        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn("Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }


        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn("Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        if (!*domain)
            domain = NULL;

        i = avahi_new(ServiceBrowserInfo, 1);
        i->id = ++client->current_id;
        i->client = client;
        i->path = avahi_strdup_printf("/Client%u/ServiceBrowser%u", client->id, i->id);
        AVAHI_LLIST_PREPEND(ServiceBrowserInfo, service_browsers, client->service_browsers, i);
        client->n_objects++;

        if (!(i->service_browser = avahi_s_service_browser_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, type, domain, service_browser_callback, i))) {
            service_browser_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        dbus_connection_register_object_path(c, i->path, &vtable, i);
        return respond_path(c, m, i->path);
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "ResolveService")) {
        Client *client;
        int32_t interface, protocol, aprotocol;
        char *name, *type, *domain;
        ServiceResolverInfo *i;
            
        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_STRING, &type,
                DBUS_TYPE_STRING, &domain,
                DBUS_TYPE_INT32, &aprotocol,
                DBUS_TYPE_INVALID) || !name || !type) {
            avahi_log_warn("Error parsing Server::ResolveService message");
            goto fail;
        }

        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn("Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }
        
        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn("Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        if (!*domain)
            domain = NULL;
        
        i = avahi_new(ServiceResolverInfo, 1);
        i->client = client;
        i->message = dbus_message_ref(m);
        AVAHI_LLIST_PREPEND(ServiceResolverInfo, service_resolvers, client->service_resolvers, i);
        client->n_objects++;

        if (!(i->service_resolver = avahi_s_service_resolver_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, name, type, domain, (AvahiProtocol) aprotocol, service_resolver_callback, i))) {
            service_resolver_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        return DBUS_HANDLER_RESULT_HANDLED;
     }

    avahi_log_warn("Missed message %s::%s()", dbus_message_get_interface(m), dbus_message_get_member(m));

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void dbus_protocol_server_state_changed(AvahiServerState state) {
    DBusMessage *m;
    int32_t t;
    
    if (!server)
        return;

    m = dbus_message_new_signal(AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, "StateChanged");
    t = (int32_t) state;
    dbus_message_append_args(m, DBUS_TYPE_INT32, &t, DBUS_TYPE_INVALID);
    dbus_connection_send(server->bus, m, NULL);
    dbus_message_unref(m);
}

int dbus_protocol_setup(const AvahiPoll *poll_api) {
    DBusError error;

    static const DBusObjectPathVTable server_vtable = {
        NULL,
        msg_server_impl,
        NULL,
        NULL,
        NULL,
        NULL
    };


    dbus_error_init(&error);

    server = avahi_new(Server, 1);
    AVAHI_LLIST_HEAD_INIT(Clients, server->clients);
    server->current_id = 0;
    server->n_clients = 0;

    server->bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error)) {
        avahi_log_warn("dbus_bus_get(): %s", error.message);
        goto fail;
    }

    avahi_dbus_connection_glue(server->bus, poll_api);

    dbus_bus_request_name(server->bus, AVAHI_DBUS_NAME, 0, &error);
    if (dbus_error_is_set(&error)) {
        avahi_log_warn("dbus_bus_request_name(): %s", error.message);
        goto fail;
    }

    dbus_connection_add_filter(server->bus, msg_signal_filter_impl, (void*) poll_api, NULL);
    dbus_bus_add_match(server->bus, "type='signal',""interface='" DBUS_INTERFACE_DBUS  "'", &error);
    dbus_connection_register_object_path(server->bus, AVAHI_DBUS_PATH_SERVER, &server_vtable, NULL);

    return 0;

fail:
    if (server->bus) {
        dbus_connection_disconnect(server->bus);
        dbus_connection_unref(server->bus);
    }
    
    dbus_error_free (&error);
    avahi_free(server);
    server = NULL;
    return -1;
}

void dbus_protocol_shutdown(void) {

    if (server) {
    
        while (server->clients)
            client_free(server->clients);

        assert(server->n_clients == 0);

        if (server->bus) {
            dbus_connection_disconnect(server->bus);
            dbus_connection_unref(server->bus);
        }

        avahi_free(server);
        server = NULL;
    }
}
