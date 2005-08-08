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
    g_free (ret);

    ret = avahi_client_get_host_name (avahi);
    printf ("Host Name: %s\n", ret);
    g_free (ret);

    ret = avahi_client_get_domain_name (avahi);
    printf ("Domain Name: %s\n", ret);
    g_free (ret);

    ret = avahi_client_get_host_name_fqdn (avahi);
    printf ("FQDN: %s\n", ret);
    g_free (ret);

    g_free (avahi);

    g_main_loop_run (loop);

    return 0;
}
