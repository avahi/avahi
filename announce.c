#include "announce.h"
#include "util.h"

static void remove_announcement(flxServer *s, flxAnnouncement *a) {
    g_assert(s);
    g_assert(a);

    flx_time_event_queue_remove(s->time_event_queue, a->time_event);

    FLX_LLIST_REMOVE(flxAnnouncement, by_interface, a->interface->announcements, a);
    FLX_LLIST_REMOVE(flxAnnouncement, by_entry, a->entry->announcements, a);
    
    g_free(a);
}

static void elapse_announce(flxTimeEvent *e, void *userdata) {
    flxAnnouncement *a = userdata;
    GTimeVal tv;
    gchar *t;
        
    g_assert(e);
    g_assert(a);

    if (a->n_announced >= 3) {
        g_message("Enough announcements for record [%s]", t = flx_record_to_string(a->entry->record));
        g_free(t);
        remove_announcement(a->server, a);
        return;
    }

    flx_interface_post_response(a->interface, a->entry->record);
    a->n_announced++;

    g_message("Announcement #%i on interface %s.%i for entry [%s]", a->n_announced, a->interface->hardware->name, a->interface->protocol, t = flx_record_to_string(a->entry->record));
    g_free(t);
    
    flx_elapse_time(&tv, 1000, 100);
    flx_time_event_queue_update(a->server->time_event_queue, a->time_event, &tv);
}

static void new_announcement(flxServer *s, flxInterface *i, flxServerEntry *e) {
    flxAnnouncement *a;
    GTimeVal tv;
    gchar *t;

    g_assert(s);
    g_assert(i);
    g_assert(e);

    if (!flx_interface_match(i, e->interface, e->protocol) || !flx_interface_relevant(i))
        return;

    /* We don't want duplicates */
    for (a = e->announcements; a; a = a->by_entry_next)
        if (a->interface == i)
            return;
    
    g_message("New announcement on interface %s.%i for entry [%s]", i->hardware->name, i->protocol, t = flx_record_to_string(e->record));
    g_free(t);
    
    flx_interface_post_response(i, e->record);
    
    a = g_new(flxAnnouncement, 1);
    a->server = s;
    a->interface = i;
    a->entry = e;
    a->n_announced = 1;
    
    FLX_LLIST_PREPEND(flxAnnouncement, by_interface, i->announcements, a);
    FLX_LLIST_PREPEND(flxAnnouncement, by_entry, e->announcements, a);
    
    flx_elapse_time(&tv, 1000, 100);
    a->time_event = flx_time_event_queue_add(s->time_event_queue, &tv, elapse_announce, a);
}

void flx_announce_interface(flxServer *s, flxInterface *i) {
    flxServerEntry *e;
    
    g_assert(s);
    g_assert(i);

    if (!flx_interface_relevant(i))
        return;
    
    for (e = s->entries; e; e = e->entry_next)
        new_announcement(s, i, e);
}

void flx_announce_entry(flxServer *s, flxServerEntry *e) {
    g_assert(s);
    g_assert(e);

    if (e->interface > 0) {

        if (e->protocol != AF_UNSPEC) {
            flxInterface *i;
    
            if ((i = flx_interface_monitor_get_interface(s->monitor, e->interface, e->protocol)))
                new_announcement(s, i, e);
        } else {
            flxHwInterface *hw;

            if ((hw = flx_interface_monitor_get_hw_interface(s->monitor, e->interface))) {
                flxInterface *i;

                for (i = hw->interfaces; i; i = i->by_hardware_next)
                    new_announcement(s, i, e);
            }
        }
    } else {
        flxInterface *i;

        for (i = s->monitor->interfaces; i; i = i->interface_next)
            new_announcement(s, i, e);
    }
}

static flxRecord *make_goodbye_record(flxRecord *r) {
    gchar *t;
    
    g_assert(r);

    g_message("Preparing goodbye for record [%s]", t = flx_record_to_string(r));
    g_free(t);

    return flx_record_new(r->key, r->data, r->size, 0);
}
    
void flx_goodbye_interface(flxServer *s, flxInterface *i, gboolean goodbye) {
    g_assert(s);
    g_assert(i);

    while (i->announcements)
        remove_announcement(s, i->announcements);

    if (goodbye && flx_interface_relevant(i)) {
        flxServerEntry *e;
        
        for (e = s->entries; e; e = e->entry_next)
            if (flx_interface_match(i, e->interface, e->protocol)) {
                flxRecord *g = make_goodbye_record(e->record);
                flx_interface_post_response(i, g);
                flx_record_unref(g);
            }
    }
}

void flx_goodbye_entry(flxServer *s, flxServerEntry *e, gboolean goodbye) {
    g_assert(s);
    g_assert(e);
    
    while (e->announcements)
        remove_announcement(s, e->announcements);
    
    if (goodbye) {
        flxRecord *g = make_goodbye_record(e->record);
        flx_server_post_response(s, e->interface, e->protocol, g);
        flx_record_unref(g);
    }
}

void flx_goodbye_all(flxServer *s, gboolean goodbye) {
    flxServerEntry *e;
    
    g_assert(s);

    for (e = s->entries; e; e = e->entry_next)
        flx_goodbye_entry(s, e, goodbye);
}
