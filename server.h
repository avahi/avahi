#ifndef fooflxserverhfoo
#define fooflxserverhfoo

#include "flx.h"
#include "iface.h"

struct _flxEntry;
typedef struct _flxEntry flxEntry;
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
    flxQuery query;
} flxQueryJob;

struct _flxQueryJobInstance;
typedef struct _flxQueryJobInstance flxQueryJobInstance;
struct _flxQueryJobInstance {
    flxQueryJob *job;
    gint interface;
    guchar protocol;
    flxQueryJobInstance *next, *prev;
};

typedef struct _flxResponseJob {
    gint ref;
    flxRecord response;
} flxResponseJob;

struct _flxResponseJobInstance;
typedef struct _flxResponseJobInstance flxResponseJobInstance;
struct _flxResponseJobInstance {
    flxResponseJob *job;
    gint interface;
    guchar protocol;
    flxResponseJob *next, *prev;
};

struct _flxServer {
    GMainContext *context;
    flxInterfaceMonitor *monitor;

    gint current_id;
    
    GHashTable *rrset_by_id;
    GHashTable *rrset_by_name;

    flxEntry *entries;

    flxResponseJobInstance *first_response_job, *last_response_job;
    flxQueryJobInstance *first_query_job, *last_query_job;
};

flxQueryJob* flx_query_job_new(void);
flxQueryJob* flx_query_job_ref(flxQueryJob *job);
void flx_query_job_unref(flxQueryJob *job);

void flx_server_post_query_job(flxServer *s, gint interface, guchar protocol, const flxQuery *q);
void flx_server_drop_query_job(flxServer *s, gint interface, guchar protocol, const flxQuery *q);

void flx_server_remove_query_job_instance(flxServer *s, flxQueryJobInstance *i);

gboolean flx_query_equal(const flxQuery *a, const flxQuery *b);


#endif
