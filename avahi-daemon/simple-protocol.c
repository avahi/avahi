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

#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>

#include <glib.h>

#include <avahi-common/llist.h>
#include <avahi-core/log.h>

#include "simple-protocol.h"
#include "main.h"

#define BUFFER_SIZE (20*1024)

#define CLIENTS_MAX 50

typedef struct Client Client;
typedef struct Server Server;

typedef enum {
    CLIENT_IDLE,
    CLIENT_RESOLVE_HOSTNAME,
    CLIENT_RESOLVE_ADDRESS,
    CLIENT_BROWSE_DNS_SERVERS,
    CLIENT_DEAD
} ClientState;

struct Client {
    Server *server;

    ClientState state;
    
    gint fd;
    GPollFD poll_fd;

    gchar inbuf[BUFFER_SIZE], outbuf[BUFFER_SIZE];
    guint inbuf_length, outbuf_length;

    AvahiHostNameResolver *host_name_resolver;
    AvahiAddressResolver *address_resolver;
    AvahiDNSServerBrowser *dns_server_browser;

    AvahiProtocol afquery;
    
    AVAHI_LLIST_FIELDS(Client, clients);
};

struct Server {
    GSource source;
    GMainContext *context;
    GPollFD poll_fd;
    gint fd;
    AVAHI_LLIST_HEAD(Client, clients);

    guint n_clients;
    gboolean bind_successful;
};

static Server *server = NULL;

static void client_free(Client *c) {
    g_assert(c);

    g_assert(c->server->n_clients >= 1);
    c->server->n_clients--;

    if (c->host_name_resolver)
        avahi_host_name_resolver_free(c->host_name_resolver);

    if (c->address_resolver)
        avahi_address_resolver_free(c->address_resolver);

    if (c->dns_server_browser)
        avahi_dns_server_browser_free(c->dns_server_browser);
    
    g_source_remove_poll(&c->server->source, &c->poll_fd);
    close(c->fd);
    AVAHI_LLIST_REMOVE(Client, clients, c->server->clients, c);
    g_free(c);
}

static void client_new(Server *s, int fd) {
    Client *c;

    g_assert(fd >= 0);

    c = g_new(Client, 1);
    c->server = s;
    c->fd = fd;
    c->state = CLIENT_IDLE;

    c->inbuf_length = c->outbuf_length = 0;

    c->host_name_resolver = NULL;
    c->address_resolver = NULL;
    c->dns_server_browser = NULL;

    memset(&c->poll_fd, 0, sizeof(GPollFD));
    c->poll_fd.fd = fd;
    c->poll_fd.events = G_IO_IN|G_IO_ERR|G_IO_HUP;
    g_source_add_poll(&s->source, &c->poll_fd);

    AVAHI_LLIST_PREPEND(Client, clients, s->clients, c);
    s->n_clients++;
}

static void client_output(Client *c, const guint8*data, guint size) {
    guint k, m;
    
    g_assert(c);
    g_assert(data);

    if (!size)
        return;

    k = sizeof(c->outbuf) - c->outbuf_length;
    m = size > k ? k : size;

    memcpy(c->outbuf + c->outbuf_length, data, m);
    c->outbuf_length += m;

    c->poll_fd.events |= G_IO_OUT;
}

static void client_output_printf(Client *c, const gchar *format, ...) {
    gchar *t;
    va_list ap;
    
    va_start(ap, format);
    t = g_strdup_vprintf(format, ap);
    va_end(ap);

    client_output(c, (guint8*) t, strlen(t));
    g_free(t);
}


static void host_name_resolver_callback(
    AvahiHostNameResolver *r,
    AvahiIfIndex iface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *hostname,
    const AvahiAddress *a,
    void* userdata) {

    Client *c = userdata;
    
    g_assert(c);

    if (event == AVAHI_RESOLVER_TIMEOUT)
        client_output_printf(c, "%+i Query timed out\n", AVAHI_ERR_TIMEOUT);
    else {
        gchar t[64];
        avahi_address_snprint(t, sizeof(t), a);
        client_output_printf(c, "+ %i %u %s %s\n", iface, protocol, hostname, t);
    }

    c->state = CLIENT_DEAD;
}

static void address_resolver_callback(
    AvahiAddressResolver *r,
    AvahiIfIndex iface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const AvahiAddress *a,
    const char *hostname,
    void* userdata) {
    
    Client *c = userdata;
    
    g_assert(c);

    if (event == AVAHI_RESOLVER_TIMEOUT)
        client_output_printf(c, "%+i Query timed out\n", AVAHI_ERR_TIMEOUT);
    else 
        client_output_printf(c, "+ %i %u %s\n", iface, protocol, hostname);

    c->state = CLIENT_DEAD;
}

static void dns_server_browser_callback(AvahiDNSServerBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *host_name, const AvahiAddress *a, uint16_t port, void* userdata) {
    Client *c = userdata;
    gchar t[64];
    
    g_assert(c);

    if (!a)
        return;

    avahi_address_snprint(t, sizeof(t), a);
    client_output_printf(c, "%c %i %u %s %u\n", event == AVAHI_BROWSER_NEW ? '>' : '<',  interface, protocol, t, port);
}

static void handle_line(Client *c, const gchar *s) {
    gchar cmd[64], arg[64];
    gint n_args;

    g_assert(c);
    g_assert(s);

    if (c->state != CLIENT_IDLE)
        return;

    if ((n_args = sscanf(s, "%63s %63s", cmd, arg)) < 1 ) {
        client_output_printf(c, "%+i Failed to parse command, try \"HELP\".\n", AVAHI_ERR_INVALID_OPERATION);
        c->state = CLIENT_DEAD;
        return;
    }

    if (strcmp(cmd, "HELP") == 0) {
        client_output_printf(c,
                             "+ Available commands are:\n"
                             "+      RESOLVE-HOSTNAME <hostname>\n"
                             "+      RESOLVE-HOSTNAME-IPV6 <hostname>\n"
                             "+      RESOLVE-HOSTNAME-IPV4 <hostname>\n"
                             "+      RESOLVE-ADDRESS <address>\n"
                             "+      BROWSE-DNS-SERVERS\n"
                             "+      BROWSE-DNS-SERVERS-IPV4\n"
                             "+      BROWSE-DNS-SERVERS-IPV6\n");
        c->state = CLIENT_DEAD; }
    else if (strcmp(cmd, "FUCK") == 0 && n_args == 1) {
        client_output_printf(c, "+ FUCK: Go fuck yourself!\n");
        c->state = CLIENT_DEAD;
    } else if (strcmp(cmd, "RESOLVE-HOSTNAME-IPV4") == 0 && n_args == 2) {
        c->state = CLIENT_RESOLVE_HOSTNAME;
        if (!(c->host_name_resolver = avahi_host_name_resolver_new(avahi_server, -1, AF_UNSPEC, arg, c->afquery = AF_INET, host_name_resolver_callback, c)))
            goto fail;
    } else if (strcmp(cmd, "RESOLVE-HOSTNAME-IPV6") == 0 && n_args == 2) {
        c->state = CLIENT_RESOLVE_HOSTNAME;
        if (!(c->host_name_resolver = avahi_host_name_resolver_new(avahi_server, -1, AF_UNSPEC, arg, c->afquery = AF_INET6, host_name_resolver_callback, c)))
            goto fail;
    } else if (strcmp(cmd, "RESOLVE-HOSTNAME") == 0 && n_args == 2) {
        c->state = CLIENT_RESOLVE_HOSTNAME;
        if (!(c->host_name_resolver = avahi_host_name_resolver_new(avahi_server, -1, AF_UNSPEC, arg, c->afquery = AF_UNSPEC, host_name_resolver_callback, c)))
            goto fail;
    } else if (strcmp(cmd, "RESOLVE-ADDRESS") == 0 && n_args == 2) {
        AvahiAddress addr;
        
        if (!(avahi_address_parse(arg, AF_UNSPEC, &addr))) {
            client_output_printf(c, "%+i Failed to parse address \"%s\".\n", AVAHI_ERR_INVALID_ADDRESS, arg);
            c->state = CLIENT_DEAD;
        } else {
            c->state = CLIENT_RESOLVE_ADDRESS;
            if (!(c->address_resolver = avahi_address_resolver_new(avahi_server, -1, AF_UNSPEC, &addr, address_resolver_callback, c)))
                goto fail;
        }
    } else if (strcmp(cmd, "BROWSE-DNS-SERVERS-IPV4") == 0 && n_args == 1) {
        c->state = CLIENT_BROWSE_DNS_SERVERS;
        if (!(c->dns_server_browser = avahi_dns_server_browser_new(avahi_server, -1, AF_UNSPEC, NULL, AVAHI_DNS_SERVER_RESOLVE, c->afquery = AF_INET, dns_server_browser_callback, c)))
            goto fail;
        client_output_printf(c, "+ Browsing ...\n");
    } else if (strcmp(cmd, "BROWSE-DNS-SERVERS-IPV6") == 0 && n_args == 1) {
        c->state = CLIENT_BROWSE_DNS_SERVERS;
        if (!(c->dns_server_browser = avahi_dns_server_browser_new(avahi_server, -1, AF_UNSPEC, NULL, AVAHI_DNS_SERVER_RESOLVE, c->afquery = AF_INET6, dns_server_browser_callback, c)))
            goto fail;
        client_output_printf(c, "+ Browsing ...\n");
    } else if (strcmp(cmd, "BROWSE-DNS-SERVERS") == 0 && n_args == 1) {
        c->state = CLIENT_BROWSE_DNS_SERVERS;
        if (!(c->dns_server_browser = avahi_dns_server_browser_new(avahi_server, -1, AF_UNSPEC, NULL, AVAHI_DNS_SERVER_RESOLVE, c->afquery = AF_UNSPEC, dns_server_browser_callback, c)))
            goto fail;
        client_output_printf(c, "+ Browsing ...\n");
    } else {
        client_output_printf(c, "%+i Invalid command \"%s\", try \"HELP\".\n", AVAHI_ERR_INVALID_OPERATION, cmd);
        c->state = CLIENT_DEAD;
    }

    return;

fail:
    client_output_printf(c, "%+i %s\n", avahi_server_errno(avahi_server), avahi_strerror(avahi_server_errno(avahi_server)));
    c->state = CLIENT_DEAD;
}

static void handle_input(Client *c) {
    g_assert(c);

    for (;;) {
        gchar *e;
        guint k;

        if (!(e = memchr(c->inbuf, '\n', c->inbuf_length)))
            break;

        k = e - (gchar*) c->inbuf;
        *e = 0;
        
        handle_line(c, c->inbuf);
        c->inbuf_length -= k + 1;
        memmove(c->inbuf, e+1, c->inbuf_length);
    }
}

static void client_work(Client *c) {
    g_assert(c);

    if ((c->poll_fd.revents & G_IO_IN) && c->inbuf_length < sizeof(c->inbuf)) {
        ssize_t r;
        
        if ((r = read(c->fd, c->inbuf + c->inbuf_length, sizeof(c->inbuf) - c->inbuf_length)) <= 0) {
            if (r < 0)
                avahi_log_warn("read(): %s", strerror(errno));
            client_free(c);
            return;
        }

        c->inbuf_length += r;
        g_assert(c->inbuf_length <= sizeof(c->inbuf));

        handle_input(c);
    }

    if ((c->poll_fd.revents & G_IO_OUT) && c->outbuf_length > 0) {
        ssize_t r;

        if ((r = write(c->fd, c->outbuf, c->outbuf_length)) < 0) {
            avahi_log_warn("write(): %s", strerror(errno));
            client_free(c);
            return;
        }

        g_assert((guint) r <= c->outbuf_length);
        c->outbuf_length -= r;
        
        if (c->outbuf_length)
            memmove(c->outbuf, c->outbuf + r, c->outbuf_length - r);

        if (c->outbuf_length == 0 && c->state == CLIENT_DEAD) {
            client_free(c);
            return;
        }
    }

    c->poll_fd.events =
        G_IO_ERR |
        G_IO_HUP |
        (c->outbuf_length > 0 ? G_IO_OUT : 0) |
        (c->inbuf_length < sizeof(c->inbuf) ? G_IO_IN : 0);
}

static gboolean prepare_func(GSource *source, gint *timeout) {
    g_assert(source);
    g_assert(timeout);
    
    *timeout = -1;
    return FALSE;
}

static gboolean check_func(GSource *source) {
    Server *s = (Server*) source;
    Client *c;
    
    g_assert(s);

    if (s->poll_fd.revents)
        return TRUE;
    
    for (c = s->clients; c; c = c->clients_next)
        if (c->poll_fd.revents)
            return TRUE;

    return FALSE;
}

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer user_data) {
    Server *s = (Server*) source;
    Client *c, *n;
    
    g_assert(s);

    if (s->poll_fd.revents & G_IO_IN) {
        gint fd;

        if ((fd = accept(s->fd, NULL, NULL)) < 0)
            avahi_log_warn("accept(): %s", strerror(errno));
        else
            client_new(s, fd);
    } else if (s->poll_fd.revents)
        g_error("Invalid revents");

    for (c = s->clients; c; c = n) {
        n = c->clients_next;
        if (c->poll_fd.revents)
            client_work(c);
    }
    
    return TRUE;
}

int simple_protocol_setup(GMainContext *c) {
    struct sockaddr_un sa;
    mode_t u;

    static GSourceFuncs source_funcs = {
        prepare_func,
        check_func,
        dispatch_func,
        NULL,
        NULL,
        NULL
    };
    
    g_assert(!server);

    server = (Server*) g_source_new(&source_funcs, sizeof(Server));
    server->bind_successful = FALSE;
    server->fd = -1;
    AVAHI_LLIST_HEAD_INIT(Client, server->clients);
    g_main_context_ref(server->context = (c ? c : g_main_context_default()));
    server->clients = NULL;

    u = umask(0000);

    if ((server->fd = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
        avahi_log_warn("socket(PF_LOCAL, SOCK_STREAM, 0): %s", strerror(errno));
        goto fail;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_LOCAL;
    strncpy(sa.sun_path, AVAHI_SOCKET, sizeof(sa.sun_path)-1);

    /* We simply remove existing UNIX sockets under this name. The
       Avahi daemons makes sure that it runs only once on a host,
       therefore sockets that already exist are stale and may be
       removed without any ill effects */

    unlink(AVAHI_SOCKET);
    
    if (bind(server->fd, &sa, sizeof(sa)) < 0) {
        avahi_log_warn("bind(): %s", strerror(errno));
        goto fail;
    }

    server->bind_successful = TRUE;
    
    if (listen(server->fd, 2) < 0) {
        avahi_log_warn("listen(): %s", strerror(errno));
        goto fail;
    }

    umask(u);

    memset(&server->poll_fd, 0, sizeof(GPollFD));
    server->poll_fd.fd = server->fd;
    server->poll_fd.events = G_IO_IN|G_IO_ERR;
    g_source_add_poll(&server->source, &server->poll_fd);

    g_source_attach(&server->source, server->context);
    
    return 0;

fail:
    
    umask(u);
    simple_protocol_shutdown();

    return -1;
}

void simple_protocol_shutdown(void) {

    if (server) {

        while (server->clients)
            client_free(server->clients);

        if (server->bind_successful)
            unlink(AVAHI_SOCKET);
        
        if (server->fd >= 0)
            close(server->fd);

        g_main_context_unref(server->context);
        
        g_source_destroy(&server->source);
        g_source_unref(&server->source);

        server = NULL;
    }
}

void simple_protocol_restart_queries(void) {
    Client *c;

    /* Restart queries in case of local domain name changes */
    
    g_assert(server);

    for (c = server->clients; c; c = c->clients_next)
        if (c->state == CLIENT_BROWSE_DNS_SERVERS && c->dns_server_browser) {
            avahi_dns_server_browser_free(c->dns_server_browser);
            c->dns_server_browser = avahi_dns_server_browser_new(avahi_server, -1, AF_UNSPEC, NULL, AVAHI_DNS_SERVER_RESOLVE, c->afquery, dns_server_browser_callback, c);
        }
}


