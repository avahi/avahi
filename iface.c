#include <string.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <errno.h>

#include "iface.h"
#include "netlink.h"

typedef struct _interface_callback_info {
    void (*callback)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterface *i, gpointer userdata);
    gpointer userdata;
    struct _interface_callback_info *next;
} interface_callback_info;

typedef struct _address_callback_info {
    void (*callback)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterfaceAddress *i, gpointer userdata);
    gpointer userdata;
    struct _address_callback_info *next;
} address_callback_info;

struct _flxInterfaceMonitor {
    flxNetlink *netlink;
    GHashTable *hash_table;
    interface_callback_info *interface_callbacks;
    address_callback_info *address_callbacks;
    flxInterface *interfaces;
    guint query_addr_seq, query_link_seq;
    enum { LIST_IFACE, LIST_ADDR, LIST_DONE } list;
};

static void run_interface_callbacks(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterface *i) {
    interface_callback_info *c;
    g_assert(m);
    g_assert(i);

    for (c = m->interface_callbacks; c; c = c->next) {
        g_assert(c->callback);
        c->callback(m, change, i, c->userdata);
    }
}

static void run_address_callbacks(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterfaceAddress *a) {
    address_callback_info *c;
    g_assert(m);
    g_assert(a);

    for (c = m->address_callbacks; c; c = c->next) {
        g_assert(c->callback);
        c->callback(m, change, a, c->userdata);
    }
}

static void free_address(flxInterfaceMonitor *m, flxInterfaceAddress *a) {
    g_assert(m);
    g_assert(a);
    g_assert(a->interface);

    if (a->address.family == AF_INET)
        a->interface->n_ipv4_addrs --;
    else if (a->address.family == AF_INET6)
        a->interface->n_ipv6_addrs --;
 
    if (a->prev)
        a->prev->next = a->next;
    else
        a->interface->addresses = a->next;

    if (a->next)
        a->next->prev = a->prev;

    g_free(a);
}

static void free_interface(flxInterfaceMonitor *m, flxInterface *i) {
    g_assert(m);
    g_assert(i);

    while (i->addresses)
        free_address(m, i->addresses);

    g_assert(i->n_ipv6_addrs == 0);
    g_assert(i->n_ipv4_addrs == 0);

    if (i->prev)
        i->prev->next = i->next;
    else
        m->interfaces = i->next;

    if (i->next)
        i->next->prev = i->prev;

    g_hash_table_remove(m->hash_table, &i->index);
    
    g_free(i->name);
    g_free(i);
}

static flxInterfaceAddress* get_address(flxInterfaceMonitor *m, flxInterface *i, const flxAddress *raddr) {
    flxInterfaceAddress *ia;
    
    g_assert(m);
    g_assert(i);
    g_assert(raddr);

    for (ia = i->addresses; ia; ia = ia->next) {
        if (flx_address_cmp(&ia->address, raddr) == 0)
            return ia;
    }

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
            i->name = NULL;
            i->index = ifinfomsg->ifi_index;
            i->addresses = NULL;
            i->n_ipv4_addrs = i->n_ipv6_addrs = 0;
            if ((i->next = m->interfaces))
                i->next->prev = i;
            m->interfaces = i;
            i->prev = NULL;
            g_hash_table_insert(m->hash_table, &i->index, i);
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

        run_interface_callbacks(m, changed ? FLX_INTERFACE_CHANGE : FLX_INTERFACE_NEW, i);
        
    } else if (n->nlmsg_type == RTM_DELLINK) {
        struct ifinfomsg *ifinfomsg = NLMSG_DATA(n);
        flxInterface *i;
        flxInterfaceAddress *a;

        if (ifinfomsg->ifi_family != AF_UNSPEC)
            return;
        
        if (!(i = (flxInterface*) flx_interface_monitor_get_interface(m, ifinfomsg->ifi_index)))
            return;

        for (a = i->addresses; a; a = a->next)
            run_address_callbacks(m, FLX_INTERFACE_REMOVE, a);

        run_interface_callbacks(m, FLX_INTERFACE_REMOVE, i);

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

                if (raddr.family == AF_INET)
                    i->n_ipv4_addrs++;
                else if (raddr.family == AF_INET6)
                    i->n_ipv6_addrs++;
                
                addr->interface = i;
                if ((addr->next = i->addresses))
                    addr->next->prev = addr;
                i->addresses = addr;
                addr->prev = NULL;
                
                changed = 0;
            }
            
            addr->flags = ifaddrmsg->ifa_flags;
            addr->scope = ifaddrmsg->ifa_scope;
            
            run_address_callbacks(m, changed ? FLX_INTERFACE_CHANGE : FLX_INTERFACE_NEW, addr);
        } else {
            flxInterfaceAddress *addr;
            
            if (!(addr = get_address(m, i, &raddr)))
                return;

            run_address_callbacks(m, FLX_INTERFACE_REMOVE, addr);
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

flxInterfaceMonitor *flx_interface_monitor_new(GMainContext *c) {
    flxInterfaceMonitor *m = NULL;

    m = g_new0(flxInterfaceMonitor, 1);
    if (!(m->netlink = flx_netlink_new(c, RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR, callback, m)))
        goto fail;

    m->hash_table = g_hash_table_new(g_int_hash, g_int_equal);
    m->interface_callbacks = NULL;
    m->address_callbacks = NULL;
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

    while (m->interface_callbacks) {
        interface_callback_info *c = m->interface_callbacks;
        m->interface_callbacks = c->next;
        g_free(c);
    }

    while (m->address_callbacks) {
        address_callback_info *c = m->address_callbacks;
        m->address_callbacks = c->next;
        g_free(c);
    }
    
    g_free(m);
}


const flxInterface* flx_interface_monitor_get_interface(flxInterfaceMonitor *m, gint index) {
    g_assert(m);
    g_assert(index > 0);

    return g_hash_table_lookup(m->hash_table, &index);
}

void flx_interface_monitor_add_interface_callback(
    flxInterfaceMonitor *m,
    void (*callback)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterface *i, gpointer userdata),
    gpointer userdata) {
    
    interface_callback_info *info;
    
    g_assert(m);
    g_assert(callback);

    info = g_new(interface_callback_info, 1);
    info->callback = callback;
    info->userdata = userdata;
    info->next = m->interface_callbacks;
    m->interface_callbacks = info;
}

void flx_interface_monitor_remove_interface_callback(
    flxInterfaceMonitor *m,
    void (*callback)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterface *i, gpointer userdata),
    gpointer userdata) {

    interface_callback_info *info, *prev;

    g_assert(m);
    g_assert(callback);

    info = m->interface_callbacks;
    prev = NULL;
    
    while (info) {
        if (info->callback == callback && info->userdata == userdata) {
            interface_callback_info *c = info;
            
            if (prev)
                prev->next = c->next;
            else
                m->interface_callbacks = c->next;
            
            info = c->next;
            g_free(c);
        } else {
            prev = info;
            info = info->next;
        }
    }
}

void flx_interface_monitor_add_address_callback(
    flxInterfaceMonitor *m,
    void (*callback)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterfaceAddress *a, gpointer userdata),
    gpointer userdata) {

    address_callback_info *info;
    
    g_assert(m);
    g_assert(callback);

    info = g_new(address_callback_info, 1);
    info->callback = callback;
    info->userdata = userdata;
    info->next = m->address_callbacks;
    m->address_callbacks = info;
}


void flx_interface_monitor_remove_address_callback(
    flxInterfaceMonitor *m,
    void (*callback)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterfaceAddress *a, gpointer userdata),
    gpointer userdata) {

    address_callback_info *info, *prev;

    g_assert(m);
    g_assert(callback);

    info = m->address_callbacks;
    prev = NULL;
    
    while (info) {
        if (info->callback == callback && info->userdata == userdata) {
            address_callback_info *c = info;
            
            if (prev)
                prev->next = c->next;
            else
                m->address_callbacks = c->next;
            
            info = c->next;
            g_free(c);
        } else {
            prev = info;
            info = info->next;
        }
    }

}

const flxInterface* flx_interface_monitor_get_first(flxInterfaceMonitor *m) {
    g_assert(m);
    return m->interfaces;
}
