#ifndef foonetlinkhfoo
#define foonetlinkhfoo

#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>

#include <glib.h>

struct _AvahiNetlink;
typedef struct _AvahiNetlink AvahiNetlink;

AvahiNetlink *avahi_netlink_new(GMainContext *c, gint priority, guint32 groups, void (*cb) (AvahiNetlink *n, struct nlmsghdr *m, gpointer userdata), gpointer userdata);
void avahi_netlink_free(AvahiNetlink *n);

int avahi_netlink_send(AvahiNetlink *n, struct nlmsghdr *m, guint *ret_seq);

gboolean avahi_netlink_work(AvahiNetlink *n, gboolean block);

#endif
