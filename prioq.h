#ifndef fooprioqhfoo
#define fooprioqhfoo

#include <glib.h>

struct _flxPrioQueue;
typedef struct _flxPrioQueue flxPrioQueue;

struct _flxPrioQueueNode;
typedef struct _flxPrioQueueNode flxPrioQueueNode;

struct _flxPrioQueue {
    flxPrioQueueNode *root, *last;
    
    guint n_nodes;
    gint (*compare) (gconstpointer a, gconstpointer b);
};

struct _flxPrioQueueNode {
    flxPrioQueue *queue;
    gpointer data;
    guint x, y;

    flxPrioQueueNode *left, *right, *parent, *next, *prev;
};

flxPrioQueue* flx_prio_queue_new(gint (*compare) (gconstpointer a, gconstpointer b));
void flx_prio_queue_free(flxPrioQueue *q);

flxPrioQueueNode* flx_prio_queue_put(flxPrioQueue *q, gpointer data);
void flx_prio_queue_remove(flxPrioQueue *q, flxPrioQueueNode *n);

void flx_prio_queue_shuffle(flxPrioQueue *q, flxPrioQueueNode *n);

#endif
