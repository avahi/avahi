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

/** \file watch.h Simplistic main loop abstraction */

#include <sys/poll.h>
#include <avahi-common/cdecl.h>

#include "timeval.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_BEGIN
#endif

/** An I/O watch object */
typedef struct AvahiWatch AvahiWatch;

/** An event polling abstraction object */
typedef struct AvahiPoll AvahiPoll;

/** Type of watch events */
typedef enum {
    AVAHI_WATCH_IN = POLLIN,      /** Input event */
    AVAHI_WATCH_OUT = POLLOUT,    /** Output event */
    AVAHI_WATCH_ERR = POLLERR,    /** Error event */
    AVAHI_WATCH_HUP = POLLHUP     /** Hangup event */
} AvahiWatchEvent;

/** Called whenever an I/O event happens  on an I/O watch */
typedef void (*AvahiWatchCallback)(AvahiWatch *w, int fd, AvahiWatchEvent event, void *userdata);

/** Called when the wakeup time is reached */
typedef void (*AvahiWakeupCallback)(AvahiPoll *api, void *userdata);

/** Defines an abstracted event polling API. This may be used to
 connect Avahi to other main loops. This is losely based on Unix
 poll(2). A consumer will call watch_new() for all file descriptors it
 wants to listen for events on. In addition he can call set_wakeup()
 to define a single wakeup time.*/
struct AvahiPoll {

    /** Some abstract user data usable by the implementor of the API */
    void* userdata; 

    /** Create a new watch for the specified file descriptor and for
     * the specified events. The API will call the callback function
     * whenever any of the events happens. */
    AvahiWatch* (*watch_new)(const AvahiPoll *api, int fd, AvahiWatchEvent event, AvahiWatchCallback callback, void *userdata);

    /** Update the events to wait for. */
    void (*watch_update)(AvahiWatch *w, AvahiWatchEvent event);

    /** Free a watch */
    void (*watch_free)(AvahiWatch *w);

    /** Set a wakeup time for the polling loop. The API will call the
    callback function when the absolute time *tv is reached. If *tv is
    NULL, the callback will be called in the next main loop
    iteration. If callback is NULL the wakeup time is disabled. */
    void (*set_wakeup)(const AvahiPoll *api, const struct timeval *tv, AvahiWakeupCallback callback, void *userdata);
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
AVAHI_C_DECL_END
#endif

#endif

