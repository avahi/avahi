#ifndef fooifacehfoo
#define fooifacehfoo

#include <glib.h>

struct _flxInterfaceMonitor;
typedef struct _flxInterfaceMonitor flxInterfaceMonitor;

struct _flxInterfaceAddress;
typedef struct _flxInterfaceAddress flxInterfaceAddress;

struct _flxInterface;
typedef struct _flxInterface flxInterface;

#include "address.h"
#include "server.h"
#include "netlink.h"

struct _flxInterfaceMonitor {
    flxServer *server;
    flxNetlink *netlink;
    GHashTable *hash_table;

    flxInterface *interfaces;
    
    guint query_addr_seq, query_link_seq;
    
    enum { LIST_IFACE, LIST_ADDR, LIST_DONE } list;
};

struct _flxInterface {
    gchar *name;
    gint index;
    guint flags;

    guint n_ipv6_addrs, n_ipv4_addrs;
    
    flxInterfaceAddress *addresses;
    flxInterface *next, *prev;
};

struct _flxInterfaceAddress {
    guchar flags;
    guchar scope;
    flxAddress address;
    
    flxInterface *interface;
    flxInterfaceAddress *next, *prev;

    gint rr_id;
};

flxInterfaceMonitor *flx_interface_monitor_new(flxServer *server);
void flx_interface_monitor_free(flxInterfaceMonitor *m);

const flxInterface* flx_interface_monitor_get_interface(flxInterfaceMonitor *m, gint index);
const flxInterface* flx_interface_monitor_get_first(flxInterfaceMonitor *m);

int flx_interface_is_relevant(flxInterface *i);
int flx_address_is_relevant(flxInterfaceAddress *a);
    
#endif
