#include <avahi-client/client.h>
#include <stdio.h>
#include <glib.h>

int
main (int argc, char *argv[])
{
    GMainLoop *loop;
    AvahiClient *avahi;
    char *ret;

    loop = g_main_loop_new (NULL, FALSE);
    
    avahi = avahi_client_new ();

    g_assert (avahi != NULL);

    ret = avahi_client_get_version_string (avahi);
    printf ("Avahi Server Version: %s\n", ret);

    ret = avahi_client_get_host_name (avahi);
    printf ("Host Name: %s\n", ret);

    ret = avahi_client_get_alternative_host_name (avahi, "ubuntu");
    printf ("Alternative Host Name: %s\n", ret);
    
    g_free (avahi);

    g_main_loop_run (loop);

    return 0;
}
