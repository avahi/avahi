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

#include <avahi-core/core.h>

#include "main.h"
#include "simple-protocol.h"
#include "static-services.h"

#define DBUS_SERVICE_AVAHI "org.freedesktop.Avahi"

AvahiServer *avahi_server = NULL;

static void server_callback(AvahiServer *s, AvahiServerState state, gpointer userdata) {
    g_assert(s);

    if (state == AVAHI_SERVER_RUNNING) {
        g_message("Server startup complete.  Host name is <%s>", avahi_server_get_host_name_fqdn(s));
        static_service_add_to_server();
    } else if (state == AVAHI_SERVER_COLLISION) {
        gchar *n;

        static_service_remove_from_server();
        
        n = avahi_alternative_host_name(avahi_server_get_host_name(s));
        g_message("Host name conflict, retrying with <%s>", n);
        avahi_server_set_host_name(s, n);
        g_free(n);
    }
}

int main(int argc, char *argv[]) {
    GMainLoop *loop = NULL;
    gint r = 255;
    AvahiServerConfig config;

    avahi_server_config_init(&config);

    loop = g_main_loop_new(NULL, FALSE);

    if (simple_protocol_setup(NULL) < 0)
        goto finish;

#ifdef ENABLE_DBUS
    if (dbus_protocol_setup () < 0)
        goto finish;
#endif

    if (!(avahi_server = avahi_server_new(NULL, &config, server_callback, NULL)))
        goto finish;

    static_service_load();

    g_main_loop_run(loop);

    r = 0;
    
finish:

    static_service_remove_from_server();
    static_service_free_all();
    
    simple_protocol_shutdown();

#ifdef ENABLE_DBUS
    dbus_protocol_shutdown();
#endif

    if (avahi_server)
        avahi_server_free(avahi_server);
    
    if (loop)
        g_main_loop_unref(loop);

    avahi_server_config_free(&config);

    return r;
}
