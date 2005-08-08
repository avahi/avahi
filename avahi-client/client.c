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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <stdlib.h>

struct _AvahiClient
{
    DBusConnection *bus;
    AVAHI_LLIST_HEAD(AvahiEntryGroup, groups);
};

struct _AvahiEntryGroup {
    char *path;
    AvahiClient *parent;
    AVAHI_LLIST_FIELDS(AvahiEntryGroup, groups);
};

static DBusHandlerResult
filter_func (DBusConnection *bus, DBusMessage *message, void *data)
{
    DBusError error;
    
    g_assert (bus != NULL);
    g_assert (message != NULL);

    printf ("dbus: interface=%s, path=%s, member=%s\n",
            dbus_message_get_interface (message),
            dbus_message_get_path (message),
            dbus_message_get_member (message));

    dbus_error_init (&error);

    if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
        gchar *name, *old, *new;
        dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &old, DBUS_TYPE_STRING, &new, DBUS_TYPE_INVALID);
        
        if (dbus_error_is_set (&error)) {
            fprintf(stderr, "Failed to parse NameOwnerChanged message: %s", error.message);
            dbus_error_free (&error);
            goto out;
        }

        if (strcmp (name, AVAHI_DBUS_NAME) == 0) {
            if (old == NULL && new != NULL) {
                fprintf(stderr, "Avahi Daemon connected\n");
            } else if (old != NULL && new == NULL) {
                fprintf(stderr, "Avahi Daemon disconnected\n");
            }
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

out: 
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

AvahiClient *
avahi_client_new ()
{
    AvahiClient *tmp;
    DBusError error;

    tmp = g_new (AvahiClient, 1);

    if (tmp == NULL)
        goto fail;

    AVAHI_LLIST_HEAD_INIT(AvahiEntryGroup, tmp->groups);

    dbus_error_init (&error);

    tmp->bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

    dbus_connection_setup_with_g_main (tmp->bus, NULL);

    if (dbus_error_is_set (&error)) {
        fprintf(stderr, "Error getting system d-bus: %s\n", error.message);
        dbus_error_free (&error);
        goto fail;
    }

    dbus_connection_set_exit_on_disconnect (tmp->bus, FALSE);

    if (!dbus_connection_add_filter (tmp->bus, filter_func, tmp, NULL))
    {
        fprintf (stderr, "Failed to add d-bus filter\n");
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
        fprintf (stderr, "Error adding filter match: %s\n", error.message);
        dbus_error_free (&error);
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
        fprintf (stderr, "Error adding filter match: %s\n", error.message);
        dbus_error_free (&error);
        goto fail;

    }   

    return tmp;

fail:
    if (tmp) free (tmp);
    return NULL;
}

static char*
avahi_client_get_string_reply_and_block (AvahiClient *client, char *method, char *param)
{
    DBusMessage *message;
    DBusMessage *reply;
    DBusError error;
    char *ret, *new;

    g_assert (client != NULL);
    g_assert (method != NULL);

    dbus_error_init (&error);

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, method);

    if (param != NULL)
    {
        if (!dbus_message_append_args (message, DBUS_TYPE_STRING, &param, DBUS_TYPE_INVALID))
        {
            fprintf (stderr, "Failed to append string argument to %s message\n", method);
            return NULL;
        }
    }
    
    reply = dbus_connection_send_with_reply_and_block (client->bus, message, -1, &error);

    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Error sending %s message: %s\n", method, error.message);
        dbus_error_free (&error);
        dbus_message_unref (message);
        return NULL;
    }

    if (reply == NULL)
    {
        dbus_message_unref (message);
        fprintf (stderr, "Could not connect to Avahi daemon\n");
        return NULL;
    }

    dbus_message_get_args (reply, &error, DBUS_TYPE_STRING, &ret, DBUS_TYPE_INVALID);

    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Failed to parse %s reply: %s\n", method, error.message);
        dbus_error_free (&error);
        return NULL;
    }

    new = strdup (ret);

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

AvahiEntryGroup*
avahi_entry_group_new (AvahiClient *client)
{
    AvahiEntryGroup *tmp;

    tmp = malloc (sizeof (AvahiEntryGroup));

    tmp->parent = client;

    AVAHI_LLIST_PREPEND(AvahiEntryGroup, groups, client->groups, tmp);

    return tmp;
fail:
    if (tmp) free (tmp);
    return NULL;
}
