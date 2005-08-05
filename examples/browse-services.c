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

static GMainLoop *main_loop = NULL;

static void resolve_callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const gchar *name,const gchar *type,const gchar *domain, const gchar *host_name, const AvahiAddress *address, guint16 port, AvahiStringList *txt, gpointer userdata) {
    g_assert(r);

    /* Called whenever a service has been resolved successfully or timed out */

    if (event == AVAHI_RESOLVER_TIMEOUT)
        g_message("Failed to resolve service '%s' of type '%s' in domain '%s'.", name, type, domain);
    else {
        gchar a[128], *t;

        g_assert(event == AVAHI_RESOLVER_FOUND);
        
        g_message("Service '%s' of type '%s' in domain '%s':", name, type, domain);

        avahi_address_snprint(a, sizeof(a), address);
        t = avahi_string_list_to_string(txt);
        g_message("\t%s:%u (%s) TXT=%s", host_name, port, a, t);
        g_free(t);
    }

    avahi_service_resolver_free(r);
}

static void browse_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const gchar *name, const gchar *type, const gchar *domain, gpointer userdata) {
    g_assert(b);
    
    AvahiServer *s = userdata;

    /* Called whenever a new services becomes available on the LAN or is removed from the LAN */

    g_message("%s: service '%s' of type '%s' in domain '%s'",
              event == AVAHI_BROWSER_NEW ? "NEW" : "REMOVED",
              name,
              type,
              domain);

    /* If it's new, let's resolve it */
    if (event == AVAHI_BROWSER_NEW)
        
        /* We ignore the returned resolver object. In the callback function
        we free it. If the server is terminated before the callback
        function is called the server will free the resolver for us. */

        avahi_service_resolver_new(s, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, resolve_callback, s);
}

int main(int argc, char*argv[]) {
    AvahiServerConfig config;
    AvahiServer *server = NULL;
    AvahiServiceBrowser *sb;
    gint error;
    int ret = 1;

    /* Do not publish any local records */
    avahi_server_config_init(&config);
    config.publish_hinfo = FALSE;
    config.publish_addresses = FALSE;
    config.publish_workstation = FALSE;
    config.publish_domain = FALSE;
    
    /* Allocate a new server */
    server = avahi_server_new(NULL, &config, NULL, NULL, &error);

    /* Free the configuration data */
    avahi_server_config_free(&config);

    /* Check wether creating the server object succeeded */
    if (!server) {
        g_message("Failed to create server: %s", avahi_strerror(error));
        goto fail;
    }
    
    /* Create the service browser */
    sb = avahi_service_browser_new(server, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_http._tcp", NULL, browse_callback, server);
    
    /* Run the main loop */
    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);

    ret = 0;
    
fail:
    
    /* Cleanup things */
    if (sb)
        avahi_service_browser_free(sb);
    
    if (server)
        avahi_server_free(server);

    if (main_loop)
        g_main_loop_unref(main_loop);

    return ret;
}
