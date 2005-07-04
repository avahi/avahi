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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
    
#include "dbus-protocol.h"

typedef struct Server Server;
typedef struct Client Client;

struct Client {
        int id;
};

struct Server {
        DBusConnection *bus;
        GSList *clients;
        int nextid;
};

static Server *server = NULL;

static DBusHandlerResult
do_register (DBusConnection *conn, DBusMessage *message)
{
    DBusError error;
    char *s;
    Client *client;
    DBusMessage *reply;
    DBusMessageIter iter;

    dbus_error_init (&error);

    dbus_message_get_args (message, &error,
                           DBUS_TYPE_STRING, &s,
                           DBUS_TYPE_INVALID);

    if (dbus_error_is_set (&error))
    {
        g_warning ("Error parsing register attempt");
        dbus_error_free (&error);
    } else {
            client = g_malloc (sizeof (Client));
            client->id = server->nextid;
            server->nextid++;

            server->clients = g_slist_append (server->clients, client);
            
            g_message ("Register received: idstring=(%s), dbus-id=(%s), client-id=(%d)", s, dbus_message_get_sender (message), client->id);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
signal_filter (DBusConnection *conn, DBusMessage *message, void *user_data)
{
    GMainLoop *loop = user_data;
    DBusError error;

    dbus_error_init (&error);

    g_message ("dbus: interface=%s, path=%s, member=%s",
               dbus_message_get_interface (message),
               dbus_message_get_path (message),
               dbus_message_get_member (message));

    if (dbus_message_is_signal (message,
                                    DBUS_INTERFACE_LOCAL,
                                "Disconnected"))
    {
        /* No, we shouldn't quit, but until we get somewhere
         * usefull such that we can restore our state, we will */
        g_warning ("Disconnnected from d-bus, terminating...");

        g_main_loop_quit (loop);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_method_call (message, DBUS_SERVICE_AVAHI,
                                            "Register"))
    {
        return do_register (conn, message);
    } else if (dbus_message_is_signal (message,
                                           DBUS_INTERFACE_DBUS,
                                       "NameAcquired"))
    {
        char *name;

        dbus_message_get_args (message, &error,
                               DBUS_TYPE_STRING, &name,
                               DBUS_TYPE_INVALID);

        if (dbus_error_is_set (&error))
        {
            g_warning ("Error parsing NameAcquired message");
            dbus_error_free (&error);

            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        g_message ("dbus: ServiceAcquired (%s)", name);

        return DBUS_HANDLER_RESULT_HANDLED;
    }

    g_message ("dbus: missed event");

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int
dbus_protocol_setup (GMainLoop *loop)
{
    DBusError error;

    dbus_error_init (&error);

    server = g_malloc (sizeof (server));

    server->clients = NULL;
    server->nextid = 1;

    server->bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

    if (server->bus == NULL)
    {
        g_warning ("dbus_bus_get(): %s", error.message);
        dbus_error_free (&error);

        return 1;
    }

    dbus_connection_setup_with_g_main (server->bus, NULL);
    dbus_connection_set_exit_on_disconnect (server->bus, FALSE);

    dbus_bus_request_name (server->bus, DBUS_SERVICE_AVAHI, 0, &error);

    if (dbus_error_is_set (&error))
    {
        g_warning ("dbus_error_is_set (): %s", error.message);
        dbus_error_free (&error);

            return 1;
    }

    dbus_connection_add_filter (server->bus, signal_filter, loop, NULL);
    dbus_bus_add_match (server->bus,
                        "type='method_call',interface='org.freedesktop.Avahi'",
                        &error);

    if (dbus_error_is_set (&error))
    {
        g_warning ("dbus_bus_add_match (): %s", error.message);
        dbus_error_free (&error);

            return 1;
    }

    return 0;
}

void
dbus_protocol_shutdown ()
{
    if (server->bus) {
        dbus_connection_disconnect(server->bus);
        dbus_connection_unref(server->bus);
    }
}
