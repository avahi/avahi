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

#include <avahi-common/malloc.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include <net/route.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>

#include "log.h"
#include "iface.h"
#include "iface-pfroute.h"
#include "util.h"

static int bitcount (unsigned int n)  
{  
  int count=0 ;
  while (n)
    {
      count++ ;
      n &= (n - 1) ;
    }
  return count ;
}

static void rtm_info(struct rt_msghdr *rtm, AvahiInterfaceMonitor *m)
{
  AvahiHwInterface *hw;
  struct if_msghdr *ifm = (struct if_msghdr *)rtm;
  struct sockaddr_dl *sdl = (struct sockaddr_dl *)(ifm + 1);
  
  if (sdl->sdl_family != AF_LINK)
    return;
  
  if (ifm->ifm_addrs == 0 && ifm->ifm_index > 0) {
    if (!(hw = avahi_interface_monitor_get_hw_interface(m, (AvahiIfIndex) ifm->ifm_index)))
      return;
    avahi_hw_interface_free(hw, 0);
    return;
  }
  
  if (!(hw = avahi_interface_monitor_get_hw_interface(m, ifm->ifm_index)))
    if (!(hw = avahi_hw_interface_new(m, (AvahiIfIndex) ifm->ifm_index)))
      return; /* OOM */
  
  hw->flags_ok =
    (ifm->ifm_flags & IFF_UP) &&
    (!m->server->config.use_iff_running || (ifm->ifm_flags & IFF_RUNNING)) &&
    !(ifm->ifm_flags & IFF_LOOPBACK) &&
    (ifm->ifm_flags & IFF_MULTICAST) &&
    (m->server->config.allow_point_to_point || !(ifm->ifm_flags & IFF_POINTOPOINT));
  
  avahi_free(hw->name);
  hw->name = avahi_strndup(sdl->sdl_data, sdl->sdl_nlen);
      
  hw->mtu = ifm->ifm_data.ifi_mtu;
  
  hw->mac_address_size = sdl->sdl_alen;
  if (hw->mac_address_size > AVAHI_MAC_ADDRESS_MAX)
    hw->mac_address_size = AVAHI_MAC_ADDRESS_MAX;
  
  memcpy(hw->mac_address, sdl->sdl_data + sdl->sdl_nlen, hw->mac_address_size);
 
/*   { */
/*     char mac[256]; */
/*     avahi_log_debug("======\n name: %s\n index:%d\n mtu:%d\n mac:%s\n flags_ok:%d\n======",  */
/* 		    hw->name, hw->index,  */
/* 		    hw->mtu,  */
/* 		    avahi_format_mac_address(mac, sizeof(mac), hw->mac_address, hw->mac_address_size), */
/* 		    hw->flags_ok); */
/*   } */
  
  avahi_hw_interface_check_relevant(hw);
  avahi_hw_interface_update_rrs(hw, 0);
}

#define ROUNDUP(a) \
     ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

static void rtm_addr(struct rt_msghdr *rtm, AvahiInterfaceMonitor *m)
{
  AvahiInterface *iface;
  AvahiAddress raddr;
  int raddr_valid = 0;
  struct ifa_msghdr *ifam = (struct ifa_msghdr *) rtm;
  char *cp = (char *)(ifam + 1);
  int addrs = ifam->ifam_addrs;
  int i;
  int prefixlen = 0;
  struct sockaddr *sa  =NULL;

#if defined(__NetBSD__) || defined(__OpenBSD__)
  if(((struct sockaddr *)cp)->sa_family == AF_UNSPEC)
    ((struct sockaddr *)cp)->sa_family = AF_INET;
#endif

  if(((struct sockaddr *)cp)->sa_family != AF_INET && ((struct sockaddr *)cp)->sa_family != AF_INET6)
    return;

  if (!(iface = avahi_interface_monitor_get_interface(m, (AvahiIfIndex) ifam->ifam_index, avahi_af_to_proto(((struct sockaddr *)cp)->sa_family))))
    return;

  raddr.proto = avahi_af_to_proto(((struct sockaddr *)cp)->sa_family);
  
  for(i = 0; addrs != 0 && i < RTAX_MAX; addrs &= ~(1<<i), i++)
    {
      if (!(addrs & 1<<i))
	continue;
      sa = (struct sockaddr *)cp;
      if (sa->sa_len == 0) 
	continue;
      switch(sa->sa_family) {
      case AF_INET:
	switch (1<<i) {
	case RTA_NETMASK:
	  prefixlen = bitcount((unsigned int)((struct sockaddr_in *)sa)->sin_addr.s_addr);
	  break;
	case RTA_IFA:
	  memcpy(raddr.data.data, &((struct sockaddr_in *)sa)->sin_addr,  sizeof(struct in_addr));
	  raddr_valid = 1;
	default:
	  break;
	}
	break;
      case AF_INET6:
	switch (1<<i) {
	case RTA_NETMASK:
	  prefixlen = bitcount((unsigned int)((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr);
	  break;
	case RTA_IFA:
	  memcpy(raddr.data.data, &((struct sockaddr_in6 *)sa)->sin6_addr,  sizeof(struct in6_addr));
	  raddr_valid = 1;
	default:
	  break;
	}
	break;
      default:
	break;
      }
#ifdef SA_SIZE
      cp += SA_SIZE(sa);
#else
      ADVANCE(cp, sa);
#endif
    }

  if (!raddr_valid)
    return;

  if(rtm->rtm_type == RTM_NEWADDR)
    {
      AvahiInterfaceAddress *addriface;
      if (!(addriface = avahi_interface_monitor_get_address(m, iface, &raddr)))
	if (!(addriface = avahi_interface_address_new(m, iface, &raddr, prefixlen)))
	  return; /* OOM */
      /*       FIXME */
      /*       addriface->global_scope = ifaddrmsg->ifa_scope == RT_SCOPE_UNIVERSE || ifaddrmsg->ifa_scope == RT_SCOPE_SITE; */
      addriface->global_scope = 1;
    }
  else
    {
      AvahiInterfaceAddress *addriface;
      assert(rtm->rtm_type == RTM_DELADDR);
      if (!(addriface = avahi_interface_monitor_get_address(m, iface, &raddr)))
	return;
      avahi_interface_address_free(addriface);
    }
  
  avahi_interface_check_relevant(iface);
  avahi_interface_update_rrs(iface, 0);
}

static void parse_rtmsg(struct rt_msghdr *rtm, AvahiInterfaceMonitor *m)
{
  assert(m);
  assert(rtm);
  
  if (rtm->rtm_version != RTM_VERSION) {
    avahi_log_warn("routing message version %d not understood",
		   rtm->rtm_version);
    return;
  }

  switch (rtm->rtm_type) {
  case RTM_IFINFO:
    rtm_info(rtm,m);
    break;
  case RTM_NEWADDR:
  case RTM_DELADDR:
    rtm_addr(rtm,m);
    break;
  default:
    break;
  }
}

static void socket_event(AvahiWatch *w, int fd, AVAHI_GCC_UNUSED AvahiWatchEvent event,void *userdata) {
  AvahiInterfaceMonitor *m = (AvahiInterfaceMonitor *)userdata;
  AvahiPfRoute *nl = m->osdep.pfroute;
    ssize_t bytes;
    char msg[2048];

    assert(m);
    assert(w);
    assert(nl);
    assert(fd == nl->fd);

    do {
      if((bytes = recv(nl->fd, msg, 2048, MSG_DONTWAIT)) < 0) {
	if (errno == EAGAIN || errno == EINTR)
	  return;
	avahi_log_error(__FILE__": recv() failed: %s", strerror(errno));
	return;
      }
      parse_rtmsg((struct rt_msghdr *)msg, m);
    }
    while (bytes > 0);
}

int avahi_interface_monitor_init_osdep(AvahiInterfaceMonitor *m) {
    int fd = -1;
    m->osdep.pfroute = NULL;

    assert(m);

    if ((fd = socket(PF_ROUTE, SOCK_RAW, AF_UNSPEC)) < 0) {
        avahi_log_error(__FILE__": socket(PF_ROUTE): %s", strerror(errno));
        goto fail;
    }

    if (!(m->osdep.pfroute = avahi_new(AvahiPfRoute , 1))) {
        avahi_log_error(__FILE__": avahi_new() failed.");
        goto fail;
    }
    m->osdep.pfroute->fd = fd;

    if (!(m->osdep.pfroute->watch = m->server->poll_api->watch_new(m->server->poll_api, 
								   m->osdep.pfroute->fd, 
								   AVAHI_WATCH_IN, 
								   socket_event, 
								   m))) {
      avahi_log_error(__FILE__": Failed to create watch.");
      goto fail;
    }
    
    return 0;

fail:

    if (m->osdep.pfroute) {
      if (m->osdep.pfroute->watch)
        m->server->poll_api->watch_free(m->osdep.pfroute->watch);
      
      if (fd >= 0)
        close(fd);
      
      m->osdep.pfroute = NULL;
    }

    return -1;
}

void avahi_interface_monitor_free_osdep(AvahiInterfaceMonitor *m) {
    assert(m);

    if (m->osdep.pfroute) {
      if (m->osdep.pfroute->watch)
        m->server->poll_api->watch_free(m->osdep.pfroute->watch);
      
      if (m->osdep.pfroute->fd >= 0)
        close(m->osdep.pfroute->fd);

      avahi_free(m->osdep.pfroute);
      m->osdep.pfroute = NULL;
    }
}

void avahi_interface_monitor_sync(AvahiInterfaceMonitor *m) {
  size_t needed;
  int mib[6];
  char *buf, *lim, *next, count = 0;
  struct rt_msghdr *rtm;

  assert(m);
  
 retry2:
  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;             /* protocol */
  mib[3] = 0;             /* wildcard address family */
  mib[4] = NET_RT_IFLIST;
  mib[5] = 0;             /* no flags */
  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
    {
      avahi_log_error("sysctl failed: %s", strerror(errno));
      avahi_log_error("route-sysctl-estimate");
      return;
    }
  if ((buf = avahi_malloc(needed)) == NULL)
    {
      avahi_log_error("malloc failed in avahi_interface_monitor_sync");
      return;
    }
  if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
    avahi_log_warn("sysctl failed: %s", strerror(errno));
    if (errno == ENOMEM && count++ < 10) {
      avahi_log_warn("Routing table grew, retrying");
      sleep(1);
      avahi_free(buf);
      goto retry2;
    }
  }
  lim = buf + needed;
  for (next = buf; next < lim; next += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)next;
    parse_rtmsg(rtm, m);
  }
  
  m->list_complete = 1;
  avahi_interface_monitor_check_relevant(m);
  avahi_interface_monitor_update_rrs(m, 0);
  avahi_log_info("Network interface enumeration completed.");
}
