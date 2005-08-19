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

#include <avahi-client/client.h>
#include <avahi-common/dbus.h>
#include <avahi-common/llist.h>
#include <avahi-common/error.h>
#include <avahi-common/dbus.h>
#include <avahi-common/malloc.h>
#include <avahi-common/dbus-watch-glue.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dbus/dbus.h>

#include <stdlib.h>

#include "client.h"
#include "internal.h"

int avahi_client_set_errno (AvahiClient *client, int error) {
    assert(client);

    return client->error = error;
}
    
static void client_set_state (AvahiClient *client, AvahiServerState state) {
    assert(state);

    if (client->state == state)
        return;

    client->state = state;

    if (client->callback)
        client->callback (client, state, client->userdata);
}

static DBusHandlerResult
filter_func (DBusConnection *bus, DBusMessage *message, void *data)
{
    AvahiClient *client = data;
    DBusError error;
    
    printf ("dbus: interface=%s, path=%s, member=%s\n",
            dbus_message_get_interface (message),
            dbus_message_get_path (message),
            dbus_message_get_member (message));

    dbus_error_init (&error);

    if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
        char *name, *old, *new;
        dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &old, DBUS_TYPE_STRING, &new, DBUS_TYPE_INVALID);
        
        if (dbus_error_is_set (&error)) {
            dbus_error_free (&error);
            goto out;
        }

        if (strcmp (name, AVAHI_DBUS_NAME) == 0) {

            if (old == NULL && new != NULL) {
                client_set_state (client, AVAHI_CLIENT_RECONNECTED);
            } else if (old != NULL && new == NULL) {
                client_set_state (client, AVAHI_CLIENT_DISCONNECTED);
                /* XXX: we really need to expire all entry groups */
            }
        }
    } else if (dbus_message_is_signal (message, AVAHI_DBUS_NAME, "StateChanged")) {
        /* XXX: todo */
        printf ("server statechange\n");
    } else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "StateChanged")) {
        const char *path;
        AvahiEntryGroup *n, *group = NULL;
        path = dbus_message_get_path (message);

        for (n = client->groups; n != NULL; n = n->groups_next)
        {
            if (strcmp (n->path, path) == 0)
            {
                group = n;
                break;
            }
        }
        
        if (group != NULL) {
            int state;
            dbus_message_get_args (message, &error, DBUS_TYPE_INT32, &state, DBUS_TYPE_INVALID);
            if (dbus_error_is_set (&error))
                goto out;
            
            avahi_entry_group_state_change (group, state);
        }
    } else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_DOMAIN_BROWSER, "ItemNew")) {
        return avahi_domain_browser_event (client, AVAHI_BROWSER_NEW, message);
    } else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_DOMAIN_BROWSER, "ItemRemove")) {
        return avahi_domain_browser_event (client, AVAHI_BROWSER_REMOVE, message);
    } else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_SERVICE_TYPE_BROWSER, "ItemNew")) {
        return avahi_service_type_browser_event (client, AVAHI_BROWSER_NEW, message);
    } else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_SERVICE_TYPE_BROWSER, "ItemRemove")) {
        return avahi_service_type_browser_event (client, AVAHI_BROWSER_REMOVE, message);
    } else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_SERVICE_BROWSER, "ItemNew")) {
        return avahi_service_browser_event (client, AVAHI_BROWSER_NEW, message);
    } else if (dbus_message_is_signal (message, AVAHI_DBUS_INTERFACE_SERVICE_BROWSER, "ItemRemove")) {
        return avahi_service_browser_event (client, AVAHI_BROWSER_REMOVE, message);
    }

    return DBUS_HANDLER_RESULT_HANDLED;

out: 
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int translate_dbus_error(const DBusError *error) {
    assert(error);

    return avahi_error_dbus_to_number (error->name);
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
        e = translate_dbus_error(&error);
        dbus_error_free(&error);
    } else
        e = AVAHI_ERR_NO_MEMORY;

    if (ret_error)
        *ret_error = e;
        
    return e;
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
    client->state = AVAHI_SERVER_INVALID;

    AVAHI_LLIST_HEAD_INIT(AvahiEntryGroup, client->groups);
    AVAHI_LLIST_HEAD_INIT(AvahiDomainBrowser, client->domain_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceBrowser, client->service_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceTypeBrowser, client->service_type_browsers);

    client->bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set (&error)) {
        if (ret_error)
            *ret_error = translate_dbus_error(&error);
        goto fail;
    }

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

    if (dbus_error_is_set (&error)) {
        if (ret_error)
            *ret_error = translate_dbus_error(&error);
        goto fail;
    }   

    dbus_bus_add_match (
        client->bus,
        "type='signal', "
        "interface='" DBUS_INTERFACE_DBUS "', "
        "sender='" DBUS_SERVICE_DBUS "', "
        "path='" DBUS_PATH_DBUS "'",
        &error);

    if (dbus_error_is_set (&error)) {
        if (ret_error)
            *ret_error = translate_dbus_error(&error);
        goto fail;
    }

    if (!(dbus_bus_name_has_owner(client->bus, AVAHI_DBUS_NAME, &error))) {

        if (dbus_error_is_set (&error)) {
            if (ret_error)
                *ret_error = translate_dbus_error(&error);
            goto fail;
        }
        
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

    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
        
    return NULL;
}

void avahi_client_free(AvahiClient *client) {
    assert(client);

    if (client->bus) {
        dbus_connection_disconnect(client->bus);
        dbus_connection_unref(client->bus);
    }

    while (client->groups)
        avahi_entry_group_free(client->groups);

    while (client->domain_browsers)
        avahi_domain_browser_free(client->domain_browsers);

    while (client->service_browsers)
        avahi_service_browser_free(client->service_browsers);

    while (client->service_type_browsers)
        avahi_service_type_browser_free(client->service_type_browsers);
    
    avahi_free(client);
}

static char*
avahi_client_get_string_reply_and_block (AvahiClient *client, const char *method, const char *param)
{
    DBusMessage *message;
    DBusMessage *reply;
    DBusError error;
    char *ret, *new;

    if (client == NULL || method == NULL) return NULL;

    dbus_error_init (&error);

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, method);

    if (param != NULL)
    {
        if (!dbus_message_append_args (message, DBUS_TYPE_STRING, &param, DBUS_TYPE_INVALID))
        {
            avahi_client_set_errno (client, AVAHI_ERR_DBUS_ERROR);
            return NULL;
        }
    }
    
    reply = dbus_connection_send_with_reply_and_block (client->bus, message, -1, &error);

    if (dbus_error_is_set (&error))
    {
        dbus_error_free (&error);
        dbus_message_unref (message);

        avahi_client_set_errno (client, AVAHI_ERR_DBUS_ERROR);
        return NULL;
    }

    if (reply == NULL)
    {
        dbus_message_unref (message);

        avahi_client_set_errno (client, AVAHI_ERR_DBUS_ERROR);
        return NULL;
    }

    dbus_message_get_args (reply, &error, DBUS_TYPE_STRING, &ret, DBUS_TYPE_INVALID);

    if (dbus_error_is_set (&error))
    {
        dbus_error_free (&error);

        avahi_client_set_errno (client, AVAHI_ERR_DBUS_ERROR);
        return NULL;
    }

    new = avahi_strdup (ret);

    avahi_client_set_errno (client, AVAHI_OK);
    return new;
}

char*
avahi_client_get_version_string (AvahiClient *client)
{
    return avahi_client_get_string_reply_and_block (client, "GetVersionString", NULL);
}

char*
avahi_client_get_domain_name (AvahiClient *client)
{
    return avahi_client_get_string_reply_and_block (client, "GetDomainName", NULL);
}

char*
avahi_client_get_host_name (AvahiClient *client)
{
    return avahi_client_get_string_reply_and_block (client, "GetHostName", NULL);
}

char*
avahi_client_get_host_name_fqdn (AvahiClient *client)
{
    return avahi_client_get_string_reply_and_block (client, "GetHostNameFqdn", NULL);
}
