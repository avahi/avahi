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

#include <avahi-core/llist.h>

#include "simple-protocol.h"

#define BUFFER_SIZE (10*1024)

#define UNIX_SOCKET_PATH "/tmp/avahi"
#define UNIX_SOCKET UNIX_SOCKET_PATH"/socket"

#define CLIENTS_MAX 50

typedef struct Client Client;
typedef struct Server Server;

struct Client {
    Server *server;
    
    gint fd;
    GPollFD poll_fd;

    gchar inbuf[BUFFER_SIZE], outbuf[BUFFER_SIZE];
    guint inbuf_length, outbuf_length;
    
    AVAHI_LLIST_FIELDS(Client, clients);
};

struct Server {
    GSource source;
    GMainContext *context;
    GPollFD poll_fd;
    gint fd;
    AVAHI_LLIST_HEAD(Client, clients);

    guint n_clients;
};

static Server *server = NULL;

static void client_free(Client *c) {
    g_assert(c);

    g_assert(c->server->n_clients >= 1);
    c->server->n_clients--;
    
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

    c->inbuf_length = c->outbuf_length = 0;

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
}

static void handle_line(Client *c, const gchar *s) {
    gchar t[256];

    g_assert(c);
    g_assert(s);

    snprintf(t, sizeof(t), "you said <%s>\n", s);
    client_output(c, (guint8*) t, strlen(t));
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
                g_warning("read(): %s", strerror(errno));
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
            g_warning("write(): %s", strerror(errno));
            client_free(c);
            return;
        }

        g_assert((guint) r <= c->outbuf_length);
        c->outbuf_length -= r;
        
        if (c->outbuf_length)
            memmove(c->outbuf, c->outbuf + r, c->outbuf_length - r);
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
            g_warning("accept(): %s", strerror(errno));
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
    server->fd = -1;
    AVAHI_LLIST_HEAD_INIT(Client, server->clients);
    if (c)
        g_main_context_ref(server->context = c);
    else
        server->context = g_main_context_default();
    server->clients = NULL;

    u = umask(0000);

    if (mkdir(UNIX_SOCKET_PATH, 0755) < 0 && errno != EEXIST) {
        g_warning("mkdir(): %s", strerror(errno));
        goto fail;
    }
    
    if ((server->fd = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
        g_warning("socket(PF_LOCAL, SOCK_STREAM, 0): %s", strerror(errno));
        goto fail;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_LOCAL;
    strncpy(sa.sun_path, UNIX_SOCKET, sizeof(sa.sun_path)-1);

    if (bind(server->fd, &sa, sizeof(sa)) < 0) {
        g_warning("bind(): %s", strerror(errno));
        goto fail;
    }
    
    if (listen(server->fd, 2) < 0) {
        g_warning("listen(): %s", strerror(errno));
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
        
        if (server->fd >= 0) {
            unlink(UNIX_SOCKET_PATH);
            close(server->fd);
        }

        g_main_context_unref(server->context);
        g_source_destroy(&server->source);
        g_source_unref(&server->source);

        server = NULL;
    }
}
