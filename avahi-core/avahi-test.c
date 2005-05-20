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

#include "core.h"
#include "alternative.h"

static AvahiEntryGroup *group = NULL;
static AvahiServer *server = NULL;
static gchar *service_name = NULL;

static gboolean quit_timeout(gpointer data) {
    g_main_loop_quit(data);
    return FALSE;
}

static gboolean dump_timeout(gpointer data) {
    AvahiServer *Avahi = data;
    avahi_server_dump(Avahi, stdout);
    return TRUE;
}

static void subscription(AvahiSubscription *s, AvahiRecord *r, gint interface, guchar protocol, AvahiSubscriptionEvent event, gpointer userdata) {
    gchar *t;
    
    g_assert(s);
    g_assert(r);
    g_assert(interface > 0);
    g_assert(protocol != AF_UNSPEC);

    g_message("SUBSCRIPTION: record [%s] on %i.%i is %s", t = avahi_record_to_string(r), interface, protocol,
              event == AVAHI_SUBSCRIPTION_NEW ? "new" : "removed");

    g_free(t);
}


static void remove_entries(void);
static void create_entries(gboolean new_name);

static void entry_group_callback(AvahiServer *s, AvahiEntryGroup *g, AvahiEntryGroupState state, gpointer userdata) {
    g_message("=======> entry group state: %i", state);

    if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        remove_entries();
        create_entries(TRUE);
    }
}

static void server_callback(AvahiServer *s, AvahiServerState state, gpointer userdata) {
    g_message("=======> server state: %i", state);

    if (state == AVAHI_SERVER_RUNNING)
        create_entries(FALSE);
    else if (state == AVAHI_SERVER_COLLISION) {
        gchar *n;
        remove_entries();

        n = avahi_alternative_host_name(avahi_server_get_host_name(s));
        avahi_server_set_host_name(s, n);
        g_free(n);
    }
}

static void remove_entries(void) {
    if (group)
        avahi_entry_group_free(group);

    group = NULL;
}

static void create_entries(gboolean new_name) {
    remove_entries();
    
    group = avahi_entry_group_new(server, entry_group_callback, NULL);   
    
    if (!service_name)
        service_name = g_strdup("Test Service");
    else if (new_name) {
        gchar *n = avahi_alternative_service_name(avahi_server_get_host_name(server));
        g_free(service_name);
        service_name = n;
    }
    
    avahi_server_add_service(server, group, 0, AF_UNSPEC, "_http._tcp", service_name, NULL, NULL, 80, "foo", NULL);
    avahi_server_add_service(server, group, 0, AF_UNSPEC, "_ftp._tcp", service_name, NULL, NULL, 21, "foo", NULL);   
    avahi_server_add_service(server, group, 0, AF_UNSPEC, "_webdav._tcp", service_name, NULL, NULL, 80, "foo", NULL);   
    
    avahi_entry_group_commit(group);   

}
int main(int argc, char *argv[]) {
    GMainLoop *loop = NULL;
/*     AvahiSubscription *s; */
/*     AvahiKey *k; */
    
    server = avahi_server_new(NULL, NULL, server_callback, NULL);

/*     k = avahi_key_new("HALLO", AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT); */
/*     s = avahi_subscription_new(avahi, k, 0, AF_UNSPEC, subscription, NULL); */
/*     avahi_key_unref(k); */

    loop = g_main_loop_new(NULL, FALSE);
    
    g_timeout_add(1000*5, dump_timeout, server); 
/*     g_timeout_add(1000*30, quit_timeout, loop);    */
    
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

/*     avahi_subscription_free(s);  */

    if (group)
        avahi_entry_group_free(group);   

    if (server)
        avahi_server_free(server);

    g_free(service_name);
    
    return 0;
}
