#include <avahi-client/client.h>

int
main (int argc, char *argv[])
{
	AvahiClient *avahi;

	avahi = avahi_client_new ();

	g_message ("Got server ID %d", avahi->serverid);
}
