#include <avahi-client/client.h>
#include <avahi-common/dbus.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <stdlib.h>

struct _AvahiClientPriv
{
    DBusConnection *bus;
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

static gint _dbus_add_match (DBusConnection *bus, char *type, char *interface, char *sender, char *path)
{
    DBusError error;
    char *filter;

    g_assert (bus != NULL);

    dbus_error_init (&error);
    filter = g_strdup_printf ("type='%s', interface='%s', sender='%s', path='%s'", type, interface, sender, path);
    dbus_bus_add_match (bus, filter, &error);
    g_free (filter);

    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Error adding filter match: %s\n", error.message);
        dbus_error_free (&error);
        return FALSE;
    }

    return TRUE;
}

AvahiClient *
avahi_client_new ()
{
    AvahiClient *tmp;
    DBusError error;

    tmp = g_new (AvahiClient, 1);
    tmp->priv = g_new (AvahiClientPriv, 1);

    g_assert (tmp != NULL);
    g_assert (tmp->priv != NULL);
    
    dbus_error_init (&error);

    tmp->priv->bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

    dbus_connection_setup_with_g_main (tmp->priv->bus, NULL);

    if (dbus_error_is_set (&error)) {
        fprintf(stderr, "Error getting system d-bus: %s\n", error.message);
        dbus_error_free (&error);
        goto fail;
    }

    dbus_connection_set_exit_on_disconnect (tmp->priv->bus, FALSE);

    if (!dbus_connection_add_filter (tmp->priv->bus, filter_func, tmp, NULL))
    {
        fprintf (stderr, "Failed to add d-bus filter\n");
        goto fail;
    }

    if (!_dbus_add_match (tmp->priv->bus, "signal", AVAHI_DBUS_INTERFACE_SERVER, AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER)) goto fail;
    if (!_dbus_add_match (tmp->priv->bus, "signal", DBUS_INTERFACE_DBUS, DBUS_SERVICE_DBUS, DBUS_PATH_DBUS)) goto fail;

    return tmp;

fail:
    if (tmp->priv) free (tmp->priv);
    if (tmp) free (tmp);
    return NULL;
}

static char*
avahi_client_get_string_reply_and_block (AvahiClient *client, char *method, char *param)
{
    DBusMessage *message;
    DBusMessage *reply;
    DBusError error;
    char *ret;

    g_assert (client != NULL);
    g_assert (method != NULL);

    dbus_error_init (&error);

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, method);
    fprintf (stderr, "message = dbus_message_new_method_call (%s, %s, %s, %s)\n", AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, method);

    if (param != NULL)
    {
        if (!dbus_message_append_args (message, DBUS_TYPE_STRING, &param, DBUS_TYPE_INVALID))
        {
            fprintf (stderr, "Failed to append string argument to %s message\n", method);
            return NULL;
        }
    }
    
    reply = dbus_connection_send_with_reply_and_block (client->priv->bus, message, -1, &error);

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

    return ret;
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

char*
avahi_client_get_alternative_host_name (AvahiClient *client, char *host)
{
    return avahi_client_get_string_reply_and_block (client, "GetAlternativeHostName", host);
}

char*
avahi_client_get_alternative_service_name (AvahiClient *client, char *service)
{
    return avahi_client_get_string_reply_and_block (client, "GetAlternativeServiceName", service);
}
