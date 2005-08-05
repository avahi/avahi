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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <avahi-core/core.h>

static AvahiEntryGroup *group = NULL;
static GMainLoop *main_loop = NULL;
static gchar *name = NULL;

static void create_services(AvahiServer *s);

static void entry_group_callback(AvahiServer *s, AvahiEntryGroup *g, AvahiEntryGroupState state, gpointer userdata) {
    g_assert(s);
    g_assert(g == group);

    /* Called whenever the entry group state changes */

    if (state == AVAHI_ENTRY_GROUP_ESTABLISHED)
        /* The entry group has been established successfully */
        g_message("Service '%s' successfully established.", name);
    
    else if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        gchar *n;

        /* A service name collision happened. Let's pick a new name */
        n = avahi_alternative_service_name(name);
        g_free(name);
        name = n;

        g_message("Service name collision, renaming service to '%s'", name);

        /* And recreate the services */
        create_services(s);
    }
}

static void create_services(AvahiServer *s) {
    gchar r[128];
    gint ret;
    g_assert(s);

    /* If this is the first time we're called, let's create a new entry group */
    if (!group) {
        if (!(group = avahi_entry_group_new(s, entry_group_callback, NULL))) {
            g_message("avahi_entry_group_new() failed: %s", avahi_strerror(avahi_server_errno(s)));
            goto fail;
        }
    }
    
    g_message("Adding service '%s'", name);

    /* Create some random TXT data */
    snprintf(r, sizeof(r), "random=%i", rand());

    /* Add the service for IPP */
    if ((ret = avahi_server_add_service(s, group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, name, "_ipp._tcp", NULL, NULL, 651, "test=blah", r, NULL)) < 0) {
        g_message("Failed to add _ipp._tcp service: %s", avahi_strerror(ret));
        goto fail;
    }

    /* Add the same service for BSD LPR */
    if ((ret = avahi_server_add_service(s, group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, name, "_printer._tcp", NULL, NULL, 515, NULL)) < 0) {
        g_message("Failed to add _printer._tcp service: %s", avahi_strerror(ret));
        goto fail;
    }

    /* Tell the server to register the service */
    if ((ret = avahi_entry_group_commit(group)) < 0) {
        g_message("Failed to commit entry_group: %s", avahi_strerror(ret));
        goto fail;
    }

    return;

fail:
    g_main_loop_quit(main_loop);
    return;
}

static void server_callback(AvahiServer *s, AvahiServerState state, gpointer userdata) {
    g_assert(s);

    /* Called whenever the server state changes */

    if (state == AVAHI_SERVER_RUNNING)
        /* The serve has startup successfully and registered its host
         * name on the network, so it's time to create our services */
        create_services(s);
    
    else if (state == AVAHI_SERVER_COLLISION) {
        gchar *n;
        gint r;
        
        /* A host name collision happened. Let's pick a new name for the server */
        n = avahi_alternative_host_name(avahi_server_get_host_name(s));
        g_message("Host name collision, retrying with '%s'", n);
        r = avahi_server_set_host_name(s, n);
        g_free(n);

        if (r < 0) {
            g_message("Failed to set new host name: %s", avahi_strerror(r));

            g_main_loop_quit(main_loop);
            return;
        }

        /* Let's drop our registered services. When the server is back
         * in AVAHI_SERVER_RUNNING state we will register them
         * again with the new host name. */
        if (group)
            avahi_entry_group_reset(group);
    }
}

int main(int argc, char*argv[]) {
    AvahiServerConfig config;
    AvahiServer *server = NULL;
    gint error;
    int ret = 1;
    
    srand(time(NULL));
    
    name = g_strdup("MegaPrinter");

    /* Let's set the host name for this server. */
    avahi_server_config_init(&config);
    config.host_name = g_strdup("gurkiman");
    config.publish_workstation = FALSE;
    
    /* Allocate a new server */
    server = avahi_server_new(NULL, &config, server_callback, NULL, &error);

    /* Free the configuration data */
    avahi_server_config_free(&config);

    /* Check wether creating the server object succeeded */
    if (!server) {
        g_message("Failed to create server: %s", avahi_strerror(error));
        goto fail;
    }
    
    /* Run the main loop */
    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);

    ret = 0;
    
fail:
    
    /* Cleanup things */
    if (group)
        avahi_entry_group_free(group);

    if (server)
        avahi_server_free(server);

    if (main_loop)
        g_main_loop_unref(main_loop);

    g_free(name);
    
    return ret;
}
