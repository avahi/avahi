#ifndef fooifacehfoo
#define fooifacehfoo

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

typedef struct AvahiInterfaceMonitor AvahiInterfaceMonitor;
typedef struct AvahiInterfaceAddress AvahiInterfaceAddress;
typedef struct AvahiInterface AvahiInterface;
typedef struct AvahiHwInterface AvahiHwInterface;

#include <avahi-common/llist.h>
#include <avahi-common/address.h>

#include "server.h"
#include "netlink.h"
#include "cache.h"
#include "response-sched.h"
#include "query-sched.h"
#include "probe-sched.h"
#include "dns.h"
#include "announce.h"

#define AVAHI_MAX_MAC_ADDRESS 32

struct AvahiInterfaceMonitor {
    AvahiServer *server;
    AvahiNetlink *netlink;
    AvahiHashmap *hashmap;

    AVAHI_LLIST_HEAD(AvahiInterface, interfaces);
    AVAHI_LLIST_HEAD(AvahiHwInterface, hw_interfaces);
    
    unsigned query_addr_seq, query_link_seq;
    
    enum {
        LIST_IFACE,
        LIST_ADDR,
        LIST_DONE
    } list;
};

struct AvahiHwInterface {
    AVAHI_LLIST_FIELDS(AvahiHwInterface, hardware);
    AvahiInterfaceMonitor *monitor;

    char *name;
    AvahiIfIndex index;
    unsigned flags;
    unsigned mtu;

    uint8_t mac_address[AVAHI_MAX_MAC_ADDRESS];
    size_t mac_address_size;

    AvahiSEntryGroup *entry_group;

    AVAHI_LLIST_HEAD(AvahiInterface, interfaces);
};

struct AvahiInterface {
    AVAHI_LLIST_FIELDS(AvahiInterface, interface);
    AVAHI_LLIST_FIELDS(AvahiInterface, by_hardware);
    AvahiInterfaceMonitor *monitor;
    
    AvahiHwInterface *hardware;
    AvahiProtocol protocol;
    int announcing;

    AvahiCache *cache;
    AvahiQueryScheduler *query_scheduler;
    AvahiResponseScheduler * response_scheduler;
    AvahiProbeScheduler *probe_scheduler;

    AVAHI_LLIST_HEAD(AvahiInterfaceAddress, addresses);
    AVAHI_LLIST_HEAD(AvahiAnnouncement, announcements);
};

struct AvahiInterfaceAddress {
    AVAHI_LLIST_FIELDS(AvahiInterfaceAddress, address);
    AvahiInterfaceMonitor *monitor;
    
    unsigned char flags;
    unsigned char scope;
    unsigned char prefix_len;
    AvahiAddress address;
    
    AvahiSEntryGroup *entry_group;
    AvahiInterface *interface;
};

AvahiInterfaceMonitor *avahi_interface_monitor_new(AvahiServer *server);
void avahi_interface_monitor_free(AvahiInterfaceMonitor *m);

void avahi_interface_monitor_sync(AvahiInterfaceMonitor *m);

AvahiInterface* avahi_interface_monitor_get_interface(AvahiInterfaceMonitor *m, AvahiIfIndex idx, AvahiProtocol protocol);
AvahiHwInterface* avahi_interface_monitor_get_hw_interface(AvahiInterfaceMonitor *m, int idx);

void avahi_interface_send_packet(AvahiInterface *i, AvahiDnsPacket *p);
void avahi_interface_send_packet_unicast(AvahiInterface *i, AvahiDnsPacket *p, const AvahiAddress *a, uint16_t port);

int avahi_interface_post_query(AvahiInterface *i, AvahiKey *k, int immediately);
int avahi_interface_post_response(AvahiInterface *i, AvahiRecord *record, int flush_cache, const AvahiAddress *querier, int immediately);
int avahi_interface_post_probe(AvahiInterface *i, AvahiRecord *p, int immediately);

int avahi_dump_caches(AvahiInterfaceMonitor *m, AvahiDumpCallback callback, void* userdata);

int avahi_interface_relevant(AvahiInterface *i);
int avahi_interface_address_relevant(AvahiInterfaceAddress *a);

int avahi_interface_match(AvahiInterface *i, AvahiIfIndex idx, AvahiProtocol protocol);

typedef void (*AvahiInterfaceMonitorWalkCallback)(AvahiInterfaceMonitor *m, AvahiInterface *i, void* userdata);
    
void avahi_interface_monitor_walk(AvahiInterfaceMonitor *m, AvahiIfIndex idx, AvahiProtocol protocol, AvahiInterfaceMonitorWalkCallback callback, void* userdata);

void avahi_update_host_rrs(AvahiInterfaceMonitor *m, int remove_rrs);

int avahi_address_is_local(AvahiInterfaceMonitor *m, const AvahiAddress *a);

int avahi_interface_address_on_link(AvahiInterface *i, const AvahiAddress *a);

int avahi_interface_has_address(AvahiInterfaceMonitor *m, AvahiIfIndex iface, const AvahiAddress *a);

#endif
