#ifndef fooprioqhfoo
#define fooprioqhfoo

/* $Id$ */

/***
  This file is part of avahi.
 
  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.
 
  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

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
