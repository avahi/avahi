#include <string.h>

#include "util.h"
#include "psched.h"

#define FLX_QUERY_HISTORY_MSEC 100
#define FLX_QUERY_DEFER_MSEC 100
#define FLX_RESPONSE_HISTORY_MSEC 700
#define FLX_RESPONSE_DEFER_MSEC 20
#define FLX_RESPONSE_JITTER_MSEC 100
#define FLX_PROBE_DEFER_MSEC 100

flxPacketScheduler *flx_packet_scheduler_new(flxServer *server, flxInterface *i) {
    flxPacketScheduler *s;

    g_assert(server);
    g_assert(i);

    s = g_new(flxPacketScheduler, 1);
    s->server = server;
    s->interface = i;

    FLX_LLIST_HEAD_INIT(flxQueryJob, s->query_jobs);
    FLX_LLIST_HEAD_INIT(flxResponseJob, s->response_jobs);
    FLX_LLIST_HEAD_INIT(flxKnownAnswer, s->known_answers);
    FLX_LLIST_HEAD_INIT(flxProbeJob, s->probe_jobs);
    
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

static void probe_job_free(flxPacketScheduler *s, flxProbeJob *pj) {
    g_assert(pj);

    if (pj->time_event)
        flx_time_event_queue_remove(pj->scheduler->server->time_event_queue, pj->time_event);

    FLX_LLIST_REMOVE(flxProbeJob, jobs, s->probe_jobs, pj);

    flx_record_unref(pj->record);
    g_free(pj);
}

void flx_packet_scheduler_free(flxPacketScheduler *s) {
    flxQueryJob *qj;
    flxResponseJob *rj;
    flxProbeJob *pj;
    flxTimeEvent *e;

    g_assert(s);

    g_assert(!s->known_answers);
    
    while ((qj = s->query_jobs))
        query_job_free(s, qj);
    while ((rj = s->response_jobs))
        response_job_free(s, rj);
    while ((pj = s->probe_jobs))
        probe_job_free(s, pj);

    g_free(s);
}

static gpointer known_answer_walk_callback(flxCache *c, flxKey *pattern, flxCacheEntry *e, gpointer userdata) {
    flxPacketScheduler *s = userdata;
    flxKnownAnswer *ka;
    
    g_assert(c);
    g_assert(pattern);
    g_assert(e);
    g_assert(s);

    if (flx_cache_entry_half_ttl(c, e))
        return NULL;
    
    ka = g_new0(flxKnownAnswer, 1);
    ka->scheduler = s;
    ka->record = flx_record_ref(e->record);

    FLX_LLIST_PREPEND(flxKnownAnswer, known_answer, s->known_answers, ka);
    return NULL;
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

        /* Add all matching known answers to the list */
        flx_cache_walk(s->interface->cache, qj->key, known_answer_walk_callback, s);
    }

    return d;
}

static void append_known_answers_and_send(flxPacketScheduler *s, flxDnsPacket *p) {
    flxKnownAnswer *ka;
    guint n;
    g_assert(s);
    g_assert(p);

    n = 0;
    
    while ((ka = s->known_answers)) {

        while (!flx_dns_packet_append_record(p, ka->record, FALSE)) {

            g_assert(!flx_dns_packet_is_empty(p));

            flx_dns_packet_set_field(p, FLX_DNS_FIELD_FLAGS, flx_dns_packet_get_field(p, FLX_DNS_FIELD_FLAGS) | FLX_DNS_FLAG_TC);
            flx_dns_packet_set_field(p, FLX_DNS_FIELD_ANCOUNT, n);
            flx_interface_send_packet(s->interface, p);
            flx_dns_packet_free(p);

            p = flx_dns_packet_new_query(s->interface->hardware->mtu - 48);
            n = 0;
        }

        FLX_LLIST_REMOVE(flxKnownAnswer, known_answer, s->known_answers, ka);
        flx_record_unref(ka->record);
        g_free(ka);
        
        n++;
    }
    
    flx_dns_packet_set_field(p, FLX_DNS_FIELD_ANCOUNT, n);
    flx_interface_send_packet(s->interface, p);
    flx_dns_packet_free(p);
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

    g_assert(!s->known_answers);
    
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

    flx_dns_packet_set_field(p, FLX_DNS_FIELD_QDCOUNT, n);

    /* Now add known answers */
    append_known_answers_and_send(s, p);
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

    for (qj = s->query_jobs; qj; qj = qj->jobs_next)

        if (flx_key_equal(qj->key, key)) {

            glong d = flx_timeval_diff(&tv, &qj->delivery);

            /* Duplicate questions suppression */
            if (d >= 0 && d <= FLX_QUERY_HISTORY_MSEC*1000) {
                g_message("WARNING! DUPLICATE QUERY SUPPRESSION ACTIVE!");
                return;
            }
            
            query_job_free(s, qj);
            break;
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

    if ((d = flx_dns_packet_append_record(p, rj->record, rj->flush_cache))) {
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

    flx_dns_packet_set_field(p, FLX_DNS_FIELD_ANCOUNT, n);
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
    rj->address_valid = FALSE;
    rj->flush_cache = FALSE;
    
    FLX_LLIST_PREPEND(flxResponseJob, jobs, s->response_jobs, rj);

    return rj;
}

void flx_packet_scheduler_post_response(flxPacketScheduler *s, const flxAddress *a, flxRecord *record, gboolean flush_cache, gboolean immediately) {
    flxResponseJob *rj;
    GTimeVal tv;
    gchar *t;
    
    g_assert(s);
    g_assert(record);

    g_assert(!flx_key_is_pattern(record->key));
    
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

            /* This job is no longer specific to a single querier, so
             * make sure it isn't suppressed by known answer
             * suppresion */

            if (rj->address_valid && (!a || flx_address_cmp(a, &rj->address) != 0))
                rj->address_valid = FALSE;

            rj->flush_cache = flush_cache;
            
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
    rj->flush_cache = flush_cache;
    rj->delivery = tv;
    rj->time_event = flx_time_event_queue_add(s->server->time_event_queue, &rj->delivery, response_elapse, rj);

    /* Store the address of the host this messages is intended to, so
       that we can drop this job in case a truncated message with
       known answer suppresion entries is recieved */

    if ((rj->address_valid = !!a))
        rj->address = *a;
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

void flx_packet_scheduler_incoming_known_answer(flxPacketScheduler *s, flxRecord *record, const flxAddress *a) {
    flxResponseJob *rj;
    
    g_assert(s);
    g_assert(record);
    g_assert(a);

    for (rj = s->response_jobs; rj; rj = rj->jobs_next) {

        g_assert(record->ttl > 0);
        g_assert(rj->record->ttl/2);
        
        if (flx_record_equal_no_ttl(rj->record, record))
            if (rj->address_valid)
                if (flx_address_cmp(&rj->address, a))
                    if (record->ttl >= rj->record->ttl/2) {

            /* Let's suppress it */

            response_job_free(s, rj);
            break;
        }
    }
}

void flx_packet_scheduler_flush_responses(flxPacketScheduler *s) {
    flxResponseJob *rj;
    
    g_assert(s);

    /* Send all scheduled responses, ignoring the scheduled time */
    
    for (rj = s->response_jobs; rj; rj = rj->jobs_next)
        if (!rj->done)
            send_response_packet(s, rj);
}

static flxProbeJob* probe_job_new(flxPacketScheduler *s, flxRecord *record) {
    flxProbeJob *pj;
    
    g_assert(s);
    g_assert(record);

    pj = g_new(flxProbeJob, 1);
    pj->scheduler = s;
    pj->record = flx_record_ref(record);
    pj->time_event = NULL;
    pj->chosen = FALSE;
    
    FLX_LLIST_PREPEND(flxProbeJob, jobs, s->probe_jobs, pj);

    return pj;
}

static guint8* packet_add_probe_query(flxPacketScheduler *s, flxDnsPacket *p, flxProbeJob *pj) {
    guint size;
    guint8 *ret;
    flxKey *k;

    g_assert(s);
    g_assert(p);
    g_assert(pj);

    g_assert(!pj->chosen);
    
    /* Estimate the size for this record */
    size =
        flx_key_get_estimate_size(pj->record->key) +
        flx_record_get_estimate_size(pj->record);

    /* Too large */
    if (size > flx_dns_packet_space(p))
        return NULL;

    /* Create the probe query */
    k = flx_key_new(pj->record->key->name, pj->record->key->class, FLX_DNS_TYPE_ANY);
    ret = flx_dns_packet_append_key(p, k);
    g_assert(ret);

    /* Mark this job for addition to the packet */
    pj->chosen = TRUE;

    /* Scan for more jobs whith matching key pattern */
    for (pj = s->probe_jobs; pj; pj = pj->jobs_next) {
        if (pj->chosen)
            continue;

        /* Does the record match the probe? */
        if (k->class != pj->record->key->class || flx_domain_equal(k->name, pj->record->key->name))
            continue;
        
        /* This job wouldn't fit in */
        if (flx_record_get_estimate_size(pj->record) > flx_dns_packet_space(p))
            break;

        /* Mark this job for addition to the packet */
        pj->chosen = TRUE;
    }

    flx_key_unref(k);
            
    return ret;
}

static void probe_elapse(flxTimeEvent *e, gpointer data) {
    flxProbeJob *pj = data, *next;
    flxPacketScheduler *s;
    flxDnsPacket *p;
    guint n;
    guint8 *d;

    g_assert(pj);
    s = pj->scheduler;

    p = flx_dns_packet_new_query(s->interface->hardware->mtu - 48);

    /* Add the import probe */
    if (!packet_add_probe_query(s, p, pj)) {
        g_warning("Record too large! ---");
        flx_dns_packet_free(p);
        return;
    }

    n = 1;
    
    /* Try to fill up packet with more probes, if available */
    for (pj = s->probe_jobs; pj; pj = pj->jobs_next) {

        if (pj->chosen)
            continue;
        
        if (!packet_add_probe_query(s, p, pj))
            break;
        
        n++;
    }

    flx_dns_packet_set_field(p, FLX_DNS_FIELD_QDCOUNT, n);

    n = 0;

    /* Now add the chosen records to the authorative section */
    for (pj = s->probe_jobs; pj; pj = next) {

        next = pj->jobs_next;

        if (!pj->chosen)
            continue;

        if (!flx_dns_packet_append_record(p, pj->record, TRUE)) {
            g_warning("Bad probe size estimate!");

            /* Unmark all following jobs */
            for (; pj; pj = pj->jobs_next)
                pj->chosen = FALSE;
            
            break;
        }

        probe_job_free(s, pj);
        n ++;
    }
    
    flx_dns_packet_set_field(p, FLX_DNS_FIELD_NSCOUNT, n);

    /* Send it now */
    flx_interface_send_packet(s->interface, p);
    flx_dns_packet_free(p);
}

void flx_packet_scheduler_post_probe(flxPacketScheduler *s, flxRecord *record, gboolean immediately) {
    flxProbeJob *pj;
    GTimeVal tv;
    
    g_assert(s);
    g_assert(record);
    g_assert(!flx_key_is_pattern(record->key));
    
    flx_elapse_time(&tv, immediately ? 0 : FLX_PROBE_DEFER_MSEC, 0);

    /* No duplication check here... */
    /* Create a new job and schedule it */
    pj = probe_job_new(s, record);
    pj->delivery = tv;
    pj->time_event = flx_time_event_queue_add(s->server->time_event_queue, &pj->delivery, probe_elapse, pj);
}
