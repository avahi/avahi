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

void avahi_entry_group_state_change (AvahiEntryGroup *group, int state)
{
    if (group == NULL || group->callback == NULL)
        return;

    group->callback (group, state, group->user_data);
}

AvahiEntryGroup*
avahi_entry_group_new (AvahiClient *client, AvahiEntryGroupCallback callback, void *user_data)
{
    AvahiEntryGroup *tmp = NULL;
    DBusMessage *message = NULL, *reply;
    DBusError error;
    char *path;

    if (client == NULL)
        return NULL;
    
    dbus_error_init (&error);

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER,
            AVAHI_DBUS_INTERFACE_SERVER, "EntryGroupNew");

    reply = dbus_connection_send_with_reply_and_block (client->bus, message, -1, &error);

    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Error sending EntryGroupNew message: %s\n", error.message);
        dbus_error_free (&error);

        avahi_client_set_errno (client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    if (reply == NULL)
    {
        fprintf (stderr, "Got NULL reply from EntryGroupNew\n");

        avahi_client_set_errno (client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    dbus_message_get_args (reply, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID);

    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Failure parsing EntryGroupNew reply: %s\n", error.message);
        avahi_client_set_errno (client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    tmp = malloc (sizeof (AvahiEntryGroup));

    tmp->client = client;

    tmp->path = strdup (path);
    tmp->callback = callback;
    tmp->user_data = user_data;

    AVAHI_LLIST_PREPEND(AvahiEntryGroup, groups, client->groups, tmp);

    dbus_message_unref (message);

    avahi_client_set_errno (client, AVAHI_OK);
    return tmp;

fail:
    if (tmp) free (tmp);
    if (message) dbus_message_unref (message);
    return NULL;
}

int
avahi_entry_group_commit (AvahiEntryGroup *group)
{
    DBusMessage *message;
    DBusError error;

    dbus_error_init (&error);

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, group->path,
            AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "Commit");

    dbus_connection_send (group->client->bus, message, NULL);

    return avahi_client_set_errno (group->client, AVAHI_OK);
}

int
avahi_entry_group_reset (AvahiEntryGroup *group)
{
    DBusMessage *message;

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, group->path,
            AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "Reset");

    dbus_connection_send (group->client->bus, message, NULL);

    return avahi_client_set_errno (group->client, AVAHI_OK);
}

int
avahi_entry_group_get_state (AvahiEntryGroup *group)
{
    DBusMessage *message, *reply;
    DBusError error;
    int state;

    dbus_error_init (&error);

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, group->path,
            AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "GetState");

    reply = dbus_connection_send_with_reply_and_block (group->client->bus, message, -1, &error);

    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Error sending GetState message for %s EntryGroup: %s\n", group->path, error.message);
        dbus_error_free (&error);

        return avahi_client_set_errno (group->client, AVAHI_ERR_DBUS_ERROR);
    }

    dbus_message_get_args(message, &error, DBUS_TYPE_BOOLEAN, &state, DBUS_TYPE_INVALID);

    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Error parsing GetState reply for %s EntryGroup: %s\n", group->path, error.message);
        dbus_error_free (&error);

        return avahi_client_set_errno (group->client, AVAHI_ERR_DBUS_ERROR);
    }

    avahi_client_set_errno (group->client, AVAHI_OK);
    return state;
}

int
avahi_client_errno (AvahiClient *client)
{
    return client->error;
}

AvahiClient*
avahi_entry_group_get_client (AvahiEntryGroup *group)
{
    return group->client;
}

int
avahi_entry_group_is_empty (AvahiEntryGroup *group)
{
    return AVAHI_OK;
}

int
avahi_entry_group_add_service (AvahiEntryGroup *group,
                               AvahiIfIndex interface,
                               AvahiProtocol protocol,
                               const char *name,
                               const char *type,
                               const char *domain,
                               const char *host,
                               uint16_t port,
                               AvahiStringList *txt)
{
    DBusMessage *message;
    DBusMessageIter iter, sub;
    AvahiStringList *p;

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, group->path,
            AVAHI_DBUS_INTERFACE_ENTRY_GROUP, "AddService");

    if (!message)
    {
        dbus_message_unref (message);
        return avahi_client_set_errno (group->client, AVAHI_ERR_DBUS_ERROR);
    }

    if (!dbus_message_append_args (message, DBUS_TYPE_INT32, &interface, DBUS_TYPE_INT32, &protocol,
                DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &type, DBUS_TYPE_STRING, &domain,
                DBUS_TYPE_STRING, &host, DBUS_TYPE_UINT16, &port, DBUS_TYPE_INVALID))
    {
        dbus_message_unref (message);
        return avahi_client_set_errno (group->client, AVAHI_ERR_DBUS_ERROR);
    }
    
    dbus_message_iter_init_append(message, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_BYTE_AS_STRING, &sub);

    /* Assemble the AvahiStringList into an Array of Array of Bytes to send over dbus */
    for (p = txt; p != NULL; p = p->next) {
        DBusMessageIter sub2;
        const guint8 *data = p->text;

        dbus_message_iter_open_container(&sub, DBUS_TYPE_ARRAY, "y", &sub2);
        dbus_message_iter_append_fixed_array(&sub2, DBUS_TYPE_BYTE, &data, p->size);
        dbus_message_iter_close_container(&sub, &sub2);
    }

    dbus_message_iter_close_container(&iter, &sub);

    dbus_connection_send (group->client->bus, message, NULL);

    return avahi_client_set_errno (group->client, AVAHI_OK);
}

/* XXX: debug function */
char* avahi_entry_group_path (AvahiEntryGroup *group)
{
    if (group != NULL) return group->path;
    else return NULL;
}
