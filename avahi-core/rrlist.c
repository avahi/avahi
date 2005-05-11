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

#include "rrlist.h"
#include "llist.h"

typedef struct AvahiRecordListItem AvahiRecordListItem;

struct AvahiRecordListItem {
    AvahiRecord *record;
    gboolean unicast_response;
    gboolean flush_cache;
    AVAHI_LLIST_FIELDS(AvahiRecordListItem, items);
};


struct AvahiRecordList {
    AVAHI_LLIST_HEAD(AvahiRecordListItem, items);
};

AvahiRecordList *avahi_record_list_new(void) {
    AvahiRecordList *l = g_new(AvahiRecordList, 1);
    AVAHI_LLIST_HEAD_INIT(AvahiRecordListItem, l->items);
    return l;
}

void avahi_record_list_free(AvahiRecordList *l) {
    g_assert(l);

    avahi_record_list_flush(l);
    g_free(l);
}

static void item_free(AvahiRecordList *l, AvahiRecordListItem *i) {
    g_assert(i);

    AVAHI_LLIST_REMOVE(AvahiRecordListItem, items, l->items, i);
    avahi_record_unref(i->record);
    g_free(i);
}

void avahi_record_list_flush(AvahiRecordList *l) {
    g_assert(l);
    
    while (l->items)
        item_free(l, l->items);
}

AvahiRecord* avahi_record_list_pop(AvahiRecordList *l, gboolean *flush_cache, gboolean *unicast_response) {
    AvahiRecord *r;

    if (!l->items)
        return NULL;
    
    r = avahi_record_ref(l->items->record);
    if (unicast_response) *unicast_response = l->items->unicast_response;
    if (flush_cache) *flush_cache = l->items->flush_cache;

    item_free(l, l->items);
    
    return r;
}

void avahi_record_list_push(AvahiRecordList *l, AvahiRecord *r, gboolean flush_cache, gboolean unicast_response) {
    AvahiRecordListItem *i;
        
    g_assert(l);
    g_assert(r);
    
    for (i = l->items; i; i = i->items_next)
        if (avahi_record_equal_no_ttl(i->record, r))
            return;

    i = g_new(AvahiRecordListItem, 1);
    i->unicast_response = unicast_response;
    i->flush_cache = flush_cache;
    i->record = avahi_record_ref(r);
    AVAHI_LLIST_PREPEND(AvahiRecordListItem, items, l->items, i);
}

void avahi_record_list_drop(AvahiRecordList *l, AvahiRecord *r) {
    AvahiRecordListItem *i;

    g_assert(l);
    g_assert(r);

    for (i = l->items; i; i = i->items_next)
        if (avahi_record_equal_no_ttl(i->record, r)) {
            item_free(l, i);
            break;
        }
}


gboolean avahi_record_list_empty(AvahiRecordList *l) {
    g_assert(l);
    
    return !l->items;
}
