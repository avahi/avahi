#ifndef footimeeventqhfoo
#define footimeeventqhfoo

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

typedef struct AvahiTimeEventQueue AvahiTimeEventQueue;
typedef struct AvahiTimeEvent AvahiTimeEvent;

#include "prioq.h"

struct AvahiTimeEvent {
    AvahiTimeEventQueue *queue;
    AvahiPrioQueueNode *node;
    GTimeVal expiry;
    void (*callback)(AvahiTimeEvent *e, void *userdata);
    void *userdata;
};

struct AvahiTimeEventQueue {
    GSource source;
    AvahiPrioQueue *prioq;
};

AvahiTimeEventQueue* avahi_time_event_queue_new(GMainContext *context, gint priority);
void avahi_time_event_queue_free(AvahiTimeEventQueue *q);

AvahiTimeEvent* avahi_time_event_queue_add(AvahiTimeEventQueue *q, const GTimeVal *timeval, void (*callback)(AvahiTimeEvent *e, void *userdata), void *userdata);
void avahi_time_event_queue_remove(AvahiTimeEventQueue *q, AvahiTimeEvent *e);

void avahi_time_event_queue_update(AvahiTimeEventQueue *q, AvahiTimeEvent *e, const GTimeVal *timeval);

AvahiTimeEvent* avahi_time_event_queue_root(AvahiTimeEventQueue *q);
AvahiTimeEvent* avahi_time_event_next(AvahiTimeEvent *e);

#endif
