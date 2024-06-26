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
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

static AvahiEntryGroup *group = NULL;
static AvahiSimplePoll *simple_poll = NULL;
const char *host_name = NULL;
static char *host_cname = NULL;

static void create_record(AvahiClient *c);

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) {
    assert(g == group || group == NULL);
    group = g;

    /* Called whenever the entry group state changes */

    switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED :
            /* The entry group has been established successfully */
            fprintf(stderr, "host CNAME '%s_cname' successfully established.\n", host_cname);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION : {
            char *n;

            /* A CNAME name collision with a remote host
             * happened. Let's pick a new name */
            n = avahi_alternative_host_name(host_cname);
            if (host_cname != host_name)     // to prevent host_name being freed
                avahi_free(host_cname);
            host_cname = n;
            fprintf(stderr, "host CNAME collision, renaming CNAME to '%s_cname'\n", host_cname);

            /* And recreate the records */
            create_record(avahi_entry_group_get_client(g));
            break;
        }

        case AVAHI_ENTRY_GROUP_FAILURE :

            fprintf(stderr, "Entry group failure: %s\n", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

            /* Some kind of failure happened while we were registering our records */
            avahi_simple_poll_quit(simple_poll);
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}

/*
 * convert a dot separated string to raw rdata
 * e.g.
 * _name = "1.22.333.4444"
 * return = |1|"1"|2|"22"|3|"333"|4|"4444"|
 */
static char *dot_string_to_rdata(const char *_name) {
    char *str1;
    char *str2;
    char *buf;
    int len;

    // 1B len in the head + 1B '\0' in the tail
    buf = avahi_malloc(strlen(_name) + 2);
    strcpy(buf+1, _name);

    str1 = strchr(buf+1, '.');
    if (str1 == NULL)
        buf[0] = strlen(_name);
    else {
        len = str1 - (buf+1);
        buf[0] = len;

        str2 = strchr(str1+1, '.');
        while (str2 != NULL) {
            len = str2 - (str1+1);
            *str1 = len;    // replace '.' with len
            str1 = str2;
            str2 = strchr(str1+1, '.');
        }

        // the last part
        *str1 = strlen(str1+1);
    }

    return buf;
}

static void create_record(AvahiClient *c) {
    char *n;
    int ret;
    char _host_cname[128];
    char *host_name_rdata;
    assert(c);

    /* If this is the first time we're called, let's create a new
     * entry group if necessary */

    if (!group)
        if (!(group = avahi_entry_group_new(c, entry_group_callback, NULL))) {
            fprintf(stderr, "avahi_entry_group_new() failed: %s\n", avahi_strerror(avahi_client_errno(c)));
            goto fail;
        }

    /* If the group is empty (either because it was just created, or
     * because it was reset previously, add our entries.  */

    if (avahi_entry_group_is_empty(group)) {

        /* create a CNAME record */
        strcpy(_host_cname, host_name);
        strcat(_host_cname, ".local");
        host_name_rdata = dot_string_to_rdata(_host_cname);

        if (host_cname == NULL)
            host_cname = (char *)host_name;
        strcpy(_host_cname, host_cname);
        strcat(_host_cname, "_cname.local");

        /* flags will be set automatically if 0 was provided in
         * avahi_entry_group_add_service(), but not in here. So
         * you have to pass all the flags you need. */
        ret = avahi_entry_group_add_record(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                AVAHI_PUBLISH_UNIQUE | AVAHI_PUBLISH_USE_MULTICAST,
                _host_cname, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_CNAME, AVAHI_DEFAULT_TTL_HOST_NAME,
                host_name_rdata, strlen(host_name_rdata) + 1);  // +1 for '\0'
        printf("Adding CNAME '%s' for '%s'\n", _host_cname, host_name);
        avahi_free(host_name_rdata);
        if (ret < 0) {
            if (ret == AVAHI_ERR_COLLISION)
                goto cname_collision;

            fprintf(stderr, "Failed to add CNAME record for '%s': %s\n", _host_cname, avahi_strerror(ret));
            goto fail;
        }

        /* Tell the server to register the record */
        if ((ret = avahi_entry_group_commit(group)) < 0) {
            fprintf(stderr, "Failed to commit entry group: %s\n", avahi_strerror(ret));
            goto fail;
        }
    }

    return;

cname_collision:

    /* A CNAME name collision with a local name happened. Let's
     * pick a new name */
    n = avahi_alternative_host_name(host_cname);
    if (host_cname != host_name)     // to prevent host_name being freed
        avahi_free(host_cname);
    host_cname = n;
    fprintf(stderr, "host CNAME collision, renaming CNAME to '%s_cname'\n", host_cname);

    avahi_entry_group_reset(group);

    create_record(c);
    return;

fail:
    avahi_simple_poll_quit(simple_poll);
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
    assert(c);

    /* Called whenever the client or server state changes */

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:

			host_name = avahi_client_get_host_name(c);

            /* The server has startup successfully and registered its host
             * name on the network, so it's time to create our records */
            create_record(c);
            break;

        case AVAHI_CLIENT_FAILURE:

            fprintf(stderr, "Client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
            avahi_simple_poll_quit(simple_poll);

            break;

        case AVAHI_CLIENT_S_COLLISION:

            /* Let's drop our registered records. When the server is back
             * in AVAHI_SERVER_RUNNING state we will register them
             * again with the new host name. */

        case AVAHI_CLIENT_S_REGISTERING:

            /* The server records are now being established. This
             * might be caused by a host name change. We need to wait
             * for our own records to register until the host name is
             * properly esatblished. */

            if (group)
                avahi_entry_group_reset(group);

            break;

        case AVAHI_CLIENT_CONNECTING:
            ;
    }
}

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char*argv[]) {
    AvahiClient *client = NULL;
    int error;
    int ret = 1;

    /* Allocate main loop object */
    if (!(simple_poll = avahi_simple_poll_new())) {
        fprintf(stderr, "Failed to create simple poll object.\n");
        goto fail;
    }

    /* Allocate a new client */
    client = avahi_client_new(avahi_simple_poll_get(simple_poll), 0, client_callback, NULL, &error);

    /* Check whether creating the client object succeeded */
    if (!client) {
        fprintf(stderr, "Failed to create client: %s\n", avahi_strerror(error));
        goto fail;
    }

    /* Run the main loop */
    avahi_simple_poll_loop(simple_poll);

    ret = 0;

fail:

    /* Cleanup things */

    if (client)
        avahi_client_free(client);

    if (simple_poll)
        avahi_simple_poll_free(simple_poll);

    return ret;
}
