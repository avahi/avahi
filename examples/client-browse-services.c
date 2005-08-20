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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include <avahi-client/client.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

static AvahiSimplePoll *simple_poll = NULL;

static void resolve_callback(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    void* userdata) {

    assert(r);

    /* Called whenever a service has been resolved successfully or timed out */

    if (event == AVAHI_RESOLVER_TIMEOUT)
        fprintf(stderr, "Failed to resolve service '%s' of type '%s' in domain '%s'.\n", name, type, domain);
    else {
        char a[128], *t;

        assert(event == AVAHI_RESOLVER_FOUND);
        
        fprintf(stderr, "Service '%s' of type '%s' in domain '%s':\n", name, type, domain);

        avahi_address_snprint(a, sizeof(a), address);
        t = avahi_string_list_to_string(txt);
        fprintf(stderr, "\t%s:%u (%s) TXT=%s\n", host_name, port, a, t);
        avahi_free(t);
    }

    avahi_service_resolver_free(r);
}

static void browse_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    void* userdata) {
    
    AvahiClient *c = userdata;
    assert(b);

    /* Called whenever a new services becomes available on the LAN or is removed from the LAN */

    fprintf(stderr, "%s: service '%s' of type '%s' in domain '%s'\n",
            event == AVAHI_BROWSER_NEW ? "NEW" : "REMOVED",
            name,
            type,
            domain);
    
    /* If it's new, let's resolve it */
    if (event == AVAHI_BROWSER_NEW)
        
        /* We ignore the returned resolver object. In the callback function
        we free it. If the server is terminated before the callback
        function is called the server will free the resolver for us. */

        if (!(avahi_service_resolver_new(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, resolve_callback, c)))
            fprintf(stderr, "Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_client_errno(c)));
}

static void client_callback(AvahiClient *c, AvahiClientState state, void * userdata) {
    assert(c);

    /* Called whenever the client or server state changes */

    if (state == AVAHI_CLIENT_DISCONNECTED) {
        fprintf(stderr, "Server connection terminated.\n");
        avahi_simple_poll_quit(simple_poll);
    }
}

int main(int argc, char*argv[]) {
    AvahiClient *client = NULL;
    AvahiServiceBrowser *sb;
    int error;
    int ret = 1;

    /* Allocate main loop object */
    if (!(simple_poll = avahi_simple_poll_new())) {
        fprintf(stderr, "Failed to create simple poll object.\n");
        goto fail;
    }

    /* Allocate a new client */
    client = avahi_client_new(avahi_simple_poll_get(simple_poll), client_callback, NULL, &error);

    /* Check wether creating the client object succeeded */
    if (!client) {
        fprintf(stderr, "Failed to create client: %s\n", avahi_strerror(error));
        goto fail;
    }
    
    /* Create the service browser */
    if (!(sb = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_http._tcp", NULL, browse_callback, client))) {
        fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
        goto fail;
    }
    
    /* Run the main loop */
    for (;;)
        if (avahi_simple_poll_iterate(simple_poll, -1) != 0)
            break;
    
    ret = 0;
    
fail:
    
    /* Cleanup things */
    if (sb)
        avahi_service_browser_free(sb);
    
    if (client)
        avahi_client_free(client);

    if (simple_poll)
        avahi_simple_poll_free(simple_poll);

    return ret;
}
