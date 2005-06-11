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
#include <unistd.h>

#include "core.h"
#include "util.h"
#include "alternative.h"
#include "log.h"

static gchar *name = NULL;
static AvahiEntryGroup *group = NULL;
static int try = 0;
static AvahiServer *avahi = NULL;

static gboolean dump_timeout(gpointer data) {
    avahi_server_dump(avahi, stdout);
    return TRUE;
}

static void entry_group_callback(AvahiServer *s, AvahiEntryGroup *g, AvahiEntryGroupState state, gpointer userdata);

static void create_service(gchar *t) {
    gchar *n;

    g_assert(t || name);

    if (group)
        avahi_entry_group_free(group);
    
    n = t ? g_strdup(t) : avahi_alternative_service_name(name);
    g_free(name);
    name = n;

    if (try > 10)
        sleep(2); /* ugly ugly ugly hack */
    
    group = avahi_entry_group_new(avahi, entry_group_callback, NULL);   
    avahi_server_add_service(avahi, group, 0, AF_UNSPEC, "_http._tcp", name, NULL, NULL, 80, "foo", NULL);   
    avahi_entry_group_commit(group);

    try++;
}

static gboolean rename_timeout(gpointer data) {
    
    if (access("flag", F_OK) == 0) { 
        create_service("New - Bonjour Service Name");
        return FALSE;
    }

    return TRUE;
}

static void entry_group_callback(AvahiServer *s, AvahiEntryGroup *g, AvahiEntryGroupState state, gpointer userdata) {
    if (state == AVAHI_ENTRY_GROUP_COLLISION)
        create_service(NULL);
    else if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {
        avahi_log_debug("ESTABLISHED !!!!");
        try = 0;
    }
}

static void server_callback(AvahiServer *s, AvahiServerState state, gpointer userdata) {
    avahi_log_debug("server state: %i", state);
}

int main(int argc, char *argv[]) {
    GMainLoop *loop = NULL;

    avahi = avahi_server_new(NULL, NULL, server_callback, NULL);
    create_service("gurke");
    avahi_server_dump(avahi, stdout);
    
    loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add(1000*5, dump_timeout, avahi);
    g_timeout_add(1000*5, rename_timeout, avahi); 
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    avahi_entry_group_free(group);   
    avahi_server_free(avahi);
    
    return 0;
}
