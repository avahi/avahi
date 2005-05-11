#ifndef foopschedhfoo
#define foopschedhfoo

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

typedef struct AvahiQueryJob AvahiQueryJob;
typedef struct AvahiResponseJob AvahiResponseJob;
typedef struct AvahiPacketScheduler AvahiPacketScheduler;
typedef struct AvahiKnownAnswer AvahiKnownAnswer;
typedef struct AvahiProbeJob AvahiProbeJob;

#include "timeeventq.h"
#include "rr.h"
#include "llist.h"
#include "iface.h"

struct AvahiQueryJob {
    AvahiPacketScheduler *scheduler;
    AvahiTimeEvent *time_event;
    AvahiKey *key;
    gboolean done;
    GTimeVal delivery;
    AVAHI_LLIST_FIELDS(AvahiQueryJob, jobs);
};

struct AvahiResponseJob {
    AvahiPacketScheduler *scheduler;
    AvahiTimeEvent *time_event;
    AvahiRecord *record;
    gboolean done;
    GTimeVal delivery;
    gboolean flush_cache;
    AVAHI_LLIST_FIELDS(AvahiResponseJob, jobs);
};

struct AvahiKnownAnswer {
    AvahiPacketScheduler *scheduler;
    AvahiRecord *record;

    AVAHI_LLIST_FIELDS(AvahiKnownAnswer, known_answer);
};

struct AvahiProbeJob {
    AvahiPacketScheduler *scheduler;
    AvahiTimeEvent *time_event;
    AvahiRecord *record;

    gboolean chosen; /* Use for packet assembling */
    GTimeVal delivery;
    
    AVAHI_LLIST_FIELDS(AvahiProbeJob, jobs);
};

struct AvahiPacketScheduler {
    AvahiServer *server;
    
    AvahiInterface *interface;

    AVAHI_LLIST_HEAD(AvahiQueryJob, query_jobs);
    AVAHI_LLIST_HEAD(AvahiResponseJob, response_jobs);
    AVAHI_LLIST_HEAD(AvahiKnownAnswer, known_answers);
    AVAHI_LLIST_HEAD(AvahiProbeJob, probe_jobs);
};

AvahiPacketScheduler *avahi_packet_scheduler_new(AvahiServer *server, AvahiInterface *i);
void avahi_packet_scheduler_free(AvahiPacketScheduler *s);

gboolean avahi_packet_scheduler_post_query(AvahiPacketScheduler *s, AvahiKey *key, gboolean immediately);
gboolean avahi_packet_scheduler_post_response(AvahiPacketScheduler *s, AvahiRecord *record, gboolean flush_cache, gboolean immediately);
gboolean avahi_packet_scheduler_post_probe(AvahiPacketScheduler *s, AvahiRecord *record, gboolean immediately);

void avahi_packet_scheduler_incoming_query(AvahiPacketScheduler *s, AvahiKey *key);
void avahi_packet_scheduler_incoming_response(AvahiPacketScheduler *s, AvahiRecord *record);

void avahi_packet_scheduler_flush_responses(AvahiPacketScheduler *s);

#endif
