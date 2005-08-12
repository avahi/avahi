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

#include <glib.h>

#include <avahi-common/timeval.h>
#include "timeeventq.h"

static AvahiTimeEventQueue *q = NULL;

static void callback(AvahiTimeEvent*e, gpointer userdata) {
    struct timeval tv = {0, 0};
    g_assert(e);
    g_message("callback(%i)", GPOINTER_TO_INT(userdata));
    avahi_elapse_time(&tv, 1000, 100);
    avahi_time_event_queue_update(q, e, &tv);
}

int main(int argc, char *argv[]) {
    GMainLoop *loop = NULL;
    struct timeval tv;
    
    q = avahi_time_event_queue_new(NULL, 0);

    avahi_time_event_queue_add(q, avahi_elapse_time(&tv, 5000, 100), callback, GINT_TO_POINTER(1));
    avahi_time_event_queue_add(q, avahi_elapse_time(&tv, 5000, 100), callback, GINT_TO_POINTER(2));

    g_message("starting");
    
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    avahi_time_event_queue_free(q);

    return 0;
}
