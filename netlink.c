#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "netlink.h"

struct _AvahiNetlink {
    GMainContext *context;
    gint fd;
    guint seq;
    GPollFD poll_fd;
    GSource *source;
    void (*callback) (AvahiNetlink *nl, struct nlmsghdr *n, gpointer userdata);
    gpointer userdata;
};

gboolean avahi_netlink_work(AvahiNetlink *nl, gboolean block) {
    g_assert(nl);

    for (;;) {
        guint8 replybuf[64*1024];
        ssize_t bytes;
        struct nlmsghdr *p = (struct nlmsghdr *) replybuf;

        if ((bytes = recv(nl->fd, replybuf, sizeof(replybuf), block ? 0 : MSG_DONTWAIT)) < 0) {

            if (errno == EAGAIN || errno == EINTR)
                break;

            g_warning("NETLINK: recv() failed");
            return FALSE;
        }

        if (nl->callback) {
            for (; bytes > 0; p = NLMSG_NEXT(p, bytes)) {
                if (!NLMSG_OK(p, (size_t) bytes)) {
                    g_warning("NETLINK: packet truncated");
                    return FALSE;
                }

                nl->callback(nl, p, nl->userdata);
            }
        }

        if (block)
            break;
    }

    return TRUE;
}

static gboolean prepare_func(GSource *source, gint *timeout) {
    g_assert(source);
    g_assert(timeout);
    
    *timeout = -1;
    return FALSE;
}

static gboolean check_func(GSource *source) {
    AvahiNetlink* nl;
    g_assert(source);

    nl = *((AvahiNetlink**) (((guint8*) source) + sizeof(GSource)));
    g_assert(nl);
    
    return nl->poll_fd.revents & (G_IO_IN|G_IO_HUP|G_IO_ERR);
}

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer user_data) {
    AvahiNetlink* nl;
    g_assert(source);

    nl = *((AvahiNetlink**) (((guint8*) source) + sizeof(GSource)));
    g_assert(nl);
    
    return avahi_netlink_work(nl, FALSE);
}

AvahiNetlink *avahi_netlink_new(GMainContext *context, gint priority, guint32 groups, void (*cb) (AvahiNetlink *nl, struct nlmsghdr *n, gpointer userdata), gpointer userdata) {
    int fd;
    struct sockaddr_nl addr;
    AvahiNetlink *nl;

    static GSourceFuncs source_funcs = {
        prepare_func,
        check_func,
        dispatch_func,
        NULL,
        NULL,
        NULL
    };
    
    g_assert(context);
    g_assert(cb);

    if ((fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) {
        g_critical("NETLINK: socket(PF_NETLINK): %s", strerror(errno));
        return NULL;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = groups;
    addr.nl_pid = getpid();

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(fd);
        g_critical("bind(): %s", strerror(errno));
        return NULL;
    }

    nl = g_new(AvahiNetlink, 1);
    nl->context = context;
    g_main_context_ref(context);
    nl->fd = fd;
    nl->seq = 0;
    nl->callback = cb;
    nl->userdata = userdata;

    nl->source = g_source_new(&source_funcs, sizeof(GSource) + sizeof(AvahiNetlink*));
    *((AvahiNetlink**) (((guint8*) nl->source) + sizeof(GSource))) = nl;

    g_source_set_priority(nl->source, priority);
    
    memset(&nl->poll_fd, 0, sizeof(GPollFD));
    nl->poll_fd.fd = fd;
    nl->poll_fd.events = G_IO_IN|G_IO_ERR|G_IO_HUP;
    g_source_add_poll(nl->source, &nl->poll_fd);
    
    g_source_attach(nl->source, nl->context);
    
    return nl;
}

void avahi_netlink_free(AvahiNetlink *nl) {
    g_assert(nl);
    
    g_source_destroy(nl->source);
    g_source_unref(nl->source);
    g_main_context_unref(nl->context);
    close(nl->fd);
    g_free(nl);
}

int avahi_netlink_send(AvahiNetlink *nl, struct nlmsghdr *m, guint *ret_seq) {
    g_assert(nl);
    g_assert(m);
    
    m->nlmsg_seq = nl->seq++;
    m->nlmsg_flags |= NLM_F_ACK;

    if (send(nl->fd, m, m->nlmsg_len, 0) < 0) {
        g_warning("NETLINK: send(): %s\n", strerror(errno));
        return -1;
    }

    if (ret_seq)
        *ret_seq = m->nlmsg_seq;

    return 0;
}
