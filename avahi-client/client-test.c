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

#include <avahi-client/client.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <stdio.h>
#include <assert.h>

static const AvahiPoll *poll_api = NULL;
static AvahiSimplePoll *simple_poll = NULL;


static void
avahi_client_callback (AvahiClient *c, AvahiClientState state, void *user_data)
{
    printf ("XXX: Callback on client, state -> %d, data -> %s\n", state, (char*)user_data);
}

static void
avahi_entry_group_callback (AvahiEntryGroup *g, AvahiEntryGroupState state, void *user_data)
{
    printf ("XXX: Callback on %s, state -> %d, data -> %s\n", avahi_entry_group_get_dbus_path(g), state, (char*)user_data);
}

static void
avahi_domain_browser_callback (AvahiDomainBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *domain, void *user_data)
{
    printf ("XXX: Callback on %s, interface (%d), protocol (%d), event (%d), domain (%s), data (%s)\n", avahi_domain_browser_get_dbus_path (b), interface, protocol, event, domain, (char*)user_data);
}

static void
avahi_service_browser_callback (AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, void *user_data)
{
    printf ("XXX: Callback on %s, interface (%d), protocol (%d), event (%d), name (%s), type (%s), domain (%s), data (%s)\n", avahi_service_browser_get_dbus_path (b), interface, protocol, event, name, type, domain, (char*)user_data);
}

static void
avahi_service_type_browser_callback (AvahiServiceTypeBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *type, const char *domain, void *user_data)
{
    printf ("XXX: Callback on %s, interface (%d), protocol (%d), event (%d), type (%s), domain (%s), data (%s)\n", avahi_service_type_browser_get_dbus_path (b), interface, protocol, event, type, domain, (char*)user_data);
}

static void test_free_domain_browser(AvahiTimeout *timeout, void* userdata)
{
    AvahiServiceBrowser *b = userdata;
    printf ("XXX: freeing domain browser\n");
    avahi_service_browser_free (b);
}

static void test_free_entry_group (AvahiTimeout *timeout, void* userdata)
{
    AvahiEntryGroup *g = userdata;
    printf ("XXX: freeing entry group\n");
    avahi_entry_group_free (g);
}

static void terminate(AvahiTimeout *timeout, void *userdata) {

    avahi_simple_poll_quit(simple_poll);
}

int main (int argc, char *argv[]) {
    AvahiClient *avahi;
    AvahiEntryGroup *group;
    AvahiDomainBrowser *domain;
    AvahiServiceBrowser *sb;
    AvahiServiceTypeBrowser *st;
    const char *ret;
    int error;
    struct timeval tv;

    simple_poll = avahi_simple_poll_new();
    poll_api = avahi_simple_poll_get(simple_poll);
    
    if (!(avahi = avahi_client_new(poll_api, avahi_client_callback, "omghai2u", &error))) {
        fprintf(stderr, "Client failed: %s\n", avahi_strerror(error));
        goto fail;
    }

    printf("State: %i\n", avahi_client_get_state(avahi));

    ret = avahi_client_get_version_string (avahi);
    printf("Avahi Server Version: %s (Error Return: %s)\n", ret, ret ? "OK" : avahi_strerror(avahi_client_errno(avahi)));

    ret = avahi_client_get_host_name (avahi);
    printf("Host Name: %s (Error Return: %s)\n", ret, ret ? "OK" : avahi_strerror(avahi_client_errno(avahi)));

    ret = avahi_client_get_domain_name (avahi);
    printf("Domain Name: %s (Error Return: %s)\n", ret, ret ? "OK" : avahi_strerror(avahi_client_errno(avahi)));

    ret = avahi_client_get_host_name_fqdn (avahi);
    printf("FQDN: %s (Error Return: %s)\n", ret, ret ? "OK" : avahi_strerror(avahi_client_errno(avahi)));
    
    group = avahi_entry_group_new (avahi, avahi_entry_group_callback, "omghai");
    printf("Creating entry group: %s\n", group ? "OK" : avahi_strerror(avahi_client_errno (avahi)));

    assert(group);
    
    printf("Sucessfully created entry group, path %s\n", avahi_entry_group_get_dbus_path (group));

    avahi_entry_group_add_service (group, AVAHI_IF_UNSPEC, AF_UNSPEC, "Lathiat's Site", "_http._tcp", "", "", 80, "foo=bar", NULL);

    avahi_entry_group_commit (group);

    domain = avahi_domain_browser_new (avahi, AVAHI_IF_UNSPEC, AF_UNSPEC, NULL, AVAHI_DOMAIN_BROWSER_BROWSE, avahi_domain_browser_callback, "omghai3u");
    
    if (domain == NULL)
        printf ("Failed to create domain browser object\n");
    else
        printf ("Sucessfully created domain browser, path %s\n", avahi_domain_browser_get_dbus_path (domain));

    st = avahi_service_type_browser_new (avahi, AVAHI_IF_UNSPEC, AF_UNSPEC, "", avahi_service_type_browser_callback, "omghai3u");
    if (st == NULL)
        printf ("Failed to create service type browser object\n");
    else
        printf ("Sucessfully created service type browser, path %s\n", avahi_service_type_browser_get_dbus_path (st));

    sb = avahi_service_browser_new (avahi, AVAHI_IF_UNSPEC, AF_UNSPEC, "_http._tcp", "", avahi_service_browser_callback, "omghai3u");
    if (sb == NULL)
        printf ("Failed to create service browser object\n");
    else
        printf ("Sucessfully created service browser, path %s\n", avahi_service_browser_get_dbus_path (sb));


    avahi_elapse_time(&tv, 5000, 0);
    poll_api->timeout_new(poll_api, &tv, test_free_entry_group, group);
    avahi_elapse_time(&tv, 8000, 0);
    poll_api->timeout_new(poll_api, &tv, test_free_domain_browser, sb);

    avahi_elapse_time(&tv, 20000, 0);
    poll_api->timeout_new(poll_api, &tv, terminate, NULL);

    for (;;)
        if (avahi_simple_poll_iterate(simple_poll, -1) != 0)
            break;

fail:

    if (avahi)
        avahi_client_free (avahi);

    if (simple_poll)
        avahi_simple_poll_free(simple_poll);

    return 0;
}
