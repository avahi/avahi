#include "announce.h"
#include "util.h"

#define FLX_ANNOUNCEMENT_JITTER_MSEC 0

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

    flx_interface_post_response(a->interface, a->entry->record, FALSE);

    if (a->n_announced++ <= 8)
        a->sec_delay *= 2;

    g_message("Announcement #%i on interface %s.%i for entry [%s]", a->n_announced, a->interface->hardware->name, a->interface->protocol, t = flx_record_to_string(a->entry->record));
    g_free(t);

    if (a->n_announced >= 4) {
        g_message("Enough announcements for record [%s]", t = flx_record_to_string(a->entry->record));
        g_free(t);
        remove_announcement(a->server, a);
    } else { 
        flx_elapse_time(&tv, a->sec_delay*1000, FLX_ANNOUNCEMENT_JITTER_MSEC);
        flx_time_event_queue_update(a->server->time_event_queue, a->time_event, &tv);
    }
}

static void new_announcement(flxServer *s, flxInterface *i, flxServerEntry *e) {
    flxAnnouncement *a;
    GTimeVal tv;
    gchar *t;

    g_assert(s);
    g_assert(i);
    g_assert(e);

    g_message("NEW ANNOUNCEMENT: %s.%i [%s]", i->hardware->name, i->protocol, t = flx_record_to_string(e->record));
    g_free(t);
    
    if (!flx_interface_match(i, e->interface, e->protocol) || !i->announcing)
        return;

    /* We don't want duplicates */
    for (a = e->announcements; a; a = a->by_entry_next)
        if (a->interface == i)
            return;
    
    g_message("New announcement on interface %s.%i for entry [%s]", i->hardware->name, i->protocol, t = flx_record_to_string(e->record));
    g_free(t);
    
    flx_interface_post_response(i, e->record, FALSE);
    
    a = g_new(flxAnnouncement, 1);
    a->server = s;
    a->interface = i;
    a->entry = e;
    a->n_announced = 1;
    a->sec_delay = 1;
    
    FLX_LLIST_PREPEND(flxAnnouncement, by_interface, i->announcements, a);
    FLX_LLIST_PREPEND(flxAnnouncement, by_entry, e->announcements, a);
    
    flx_elapse_time(&tv, a->sec_delay*1000, FLX_ANNOUNCEMENT_JITTER_MSEC);
    a->time_event = flx_time_event_queue_add(s->time_event_queue, &tv, elapse_announce, a);
}

void flx_announce_interface(flxServer *s, flxInterface *i) {
    flxServerEntry *e;
    
    g_assert(s);
    g_assert(i);

    if (!i->announcing)
        return;

    g_message("ANNOUNCE INTERFACE");
    
    for (e = s->entries; e; e = e->entry_next)
        new_announcement(s, i, e);
}

static void announce_walk_callback(flxInterfaceMonitor *m, flxInterface *i, gpointer userdata) {
    flxServerEntry *e = userdata;
    
    g_assert(m);
    g_assert(i);
    g_assert(e);

    new_announcement(m->server, i, e);
}

void flx_announce_entry(flxServer *s, flxServerEntry *e) {
    g_assert(s);
    g_assert(e);

    g_message("ANNOUNCE ENTRY");

    flx_interface_monitor_walk(s->monitor, e->interface, e->protocol, announce_walk_callback, e);
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
                flx_interface_post_response(i, g, TRUE);
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
