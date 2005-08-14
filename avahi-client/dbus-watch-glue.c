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

#include <assert.h>

#include <avahi-common/malloc.h>

#include "dbus-watch-glue.h"

static AvahiWatchEvent translate_dbus_to_avahi(unsigned int f) {
    AvahiWatchEvent e = 0;

    if (f & DBUS_WATCH_READABLE)
        e |= AVAHI_WATCH_IN;
    if (f & DBUS_WATCH_WRITABLE)
        e |= AVAHI_WATCH_OUT;
    if (f & DBUS_WATCH_ERROR)
        e |= AVAHI_WATCH_ERR;
    if (f & DBUS_WATCH_HANGUP)
        e |= AVAHI_WATCH_HUP;

    return e;
}

static unsigned int translate_avahi_to_dbus(AvahiWatchEvent e) {
    unsigned int f = 0;

    if (e & AVAHI_WATCH_IN)
        f |= DBUS_WATCH_READABLE;
    if (e & AVAHI_WATCH_OUT)
        f |= DBUS_WATCH_WRITABLE;
    if (e & AVAHI_WATCH_ERR)
        f |= DBUS_WATCH_ERROR;
    if (e & AVAHI_WATCH_HUP)
        f |= DBUS_WATCH_HANGUP;

    return f;
}

static void watch_callback(AvahiWatch *avahi_watch, int fd, AvahiWatchEvent events, void *userdata) {
    DBusWatch *dbus_watch = userdata;
    assert(avahi_watch);
    assert(dbus_watch);

    dbus_watch_handle(dbus_watch, translate_avahi_to_dbus(events));
    /* Ignore the return value */
}

static dbus_bool_t update_watch(const AvahiPoll *poll_api, DBusWatch *dbus_watch) {
    AvahiWatch *avahi_watch;
    dbus_bool_t b;
    
    assert(dbus_watch);

    avahi_watch = dbus_watch_get_data(dbus_watch);

    b = dbus_watch_get_enabled(dbus_watch);
    
    if (b && !avahi_watch) {

        if (!(avahi_watch = poll_api->watch_new(
                  poll_api,
                  dbus_watch_get_fd(dbus_watch),
                  translate_dbus_to_avahi(dbus_watch_get_flags(dbus_watch)),
                  watch_callback,
                  dbus_watch)))
            return FALSE;

        dbus_watch_set_data(dbus_watch, avahi_watch, (DBusFreeFunction) poll_api->watch_free);
        
    } else if (!b && avahi_watch) {
        
        poll_api->watch_free(avahi_watch);
        dbus_watch_set_data(dbus_watch, NULL, NULL);

    } else if (avahi_watch) {
        
        /* Update flags */
        poll_api->watch_update(avahi_watch, dbus_watch_get_flags(dbus_watch));
    }

    return TRUE;
}

static dbus_bool_t add_watch(DBusWatch *dbus_watch, void *userdata) {
    const AvahiPoll *poll_api = (const AvahiPoll*) userdata;
    
    assert(dbus_watch);
    assert(poll_api);

    return update_watch(poll_api, dbus_watch);
}

static void remove_watch(DBusWatch *dbus_watch, void *userdata) {
    const AvahiPoll *poll_api = (const AvahiPoll*) userdata;
    AvahiWatch *avahi_watch;
    
    assert(dbus_watch);
    assert(poll_api);

    avahi_watch = dbus_watch_get_data(dbus_watch);
    poll_api->watch_free(avahi_watch);
    dbus_watch_set_data(dbus_watch, NULL, NULL);
}

static void watch_toggled(DBusWatch *dbus_watch, void *userdata) {
    const AvahiPoll *poll_api = (const AvahiPoll*) userdata;
    
    assert(dbus_watch);
    assert(poll_api);

    update_watch(poll_api, dbus_watch);
}

typedef struct TimeoutData {
    const AvahiPoll *poll_api;
    AvahiTimeout *avahi_timeout;
    DBusTimeout *dbus_timeout;
} TimeoutData;

static void update_timeout(TimeoutData *timeout) {
    assert(timeout);
    
    if (dbus_timeout_get_enabled(timeout->dbus_timeout)) {
        struct timeval tv;
        avahi_elapse_time(&tv, dbus_timeout_get_interval(timeout->dbus_timeout), 0);
        timeout->poll_api->timeout_update(timeout->
                                      avahi_timeout, &tv);
    } else
        timeout->poll_api->timeout_update(timeout->avahi_timeout, NULL);

}

static void timeout_callback(AvahiTimeout *avahi_timeout, void *userdata) {
    TimeoutData *timeout = userdata;
    
    assert(avahi_timeout);
    assert(timeout);

    dbus_timeout_handle(timeout->dbus_timeout);
    /* Ignore the return value */
    
    update_timeout(timeout);
}

static dbus_bool_t add_timeout(DBusTimeout *dbus_timeout, void *userdata) {
    TimeoutData *timeout;
    const AvahiPoll *poll_api = (const AvahiPoll*) userdata;
    struct timeval tv;
    dbus_bool_t b;

    assert(dbus_timeout);
    assert(poll_api);

    if (!(timeout = avahi_new(TimeoutData, 1)))
        return FALSE;

    timeout->dbus_timeout = dbus_timeout;
    timeout->poll_api = poll_api;

    if ((b = dbus_timeout_get_enabled(dbus_timeout)))
        avahi_elapse_time(&tv, dbus_timeout_get_interval(dbus_timeout), 0);
    
    if (!(timeout->avahi_timeout = poll_api->timeout_new(
              poll_api,
              b ? &tv : NULL,
              timeout_callback,
              dbus_timeout))) {
        avahi_free(timeout);
        return FALSE;
    }

    dbus_timeout_set_data(dbus_timeout, timeout, NULL);
    return TRUE;
}

static void remove_timeout(DBusTimeout *dbus_timeout, void *userdata) {
    TimeoutData *timeout;
    const AvahiPoll *poll_api = (const AvahiPoll*) userdata;

    assert(dbus_timeout);
    assert(poll_api);

    timeout = dbus_timeout_get_data(dbus_timeout);
    assert(timeout);

    poll_api->timeout_free(timeout->avahi_timeout);
    avahi_free(timeout);
    dbus_timeout_set_data(dbus_timeout, NULL, NULL);
}

static void timeout_toggled(DBusTimeout *dbus_timeout, void *userdata) {
    TimeoutData *timeout;
    const AvahiPoll *poll_api = (const AvahiPoll*) userdata;

    assert(dbus_timeout);
    assert(poll_api);

    timeout = dbus_timeout_get_data(dbus_timeout);
    assert(timeout);

    update_timeout(timeout);
}

int avahi_dbus_connection_glue(DBusConnection *c, const AvahiPoll *poll_api) {
    assert(c);
    assert(poll_api);

    if (!(dbus_connection_set_watch_functions(c, add_watch, remove_watch, watch_toggled, (void*) poll_api, NULL)))
        return -1;

    if (!(dbus_connection_set_timeout_functions(c, add_timeout, remove_timeout, timeout_toggled, (void*) poll_api, NULL)))
        return -1;

    return 0;
}
