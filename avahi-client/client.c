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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <stdlib.h>

#include "client.h"
#include "internal.h"

int
avahi_client_set_errno (AvahiClient *client, int error)
{
    if (client == NULL) return error;

    client->error = error;

    return error;
}
    
static
void avahi_client_state_change (AvahiClient *client, int state)
{
    if (client == NULL || client->callback == NULL) 
        return;

    client->callback (client, state, client->user_data);
}

void
avahi_client_state_request_callback (DBusPendingCall *call, void *data)
{
    AvahiClient *client = data;
    DBusError error;
    DBusMessage *reply;
    int state, type;

    dbus_error_init (&error);

    reply = dbus_pending_call_steal_reply (call);

    type = dbus_message_get_type (reply);

    if (type == DBUS_MESSAGE_TYPE_METHOD_RETURN)
    {
        dbus_message_get_args (reply, &error, DBUS_TYPE_INT32, &state, DBUS_TYPE_INVALID);
        
        if (dbus_error_is_set (&error))
            return;
        
        avahi_client_state_change (client, state);
    } else if (type == DBUS_MESSAGE_TYPE_ERROR) {
        dbus_set_error_from_message (&error, reply);
    }

    dbus_pending_call_unref (call);
}

void
avahi_client_schedule_state_request (AvahiClient *client)
{
    DBusMessage *message;
    DBusPendingCall *pcall;

    if (client == NULL) return;

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, "GetState");

    dbus_connection_send_with_reply (client->bus, message, &pcall, -1);

    dbus_pending_call_set_notify (pcall, avahi_client_state_request_callback, client, NULL);
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
        gchar *name, *old, *new;
        dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &old, DBUS_TYPE_STRING, &new, DBUS_TYPE_INVALID);
        
        if (dbus_error_is_set (&error)) {
            dbus_error_free (&error);
            goto out;
        }

        if (strcmp (name, AVAHI_DBUS_NAME) == 0) {

            if (old == NULL && new != NULL) {
                avahi_client_state_change (client, AVAHI_CLIENT_RECONNECTED);
            } else if (old != NULL && new == NULL) {
                avahi_client_state_change (client, AVAHI_CLIENT_DISCONNECTED);
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
            DBusError error;
            dbus_error_init (&error);
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

AvahiClient *
avahi_client_new (AvahiClientCallback callback, void *user_data)
{
    AvahiClient *tmp = NULL;
    DBusError error;

    dbus_error_init (&error);

    if (!(tmp = malloc(sizeof(AvahiClient))))
        goto fail;

    AVAHI_LLIST_HEAD_INIT(AvahiEntryGroup, tmp->groups);
    AVAHI_LLIST_HEAD_INIT(AvahiDomainBrowser, tmp->domain_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceBrowser, tmp->service_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceTypeBrowser, tmp->service_type_browsers);
    
    tmp->bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

    if (dbus_error_is_set (&error)) {
        goto fail;
    }

    dbus_connection_setup_with_g_main (tmp->bus, NULL);
    dbus_connection_set_exit_on_disconnect (tmp->bus, FALSE);

    if (!dbus_connection_add_filter (tmp->bus, filter_func, tmp, NULL))
    {
        goto fail;
    }

    dbus_bus_add_match (tmp->bus,
            "type='signal', "
            "interface='" AVAHI_DBUS_INTERFACE_SERVER "', "
            "sender='" AVAHI_DBUS_NAME "', "
            "path='" AVAHI_DBUS_PATH_SERVER "'",
            &error);

    if (dbus_error_is_set (&error))
    {
        goto fail;
    }   

    dbus_bus_add_match (tmp->bus,
            "type='signal', "
            "interface='" DBUS_INTERFACE_DBUS "', "
            "sender='" DBUS_SERVICE_DBUS "', "
            "path='" DBUS_PATH_DBUS "'",
            &error);

    if (dbus_error_is_set (&error))
    {
        goto fail;
    }

    tmp->callback = callback;
    tmp->user_data = user_data;

    avahi_client_schedule_state_request (tmp);

    avahi_client_set_errno (tmp, AVAHI_OK);
    return tmp;

fail:
    free (tmp);

    if (dbus_error_is_set(&error))
        dbus_error_free(&error);
        
    return NULL;
}

static char*
avahi_client_get_string_reply_and_block (AvahiClient *client, char *method, char *param)
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

    new = strdup (ret);

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
