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

#include <avahi-common/timeval.h>
#include "query-sched.h"

#define AVAHI_QUERY_HISTORY_MSEC 100
#define AVAHI_QUERY_DEFER_MSEC 100

typedef struct AvahiQueryJob AvahiQueryJob;
typedef struct AvahiKnownAnswer AvahiKnownAnswer;

struct AvahiQueryJob {
    AvahiQueryScheduler *scheduler;
    AvahiTimeEvent *time_event;
    
    gboolean done;
    struct timeval delivery;

    AvahiKey *key;

    AVAHI_LLIST_FIELDS(AvahiQueryJob, jobs);
};

struct AvahiKnownAnswer {
    AvahiQueryScheduler *scheduler;
    AvahiRecord *record;

    AVAHI_LLIST_FIELDS(AvahiKnownAnswer, known_answer);
};

struct AvahiQueryScheduler {
    AvahiInterface *interface;
    AvahiTimeEventQueue *time_event_queue;

    AVAHI_LLIST_HEAD(AvahiQueryJob, jobs);
    AVAHI_LLIST_HEAD(AvahiQueryJob, history);
    AVAHI_LLIST_HEAD(AvahiKnownAnswer, known_answers);
};

static AvahiQueryJob* job_new(AvahiQueryScheduler *s, AvahiKey *key, gboolean done) {
    AvahiQueryJob *qj;
    
    g_assert(s);
    g_assert(key);

    qj = g_new(AvahiQueryJob, 1);
    qj->scheduler = s;
    qj->key = avahi_key_ref(key);
    qj->time_event = NULL;
    
    if ((qj->done = done)) 
        AVAHI_LLIST_PREPEND(AvahiQueryJob, jobs, s->history, qj);
    else
        AVAHI_LLIST_PREPEND(AvahiQueryJob, jobs, s->jobs, qj);

    return qj;
}

static void job_free(AvahiQueryScheduler *s, AvahiQueryJob *qj) {
    g_assert(s);
    g_assert(qj);

    if (qj->time_event)
        avahi_time_event_queue_remove(s->time_event_queue, qj->time_event);

    if (qj->done)
        AVAHI_LLIST_REMOVE(AvahiQueryJob, jobs, s->history, qj);
    else
        AVAHI_LLIST_REMOVE(AvahiQueryJob, jobs, s->jobs, qj);

    avahi_key_unref(qj->key);
    g_free(qj);
}

static void elapse_callback(AvahiTimeEvent *e, gpointer data);

static void job_set_elapse_time(AvahiQueryScheduler *s, AvahiQueryJob *qj, guint msec, guint jitter) {
    struct timeval tv;

    g_assert(s);
    g_assert(qj);

    avahi_elapse_time(&tv, msec, jitter);

    if (qj->time_event)
        avahi_time_event_queue_update(s->time_event_queue, qj->time_event, &tv);
    else
        qj->time_event = avahi_time_event_queue_add(s->time_event_queue, &tv, elapse_callback, qj);
}

static void job_mark_done(AvahiQueryScheduler *s, AvahiQueryJob *qj) {
    g_assert(s);
    g_assert(qj);

    g_assert(!qj->done);

    AVAHI_LLIST_REMOVE(AvahiQueryJob, jobs, s->jobs, qj);
    AVAHI_LLIST_PREPEND(AvahiQueryJob, jobs, s->history, qj);

    qj->done = TRUE;

    job_set_elapse_time(s, qj, AVAHI_QUERY_HISTORY_MSEC, 0);
    gettimeofday(&qj->delivery, NULL);
}

AvahiQueryScheduler *avahi_query_scheduler_new(AvahiInterface *i) {
    AvahiQueryScheduler *s;
    g_assert(i);

    s = g_new(AvahiQueryScheduler, 1);
    s->interface = i;
    s->time_event_queue = i->monitor->server->time_event_queue;
    
    AVAHI_LLIST_HEAD_INIT(AvahiQueryJob, s->jobs);
    AVAHI_LLIST_HEAD_INIT(AvahiQueryJob, s->history);
    AVAHI_LLIST_HEAD_INIT(AvahiKnownAnswer, s->known_answers);

    return s;
}

void avahi_query_scheduler_free(AvahiQueryScheduler *s) {
    g_assert(s);

    g_assert(!s->known_answers);
    avahi_query_scheduler_clear(s);
    g_free(s);
}

void avahi_query_scheduler_clear(AvahiQueryScheduler *s) {
    g_assert(s);
    
    while (s->jobs)
        job_free(s, s->jobs);
    while (s->history)
        job_free(s, s->history);
}

static gpointer known_answer_walk_callback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, gpointer userdata) {
    AvahiQueryScheduler *s = userdata;
    AvahiKnownAnswer *ka;
    
    g_assert(c);
    g_assert(pattern);
    g_assert(e);
    g_assert(s);

    if (avahi_cache_entry_half_ttl(c, e))
        return NULL;
    
    ka = g_new0(AvahiKnownAnswer, 1);
    ka->scheduler = s;
    ka->record = avahi_record_ref(e->record);

    AVAHI_LLIST_PREPEND(AvahiKnownAnswer, known_answer, s->known_answers, ka);
    return NULL;
}

static gboolean packet_add_query_job(AvahiQueryScheduler *s, AvahiDnsPacket *p, AvahiQueryJob *qj) {
    g_assert(s);
    g_assert(p);
    g_assert(qj);

    if (!avahi_dns_packet_append_key(p, qj->key, FALSE))
        return FALSE;

    /* Add all matching known answers to the list */
    avahi_cache_walk(s->interface->cache, qj->key, known_answer_walk_callback, s);
    
    job_mark_done(s, qj);

    return TRUE;
}

static void append_known_answers_and_send(AvahiQueryScheduler *s, AvahiDnsPacket *p) {
    AvahiKnownAnswer *ka;
    guint n;
    g_assert(s);
    g_assert(p);

    n = 0;
    
    while ((ka = s->known_answers)) {
        gboolean too_large = FALSE;

        while (!avahi_dns_packet_append_record(p, ka->record, FALSE, 0)) {

            if (avahi_dns_packet_is_empty(p)) {
                /* The record is too large to fit into one packet, so
                   there's no point in sending it. Better is letting
                   the owner of the record send it as a response. This
                   has the advantage of a cache refresh. */

                too_large = TRUE;
                break;
            }

            avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_FLAGS, avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_FLAGS) | AVAHI_DNS_FLAG_TC);
            avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ANCOUNT, n);
            avahi_interface_send_packet(s->interface, p);
            avahi_dns_packet_free(p);

            p = avahi_dns_packet_new_query(s->interface->hardware->mtu);
            n = 0;
        }

        AVAHI_LLIST_REMOVE(AvahiKnownAnswer, known_answer, s->known_answers, ka);
        avahi_record_unref(ka->record);
        g_free(ka);

        if (!too_large)
            n++;
    }
    
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ANCOUNT, n);
    avahi_interface_send_packet(s->interface, p);
    avahi_dns_packet_free(p);
}

static void elapse_callback(AvahiTimeEvent *e, gpointer data) {
    AvahiQueryJob *qj = data;
    AvahiQueryScheduler *s;
    AvahiDnsPacket *p;
    guint n;
    gboolean b;

    g_assert(qj);
    s = qj->scheduler;

    if (qj->done) {
        /* Lets remove it  from the history */
        job_free(s, qj);
        return;
    }

    g_assert(!s->known_answers);
    
    p = avahi_dns_packet_new_query(s->interface->hardware->mtu);
    b = packet_add_query_job(s, p, qj);
    g_assert(b); /* An query must always fit in */
    n = 1;

    /* Try to fill up packet with more queries, if available */
    while (s->jobs) {

        if (!packet_add_query_job(s, p, s->jobs))
            break;

        n++;
    }

    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_QDCOUNT, n);

    /* Now add known answers */
    append_known_answers_and_send(s, p);
}

static AvahiQueryJob* find_scheduled_job(AvahiQueryScheduler *s, AvahiKey *key) {
    AvahiQueryJob *qj;

    g_assert(s);
    g_assert(key);

    for (qj = s->jobs; qj; qj = qj->jobs_next) {
        g_assert(!qj->done);
        
        if (avahi_key_equal(qj->key, key))
            return qj;
    }

    return NULL;
}

static AvahiQueryJob* find_history_job(AvahiQueryScheduler *s, AvahiKey *key) {
    AvahiQueryJob *qj;
    
    g_assert(s);
    g_assert(key);

    for (qj = s->history; qj; qj = qj->jobs_next) {
        g_assert(qj->done);

        if (avahi_key_equal(qj->key, key)) {
            /* Check whether this entry is outdated */

            if (avahi_age(&qj->delivery) > AVAHI_QUERY_HISTORY_MSEC*1000) {
                /* it is outdated, so let's remove it */
                job_free(s, qj);
                return NULL;
            }
                
            return qj;
        }
    }

    return NULL;
}

gboolean avahi_query_scheduler_post(AvahiQueryScheduler *s, AvahiKey *key, gboolean immediately) {
    struct timeval tv;
    AvahiQueryJob *qj;
    
    g_assert(s);
    g_assert(key);

    if ((qj = find_history_job(s, key))) {
/*         avahi_log_debug("Query suppressed by local duplicate suppression (history)"); */
        return FALSE;
    }
    
    avahi_elapse_time(&tv, immediately ? 0 : AVAHI_QUERY_DEFER_MSEC, 0);

    if ((qj = find_scheduled_job(s, key))) {
        /* Duplicate questions suppression */

/*         avahi_log_debug("Query suppressed by local duplicate suppression (scheduled)"); */
        
        if (avahi_timeval_compare(&tv, &qj->delivery) < 0) {
            /* If the new entry should be scheduled earlier,
             * update the old entry */
            qj->delivery = tv;
            avahi_time_event_queue_update(s->time_event_queue, qj->time_event, &qj->delivery);
        }

        return TRUE;
    } else {
/*         avahi_log_debug("Accepted new query job.\n"); */

        qj = job_new(s, key, FALSE);
        qj->delivery = tv;
        qj->time_event = avahi_time_event_queue_add(s->time_event_queue, &qj->delivery, elapse_callback, qj);
        
        return TRUE;
    }
}

void avahi_query_scheduler_incoming(AvahiQueryScheduler *s, AvahiKey *key) {
    AvahiQueryJob *qj;
    
    g_assert(s);
    g_assert(key);

    /* This function is called whenever an incoming query was
     * receieved. We drop scheduled queries that match. The keyword is
     * "DUPLICATE QUESTION SUPPRESION". */

    if ((qj = find_scheduled_job(s, key))) {
/*         avahi_log_debug("Query suppressed by distributed duplicate suppression"); */
        job_mark_done(s, qj);
        return;
    }
    
    qj = job_new(s, key, TRUE);
    gettimeofday(&qj->delivery, NULL);
    job_set_elapse_time(s, qj, AVAHI_QUERY_HISTORY_MSEC, 0);
}

