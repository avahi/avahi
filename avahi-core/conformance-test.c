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
#include <stdio.h>
#include <assert.h>

#include <avahi-common/alternative.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include "core.h"
#include "log.h"

static char *name = NULL;
static AvahiSEntryGroup *group = NULL;
static int try = 0;
static AvahiServer *avahi = NULL;

static void dump_line(const char *text, void* userdata) {
    printf("%s\n", text);
}

static int dump_timeout(void* data) {
    avahi_server_dump(avahi, dump_line, NULL);
    return 1;
}

static void entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state, void* userdata);

static void create_service(const char *t) {
    char *n;

    assert(t || name);

    n = t ? g_strdup(t) : avahi_alternative_service_name(name);
    avahi_free(name);
    name = n;

    if (group)
        avahi_s_entry_group_reset(group);
    else
        group = avahi_s_entry_group_new(avahi, entry_group_callback, NULL);
    
    avahi_server_add_service(avahi, group, 0, AF_UNSPEC, name, "_http._tcp", NULL, NULL, 80, "foo", NULL);   
    avahi_s_entry_group_commit(group);

    try++;
}

static int rename_timeout(void* data) {
    
    if (access("flag", F_OK) == 0) { 
        create_service("New - Bonjour Service Name");
        return 0;
    }

    return 1;
}

static void entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state, void* userdata) {
    if (state == AVAHI_ENTRY_GROUP_COLLISION)
        create_service(NULL);
    else if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {
        avahi_log_debug("ESTABLISHED !!!!");
        try = 0;
    }
}

static void server_callback(AvahiServer *s, AvahiServerState state, void* userdata) {
    avahi_log_debug("server state: %i", state);

    if (state == AVAHI_SERVER_RUNNING) {
        create_service("gurke");
        avahi_server_dump(avahi, dump_line, NULL);
    }
}

int main(int argc, char *argv[]) {
    GMainLoop *loop = NULL;
    gint error;
    AvahiGLibPoll *glib_poll;

    avahi_set_allocator(avahi_glib_allocator());

    glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    
    avahi = avahi_server_new(avahi_glib_poll_get(glib_poll), NULL, server_callback, NULL, &error);
    
    loop = g_main_loop_new(NULL, 0);
    g_timeout_add(1000*5, dump_timeout, avahi);
    g_timeout_add(1000*5, rename_timeout, avahi); 
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    if (group)
        avahi_s_entry_group_free(group);   
    avahi_server_free(avahi);

    avahi_glib_poll_free(glib_poll);
    
    return 0;
}
