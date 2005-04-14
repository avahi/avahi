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

static void elapse_announce(flxTimeEvent *e, void *userdata);

static void send_packet(flxAnnouncement *a) {
    GTimeVal tv;
    g_assert(a);

/*     g_message("%i -- %u", a->state, a->n_iteration); */
    
    if (a->state == FLX_PROBING && a->n_iteration >= 1) {
        flx_interface_post_probe(a->interface, a->entry->record, FALSE);
    } else if (a->state == FLX_ANNOUNCING && a->n_iteration >= 1)
        flx_interface_post_response(a->interface, NULL, a->entry->record, a->entry->flags & FLX_SERVER_ENTRY_UNIQUE, TRUE);

    a->n_iteration++;

    if (a->state == FLX_PROBING) {

        if (a->n_iteration == 1)
            flx_elapse_time(&tv, 0, 250);
        else
            flx_elapse_time(&tv, 250, 0);

        /* Probing done */
        if (a->n_iteration >= 4) {
            gchar *t;
            g_message("Enough probes for record [%s]", t = flx_record_to_string(a->entry->record));
            g_free(t);
            a->state = FLX_ANNOUNCING;
            a->n_iteration = 1;
        }
        
    } else if (a->state == FLX_ANNOUNCING) {

        flx_elapse_time(&tv, a->sec_delay*1000, FLX_ANNOUNCEMENT_JITTER_MSEC);
        
        if (a->n_iteration < 10)
            a->sec_delay *= 2;

        /* Announcing done */
        if (a->n_iteration >= 4) {
            gchar *t;
            g_message("Enough announcements for record [%s]", t = flx_record_to_string(a->entry->record));
            g_free(t);
            remove_announcement(a->server, a);
            return;
        }
    }

    if (a->time_event) 
        flx_time_event_queue_update(a->server->time_event_queue, a->time_event, &tv);
    else
        a->time_event = flx_time_event_queue_add(a->server->time_event_queue, &tv, elapse_announce, a);
}

static void elapse_announce(flxTimeEvent *e, void *userdata) {
    g_assert(e);

    send_packet(userdata);
}

static flxAnnouncement *get_announcement(flxServer *s, flxServerEntry *e, flxInterface *i) {
    flxAnnouncement *a;
    
    g_assert(s);
    g_assert(e);
    g_assert(i);

    for (a = e->announcements; a; a = a->by_entry_next)
        if (a->interface == i)
            return a;

    return NULL;
}

static void new_announcement(flxServer *s, flxInterface *i, flxServerEntry *e) {
    flxAnnouncement *a;
    GTimeVal tv;
    gchar *t; 

    g_assert(s);
    g_assert(i);
    g_assert(e);

/*     g_message("NEW ANNOUNCEMENT: %s.%i [%s]", i->hardware->name, i->protocol, t = flx_record_to_string(e->record)); */
/*     g_free(t); */
    
    if (!flx_interface_match(i, e->interface, e->protocol) || !i->announcing || e->flags & FLX_SERVER_ENTRY_NOANNOUNCE)
        return;

    /* We don't want duplicate announcements */
    if (get_announcement(s, e, i))
        return;

    g_message("New announcement on interface %s.%i for entry [%s]", i->hardware->name, i->protocol, t = flx_record_to_string(e->record));
    g_free(t);

    a = g_new(flxAnnouncement, 1);
    a->server = s;
    a->interface = i;
    a->entry = e;

    a->state = (e->flags & FLX_SERVER_ENTRY_UNIQUE) && !(e->flags & FLX_SERVER_ENTRY_NOPROBE) ? FLX_PROBING : FLX_ANNOUNCING;
    a->n_iteration = 0;
    a->sec_delay = 1;
    a->time_event = NULL;
    
    FLX_LLIST_PREPEND(flxAnnouncement, by_interface, i->announcements, a);
    FLX_LLIST_PREPEND(flxAnnouncement, by_entry, e->announcements, a);

    send_packet(a);
}

void flx_announce_interface(flxServer *s, flxInterface *i) {
    flxServerEntry *e;
    
    g_assert(s);
    g_assert(i);

    if (!i->announcing)
        return;

/*     g_message("ANNOUNCE INTERFACE"); */
    
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

/*     g_message("ANNOUNCE ENTRY"); */

    flx_interface_monitor_walk(s->monitor, e->interface, e->protocol, announce_walk_callback, e);
}

gboolean flx_entry_established(flxServer *s, flxServerEntry *e, flxInterface *i) {
    flxAnnouncement *a;

    g_assert(s);
    g_assert(e);
    g_assert(i);

    if (!(e->flags & FLX_SERVER_ENTRY_UNIQUE) || (e->flags & FLX_SERVER_ENTRY_NOPROBE))
        return TRUE;

    if ((a = get_announcement(s, e, i)))
        if (a->state == FLX_PROBING)
            return FALSE;

    return TRUE;
}


static flxRecord *make_goodbye_record(flxRecord *r) {
    gchar *t;
    flxRecord *g;
    
    g_assert(r);

    g_message("Preparing goodbye for record [%s]", t = flx_record_to_string(r));
    g_free(t);

    g = flx_record_copy(r);
    g_assert(g->ref == 1);
    g->ttl = 0;

    return g;
}


static void send_goodbye_callback(flxInterfaceMonitor *m, flxInterface *i, gpointer userdata) {
    flxServerEntry *e = userdata;
    flxRecord *g;
    
    g_assert(m);
    g_assert(i);
    g_assert(e);

    if (!flx_interface_match(i, e->interface, e->protocol))
        return;

    if (e->flags & FLX_SERVER_ENTRY_NOANNOUNCE)
        return;

    if (!flx_entry_established(m->server, e, i))
        return;
    
    g = make_goodbye_record(e->record);
    flx_interface_post_response(i, NULL, g, e->flags & FLX_SERVER_ENTRY_UNIQUE, TRUE);
    flx_record_unref(g);
}
    
void flx_goodbye_interface(flxServer *s, flxInterface *i, gboolean goodbye) {
    g_assert(s);
    g_assert(i);

    g_message("goodbye interface: %s.%u", i->hardware->name, i->protocol);

    if (goodbye && flx_interface_relevant(i)) {
        flxServerEntry *e;
        
        for (e = s->entries; e; e = e->entry_next)
            send_goodbye_callback(s->monitor, i, e);
    }

    while (i->announcements)
        remove_announcement(s, i->announcements);

    g_message("goodbye interface done: %s.%u", i->hardware->name, i->protocol);

}

void flx_goodbye_entry(flxServer *s, flxServerEntry *e, gboolean goodbye) {
    g_assert(s);
    g_assert(e);

    g_message("goodbye entry: %p", e);
    
    if (goodbye)
        flx_interface_monitor_walk(s->monitor, 0, AF_UNSPEC, send_goodbye_callback, e);

    while (e->announcements)
        remove_announcement(s, e->announcements);

    g_message("goodbye entry done: %p", e);

}

void flx_goodbye_all(flxServer *s, gboolean goodbye) {
    flxServerEntry *e;
    
    g_assert(s);

    g_message("goodbye all: %p", e);

    for (e = s->entries; e; e = e->entry_next)
        flx_goodbye_entry(s, e, goodbye);

    g_message("goodbye all done: %p", e);

}

