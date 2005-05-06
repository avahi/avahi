#ifndef fooifacehfoo
#define fooifacehfoo

#include <glib.h>

typedef struct _AvahiInterfaceMonitor AvahiInterfaceMonitor;
typedef struct _AvahiInterfaceAddress AvahiInterfaceAddress;
typedef struct _AvahiInterface AvahiInterface;
typedef struct _AvahiHwInterface AvahiHwInterface;

#include "address.h"
#include "server.h"
#include "netlink.h"
#include "cache.h"
#include "llist.h"
#include "psched.h"
#include "dns.h"
#include "announce.h"

struct _AvahiInterfaceMonitor {
    AvahiServer *server;
    AvahiNetlink *netlink;
    GHashTable *hash_table;

    AVAHI_LLIST_HEAD(AvahiInterface, interfaces);
    AVAHI_LLIST_HEAD(AvahiHwInterface, hw_interfaces);
    
    guint query_addr_seq, query_link_seq;
    
    enum {
        LIST_IFACE,
        LIST_ADDR,
        LIST_DONE
    } list;
};

struct _AvahiHwInterface {
    AVAHI_LLIST_FIELDS(AvahiHwInterface, hardware);
    AvahiInterfaceMonitor *monitor;

    gchar *name;
    gint index;
    guint flags;
    guint mtu;

    AVAHI_LLIST_HEAD(AvahiInterface, interfaces);
};

struct _AvahiInterface {
    AVAHI_LLIST_FIELDS(AvahiInterface, interface);
    AVAHI_LLIST_FIELDS(AvahiInterface, by_hardware);
    AvahiInterfaceMonitor *monitor;
    
    AvahiHwInterface *hardware;
    guchar protocol;
    gboolean announcing;

    AvahiCache *cache;
    AvahiPacketScheduler *scheduler;

    AVAHI_LLIST_HEAD(AvahiInterfaceAddress, addresses);
    AVAHI_LLIST_HEAD(AvahiAnnouncement, announcements);
};

struct _AvahiInterfaceAddress {
    AVAHI_LLIST_FIELDS(AvahiInterfaceAddress, address);
    AvahiInterfaceMonitor *monitor;
    
    guchar flags;
    guchar scope;
    AvahiAddress address;
    
    AvahiEntryGroup *entry_group;
    AvahiInterface *interface;
};

AvahiInterfaceMonitor *avahi_interface_monitor_new(AvahiServer *server);
void avahi_interface_monitor_free(AvahiInterfaceMonitor *m);

void avahi_interface_monitor_sync(AvahiInterfaceMonitor *m);

AvahiInterface* avahi_interface_monitor_get_interface(AvahiInterfaceMonitor *m, gint index, guchar protocol);
AvahiHwInterface* avahi_interface_monitor_get_hw_interface(AvahiInterfaceMonitor *m, gint index);

void avahi_interface_send_packet(AvahiInterface *i, AvahiDnsPacket *p);

void avahi_interface_post_query(AvahiInterface *i, AvahiKey *k, gboolean immediately);
void avahi_interface_post_probe(AvahiInterface *i, AvahiRecord *p, gboolean immediately);
void avahi_interface_post_response(AvahiInterface *i, const AvahiAddress *a, AvahiRecord *record, gboolean flush_cache, gboolean immediately);

void avahi_dump_caches(AvahiInterfaceMonitor *m, FILE *f);

gboolean avahi_interface_relevant(AvahiInterface *i);
gboolean avahi_interface_address_relevant(AvahiInterfaceAddress *a);

gboolean avahi_interface_match(AvahiInterface *i, gint index, guchar protocol);

typedef void (*AvahiInterfaceMonitorWalkCallback)(AvahiInterfaceMonitor *m, AvahiInterface *i, gpointer userdata);
    
void avahi_interface_monitor_walk(AvahiInterfaceMonitor *m, gint index, guchar protocol, AvahiInterfaceMonitorWalkCallback callback, gpointer userdata);

#endif
