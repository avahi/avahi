#ifndef fooprioqhfoo
#define fooprioqhfoo

#include <glib.h>

struct _AvahiPrioQueue;
typedef struct _AvahiPrioQueue AvahiPrioQueue;

struct _AvahiPrioQueueNode;
typedef struct _AvahiPrioQueueNode AvahiPrioQueueNode;

struct _AvahiPrioQueue {
    AvahiPrioQueueNode *root, *last;
    
    guint n_nodes;
    gint (*compare) (gconstpointer a, gconstpointer b);
};

struct _AvahiPrioQueueNode {
    AvahiPrioQueue *queue;
    gpointer data;
    guint x, y;

    AvahiPrioQueueNode *left, *right, *parent, *next, *prev;
};

AvahiPrioQueue* avahi_prio_queue_new(gint (*compare) (gconstpointer a, gconstpointer b));
void avahi_prio_queue_free(AvahiPrioQueue *q);

AvahiPrioQueueNode* avahi_prio_queue_put(AvahiPrioQueue *q, gpointer data);
void avahi_prio_queue_remove(AvahiPrioQueue *q, AvahiPrioQueueNode *n);

void avahi_prio_queue_shuffle(AvahiPrioQueue *q, AvahiPrioQueueNode *n);

#endif
