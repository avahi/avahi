#include <avahi-client/client.h>

AvahiClient *
avahi_client_new ()
{
	AvahiClient *tmp;

	tmp = g_new0 (AvahiClient, 1);

	return tmp;
}
