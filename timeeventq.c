#include "timeeventq.h"
#include "util.h"

static gint compare(gconstpointer _a, gconstpointer _b) {
    const flxTimeEvent *a = _a,  *b = _b;

    return flx_timeval_compare(&a->expiry, &b->expiry);
}

static gboolean prepare_func(GSource *source, gint *timeout) {
    flxTimeEventQueue *q = (flxTimeEventQueue*) source;
    flxTimeEvent *e;
    GTimeVal now;

    g_assert(source);
    g_assert(timeout);

    if (!q->prioq->root) {
        *timeout = -1;
        return FALSE;
    }
    
    e = q->prioq->root->data;
    g_assert(e);

    g_source_get_current_time(source, &now);

    if (flx_timeval_compare(&now, &e->expiry) >= 0) {
        *timeout = -1;
        return TRUE;
    }

    *timeout = (gint) (flx_timeval_diff(&e->expiry, &now)/1000);
    
    return FALSE;
}

static gboolean check_func(GSource *source) {
    flxTimeEventQueue *q = (flxTimeEventQueue*) source;
    flxTimeEvent *e;
    GTimeVal now;

    g_assert(source);

    if (!q->prioq->root)
        return FALSE;

    e = q->prioq->root->data;
    g_assert(e);

    g_source_get_current_time(source, &now);
    
    return flx_timeval_compare(&now, &e->expiry) >= 0;
}

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer user_data) {
    flxTimeEventQueue *q = (flxTimeEventQueue*) source;
    GTimeVal now;

    g_assert(source);

    g_source_get_current_time(source, &now);

    while (q->prioq->root) {
        flxTimeEvent *e = q->prioq->root->data;

        if (flx_timeval_compare(&now, &e->expiry) < 0)
            break;

        g_assert(e->callback);
        e->callback(e, e->userdata);
    }

    return TRUE;
}

flxTimeEventQueue* flx_time_event_queue_new(GMainContext *context) {
    flxTimeEventQueue *q;

    static GSourceFuncs source_funcs = {
        prepare_func,
        check_func,
        dispatch_func,
        NULL,
        NULL,
        NULL
    };

    q = (flxTimeEventQueue*) g_source_new(&source_funcs, sizeof(flxTimeEventQueue));
    q->prioq = flx_prio_queue_new(compare);

    g_source_attach(&q->source, context);
    
    return q;
}

void flx_time_event_queue_free(flxTimeEventQueue *q) {
    g_assert(q);

    while (q->prioq->root)
        flx_time_event_queue_remove(q, q->prioq->root->data);
    flx_prio_queue_free(q->prioq);

    g_source_destroy(&q->source);
    g_source_unref(&q->source);
}

flxTimeEvent* flx_time_event_queue_add(flxTimeEventQueue *q, const GTimeVal *timeval, void (*callback)(flxTimeEvent *e, void *userdata), void *userdata) {
    flxTimeEvent *e;
    
    g_assert(q);
    g_assert(timeval);
    g_assert(callback);
    g_assert(userdata);

    e = g_new(flxTimeEvent, 1);
    e->queue = q;
    e->expiry = *timeval;
    e->callback = callback;
    e->userdata = userdata;

    e->node = flx_prio_queue_put(q->prioq, e);
    
    return e;
}

void flx_time_event_queue_remove(flxTimeEventQueue *q, flxTimeEvent *e) {
    g_assert(q);
    g_assert(e);
    g_assert(e->queue == q);

    flx_prio_queue_remove(q->prioq, e->node);
    g_free(e);
}

void flx_time_event_queue_update(flxTimeEventQueue *q, flxTimeEvent *e, const GTimeVal *timeval) {
    g_assert(q);
    g_assert(e);
    g_assert(e->queue == q);

    e->expiry = *timeval;

    flx_prio_queue_shuffle(q->prioq, e->node);
}

flxTimeEvent* flx_time_event_queue_root(flxTimeEventQueue *q) {
    g_assert(q);

    return q->prioq->root ? q->prioq->root->data : NULL;
}

flxTimeEvent* flx_time_event_next(flxTimeEvent *e) {
    g_assert(e);

    return e->node->next->data;
}


