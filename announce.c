#include "announce.h"
#include "util.h"

#define FLX_ANNOUNCEMENT_JITTER_MSEC 250
#define FLX_PROBE_JITTER_MSEC 250
#define FLX_PROBE_INTERVAL_MSEC 250

static void remove_announcement(flxServer *s, flxAnnouncement *a) {
    g_assert(s);
    g_assert(a);

    if (a->time_event)
        flx_time_event_queue_remove(s->time_event_queue, a->time_event);

    FLX_LLIST_REMOVE(flxAnnouncement, by_interface, a->interface->announcements, a);
    FLX_LLIST_REMOVE(flxAnnouncement, by_entry, a->entry->announcements, a);
    
    g_free(a);
}

static void elapse_announce(flxTimeEvent *e, void *userdata);

static void set_timeout(flxAnnouncement *a, const GTimeVal *tv) {
    g_assert(a);

    if (!tv) {
        if (a->time_event) {
            flx_time_event_queue_remove(a->server->time_event_queue, a->time_event);
            a->time_event = NULL;
        }
    } else {

        if (a->time_event) 
            flx_time_event_queue_update(a->server->time_event_queue, a->time_event, tv);
        else
            a->time_event = flx_time_event_queue_add(a->server->time_event_queue, tv, elapse_announce, a);
    }
}

static void next_state(flxAnnouncement *a);

void flx_entry_group_check_probed(flxEntryGroup *g, gboolean immediately) {
    flxEntry *e;
    g_assert(g);
    g_assert(!g->dead);

    /* Check whether all group members have been probed */
    
    if (g->state != FLX_ENTRY_GROUP_REGISTERING || g->n_probing > 0) 
        return;

    flx_entry_group_change_state(g, FLX_ENTRY_GROUP_ESTABLISHED);

    if (g->dead)
        return;
    
    for (e = g->entries; e; e = e->entries_next) {
        flxAnnouncement *a;
        
        for (a = e->announcements; a; a = a->by_entry_next) {

            if (a->state != FLX_WAITING)
                continue;
            
            a->state = FLX_ANNOUNCING;

            if (immediately) {
                /* Shortcut */
                
                a->n_iteration = 1;
                next_state(a);
            } else {
                GTimeVal tv;
                a->n_iteration = 0;
                flx_elapse_time(&tv, 0, FLX_ANNOUNCEMENT_JITTER_MSEC);
                set_timeout(a, &tv);
            }
        }
    }
}

static void next_state(flxAnnouncement *a) {
    g_assert(a);

/*     g_message("%i -- %u", a->state, a->n_iteration);   */
    
    if (a->state == FLX_WAITING) {

        g_assert(a->entry->group);

        flx_entry_group_check_probed(a->entry->group, TRUE);
        
    } else if (a->state == FLX_PROBING) {

        if (a->n_iteration >= 4) {
            /* Probing done */
            
            gchar *t;

            g_message("Enough probes for record [%s]", t = flx_record_to_string(a->entry->record));
            g_free(t);

            if (a->entry->group) {
                g_assert(a->entry->group->n_probing);
                a->entry->group->n_probing--;
            }
            
            if (a->entry->group && a->entry->group->state == FLX_ENTRY_GROUP_REGISTERING)
                a->state = FLX_WAITING;
            else {
                a->state = FLX_ANNOUNCING;
                a->n_iteration = 1;
            }

            set_timeout(a, NULL);
            next_state(a);
        } else {
            GTimeVal tv;

            flx_interface_post_probe(a->interface, a->entry->record, FALSE);
            
            flx_elapse_time(&tv, FLX_PROBE_INTERVAL_MSEC, 0);
            set_timeout(a, &tv);
            
            a->n_iteration++;
        }

    } else if (a->state == FLX_ANNOUNCING) {

        flx_interface_post_response(a->interface, NULL, a->entry->record, a->entry->flags & FLX_ENTRY_UNIQUE, FALSE);

        if (++a->n_iteration >= 4) {
            gchar *t;
            /* Announcing done */

            g_message("Enough announcements for record [%s]", t = flx_record_to_string(a->entry->record));
            g_free(t);

            a->state = FLX_ESTABLISHED;

            set_timeout(a, NULL);
        } else {
            GTimeVal tv;
            flx_elapse_time(&tv, a->sec_delay*1000, FLX_ANNOUNCEMENT_JITTER_MSEC);
        
            if (a->n_iteration < 10)
                a->sec_delay *= 2;
            
            set_timeout(a, &tv);
        }
    }
}

static void elapse_announce(flxTimeEvent *e, void *userdata) {
    g_assert(e);

    next_state(userdata);
}

flxAnnouncement *flx_get_announcement(flxServer *s, flxEntry *e, flxInterface *i) {
    flxAnnouncement *a;
    
    g_assert(s);
    g_assert(e);
    g_assert(i);

    for (a = e->announcements; a; a = a->by_entry_next)
        if (a->interface == i)
            return a;

    return NULL;
}

static void new_announcement(flxServer *s, flxInterface *i, flxEntry *e) {
    flxAnnouncement *a;
    GTimeVal tv;
    gchar *t; 

    g_assert(s);
    g_assert(i);
    g_assert(e);
    g_assert(!e->dead);

/*     g_message("NEW ANNOUNCEMENT: %s.%i [%s]", i->hardware->name, i->protocol, t = flx_record_to_string(e->record)); */
/*     g_free(t); */
    
    if (!flx_interface_match(i, e->interface, e->protocol) || !i->announcing || !flx_entry_commited(e))
        return;

    /* We don't want duplicate announcements */
    if (flx_get_announcement(s, e, i))
        return;

    a = g_new(flxAnnouncement, 1);
    a->server = s;
    a->interface = i;
    a->entry = e;

    if ((e->flags & FLX_ENTRY_UNIQUE) && !(e->flags & FLX_ENTRY_NOPROBE))
        a->state = FLX_PROBING;
    else if (!(e->flags & FLX_ENTRY_NOANNOUNCE)) {

        if (!e->group || e->group->state == FLX_ENTRY_GROUP_ESTABLISHED)
            a->state = FLX_ANNOUNCING;
        else
            a->state = FLX_WAITING;
        
    } else
        a->state = FLX_ESTABLISHED;


    g_message("New announcement on interface %s.%i for entry [%s] state=%i", i->hardware->name, i->protocol, t = flx_record_to_string(e->record), a->state);
    g_free(t);

    a->n_iteration = 1;
    a->sec_delay = 1;
    a->time_event = NULL;

    if (a->state == FLX_PROBING)
        if (e->group)
            e->group->n_probing++;
    
    FLX_LLIST_PREPEND(flxAnnouncement, by_interface, i->announcements, a);
    FLX_LLIST_PREPEND(flxAnnouncement, by_entry, e->announcements, a);

    if (a->state == FLX_PROBING) {
        flx_elapse_time(&tv, 0, FLX_PROBE_JITTER_MSEC);
        set_timeout(a, &tv);
    } else if (a->state == FLX_ANNOUNCING) {
        flx_elapse_time(&tv, 0, FLX_ANNOUNCEMENT_JITTER_MSEC);
        set_timeout(a, &tv);
    }
}

void flx_announce_interface(flxServer *s, flxInterface *i) {
    flxEntry *e;
    
    g_assert(s);
    g_assert(i);

    if (!i->announcing)
        return;

    for (e = s->entries; e; e = e->entries_next)
        if (!e->dead)
            new_announcement(s, i, e);
}

static void announce_walk_callback(flxInterfaceMonitor *m, flxInterface *i, gpointer userdata) {
    flxEntry *e = userdata;
    
    g_assert(m);
    g_assert(i);
    g_assert(e);
    g_assert(!e->dead);

    new_announcement(m->server, i, e);
}

void flx_announce_entry(flxServer *s, flxEntry *e) {
    g_assert(s);
    g_assert(e);
    g_assert(!e->dead);

    flx_interface_monitor_walk(s->monitor, e->interface, e->protocol, announce_walk_callback, e);
}

void flx_announce_group(flxServer *s, flxEntryGroup *g) {
    flxEntry *e;
    
    g_assert(s);
    g_assert(g);

    for (e = g->entries; e; e = e->by_group_next)
        if (!e->dead)
            flx_announce_entry(s, e);
}

gboolean flx_entry_registered(flxServer *s, flxEntry *e, flxInterface *i) {
    flxAnnouncement *a;

    g_assert(s);
    g_assert(e);
    g_assert(i);
    g_assert(!e->dead);

    if (!(a = flx_get_announcement(s, e, i)))
        return FALSE;
    
    return a->state == FLX_ANNOUNCING || a->state == FLX_ESTABLISHED;
}

gboolean flx_entry_registering(flxServer *s, flxEntry *e, flxInterface *i) {
    flxAnnouncement *a;

    g_assert(s);
    g_assert(e);
    g_assert(i);
    g_assert(!e->dead);

    if (!(a = flx_get_announcement(s, e, i)))
        return FALSE;
    
    return a->state == FLX_PROBING || a->state == FLX_WAITING;
}

static flxRecord *make_goodbye_record(flxRecord *r) {
/*     gchar *t; */
    flxRecord *g;
    
    g_assert(r);

/*     g_message("Preparing goodbye for record [%s]", t = flx_record_to_string(r)); */
/*     g_free(t); */

    g = flx_record_copy(r);
    g_assert(g->ref == 1);
    g->ttl = 0;

    return g;
}

static void send_goodbye_callback(flxInterfaceMonitor *m, flxInterface *i, gpointer userdata) {
    flxEntry *e = userdata;
    flxRecord *g;
    
    g_assert(m);
    g_assert(i);
    g_assert(e);
    g_assert(!e->dead);

    if (!flx_interface_match(i, e->interface, e->protocol))
        return;

    if (e->flags & FLX_ENTRY_NOANNOUNCE)
        return;

    if (!flx_entry_registered(m->server, e, i))
        return;
    
    g = make_goodbye_record(e->record);
    flx_interface_post_response(i, NULL, g, e->flags & FLX_ENTRY_UNIQUE, TRUE);
    flx_record_unref(g);
}
    
void flx_goodbye_interface(flxServer *s, flxInterface *i, gboolean goodbye) {
    g_assert(s);
    g_assert(i);

/*     g_message("goodbye interface: %s.%u", i->hardware->name, i->protocol); */

    if (goodbye && flx_interface_relevant(i)) {
        flxEntry *e;
        
        for (e = s->entries; e; e = e->entries_next)
            if (!e->dead)
                send_goodbye_callback(s->monitor, i, e);
    }

    while (i->announcements)
        remove_announcement(s, i->announcements);

/*     g_message("goodbye interface done: %s.%u", i->hardware->name, i->protocol); */

}

void flx_goodbye_entry(flxServer *s, flxEntry *e, gboolean goodbye) {
    g_assert(s);
    g_assert(e);
    
/*     g_message("goodbye entry: %p", e); */
    
    if (goodbye && !e->dead)
        flx_interface_monitor_walk(s->monitor, 0, AF_UNSPEC, send_goodbye_callback, e);

    while (e->announcements)
        remove_announcement(s, e->announcements);

/*     g_message("goodbye entry done: %p", e); */

}

void flx_goodbye_all(flxServer *s, gboolean goodbye) {
    flxEntry *e;
    
    g_assert(s);

/*     g_message("goodbye all"); */

    for (e = s->entries; e; e = e->entries_next)
        if (!e->dead)
            flx_goodbye_entry(s, e, goodbye);

/*     g_message("goodbye all done"); */

}

