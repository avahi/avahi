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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "announce.h"
#include "util.h"

#define AVAHI_ANNOUNCEMENT_JITTER_MSEC 250
#define AVAHI_PROBE_JITTER_MSEC 250
#define AVAHI_PROBE_INTERVAL_MSEC 250

static void remove_announcement(AvahiServer *s, AvahiAnnouncement *a) {
    g_assert(s);
    g_assert(a);

    if (a->time_event)
        avahi_time_event_queue_remove(s->time_event_queue, a->time_event);

    AVAHI_LLIST_REMOVE(AvahiAnnouncement, by_interface, a->interface->announcements, a);
    AVAHI_LLIST_REMOVE(AvahiAnnouncement, by_entry, a->entry->announcements, a);
    
    g_free(a);
}

static void elapse_announce(AvahiTimeEvent *e, void *userdata);

static void set_timeout(AvahiAnnouncement *a, const GTimeVal *tv) {
    g_assert(a);

    if (!tv) {
        if (a->time_event) {
            avahi_time_event_queue_remove(a->server->time_event_queue, a->time_event);
            a->time_event = NULL;
        }
    } else {

        if (a->time_event) 
            avahi_time_event_queue_update(a->server->time_event_queue, a->time_event, tv);
        else
            a->time_event = avahi_time_event_queue_add(a->server->time_event_queue, tv, elapse_announce, a);
    }
}

static void next_state(AvahiAnnouncement *a);

void avahi_entry_group_check_probed(AvahiEntryGroup *g, gboolean immediately) {
    AvahiEntry *e;
    g_assert(g);
    g_assert(!g->dead);

    /* Check whether all group members have been probed */
    
    if (g->state != AVAHI_ENTRY_GROUP_REGISTERING || g->n_probing > 0) 
        return;

    avahi_entry_group_change_state(g, AVAHI_ENTRY_GROUP_ESTABLISHED);

    if (g->dead)
        return;
    
    for (e = g->entries; e; e = e->entries_next) {
        AvahiAnnouncement *a;
        
        for (a = e->announcements; a; a = a->by_entry_next) {

            if (a->state != AVAHI_WAITING)
                continue;
            
            a->state = AVAHI_ANNOUNCING;

            if (immediately) {
                /* Shortcut */
                
                a->n_iteration = 1;
                next_state(a);
            } else {
                GTimeVal tv;
                a->n_iteration = 0;
                avahi_elapse_time(&tv, 0, AVAHI_ANNOUNCEMENT_JITTER_MSEC);
                set_timeout(a, &tv);
            }
        }
    }
}

static void next_state(AvahiAnnouncement *a) {
    g_assert(a);

/*     g_message("%i -- %u", a->state, a->n_iteration);   */
    
    if (a->state == AVAHI_WAITING) {

        g_assert(a->entry->group);

        avahi_entry_group_check_probed(a->entry->group, TRUE);
        
    } else if (a->state == AVAHI_PROBING) {

        if (a->n_iteration >= 4) {
            /* Probing done */
            
            gchar *t;

            g_message("Enough probes for record [%s]", t = avahi_record_to_string(a->entry->record));
            g_free(t);

            if (a->entry->group) {
                g_assert(a->entry->group->n_probing);
                a->entry->group->n_probing--;
            }
            
            if (a->entry->group && a->entry->group->state == AVAHI_ENTRY_GROUP_REGISTERING)
                a->state = AVAHI_WAITING;
            else {
                a->state = AVAHI_ANNOUNCING;
                a->n_iteration = 1;
            }

            set_timeout(a, NULL);
            next_state(a);
        } else {
            GTimeVal tv;

            avahi_interface_post_probe(a->interface, a->entry->record, FALSE);
            
            avahi_elapse_time(&tv, AVAHI_PROBE_INTERVAL_MSEC, 0);
            set_timeout(a, &tv);
            
            a->n_iteration++;
        }

    } else if (a->state == AVAHI_ANNOUNCING) {

        avahi_interface_post_response(a->interface, NULL, a->entry->record, a->entry->flags & AVAHI_ENTRY_UNIQUE, FALSE);

        if (++a->n_iteration >= 4) {
            gchar *t;
            /* Announcing done */

            g_message("Enough announcements for record [%s]", t = avahi_record_to_string(a->entry->record));
            g_free(t);

            a->state = AVAHI_ESTABLISHED;

            set_timeout(a, NULL);
        } else {
            GTimeVal tv;
            avahi_elapse_time(&tv, a->sec_delay*1000, AVAHI_ANNOUNCEMENT_JITTER_MSEC);
        
            if (a->n_iteration < 10)
                a->sec_delay *= 2;
            
            set_timeout(a, &tv);
        }
    }
}

static void elapse_announce(AvahiTimeEvent *e, void *userdata) {
    g_assert(e);

    next_state(userdata);
}

AvahiAnnouncement *avahi_get_announcement(AvahiServer *s, AvahiEntry *e, AvahiInterface *i) {
    AvahiAnnouncement *a;
    
    g_assert(s);
    g_assert(e);
    g_assert(i);

    for (a = e->announcements; a; a = a->by_entry_next)
        if (a->interface == i)
            return a;

    return NULL;
}

static void new_announcement(AvahiServer *s, AvahiInterface *i, AvahiEntry *e) {
    AvahiAnnouncement *a;
    GTimeVal tv;
    gchar *t; 

    g_assert(s);
    g_assert(i);
    g_assert(e);
    g_assert(!e->dead);

/*     g_message("NEW ANNOUNCEMENT: %s.%i [%s]", i->hardware->name, i->protocol, t = avahi_record_to_string(e->record)); */
/*     g_free(t); */
    
    if (!avahi_interface_match(i, e->interface, e->protocol) || !i->announcing || !avahi_entry_commited(e))
        return;

    /* We don't want duplicate announcements */
    if (avahi_get_announcement(s, e, i))
        return;

    a = g_new(AvahiAnnouncement, 1);
    a->server = s;
    a->interface = i;
    a->entry = e;

    if ((e->flags & AVAHI_ENTRY_UNIQUE) && !(e->flags & AVAHI_ENTRY_NOPROBE))
        a->state = AVAHI_PROBING;
    else if (!(e->flags & AVAHI_ENTRY_NOANNOUNCE)) {

        if (!e->group || e->group->state == AVAHI_ENTRY_GROUP_ESTABLISHED)
            a->state = AVAHI_ANNOUNCING;
        else
            a->state = AVAHI_WAITING;
        
    } else
        a->state = AVAHI_ESTABLISHED;


    g_message("New announcement on interface %s.%i for entry [%s] state=%i", i->hardware->name, i->protocol, t = avahi_record_to_string(e->record), a->state);
    g_free(t);

    a->n_iteration = 1;
    a->sec_delay = 1;
    a->time_event = NULL;

    if (a->state == AVAHI_PROBING)
        if (e->group)
            e->group->n_probing++;
    
    AVAHI_LLIST_PREPEND(AvahiAnnouncement, by_interface, i->announcements, a);
    AVAHI_LLIST_PREPEND(AvahiAnnouncement, by_entry, e->announcements, a);

    if (a->state == AVAHI_PROBING) {
        avahi_elapse_time(&tv, 0, AVAHI_PROBE_JITTER_MSEC);
        set_timeout(a, &tv);
    } else if (a->state == AVAHI_ANNOUNCING) {
        avahi_elapse_time(&tv, 0, AVAHI_ANNOUNCEMENT_JITTER_MSEC);
        set_timeout(a, &tv);
    }
}

void avahi_announce_interface(AvahiServer *s, AvahiInterface *i) {
    AvahiEntry *e;
    
    g_assert(s);
    g_assert(i);

    if (!i->announcing)
        return;

    for (e = s->entries; e; e = e->entries_next)
        if (!e->dead)
            new_announcement(s, i, e);
}

static void announce_walk_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, gpointer userdata) {
    AvahiEntry *e = userdata;
    
    g_assert(m);
    g_assert(i);
    g_assert(e);
    g_assert(!e->dead);

    new_announcement(m->server, i, e);
}

void avahi_announce_entry(AvahiServer *s, AvahiEntry *e) {
    g_assert(s);
    g_assert(e);
    g_assert(!e->dead);

    avahi_interface_monitor_walk(s->monitor, e->interface, e->protocol, announce_walk_callback, e);
}

void avahi_announce_group(AvahiServer *s, AvahiEntryGroup *g) {
    AvahiEntry *e;
    
    g_assert(s);
    g_assert(g);

    for (e = g->entries; e; e = e->by_group_next)
        if (!e->dead)
            avahi_announce_entry(s, e);
}

gboolean avahi_entry_registered(AvahiServer *s, AvahiEntry *e, AvahiInterface *i) {
    AvahiAnnouncement *a;

    g_assert(s);
    g_assert(e);
    g_assert(i);
    g_assert(!e->dead);

    if (!(a = avahi_get_announcement(s, e, i)))
        return FALSE;
    
    return a->state == AVAHI_ANNOUNCING || a->state == AVAHI_ESTABLISHED;
}

gboolean avahi_entry_registering(AvahiServer *s, AvahiEntry *e, AvahiInterface *i) {
    AvahiAnnouncement *a;

    g_assert(s);
    g_assert(e);
    g_assert(i);
    g_assert(!e->dead);

    if (!(a = avahi_get_announcement(s, e, i)))
        return FALSE;
    
    return a->state == AVAHI_PROBING || a->state == AVAHI_WAITING;
}

static AvahiRecord *make_goodbye_record(AvahiRecord *r) {
/*     gchar *t; */
    AvahiRecord *g;
    
    g_assert(r);

/*     g_message("Preparing goodbye for record [%s]", t = avahi_record_to_string(r)); */
/*     g_free(t); */

    g = avahi_record_copy(r);
    g_assert(g->ref == 1);
    g->ttl = 0;

    return g;
}

static void send_goodbye_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, gpointer userdata) {
    AvahiEntry *e = userdata;
    AvahiRecord *g;
    
    g_assert(m);
    g_assert(i);
    g_assert(e);
    g_assert(!e->dead);

    if (!avahi_interface_match(i, e->interface, e->protocol))
        return;

    if (e->flags & AVAHI_ENTRY_NOANNOUNCE)
        return;

    if (!avahi_entry_registered(m->server, e, i))
        return;
    
    g = make_goodbye_record(e->record);
    avahi_interface_post_response(i, NULL, g, e->flags & AVAHI_ENTRY_UNIQUE, TRUE);
    avahi_record_unref(g);
}
    
void avahi_goodbye_interface(AvahiServer *s, AvahiInterface *i, gboolean goodbye) {
    g_assert(s);
    g_assert(i);

/*     g_message("goodbye interface: %s.%u", i->hardware->name, i->protocol); */

    if (goodbye && avahi_interface_relevant(i)) {
        AvahiEntry *e;
        
        for (e = s->entries; e; e = e->entries_next)
            if (!e->dead)
                send_goodbye_callback(s->monitor, i, e);
    }

    while (i->announcements)
        remove_announcement(s, i->announcements);

/*     g_message("goodbye interface done: %s.%u", i->hardware->name, i->protocol); */

}

void avahi_goodbye_entry(AvahiServer *s, AvahiEntry *e, gboolean goodbye) {
    g_assert(s);
    g_assert(e);
    
/*     g_message("goodbye entry: %p", e); */
    
    if (goodbye && !e->dead)
        avahi_interface_monitor_walk(s->monitor, 0, AF_UNSPEC, send_goodbye_callback, e);

    while (e->announcements)
        remove_announcement(s, e->announcements);

/*     g_message("goodbye entry done: %p", e); */

}

void avahi_goodbye_all(AvahiServer *s, gboolean goodbye) {
    AvahiEntry *e;
    
    g_assert(s);

/*     g_message("goodbye all"); */

    for (e = s->entries; e; e = e->entries_next)
        if (!e->dead)
            avahi_goodbye_entry(s, e, goodbye);

/*     g_message("goodbye all done"); */

}

