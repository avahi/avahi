#include <string.h>

#include "util.h"
#include "psched.h"

#define FLX_QUERY_HISTORY_MSEC 700
#define FLX_QUERY_DEFER_MSEC 100
#define FLX_RESPONSE_HISTORY_MSEC 700
#define FLX_RESPONSE_DEFER_MSEC 20
#define FLX_RESPONSE_JITTER_MSEC 100

flxPacketScheduler *flx_packet_scheduler_new(flxServer *server, flxInterface *i) {
    flxPacketScheduler *s;

    g_assert(server);
    g_assert(i);

    s = g_new(flxPacketScheduler, 1);
    s->server = server;
    s->interface = i;

    FLX_LLIST_HEAD_INIT(flxQueryJob, s->query_jobs);
    FLX_LLIST_HEAD_INIT(flxResponseJob, s->response_jobs);
    
    return s;
}

static void query_job_free(flxPacketScheduler *s, flxQueryJob *qj) {
    g_assert(qj);

    if (qj->time_event)
        flx_time_event_queue_remove(qj->scheduler->server->time_event_queue, qj->time_event);

    FLX_LLIST_REMOVE(flxQueryJob, jobs, s->query_jobs, qj);
    
    flx_key_unref(qj->key);
    g_free(qj);
}

static void response_job_free(flxPacketScheduler *s, flxResponseJob *rj) {
    g_assert(rj);

    if (rj->time_event)
        flx_time_event_queue_remove(rj->scheduler->server->time_event_queue, rj->time_event);

    FLX_LLIST_REMOVE(flxResponseJob, jobs, s->response_jobs, rj);

    flx_record_unref(rj->record);
    g_free(rj);
}

void flx_packet_scheduler_free(flxPacketScheduler *s) {
    flxQueryJob *qj;
    flxResponseJob *rj;
    flxTimeEvent *e;
    
    g_assert(s);

    while ((qj = s->query_jobs))
        query_job_free(s, qj);
    while ((rj = s->response_jobs))
        response_job_free(s, rj);

    g_free(s);
}

static guint8* packet_add_query_job(flxPacketScheduler *s, flxDnsPacket *p, flxQueryJob *qj) {
    guint8 *d;

    g_assert(s);
    g_assert(p);
    g_assert(qj);

    if ((d = flx_dns_packet_append_key(p, qj->key))) {
        GTimeVal tv;

        qj->done = 1;

        /* Drop query after some time from history from history */
        flx_elapse_time(&tv, FLX_QUERY_HISTORY_MSEC, 0);
        flx_time_event_queue_update(s->server->time_event_queue, qj->time_event, &tv);

        g_get_current_time(&qj->delivery);
    }

    return d;
}
                                 
static void query_elapse(flxTimeEvent *e, gpointer data) {
    flxQueryJob *qj = data;
    flxPacketScheduler *s;
    flxDnsPacket *p;
    guint n;
    guint8 *d;

    g_assert(qj);
    s = qj->scheduler;

    if (qj->done) {
        /* Lets remove it  from the history */
        query_job_free(s, qj);
        return;
    }

    p = flx_dns_packet_new_query(s->interface->hardware->mtu - 48);
    d = packet_add_query_job(s, p, qj);
    g_assert(d);
    n = 1;

    /* Try to fill up packet with more queries, if available */
    for (qj = s->query_jobs; qj; qj = qj->jobs_next) {

        if (qj->done)
            continue;

        if (!packet_add_query_job(s, p, qj))
            break;

        n++;
    }

    flx_dns_packet_set_field(p, DNS_FIELD_QDCOUNT, n);
    flx_interface_send_packet(s->interface, p);
    flx_dns_packet_free(p);
}

static flxQueryJob* look_for_query(flxPacketScheduler *s, flxKey *key) {
    flxQueryJob *qj;
    
    g_assert(s);
    g_assert(key);

    for (qj = s->query_jobs; qj; qj = qj->jobs_next)
        if (flx_key_equal(qj->key, key))
            return qj;

    return NULL;
}

flxQueryJob* query_job_new(flxPacketScheduler *s, flxKey *key) {
    flxQueryJob *qj;
    
    g_assert(s);
    g_assert(key);

    qj = g_new(flxQueryJob, 1);
    qj->scheduler = s;
    qj->key = flx_key_ref(key);
    qj->done = FALSE;
    qj->time_event = NULL;
    
    FLX_LLIST_PREPEND(flxQueryJob, jobs, s->query_jobs, qj);

    return qj;
}

void flx_packet_scheduler_post_query(flxPacketScheduler *s, flxKey *key, gboolean immediately) {
    GTimeVal tv;
    flxQueryJob *qj;
    
    g_assert(s);
    g_assert(key);

    flx_elapse_time(&tv, immediately ? 0 : FLX_QUERY_DEFER_MSEC, 0);
    
    if ((qj = look_for_query(s, key))) {
        glong d = flx_timeval_diff(&tv, &qj->delivery);

        /* Duplicate questions suppression */
        if (d >= 0 && d <= FLX_QUERY_HISTORY_MSEC*1000) {
            g_message("WARNING! DUPLICATE QUERY SUPPRESSION ACTIVE!");
            return;
        }

        query_job_free(s, qj);
    }

    qj = query_job_new(s, key);
    qj->delivery = tv;
    qj->time_event = flx_time_event_queue_add(s->server->time_event_queue, &qj->delivery, query_elapse, qj);
}

static guint8* packet_add_response_job(flxPacketScheduler *s, flxDnsPacket *p, flxResponseJob *rj) {
    guint8 *d;

    g_assert(s);
    g_assert(p);
    g_assert(rj);

    if ((d = flx_dns_packet_append_record(p, rj->record, FALSE))) {
        GTimeVal tv;

        rj->done = 1;

        /* Drop response after some time from history */
        flx_elapse_time(&tv, FLX_RESPONSE_HISTORY_MSEC, 0);
        flx_time_event_queue_update(s->server->time_event_queue, rj->time_event, &tv);

        g_get_current_time(&rj->delivery);
    }

    return d;
}

static void send_response_packet(flxPacketScheduler *s, flxResponseJob *rj) {
    flxDnsPacket *p;
    guint n;

    g_assert(s);

    p = flx_dns_packet_new_response(s->interface->hardware->mtu - 200);
    n = 0;

    /* If a job was specified, put it in the packet. */
    if (rj) {
        guint8 *d;
        d = packet_add_response_job(s, p, rj);
        g_assert(d);
        n++;
    }

    /* Try to fill up packet with more responses, if available */
    for (rj = s->response_jobs; rj; rj = rj->jobs_next) {

        if (rj->done)
            continue;

        if (!packet_add_response_job(s, p, rj))
            break;

        n++;
    }

    flx_dns_packet_set_field(p, DNS_FIELD_ANCOUNT, n);
    flx_interface_send_packet(s->interface, p);
    flx_dns_packet_free(p);
}

static void response_elapse(flxTimeEvent *e, gpointer data) {
    flxResponseJob *rj = data;
    flxPacketScheduler *s;

    g_assert(rj);
    s = rj->scheduler;

    if (rj->done) {
        /* Lets remove it  from the history */
        response_job_free(s, rj);
        return;
    }

    send_response_packet(s, rj);
}

static flxResponseJob* look_for_response(flxPacketScheduler *s, flxRecord *record) {
    flxResponseJob *rj;

    g_assert(s);
    g_assert(record);

    for (rj = s->response_jobs; rj; rj = rj->jobs_next)
        if (flx_record_equal_no_ttl(rj->record, record))
            return rj;

    return NULL;
}

static flxResponseJob* response_job_new(flxPacketScheduler *s, flxRecord *record) {
    flxResponseJob *rj;
    
    g_assert(s);
    g_assert(record);

    rj = g_new(flxResponseJob, 1);
    rj->scheduler = s;
    rj->record = flx_record_ref(record);
    rj->done = FALSE;
    rj->time_event = NULL;
    
    FLX_LLIST_PREPEND(flxResponseJob, jobs, s->response_jobs, rj);

    return rj;
}

void flx_packet_scheduler_post_response(flxPacketScheduler *s, flxRecord *record, gboolean immediately) {
    flxResponseJob *rj;
    GTimeVal tv;
    gchar *t;
    
    g_assert(s);
    g_assert(record);

    flx_elapse_time(&tv, immediately ? 0 : FLX_RESPONSE_DEFER_MSEC, immediately ? 0 : FLX_RESPONSE_JITTER_MSEC);
    
    /* Don't send out duplicates */
    
    if ((rj = look_for_response(s, record))) {
        glong d;

        d = flx_timeval_diff(&tv, &rj->delivery);
        
        /* If there's already a matching packet in our history or in
         * the schedule, we do nothing. */
        if (!!record->ttl == !!rj->record->ttl &&
            d >= 0 && d <= FLX_RESPONSE_HISTORY_MSEC*1000) {
            g_message("WARNING! DUPLICATE RESPONSE SUPPRESSION ACTIVE!");
            return;
        }

        /* Either one was a goodbye packet, but the other was not, so
         * let's drop the older one. */
        response_job_free(s, rj);
    }

    g_message("ACCEPTED NEW RESPONSE [%s]", t = flx_record_to_string(record));
    g_free(t);

    /* Create a new job and schedule it */
    rj = response_job_new(s, record);
    rj->delivery = tv;
    rj->time_event = flx_time_event_queue_add(s->server->time_event_queue, &rj->delivery, response_elapse, rj);
}

void flx_packet_scheduler_incoming_query(flxPacketScheduler *s, flxKey *key) {
    GTimeVal tv;
    flxQueryJob *qj;
    
    g_assert(s);
    g_assert(key);

    /* This function is called whenever an incoming query was
     * receieved. We drop all scheduled queries which match here. The
     * keyword is "DUPLICATE QUESTION SUPPRESION". */

    for (qj = s->query_jobs; qj; qj = qj->jobs_next)
        if (flx_key_equal(qj->key, key)) {

            if (qj->done)
                return;

            goto mark_done;
        }


    /* No matching job was found. Add the query to the history */
    qj = query_job_new(s, key);

mark_done:
    qj->done = TRUE;

    /* Drop the query after some time */
    flx_elapse_time(&tv, FLX_QUERY_HISTORY_MSEC, 0);
    qj->time_event = flx_time_event_queue_add(s->server->time_event_queue, &tv, query_elapse, qj);

    g_get_current_time(&qj->delivery);
}

void response_job_set_elapse_time(flxPacketScheduler *s, flxResponseJob *rj, guint msec, guint jitter) {
    GTimeVal tv;

    g_assert(s);
    g_assert(rj);

    flx_elapse_time(&tv, msec, jitter);

    if (rj->time_event)
        flx_time_event_queue_update(s->server->time_event_queue, rj->time_event, &tv);
    else
        rj->time_event = flx_time_event_queue_add(s->server->time_event_queue, &tv, response_elapse, rj);
    
}

void flx_packet_scheduler_incoming_response(flxPacketScheduler *s, flxRecord *record) {
    flxResponseJob *rj;
    
    g_assert(s);
    g_assert(record);

    /* This function is called whenever an incoming response was
     * receieved. We drop all scheduled responses which match
     * here. The keyword is "DUPLICATE ANSWER SUPPRESION". */
    
    for (rj = s->response_jobs; rj; rj = rj->jobs_next)
        if (flx_record_equal_no_ttl(rj->record, record)) {

            if (rj->done) {

                if (!!record->ttl == !!rj->record->ttl) {
                    /* An entry like this is already in our history,
                     * so let's get out of here! */
                    
                    return;
                    
                } else {
                    /* Either one was a goodbye packet but other was
                     * none. We remove the history entry, and add a
                     * new one */
                    
                    response_job_free(s, rj);
                    break;
                }
        
            } else {

                if (!!record->ttl == !!rj->record->ttl) {

                    /* The incoming packet matches our scheduled
                     * record, so let's mark that one as done */

                    goto mark_done;
                    
                } else {

                    /* Either one was a goodbye packet but other was
                     * none. We ignore the incoming packet. */

                    return;
                }
            }
        }

    /* No matching job was found. Add the query to the history */
    rj = response_job_new(s, record);

mark_done:
    rj->done = TRUE;
                    
    /* Drop response after 500ms from history */
    response_job_set_elapse_time(s, rj, FLX_RESPONSE_HISTORY_MSEC, 0);

    g_get_current_time(&rj->delivery);
}

void flx_packet_scheduler_flush_responses(flxPacketScheduler *s) {
    flxResponseJob *rj;
    
    g_assert(s);

    /* Send all scheduled responses, ignoring the scheduled time */
    
    for (rj = s->response_jobs; rj; rj = rj->jobs_next)
        if (!rj->done)
            send_response_packet(s, rj);
}
