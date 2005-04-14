#ifndef fooifacehfoo
#define fooifacehfoo

#include <glib.h>

typedef struct _flxInterfaceMonitor flxInterfaceMonitor;
typedef struct _flxInterfaceAddress flxInterfaceAddress;
typedef struct _flxInterface flxInterface;
typedef struct _flxHwInterface flxHwInterface;

#include "address.h"
#include "server.h"
#include "netlink.h"
#include "cache.h"
#include "llist.h"
#include "psched.h"
#include "dns.h"
#include "announce.h"

struct _flxInterfaceMonitor {
    flxServer *server;
    flxNetlink *netlink;
    GHashTable *hash_table;

    FLX_LLIST_HEAD(flxInterface, interfaces);
    FLX_LLIST_HEAD(flxHwInterface, hw_interfaces);
    
    guint query_addr_seq, query_link_seq;
    
    enum {
        LIST_IFACE,
        LIST_ADDR,
        LIST_DONE
    } list;
};

struct _flxHwInterface {
    FLX_LLIST_FIELDS(flxHwInterface, hardware);
    flxInterfaceMonitor *monitor;

    gchar *name;
    gint index;
    guint flags;
    guint mtu;

    FLX_LLIST_HEAD(flxInterface, interfaces);
};

struct _flxInterface {
    FLX_LLIST_FIELDS(flxInterface, interface);
    FLX_LLIST_FIELDS(flxInterface, by_hardware);
    flxInterfaceMonitor *monitor;
    
    flxHwInterface *hardware;
    guchar protocol;
    gboolean announcing;

    flxCache *cache;
    flxPacketScheduler *scheduler;

    FLX_LLIST_HEAD(flxInterfaceAddress, addresses);
    FLX_LLIST_HEAD(flxAnnouncement, announcements);
};

struct _flxInterfaceAddress {
    FLX_LLIST_FIELDS(flxInterfaceAddress, address);
    flxInterfaceMonitor *monitor;
    
    guchar flags;
    guchar scope;
    flxAddress address;
    
    gint rr_id;
    flxInterface *interface;
};

flxInterfaceMonitor *flx_interface_monitor_new(flxServer *server);
void flx_interface_monitor_free(flxInterfaceMonitor *m);

flxInterface* flx_interface_monitor_get_interface(flxInterfaceMonitor *m, gint index, guchar protocol);
flxHwInterface* flx_interface_monitor_get_hw_interface(flxInterfaceMonitor *m, gint index);

void flx_interface_send_packet(flxInterface *i, flxDnsPacket *p);

void flx_interface_post_query(flxInterface *i, flxKey *k, gboolean immediately);
void flx_interface_post_probe(flxInterface *i, flxRecord *p, gboolean immediately);
void flx_interface_post_response(flxInterface *i, const flxAddress *a, flxRecord *record, gboolean flush_cache, gboolean immediately);

void flx_dump_caches(flxInterfaceMonitor *m, FILE *f);

gboolean flx_interface_relevant(flxInterface *i);
gboolean flx_interface_address_relevant(flxInterfaceAddress *a);

gboolean flx_interface_match(flxInterface *i, gint index, guchar protocol);

typedef void (*flxInterfaceMonitorWalkCallback)(flxInterfaceMonitor *m, flxInterface *i, gpointer userdata);
    
void flx_interface_monitor_walk(flxInterfaceMonitor *m, gint index, guchar protocol, flxInterfaceMonitorWalkCallback callback, gpointer userdata);

#endif
