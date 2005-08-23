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

#include <avahi-common/dbus.h>
#include <avahi-common/llist.h>
#include <avahi-common/malloc.h>
#include <avahi-common/dbus.h>
#include <avahi-common/dbus-watch-glue.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-core/log.h>
#include <avahi-core/core.h>

#include "dbus-protocol.h"
#include "main.h"

typedef struct Server Server;
typedef struct Client Client;
typedef struct EntryGroupInfo EntryGroupInfo;
typedef struct SyncHostNameResolverInfo SyncHostNameResolverInfo;
typedef struct AsyncHostNameResolverInfo AsyncHostNameResolverInfo;
typedef struct SyncAddressResolverInfo SyncAddressResolverInfo;
typedef struct AsyncAddressResolverInfo AsyncAddressResolverInfo;
typedef struct DomainBrowserInfo DomainBrowserInfo;
typedef struct ServiceTypeBrowserInfo ServiceTypeBrowserInfo;
typedef struct ServiceBrowserInfo ServiceBrowserInfo;
typedef struct SyncServiceResolverInfo SyncServiceResolverInfo;
typedef struct AsyncServiceResolverInfo AsyncServiceResolverInfo;

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

struct SyncHostNameResolverInfo {
    Client *client;
    AvahiSHostNameResolver *host_name_resolver;
    DBusMessage *message;

    AVAHI_LLIST_FIELDS(SyncHostNameResolverInfo, sync_host_name_resolvers);
};

struct AsyncHostNameResolverInfo {
    unsigned id;
    Client *client;
    AvahiSHostNameResolver *host_name_resolver;
    char *path;

    AVAHI_LLIST_FIELDS(AsyncHostNameResolverInfo, async_host_name_resolvers);
};

struct SyncAddressResolverInfo {
    Client *client;
    AvahiSAddressResolver *address_resolver;
    DBusMessage *message;

    AVAHI_LLIST_FIELDS(SyncAddressResolverInfo, sync_address_resolvers);
};

struct AsyncAddressResolverInfo {
    unsigned id;
    Client *client;
    AvahiSAddressResolver *address_resolver;
    char *path;

    AVAHI_LLIST_FIELDS(AsyncAddressResolverInfo, async_address_resolvers);
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

struct SyncServiceResolverInfo {
    Client *client;
    AvahiSServiceResolver *service_resolver;
    DBusMessage *message;

    AVAHI_LLIST_FIELDS(SyncServiceResolverInfo, sync_service_resolvers);
};

struct AsyncServiceResolverInfo {
    unsigned id;
    Client *client;
    AvahiSServiceResolver *service_resolver;
    char *path;

    AVAHI_LLIST_FIELDS(AsyncServiceResolverInfo, async_service_resolvers);
};

struct Client {
    unsigned id;
    char *name;
    unsigned current_id;
    int n_objects;
    
    AVAHI_LLIST_FIELDS(Client, clients);
    AVAHI_LLIST_HEAD(EntryGroupInfo, entry_groups);
    AVAHI_LLIST_HEAD(SyncHostNameResolverInfo, sync_host_name_resolvers);
    AVAHI_LLIST_HEAD(AsyncHostNameResolverInfo, async_host_name_resolvers);
    AVAHI_LLIST_HEAD(SyncAddressResolverInfo, sync_address_resolvers);
    AVAHI_LLIST_HEAD(AsyncAddressResolverInfo, async_address_resolvers);
    AVAHI_LLIST_HEAD(DomainBrowserInfo, domain_browsers);
    AVAHI_LLIST_HEAD(ServiceTypeBrowserInfo, service_type_browsers);
    AVAHI_LLIST_HEAD(ServiceBrowserInfo, service_browsers);
    AVAHI_LLIST_HEAD(SyncServiceResolverInfo, sync_service_resolvers);
    AVAHI_LLIST_HEAD(AsyncServiceResolverInfo, async_service_resolvers);
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

static void sync_host_name_resolver_free(SyncHostNameResolverInfo *i) {
    assert(i);

    if (i->host_name_resolver)
        avahi_s_host_name_resolver_free(i->host_name_resolver);
    dbus_message_unref(i->message);
    AVAHI_LLIST_REMOVE(SyncHostNameResolverInfo, sync_host_name_resolvers, i->client->sync_host_name_resolvers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void async_host_name_resolver_free(AsyncHostNameResolverInfo *i) {
    assert(i);

    if (i->host_name_resolver)
        avahi_s_host_name_resolver_free(i->host_name_resolver);
    dbus_connection_unregister_object_path(server->bus, i->path);
    AVAHI_LLIST_REMOVE(AsyncHostNameResolverInfo, async_host_name_resolvers, i->client->async_host_name_resolvers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void sync_address_resolver_free(SyncAddressResolverInfo *i) {
    assert(i);

    if (i->address_resolver)
        avahi_s_address_resolver_free(i->address_resolver);
    dbus_message_unref(i->message);
    AVAHI_LLIST_REMOVE(SyncAddressResolverInfo, sync_address_resolvers, i->client->sync_address_resolvers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void async_address_resolver_free(AsyncAddressResolverInfo *i) {
    assert(i);

    if (i->address_resolver)
        avahi_s_address_resolver_free(i->address_resolver);
    dbus_connection_unregister_object_path(server->bus, i->path);
    AVAHI_LLIST_REMOVE(AsyncAddressResolverInfo, async_address_resolvers, i->client->async_address_resolvers, i);

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

static void sync_service_resolver_free(SyncServiceResolverInfo *i) {
    assert(i);

    if (i->service_resolver)
        avahi_s_service_resolver_free(i->service_resolver);
    dbus_message_unref(i->message);
    AVAHI_LLIST_REMOVE(SyncServiceResolverInfo, sync_service_resolvers, i->client->sync_service_resolvers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void async_service_resolver_free(AsyncServiceResolverInfo *i) {
    assert(i);

    if (i->service_resolver)
        avahi_s_service_resolver_free(i->service_resolver);

    dbus_connection_unregister_object_path(server->bus, i->path);
    AVAHI_LLIST_REMOVE(AsyncServiceResolverInfo, async_service_resolvers, i->client->async_service_resolvers, i);

    i->client->n_objects--;
    assert(i->client->n_objects >= 0);

    avahi_free(i);
}

static void client_free(Client *c) {
    
    assert(server);
    assert(c);

    while (c->entry_groups)
        entry_group_free(c->entry_groups);

    while (c->sync_host_name_resolvers)
        sync_host_name_resolver_free(c->sync_host_name_resolvers);

    while (c->async_host_name_resolvers)
        async_host_name_resolver_free(c->async_host_name_resolvers);
    
    while (c->sync_address_resolvers)
        sync_address_resolver_free(c->sync_address_resolvers);

    while (c->async_address_resolvers)
        async_address_resolver_free(c->async_address_resolvers);

    while (c->domain_browsers)
        domain_browser_free(c->domain_browsers);

    while (c->service_type_browsers)
        service_type_browser_free(c->service_type_browsers);

    while (c->service_browsers)
        service_browser_free(c->service_browsers);

    while (c->sync_service_resolvers)
        sync_service_resolver_free(c->sync_service_resolvers);

    while (c->async_service_resolvers)
        async_service_resolver_free(c->async_service_resolvers);

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
    AVAHI_LLIST_HEAD_INIT(SyncHostNameResolverInfo, client->sync_host_name_resolvers);
    AVAHI_LLIST_HEAD_INIT(AsyncHostNameResolverInfo, client->async_host_name_resolvers);
    AVAHI_LLIST_HEAD_INIT(SyncAddressResolverInfo, client->sync_address_resolvers);
    AVAHI_LLIST_HEAD_INIT(AsyncAddressResolverInfo, client->async_address_resolvers);
    AVAHI_LLIST_HEAD_INIT(DomainBrowserInfo, client->domain_browsers);
    AVAHI_LLIST_HEAD_INIT(ServiceTypeBrowserInfo, client->service_type_browsers);
    AVAHI_LLIST_HEAD_INIT(ServiceBrowserInfo, client->service_browsers);
    AVAHI_LLIST_HEAD_INIT(SyncServiceResolverInfo, client->sync_service_resolvers);
    AVAHI_LLIST_HEAD_INIT(AsyncServiceResolverInfo, client->async_service_resolvers);

    AVAHI_LLIST_PREPEND(Client, clients, server->clients, client);

    server->n_clients++;
    assert(server->n_clients > 0);
    
    return client;
}

static DBusHandlerResult respond_error(DBusConnection *c, DBusMessage *m, int error, const char *text) {
    DBusMessage *reply;

    assert(-error > -AVAHI_OK);
    assert(-error < -AVAHI_ERR_MAX);
    
    reply = dbus_message_new_error(m, avahi_error_number_to_dbus (error), text ? text : avahi_strerror(error));
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

static void sync_host_name_resolver_callback(AvahiSHostNameResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *host_name, const AvahiAddress *a, void* userdata) {
    SyncHostNameResolverInfo *i = userdata;
    
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

    sync_host_name_resolver_free(i);
}

static void sync_address_resolver_callback(AvahiSAddressResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const AvahiAddress *address, const char *host_name, void* userdata) {
    SyncAddressResolverInfo *i = userdata;
    
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

    sync_address_resolver_free(i);
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

static void append_string_list(DBusMessage *reply, AvahiStringList *txt) {
    AvahiStringList *p;
    DBusMessageIter iter, sub;
    
    assert(reply);

    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "ay", &sub);
    
    for (p = txt; p; p = p->next) {
        DBusMessageIter sub2;
        const uint8_t *data = p->text;
        
        dbus_message_iter_open_container(&sub, DBUS_TYPE_ARRAY, "y", &sub2);
        dbus_message_iter_append_fixed_array(&sub2, DBUS_TYPE_BYTE, &data, p->size); 
        dbus_message_iter_close_container(&sub, &sub2);
        
    }
    dbus_message_iter_close_container(&iter, &sub);
}

static void sync_service_resolver_callback(
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
    
    SyncServiceResolverInfo *i = userdata;
    
    assert(r);
    assert(i);

    if (event == AVAHI_RESOLVER_FOUND) {
        char t[256], *pt = t;
        int32_t i_interface, i_protocol, i_aprotocol;
        DBusMessage *reply;
    
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

        append_string_list(reply, txt);
                
        dbus_connection_send(server->bus, reply, NULL);
        dbus_message_unref(reply);
    } else {
        assert(event == AVAHI_RESOLVER_TIMEOUT);

        respond_error(server->bus, i->message, AVAHI_ERR_TIMEOUT, NULL);
    }

    sync_service_resolver_free(i);
}

static void async_address_resolver_callback(AvahiSAddressResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const AvahiAddress *address, const char *host_name, void* userdata) {
    AsyncAddressResolverInfo *i = userdata;
    DBusMessage *reply;
    
    assert(r);
    assert(address);
    assert(i);

    if (event == AVAHI_RESOLVER_FOUND) {
        char t[256], *pt = t;
        int32_t i_interface, i_protocol, i_aprotocol;

        assert(host_name);
        avahi_address_snprint(t, sizeof(t), address);

        i_interface = (int32_t) interface;
        i_protocol = (int32_t) protocol;
        i_aprotocol = (int32_t) address->family;
        
        reply = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_ADDRESS_RESOLVER, "Found");
        dbus_message_append_args(
            reply,
            DBUS_TYPE_INT32, &i_interface,
            DBUS_TYPE_INT32, &i_protocol,
            DBUS_TYPE_INT32, &i_aprotocol,
            DBUS_TYPE_STRING, &pt,
            DBUS_TYPE_STRING, &host_name,
            DBUS_TYPE_INVALID);

    } else {
        assert(event == AVAHI_RESOLVER_TIMEOUT);

        reply = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_ADDRESS_RESOLVER, "Timeout");
    }

    dbus_connection_send(server->bus, reply, NULL);
    dbus_message_unref(reply);
}

static DBusHandlerResult msg_async_address_resolver_impl(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError error;
    AsyncAddressResolverInfo *i = userdata;

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
        return handle_introspect(c, m, "AddressResolver.Introspect");
    
    /* Access control */
    if (strcmp(dbus_message_get_sender(m), i->client->name)) 
        return respond_error(c, m, AVAHI_ERR_ACCESS_DENIED, NULL);
    
    if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_ADDRESS_RESOLVER, "Free")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing AddressResolver::Free message");
            goto fail;
        }

        async_address_resolver_free(i);
        return respond_ok(c, m);
        
    }
    
    avahi_log_warn("Missed message %s::%s()", dbus_message_get_interface(m), dbus_message_get_member(m));

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void async_host_name_resolver_callback(AvahiSHostNameResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *host_name, const AvahiAddress *a, void* userdata) {
    AsyncHostNameResolverInfo *i = userdata;
    DBusMessage *reply;
    
    assert(r);
    assert(host_name);
    assert(i);

    if (event == AVAHI_RESOLVER_FOUND) {
        char t[256], *pt = t;
        int32_t i_interface, i_protocol, i_aprotocol;

        assert(a);
        avahi_address_snprint(t, sizeof(t), a);

        i_interface = (int32_t) interface;
        i_protocol = (int32_t) protocol;
        i_aprotocol = (int32_t) a->family;
        
        reply = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_HOST_NAME_RESOLVER, "Found");
        dbus_message_append_args(
            reply,
            DBUS_TYPE_INT32, &i_interface,
            DBUS_TYPE_INT32, &i_protocol,
            DBUS_TYPE_STRING, &host_name,
            DBUS_TYPE_INT32, &i_aprotocol,
            DBUS_TYPE_STRING, &pt,
            DBUS_TYPE_INVALID);
    } else {
        assert(event == AVAHI_RESOLVER_TIMEOUT);

        reply = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_HOST_NAME_RESOLVER, "Timeout");
    }

    dbus_connection_send(server->bus, reply, NULL);
    dbus_message_unref(reply);
}

static DBusHandlerResult msg_async_host_name_resolver_impl(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError error;
    AsyncHostNameResolverInfo *i = userdata;

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
        return handle_introspect(c, m, "HostNameResolver.Introspect");
    
    /* Access control */
    if (strcmp(dbus_message_get_sender(m), i->client->name)) 
        return respond_error(c, m, AVAHI_ERR_ACCESS_DENIED, NULL);
    
    if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_HOST_NAME_RESOLVER, "Free")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing HostNameResolver::Free message");
            goto fail;
        }

        async_host_name_resolver_free(i);
        return respond_ok(c, m);
    }
    
    avahi_log_warn("Missed message %s::%s()", dbus_message_get_interface(m), dbus_message_get_member(m));

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void async_service_resolver_callback(
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

    AsyncServiceResolverInfo *i = userdata;
    DBusMessage *reply;
    
    assert(r);
    assert(host_name);
    assert(i);

    if (event == AVAHI_RESOLVER_FOUND) {
        char t[256], *pt = t;
        int32_t i_interface, i_protocol, i_aprotocol;
    
        assert(host_name);
        
        assert(a);
        avahi_address_snprint(t, sizeof(t), a);

        i_interface = (int32_t) interface;
        i_protocol = (int32_t) protocol;
        i_aprotocol = (int32_t) a->family;

        reply = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_SERVICE_RESOLVER, "Found");
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

        append_string_list(reply, txt);
        
    } else {
        assert(event == AVAHI_RESOLVER_TIMEOUT);

        reply = dbus_message_new_signal(i->path, AVAHI_DBUS_INTERFACE_SERVICE_RESOLVER, "Timeout");
    }

    dbus_connection_send(server->bus, reply, NULL);
    dbus_message_unref(reply);
}

static DBusHandlerResult msg_async_service_resolver_impl(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError error;
    AsyncServiceResolverInfo *i = userdata;

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
        return handle_introspect(c, m, "ServiceResolver.Introspect");
    
    /* Access control */
    if (strcmp(dbus_message_get_sender(m), i->client->name)) 
        return respond_error(c, m, AVAHI_ERR_ACCESS_DENIED, NULL);
    
    if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVICE_RESOLVER, "Free")) {

        if (!dbus_message_get_args(m, &error, DBUS_TYPE_INVALID)) {
            avahi_log_warn("Error parsing ServiceResolver::Free message");
            goto fail;
        }

        async_service_resolver_free(i);
        return respond_ok(c, m);
    }
    
    avahi_log_warn("Missed message %s::%s()", dbus_message_get_interface(m), dbus_message_get_member(m));

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
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
        SyncHostNameResolverInfo *i;
            
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

        i = avahi_new(SyncHostNameResolverInfo, 1);
        i->client = client;
        i->message = dbus_message_ref(m);
        AVAHI_LLIST_PREPEND(SyncHostNameResolverInfo, sync_host_name_resolvers, client->sync_host_name_resolvers, i);
        client->n_objects++;

        if (!(i->host_name_resolver = avahi_s_host_name_resolver_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, name, (AvahiProtocol) aprotocol, sync_host_name_resolver_callback, i))) {
            sync_host_name_resolver_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "ResolveAddress")) {
        Client *client;
        int32_t interface, protocol;
        char *address;
        SyncAddressResolverInfo *i;
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

        i = avahi_new(SyncAddressResolverInfo, 1);
        i->client = client;
        i->message = dbus_message_ref(m);
        AVAHI_LLIST_PREPEND(SyncAddressResolverInfo, sync_address_resolvers, client->sync_address_resolvers, i);
        client->n_objects++;

        if (!(i->address_resolver = avahi_s_address_resolver_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, &a, sync_address_resolver_callback, i))) {
            sync_address_resolver_free(i);
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
        SyncServiceResolverInfo *i;
            
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
        
        i = avahi_new(SyncServiceResolverInfo, 1);
        i->client = client;
        i->message = dbus_message_ref(m);
        AVAHI_LLIST_PREPEND(SyncServiceResolverInfo, sync_service_resolvers, client->sync_service_resolvers, i);
        client->n_objects++;

        if (!(i->service_resolver = avahi_s_service_resolver_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, name, type, domain, (AvahiProtocol) aprotocol, sync_service_resolver_callback, i))) {
            sync_service_resolver_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "ServiceResolverNew")) {
        Client *client;
        int32_t interface, protocol, aprotocol;
        char *name, *type, *domain;
        AsyncServiceResolverInfo *i;
        static const DBusObjectPathVTable vtable = {
            NULL,
            msg_async_service_resolver_impl,
            NULL,
            NULL,
            NULL,
            NULL
        };

        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_STRING, &type,
                DBUS_TYPE_STRING, &domain,
                DBUS_TYPE_INT32, &aprotocol,
                DBUS_TYPE_INVALID) || !name || !type) {
            avahi_log_warn("Error parsing Server::ServiceResolverNew message");
            goto fail;
        }
            
        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn(__FILE__": Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }

        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn(__FILE__": Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        i = avahi_new(AsyncServiceResolverInfo, 1);
        i->id = ++client->current_id;
        i->client = client;
        i->path = avahi_strdup_printf("/Client%u/ServiceResolver%u", client->id, i->id);
        AVAHI_LLIST_PREPEND(AsyncServiceResolverInfo, async_service_resolvers, client->async_service_resolvers, i);
        client->n_objects++;

        if (!(i->service_resolver = avahi_s_service_resolver_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, name, type, domain, (AvahiProtocol) aprotocol, async_service_resolver_callback, i))) {
            async_service_resolver_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        dbus_connection_register_object_path(c, i->path, &vtable, i);
        return respond_path(c, m, i->path);

    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "HostNameResolverNew")) {
        Client *client;
        int32_t interface, protocol, aprotocol;
        char *name;
        AsyncHostNameResolverInfo *i;
        static const DBusObjectPathVTable vtable = {
            NULL,
            msg_async_host_name_resolver_impl,
            NULL,
            NULL,
            NULL,
            NULL
        };
            
        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INT32, &aprotocol,
                DBUS_TYPE_INVALID) || !name) {
            avahi_log_warn("Error parsing Server::HostNameResolverNew message");
            goto fail;
        }
            
        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn(__FILE__": Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }

        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn(__FILE__": Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        i = avahi_new(AsyncHostNameResolverInfo, 1);
        i->id = ++client->current_id;
        i->client = client;
        i->path = avahi_strdup_printf("/Client%u/HostNameResolver%u", client->id, i->id);
        AVAHI_LLIST_PREPEND(AsyncHostNameResolverInfo, async_host_name_resolvers, client->async_host_name_resolvers, i);
        client->n_objects++;

        if (!(i->host_name_resolver = avahi_s_host_name_resolver_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, name, aprotocol, async_host_name_resolver_callback, i))) {
            async_host_name_resolver_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        dbus_connection_register_object_path(c, i->path, &vtable, i);
        return respond_path(c, m, i->path);

    } else if (dbus_message_is_method_call(m, AVAHI_DBUS_INTERFACE_SERVER, "AddressResolverNew")) {
        Client *client;
        int32_t interface, protocol;
        char *address;
        AsyncAddressResolverInfo *i;
        AvahiAddress a;
        static const DBusObjectPathVTable vtable = {
            NULL,
            msg_async_address_resolver_impl,
            NULL,
            NULL,
            NULL,
            NULL
        };
            
        if (!dbus_message_get_args(
                m, &error,
                DBUS_TYPE_INT32, &interface,
                DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &address,
                DBUS_TYPE_INVALID) || !address) {
            avahi_log_warn("Error parsing Server::AddressResolverNew message");
            goto fail;
        }

        if (!avahi_address_parse(address, AVAHI_PROTO_UNSPEC, &a))
            return respond_error(c, m, AVAHI_ERR_INVALID_ADDRESS, NULL);

        if (!(client = client_get(dbus_message_get_sender(m), TRUE))) {
            avahi_log_warn(__FILE__": Too many clients, client request failed.");
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_CLIENTS, NULL);
        }

        if (client->n_objects >= MAX_OBJECTS_PER_CLIENT) {
            avahi_log_warn(__FILE__": Too many objects for client '%s', client request failed.", client->name);
            return respond_error(c, m, AVAHI_ERR_TOO_MANY_OBJECTS, NULL);
        }

        i = avahi_new(AsyncAddressResolverInfo, 1);
        i->id = ++client->current_id;
        i->client = client;
        i->path = avahi_strdup_printf("/Client%u/AddressResolver%u", client->id, i->id);
        AVAHI_LLIST_PREPEND(AsyncAddressResolverInfo, async_address_resolvers, client->async_address_resolvers, i);
        client->n_objects++;

        if (!(i->address_resolver = avahi_s_address_resolver_new(avahi_server, (AvahiIfIndex) interface, (AvahiProtocol) protocol, &a, async_address_resolver_callback, i))) {
            async_address_resolver_free(i);
            return respond_error(c, m, avahi_server_errno(avahi_server), NULL);
        }
        
        dbus_connection_register_object_path(c, i->path, &vtable, i);
        return respond_path(c, m, i->path);
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

    if (!(server->bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error))) {
        assert(dbus_error_is_set(&error));
        avahi_log_error("dbus_bus_get(): %s", error.message);
        goto fail;
    }

    if (avahi_dbus_connection_glue(server->bus, poll_api) < 0) {
        avahi_log_error("avahi_dbus_connection_glue() failed");
        goto fail;
    }

    if (dbus_bus_request_name(server->bus, AVAHI_DBUS_NAME, DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT, &error) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set(&error)) {
            avahi_log_error("dbus_bus_request_name(): %s", error.message);
            goto fail;
        }

        avahi_log_error("Failed to acquire DBUS name '"AVAHI_DBUS_NAME"'");
        goto fail;
    }

    if (!(dbus_connection_add_filter(server->bus, msg_signal_filter_impl, (void*) poll_api, NULL))) {
        avahi_log_error("dbus_connection_add_filter() failed");
        goto fail;
    }
    
    dbus_bus_add_match(server->bus, "type='signal',""interface='" DBUS_INTERFACE_DBUS  "'", &error);

    if (dbus_error_is_set(&error)) {
        avahi_log_error("dbus_bus_add_match(): %s", error.message);
        goto fail;
    }
    
    if (!(dbus_connection_register_object_path(server->bus, AVAHI_DBUS_PATH_SERVER, &server_vtable, NULL))) {
        avahi_log_error("dbus_connection_register_object_path() failed");
        goto fail;
    }

    return 0;

fail:
    if (server->bus) {
        dbus_connection_disconnect(server->bus);
        dbus_connection_unref(server->bus);
    }

    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
        
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
