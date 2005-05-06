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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "avahi.h"

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
              event == AVAHI_SUBSCRIPTION_NEW ? "new" : (event == AVAHI_SUBSCRIPTION_CHANGE ? "changed" : "removed"));

    g_free(t);
}

static void entry_group_callback(AvahiServer *s, AvahiEntryGroup *g, AvahiEntryGroupState state, gpointer userdata) {
    g_message("entry group state: %i", state);
}

int main(int argc, char *argv[]) {
    AvahiServer *avahi;
    gchar *r;
    GMainLoop *loop = NULL;
    AvahiSubscription *s;
    AvahiKey *k;
    AvahiEntryGroup *g;

    avahi = avahi_server_new(NULL);

/*     g = avahi_entry_group_new(avahi, entry_group_callback, NULL);  */
    
/*    avahi_server_add_text(avahi, g, 0, AF_UNSPEC, AVAHI_ENTRY_UNIQUE, NULL, "hallo", NULL); */
/*      avahi_server_add_service(avahi, g, 0, AF_UNSPEC, "_http._tcp", "gurke", NULL, NULL, 80, "foo", NULL);  */
    
/*     avahi_entry_group_commit(g);  */

    avahi_server_dump(avahi, stdout);
    
    
/*     k = avahi_key_new("ecstasy.local.", AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_ANY); */
/*     s = avahi_subscription_new(avahi, k, 0, AF_UNSPEC, subscription, NULL); */
/*     avahi_key_unref(k); */

    loop = g_main_loop_new(NULL, FALSE);
    
  /*   g_timeout_add(1000*20, dump_timeout, Avahi); */
/*     g_timeout_add(1000*30, quit_timeout, loop); */
    
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

/*     avahi_subscription_free(s); */
    /* avahi_entry_group_free(g);  */
    avahi_server_free(avahi);
    
    return 0;
}
