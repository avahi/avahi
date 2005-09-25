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

#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>

#include "announce.h"
#include "log.h"

#define AVAHI_ANNOUNCEMENT_JITTER_MSEC 250
#define AVAHI_PROBE_JITTER_MSEC 250
#define AVAHI_PROBE_INTERVAL_MSEC 250

static void remove_announcement(AvahiServer *s, AvahiAnnouncement *a) {
    assert(s);
    assert(a);

    if (a->time_event)
        avahi_time_event_free(a->time_event);

    AVAHI_LLIST_REMOVE(AvahiAnnouncement, by_interface, a->interface->announcements, a);
    AVAHI_LLIST_REMOVE(AvahiAnnouncement, by_entry, a->entry->announcements, a);
    
    avahi_free(a);
}

static void elapse_announce(AvahiTimeEvent *e, void *userdata);

static void set_timeout(AvahiAnnouncement *a, const struct timeval *tv) {
    assert(a);

    if (!tv) {
        if (a->time_event) {
            avahi_time_event_free(a->time_event);
            a->time_event = NULL;
        }
    } else {

        if (a->time_event) 
            avahi_time_event_update(a->time_event, tv);
        else
            a->time_event = avahi_time_event_new(a->server->time_event_queue, tv, elapse_announce, a);
    }
}

static void next_state(AvahiAnnouncement *a);

void avahi_s_entry_group_check_probed(AvahiSEntryGroup *g, int immediately) {
    AvahiEntry *e;
    assert(g);
    assert(!g->dead);

    /* Check whether all group members have been probed */
    
    if (g->state != AVAHI_ENTRY_GROUP_REGISTERING || g->n_probing > 0) 
        return;

    avahi_s_entry_group_change_state(g, AVAHI_ENTRY_GROUP_ESTABLISHED);

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
                struct timeval tv;
                a->n_iteration = 0;
                avahi_elapse_time(&tv, 0, AVAHI_ANNOUNCEMENT_JITTER_MSEC);
                set_timeout(a, &tv);
            }
        }
    }
}

static void next_state(AvahiAnnouncement *a) {
    assert(a);

/*     avahi_log_debug("%i -- %u", a->state, a->n_iteration);   */
    
    if (a->state == AVAHI_WAITING) {

        assert(a->entry->group);

        avahi_s_entry_group_check_probed(a->entry->group, 1);
        
    } else if (a->state == AVAHI_PROBING) {

        if (a->n_iteration >= 4) {
            /* Probing done */
            
/*             char *t; */

/*             avahi_log_debug("Enough probes for record [%s]", t = avahi_record_to_string(a->entry->record)); */
/*             avahi_free(t); */

            if (a->entry->group) {
                assert(a->entry->group->n_probing);
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
            struct timeval tv;

            avahi_interface_post_probe(a->interface, a->entry->record, 0);
            
            avahi_elapse_time(&tv, AVAHI_PROBE_INTERVAL_MSEC, 0);
            set_timeout(a, &tv);
            
            a->n_iteration++;
        }

    } else if (a->state == AVAHI_ANNOUNCING) {

        if (a->entry->flags & AVAHI_ENTRY_UNIQUE)
            /* Send the whole rrset at once */
            avahi_server_prepare_matching_responses(a->server, a->interface, a->entry->record->key, 0);
        else
            avahi_server_prepare_response(a->server, a->interface, a->entry, 0, 0);

        avahi_server_generate_response(a->server, a->interface, NULL, NULL, 0, 0, 0);

        if (++a->n_iteration >= 4) {
/*             char *t; */
            /* Announcing done */

/*             avahi_log_debug("Enough announcements for record [%s]", t = avahi_record_to_string(a->entry->record)); */
/*             avahi_free(t); */

            a->state = AVAHI_ESTABLISHED;

            set_timeout(a, NULL);
        } else {
            struct timeval tv;
            avahi_elapse_time(&tv, a->sec_delay*1000, AVAHI_ANNOUNCEMENT_JITTER_MSEC);
        
            if (a->n_iteration < 10)
                a->sec_delay *= 2;
            
            set_timeout(a, &tv);
        }
    }
}

static void elapse_announce(AvahiTimeEvent *e, void *userdata) {
    assert(e);

    next_state(userdata);
}

AvahiAnnouncement *avahi_get_announcement(AvahiServer *s, AvahiEntry *e, AvahiInterface *i) {
    AvahiAnnouncement *a;
    
    assert(s);
    assert(e);
    assert(i);

    for (a = e->announcements; a; a = a->by_entry_next)
        if (a->interface == i)
            return a;

    return NULL;
}

static void go_to_initial_state(AvahiAnnouncement *a, int immediately) {
    AvahiEntry *e;
    struct timeval tv;
        
    assert(a);
    e = a->entry;

    if ((e->flags & AVAHI_ENTRY_UNIQUE) && !(e->flags & AVAHI_ENTRY_NOPROBE))
        a->state = AVAHI_PROBING;
    else if (!(e->flags & AVAHI_ENTRY_NOANNOUNCE)) {

        if (!e->group || e->group->state == AVAHI_ENTRY_GROUP_ESTABLISHED)
            a->state = AVAHI_ANNOUNCING;
        else
            a->state = AVAHI_WAITING;
        
    } else
        a->state = AVAHI_ESTABLISHED;

    a->n_iteration = 1;
    a->sec_delay = 1;

    if (a->state == AVAHI_PROBING && e->group)
        e->group->n_probing++;

    if (a->state == AVAHI_PROBING) {
        avahi_elapse_time(&tv, 0, immediately ? 0 : AVAHI_PROBE_JITTER_MSEC);
        set_timeout(a, &tv);
    } else if (a->state == AVAHI_ANNOUNCING) {
        avahi_elapse_time(&tv, 0, immediately ? 0 : AVAHI_ANNOUNCEMENT_JITTER_MSEC);
        set_timeout(a, &tv);
    } else
        set_timeout(a, NULL);
}

static void new_announcement(AvahiServer *s, AvahiInterface *i, AvahiEntry *e) {
    AvahiAnnouncement *a;
/*     char *t;  */

    assert(s);
    assert(i);
    assert(e);
    assert(!e->dead);

/*     avahi_log_debug("NEW ANNOUNCEMENT: %s.%i [%s]", i->hardware->name, i->protocol, t = avahi_record_to_string(e->record)); */
/*     avahi_free(t); */
    
    if (!avahi_interface_match(i, e->interface, e->protocol) || !i->announcing || !avahi_entry_is_commited(e))
        return;

    /* We don't want duplicate announcements */
    if (avahi_get_announcement(s, e, i))
        return;    

    if ((!(a = avahi_new(AvahiAnnouncement, 1)))) {
        avahi_log_error(__FILE__": Out of memory.");
        return;
    }
    
    a->server = s;
    a->interface = i;
    a->entry = e;
    a->time_event = NULL;

    AVAHI_LLIST_PREPEND(AvahiAnnouncement, by_interface, i->announcements, a);
    AVAHI_LLIST_PREPEND(AvahiAnnouncement, by_entry, e->announcements, a);

    go_to_initial_state(a, 0);
    
/*     avahi_log_debug("New announcement on interface %s.%i for entry [%s] state=%i", i->hardware->name, i->protocol, t = avahi_record_to_string(e->record), a->state); */
/*     avahi_free(t); */
}

void avahi_announce_interface(AvahiServer *s, AvahiInterface *i) {
    AvahiEntry *e;
    
    assert(s);
    assert(i);

    if (!i->announcing)
        return;

    for (e = s->entries; e; e = e->entries_next)
        if (!e->dead)
            new_announcement(s, i, e);
}

static void announce_walk_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void* userdata) {
    AvahiEntry *e = userdata;
    
    assert(m);
    assert(i);
    assert(e);
    assert(!e->dead);

    new_announcement(m->server, i, e);
}

void avahi_announce_entry(AvahiServer *s, AvahiEntry *e) {
    assert(s);
    assert(e);
    assert(!e->dead);

    avahi_interface_monitor_walk(s->monitor, e->interface, e->protocol, announce_walk_callback, e);
}

void avahi_announce_group(AvahiServer *s, AvahiSEntryGroup *g) {
    AvahiEntry *e;
    
    assert(s);
    assert(g);

    for (e = g->entries; e; e = e->by_group_next)
        if (!e->dead)
            avahi_announce_entry(s, e);
}

int avahi_entry_is_registered(AvahiServer *s, AvahiEntry *e, AvahiInterface *i) {
    AvahiAnnouncement *a;

    assert(s);
    assert(e);
    assert(i);
    assert(!e->dead);

    if (!(a = avahi_get_announcement(s, e, i)))
        return 0;
    
    return
        a->state == AVAHI_ANNOUNCING ||
        a->state == AVAHI_ESTABLISHED ||
        (a->state == AVAHI_WAITING && !(e->flags & AVAHI_ENTRY_UNIQUE));
}

int avahi_entry_is_probing(AvahiServer *s, AvahiEntry *e, AvahiInterface *i) {
    AvahiAnnouncement *a;

    assert(s);
    assert(e);
    assert(i);
    assert(!e->dead);

    if (!(a = avahi_get_announcement(s, e, i)))
        return 0;

/*     avahi_log_debug("state: %i", a->state); */
    
    return
        a->state == AVAHI_PROBING ||
        (a->state == AVAHI_WAITING && (e->flags & AVAHI_ENTRY_UNIQUE));
}

void avahi_entry_return_to_initial_state(AvahiServer *s, AvahiEntry *e, AvahiInterface *i) {
    AvahiAnnouncement *a;
    
    assert(s);
    assert(e);
    assert(i);

    if (!(a = avahi_get_announcement(s, e, i)))
        return;

    if (a->state == AVAHI_PROBING && a->entry->group)
        a->entry->group->n_probing--;

    go_to_initial_state(a, 1);
}

static AvahiRecord *make_goodbye_record(AvahiRecord *r) {
/*     char *t; */
    AvahiRecord *g;
    
    assert(r);

/*     avahi_log_debug("Preparing goodbye for record [%s]", t = avahi_record_to_string(r)); */
/*     avahi_free(t); */

    if (!(g = avahi_record_copy(r)))
        return NULL; /* OOM */
    
    assert(g->ref == 1);
    g->ttl = 0;

    return g;
}

static void send_goodbye_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void* userdata) {
    AvahiEntry *e = userdata;
    AvahiRecord *g;
    
    assert(m);
    assert(i);
    assert(e);
    assert(!e->dead);

    if (!avahi_interface_match(i, e->interface, e->protocol))
        return;

    if (e->flags & AVAHI_ENTRY_NOANNOUNCE)
        return;

    if (!avahi_entry_is_registered(m->server, e, i))
        return;
    
    if (!(g = make_goodbye_record(e->record)))
        return; /* OOM */
    
    avahi_interface_post_response(i, g, e->flags & AVAHI_ENTRY_UNIQUE, NULL, 1);
    avahi_record_unref(g);
}
    
void avahi_goodbye_interface(AvahiServer *s, AvahiInterface *i, int goodbye) {
    assert(s);
    assert(i);

/*     avahi_log_debug("goodbye interface: %s.%u", i->hardware->name, i->protocol); */

    if (goodbye && avahi_interface_relevant(i)) {
        AvahiEntry *e;
        
        for (e = s->entries; e; e = e->entries_next)
            if (!e->dead)
                send_goodbye_callback(s->monitor, i, e);
    }

    while (i->announcements)
        remove_announcement(s, i->announcements);

/*     avahi_log_debug("goodbye interface done: %s.%u", i->hardware->name, i->protocol); */
}

void avahi_goodbye_entry(AvahiServer *s, AvahiEntry *e, int goodbye) {
    assert(s);
    assert(e);
    
/*     avahi_log_debug("goodbye entry: %p", e); */
    
    if (goodbye && !e->dead)
        avahi_interface_monitor_walk(s->monitor, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, send_goodbye_callback, e);

    while (e->announcements)
        remove_announcement(s, e->announcements);

/*     avahi_log_debug("goodbye entry done: %p", e); */

}

void avahi_goodbye_all(AvahiServer *s, int goodbye) {
    AvahiEntry *e;
    
    assert(s);

/*     avahi_log_debug("goodbye all"); */

    for (e = s->entries; e; e = e->entries_next)
        if (!e->dead)
            avahi_goodbye_entry(s, e, goodbye);

/*     avahi_log_debug("goodbye all done"); */

}

