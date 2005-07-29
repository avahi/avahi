#include <avahi-client/client.h>
#include <avahi-common/dbus.h>
#include <glib.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>

#include <stdlib.h>

AvahiClient *
avahi_client_new ()
{
    AvahiClient *tmp;

    tmp = malloc (sizeof (AvahiClient));

    return tmp;
}
