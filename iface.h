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
#include "cache.h"
#include "llist.h"

struct _flxInterfaceMonitor {
    flxServer *server;
    flxNetlink *netlink;
    GHashTable *hash_table;

    FLX_LLIST_HEAD(flxInterface, interfaces);
    
    guint query_addr_seq, query_link_seq;
    
    enum { LIST_IFACE, LIST_ADDR, LIST_DONE } list;
};

struct _flxInterface {
    flxInterfaceMonitor *monitor;
    gchar *name;
    gint index;
    guint flags;

    FLX_LLIST_HEAD(flxInterfaceAddress, addresses);
    FLX_LLIST_FIELDS(flxInterface, interface);

    guint n_ipv6_addrs, n_ipv4_addrs;
    flxCache *ipv4_cache, *ipv6_cache;
};

struct _flxInterfaceAddress {
    guchar flags;
    guchar scope;
    flxAddress address;
    
    flxInterface *interface;

    FLX_LLIST_FIELDS(flxInterfaceAddress, address);

    gint rr_id;
};

flxInterfaceMonitor *flx_interface_monitor_new(flxServer *server);
void flx_interface_monitor_free(flxInterfaceMonitor *m);

flxInterface* flx_interface_monitor_get_interface(flxInterfaceMonitor *m, gint index);
flxInterface* flx_interface_monitor_get_first(flxInterfaceMonitor *m);

int flx_interface_is_relevant(flxInterface *i);
int flx_address_is_relevant(flxInterfaceAddress *a);

void flx_interface_send_query(flxInterface *i, guchar protocol, flxKey *k);
void flx_interface_send_response(flxInterface *i, guchar protocol, flxRecord *rr);

void flx_dump_caches(flxServer *s, FILE *f);

#endif
