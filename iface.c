#include <string.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <errno.h>
#include <net/if.h>

#include "iface.h"
#include "netlink.h"

static void update_address_rr(flxInterfaceMonitor *m, flxInterfaceAddress *a, int remove) {
    g_assert(m);
    g_assert(a);

    if (!flx_address_is_relevant(a) || remove) {
        if (a->rr_id >= 0) {
            flx_server_remove(m->server, a->rr_id);
            a->rr_id = -1;
        }
    } else {
        if (a->rr_id < 0) {
            a->rr_id = flx_server_get_next_id(m->server);
            flx_server_add_address(m->server, a->rr_id, a->interface->index, AF_UNSPEC, FALSE, m->server->hostname, &a->address);
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

static void free_address(flxInterfaceMonitor *m, flxInterfaceAddress *a) {
    g_assert(m);
    g_assert(a);
    g_assert(a->interface);

    if (a->address.family == AF_INET)
        a->interface->n_ipv4_addrs --;
    else if (a->address.family == AF_INET6)
        a->interface->n_ipv6_addrs --;

    FLX_LLIST_REMOVE(flxInterfaceAddress, address, a->interface->addresses, a);

    flx_server_remove(m->server, a->rr_id);
    
    g_free(a);
}

static void free_interface(flxInterfaceMonitor *m, flxInterface *i) {
    g_assert(m);
    g_assert(i);

    while (i->addresses)
        free_address(m, i->addresses);

    if (i->ipv4_cache)
        flx_cache_free(i->ipv4_cache);
    if (i->ipv6_cache)
        flx_cache_free(i->ipv6_cache);
    
    g_assert(i->n_ipv6_addrs == 0);
    g_assert(i->n_ipv4_addrs == 0);

    FLX_LLIST_REMOVE(flxInterface, interface, m->interfaces, i);
    g_hash_table_remove(m->hash_table, &i->index);

    flx_cache_free(i->ipv4_cache);
    flx_cache_free(i->ipv6_cache);
    
    g_free(i->name);
    g_free(i);
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

static void callback(flxNetlink *nl, struct nlmsghdr *n, gpointer userdata) {
    flxInterfaceMonitor *m = userdata;
    
    g_assert(m);
    g_assert(n);
    g_assert(m->netlink == nl);

    if (n->nlmsg_type == RTM_NEWLINK) {
        struct ifinfomsg *ifinfomsg = NLMSG_DATA(n);
        flxInterface *i;
        struct rtattr *a = NULL;
        size_t l;
        int changed;

        if (ifinfomsg->ifi_family != AF_UNSPEC)
            return;

        if ((i = (flxInterface*) flx_interface_monitor_get_interface(m, ifinfomsg->ifi_index)))
            changed = 1;
        else {
            i = g_new(flxInterface, 1);
            i->monitor = m;
            i->name = NULL;
            i->index = ifinfomsg->ifi_index;
            i->addresses = NULL;
            i->n_ipv4_addrs = i->n_ipv6_addrs = 0;
            FLX_LLIST_PREPEND(flxInterface, interface, m->interfaces, i);
            g_hash_table_insert(m->hash_table, &i->index, i);
            i->ipv4_cache = flx_cache_new(m->server, i);
            i->ipv6_cache = flx_cache_new(m->server, i);
            
            changed = 0;
        }
        
        i->flags = ifinfomsg->ifi_flags;

        l = NLMSG_PAYLOAD(n, sizeof(struct ifinfomsg));
        a = IFLA_RTA(ifinfomsg);

        while (RTA_OK(a, l)) {
            switch(a->rta_type) {
                case IFLA_IFNAME:
                    g_free(i->name);
                    i->name = g_strndup(RTA_DATA(a), RTA_PAYLOAD(a));
                    break;
                    
                default:
                    ;
            }

            a = RTA_NEXT(a, l);
        }

        update_interface_rr(m, i, 0);
    } else if (n->nlmsg_type == RTM_DELLINK) {
        struct ifinfomsg *ifinfomsg = NLMSG_DATA(n);
        flxInterface *i;

        if (ifinfomsg->ifi_family != AF_UNSPEC)
            return;
        
        if (!(i = (flxInterface*) flx_interface_monitor_get_interface(m, ifinfomsg->ifi_index)))
            return;

        update_interface_rr(m, i, 1);
        free_interface(m, i);
        
    } else if (n->nlmsg_type == RTM_NEWADDR || n->nlmsg_type == RTM_DELADDR) {

        struct ifaddrmsg *ifaddrmsg = NLMSG_DATA(n);
        flxInterface *i;
        struct rtattr *a = NULL;
        size_t l;
        int changed;
        flxAddress raddr;
        int raddr_valid = 0;

        if (ifaddrmsg->ifa_family != AF_INET && ifaddrmsg->ifa_family != AF_INET6)
            return;

        if (!(i = (flxInterface*) flx_interface_monitor_get_interface(m, ifaddrmsg->ifa_index)))
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

                    memcpy(raddr.data, RTA_DATA(a), RTA_PAYLOAD(a));
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
            
            if ((addr = get_address(m, i, &raddr)))
                changed = 1;
            else {
                addr = g_new(flxInterfaceAddress, 1);
                addr->address = raddr;
                addr->interface = i;

                if (raddr.family == AF_INET)
                    i->n_ipv4_addrs++;
                else if (raddr.family == AF_INET6)
                    i->n_ipv6_addrs++;

                addr->rr_id = -1;

                FLX_LLIST_PREPEND(flxInterfaceAddress, address, i->addresses, addr);
                
                changed = 0;
            }
            
            addr->flags = ifaddrmsg->ifa_flags;
            addr->scope = ifaddrmsg->ifa_scope;

            update_address_rr(m, addr, 0);
        } else {
            flxInterfaceAddress *addr;
            
            if (!(addr = get_address(m, i, &raddr)))
                return;

            update_address_rr(m, addr, 1);
            free_address(m, addr);
        }
                
    } else if (n->nlmsg_type == NLMSG_DONE) {

        if (m->list == LIST_IFACE) {
            m->list = LIST_DONE;
            
            if (netlink_list_items(m->netlink, RTM_GETADDR, &m->query_addr_seq) < 0) {
                g_warning("NETLINK: Failed to list addrs: %s", strerror(errno));
            } else
                m->list = LIST_ADDR;
        } else
            m->list = LIST_DONE;
        
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
    m->interfaces = NULL;

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

    if (m->netlink)
        flx_netlink_free(m->netlink);

    if (m->hash_table)
        g_hash_table_destroy(m->hash_table);

    g_free(m);
}


flxInterface* flx_interface_monitor_get_interface(flxInterfaceMonitor *m, gint index) {
    g_assert(m);
    g_assert(index > 0);

    return g_hash_table_lookup(m->hash_table, &index);
}

flxInterface* flx_interface_monitor_get_first(flxInterfaceMonitor *m) {
    g_assert(m);
    return m->interfaces;
}

int flx_interface_is_relevant(flxInterface *i) {
    g_assert(i);

    return
        (i->flags & IFF_UP) &&
        (i->flags & IFF_RUNNING) &&
        !(i->flags & IFF_LOOPBACK);
}

int flx_address_is_relevant(flxInterfaceAddress *a) {
    g_assert(a);

    return
        a->scope == RT_SCOPE_UNIVERSE &&
        flx_interface_is_relevant(a->interface);
}

void flx_interface_send_query(flxInterface *i, guchar protocol, flxKey *k) {
    g_assert(i);
    g_assert(k);
}
