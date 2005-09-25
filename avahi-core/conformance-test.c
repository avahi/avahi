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
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>

#include "core.h"
#include "log.h"
#include "lookup.h"

static char *name = NULL;
static AvahiSEntryGroup *group = NULL;
static int try = 0;
static AvahiServer *avahi = NULL;
static const AvahiPoll *poll_api;

static void dump_line(const char *text, void* userdata) {
    printf("%s\n", text);
}

static void dump_timeout_callback(AvahiTimeout *timeout, void* data) {
    struct timeval tv;
    
    avahi_server_dump(avahi, dump_line, NULL);

    avahi_elapse_time(&tv, 5000, 0);
    poll_api->timeout_update(timeout, &tv);
}

static void entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state, void* userdata);

static void create_service(const char *t) {
    char *n;

    assert(t || name);

    n = t ? avahi_strdup(t) : avahi_alternative_service_name(name);
    avahi_free(name);
    name = n;

    if (group)
        avahi_s_entry_group_reset(group);
    else
        group = avahi_s_entry_group_new(avahi, entry_group_callback, NULL);
    
    avahi_server_add_service(avahi, group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, name, "_http._tcp", NULL, NULL, 80, "foo", NULL);   
    avahi_s_entry_group_commit(group);

    try++;
}

static void rename_timeout_callback(AvahiTimeout *timeout, void *userdata) {
    struct timeval tv;
    
    if (access("flag", F_OK) == 0) { 
        create_service("New - Bonjour Service Name");
        return;
    }

    avahi_elapse_time(&tv, 5000, 0);
    poll_api->timeout_update(timeout, &tv);
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
    int error;
    AvahiSimplePoll *simple_poll;
    struct timeval tv;

    simple_poll = avahi_simple_poll_new();
    poll_api = avahi_simple_poll_get(simple_poll);
    
    avahi = avahi_server_new(poll_api, NULL, server_callback, NULL, &error);

    avahi_elapse_time(&tv, 5000, 0);
    poll_api->timeout_new(poll_api, &tv, dump_timeout_callback, avahi);

    avahi_elapse_time(&tv, 5000, 0);
    poll_api->timeout_new(poll_api, &tv, rename_timeout_callback, avahi);

    for (;;)
        if (avahi_simple_poll_iterate(simple_poll, -1) != 0)
            break;
    
    if (group)
        avahi_s_entry_group_free(group);   
    avahi_server_free(avahi);

    avahi_simple_poll_free(simple_poll);
    
    return 0;
}
