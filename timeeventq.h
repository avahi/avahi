#ifndef footimeeventqhfoo
#define footimeeventqhfoo

typedef struct _AvahiTimeEventQueue AvahiTimeEventQueue;
typedef struct _AvahiTimeEvent AvahiTimeEvent;

#include "prioq.h"

struct _AvahiTimeEvent {
    AvahiTimeEventQueue *queue;
    AvahiPrioQueueNode *node;
    GTimeVal expiry;
    void (*callback)(AvahiTimeEvent *e, void *userdata);
    void *userdata;
};

struct _AvahiTimeEventQueue {
    GSource source;
    AvahiPrioQueue *prioq;
};

AvahiTimeEventQueue* avahi_time_event_queue_new(GMainContext *context, gint priority);
void avahi_time_event_queue_free(AvahiTimeEventQueue *q);

AvahiTimeEvent* avahi_time_event_queue_add(AvahiTimeEventQueue *q, const GTimeVal *timeval, void (*callback)(AvahiTimeEvent *e, void *userdata), void *userdata);
void avahi_time_event_queue_remove(AvahiTimeEventQueue *q, AvahiTimeEvent *e);

void avahi_time_event_queue_update(AvahiTimeEventQueue *q, AvahiTimeEvent *e, const GTimeVal *timeval);

AvahiTimeEvent* avahi_time_event_queue_root(AvahiTimeEventQueue *q);
AvahiTimeEvent* avahi_time_event_next(AvahiTimeEvent *e);




#endif
