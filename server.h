#ifndef fooflxserverhfoo
#define fooflxserverhfoo

struct _flxEntry;
typedef struct _flxEntry flxEntry;

#include "flx.h"
#include "iface.h"
#include "prioq.h"

struct _flxEntry {
    flxRecord rr;
    gint id;
    gint interface;
    guchar protocol;

    gboolean unique;

    flxEntry *next, *prev;
    flxEntry *next_by_name, *prev_by_name;
    flxEntry *next_by_id, *prev_by_id;
};

typedef struct _flxQueryJob {
    gint ref;
    GTimeVal time;
    flxQuery query;
} flxQueryJob;

typedef struct _flxQueryJobInstance {
    flxPrioQueueNode *node;
    flxQueryJob *job;
    gint interface;
    guchar protocol;
} flxQueryJobInstance;

typedef struct _flxResponseJob {
    gint ref;
    GTimeVal time;
    flxRecord response;
} flxResponseJob;

typedef struct _flxResponseJobInstance {
    flxPrioQueueNode *node;
    flxResponseJob *job;
    gint interface;
    guchar protocol;
} flxResponseJobInstance;

struct _flxServer {
    GMainContext *context;
    flxInterfaceMonitor *monitor;

    gint current_id;
    
    GHashTable *rrset_by_id;
    GHashTable *rrset_by_name;

    flxEntry *entries;

    flxPrioQueue *query_job_queue;
    flxPrioQueue *response_job_queue;

    gint hinfo_rr_id;

    gchar *hostname;
};

flxQueryJob* flx_query_job_new(void);
flxQueryJob* flx_query_job_ref(flxQueryJob *job);
void flx_query_job_unref(flxQueryJob *job);

void flx_server_post_query_job(flxServer *s, gint interface, guchar protocol, const GTimeVal *tv, const flxQuery *q);
void flx_server_drop_query_job(flxServer *s, gint interface, guchar protocol, const flxQuery *q);

void flx_server_remove_query_job_instance(flxServer *s, flxQueryJobInstance *i);

gboolean flx_query_equal(const flxQuery *a, const flxQuery *b);

#endif
