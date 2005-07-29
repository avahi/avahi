#include <avahi-client/client.h>
#include <stdio.h>

int
main (int argc, char *argv[])
{
    AvahiClient *avahi;
    
    avahi = avahi_client_new ();

    if (avahi != NULL)
        free (avahi);

    return 0;
}
