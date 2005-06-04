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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include <avahi-core/core.h>

int main(int argc, char*argv[]) {
    AvahiServer *server;
    AvahiServerConfig config;
    GMainLoop *loop;

    avahi_server_config_init(&config);
    config.register_hinfo = FALSE;
    config.register_addresses = FALSE;
    config.register_workstation = FALSE;
    config.announce_domain = FALSE;
    config.use_ipv6 = FALSE;
    config.enable_reflector = TRUE;
    
    server = avahi_server_new(NULL, &config, NULL, NULL);
    avahi_server_config_free(&config);

    loop = g_main_loop_new(NULL, FALSE);
    
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    avahi_server_free(server);
}
