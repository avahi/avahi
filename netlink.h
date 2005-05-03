#ifndef foonetlinkhfoo
#define foonetlinkhfoo

#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>

#include <glib.h>

struct _flxNetlink;
typedef struct _flxNetlink flxNetlink;

flxNetlink *flx_netlink_new(GMainContext *c, gint priority, guint32 groups, void (*cb) (flxNetlink *n, struct nlmsghdr *m, gpointer userdata), gpointer userdata);
void flx_netlink_free(flxNetlink *n);

int flx_netlink_send(flxNetlink *n, struct nlmsghdr *m, guint *ret_seq);

gboolean flx_netlink_work(flxNetlink *n, gboolean block);

#endif
