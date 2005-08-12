#ifndef foowatchhfoo
#define foowatchhfoo

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

#include <sys/poll.h>
#include <avahi-common/cdecl.h>

#include "timeval.h"

AVAHI_C_DECL_BEGIN

typedef struct AvahiWatch AvahiWatch;
typedef struct AvahiPoll AvahiPoll;

typedef enum {
    AVAHI_WATCH_IN = POLLIN,
    AVAHI_WATCH_OUT = POLLOUT,
    AVAHI_WATCH_ERR = POLLERR,
    AVAHI_WATCH_HUP = POLLHUP
} AvahiWatchEvent;

typedef void (*AvahiWatchCallback)(AvahiWatch *w, int fd, AvahiWatchEvent event, void *userdata);

struct AvahiPoll {
    void* userdata;
    
    AvahiWatch* (*watch_new)(AvahiPoll *api, int fd, AvahiWatchEvent event, AvahiWatchCallback callback, void *userdata);
    void (*watch_update)(AvahiWatch *w, AvahiWatchEvent event);
    void (*watch_free)(AvahiWatch *w);
    
    void (*set_wakeup_time)(AvahiPoll *api, const struct timeval *tv);
};

AVAHI_C_DECL_END

#endif

