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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <errno.h>
#include <net/if.h>

#include "iface.h"
#include "netlink.h"
#include "dns.h"
#include "socket.h"
#include "announce.h"

static void update_address_rr(AvahiInterfaceMonitor *m, AvahiInterfaceAddress *a, int remove) {
    g_assert(m);
    g_assert(a);

    if (!avahi_interface_address_relevant(a) || remove) {
        if (a->entry_group) {
            avahi_entry_group_free(a->entry_group);
            a->entry_group = NULL;
        }
    } else {
        if (!a->entry_group) {
            a->entry_group = avahi_entry_group_new(m->server, NULL, NULL);
            avahi_server_add_address(m->server, a->entry_group, a->interface->hardware->index, AF_UNSPEC, 0, NULL, &a->address); 
            avahi_entry_group_commit(a->entry_group);
        }
    }
}

static void update_interface_rr(AvahiInterfaceMonitor *m, AvahiInterface *i, int remove) {
    AvahiInterfaceAddress *a;
    g_assert(m);
    g_assert(i);

    for (a = i->addresses; a; a = a->address_next)
        update_address_rr(m, a, remove);
}

static void update_hw_interface_rr(AvahiInterfaceMonitor *m, AvahiHwInterface *hw, int remove) {
    AvahiInterface *i;

    g_assert(m);
    g_assert(hw);

    for (i = hw->interfaces; i; i = i->by_hardware_next)
        update_interface_rr(m, i, remove);
}

static void free_address(AvahiInterfaceMonitor *m, AvahiInterfaceAddress *a) {
    g_assert(m);
    g_assert(a);
    g_assert(a->interface);

    AVAHI_LLIST_REMOVE(AvahiInterfaceAddress, address, a->interface->addresses, a);

    if (a->entry_group)
        avahi_entry_group_free(a->entry_group);
    
    g_free(a);
}

static void free_interface(AvahiInterfaceMonitor *m, AvahiInterface *i, gboolean send_goodbye) {
    g_assert(m);
    g_assert(i);

    g_message("removing interface %s.%i", i->hardware->name, i->protocol);
    avahi_goodbye_interface(m->server, i, send_goodbye);
    g_message("flushing...");
    avahi_packet_scheduler_flush_responses(i->scheduler);
    g_message("done");
    
    g_assert(!i->announcements);

    while (i->addresses)
        free_address(m, i->addresses);

    avahi_packet_scheduler_free(i->scheduler);
    avahi_cache_free(i->cache);
    
    AVAHI_LLIST_REMOVE(AvahiInterface, interface, m->interfaces, i);
    AVAHI_LLIST_REMOVE(AvahiInterface, by_hardware, i->hardware->interfaces, i);
    
    g_free(i);
}

static void free_hw_interface(AvahiInterfaceMonitor *m, AvahiHwInterface *hw, gboolean send_goodbye) {
    g_assert(m);
    g_assert(hw);

    while (hw->interfaces)
        free_interface(m, hw->interfaces, send_goodbye);

    AVAHI_LLIST_REMOVE(AvahiHwInterface, hardware, m->hw_interfaces, hw);
    g_hash_table_remove(m->hash_table, &hw->index);

    g_free(hw->name);
    g_free(hw);
}

static AvahiInterfaceAddress* get_address(AvahiInterfaceMonitor *m, AvahiInterface *i, const AvahiAddress *raddr) {
    AvahiInterfaceAddress *ia;
    
    g_assert(m);
    g_assert(i);
    g_assert(raddr);

    for (ia = i->addresses; ia; ia = ia->address_next)
        if (avahi_address_cmp(&ia->address, raddr) == 0)
            return ia;

    return NULL;
}

static int netlink_list_items(AvahiNetlink *nl, guint16 type, guint *ret_seq) {
    struct nlmsghdr *n;
    struct rtgenmsg *gen;
    guint8 req[1024];
    
    memset(&req, 0, sizeof(req));
    n = (struct nlmsghdr*) req;
    n->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    n->nlmsg_type = type;
    n->nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
    n->nlmsg_pid = 0;

    gen = NLMSG_DATA(n);
    memset(gen, 0, sizeof(struct rtgenmsg));
    gen->rtgen_family = AF_UNSPEC;

    return avahi_netlink_send(nl, n, ret_seq);
}

static void new_interface(AvahiInterfaceMonitor *m, AvahiHwInterface *hw, guchar protocol) {
    AvahiInterface *i;
    
    g_assert(m);
    g_assert(hw);
    g_assert(protocol != AF_UNSPEC);

    i = g_new(AvahiInterface, 1);
    i->monitor = m;
    i->hardware = hw;
    i->protocol = protocol;
    i->announcing = FALSE;

    AVAHI_LLIST_HEAD_INIT(AvahiInterfaceAddress, i->addresses);
    AVAHI_LLIST_HEAD_INIT(AvahiAnnouncement, i->announcements);

    i->cache = avahi_cache_new(m->server, i);
    i->scheduler = avahi_packet_scheduler_new(m->server, i);

    AVAHI_LLIST_PREPEND(AvahiInterface, by_hardware, hw->interfaces, i);
    AVAHI_LLIST_PREPEND(AvahiInterface, interface, m->interfaces, i);
}

static void check_interface_relevant(AvahiInterfaceMonitor *m, AvahiInterface *i) {
    gboolean b;

    g_assert(m);
    g_assert(i);

    b = avahi_interface_relevant(i);

    if (b && !i->announcing) {
        g_message("New relevant interface %s.%i", i->hardware->name, i->protocol);

        if (i->protocol == AF_INET)
            avahi_mdns_mcast_join_ipv4 (i->hardware->index, m->server->fd_ipv4);
        if (i->protocol == AF_INET6)
            avahi_mdns_mcast_join_ipv6 (i->hardware->index, m->server->fd_ipv6);

        i->announcing = TRUE;
        avahi_announce_interface(m->server, i);
    } else if (!b && i->announcing) {
        g_message("Interface %s.%i no longer relevant", i->hardware->name, i->protocol);

        if (i->protocol == AF_INET)
            avahi_mdns_mcast_leave_ipv4 (i->hardware->index, m->server->fd_ipv4);
        if (i->protocol == AF_INET6)
            avahi_mdns_mcast_leave_ipv6 (i->hardware->index, m->server->fd_ipv6);

        avahi_goodbye_interface(m->server, i, FALSE);
        avahi_cache_flush(i->cache);

        i->announcing = FALSE;
    }
}

static void check_hw_interface_relevant(AvahiInterfaceMonitor *m, AvahiHwInterface *hw) {
    AvahiInterface *i;
    
    g_assert(m);
    g_assert(hw);

    for (i = hw->interfaces; i; i = i->by_hardware_next)
        check_interface_relevant(m, i);
}

static void callback(AvahiNetlink *nl, struct nlmsghdr *n, gpointer userdata) {
    AvahiInterfaceMonitor *m = userdata;
    
    g_assert(m);
    g_assert(n);
    g_assert(m->netlink == nl);

    if (n->nlmsg_type == RTM_NEWLINK) {
        struct ifinfomsg *ifinfomsg = NLMSG_DATA(n);
        AvahiHwInterface *hw;
        struct rtattr *a = NULL;
        size_t l;

        if (ifinfomsg->ifi_family != AF_UNSPEC)
            return;

        if (!(hw = g_hash_table_lookup(m->hash_table, &ifinfomsg->ifi_index))) {
            hw = g_new(AvahiHwInterface, 1);
            hw->monitor = m;
            hw->name = NULL;
            hw->flags = 0;
            hw->mtu = 1500;
            hw->index = ifinfomsg->ifi_index;

            AVAHI_LLIST_HEAD_INIT(AvahiInterface, hw->interfaces);
            AVAHI_LLIST_PREPEND(AvahiHwInterface, hardware, m->hw_interfaces, hw);
            
            g_hash_table_insert(m->hash_table, &hw->index, hw);

            if (m->server->fd_ipv4 >= 0)
                new_interface(m, hw, AF_INET);
            if (m->server->fd_ipv6 >= 0)
                new_interface(m, hw, AF_INET6);
        }
        
        hw->flags = ifinfomsg->ifi_flags;

        l = NLMSG_PAYLOAD(n, sizeof(struct ifinfomsg));
        a = IFLA_RTA(ifinfomsg);

        while (RTA_OK(a, l)) {
            switch(a->rta_type) {
                case IFLA_IFNAME:
                    g_free(hw->name);
                    hw->name = g_strndup(RTA_DATA(a), RTA_PAYLOAD(a));
                    break;

                case IFLA_MTU:
                    g_assert(RTA_PAYLOAD(a) == sizeof(unsigned int));
                    hw->mtu = *((unsigned int*) RTA_DATA(a));
                    break;
                    
                default:
                    ;
            }

            a = RTA_NEXT(a, l);
        }

        update_hw_interface_rr(m, hw, FALSE);
        check_hw_interface_relevant(m, hw);
        
    } else if (n->nlmsg_type == RTM_DELLINK) {
        struct ifinfomsg *ifinfomsg = NLMSG_DATA(n);
        AvahiHwInterface *hw;

        if (ifinfomsg->ifi_family != AF_UNSPEC)
            return;
        
        if (!(hw = avahi_interface_monitor_get_hw_interface(m, ifinfomsg->ifi_index)))
            return;

        update_hw_interface_rr(m, hw, TRUE);
        free_hw_interface(m, hw, FALSE);
        
    } else if (n->nlmsg_type == RTM_NEWADDR || n->nlmsg_type == RTM_DELADDR) {

        struct ifaddrmsg *ifaddrmsg = NLMSG_DATA(n);
        AvahiInterface *i;
        struct rtattr *a = NULL;
        size_t l;
        AvahiAddress raddr;
        int raddr_valid = 0;

        if (ifaddrmsg->ifa_family != AF_INET && ifaddrmsg->ifa_family != AF_INET6)
            return;

        if (!(i = (AvahiInterface*) avahi_interface_monitor_get_interface(m, ifaddrmsg->ifa_index, ifaddrmsg->ifa_family)))
            return;

        raddr.family = ifaddrmsg->ifa_family;

        l = NLMSG_PAYLOAD(n, sizeof(struct ifinfomsg));
        a = IFA_RTA(ifaddrmsg);

        while (RTA_OK(a, l)) {
            switch(a->rta_type) {
                case IFA_ADDRESS:
                    if ((raddr.family == AF_INET6 && RTA_PAYLOAD(a) != 16) ||
                        (raddr.family == AF_INET && RTA_PAYLOAD(a) != 4))
                        return;

                    memcpy(raddr.data.data, RTA_DATA(a), RTA_PAYLOAD(a));
                    raddr_valid = 1;

                    break;
                    
                default:
                    ;
            }
            
            a = RTA_NEXT(a, l);
        }

        
        if (!raddr_valid)
            return;

        if (n->nlmsg_type == RTM_NEWADDR) {
            AvahiInterfaceAddress *addr;
            
            if (!(addr = get_address(m, i, &raddr))) {
                addr = g_new(AvahiInterfaceAddress, 1);
                addr->monitor = m;
                addr->address = raddr;
                addr->interface = i;
                addr->entry_group = NULL;

                AVAHI_LLIST_PREPEND(AvahiInterfaceAddress, address, i->addresses, addr);
            }
            
            addr->flags = ifaddrmsg->ifa_flags;
            addr->scope = ifaddrmsg->ifa_scope;

            update_address_rr(m, addr, FALSE);
            check_interface_relevant(m, i);
        } else {
            AvahiInterfaceAddress *addr;
            
            if (!(addr = get_address(m, i, &raddr)))
                return;

            update_address_rr(m, addr, TRUE);
            free_address(m, addr);

            check_interface_relevant(m, i);
        }
                
    } else if (n->nlmsg_type == NLMSG_DONE) {
        
        if (m->list == LIST_IFACE) {
            m->list = LIST_DONE;
            
            if (netlink_list_items(m->netlink, RTM_GETADDR, &m->query_addr_seq) < 0)
                g_warning("NETLINK: Failed to list addrs: %s", strerror(errno));
            else
                m->list = LIST_ADDR;
        } else {
            m->list = LIST_DONE;
            g_message("Enumeration complete");
        }
        
    } else if (n->nlmsg_type == NLMSG_ERROR && (n->nlmsg_seq == m->query_link_seq || n->nlmsg_seq == m->query_addr_seq)) {
        struct nlmsgerr *e = NLMSG_DATA (n);
                    
        if (e->error)
            g_warning("NETLINK: Failed to browse: %s", strerror(-e->error));
    }
}

AvahiInterfaceMonitor *avahi_interface_monitor_new(AvahiServer *s) {
    AvahiInterfaceMonitor *m = NULL;

    m = g_new0(AvahiInterfaceMonitor, 1);
    m->server = s;
    if (!(m->netlink = avahi_netlink_new(s->context, G_PRIORITY_DEFAULT-10, RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR, callback, m)))
        goto fail;

    m->hash_table = g_hash_table_new(g_int_hash, g_int_equal);

    AVAHI_LLIST_HEAD_INIT(AvahiInterface, m->interfaces);
    AVAHI_LLIST_HEAD_INIT(AvahiHwInterface, m->hw_interfaces);

    if (netlink_list_items(m->netlink, RTM_GETLINK, &m->query_link_seq) < 0)
        goto fail;

    m->list = LIST_IFACE;

    return m;

fail:
    avahi_interface_monitor_free(m);
    return NULL;
}

void avahi_interface_monitor_sync(AvahiInterfaceMonitor *m) {
    g_assert(m);
    
    while (m->list != LIST_DONE) {
        if (!avahi_netlink_work(m->netlink, TRUE))
            break;
    } 
}

void avahi_interface_monitor_free(AvahiInterfaceMonitor *m) {
    g_assert(m);

    while (m->hw_interfaces)
        free_hw_interface(m, m->hw_interfaces, TRUE);

    g_assert(!m->interfaces);

    
    if (m->netlink)
        avahi_netlink_free(m->netlink);
    
    if (m->hash_table)
        g_hash_table_destroy(m->hash_table);

    g_free(m);
}


AvahiInterface* avahi_interface_monitor_get_interface(AvahiInterfaceMonitor *m, gint index, guchar protocol) {
    AvahiHwInterface *hw;
    AvahiInterface *i;
    
    g_assert(m);
    g_assert(index > 0);
    g_assert(protocol != AF_UNSPEC);

    if (!(hw = avahi_interface_monitor_get_hw_interface(m, index)))
        return NULL;

    for (i = hw->interfaces; i; i = i->by_hardware_next)
        if (i->protocol == protocol)
            return i;

    return NULL;
}

AvahiHwInterface* avahi_interface_monitor_get_hw_interface(AvahiInterfaceMonitor *m, gint index) {
    g_assert(m);
    g_assert(index > 0);

    return g_hash_table_lookup(m->hash_table, &index);
}


void avahi_interface_send_packet_unicast(AvahiInterface *i, AvahiDnsPacket *p, const AvahiAddress *a, guint16 port) {
    g_assert(i);
    g_assert(p);
    char t[64];

    if (!avahi_interface_relevant(i))
        return;
    
    g_assert(!a || a->family == i->protocol);

    if (a)
        g_message("unicast sending on '%s.%i' to %s:%u", i->hardware->name, i->protocol, avahi_address_snprint(t, sizeof(t), a), port);
    else
        g_message("multicast sending on '%s.%i'", i->hardware->name, i->protocol);
    
    if (i->protocol == AF_INET && i->monitor->server->fd_ipv4 >= 0)
        avahi_send_dns_packet_ipv4(i->monitor->server->fd_ipv4, i->hardware->index, p, a ? &a->data.ipv4 : NULL, port);
    else if (i->protocol == AF_INET6 && i->monitor->server->fd_ipv6 >= 0)
        avahi_send_dns_packet_ipv6(i->monitor->server->fd_ipv6, i->hardware->index, p, a ? &a->data.ipv6 : NULL, port);
}

void avahi_interface_send_packet(AvahiInterface *i, AvahiDnsPacket *p) {
    g_assert(i);
    g_assert(p);

    avahi_interface_send_packet_unicast(i, p, NULL, 0);
}

gboolean avahi_interface_post_query(AvahiInterface *i, AvahiKey *key, gboolean immediately) {
    g_assert(i);
    g_assert(key);

    if (avahi_interface_relevant(i))
        return avahi_packet_scheduler_post_query(i->scheduler, key, immediately);

    return FALSE;
}

gboolean avahi_interface_post_response(AvahiInterface *i, AvahiRecord *record, gboolean flush_cache, gboolean immediately, const AvahiAddress *querier) {
    g_assert(i);
    g_assert(record);

    if (avahi_interface_relevant(i))
        return avahi_packet_scheduler_post_response(i->scheduler, record, flush_cache, immediately, querier);

    return FALSE;
}

gboolean avahi_interface_post_probe(AvahiInterface *i, AvahiRecord *record, gboolean immediately) {
    g_assert(i);
    g_assert(record);
    
    if (avahi_interface_relevant(i))
        return avahi_packet_scheduler_post_probe(i->scheduler, record, immediately);

    return FALSE;
}

void avahi_dump_caches(AvahiInterfaceMonitor *m, FILE *f) {
    AvahiInterface *i;
    g_assert(m);

    for (i = m->interfaces; i; i = i->interface_next) {
        if (avahi_interface_relevant(i)) {
            fprintf(f, "\n;;; INTERFACE %s.%i ;;;\n", i->hardware->name, i->protocol);
            avahi_cache_dump(i->cache, f);
        }
    }
    fprintf(f, "\n");
}

gboolean avahi_interface_relevant(AvahiInterface *i) {
    g_assert(i);

    return
        (i->hardware->flags & IFF_UP) &&
        (i->hardware->flags & IFF_RUNNING) &&
        !(i->hardware->flags & IFF_LOOPBACK) &&
        (i->hardware->flags & IFF_MULTICAST) &&
        i->addresses;
}

gboolean avahi_interface_address_relevant(AvahiInterfaceAddress *a) { 
    g_assert(a);

    return a->scope == RT_SCOPE_UNIVERSE;
}


gboolean avahi_interface_match(AvahiInterface *i, gint index, guchar protocol) {
    g_assert(i);
    
    if (index > 0 && index != i->hardware->index)
        return FALSE;

    if (protocol != AF_UNSPEC && protocol != i->protocol)
        return FALSE;

    return TRUE;
}


void avahi_interface_monitor_walk(AvahiInterfaceMonitor *m, gint interface, guchar protocol, AvahiInterfaceMonitorWalkCallback callback, gpointer userdata) {
    g_assert(m);
    g_assert(callback);
    
    if (interface > 0) {
        if (protocol != AF_UNSPEC) {
            AvahiInterface *i;
            
            if ((i = avahi_interface_monitor_get_interface(m, interface, protocol)))
                callback(m, i, userdata);
            
        } else {
            AvahiHwInterface *hw;
            AvahiInterface *i;

            if ((hw = avahi_interface_monitor_get_hw_interface(m, interface)))
                for (i = hw->interfaces; i; i = i->by_hardware_next)
                    if (avahi_interface_match(i, interface, protocol))
                        callback(m, i, userdata);
        }
        
    } else {
        AvahiInterface *i;
        
        for (i = m->interfaces; i; i = i->interface_next)
            if (avahi_interface_match(i, interface, protocol))
                callback(m, i, userdata);
    }
}
