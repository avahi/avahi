#ifndef footimeeventqhfoo
#define footimeeventqhfoo

typedef struct _flxTimeEventQueue flxTimeEventQueue;
typedef struct _flxTimeEvent flxTimeEvent;

#include "prioq.h"

struct _flxTimeEvent {
    flxTimeEventQueue *queue;
    flxPrioQueueNode *node;
    GTimeVal expiry;
    void (*callback)(flxTimeEvent *e, void *userdata);
    void *userdata;
};

struct _flxTimeEventQueue {
    GSource source;
    flxPrioQueue *prioq;
};

flxTimeEventQueue* flx_time_event_queue_new(GMainContext *context);
void flx_time_event_queue_free(flxTimeEventQueue *q);

flxTimeEvent* flx_time_event_queue_add(flxTimeEventQueue *q, const GTimeVal *timeval, void (*callback)(flxTimeEvent *e, void *userdata), void *userdata);
void flx_time_event_queue_remove(flxTimeEventQueue *q, flxTimeEvent *e);

void flx_time_event_update(flxTimeEventQueue *q, flxTimeEvent *e, const GTimeVal *timeval);

#endif
