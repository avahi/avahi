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
#include <net/if.h>
#include <errno.h>
#include <string.h>

#include <avahi-common/malloc.h>

#include "log.h"
#include "iface.h"
#include "iface-linux.h"

static int netlink_list_items(AvahiNetlink *nl, uint16_t type, unsigned *ret_seq) {
    struct nlmsghdr *n;
    struct rtgenmsg *gen;
    uint8_t req[1024];
    
    memset(&req, 0, sizeof(req));
    n = (struct nlmsghdr*) req;
    n->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    n->nlmsg_type = type;
    n->nlmsg_flags = NLM_F_ROOT|NLM_F_REQUEST;
    n->nlmsg_pid = 0;

    gen = NLMSG_DATA(n);
    memset(gen, 0, sizeof(struct rtgenmsg));
    gen->rtgen_family = AF_UNSPEC;

    return avahi_netlink_send(nl, n, ret_seq);
}

static void netlink_callback(AvahiNetlink *nl, struct nlmsghdr *n, void* userdata) {
    AvahiInterfaceMonitor *m = userdata;
    
    assert(m);
    assert(n);
    assert(m->osdep.netlink == nl);

    if (n->nlmsg_type == RTM_NEWLINK) {
        struct ifinfomsg *ifinfomsg = NLMSG_DATA(n);
        AvahiHwInterface *hw;
        struct rtattr *a = NULL;
        size_t l;
        
        if (ifinfomsg->ifi_family != AF_UNSPEC)
            return;

        if (!(hw = avahi_interface_monitor_get_hw_interface(m, ifinfomsg->ifi_index)))
            if (!(hw = avahi_hw_interface_new(m, (AvahiIfIndex) ifinfomsg->ifi_index)))
                return; /* OOM */

        /* Check whether the flags of this interface are OK for us */
        hw->flags_ok =
            (ifinfomsg->ifi_flags & IFF_UP) &&
            (!m->server->config.use_iff_running || (ifinfomsg->ifi_flags & IFF_RUNNING)) &&
            !(ifinfomsg->ifi_flags & IFF_LOOPBACK) &&
            (ifinfomsg->ifi_flags & IFF_MULTICAST) &&
            !(ifinfomsg->ifi_flags & IFF_POINTOPOINT);
            

        l = NLMSG_PAYLOAD(n, sizeof(struct ifinfomsg));
        a = IFLA_RTA(ifinfomsg);

        while (RTA_OK(a, l)) {
            switch(a->rta_type) {
                case IFLA_IFNAME:
                    avahi_free(hw->name);
                    hw->name = avahi_strndup(RTA_DATA(a), RTA_PAYLOAD(a));
                    break;

                case IFLA_MTU:
                    assert(RTA_PAYLOAD(a) == sizeof(unsigned int));
                    hw->mtu = *((unsigned int*) RTA_DATA(a));
                    break;

                case IFLA_ADDRESS: {
                    hw->mac_address_size = RTA_PAYLOAD(a);
                    if (hw->mac_address_size > AVAHI_MAX_MAC_ADDRESS)
                        hw->mac_address_size = AVAHI_MAX_MAC_ADDRESS;
                    
                    memcpy(hw->mac_address, RTA_DATA(a), hw->mac_address_size);
                    break;
                }
                    
                default:
                    ;
            }

            a = RTA_NEXT(a, l);
        }

        avahi_hw_interface_check_relevant(hw);
        avahi_hw_interface_update_rrs(hw, 0);
        
    } else if (n->nlmsg_type == RTM_DELLINK) {
        struct ifinfomsg *ifinfomsg = NLMSG_DATA(n);
        AvahiHwInterface *hw;

        if (ifinfomsg->ifi_family != AF_UNSPEC)
            return;
        
        if (!(hw = avahi_interface_monitor_get_hw_interface(m, (AvahiIfIndex) ifinfomsg->ifi_index)))
            return;

        avahi_hw_interface_free(hw, 0);
        
    } else if (n->nlmsg_type == RTM_NEWADDR || n->nlmsg_type == RTM_DELADDR) {

        struct ifaddrmsg *ifaddrmsg = NLMSG_DATA(n);
        AvahiInterface *i;
        struct rtattr *a = NULL;
        size_t l;
        AvahiAddress raddr;
        int raddr_valid = 0;

        if (ifaddrmsg->ifa_family != AF_INET && ifaddrmsg->ifa_family != AF_INET6)
            return;

        if (!(i = avahi_interface_monitor_get_interface(m, (AvahiIfIndex) ifaddrmsg->ifa_index, avahi_af_to_proto(ifaddrmsg->ifa_family))))
            return;

        raddr.proto = avahi_af_to_proto(ifaddrmsg->ifa_family);

        l = NLMSG_PAYLOAD(n, sizeof(struct ifaddrmsg));
        a = IFA_RTA(ifaddrmsg);

        while (RTA_OK(a, l)) {

            switch(a->rta_type) {
                case IFA_ADDRESS:
                    if ((raddr.proto == AVAHI_PROTO_INET6 && RTA_PAYLOAD(a) != 16) ||
                        (raddr.proto == AVAHI_PROTO_INET && RTA_PAYLOAD(a) != 4))
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
            
            if (!(addr = avahi_interface_monitor_get_address(m, i, &raddr)))
                if (!(addr = avahi_interface_address_new(m, i, &raddr, ifaddrmsg->ifa_prefixlen)))
                    return; /* OOM */
            
            addr->global_scope = ifaddrmsg->ifa_scope == RT_SCOPE_UNIVERSE || ifaddrmsg->ifa_scope == RT_SCOPE_SITE;
        } else {
            AvahiInterfaceAddress *addr;
            
            if (!(addr = avahi_interface_monitor_get_address(m, i, &raddr)))
                return;

            avahi_interface_address_free(addr);
        }

        avahi_interface_check_relevant(i);
        avahi_interface_update_rrs(i, 0);
        
    } else if (n->nlmsg_type == NLMSG_DONE) {
        
        if (m->osdep.list == LIST_IFACE) {
            
            if (netlink_list_items(m->osdep.netlink, RTM_GETADDR, &m->osdep.query_addr_seq) < 0) {
                avahi_log_warn("NETLINK: Failed to list addrs: %s", strerror(errno));
                m->osdep.list = LIST_DONE;
            } else
                m->osdep.list = LIST_ADDR;

        } else
            /* We're through */
            m->osdep.list = LIST_DONE;

        if (m->osdep.list == LIST_DONE) {
            m->list_complete = 1;
            avahi_interface_monitor_check_relevant(m);
            avahi_interface_monitor_update_rrs(m, 0);
            avahi_log_info("Network interface enumeration completed.");
        }
        
    } else if (n->nlmsg_type == NLMSG_ERROR &&
               (n->nlmsg_seq == m->osdep.query_link_seq || n->nlmsg_seq == m->osdep.query_addr_seq)) {
        struct nlmsgerr *e = NLMSG_DATA (n);
                    
        if (e->error)
            avahi_log_warn("NETLINK: Failed to browse: %s", strerror(-e->error));
    }
}

int avahi_interface_monitor_init_osdep(AvahiInterfaceMonitor *m) {
    assert(m);

    m->osdep.netlink = NULL;
    m->osdep.query_addr_seq = m->osdep.query_link_seq = 0;
    
    if (!(m->osdep.netlink = avahi_netlink_new(m->server->poll_api, RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR, netlink_callback, m)))
        goto fail;

    if (netlink_list_items(m->osdep.netlink, RTM_GETLINK, &m->osdep.query_link_seq) < 0)
        goto fail;

    m->osdep.list = LIST_IFACE;

    return 0;

fail:

    if (m->osdep.netlink) {
        avahi_netlink_free(m->osdep.netlink);
        m->osdep.netlink = NULL;
    }

    return -1;
}

void avahi_interface_monitor_free_osdep(AvahiInterfaceMonitor *m) {
    assert(m);

    if (m->osdep.netlink) {
        avahi_netlink_free(m->osdep.netlink);
        m->osdep.netlink = NULL;
    }
}

void avahi_interface_monitor_sync(AvahiInterfaceMonitor *m) {
    assert(m);
    
    while (m->osdep.list != LIST_DONE) {
        if (!avahi_netlink_work(m->osdep.netlink, 1))
            break;
    } 
}
