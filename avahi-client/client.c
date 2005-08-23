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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dbus/dbus.h>

#include <avahi-common/dbus.h>
#include <avahi-common/llist.h>
#include <avahi-common/error.h>
#include <avahi-common/dbus.h>
#include <avahi-common/malloc.h>
#include <avahi-common/dbus-watch-glue.h>

#include "client.h"
#include "internal.h"

int avahi_client_set_errno (AvahiClient *client, int error) {
    assert(client);

    return client->error = error;
}

int avahi_client_set_dbus_error(AvahiClient *client, DBusError *error) {
    assert(client);
    assert(error);

    return avahi_client_set_errno(client, avahi_error_dbus_to_number(error->name));
}

static void client_set_state (AvahiClient *client, AvahiServerState state) {
    assert(client);

    if (client->state == state)
        return;

    client->state = state;

    switch (client->state) {
        case AVAHI_CLIENT_DISCONNECTED:
            if (client->bus) {
                dbus_connection_disconnect(client->bus);
                dbus_connection_unref(client->bus);
                client->bus = NULL;
            }

            /* Fall through */

        case AVAHI_CLIENT_S_COLLISION:
        case AVAHI_CLIENT_S_REGISTERING:

            /* Clear cached strings */
            avahi_free(client->host_name);
            avahi_free(client->host_name_fqdn);
            avahi_free(client->domain_name);

            client->host_name =  NULL;
            client->host_name_fqdn = NULL;
            client->domain_name = NULL;
            break;

        case AVAHI_CLIENT_S_INVALID:
        case AVAHI_CLIENT_S_RUNNING:
            break;
            
    }
    
    if (client->callback)
        client->callback (client, state, client->userdata);
}

static DBusHandlerResult filter_func(DBusConnection *bus, DBusMessage *message, void *userdata) {
    AvahiClient *client = userdata;
    DBusError error;

    assert(bus);
    assert(message);
    
    dbus_error_init (&error);

/*     fprintf(stderr, "dbus: interface=%s, path=%s, member=%s\n", */
/*             dbus_message_get_interface (message), */
/*             dbus_message_get_path (message), */
/*             dbus_message_get_member (message)); */

    if (client->state == AVAHI_CLIENT_DISCONNECTED)
        goto fail;

    if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL, "Disconnected")) {

        /* The DBUS server died or kicked us */
        client_set_state(client, AVAHI_CLIENT_DISCONNECTED);

    } if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
        char *name, *old, *new;
        
        if (!(dbus_message_get_args(
                  message, &error,
                  DBUS_TYPE_STRING, &name,
                  DBUS_TYPE_STRING, &old,
                  DBUS_TYPE_STRING, &new,
                  DBUS_TYPE_INVALID) || dbus_error_is_set (&error))) {

            fprintf(stderr, "WARNING: Failed to parse NameOwnerChanged signal: %s\n", error.message);
            goto fail;
        }

        if (strcmp(name, AVAHI_DBUS_NAME) == 0)

            /* Regardless if the server lost or acquired its name or
             * if the name was transfered: our services are no longer
             * available, so we disconnect ourselves */
            
            client_set_state(client, AVAHI_CLIENT_DISCONNECTED);

    } else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_SERVER, "StateChanged")) {
        int32_t state;
        
        if (!(dbus_message_get_args(
                  message, &error,
                  DBUS_TYPE_INT32, &state,
                  DBUS_TYPE_INVALID) || dbus_error_is_set (&error))) {
            fprintf(stderr, "WARNING: Failed to parse Server.StateChanged signal: %s\n", error.message);
            goto fail;
        }
            
        client_set_state(client, (AvahiClientState) state);

    } else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "StateChanged")) {
        const char *path;
        AvahiEntryGroup *g;
        path = dbus_message_get_path(message);

        for (g = client->groups; g; g = g->groups_next)
            if (strcmp(g->path, path) == 0)
                break;
        
        if (g) {
            int32_t state;
            if (!(dbus_message_get_args (message, &error, DBUS_TYPE_INT32, &state, DBUS_TYPE_INVALID)) ||
                dbus_error_is_set(&error)) {
                fprintf(stderr, "WARNING: Failed to parse EntryGroup.StateChanged signal: %s\n", error.message);
                goto fail;
            }
            
            avahi_entry_group_set_state(g, state);
        }
        
    } else if (dbus_message_is_signal(message, AVAHI_DBUS_INTERFACE_DOMAIN_BROWSER, "ItemNew"))
        return avahi_domain_browser_event(client, AVAHI_BROWSER_NEW, message);
    else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_DOMAIN_BROWSER, "ItemRemove")) 
        return avahi_domain_browser_event(client, AVAHI_BROWSER_REMOVE, message);

    else if (dbus_message_is_signal(message, AVAHI_DBUS_INTERFACE_SERVICE_TYPE_BROWSER, "ItemNew")) 
        return avahi_service_type_browser_event (client, AVAHI_BROWSER_NEW, message);
    else if (dbus_message_is_signal(message, AVAHI_DBUS_INTERFACE_SERVICE_TYPE_BROWSER, "ItemRemove")) 
        return avahi_service_type_browser_event (client, AVAHI_BROWSER_REMOVE, message);

    else if (dbus_message_is_signal(message, AVAHI_DBUS_INTERFACE_SERVICE_BROWSER, "ItemNew")) 
        return avahi_service_browser_event (client, AVAHI_BROWSER_NEW, message);
    else if (dbus_message_is_signal(message, AVAHI_DBUS_INTERFACE_SERVICE_BROWSER, "ItemRemove")) 
        return avahi_service_browser_event (client, AVAHI_BROWSER_REMOVE, message);

    return DBUS_HANDLER_RESULT_HANDLED;

fail:

    dbus_error_free (&error);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int get_server_state(AvahiClient *client, int *ret_error) {
    DBusMessage *message, *reply;
    DBusError error;
    int32_t state;
    int e;
    
    assert(client);

    dbus_error_init(&error);

    if (!(message = dbus_message_new_method_call(AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, "GetState")))
        goto fail;

    reply = dbus_connection_send_with_reply_and_block (client->bus, message, -1, &error);
    dbus_message_unref(message);

    if (!reply)
        goto fail;

    if (!(dbus_message_get_args(reply, &error, DBUS_TYPE_INT32, &state, DBUS_TYPE_INVALID)))
        goto fail;

    client_set_state(client, (AvahiServerState) state);

    return AVAHI_OK;

fail:
    if (dbus_error_is_set(&error)) {
        e = avahi_error_dbus_to_number (error.name);
        dbus_error_free(&error);
    } else
        e = AVAHI_ERR_NO_MEMORY;

    if (ret_error)
        *ret_error = e;
        
    return e;
}

/* This function acts like dbus_bus_get but creates a private
 * connection instead */
static DBusConnection*
avahi_dbus_bus_get (DBusBusType type, DBusError *error)
{
    DBusConnection *conn;
    char *env_addr;

    env_addr = getenv ("DBUS_SYSTEM_BUS_ADDRESS");

    if (env_addr == NULL || (strcmp (env_addr, "") == 0))
    {
        env_addr = DBUS_SYSTEM_BUS_DEFAULT_ADDRESS;
    }

    conn = dbus_connection_open_private (env_addr, error);

    if (!conn)
    {
        printf ("Failed to open private connection: %s\n", error->message);
        return NULL;
    }

    dbus_connection_set_exit_on_disconnect (conn, TRUE);

    if (!dbus_bus_register (conn, error))
    {
        printf ("Failed to register connection\n");
        dbus_connection_close (conn);
        dbus_connection_unref (conn);

        return NULL;
    }

    return conn;
}

AvahiClient *avahi_client_new(const AvahiPoll *poll_api, AvahiClientCallback callback, void *userdata, int *ret_error) {
    AvahiClient *client = NULL;
    DBusError error;

    dbus_error_init (&error);

    if (!(client = avahi_new(AvahiClient, 1))) {
        if (ret_error)
            *ret_error = AVAHI_ERR_NO_MEMORY;
        goto fail;
    }

    client->poll_api = poll_api;
    client->error = AVAHI_OK;
    client->callback = callback;
    client->userdata = userdata;
    client->state = AVAHI_CLIENT_DISCONNECTED;

    client->host_name = NULL;
    client->host_name_fqdn = NULL;
    client->domain_name = NULL;
    client->version_string = NULL;

    AVAHI_LLIST_HEAD_INIT(AvahiEntryGroup, client->groups);
    AVAHI_LLIST_HEAD_INIT(AvahiDomainBrowser, client->domain_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceBrowser, client->service_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceTypeBrowser, client->service_type_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceResolver, client->service_resolvers);

    if (!(client->bus = avahi_dbus_bus_get(DBUS_BUS_SYSTEM, &error)) ||
        dbus_error_is_set (&error))
        goto fail;

    if (avahi_dbus_connection_glue(client->bus, poll_api) < 0) {
        if (ret_error)
            *ret_error = AVAHI_ERR_NO_MEMORY; /* Not optimal */
        goto fail;
    }

    if (!dbus_connection_add_filter (client->bus, filter_func, client, NULL)) {
        if (ret_error)
            *ret_error = AVAHI_ERR_NO_MEMORY; 
        goto fail;
    }
        
    dbus_bus_add_match(
        client->bus,
        "type='signal', "
        "interface='" AVAHI_DBUS_INTERFACE_SERVER "', "
        "sender='" AVAHI_DBUS_NAME "', "
        "path='" AVAHI_DBUS_PATH_SERVER "'",
        &error);

    if (dbus_error_is_set (&error))
        goto fail;

    dbus_bus_add_match (
        client->bus,
        "type='signal', "
        "interface='" DBUS_INTERFACE_DBUS "', "
        "sender='" DBUS_SERVICE_DBUS "', "
        "path='" DBUS_PATH_DBUS "'",
        &error);

    if (dbus_error_is_set (&error))
        goto fail;

        dbus_bus_add_match (
        client->bus,
        "type='signal', "
        "interface='" DBUS_INTERFACE_LOCAL "'",
        &error);

    if (dbus_error_is_set (&error))
        goto fail;

    if (!(dbus_bus_name_has_owner(client->bus, AVAHI_DBUS_NAME, &error)) ||
        dbus_error_is_set(&error)) {

        if (ret_error)
            *ret_error = AVAHI_ERR_NO_DAEMON;
        
        goto fail;
    }

    if (get_server_state(client, ret_error) < 0)
        goto fail;

    return client;

fail:

    if (client)
        avahi_client_free(client);

    if (dbus_error_is_set(&error)) {

        if (ret_error)
            *ret_error = avahi_error_dbus_to_number(error.name);
        
        dbus_error_free(&error);
    }
        
    return NULL;
}

void avahi_client_free(AvahiClient *client) {
    assert(client);

    while (client->groups)
        avahi_entry_group_free(client->groups);

    while (client->domain_browsers)
        avahi_domain_browser_free(client->domain_browsers);

    while (client->service_browsers)
        avahi_service_browser_free(client->service_browsers);

    while (client->service_type_browsers)
        avahi_service_type_browser_free(client->service_type_browsers);

    while (client->service_resolvers)
        avahi_service_resolver_free(client->service_resolvers);

    if (client->bus) {
        dbus_connection_disconnect(client->bus);
        dbus_connection_unref(client->bus);
    }

    avahi_free(client->version_string);
    avahi_free(client->host_name);
    avahi_free(client->host_name_fqdn);
    avahi_free(client->domain_name);
    
    avahi_free(client);
}

static char* avahi_client_get_string_reply_and_block (AvahiClient *client, const char *method, const char *param) {
    DBusMessage *message = NULL, *reply = NULL;
    DBusError error;
    char *ret, *n;

    assert(client);
    assert(method);

    dbus_error_init (&error);

    if (!(message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, method))) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    if (param) {
        if (!dbus_message_append_args (message, DBUS_TYPE_STRING, &param, DBUS_TYPE_INVALID)) {
            avahi_client_set_errno (client, AVAHI_ERR_NO_MEMORY);
            goto fail;
        }
    }
    
    reply = dbus_connection_send_with_reply_and_block (client->bus, message, -1, &error);

    if (!reply || dbus_error_is_set (&error))
        goto fail;

    if (!dbus_message_get_args (reply, &error, DBUS_TYPE_STRING, &ret, DBUS_TYPE_INVALID) ||
        dbus_error_is_set (&error))
        goto fail;
    
    if (!(n = avahi_strdup(ret))) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    dbus_message_unref(message);
    dbus_message_unref(reply);
    
    return n;

fail:

    if (message)
        dbus_message_unref(message);
    if (reply)
        dbus_message_unref(reply);
    
    if (dbus_error_is_set(&error)) {
        avahi_client_set_dbus_error(client, &error);
        dbus_error_free(&error);
    }

    return NULL;
}

const char* avahi_client_get_version_string (AvahiClient *client) {
    assert(client);

    if (client->state == AVAHI_CLIENT_DISCONNECTED) {
        avahi_client_set_errno(client, AVAHI_ERR_BAD_STATE);
        return NULL;
    }

    if (!client->version_string)
        client->version_string = avahi_client_get_string_reply_and_block(client, "GetVersionString", NULL);

    return client->version_string;
}

const char* avahi_client_get_domain_name (AvahiClient *client) {
    assert(client);

    if (client->state == AVAHI_CLIENT_DISCONNECTED) {
        avahi_client_set_errno(client, AVAHI_ERR_BAD_STATE);
        return NULL;
    }

    if (!client->domain_name)
        client->domain_name = avahi_client_get_string_reply_and_block(client, "GetDomainName", NULL);
    
    return client->domain_name;
}

const char* avahi_client_get_host_name (AvahiClient *client) {
    assert(client);
    
    if (client->state == AVAHI_CLIENT_DISCONNECTED) {
        avahi_client_set_errno(client, AVAHI_ERR_BAD_STATE);
        return NULL;
    }

    if (!client->host_name)
        client->host_name = avahi_client_get_string_reply_and_block(client, "GetHostName", NULL);
    
    return client->host_name;
}

const char* avahi_client_get_host_name_fqdn (AvahiClient *client) {
    assert(client);

    if (client->state == AVAHI_CLIENT_DISCONNECTED) {
        avahi_client_set_errno(client, AVAHI_ERR_BAD_STATE);
        return NULL;
    }
    
    if (!client->host_name_fqdn)
        client->host_name_fqdn = avahi_client_get_string_reply_and_block(client, "GetHostNameFqdn", NULL);

    return client->host_name_fqdn;
}

AvahiClientState avahi_client_get_state(AvahiClient *client) {
    assert(client);

    return client->state;
}

int avahi_client_errno(AvahiClient *client) {
    assert(client);
    
    return client->error;
}
