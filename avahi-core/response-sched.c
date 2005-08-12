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

#include "response-sched.h"
#include "timeval.h"
#include "log.h"

#define AVAHI_RESPONSE_HISTORY_MSEC 500
#define AVAHI_RESPONSE_DEFER_MSEC 20
#define AVAHI_RESPONSE_JITTER_MSEC 100
#define AVAHI_RESPONSE_SUPPRESS_MSEC 700

typedef struct AvahiResponseJob AvahiResponseJob;

typedef enum {
    AVAHI_SCHEDULED,
    AVAHI_DONE,
    AVAHI_SUPPRESSED
} AvahiResponseJobState;

struct AvahiResponseJob {
    AvahiResponseScheduler *scheduler;
    AvahiTimeEvent *time_event;
    
    AvahiResponseJobState state;
    struct timeval delivery;

    AvahiRecord *record;
    gboolean flush_cache;
    AvahiAddress querier;
    gboolean querier_valid;
    
    AVAHI_LLIST_FIELDS(AvahiResponseJob, jobs);
};

struct AvahiResponseScheduler {
    AvahiInterface *interface;
    AvahiTimeEventQueue *time_event_queue;

    AVAHI_LLIST_HEAD(AvahiResponseJob, jobs);
    AVAHI_LLIST_HEAD(AvahiResponseJob, history);
    AVAHI_LLIST_HEAD(AvahiResponseJob, suppressed);
};

static AvahiResponseJob* job_new(AvahiResponseScheduler *s, AvahiRecord *record, AvahiResponseJobState state) {
    AvahiResponseJob *rj;
    
    g_assert(s);
    g_assert(record);

    rj = g_new(AvahiResponseJob, 1);
    rj->scheduler = s;
    rj->record = avahi_record_ref(record);
    rj->time_event = NULL;
    rj->flush_cache = FALSE;
    rj->querier_valid = FALSE;
    
    if ((rj->state = state) == AVAHI_SCHEDULED) 
        AVAHI_LLIST_PREPEND(AvahiResponseJob, jobs, s->jobs, rj);
    else if (rj->state == AVAHI_DONE)
        AVAHI_LLIST_PREPEND(AvahiResponseJob, jobs, s->history, rj);
    else  /* rj->state == AVAHI_SUPPRESSED */
        AVAHI_LLIST_PREPEND(AvahiResponseJob, jobs, s->suppressed, rj);

    return rj;
}

static void job_free(AvahiResponseScheduler *s, AvahiResponseJob *rj) {
    g_assert(s);
    g_assert(rj);

    if (rj->time_event)
        avahi_time_event_queue_remove(s->time_event_queue, rj->time_event);

    if (rj->state == AVAHI_SCHEDULED)
        AVAHI_LLIST_REMOVE(AvahiResponseJob, jobs, s->jobs, rj);
    else if (rj->state == AVAHI_DONE)
        AVAHI_LLIST_REMOVE(AvahiResponseJob, jobs, s->history, rj);
    else /* rj->state == AVAHI_SUPPRESSED */
        AVAHI_LLIST_REMOVE(AvahiResponseJob, jobs, s->suppressed, rj);

    avahi_record_unref(rj->record);
    g_free(rj);
}

static void elapse_callback(AvahiTimeEvent *e, gpointer data);

static void job_set_elapse_time(AvahiResponseScheduler *s, AvahiResponseJob *rj, guint msec, guint jitter) {
    struct timeval tv;

    g_assert(s);
    g_assert(rj);

    avahi_elapse_time(&tv, msec, jitter);

    if (rj->time_event)
        avahi_time_event_queue_update(s->time_event_queue, rj->time_event, &tv);
    else
        rj->time_event = avahi_time_event_queue_add(s->time_event_queue, &tv, elapse_callback, rj);
}

static void job_mark_done(AvahiResponseScheduler *s, AvahiResponseJob *rj) {
    g_assert(s);
    g_assert(rj);

    g_assert(rj->state == AVAHI_SCHEDULED);

    AVAHI_LLIST_REMOVE(AvahiResponseJob, jobs, s->jobs, rj);
    AVAHI_LLIST_PREPEND(AvahiResponseJob, jobs, s->history, rj);

    rj->state = AVAHI_DONE;

    job_set_elapse_time(s, rj, AVAHI_RESPONSE_HISTORY_MSEC, 0);

    gettimeofday(&rj->delivery, NULL);
}

AvahiResponseScheduler *avahi_response_scheduler_new(AvahiInterface *i) {
    AvahiResponseScheduler *s;
    g_assert(i);

    s = g_new(AvahiResponseScheduler, 1);
    s->interface = i;
    s->time_event_queue = i->monitor->server->time_event_queue;
    
    AVAHI_LLIST_HEAD_INIT(AvahiResponseJob, s->jobs);
    AVAHI_LLIST_HEAD_INIT(AvahiResponseJob, s->history);
    AVAHI_LLIST_HEAD_INIT(AvahiResponseJob, s->suppressed);

    return s;
}

void avahi_response_scheduler_free(AvahiResponseScheduler *s) {
    g_assert(s);

    avahi_response_scheduler_clear(s);
    g_free(s);
}

void avahi_response_scheduler_clear(AvahiResponseScheduler *s) {
    g_assert(s);
    
    while (s->jobs)
        job_free(s, s->jobs);
    while (s->history)
        job_free(s, s->history);
    while (s->suppressed)
        job_free(s, s->suppressed);
}

static void enumerate_aux_records_callback(AvahiServer *s, AvahiRecord *r, gboolean flush_cache, gpointer userdata) {
    AvahiResponseJob *rj = userdata;
    
    g_assert(r);
    g_assert(rj);

    avahi_response_scheduler_post(rj->scheduler, r, flush_cache, rj->querier_valid ? &rj->querier : NULL, FALSE);
}

static gboolean packet_add_response_job(AvahiResponseScheduler *s, AvahiDnsPacket *p, AvahiResponseJob *rj) {
    g_assert(s);
    g_assert(p);
    g_assert(rj);

    /* Try to add this record to the packet */
    if (!avahi_dns_packet_append_record(p, rj->record, rj->flush_cache, 0))
        return FALSE;

    /* Ok, this record will definitely be sent, so schedule the
     * auxilliary packets, too */
    avahi_server_enumerate_aux_records(s->interface->monitor->server, s->interface, rj->record, enumerate_aux_records_callback, rj);
    job_mark_done(s, rj);
    
    return TRUE;
}

static void send_response_packet(AvahiResponseScheduler *s, AvahiResponseJob *rj) {
    AvahiDnsPacket *p;
    guint n;

    g_assert(s);
    g_assert(rj);

    p = avahi_dns_packet_new_response(s->interface->hardware->mtu, TRUE);
    n = 1;

    /* Put it in the packet. */
    if (packet_add_response_job(s, p, rj)) {

        /* Try to fill up packet with more responses, if available */
        while (s->jobs) {
            
            if (!packet_add_response_job(s, p, s->jobs))
                break;
            
            n++;
        }
        
    } else {
        guint size;
        
        avahi_dns_packet_free(p);

        /* OK, the packet was too small, so create one that fits */
        size = avahi_record_get_estimate_size(rj->record) + AVAHI_DNS_PACKET_HEADER_SIZE;

        if (size > AVAHI_DNS_PACKET_MAX_SIZE)
            size = AVAHI_DNS_PACKET_MAX_SIZE;
        
        p = avahi_dns_packet_new_response(size, TRUE);

        if (!packet_add_response_job(s, p, rj)) {
            avahi_dns_packet_free(p);

            avahi_log_warn("Record too large, cannot send");
            job_mark_done(s, rj);
            return;
        }
    }

    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ANCOUNT, n);
    avahi_interface_send_packet(s->interface, p);
    avahi_dns_packet_free(p);
}

static void elapse_callback(AvahiTimeEvent *e, gpointer data) {
    AvahiResponseJob *rj = data;

    g_assert(rj);

    if (rj->state == AVAHI_DONE || rj->state == AVAHI_SUPPRESSED) 
        job_free(rj->scheduler, rj);         /* Lets drop this entry */
    else
        send_response_packet(rj->scheduler, rj);
}

static AvahiResponseJob* find_scheduled_job(AvahiResponseScheduler *s, AvahiRecord *record) {
    AvahiResponseJob *rj;

    g_assert(s);
    g_assert(record);

    for (rj = s->jobs; rj; rj = rj->jobs_next) {
        g_assert(rj->state == AVAHI_SCHEDULED);
    
        if (avahi_record_equal_no_ttl(rj->record, record))
            return rj;
    }

    return NULL;
}

static AvahiResponseJob* find_history_job(AvahiResponseScheduler *s, AvahiRecord *record) {
    AvahiResponseJob *rj;
    
    g_assert(s);
    g_assert(record);

    for (rj = s->history; rj; rj = rj->jobs_next) {
        g_assert(rj->state == AVAHI_DONE);

        if (avahi_record_equal_no_ttl(rj->record, record)) {
            /* Check whether this entry is outdated */

/*             avahi_log_debug("history age: %u", (unsigned) (avahi_age(&rj->delivery)/1000)); */
            
            if (avahi_age(&rj->delivery)/1000 > AVAHI_RESPONSE_HISTORY_MSEC) {
                /* it is outdated, so let's remove it */
                job_free(s, rj);
                return NULL;
            }
                
            return rj;
        }
    }

    return NULL;
}

static AvahiResponseJob* find_suppressed_job(AvahiResponseScheduler *s, AvahiRecord *record, const AvahiAddress *querier) {
    AvahiResponseJob *rj;
    
    g_assert(s);
    g_assert(record);
    g_assert(querier);

    for (rj = s->suppressed; rj; rj = rj->jobs_next) {
        g_assert(rj->state == AVAHI_SUPPRESSED);
        g_assert(rj->querier_valid);
        
        if (avahi_record_equal_no_ttl(rj->record, record) &&
            avahi_address_cmp(&rj->querier, querier) == 0) {
            /* Check whether this entry is outdated */

            if (avahi_age(&rj->delivery) > AVAHI_RESPONSE_SUPPRESS_MSEC*1000) {
                /* it is outdated, so let's remove it */
                job_free(s, rj);
                return NULL;
            }

            return rj;
        }
    }

    return NULL;
}

gboolean avahi_response_scheduler_post(AvahiResponseScheduler *s, AvahiRecord *record, gboolean flush_cache, const AvahiAddress *querier, gboolean immediately) {
    AvahiResponseJob *rj;
    struct timeval tv;
/*     gchar *t; */
    
    g_assert(s);
    g_assert(record);

    g_assert(!avahi_key_is_pattern(record->key));

/*     t = avahi_record_to_string(record); */
/*     avahi_log_debug("post %i %s", immediately, t); */
/*     g_free(t); */

    /* Check whether this response is suppressed */
    if (querier &&
        (rj = find_suppressed_job(s, record, querier)) &&
        avahi_record_is_goodbye(record) == avahi_record_is_goodbye(rj->record) &&
        rj->record->ttl >= record->ttl/2) {

/*         avahi_log_debug("Response suppressed by known answer suppression.");  */
        return FALSE;
    }

    /* Check if we already sent this response recently */
    if ((rj = find_history_job(s, record))) {

        if (avahi_record_is_goodbye(record) == avahi_record_is_goodbye(rj->record) &&
            rj->record->ttl >= record->ttl/2 &&
            (rj->flush_cache || !flush_cache)) {
/*             avahi_log_debug("Response suppressed by local duplicate suppression (history)");  */
            return FALSE;
        }

        /* Outdated ... */
        job_free(s, rj);
    }

    avahi_elapse_time(&tv, immediately ? 0 : AVAHI_RESPONSE_DEFER_MSEC, immediately ? 0 : AVAHI_RESPONSE_JITTER_MSEC);
         
    if ((rj = find_scheduled_job(s, record))) {
/*          avahi_log_debug("Response suppressed by local duplicate suppression (scheduled)"); */

        /* Update a little ... */

        /* Update the time if the new is prior to the old */
        if (avahi_timeval_compare(&tv, &rj->delivery) < 0) {
            rj->delivery = tv;
            avahi_time_event_queue_update(s->time_event_queue, rj->time_event, &rj->delivery);
        }

        /* Update the flush cache bit */
        if (flush_cache)
            rj->flush_cache = TRUE;

        /* Update the querier field */
        if (!querier || (rj->querier_valid && avahi_address_cmp(querier, &rj->querier) != 0))
            rj->querier_valid = FALSE;

        /* Update record data (just for the TTL) */
        avahi_record_unref(rj->record);
        rj->record = avahi_record_ref(record);

        return TRUE;
    } else {
/*         avahi_log_debug("Accepted new response job.");  */

        /* Create a new job and schedule it */
        rj = job_new(s, record, AVAHI_SCHEDULED);
        rj->delivery = tv;
        rj->time_event = avahi_time_event_queue_add(s->time_event_queue, &rj->delivery, elapse_callback, rj);
        rj->flush_cache = flush_cache;

        if ((rj->querier_valid = !!querier))
            rj->querier = *querier;

        return TRUE;
    }
}

void avahi_response_scheduler_incoming(AvahiResponseScheduler *s, AvahiRecord *record, gboolean flush_cache) {
    AvahiResponseJob *rj;
    g_assert(s);

    /* This function is called whenever an incoming response was
     * receieved. We drop scheduled responses which match here. The
     * keyword is "DUPLICATE ANSWER SUPPRESION". */
    
    if ((rj = find_scheduled_job(s, record))) {

        if ((!rj->flush_cache || flush_cache) &&    /* flush cache bit was set correctly */
            avahi_record_is_goodbye(record) == avahi_record_is_goodbye(rj->record) &&   /* both goodbye packets, or both not */
            record->ttl >= rj->record->ttl/2) {     /* sensible TTL */

            /* A matching entry was found, so let's mark it done */
/*             avahi_log_debug("Response suppressed by distributed duplicate suppression"); */
            job_mark_done(s, rj);
        }

        return;
    }

    if ((rj = find_history_job(s, record))) {
        /* Found a history job, let's update it */
        avahi_record_unref(rj->record);
        rj->record = avahi_record_ref(record);
    } else
        /* Found no existing history job, so let's create a new one */
        rj = job_new(s, record, AVAHI_DONE);

    rj->flush_cache = flush_cache;
    rj->querier_valid = FALSE;
    
    gettimeofday(&rj->delivery, NULL);
    job_set_elapse_time(s, rj, AVAHI_RESPONSE_HISTORY_MSEC, 0);
}

void avahi_response_scheduler_suppress(AvahiResponseScheduler *s, AvahiRecord *record, const AvahiAddress *querier) {
    AvahiResponseJob *rj;
    
    g_assert(s);
    g_assert(record);
    g_assert(querier);

    if ((rj = find_scheduled_job(s, record))) {
        
        if (rj->querier_valid && avahi_address_cmp(querier, &rj->querier) == 0 && /* same originator */
            avahi_record_is_goodbye(record) == avahi_record_is_goodbye(rj->record) && /* both goodbye packets, or both not */
            record->ttl >= rj->record->ttl/2) {                                  /* sensible TTL */

            /* A matching entry was found, so let's drop it */
/*             avahi_log_debug("Known answer suppression active!"); */
            job_free(s, rj);
        }
    }

    if ((rj = find_suppressed_job(s, record, querier))) {

        /* Let's update the old entry */
        avahi_record_unref(rj->record);
        rj->record = avahi_record_ref(record);
        
    } else {

        /* Create a new entry */
        rj = job_new(s, record, AVAHI_SUPPRESSED);
        rj->querier_valid = TRUE;
        rj->querier = *querier;
    }

    gettimeofday(&rj->delivery, NULL);
    job_set_elapse_time(s, rj, AVAHI_RESPONSE_SUPPRESS_MSEC, 0);
}

void avahi_response_scheduler_force(AvahiResponseScheduler *s) {
    g_assert(s);

    /* Send all scheduled responses immediately */
    while (s->jobs)
        send_response_packet(s, s->jobs);
}
