#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "prioq.h"

static gint compare_int(gconstpointer a, gconstpointer b) {
    gint i = GPOINTER_TO_INT(a), j = GPOINTER_TO_INT(b);

    return i < j ? -1 : (i > j ? 1 : 0);
}

static int compare_ptr(gconstpointer a, gconstpointer b) {
    return a < b ? -1 : (a > b ? 1 : 0);
}

static void rec(flxPrioQueueNode *n) {
    if (!n)
        return;

    if (n->left)
        g_assert(n->left->parent == n);

    if (n->right)
        g_assert(n->right->parent == n);

    if (n->parent) {
        g_assert(n->parent->left == n || n->parent->right == n);

        if (n->parent->left == n)
            g_assert(n->next == n->parent->right);
    }

    if (!n->next) {
        g_assert(n->queue->last == n);

        if (n->parent && n->parent->left == n)
            g_assert(n->parent->right == NULL);
    }

    
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
    flxPrioQueue *q, *q2;
    gint i, prev;

    q = flx_prio_queue_new(compare_int);
    q2 = flx_prio_queue_new(compare_ptr);

    srand(time(NULL));

    for (i = 0; i < 10000; i++)
        flx_prio_queue_put(q2, flx_prio_queue_put(q, GINT_TO_POINTER(random() & 0xFFFF)));

    while (q2->root) {
        rec(q->root);
        rec(q2->root);

        g_assert(q->n_nodes == q2->n_nodes);

        printf("%i\n", GPOINTER_TO_INT(((flxPrioQueueNode*)q2->root->data)->data));
        
        flx_prio_queue_remove(q, q2->root->data);
        flx_prio_queue_remove(q2, q2->root);
    }

        
/*     prev = 0; */
/*     while (q->root) { */
/*         gint v = GPOINTER_TO_INT(q->root->data); */
/*         rec(q->root); */
/*         printf("%i\n", v); */
/*         flx_prio_queue_remove(q, q->root); */
/*         g_assert(v >= prev); */
/*         prev = v; */
/*     } */

    flx_prio_queue_free(q);
    return 0;
}
