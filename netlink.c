#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "netlink.h"

struct _flxNetlink {
    GMainContext *context;
    gint fd;
    guint seq;
    GPollFD poll_fd;
    GSource *source;
    void (*callback) (flxNetlink *nl, struct nlmsghdr *n, gpointer userdata);
    gpointer userdata;
};
static gboolean work(flxNetlink *nl) {
    g_assert(nl);

    for (;;) {
        guint8 replybuf[64*1024];
        ssize_t bytes;
        struct nlmsghdr *p = (struct nlmsghdr *) replybuf;

        if ((bytes = recv(nl->fd, replybuf, sizeof(replybuf), MSG_DONTWAIT)) < 0) {

            if (errno == EAGAIN || errno == EINTR)
                break;

            g_warning("NETLINK: recv() failed");
            return FALSE;
        }

        if (nl->callback) {
            for (; bytes > 0; p = NLMSG_NEXT(p, bytes)) {
                if (!NLMSG_OK(p, bytes)) {
                    g_warning("NETLINK: packet truncated");
                    return FALSE;
                }

                nl->callback(nl, p, nl->userdata);
            }
        }
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
    flxNetlink* nl;
    g_assert(source);

    nl = *((flxNetlink**) (((guint8*) source) + sizeof(GSource)));
    g_assert(nl);
    
    return nl->poll_fd.revents & (G_IO_IN|G_IO_HUP|G_IO_ERR);
}

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer user_data) {
    flxNetlink* nl;
    g_assert(source);

    nl = *((flxNetlink**) (((guint8*) source) + sizeof(GSource)));
    g_assert(nl);
    
    return work(nl);
}

flxNetlink *flx_netlink_new(GMainContext *context, guint32 groups, void (*cb) (flxNetlink *nl, struct nlmsghdr *n, gpointer userdata), gpointer userdata) {
    int fd;
    struct sockaddr_nl addr;
    flxNetlink *nl;

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

    nl = g_new(flxNetlink, 1);
    nl->context = context;
    g_main_context_ref(context);
    nl->fd = fd;
    nl->seq = 0;
    nl->callback = cb;
    nl->userdata = userdata;

    nl->source = g_source_new(&source_funcs, sizeof(GSource) + sizeof(flxNetlink*));
    *((flxNetlink**) (((guint8*) nl->source) + sizeof(GSource))) = nl;

    memset(&nl->poll_fd, 0, sizeof(GPollFD));
    nl->poll_fd.fd = fd;
    nl->poll_fd.events = G_IO_IN|G_IO_ERR|G_IO_HUP;
    g_source_add_poll(nl->source, &nl->poll_fd);
    
    g_source_attach(nl->source, nl->context);
    
    return nl;
}

void flx_netlink_free(flxNetlink *nl) {
    g_assert(nl);
    
    g_source_destroy(nl->source);
    g_source_unref(nl->source);
    g_main_context_unref(nl->context);
    close(nl->fd);
    g_free(nl);
}

int flx_netlink_send(flxNetlink *nl, struct nlmsghdr *m, guint *ret_seq) {
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
