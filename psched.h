#ifndef foopschedhfoo
#define foopschedhfoo

typedef struct _flxQueryJob flxQueryJob;
typedef struct _flxResponseJob flxResponseJob;
typedef struct _flxPacketScheduler flxPacketScheduler;

#include "timeeventq.h"
#include "rr.h"
#include "llist.h"
#include "iface.h"

struct _flxQueryJob {
    flxPacketScheduler *scheduler;
    flxTimeEvent *time_event;
    flxKey *key;
    gboolean done;
    FLX_LLIST_FIELDS(flxQueryJob, jobs);
};

struct _flxResponseJob {
    flxPacketScheduler *scheduler;
    flxTimeEvent *time_event;
    flxRecord *record;
    gboolean done;
    FLX_LLIST_FIELDS(flxResponseJob, jobs);
};

struct _flxPacketScheduler {
    flxServer *server;
    
    flxInterface *interface;

    FLX_LLIST_HEAD(flxQueryJob, query_jobs);
    FLX_LLIST_HEAD(flxResponseJob, response_jobs);
};

flxPacketScheduler *flx_packet_scheduler_new(flxServer *server, flxInterface *i);
void flx_packet_scheduler_free(flxPacketScheduler *s);

void flx_packet_scheduler_post_query(flxPacketScheduler *s, flxKey *key);
void flx_packet_scheduler_post_response(flxPacketScheduler *s, flxRecord *record);

void flx_packet_scheduler_drop_query(flxPacketScheduler *s, flxKey *key);
void flx_packet_scheduler_drop_response(flxPacketScheduler *s, flxRecord *record);

#endif
