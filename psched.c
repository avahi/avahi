#include "util.h"
#include "psched.h"

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

        /* Drop query after 100ms from history */
        flx_elapse_time(&tv, 100, 0);
        flx_time_event_queue_update(s->server->time_event_queue, qj->time_event, &tv);
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

void flx_packet_scheduler_post_query(flxPacketScheduler *s, flxKey *key) {
    flxQueryJob *qj;
    GTimeVal tv;
    
    g_assert(s);
    g_assert(key);

    if (look_for_query(s, key))
        return;

    qj = g_new(flxQueryJob, 1);
    qj->key = flx_key_ref(key);
    qj->done = FALSE;

    flx_elapse_time(&tv, 100, 0);
    qj->time_event = flx_time_event_queue_add(s->server->time_event_queue, &tv, query_elapse, qj);
    qj->scheduler = s;

    FLX_LLIST_PREPEND(flxQueryJob, jobs, s->query_jobs, qj);
}

static guint8* packet_add_response_job(flxPacketScheduler *s, flxDnsPacket *p, flxResponseJob *rj) {
    guint8 *d;

    g_assert(s);
    g_assert(p);
    g_assert(rj);

    if ((d = flx_dns_packet_append_record(p, rj->record, FALSE))) {
        GTimeVal tv;

        rj->done = 1;

        /* Drop response after 1s from history */
        flx_elapse_time(&tv, 1000, 0);
        flx_time_event_queue_update(s->server->time_event_queue, rj->time_event, &tv);
    }

    return d;
}
                                 

static void response_elapse(flxTimeEvent *e, gpointer data) {
    flxResponseJob *rj = data;
    flxPacketScheduler *s;
    flxDnsPacket *p;
    guint n;
    guint8 *d;

    g_assert(rj);
    s = rj->scheduler;

    if (rj->done) {
        /* Lets remove it  from the history */
        response_job_free(s, rj);
        return;
    }

    p = flx_dns_packet_new_response(s->interface->hardware->mtu - 200);
    d = packet_add_response_job(s, p, rj);
    g_assert(d);
    n = 1;

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

static flxResponseJob* look_for_response(flxPacketScheduler *s, flxRecord *record) {
    flxResponseJob *rj;

    g_assert(s);
    g_assert(record);

    for (rj = s->response_jobs; rj; rj = rj->jobs_next)
        if (flx_record_equal_no_ttl(rj->record, record))
            return rj;

    return NULL;
}

void flx_packet_scheduler_post_response(flxPacketScheduler *s, flxRecord *record) {
    flxResponseJob *rj;
    GTimeVal tv;
    
    g_assert(s);
    g_assert(record);

    if (look_for_response(s, record))
        return;

    rj = g_new(flxResponseJob, 1);
    rj->record = flx_record_ref(record);
    rj->done = FALSE;

    flx_elapse_time(&tv, 20, 100);
    rj->time_event = flx_time_event_queue_add(s->server->time_event_queue, &tv, response_elapse, rj);
    rj->scheduler = s;

    FLX_LLIST_PREPEND(flxResponseJob, jobs, s->response_jobs, rj);
}

void flx_packet_scheduler_drop_query(flxPacketScheduler *s, flxKey *key) {
    flxQueryJob *qj;
    
    g_assert(s);
    g_assert(key);

    for (qj = s->query_jobs; qj; qj = qj->jobs_next)
        if (flx_key_equal(qj->key, key)) {

            if (!qj->done) {
                GTimeVal tv;
                qj->done = TRUE;
                
                /* Drop query after 100ms from history */
                flx_elapse_time(&tv, 100, 0);
                flx_time_event_queue_update(s->server->time_event_queue, qj->time_event, &tv);
            }

            break;
        }
}

void flx_packet_scheduler_drop_response(flxPacketScheduler *s, flxRecord *record) {
    flxResponseJob *rj;
    
    g_assert(s);
    g_assert(record);

    for  (rj = s->response_jobs; rj; rj = rj->jobs_next)
        if (flx_record_equal_no_ttl(rj->record, record)) {

            if (!rj->done) {
                GTimeVal tv;
                rj->done = TRUE;
                
                /* Drop response after 100ms from history */
                flx_elapse_time(&tv, 100, 0);
                flx_time_event_queue_update(s->server->time_event_queue, rj->time_event, &tv);
            }

            break;
        }
}
