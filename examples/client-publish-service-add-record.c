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

// see avahi-core/rr.h: AvahiRecord.data.srv
typedef struct mdns_dns_srv {
    uint16_t priority;
    uint16_t weight;
    uint16_t port;
    uint16_t pad;    // pad to 64bit
} mdns_dns_srv_t;
#define MDNS_DNS_SRV_HEAD_SIZE (3 * sizeof(uint16_t))

static bool enable_debug = 0;
static AvahiEntryGroup *group = NULL;
static AvahiSimplePoll *simple_poll = NULL;
static char *name = NULL;
static char *host_cname = NULL;

static void create_services(AvahiClient *c);
static void print_txt_record(char *txt);
static char *mdns_dns_srv_record(mdns_dns_srv_t *srv, const char *txt, int *rr_size);

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) {
    assert(g == group || group == NULL);
    group = g;

    /* Called whenever the entry group state changes */

    switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED :
            /* The entry group has been established successfully */
            fprintf(stderr, "Service '%s' successfully established.\n", name);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION : {
            char *n;

            /* A service name collision with a remote service
             * happened. Let's pick a new name */
            n = avahi_alternative_service_name(name);
            avahi_free(name);
            name = n;

            fprintf(stderr, "Service name collision, renaming service to '%s'\n", name);

            /* CNAME collision could also happen */
            n = avahi_alternative_service_name(host_cname);
            avahi_free(host_cname);
            host_cname = n;
            fprintf(stderr, "host CNAME collision, renaming CNAME to '%s'\n", host_cname);

            /* And recreate the services */
            create_services(avahi_entry_group_get_client(g));
            break;
        }

        case AVAHI_ENTRY_GROUP_FAILURE :

            fprintf(stderr, "Entry group failure: %s\n", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

            /* Some kind of failure happened while we were registering our services */
            avahi_simple_poll_quit(simple_poll);
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}

/* for debug use */
static void print_txt_record(char *txt) {
    char *s = txt;
    int len;
    int i;

    while (*s != '\0') {
        len = *(s++);
        printf("(%d)", len);
        for (i = 0; i < len; i++)
            putchar(*(s++));
    }
    putchar('\n');
}

/*
 * convert a dot separated string to raw txt record
 * e.g.
 * _name = "1.22.333.4444"
 * return = |1|"1"|2|"22"|3|"333"|4|"4444"|
 */
static char *mdns_name_to_txt_record(const char *_name) {
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
    if (enable_debug)
        print_txt_record(buf);

    return buf;
}

static char *mdns_dns_srv_record(mdns_dns_srv_t *srv, const char *txt, int *rr_size) {
    char *buf;

    *rr_size = MDNS_DNS_SRV_HEAD_SIZE + (strlen(txt) + 1);   // 1B '\0'
    buf = avahi_malloc(*rr_size);
    memcpy(buf, srv, MDNS_DNS_SRV_HEAD_SIZE);
    strcpy(buf + MDNS_DNS_SRV_HEAD_SIZE, txt);

    return buf;
}

static void create_services(AvahiClient *c) {
    char *n, r[128];
    int ret;
    char full_name[128];
    char *full_name_txt;
    char service_name[] = "_printer._tcp.local";
    char *service_name_txt;
    const char *host_name;
    char _host_cname[128];
    char *host_name_txt;
    char *host_name_srv;
    char dns_sd_service_name[] = "_services._dns-sd._udp.local";
    char *random_txt;
    int rr_size;
    mdns_dns_srv_t srv;
    bool cname_collide = false;
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
        fprintf(stderr, "Adding service '%s'\n", name);

        /* We will now try to achieve what avahi_entry_group_add_service()
         * does, using avahi_entry_group_add_record() API only. */

        /* create a PTR record that points to the SRV record */
        strcpy(full_name, name);
        strcat(full_name, ".");
        strcat(full_name, service_name);
        full_name_txt = mdns_name_to_txt_record(full_name);

        /* flags will be set automatically if 0 was provided in
         * avahi_entry_group_add_service(), but not in here. So
         * you have to pass all the flags you need. */
        ret = avahi_entry_group_add_record(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                AVAHI_PUBLISH_USE_MULTICAST,
                service_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR, AVAHI_DEFAULT_TTL,
                full_name_txt, strlen(full_name_txt) + 1);  // +1 for '\0'
        if (enable_debug)
            printf("avahi_entry_group_add_record PTR\n");
        avahi_free(full_name_txt);
        if (ret < 0) {
            if (ret == AVAHI_ERR_COLLISION)
                goto collision;

            fprintf(stderr, "Failed to add PTR record for %s: %s\n", service_name, avahi_strerror(ret));
            goto fail;
        }

        /* create a SRV record for our service.
         * SRV does not accept plain txt, but use a struct.
         * See avahi-core/dns.c : parse_rdata() for detail. */
        srv.priority = 0;
        srv.weight = 0;
        srv.port = 1234;
        host_name = avahi_client_get_host_name(c);
        host_name_txt = mdns_name_to_txt_record(host_name);
        host_name_srv = mdns_dns_srv_record(&srv, host_name_txt, &rr_size);

        /* SRV should use a different TTL than other 3 record */
        ret = avahi_entry_group_add_record(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                AVAHI_PUBLISH_UNIQUE | AVAHI_PUBLISH_USE_MULTICAST,
                full_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_SRV, AVAHI_DEFAULT_TTL_HOST_NAME,
                host_name_srv, rr_size);
        if (enable_debug)
            printf("avahi_entry_group_add_record SRV\n");
        avahi_free(host_name_txt);
        avahi_free(host_name_srv);
        if (ret < 0) {
            if (ret == AVAHI_ERR_COLLISION)
                goto collision;

            fprintf(stderr, "Failed to add SRV record for %s: %s\n", full_name, avahi_strerror(ret));
            goto fail;
        }

        /* create a TXT record for our service */
        snprintf(r, sizeof(r), "random=%i", rand());
        random_txt = mdns_name_to_txt_record(r);

        ret = avahi_entry_group_add_record(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                AVAHI_PUBLISH_UNIQUE | AVAHI_PUBLISH_USE_MULTICAST,
                full_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT, AVAHI_DEFAULT_TTL,
                random_txt, strlen(random_txt) + 1);  // +1 for '\0'
        if (enable_debug)
            printf("avahi_entry_group_add_record TXT\n");
        avahi_free(random_txt);
        if (ret < 0) {
            if (ret == AVAHI_ERR_COLLISION)
                goto collision;

            fprintf(stderr, "Failed to add TXT record for %s: %s\n", full_name, avahi_strerror(ret));
            goto fail;
        }

        /* create a PTR record for DNS-SD */
        service_name_txt = mdns_name_to_txt_record(service_name);

        /* don't use AVAHI_PUBLISH_UNIQUE in flags */
        ret = avahi_entry_group_add_record(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                AVAHI_PUBLISH_USE_MULTICAST,
                dns_sd_service_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR, AVAHI_DEFAULT_TTL,
                service_name_txt, strlen(service_name_txt) + 1);  // +1 for '\0'
        if (enable_debug)
            printf("avahi_entry_group_add_record DNS-SD PTR\n");
        avahi_free(service_name_txt);
        if (ret < 0) {
            if (ret == AVAHI_ERR_COLLISION)
                goto collision;

            fprintf(stderr, "Failed to add PTR record for %s: %s\n", dns_sd_service_name, avahi_strerror(ret));
            goto fail;
        }

        /* create a CNAME record, this record is optional */
        strcpy(_host_cname, host_name);
        strcat(_host_cname, ".local");
        host_name_txt = mdns_name_to_txt_record(_host_cname);

        if (host_cname == NULL)
            host_cname = (char *)host_name;
        strcpy(_host_cname, host_cname);
        strcat(_host_cname, "_cname.local");
        if (host_cname == host_name)
            host_cname = NULL;      // to prevent host_name being freed

        ret = avahi_entry_group_add_record(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                AVAHI_PUBLISH_UNIQUE | AVAHI_PUBLISH_USE_MULTICAST,
                _host_cname, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_CNAME, AVAHI_DEFAULT_TTL_HOST_NAME,
                host_name_txt, strlen(host_name_txt) + 1);  // +1 for '\0'
        printf("Adding CNAME %s for %s\n", _host_cname, host_name);
        avahi_free(host_name_txt);
        if (ret < 0) {
            if (ret == AVAHI_ERR_COLLISION) {
                cname_collide = true;
                goto cname_collision;
            }

            fprintf(stderr, "Failed to add CNAME record for %s: %s\n", _host_cname, avahi_strerror(ret));
            goto fail;
        }

        /* Tell the server to register the service */
        if ((ret = avahi_entry_group_commit(group)) < 0) {
            fprintf(stderr, "Failed to commit entry group: %s\n", avahi_strerror(ret));
            goto fail;
        }
    }

    return;

collision:

    /* A service name collision with a local service happened. Let's
     * pick a new name */
    n = avahi_alternative_service_name(name);
    avahi_free(name);
    name = n;

    fprintf(stderr, "Service name collision, renaming service to '%s'\n", name);

cname_collision:

    if (cname_collide) {
        cname_collide = false;
        n = avahi_alternative_service_name(host_cname);
        avahi_free(host_cname);
        host_cname = n;
        fprintf(stderr, "host CNAME collision, renaming CNAME to '%s'\n", host_cname);
    }

    avahi_entry_group_reset(group);

    create_services(c);
    return;

fail:
    avahi_simple_poll_quit(simple_poll);
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
    assert(c);

    /* Called whenever the client or server state changes */

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:

            /* The server has startup successfully and registered its host
             * name on the network, so it's time to create our services */
            create_services(c);
            break;

        case AVAHI_CLIENT_FAILURE:

            fprintf(stderr, "Client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
            avahi_simple_poll_quit(simple_poll);

            break;

        case AVAHI_CLIENT_S_COLLISION:

            /* Let's drop our registered services. When the server is back
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

static void modify_callback(AVAHI_GCC_UNUSED AvahiTimeout *e, void *userdata) {
    AvahiClient *client = userdata;

    fprintf(stderr, "Doing some weird modification\n");

    avahi_free(name);
    name = avahi_strdup("Modified MegaPrinter");

    /* If the server is currently running, we need to remove our
     * service and create it anew */
    if (avahi_client_get_state(client) == AVAHI_CLIENT_S_RUNNING) {

        /* Remove the old services */
        if (group)
            avahi_entry_group_reset(group);

        /* And create them again with the new name */
        create_services(client);
    }
}

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char*argv[]) {
    AvahiClient *client = NULL;
    int error;
    int ret = 1;
    struct timeval tv;

    /* Allocate main loop object */
    if (!(simple_poll = avahi_simple_poll_new())) {
        fprintf(stderr, "Failed to create simple poll object.\n");
        goto fail;
    }

    name = avahi_strdup("MegaPrinter");

    /* Allocate a new client */
    client = avahi_client_new(avahi_simple_poll_get(simple_poll), 0, client_callback, NULL, &error);

    /* Check whether creating the client object succeeded */
    if (!client) {
        fprintf(stderr, "Failed to create client: %s\n", avahi_strerror(error));
        goto fail;
    }

    /* After 10s do some weird modification to the service */
    avahi_simple_poll_get(simple_poll)->timeout_new(
        avahi_simple_poll_get(simple_poll),
        avahi_elapse_time(&tv, 1000*10, 0),
        modify_callback,
        client);

    /* Run the main loop */
    avahi_simple_poll_loop(simple_poll);

    ret = 0;

fail:

    /* Cleanup things */

    if (client)
        avahi_client_free(client);

    if (simple_poll)
        avahi_simple_poll_free(simple_poll);

    avahi_free(name);

    return ret;
}
