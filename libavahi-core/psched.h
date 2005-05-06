#ifndef foopschedhfoo
#define foopschedhfoo

typedef struct _AvahiQueryJob AvahiQueryJob;
typedef struct _AvahiResponseJob AvahiResponseJob;
typedef struct _AvahiPacketScheduler AvahiPacketScheduler;
typedef struct _AvahiKnownAnswer AvahiKnownAnswer;
typedef struct _AvahiProbeJob AvahiProbeJob;

#include "timeeventq.h"
#include "rr.h"
#include "llist.h"
#include "iface.h"

struct _AvahiQueryJob {
    AvahiPacketScheduler *scheduler;
    AvahiTimeEvent *time_event;
    AvahiKey *key;
    gboolean done;
    GTimeVal delivery;
    AVAHI_LLIST_FIELDS(AvahiQueryJob, jobs);
};

struct _AvahiResponseJob {
    AvahiPacketScheduler *scheduler;
    AvahiTimeEvent *time_event;
    AvahiRecord *record;
    AvahiAddress address;
    gboolean address_valid;
    gboolean done;
    GTimeVal delivery;
    gboolean flush_cache;
    AVAHI_LLIST_FIELDS(AvahiResponseJob, jobs);
};

struct _AvahiKnownAnswer {
    AvahiPacketScheduler *scheduler;
    AvahiRecord *record;

    AVAHI_LLIST_FIELDS(AvahiKnownAnswer, known_answer);
};

struct _AvahiProbeJob {
    AvahiPacketScheduler *scheduler;
    AvahiTimeEvent *time_event;
    AvahiRecord *record;

    gboolean chosen; /* Use for packet assembling */
    GTimeVal delivery;
    
    AVAHI_LLIST_FIELDS(AvahiProbeJob, jobs);
};

struct _AvahiPacketScheduler {
    AvahiServer *server;
    
    AvahiInterface *interface;

    AVAHI_LLIST_HEAD(AvahiQueryJob, query_jobs);
    AVAHI_LLIST_HEAD(AvahiResponseJob, response_jobs);
    AVAHI_LLIST_HEAD(AvahiKnownAnswer, known_answers);
    AVAHI_LLIST_HEAD(AvahiProbeJob, probe_jobs);
};

AvahiPacketScheduler *avahi_packet_scheduler_new(AvahiServer *server, AvahiInterface *i);
void avahi_packet_scheduler_free(AvahiPacketScheduler *s);

void avahi_packet_scheduler_post_query(AvahiPacketScheduler *s, AvahiKey *key, gboolean immediately);
void avahi_packet_scheduler_post_response(AvahiPacketScheduler *s, const AvahiAddress *a, AvahiRecord *record, gboolean flush_cache, gboolean immediately);
void avahi_packet_scheduler_post_probe(AvahiPacketScheduler *s, AvahiRecord *record, gboolean immediately);

void avahi_packet_scheduler_incoming_query(AvahiPacketScheduler *s, AvahiKey *key);
void avahi_packet_scheduler_incoming_response(AvahiPacketScheduler *s, AvahiRecord *record);
void avahi_packet_scheduler_incoming_known_answer(AvahiPacketScheduler *s, AvahiRecord *record, const AvahiAddress *a);

void avahi_packet_scheduler_flush_responses(AvahiPacketScheduler *s);

#endif
