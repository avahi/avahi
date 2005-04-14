#ifndef foopschedhfoo
#define foopschedhfoo

typedef struct _flxQueryJob flxQueryJob;
typedef struct _flxResponseJob flxResponseJob;
typedef struct _flxPacketScheduler flxPacketScheduler;
typedef struct _flxKnownAnswer flxKnownAnswer;
typedef struct _flxProbeJob flxProbeJob;

#include "timeeventq.h"
#include "rr.h"
#include "llist.h"
#include "iface.h"

struct _flxQueryJob {
    flxPacketScheduler *scheduler;
    flxTimeEvent *time_event;
    flxKey *key;
    gboolean done;
    GTimeVal delivery;
    FLX_LLIST_FIELDS(flxQueryJob, jobs);
};

struct _flxResponseJob {
    flxPacketScheduler *scheduler;
    flxTimeEvent *time_event;
    flxRecord *record;
    flxAddress address;
    gboolean address_valid;
    gboolean done;
    GTimeVal delivery;
    gboolean flush_cache;
    FLX_LLIST_FIELDS(flxResponseJob, jobs);
};

struct _flxKnownAnswer {
    flxPacketScheduler *scheduler;
    flxRecord *record;

    FLX_LLIST_FIELDS(flxKnownAnswer, known_answer);
};

struct _flxProbeJob {
    flxPacketScheduler *scheduler;
    flxTimeEvent *time_event;
    flxRecord *record;

    gboolean chosen; /* Use for packet assembling */
    GTimeVal delivery;
    
    FLX_LLIST_FIELDS(flxProbeJob, jobs);
};

struct _flxPacketScheduler {
    flxServer *server;
    
    flxInterface *interface;

    FLX_LLIST_HEAD(flxQueryJob, query_jobs);
    FLX_LLIST_HEAD(flxResponseJob, response_jobs);
    FLX_LLIST_HEAD(flxKnownAnswer, known_answers);
    FLX_LLIST_HEAD(flxProbeJob, probe_jobs);
};

flxPacketScheduler *flx_packet_scheduler_new(flxServer *server, flxInterface *i);
void flx_packet_scheduler_free(flxPacketScheduler *s);

void flx_packet_scheduler_post_query(flxPacketScheduler *s, flxKey *key, gboolean immediately);
void flx_packet_scheduler_post_response(flxPacketScheduler *s, const flxAddress *a, flxRecord *record, gboolean flush_cache, gboolean immediately);
void flx_packet_scheduler_post_probe(flxPacketScheduler *s, flxRecord *record, gboolean immediately);

void flx_packet_scheduler_incoming_query(flxPacketScheduler *s, flxKey *key);
void flx_packet_scheduler_incoming_response(flxPacketScheduler *s, flxRecord *record);
void flx_packet_scheduler_incoming_known_answer(flxPacketScheduler *s, flxRecord *record, const flxAddress *a);

void flx_packet_scheduler_flush_responses(flxPacketScheduler *s);

#endif
