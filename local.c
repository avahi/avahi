#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <net/if.h>

#include "flx.h"
#include "server.h"
#include "util.h"
#include "iface.h"

typedef struct {
    flxServer *server;
    flxAddress address;
    gint id;
} addr_info;

struct _flxLocalAddrSource {
    flxServer *server;
    GHashTable *hash_table;
    gint hinfo_id;
    gchar *hostname;
};

static gboolean addr_equal(gconstpointer a, gconstpointer b) {
    return flx_address_cmp(a, b) == 0;
}

static guint hash(gconstpointer v, guint l) {
    const guint8 *c;
    guint hash = 0;

    for (c = v; l > 0; c++, l--)
        hash = 31 * hash + *c;

    return hash;
}

static guint addr_hash(gconstpointer v) {
    const flxAddress *a = v;

    return hash(a, sizeof(a->family) + flx_address_get_size(a));
}

static void add_addr(flxLocalAddrSource *l, const flxInterfaceAddress *a) {
    addr_info *ai;
    gchar *r;
    g_assert(l);
    g_assert(a);

    if (!(a->interface->flags & IFF_UP) ||
        !(a->interface->flags & IFF_RUNNING) ||
        (a->interface->flags & IFF_LOOPBACK))
        return;

    if (a->scope != RT_SCOPE_UNIVERSE)
        return;
    
    ai = g_new(addr_info, 1);
    ai->server = l->server;
    ai->address = a->address;
    
    ai->id = flx_server_get_next_id(l->server);
    flx_server_add(l->server, ai->id, l->hostname,
                   ai->address.family == AF_INET ? FLX_DNS_TYPE_A : FLX_DNS_TYPE_AAAA,
                   ai->address.data, flx_address_get_size(&ai->address));

    r = flx_reverse_lookup_name(&ai->address);
    flx_server_add(l->server, ai->id, r,
                   FLX_DNS_TYPE_PTR,
                   l->hostname, strlen(l->hostname)+1);
    g_free(r);

    g_hash_table_replace(l->hash_table, &ai->address, ai);

}

/* Called whenever a new address becomes available, is changed or removed on the local machine */
static void addr_callback(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterfaceAddress *a, gpointer userdata) {
    flxLocalAddrSource *l = userdata;
    g_assert(m);
    g_assert(a);
    g_assert(l);

    if (change == FLX_INTERFACE_REMOVE)
        g_hash_table_remove(l->hash_table, &a->address);
    else if (change == FLX_INTERFACE_NEW)
        add_addr(l, a);
}

static void destroy(gpointer data) {
    addr_info *ai = data;
    flx_server_remove(ai->server, ai->id);
    g_free(ai);
}

flxLocalAddrSource *flx_local_addr_source_new(flxServer *s) {
    flxLocalAddrSource *l;
    const flxInterface *i;
    struct utsname utsname;
    gint length;
    gchar *e, *hn, *c;

    l = g_new(flxLocalAddrSource, 1);
    l->server = s;
    l->hash_table = g_hash_table_new_full(addr_hash, addr_equal, NULL, destroy);

    hn = flx_get_host_name();
    if ((e = strchr(hn, '.')))
        *e = 0;

    l->hostname = g_strdup_printf("%s.local.", hn);
    g_free(hn);

    flx_interface_monitor_add_address_callback(s->monitor, addr_callback, l);

    for (i = flx_interface_monitor_get_first(s->monitor); i; i = i->next) {
        flxInterfaceAddress *a;

        for (a = i->addresses; a; a = a->next)
            add_addr(l, a);
    }

    l->hinfo_id = flx_server_get_next_id(l->server);

    uname(&utsname);
    c = g_strdup_printf("%s%c%s%n", g_strup(utsname.machine), 0, g_strup(utsname.sysname), &length);
    
    flx_server_add(l->server, l->hinfo_id, l->hostname,
                   FLX_DNS_TYPE_HINFO,
                   c, length+1);
    g_free(c);
    
    return l;
}

void flx_local_addr_source_free(flxLocalAddrSource *l) {
    g_assert(l);
    
    flx_interface_monitor_remove_address_callback(l->server->monitor, addr_callback, l);
    g_hash_table_destroy(l->hash_table);
    flx_server_remove(l->server, l->hinfo_id);
    g_free(l->hostname);
    g_free(l);
}
