#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "prioq.h"

static gint compare(gpointer a, gpointer b) {
    gint i = GPOINTER_TO_INT(a), j = GPOINTER_TO_INT(b);

    return i < j ? -1 : (i > j ? 1 : 0);
}

static void rec(flxPrioQueueNode *n) {
    if (!n)
        return;

    if (n->parent) {
        int a = GPOINTER_TO_INT(n->parent->data), b = GPOINTER_TO_INT(n->data);
        if (a > b) {
            printf("%i <= %i: NO\n", a, b);
            abort();
        }
    }

    rec(n->left);
    rec(n->right);
}

int main(int argc, char *argv[]) {
    flxPrioQueue *q;
    gint i, prev;

    q = flx_prio_queue_new(compare);

    srand(time(NULL));

    flx_prio_queue_put(q, GINT_TO_POINTER(255)); 
    flx_prio_queue_put(q, GINT_TO_POINTER(255)); 
    
    for (i = 0; i < 10000; i++) 
        flx_prio_queue_put(q, GINT_TO_POINTER(random() & 0xFFFF)); 

    prev = 0;
    while (q->root) {
        gint v = GPOINTER_TO_INT(q->root->data);
        rec(q->root);
        printf("%i\n", v);
        flx_prio_queue_remove(q, q->root);
        g_assert(v >= prev);
        prev = v;
    }

    flx_prio_queue_free(q);
    return 0;
}
