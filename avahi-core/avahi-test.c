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

static void record_browser_callback(AvahiRecordBrowser *r, gint interface, guchar protocol, AvahiBrowserEvent event, AvahiRecord *record, gpointer userdata) {
    gchar *t;
    
    g_assert(r);
    g_assert(record);
    g_assert(interface > 0);
    g_assert(protocol != AF_UNSPEC);

    g_message("SUBSCRIPTION: record [%s] on %i.%i is %s", t = avahi_record_to_string(record), interface, protocol,
              event == AVAHI_BROWSER_NEW ? "new" : "remove");

    g_free(t);
}

static void remove_entries(void);
static void create_entries(gboolean new_name);

static void entry_group_callback(AvahiServer *s, AvahiEntryGroup *g, AvahiEntryGroupState state, gpointer userdata) {
    g_message("entry group state: %i", state); 

    if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        remove_entries();
        create_entries(TRUE);
        g_message("Service name conflict, retrying with <%s>", service_name);
    } else if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {
        g_message("Service established under name <%s>", service_name);
    }
}

static void server_callback(AvahiServer *s, AvahiServerState state, gpointer userdata) {

     g_message("server state: %i", state); 
    
    if (state == AVAHI_SERVER_RUNNING) {
        g_message("Server startup complete.  Host name is <%s>", avahi_server_get_host_name_fqdn(s));
        create_entries(FALSE);
    } else if (state == AVAHI_SERVER_COLLISION) {
        gchar *n;
        remove_entries();

        n = avahi_alternative_host_name(avahi_server_get_host_name(s));

        g_message("Host name conflict, retrying with <%s>", n);
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
        gchar *n = avahi_alternative_service_name(service_name);
        g_free(service_name);
        service_name = n;
    }
    
    avahi_server_add_service(server, group, 0, AF_UNSPEC, "_http._tcp", service_name, NULL, NULL, 80, "foo", NULL);
    avahi_server_add_service(server, group, 0, AF_UNSPEC, "_ftp._tcp", service_name, NULL, NULL, 21, "foo", NULL);   
    avahi_server_add_service(server, group, 0, AF_UNSPEC, "_webdav._tcp", service_name, NULL, NULL, 80, "foo", NULL);   
    
    avahi_entry_group_commit(group);   

}

static void hnr_callback(AvahiHostNameResolver *r, gint iface, guchar protocol, AvahiBrowserEvent event, const gchar *hostname, const AvahiAddress *a, gpointer userdata) {
    gchar t[64];

    if (a)
        avahi_address_snprint(t, sizeof(t), a);

    g_message("HNR: (%i.%i) <%s> -> %s [%s]", iface, protocol, hostname, a ? t : "n/a", event == AVAHI_RESOLVER_FOUND ? "found" : "timeout");
}

static void ar_callback(AvahiAddressResolver *r, gint iface, guchar protocol, AvahiBrowserEvent event, const AvahiAddress *a, const gchar *hostname, gpointer userdata) {
    gchar t[64];

    avahi_address_snprint(t, sizeof(t), a);

    g_message("AR: (%i.%i) %s -> <%s> [%s]", iface, protocol, t, hostname ? hostname : "n/a", event == AVAHI_RESOLVER_FOUND ? "found" : "timeout");
}

static void db_callback(AvahiDomainBrowser *b, gint iface, guchar protocol, AvahiBrowserEvent event, const gchar *domain, gpointer userdata) {

    g_message("DB: (%i.%i) <%s> [%s]", iface, protocol, domain, event == AVAHI_BROWSER_NEW ? "new" : "remove");
}

static void stb_callback(AvahiServiceTypeBrowser *b, gint iface, guchar protocol, AvahiBrowserEvent event, const gchar *service_type, const gchar *domain, gpointer userdata) {

    g_message("STB: (%i.%i) %s in <%s> [%s]", iface, protocol, service_type, domain, event == AVAHI_BROWSER_NEW ? "new" : "remove");
}

static void sb_callback(AvahiServiceBrowser *b, gint iface, guchar protocol, AvahiBrowserEvent event, const gchar *name, const gchar *service_type, const gchar *domain, gpointer userdata) {
   g_message("SB: (%i.%i) <%s> as %s in <%s> [%s]", iface, protocol, name, service_type, domain, event == AVAHI_BROWSER_NEW ? "new" : "remove");
}


static void sr_callback(AvahiServiceResolver *r, gint iface, guchar protocol, AvahiBrowserEvent event, const gchar *service_name, const gchar*service_type, const gchar*domain_name, const gchar*hostname, const AvahiAddress *a, guint16 port, AvahiStringList *txt, gpointer userdata) {

    if (event == AVAHI_RESOLVER_TIMEOUT)
        g_message("SR: (%i.%i) <%s> as %s in <%s> [timeout]", iface, protocol, service_name, service_type, domain_name);
    else {
        gchar t[64], *s;
        
        avahi_address_snprint(t, sizeof(t), a);

        s = avahi_string_list_to_string(txt);
        g_message("SR: (%i.%i) <%s> as %s in <%s>: %s/%s:%i (%s) [found]", iface, protocol, service_name, service_type, domain_name, hostname, t, port, s);
        g_free(s);
    }
}

int main(int argc, char *argv[]) {
    GMainLoop *loop = NULL;
    AvahiRecordBrowser *r;
    AvahiHostNameResolver *hnr;
    AvahiAddressResolver *ar;
    AvahiKey *k;
    AvahiServerConfig config;
    AvahiAddress a;
    AvahiDomainBrowser *db;
    AvahiServiceTypeBrowser *stb;
    AvahiServiceBrowser *sb;
    AvahiServiceResolver *sr;

    avahi_server_config_init(&config);
/*     config.host_name = g_strdup("test"); */
    server = avahi_server_new(NULL, &config, server_callback, NULL);
    avahi_server_config_free(&config);

    k = avahi_key_new("_http._tcp.local", AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    r = avahi_record_browser_new(server, -1, AF_UNSPEC, k, record_browser_callback, NULL);
    avahi_key_unref(k);

    hnr = avahi_host_name_resolver_new(server, -1, AF_UNSPEC, "codes-CompUTER.local", AF_UNSPEC, hnr_callback, NULL);

    ar = avahi_address_resolver_new(server, -1, AF_UNSPEC, avahi_address_parse("192.168.50.15", AF_INET, &a), ar_callback, NULL);

    db = avahi_domain_browser_new(server, -1, AF_UNSPEC, NULL, AVAHI_DOMAIN_BROWSER_BROWSE, db_callback, NULL);

    stb = avahi_service_type_browser_new(server, -1, AF_UNSPEC, NULL, stb_callback, NULL);

    sb = avahi_service_browser_new(server, -1, AF_UNSPEC, "_http._tcp", NULL, sb_callback, NULL);

    sr = avahi_service_resolver_new(server, -1, AF_UNSPEC, "Ecstasy HTTP", "_http._tcp", "local", AF_UNSPEC, sr_callback, NULL);
    
    loop = g_main_loop_new(NULL, FALSE);
    
    g_timeout_add(1000*5, dump_timeout, server);
    g_timeout_add(1000*30, quit_timeout, loop);     
    
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    avahi_record_browser_free(r);
    avahi_host_name_resolver_free(hnr);
    avahi_address_resolver_free(ar);
    avahi_service_type_browser_free(stb);
    avahi_service_browser_free(sb);
    avahi_service_resolver_free(sr);

    if (group)
        avahi_entry_group_free(group);   

    if (server)
        avahi_server_free(server);

    g_free(service_name);
    
    return 0;
}
