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

static void update_address_rr(flxInterfaceMonitor *m, flxInterfaceAddress *a, int remove) {
    g_assert(m);
    g_assert(a);

    if (!flx_interface_address_relevant(a) || remove) {
        if (a->rr_id >= 0) {
            flx_server_remove(m->server, a->rr_id);
            a->rr_id = -1;
        }
    } else {
        if (a->rr_id < 0) {
            a->rr_id = flx_server_get_next_id(m->server);
            flx_server_add_address(m->server, a->rr_id, a->interface->hardware->index, AF_UNSPEC, FALSE, m->server->hostname, &a->address);
        }
    }
}

static void update_interface_rr(flxInterfaceMonitor *m, flxInterface *i, int remove) {
    flxInterfaceAddress *a;
    g_assert(m);
    g_assert(i);

    for (a = i->addresses; a; a = a->address_next)
        update_address_rr(m, a, remove);
}

static void update_hw_interface_rr(flxInterfaceMonitor *m, flxHwInterface *hw, int remove) {
    flxInterface *i;

    g_assert(m);
    g_assert(hw);

    for (i = hw->interfaces; i; i = i->by_hardware_next)
        update_interface_rr(m, i, remove);
}

static void free_address(flxInterfaceMonitor *m, flxInterfaceAddress *a) {
    g_assert(m);
    g_assert(a);
    g_assert(a->interface);

    FLX_LLIST_REMOVE(flxInterfaceAddress, address, a->interface->addresses, a);
    flx_server_remove(m->server, a->rr_id);
    
    g_free(a);
}

static void free_interface(flxInterfaceMonitor *m, flxInterface *i, gboolean send_goodbye) {
    g_assert(m);
    g_assert(i);

    g_message("removing interface %s.%i", i->hardware->name, i->protocol);
    flx_goodbye_interface(m->server, i, send_goodbye);
    g_message("flushing...");
    flx_packet_scheduler_flush_responses(i->scheduler);
    g_message("done");
    
    g_assert(!i->announcements);

    while (i->addresses)
        free_address(m, i->addresses);

    flx_packet_scheduler_free(i->scheduler);
    flx_cache_free(i->cache);
    
    FLX_LLIST_REMOVE(flxInterface, interface, m->interfaces, i);
    FLX_LLIST_REMOVE(flxInterface, by_hardware, i->hardware->interfaces, i);
    
    g_free(i);
}

static void free_hw_interface(flxInterfaceMonitor *m, flxHwInterface *hw, gboolean send_goodbye) {
    g_assert(m);
    g_assert(hw);

    while (hw->interfaces)
        free_interface(m, hw->interfaces, send_goodbye);

    FLX_LLIST_REMOVE(flxHwInterface, hardware, m->hw_interfaces, hw);
    g_hash_table_remove(m->hash_table, &hw->index);

    g_free(hw->name);
    g_free(hw);
}

static flxInterfaceAddress* get_address(flxInterfaceMonitor *m, flxInterface *i, const flxAddress *raddr) {
    flxInterfaceAddress *ia;
    
    g_assert(m);
    g_assert(i);
    g_assert(raddr);

    for (ia = i->addresses; ia; ia = ia->address_next)
        if (flx_address_cmp(&ia->address, raddr) == 0)
            return ia;

    return NULL;
}

static int netlink_list_items(flxNetlink *nl, guint16 type, guint *ret_seq) {
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

    return flx_netlink_send(nl, n, ret_seq);
}

static void new_interface(flxInterfaceMonitor *m, flxHwInterface *hw, guchar protocol) {
    flxInterface *i;
    
    g_assert(m);
    g_assert(hw);
    g_assert(protocol != AF_UNSPEC);

    i = g_new(flxInterface, 1);
    i->monitor = m;
    i->hardware = hw;
    i->protocol = protocol;
    i->announcing = FALSE;

    FLX_LLIST_HEAD_INIT(flxInterfaceAddress, i->addresses);
    FLX_LLIST_HEAD_INIT(flxAnnouncement, i->announcements);

    i->cache = flx_cache_new(m->server, i);
    i->scheduler = flx_packet_scheduler_new(m->server, i);

    FLX_LLIST_PREPEND(flxInterface, by_hardware, hw->interfaces, i);
    FLX_LLIST_PREPEND(flxInterface, interface, m->interfaces, i);
}

static void check_interface_relevant(flxInterfaceMonitor *m, flxInterface *i) {
    gboolean b;
    g_assert(m);
    g_assert(i);

    b = flx_interface_relevant(i);

    if (b && !i->announcing) {
        g_message("New relevant interface %s.%i", i->hardware->name, i->protocol);

        i->announcing = TRUE;
        flx_announce_interface(m->server, i);
    } else if (!b && i->announcing) {
        g_message("Interface %s.%i no longer relevant", i->hardware->name, i->protocol);

        i->announcing = FALSE;
        flx_goodbye_interface(m->server, i, FALSE);
    }
}

static void check_hw_interface_relevant(flxInterfaceMonitor *m, flxHwInterface *hw) {
    flxInterface *i;
    
    g_assert(m);
    g_assert(hw);

    for (i = hw->interfaces; i; i = i->by_hardware_next)
        check_interface_relevant(m, i);
}

static void callback(flxNetlink *nl, struct nlmsghdr *n, gpointer userdata) {
    flxInterfaceMonitor *m = userdata;
    
    g_assert(m);
    g_assert(n);
    g_assert(m->netlink == nl);

    if (n->nlmsg_type == RTM_NEWLINK) {
        struct ifinfomsg *ifinfomsg = NLMSG_DATA(n);
        flxHwInterface *hw;
        struct rtattr *a = NULL;
        size_t l;

        if (ifinfomsg->ifi_family != AF_UNSPEC)
            return;

        if (!(hw = g_hash_table_lookup(m->hash_table, &ifinfomsg->ifi_index))) {
            hw = g_new(flxHwInterface, 1);
            hw->monitor = m;
            hw->name = NULL;
            hw->flags = 0;
            hw->mtu = 1500;
            hw->index = ifinfomsg->ifi_index;

            FLX_LLIST_HEAD_INIT(flxInterface, hw->interfaces);
            FLX_LLIST_PREPEND(flxHwInterface, hardware, m->hw_interfaces, hw);
            
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
        flxHwInterface *hw;
        flxInterface *i;

        if (ifinfomsg->ifi_family != AF_UNSPEC)
            return;
        
        if (!(hw = flx_interface_monitor_get_hw_interface(m, ifinfomsg->ifi_index)))
            return;

        update_hw_interface_rr(m, hw, TRUE);
        free_hw_interface(m, hw, FALSE);
        
    } else if (n->nlmsg_type == RTM_NEWADDR || n->nlmsg_type == RTM_DELADDR) {

        struct ifaddrmsg *ifaddrmsg = NLMSG_DATA(n);
        flxInterface *i;
        struct rtattr *a = NULL;
        size_t l;
        flxAddress raddr;
        int raddr_valid = 0;

        if (ifaddrmsg->ifa_family != AF_INET && ifaddrmsg->ifa_family != AF_INET6)
            return;

        if (!(i = (flxInterface*) flx_interface_monitor_get_interface(m, ifaddrmsg->ifa_index, ifaddrmsg->ifa_family)))
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
            flxInterfaceAddress *addr;
            
            if (!(addr = get_address(m, i, &raddr))) {
                addr = g_new(flxInterfaceAddress, 1);
                addr->monitor = m;
                addr->address = raddr;
                addr->interface = i;
                addr->rr_id = -1;

                FLX_LLIST_PREPEND(flxInterfaceAddress, address, i->addresses, addr);
            }
            
            addr->flags = ifaddrmsg->ifa_flags;
            addr->scope = ifaddrmsg->ifa_scope;

            update_address_rr(m, addr, FALSE);
            check_interface_relevant(m, i);
        } else {
            flxInterfaceAddress *addr;
            
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

flxInterfaceMonitor *flx_interface_monitor_new(flxServer *s) {
    flxInterfaceMonitor *m = NULL;

    m = g_new0(flxInterfaceMonitor, 1);
    m->server = s;
    if (!(m->netlink = flx_netlink_new(s->context, RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR, callback, m)))
        goto fail;

    m->hash_table = g_hash_table_new(g_int_hash, g_int_equal);

    FLX_LLIST_HEAD_INIT(flxInterface, m->interfaces);
    FLX_LLIST_HEAD_INIT(flxHwInterface, m->hw_interfaces);

    if (netlink_list_items(m->netlink, RTM_GETLINK, &m->query_link_seq) < 0)
        goto fail;

    m->list = LIST_IFACE;
    
    return m;

fail:
    flx_interface_monitor_free(m);
    return NULL;
}

void flx_interface_monitor_free(flxInterfaceMonitor *m) {
    g_assert(m);

    while (m->hw_interfaces)
        free_hw_interface(m, m->hw_interfaces, TRUE);

    g_assert(!m->interfaces);

    
    if (m->netlink)
        flx_netlink_free(m->netlink);
    
    if (m->hash_table)
        g_hash_table_destroy(m->hash_table);

    g_free(m);
}


flxInterface* flx_interface_monitor_get_interface(flxInterfaceMonitor *m, gint index, guchar protocol) {
    flxHwInterface *hw;
    flxInterface *i;
    
    g_assert(m);
    g_assert(index > 0);
    g_assert(protocol != AF_UNSPEC);

    if (!(hw = flx_interface_monitor_get_hw_interface(m, index)))
        return NULL;

    for (i = hw->interfaces; i; i = i->by_hardware_next)
        if (i->protocol == protocol)
            return i;

    return NULL;
}

flxHwInterface* flx_interface_monitor_get_hw_interface(flxInterfaceMonitor *m, gint index) {
    g_assert(m);
    g_assert(index > 0);

    return g_hash_table_lookup(m->hash_table, &index);
}


void flx_interface_send_packet(flxInterface *i, flxDnsPacket *p) {
    g_assert(i);
    g_assert(p);

    if (flx_interface_relevant(i)) {
        g_message("sending on '%s.%i'", i->hardware->name, i->protocol);

        if (i->protocol == AF_INET && i->monitor->server->fd_ipv4 >= 0)
            flx_send_dns_packet_ipv4(i->monitor->server->fd_ipv4, i->hardware->index, p);
        else if (i->protocol == AF_INET6 && i->monitor->server->fd_ipv6 >= 0)
            flx_send_dns_packet_ipv6(i->monitor->server->fd_ipv6, i->hardware->index, p);
    }
}

void flx_interface_post_query(flxInterface *i, flxKey *key, gboolean immediately) {
    g_assert(i);
    g_assert(key);

    if (flx_interface_relevant(i))
        flx_packet_scheduler_post_query(i->scheduler, key, immediately);
}


void flx_interface_post_response(flxInterface *i, flxRecord *record, gboolean immediately) {
    g_assert(i);
    g_assert(record);

    if (flx_interface_relevant(i))
        flx_packet_scheduler_post_response(i->scheduler, record, immediately);
}

void flx_dump_caches(flxInterfaceMonitor *m, FILE *f) {
    flxInterface *i;
    g_assert(m);

    for (i = m->interfaces; i; i = i->interface_next) {
        if (flx_interface_relevant(i)) {
            fprintf(f, "\n;;; INTERFACE %s.%i ;;;\n", i->hardware->name, i->protocol);
            flx_cache_dump(i->cache, f);
        }
    }
    fprintf(f, "\n");
}

gboolean flx_interface_relevant(flxInterface *i) {
    g_assert(i);

    return
        (i->hardware->flags & IFF_UP) &&
        (i->hardware->flags & IFF_RUNNING) &&
        !(i->hardware->flags & IFF_LOOPBACK) &&
        (i->hardware->flags & IFF_MULTICAST) &&
        i->addresses;
}

gboolean flx_interface_address_relevant(flxInterfaceAddress *a) { 
    g_assert(a);

    return a->scope == RT_SCOPE_UNIVERSE;
}


gboolean flx_interface_match(flxInterface *i, gint index, guchar protocol) {
    g_assert(i);
    
    if (index > 0 && index != i->hardware->index)
        return FALSE;

    if (protocol != AF_UNSPEC && protocol != i->protocol)
        return FALSE;

    return TRUE;
}


void flx_interface_monitor_walk(flxInterfaceMonitor *m, gint interface, guchar protocol, flxInterfaceMonitorWalkCallback callback, gpointer userdata) {
    g_assert(m);
    g_assert(callback);
    
    if (interface > 0) {
        if (protocol != AF_UNSPEC) {
            flxInterface *i;
            
            if ((i = flx_interface_monitor_get_interface(m, interface, protocol)))
                callback(m, i, userdata);
            
        } else {
            flxHwInterface *hw;
            flxInterface *i;

            if ((hw = flx_interface_monitor_get_hw_interface(m, interface)))
                for (i = hw->interfaces; i; i = i->by_hardware_next)
                    if (flx_interface_match(i, interface, protocol))
                        callback(m, i, userdata);
        }
        
    } else {
        flxInterface *i;
        
        for (i = m->interfaces; i; i = i->interface_next)
            if (flx_interface_match(i, interface, protocol))
                callback(m, i, userdata);
    }
}
