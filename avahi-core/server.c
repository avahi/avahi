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

#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <errno.h>

#include "server.h"
#include "util.h"
#include "iface.h"
#include "socket.h"
#include "browse.h"
#include "log.h"

#define AVAHI_HOST_RR_HOLDOFF_MSEC 2000

static void free_entry(AvahiServer*s, AvahiEntry *e) {
    AvahiEntry *t;

    g_assert(s);
    g_assert(e);

    avahi_goodbye_entry(s, e, TRUE);

    /* Remove from linked list */
    AVAHI_LLIST_REMOVE(AvahiEntry, entries, s->entries, e);

    /* Remove from hash table indexed by name */
    t = g_hash_table_lookup(s->entries_by_key, e->record->key);
    AVAHI_LLIST_REMOVE(AvahiEntry, by_key, t, e);
    if (t)
        g_hash_table_replace(s->entries_by_key, t->record->key, t);
    else
        g_hash_table_remove(s->entries_by_key, e->record->key);

    /* Remove from associated group */
    if (e->group)
        AVAHI_LLIST_REMOVE(AvahiEntry, by_group, e->group->entries, e);

    avahi_record_unref(e->record);
    g_free(e);
}

static void free_group(AvahiServer *s, AvahiEntryGroup *g) {
    g_assert(s);
    g_assert(g);

    while (g->entries)
        free_entry(s, g->entries);

    AVAHI_LLIST_REMOVE(AvahiEntryGroup, groups, s->groups, g);
    g_free(g);
}

static void cleanup_dead(AvahiServer *s) {
    AvahiEntryGroup *g, *ng;
    AvahiEntry *e, *ne;
    g_assert(s);


    if (s->need_group_cleanup) {
        for (g = s->groups; g; g = ng) {
            ng = g->groups_next;
            
            if (g->dead)
                free_group(s, g);
        }

        s->need_group_cleanup = FALSE;
    }

    if (s->need_entry_cleanup) {
        for (e = s->entries; e; e = ne) {
            ne = e->entries_next;
            
            if (e->dead)
                free_entry(s, e);
        }

        s->need_entry_cleanup = FALSE;
    }

    if (s->need_browser_cleanup)
        avahi_browser_cleanup(s);
}

static void enum_aux_records(AvahiServer *s, AvahiInterface *i, const gchar *name, guint16 type, void (*callback)(AvahiServer *s, AvahiRecord *r, gboolean flush_cache, gpointer userdata), gpointer userdata) {
    AvahiKey *k;
    AvahiEntry *e;

    g_assert(s);
    g_assert(i);
    g_assert(name);
    g_assert(callback);

    g_assert(type != AVAHI_DNS_TYPE_ANY);

    k = avahi_key_new(name, AVAHI_DNS_CLASS_IN, type);

    for (e = g_hash_table_lookup(s->entries_by_key, k); e; e = e->by_key_next)
        if (!e->dead && avahi_entry_registered(s, e, i)) 
            callback(s, e->record, e->flags & AVAHI_ENTRY_UNIQUE, userdata);

    avahi_key_unref(k);
}

void avahi_server_enumerate_aux_records(AvahiServer *s, AvahiInterface *i, AvahiRecord *r, void (*callback)(AvahiServer *s, AvahiRecord *r, gboolean flush_cache, gpointer userdata), gpointer userdata) {
    g_assert(s);
    g_assert(i);
    g_assert(r);
    g_assert(callback);
    
    if (r->key->class == AVAHI_DNS_CLASS_IN) {
        if (r->key->type == AVAHI_DNS_TYPE_PTR) {
            enum_aux_records(s, i, r->data.ptr.name, AVAHI_DNS_TYPE_SRV, callback, userdata);
            enum_aux_records(s, i, r->data.ptr.name, AVAHI_DNS_TYPE_TXT, callback, userdata);
        } else if (r->key->type == AVAHI_DNS_TYPE_SRV) {
            enum_aux_records(s, i, r->data.srv.name, AVAHI_DNS_TYPE_A, callback, userdata);
            enum_aux_records(s, i, r->data.srv.name, AVAHI_DNS_TYPE_AAAA, callback, userdata);
        }
    }
}

void avahi_server_prepare_response(AvahiServer *s, AvahiInterface *i, AvahiEntry *e, gboolean unicast_response, gboolean auxiliary) {
    g_assert(s);
    g_assert(i);
    g_assert(e);

    avahi_record_list_push(s->record_list, e->record, e->flags & AVAHI_ENTRY_UNIQUE, unicast_response, auxiliary);
}

void avahi_server_prepare_matching_responses(AvahiServer *s, AvahiInterface *i, AvahiKey *k, gboolean unicast_response) {
    AvahiEntry *e;
/*     gchar *txt; */
    
    g_assert(s);
    g_assert(i);
    g_assert(k);

/*     avahi_log_debug("Posting responses matching [%s]", txt = avahi_key_to_string(k)); */
/*     g_free(txt); */

    if (avahi_key_is_pattern(k)) {

        /* Handle ANY query */
        
        for (e = s->entries; e; e = e->entries_next)
            if (!e->dead && avahi_key_pattern_match(k, e->record->key) && avahi_entry_registered(s, e, i))
                avahi_server_prepare_response(s, i, e, unicast_response, FALSE);

    } else {

        /* Handle all other queries */
        
        for (e = g_hash_table_lookup(s->entries_by_key, k); e; e = e->by_key_next)
            if (!e->dead && avahi_entry_registered(s, e, i))
                avahi_server_prepare_response(s, i, e, unicast_response, FALSE);
    }
}

static void withdraw_entry(AvahiServer *s, AvahiEntry *e) {
    g_assert(s);
    g_assert(e);
    
    if (e->group) {
        AvahiEntry *k;
        
        for (k = e->group->entries; k; k = k->by_group_next) {
            avahi_goodbye_entry(s, k, FALSE);
            k->dead = TRUE;
        }
        
        avahi_entry_group_change_state(e->group, AVAHI_ENTRY_GROUP_COLLISION);
    } else {
        avahi_goodbye_entry(s, e, FALSE);
        e->dead = TRUE;
    }

    s->need_entry_cleanup = TRUE;
}

static void withdraw_rrset(AvahiServer *s, AvahiKey *key) {
    AvahiEntry *e;
    
    g_assert(s);
    g_assert(key);

   for (e = g_hash_table_lookup(s->entries_by_key, key); e; e = e->by_key_next)
        withdraw_entry(s, e);
}

static void incoming_probe(AvahiServer *s, AvahiRecord *record, AvahiInterface *i) {
    AvahiEntry *e, *n;
    gchar *t;
    gboolean ours = FALSE, won = FALSE, lost = FALSE;
    
    g_assert(s);
    g_assert(record);
    g_assert(i);

    for (e = g_hash_table_lookup(s->entries_by_key, record->key); e; e = n) {
        gint cmp;
        n = e->by_key_next;

        if (e->dead)
            continue;
        
        if ((cmp = avahi_record_lexicographical_compare(e->record, record)) == 0) {
            ours = TRUE;
            break;
        } else {
            
            if (avahi_entry_probing(s, e, i)) {
                if (cmp > 0)
                    won = TRUE;
                else /* cmp < 0 */
                    lost = TRUE;
            }
        }
    }

    t = avahi_record_to_string(record);

    if (!ours) {

        if (won)
            avahi_log_debug("xxx Recieved conflicting probe [%s]. Local host won.", t);
        else if (lost) {
            avahi_log_debug("yyy Recieved conflicting probe [%s]. Local host lost. Withdrawing.", t);
            withdraw_rrset(s, record->key);
        }
    }

    g_free(t);
}

static gboolean handle_conflict(AvahiServer *s, AvahiInterface *i, AvahiRecord *record, gboolean unique, const AvahiAddress *a) {
    gboolean valid = TRUE, ours = FALSE, conflict = FALSE, withdraw_immediately = FALSE;
    AvahiEntry *e, *n, *conflicting_entry = NULL;
    
    g_assert(s);
    g_assert(i);
    g_assert(record);


/*     avahi_log_debug("CHECKING FOR CONFLICT: [%s]", t);   */

    for (e = g_hash_table_lookup(s->entries_by_key, record->key); e; e = n) {
        n = e->by_key_next;

        if (e->dead || (!(e->flags & AVAHI_ENTRY_UNIQUE) && !unique))
            continue;

        /* Either our entry or the other is intended to be unique, so let's check */
        
        if (avahi_record_equal_no_ttl(e->record, record)) {
            ours = TRUE; /* We have an identical record, so this is no conflict */
            
            /* Check wheter there is a TTL conflict */
            if (record->ttl <= e->record->ttl/2 &&
                avahi_entry_registered(s, e, i)) {
                gchar *t;
                /* Refresh */
                t = avahi_record_to_string(record); 
                
                avahi_log_debug("Recieved record with bad TTL [%s]. Refreshing.", t);
                avahi_server_prepare_matching_responses(s, i, e->record->key, FALSE);
                valid = FALSE;
                
                g_free(t);
            }
                
            /* There's no need to check the other entries of this RRset */
            break;

        } else {
            
            if (avahi_entry_registered(s, e, i)) {
                
                /* A conflict => we have to return to probe mode */
                conflict = TRUE;
                conflicting_entry = e;

            } else if (avahi_entry_probing(s, e, i)) {

                /* We are currently registering a matching record, but
                 * someone else already claimed it, so let's
                 * withdraw */
                conflict = TRUE;
                withdraw_immediately = TRUE;
            }
        }
    }

/*     avahi_log_debug("ours=%i conflict=%i", ours, conflict); */

    if (!ours && conflict) {
        gchar *t;
 
        valid = FALSE;

        t = avahi_record_to_string(record); 
 
        if (withdraw_immediately) {
            avahi_log_debug("Recieved conflicting record [%s] with local record to be. Withdrawing.", t);
            withdraw_rrset(s, record->key);
        } else {
            g_assert(conflicting_entry);
            avahi_log_debug("Recieved conflicting record [%s]. Resetting our record.", t);
            avahi_entry_return_to_initial_state(s, conflicting_entry, i);

            /* Local unique records are returned to probin
             * state. Local shared records are reannounced. */
        }

        g_free(t);
    }

    return valid;
}

static void append_aux_callback(AvahiServer *s, AvahiRecord *r, gboolean flush_cache, gpointer userdata) {
    gboolean *unicast_response = userdata;

    g_assert(s);
    g_assert(r);
    g_assert(unicast_response);
    
    avahi_record_list_push(s->record_list, r, flush_cache, *unicast_response, TRUE);
}

static void append_aux_records_to_list(AvahiServer *s, AvahiInterface *i, AvahiRecord *r, gboolean unicast_response) {
    g_assert(s);
    g_assert(r);

    avahi_server_enumerate_aux_records(s, i, r, append_aux_callback, &unicast_response);
}

void avahi_server_generate_response(AvahiServer *s, AvahiInterface *i, AvahiDnsPacket *p, const AvahiAddress *a, guint16 port, gboolean legacy_unicast) {

    g_assert(s);
    g_assert(i);
    g_assert(!legacy_unicast || (a && port > 0 && p));

    if (legacy_unicast) {
        AvahiDnsPacket *reply;
        AvahiRecord *r;

        reply = avahi_dns_packet_new_reply(p, 512 /* unicast DNS maximum packet size is 512 */ , TRUE, TRUE);
        
        while ((r = avahi_record_list_next(s->record_list, NULL, NULL, NULL))) {

            append_aux_records_to_list(s, i, r, FALSE);
            
            if (avahi_dns_packet_append_record(reply, r, FALSE, 10))
                avahi_dns_packet_inc_field(reply, AVAHI_DNS_FIELD_ANCOUNT);
            else {
                gchar *t = avahi_record_to_string(r);
                avahi_log_warn("Record [%s] not fitting in legacy unicast packet, dropping.", t);
                g_free(t);
            }

            avahi_record_unref(r);
        }

        if (avahi_dns_packet_get_field(reply, AVAHI_DNS_FIELD_ANCOUNT) != 0)
            avahi_interface_send_packet_unicast(i, reply, a, port);

        avahi_dns_packet_free(reply);

    } else {
        gboolean unicast_response, flush_cache, auxiliary;
        AvahiDnsPacket *reply = NULL;
        AvahiRecord *r;

        /* In case the query packet was truncated never respond
        immediately, because known answer suppression records might be
        contained in later packets */
        gboolean tc = p && !!(avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_FLAGS) & AVAHI_DNS_FLAG_TC);
        
        while ((r = avahi_record_list_next(s->record_list, &flush_cache, &unicast_response, &auxiliary))) {
                        
            if (!avahi_interface_post_response(i, r, flush_cache, a, !tc && flush_cache && !auxiliary) && unicast_response) {

                append_aux_records_to_list(s, i, r, unicast_response);
                
                /* Due to some reasons the record has not been scheduled.
                 * The client requested an unicast response in that
                 * case. Therefore we prepare such a response */

                for (;;) {
                
                    if (!reply) {
                        g_assert(p);
                        reply = avahi_dns_packet_new_reply(p, i->hardware->mtu, FALSE, FALSE);
                    }
                
                    if (avahi_dns_packet_append_record(reply, r, flush_cache, 0)) {

                        /* Appending this record succeeded, so incremeant
                         * the specific header field, and return to the caller */
                        
                        avahi_dns_packet_inc_field(reply, AVAHI_DNS_FIELD_ANCOUNT);

                        break;
                    }

                    if (avahi_dns_packet_get_field(reply, AVAHI_DNS_FIELD_ANCOUNT) == 0) {
                        guint size;

                        /* The record is too large for one packet, so create a larger packet */

                        avahi_dns_packet_free(reply);
                        size = avahi_record_get_estimate_size(r) + AVAHI_DNS_PACKET_HEADER_SIZE;
                        if (size > AVAHI_DNS_PACKET_MAX_SIZE)
                            size = AVAHI_DNS_PACKET_MAX_SIZE;
                        reply = avahi_dns_packet_new_reply(p, size, FALSE, TRUE);

                        if (!avahi_dns_packet_append_record(reply, r, flush_cache, 0)) {
                            avahi_dns_packet_free(reply);
                            
                            gchar *t = avahi_record_to_string(r);
                            avahi_log_warn("Record [%s] too large, doesn't fit in any packet!", t);
                            g_free(t);
                            break;
                        } else
                            avahi_dns_packet_inc_field(reply, AVAHI_DNS_FIELD_ANCOUNT);
                    }

                    /* Appending the record didn't succeeed, so let's send this packet, and create a new one */
                    avahi_interface_send_packet_unicast(i, reply, a, port);
                    avahi_dns_packet_free(reply);
                    reply = NULL;
                }
            }

            avahi_record_unref(r);
        }

        if (reply) {
            if (avahi_dns_packet_get_field(reply, AVAHI_DNS_FIELD_ANCOUNT) != 0) 
                avahi_interface_send_packet_unicast(i, reply, a, port);
            avahi_dns_packet_free(reply);
        }
    }

    avahi_record_list_flush(s->record_list);
}


static void reflect_response(AvahiServer *s, AvahiInterface *i, AvahiRecord *r, gboolean flush_cache) {
    AvahiInterface *j;
    
    g_assert(s);
    g_assert(i);
    g_assert(r);

    if (!s->config.enable_reflector)
        return;

    for (j = s->monitor->interfaces; j; j = j->interface_next)
        if (j != i && (s->config.ipv_reflect || j->protocol == i->protocol))
            avahi_interface_post_response(j, r, flush_cache, NULL, TRUE);
}

static gpointer reflect_cache_walk_callback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, gpointer userdata) {
    AvahiServer *s = userdata;

    g_assert(c);
    g_assert(pattern);
    g_assert(e);
    g_assert(s);

    avahi_record_list_push(s->record_list, e->record, e->cache_flush, FALSE, FALSE);
    return NULL;
}

static void reflect_query(AvahiServer *s, AvahiInterface *i, AvahiKey *k) {
    AvahiInterface *j;
    
    g_assert(s);
    g_assert(i);
    g_assert(k);

    if (!s->config.enable_reflector)
        return;

    for (j = s->monitor->interfaces; j; j = j->interface_next)
        if (j != i && (s->config.ipv_reflect || j->protocol == i->protocol)) {
            /* Post the query to other networks */
            avahi_interface_post_query(j, k, TRUE);

            /* Reply from caches of other network. This is needed to
             * "work around" known answer suppression. */

            avahi_cache_walk(j->cache, k, reflect_cache_walk_callback, s);
        }
}

static void reflect_probe(AvahiServer *s, AvahiInterface *i, AvahiRecord *r) {
    AvahiInterface *j;
    
    g_assert(s);
    g_assert(i);
    g_assert(r);

    if (!s->config.enable_reflector)
        return;

    for (j = s->monitor->interfaces; j; j = j->interface_next)
        if (j != i && (s->config.ipv_reflect || j->protocol == i->protocol))
            avahi_interface_post_probe(j, r, TRUE);
}

static void handle_query_packet(AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *a, guint16 port, gboolean legacy_unicast) {
    guint n;
    
    g_assert(s);
    g_assert(p);
    g_assert(i);
    g_assert(a);

/*     avahi_log_debug("query"); */

    g_assert(avahi_record_list_empty(s->record_list));
    
    /* Handle the questions */
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT); n > 0; n --) {
        AvahiKey *key;
        gboolean unicast_response = FALSE;

        if (!(key = avahi_dns_packet_consume_key(p, &unicast_response))) {
            avahi_log_warn("Packet too short (1)");
            goto fail;
        }

        if (!legacy_unicast)
            reflect_query(s, i, key);
        avahi_query_scheduler_incoming(i->query_scheduler, key);
        avahi_server_prepare_matching_responses(s, i, key, unicast_response);
        avahi_key_unref(key);
    }

    /* Known Answer Suppression */
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT); n > 0; n --) {
        AvahiRecord *record;
        gboolean unique = FALSE;

        if (!(record = avahi_dns_packet_consume_record(p, &unique))) {
            avahi_log_warn("Packet too short (2)");
            goto fail;
        }

        if (handle_conflict(s, i, record, unique, a)) {
            avahi_response_scheduler_suppress(i->response_scheduler, record, a);
            avahi_record_list_drop(s->record_list, record);
        }
        
        avahi_record_unref(record);
    }

    /* Probe record */
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT); n > 0; n --) {
        AvahiRecord *record;
        gboolean unique = FALSE;

        if (!(record = avahi_dns_packet_consume_record(p, &unique))) {
            avahi_log_warn("Packet too short (3)");
            goto fail;
        }

        if (record->key->type != AVAHI_DNS_TYPE_ANY) {
            reflect_probe(s, i, record);
            incoming_probe(s, record, i);
        }
        
        avahi_record_unref(record);
    }

    if (!avahi_record_list_empty(s->record_list))
        avahi_server_generate_response(s, i, p, a, port, legacy_unicast);

    return;
    
fail:
    avahi_record_list_flush(s->record_list);

}

static void handle_response_packet(AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *a) {
    guint n;
    
    g_assert(s);
    g_assert(p);
    g_assert(i);
    g_assert(a);

/*     avahi_log_debug("response"); */
    
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) +
             avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ARCOUNT); n > 0; n--) {
        AvahiRecord *record;
        gboolean cache_flush = FALSE;
/*         gchar *txt; */
        
        if (!(record = avahi_dns_packet_consume_record(p, &cache_flush))) {
            avahi_log_warn("Packet too short (4)");
            break;
        }

        if (record->key->type != AVAHI_DNS_TYPE_ANY) {

/*             avahi_log_debug("Handling response: %s", txt = avahi_record_to_string(record)); */
/*             g_free(txt); */
            
            if (handle_conflict(s, i, record, cache_flush, a)) {
                reflect_response(s, i, record, cache_flush);
                avahi_cache_update(i->cache, record, cache_flush, a);
                avahi_response_scheduler_incoming(i->response_scheduler, record, cache_flush);
            }
        }
            
        avahi_record_unref(record);
    }
}

static AvahiLegacyUnicastReflectSlot* allocate_slot(AvahiServer *s) {
    guint n, index = (guint) -1;
    AvahiLegacyUnicastReflectSlot *slot;
    
    g_assert(s);

    if (!s->legacy_unicast_reflect_slots)
        s->legacy_unicast_reflect_slots = g_new0(AvahiLegacyUnicastReflectSlot*, AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS);

    for (n = 0; n < AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS; n++, s->legacy_unicast_reflect_id++) {
        index = s->legacy_unicast_reflect_id % AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS;
        
        if (!s->legacy_unicast_reflect_slots[index])
            break;
    }

    if (index == (guint) -1 || s->legacy_unicast_reflect_slots[index])
        return NULL;

    slot = s->legacy_unicast_reflect_slots[index] = g_new(AvahiLegacyUnicastReflectSlot, 1);
    slot->id = s->legacy_unicast_reflect_id++;
    slot->server = s;
    return slot;
}

static void deallocate_slot(AvahiServer *s, AvahiLegacyUnicastReflectSlot *slot) {
    guint index;

    g_assert(s);
    g_assert(slot);

    index = slot->id % AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS;

    g_assert(s->legacy_unicast_reflect_slots[index] == slot);

    avahi_time_event_queue_remove(s->time_event_queue, slot->time_event);
    
    g_free(slot);
    s->legacy_unicast_reflect_slots[index] = NULL;
}

static void free_slots(AvahiServer *s) {
    guint index;
    g_assert(s);

    if (!s->legacy_unicast_reflect_slots)
        return;

    for (index = 0; index < AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS; index ++)
        if (s->legacy_unicast_reflect_slots[index])
            deallocate_slot(s, s->legacy_unicast_reflect_slots[index]);

    g_free(s->legacy_unicast_reflect_slots);
    s->legacy_unicast_reflect_slots = NULL;
}

static AvahiLegacyUnicastReflectSlot* find_slot(AvahiServer *s, guint16 id) {
    guint index;
    
    g_assert(s);

    if (!s->legacy_unicast_reflect_slots)
        return NULL;
    
    index = id % AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS;

    if (!s->legacy_unicast_reflect_slots[index] || s->legacy_unicast_reflect_slots[index]->id != id)
        return NULL;

    return s->legacy_unicast_reflect_slots[index];
}

static void legacy_unicast_reflect_slot_timeout(AvahiTimeEvent *e, void *userdata) {
    AvahiLegacyUnicastReflectSlot *slot = userdata;

    g_assert(e);
    g_assert(slot);
    g_assert(slot->time_event == e);

    deallocate_slot(slot->server, slot);
}

static void reflect_legacy_unicast_query_packet(AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *a, guint16 port) {
    AvahiLegacyUnicastReflectSlot *slot;
    AvahiInterface *j;

    g_assert(s);
    g_assert(p);
    g_assert(i);
    g_assert(a);
    g_assert(port > 0);
    g_assert(i->protocol == a->family);
    
    if (!s->config.enable_reflector)
        return;

/*     avahi_log_debug("legacy unicast reflectr"); */
    
    /* Reflecting legacy unicast queries is a little more complicated
       than reflecting normal queries, since we must route the
       responses back to the right client. Therefore we must store
       some information for finding the right client contact data for
       response packets. In contrast to normal queries legacy
       unicast query and response packets are reflected untouched and
       are not reassembled into larger packets */

    if (!(slot = allocate_slot(s))) {
        /* No slot available, we drop this legacy unicast query */
        avahi_log_warn("No slot available for legacy unicast reflection, dropping query packet.");
        return;
    }

    slot->original_id = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ID);
    slot->address = *a;
    slot->port = port;
    slot->interface = i->hardware->index;

    avahi_elapse_time(&slot->elapse_time, 2000, 0);
    slot->time_event = avahi_time_event_queue_add(s->time_event_queue, &slot->elapse_time, legacy_unicast_reflect_slot_timeout, slot);

    /* Patch the packet with our new locally generatedt id */
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ID, slot->id);
    
    for (j = s->monitor->interfaces; j; j = j->interface_next)
        if (avahi_interface_relevant(j) &&
            j != i &&
            (s->config.ipv_reflect || j->protocol == i->protocol)) {

            if (j->protocol == AF_INET && s->fd_legacy_unicast_ipv4 >= 0) {
                avahi_send_dns_packet_ipv4(s->fd_legacy_unicast_ipv4, j->hardware->index, p, NULL, 0);
                } else if (j->protocol == AF_INET6 && s->fd_legacy_unicast_ipv6 >= 0)
                avahi_send_dns_packet_ipv6(s->fd_legacy_unicast_ipv6, j->hardware->index, p, NULL, 0);
        }

    /* Reset the id */
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ID, slot->original_id);
}

static gboolean originates_from_local_legacy_unicast_socket(AvahiServer *s, const struct sockaddr *sa) {
    AvahiAddress a;
    g_assert(s);
    g_assert(sa);

    if (!s->config.enable_reflector)
        return FALSE;
    
    avahi_address_from_sockaddr(sa, &a);

    if (!avahi_address_is_local(s->monitor, &a))
        return FALSE;
    
    if (sa->sa_family == AF_INET && s->fd_legacy_unicast_ipv4 >= 0) {
        struct sockaddr_in lsa;
        socklen_t l = sizeof(lsa);
        
        if (getsockname(s->fd_legacy_unicast_ipv4, &lsa, &l) != 0)
            avahi_log_warn("getsockname(): %s", strerror(errno));
        else
            return lsa.sin_port == ((struct sockaddr_in*) sa)->sin_port;

    }

    if (sa->sa_family == AF_INET6 && s->fd_legacy_unicast_ipv6 >= 0) {
        struct sockaddr_in6 lsa;
        socklen_t l = sizeof(lsa);

        if (getsockname(s->fd_legacy_unicast_ipv6, &lsa, &l) != 0)
            avahi_log_warn("getsockname(): %s", strerror(errno));
        else
            return lsa.sin6_port == ((struct sockaddr_in6*) sa)->sin6_port;
    }

    return FALSE;
}

static void dispatch_packet(AvahiServer *s, AvahiDnsPacket *p, const struct sockaddr *sa, gint iface, gint ttl) {
    AvahiInterface *i;
    AvahiAddress a;
    guint16 port;
    
    g_assert(s);
    g_assert(p);
    g_assert(sa);
    g_assert(iface > 0);

    if (!(i = avahi_interface_monitor_get_interface(s->monitor, iface, sa->sa_family)) ||
        !avahi_interface_relevant(i)) {
        avahi_log_warn("Recieved packet from invalid interface.");
        return;
    }

/*     avahi_log_debug("new packet recieved on interface '%s.%i'.", i->hardware->name, i->protocol); */

    port = avahi_port_from_sockaddr(sa);
    avahi_address_from_sockaddr(sa, &a);
    
    if (avahi_address_is_ipv4_in_ipv6(&a))
        /* This is an IPv4 address encapsulated in IPv6, so let's ignore it. */
        return;

    if (originates_from_local_legacy_unicast_socket(s, sa))
        /* This originates from our local reflector, so let's ignore it */
        return;

    if (avahi_dns_packet_check_valid(p) < 0) {
        avahi_log_warn("Recieved invalid packet.");
        return;
    }

    if (avahi_dns_packet_is_query(p)) {
        gboolean legacy_unicast = FALSE;

        if (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ARCOUNT) != 0) {
            avahi_log_warn("Invalid query packet.");
            return;
        }

        if (port != AVAHI_MDNS_PORT) {
            /* Legacy Unicast */

            if ((avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) != 0 ||
                 avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT) != 0)) {
                avahi_log_warn("Invalid legacy unicast query packet.");
                return;
            }
        
            legacy_unicast = TRUE;
        }

        if (legacy_unicast)
            reflect_legacy_unicast_query_packet(s, p, i, &a, port);
        
        handle_query_packet(s, p, i, &a, port, legacy_unicast);
        
/*         avahi_log_debug("Handled query"); */
    } else {

        if (port != AVAHI_MDNS_PORT) {
            avahi_log_warn("Recieved repsonse with invalid source port %u on interface '%s.%i'", port, i->hardware->name, i->protocol);
            return;
        }

        if (ttl != 255) {
            avahi_log_warn("Recieved response with invalid TTL %u on interface '%s.%i'.", ttl, i->hardware->name, i->protocol);
            if (s->config.check_response_ttl)
                return;
        }

        if (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT) != 0 ||
            avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) == 0 ||
            avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT) != 0) {
            avahi_log_warn("Invalid response packet.");
            return;
        }

        handle_response_packet(s, p, i, &a);
/*         avahi_log_debug("Handled response"); */
    }
}

static void dispatch_legacy_unicast_packet(AvahiServer *s, AvahiDnsPacket *p, const struct sockaddr *sa, gint iface, gint ttl) {
    AvahiInterface *i, *j;
    AvahiAddress a;
    guint16 port;
    AvahiLegacyUnicastReflectSlot *slot;
    
    g_assert(s);
    g_assert(p);
    g_assert(sa);
    g_assert(iface > 0);

    if (!(i = avahi_interface_monitor_get_interface(s->monitor, iface, sa->sa_family)) ||
        !avahi_interface_relevant(i)) {
        avahi_log_warn("Recieved packet from invalid interface.");
        return;
    }

/*     avahi_log_debug("new legacy unicast packet recieved on interface '%s.%i'.", i->hardware->name, i->protocol); */

    port = avahi_port_from_sockaddr(sa);
    avahi_address_from_sockaddr(sa, &a);
    
    if (avahi_address_is_ipv4_in_ipv6(&a))
        /* This is an IPv4 address encapsulated in IPv6, so let's ignore it. */
        return;

    if (avahi_dns_packet_check_valid(p) < 0 || avahi_dns_packet_is_query(p)) {
        avahi_log_warn("Recieved invalid packet.");
        return;
    }

    if (!(slot = find_slot(s, avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ID)))) {
        avahi_log_warn("Recieved legacy unicast response with unknown id");
        return;
    }

    if (!(j = avahi_interface_monitor_get_interface(s->monitor, slot->interface, slot->address.family)) ||
        !avahi_interface_relevant(j))
        return;

    /* Patch the original ID into this response */
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ID, slot->original_id);

    /* Forward the response to the correct client */
    avahi_interface_send_packet_unicast(j, p, &slot->address, slot->port);

    /* Undo changes to packet */
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ID, slot->id);
}

static void work(AvahiServer *s) {
    struct sockaddr_in6 sa6;
    struct sockaddr_in sa;
    AvahiDnsPacket *p;
    gint iface = 0;
    guint8 ttl;
        
    g_assert(s);

    if (s->fd_ipv4 >= 0 && (s->pollfd_ipv4.revents & G_IO_IN)) {
        if ((p = avahi_recv_dns_packet_ipv4(s->fd_ipv4, &sa, &iface, &ttl))) {
            dispatch_packet(s, p, (struct sockaddr*) &sa, iface, ttl);
            avahi_dns_packet_free(p);
        }
    }

    if (s->fd_ipv6 >= 0 && (s->pollfd_ipv6.revents & G_IO_IN)) {
        if ((p = avahi_recv_dns_packet_ipv6(s->fd_ipv6, &sa6, &iface, &ttl))) {
            dispatch_packet(s, p, (struct sockaddr*) &sa6, iface, ttl);
            avahi_dns_packet_free(p);
        }
    }

    if (s->fd_legacy_unicast_ipv4 >= 0 && (s->pollfd_legacy_unicast_ipv4.revents & G_IO_IN)) {
        if ((p = avahi_recv_dns_packet_ipv4(s->fd_legacy_unicast_ipv4, &sa, &iface, &ttl))) {
            dispatch_legacy_unicast_packet(s, p, (struct sockaddr*) &sa, iface, ttl);
            avahi_dns_packet_free(p);
        }
    }

    if (s->fd_legacy_unicast_ipv6 >= 0 && (s->pollfd_legacy_unicast_ipv6.revents & G_IO_IN)) {
        if ((p = avahi_recv_dns_packet_ipv6(s->fd_legacy_unicast_ipv6, &sa6, &iface, &ttl))) {
            dispatch_legacy_unicast_packet(s, p, (struct sockaddr*) &sa6, iface, ttl);
            avahi_dns_packet_free(p);
        }
    }
}

static gboolean prepare_func(GSource *source, gint *timeout) {
    g_assert(source);
    g_assert(timeout);
    
    *timeout = -1;
    return FALSE;
}

static gboolean check_func(GSource *source) {
    AvahiServer* s;
    gushort revents = 0;
    
    g_assert(source);

    s = *((AvahiServer**) (((guint8*) source) + sizeof(GSource)));
    g_assert(s);

    if (s->fd_ipv4 >= 0)
        revents |= s->pollfd_ipv4.revents;
    if (s->fd_ipv6 >= 0)
        revents |= s->pollfd_ipv6.revents;
    if (s->fd_legacy_unicast_ipv4 >= 0)
        revents |= s->pollfd_legacy_unicast_ipv4.revents;
    if (s->fd_legacy_unicast_ipv6 >= 0)
        revents |= s->pollfd_legacy_unicast_ipv6.revents;
    
    return !!(revents & (G_IO_IN | G_IO_HUP | G_IO_ERR));
}

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer user_data) {
    AvahiServer* s;
    g_assert(source);

    s = *((AvahiServer**) (((guint8*) source) + sizeof(GSource)));
    g_assert(s);

    work(s);
    cleanup_dead(s);

    return TRUE;
}

static void server_set_state(AvahiServer *s, AvahiServerState state) {
    g_assert(s);

    if (s->state == state)
        return;
    
    s->state = state;

    if (s->callback)
        s->callback(s, state, s->userdata);
}

static void withdraw_host_rrs(AvahiServer *s) {
    g_assert(s);

    if (s->hinfo_entry_group) {
        avahi_entry_group_free(s->hinfo_entry_group);
        s->hinfo_entry_group = NULL;
    }

    if (s->browse_domain_entry_group) {
        avahi_entry_group_free(s->browse_domain_entry_group);
        s->browse_domain_entry_group = NULL;
    }

    avahi_update_host_rrs(s->monitor, TRUE);
    s->n_host_rr_pending = 0;
}

void avahi_server_decrease_host_rr_pending(AvahiServer *s) {
    g_assert(s);
    
    g_assert(s->n_host_rr_pending > 0);

    if (--s->n_host_rr_pending == 0)
        server_set_state(s, AVAHI_SERVER_RUNNING);
}

void avahi_server_increase_host_rr_pending(AvahiServer *s) {
    g_assert(s);

    s->n_host_rr_pending ++;
}

void avahi_host_rr_entry_group_callback(AvahiServer *s, AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {
    g_assert(s);
    g_assert(g);

    if (state == AVAHI_ENTRY_GROUP_REGISTERING &&
        s->state == AVAHI_SERVER_REGISTERING)
        avahi_server_increase_host_rr_pending(s);
    else if (state == AVAHI_ENTRY_GROUP_COLLISION &&
        (s->state == AVAHI_SERVER_REGISTERING || s->state == AVAHI_SERVER_RUNNING)) {
        withdraw_host_rrs(s);
        server_set_state(s, AVAHI_SERVER_COLLISION);
    } else if (state == AVAHI_ENTRY_GROUP_ESTABLISHED &&
               s->state == AVAHI_SERVER_REGISTERING)
        avahi_server_decrease_host_rr_pending(s);
}

static void register_hinfo(AvahiServer *s) {
    struct utsname utsname;
    AvahiRecord *r;
    
    g_assert(s);
    
    if (!s->config.register_hinfo || s->hinfo_entry_group)
        return;
    
    s->hinfo_entry_group = avahi_entry_group_new(s, avahi_host_rr_entry_group_callback, NULL);
    
    /* Fill in HINFO rr */
    r = avahi_record_new_full(s->host_name_fqdn, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_HINFO);
    uname(&utsname);
    r->data.hinfo.cpu = g_strdup(g_strup(utsname.machine));
    r->data.hinfo.os = g_strdup(g_strup(utsname.sysname));
    avahi_server_add(s, s->hinfo_entry_group, 0, AF_UNSPEC, AVAHI_ENTRY_UNIQUE, r);
    avahi_record_unref(r);

    avahi_entry_group_commit(s->hinfo_entry_group);
}

static void register_localhost(AvahiServer *s) {
    AvahiAddress a;
    g_assert(s);
    
    /* Add localhost entries */
    avahi_address_parse("127.0.0.1", AF_INET, &a);
    avahi_server_add_address(s, NULL, 0, AF_UNSPEC, AVAHI_ENTRY_NOPROBE|AVAHI_ENTRY_NOANNOUNCE, "localhost", &a);

    avahi_address_parse("::1", AF_INET6, &a);
    avahi_server_add_address(s, NULL, 0, AF_UNSPEC, AVAHI_ENTRY_NOPROBE|AVAHI_ENTRY_NOANNOUNCE, "ip6-localhost", &a);
}

static void register_browse_domain(AvahiServer *s) {
    g_assert(s);

    if (!s->config.announce_domain || s->browse_domain_entry_group)
        return;

    s->browse_domain_entry_group = avahi_entry_group_new(s, NULL, NULL);
    avahi_server_add_ptr(s, s->browse_domain_entry_group, 0, AF_UNSPEC, 0, "_browse._dns-sd._udp.local", s->domain_name);
    avahi_entry_group_commit(s->browse_domain_entry_group);
}

static void register_stuff(AvahiServer *s) {
    g_assert(s);

    server_set_state(s, AVAHI_SERVER_REGISTERING);
    register_hinfo(s);
    register_browse_domain(s);
    avahi_update_host_rrs(s->monitor, FALSE);

    if (s->n_host_rr_pending == 0)
        server_set_state(s, AVAHI_SERVER_RUNNING);
}

static void update_fqdn(AvahiServer *s) {
    g_assert(s);
    
    g_assert(s->host_name);
    g_assert(s->domain_name);

    g_free(s->host_name_fqdn);
    s->host_name_fqdn = g_strdup_printf("%s.%s", s->host_name, s->domain_name);
}

static void register_time_event_callback(AvahiTimeEvent *e, gpointer userdata) {
    AvahiServer *s = userdata;
    
    g_assert(e);
    g_assert(s);

    g_assert(e == s->register_time_event);
    avahi_time_event_queue_remove(s->time_event_queue, s->register_time_event);
    s->register_time_event = NULL;

    if (s->state == AVAHI_SERVER_SLEEPING)
        register_stuff(s);
}

static void delayed_register_stuff(AvahiServer *s) {
    GTimeVal tv;
    
    g_assert(s);

    avahi_elapse_time(&tv, AVAHI_HOST_RR_HOLDOFF_MSEC, 0);

    if (s->register_time_event)
        avahi_time_event_queue_update(s->time_event_queue, s->register_time_event, &tv);
    else
        s->register_time_event = avahi_time_event_queue_add(s->time_event_queue, &tv, register_time_event_callback, s);
}

gint avahi_server_set_host_name(AvahiServer *s, const gchar *host_name) {
    g_assert(s);
    g_assert(host_name);

    server_set_state(s, AVAHI_SERVER_SLEEPING);
    withdraw_host_rrs(s);

    g_free(s->host_name);
    s->host_name = host_name ? avahi_normalize_name(host_name) : avahi_get_host_name();
    s->host_name[strcspn(s->host_name, ".")] = 0;
    update_fqdn(s);

    delayed_register_stuff(s);
    return 0;
}

gint avahi_server_set_domain_name(AvahiServer *s, const gchar *domain_name) {
    g_assert(s);
    g_assert(domain_name);

    server_set_state(s, AVAHI_SERVER_SLEEPING);
    withdraw_host_rrs(s);

    g_free(s->domain_name);
    s->domain_name = domain_name ? avahi_normalize_name(domain_name) : g_strdup("local.");
    update_fqdn(s);

    delayed_register_stuff(s);
    return 0;
}


static void prepare_pollfd(AvahiServer *s, GPollFD *pollfd, gint fd) {
    g_assert(s);
    g_assert(pollfd);
    g_assert(fd >= 0);

    memset(pollfd, 0, sizeof(GPollFD));
    pollfd->fd = fd;
    pollfd->events = G_IO_IN|G_IO_ERR|G_IO_HUP;
    g_source_add_poll(s->source, pollfd);
}

AvahiServer *avahi_server_new(GMainContext *c, const AvahiServerConfig *sc, AvahiServerCallback callback, gpointer userdata) {
    AvahiServer *s;
    
    static GSourceFuncs source_funcs = {
        prepare_func,
        check_func,
        dispatch_func,
        NULL,
        NULL,
        NULL
    };

    s = g_new(AvahiServer, 1);
    s->n_host_rr_pending = 0;
    s->need_entry_cleanup = s->need_group_cleanup = s->need_browser_cleanup = FALSE;

    if (sc)
        avahi_server_config_copy(&s->config, sc);
    else
        avahi_server_config_init(&s->config);
    
    s->fd_ipv4 = s->config.use_ipv4 ? avahi_open_socket_ipv4() : -1;
    s->fd_ipv6 = s->config.use_ipv6 ? avahi_open_socket_ipv6() : -1;
    
    if (s->fd_ipv6 < 0 && s->fd_ipv4 < 0) {
        g_critical("Selected neither IPv6 nor IPv4 support, aborting.\n");
        avahi_server_config_free(&s->config);
        g_free(s);
        return NULL;
    }

    if (s->fd_ipv4 < 0 && s->config.use_ipv4)
        avahi_log_debug("Failed to create IPv4 socket, proceeding in IPv6 only mode");
    else if (s->fd_ipv6 < 0 && s->config.use_ipv6)
        avahi_log_debug("Failed to create IPv6 socket, proceeding in IPv4 only mode");

    s->fd_legacy_unicast_ipv4 = s->fd_ipv4 >= 0 && s->config.enable_reflector ? avahi_open_legacy_unicast_socket_ipv4() : -1;
    s->fd_legacy_unicast_ipv6 = s->fd_ipv6 >= 0 && s->config.enable_reflector ? avahi_open_legacy_unicast_socket_ipv6() : -1;
    
    if (c)
        g_main_context_ref(s->context = c);
    else
        s->context = g_main_context_default();

    /* Prepare IO source registration */
    s->source = g_source_new(&source_funcs, sizeof(GSource) + sizeof(AvahiServer*));
    *((AvahiServer**) (((guint8*) s->source) + sizeof(GSource))) = s;

    if (s->fd_ipv4 >= 0)
        prepare_pollfd(s, &s->pollfd_ipv4, s->fd_ipv4);
    if (s->fd_ipv6 >= 0)
        prepare_pollfd(s, &s->pollfd_ipv6, s->fd_ipv6);
    if (s->fd_legacy_unicast_ipv4 >= 0)
        prepare_pollfd(s, &s->pollfd_legacy_unicast_ipv4, s->fd_legacy_unicast_ipv4);
    if (s->fd_legacy_unicast_ipv6 >= 0)
        prepare_pollfd(s, &s->pollfd_legacy_unicast_ipv6, s->fd_legacy_unicast_ipv6);
    
    g_source_attach(s->source, s->context);
    
    s->callback = callback;
    s->userdata = userdata;
    
    AVAHI_LLIST_HEAD_INIT(AvahiEntry, s->entries);
    s->entries_by_key = g_hash_table_new((GHashFunc) avahi_key_hash, (GEqualFunc) avahi_key_equal);
    AVAHI_LLIST_HEAD_INIT(AvahiGroup, s->groups);

    AVAHI_LLIST_HEAD_INIT(AvahiRecordBrowser, s->record_browsers);
    s->record_browser_hashtable = g_hash_table_new((GHashFunc) avahi_key_hash, (GEqualFunc) avahi_key_equal);
    AVAHI_LLIST_HEAD_INIT(AvahiHostNameResolver, s->host_name_resolvers);
    AVAHI_LLIST_HEAD_INIT(AvahiAddressResolver, s->address_resolvers);
    AVAHI_LLIST_HEAD_INIT(AvahiDomainBrowser, s->domain_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceTypeBrowser, s->service_type_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceBrowser, s->service_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiServiceResolver, s->service_resolvers);

    s->legacy_unicast_reflect_slots = NULL;
    s->legacy_unicast_reflect_id = 0;
    
    /* Get host name */
    s->host_name = s->config.host_name ? avahi_normalize_name(s->config.host_name) : avahi_get_host_name();
    s->host_name[strcspn(s->host_name, ".")] = 0;
    s->domain_name = s->config.domain_name ? avahi_normalize_name(s->config.domain_name) : g_strdup("local.");
    s->host_name_fqdn = NULL;
    update_fqdn(s);

    s->record_list = avahi_record_list_new();

    s->time_event_queue = avahi_time_event_queue_new(s->context, G_PRIORITY_DEFAULT+10); /* Slightly less priority than the FDs */
    s->register_time_event = NULL;
    
    s->state = AVAHI_SERVER_INVALID;

    s->monitor = avahi_interface_monitor_new(s);
    avahi_interface_monitor_sync(s->monitor);

    register_localhost(s);

    s->hinfo_entry_group = NULL;
    s->browse_domain_entry_group = NULL;
    register_stuff(s);
    
    return s;
}

void avahi_server_free(AvahiServer* s) {
    g_assert(s);

    while(s->entries)
        free_entry(s, s->entries);

    avahi_interface_monitor_free(s->monitor);

    while (s->groups)
        free_group(s, s->groups);

    free_slots(s);
    
    while (s->host_name_resolvers)
        avahi_host_name_resolver_free(s->host_name_resolvers);
    while (s->address_resolvers)
        avahi_address_resolver_free(s->address_resolvers);
    while (s->domain_browsers)
        avahi_domain_browser_free(s->domain_browsers);
    while (s->service_type_browsers)
        avahi_service_type_browser_free(s->service_type_browsers);
    while (s->service_browsers)
        avahi_service_browser_free(s->service_browsers);
    while (s->service_resolvers)
        avahi_service_resolver_free(s->service_resolvers);
    while (s->record_browsers)
        avahi_record_browser_destroy(s->record_browsers);
    g_hash_table_destroy(s->record_browser_hashtable);

    g_hash_table_destroy(s->entries_by_key);

    if (s->register_time_event)
        avahi_time_event_queue_remove(s->time_event_queue, s->register_time_event);
    avahi_time_event_queue_free(s->time_event_queue);

    avahi_record_list_free(s->record_list);
    
    if (s->fd_ipv4 >= 0)
        close(s->fd_ipv4);
    if (s->fd_ipv6 >= 0)
        close(s->fd_ipv6);
    if (s->fd_legacy_unicast_ipv4 >= 0)
        close(s->fd_legacy_unicast_ipv4);
    if (s->fd_legacy_unicast_ipv6 >= 0)
        close(s->fd_legacy_unicast_ipv6);

    g_free(s->host_name);
    g_free(s->domain_name);
    g_free(s->host_name_fqdn);

    g_source_destroy(s->source);
    g_source_unref(s->source);
    g_main_context_unref(s->context);

    avahi_server_config_free(&s->config);

    g_free(s);
}

gint avahi_server_add(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    AvahiRecord *r) {
    
    AvahiEntry *e, *t;
    g_assert(s);
    g_assert(r);

    g_assert(r->key->type != AVAHI_DNS_TYPE_ANY);

    e = g_new(AvahiEntry, 1);
    e->server = s;
    e->record = avahi_record_ref(r);
    e->group = g;
    e->interface = interface;
    e->protocol = protocol;
    e->flags = flags;
    e->dead = FALSE;

    AVAHI_LLIST_HEAD_INIT(AvahiAnnouncement, e->announcements);

    AVAHI_LLIST_PREPEND(AvahiEntry, entries, s->entries, e);

    /* Insert into hash table indexed by name */
    t = g_hash_table_lookup(s->entries_by_key, e->record->key);
    AVAHI_LLIST_PREPEND(AvahiEntry, by_key, t, e);
    g_hash_table_replace(s->entries_by_key, e->record->key, t);

    /* Insert into group list */
    if (g)
        AVAHI_LLIST_PREPEND(AvahiEntry, by_group, g->entries, e); 

    avahi_announce_entry(s, e);

    return 0;
}

const AvahiRecord *avahi_server_iterate(AvahiServer *s, AvahiEntryGroup *g, void **state) {
    AvahiEntry **e = (AvahiEntry**) state;
    g_assert(s);
    g_assert(e);

    if (!*e)
        *e = g ? g->entries : s->entries;
    
    while (*e && (*e)->dead)
        *e = g ? (*e)->by_group_next : (*e)->entries_next;
        
    if (!*e)
        return NULL;

    return avahi_record_ref((*e)->record);
}

void avahi_server_dump(AvahiServer *s, FILE *f) {
    AvahiEntry *e;
    g_assert(s);
    g_assert(f);

    fprintf(f, "\n;;; ZONE DUMP FOLLOWS ;;;\n");

    for (e = s->entries; e; e = e->entries_next) {
        gchar *t;

        if (e->dead)
            continue;
        
        t = avahi_record_to_string(e->record);
        fprintf(f, "%s ; iface=%i proto=%i\n", t, e->interface, e->protocol);
        g_free(t);
    }

    avahi_dump_caches(s->monitor, f);
}

gint avahi_server_add_ptr(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    const gchar *dest) {

    AvahiRecord *r;

    g_assert(dest);

    r = avahi_record_new_full(name ? name : s->host_name_fqdn, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR);
    r->data.ptr.name = avahi_normalize_name(dest);
    avahi_server_add(s, g, interface, protocol, flags, r);
    avahi_record_unref(r);
    return 0;
}

gint avahi_server_add_address(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    AvahiAddress *a) {

    gchar *n = NULL;
    g_assert(s);
    g_assert(a);

    name = name ? (n = avahi_normalize_name(name)) : s->host_name_fqdn;
    
    if (a->family == AF_INET) {
        gchar *reverse;
        AvahiRecord  *r;

        r = avahi_record_new_full(name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A);
        r->data.a.address = a->data.ipv4;
        avahi_server_add(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE, r);
        avahi_record_unref(r);
        
        reverse = avahi_reverse_lookup_name_ipv4(&a->data.ipv4);
        avahi_server_add_ptr(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE, reverse, name);
        g_free(reverse);
        
    } else {
        gchar *reverse;
        AvahiRecord *r;
            
        r = avahi_record_new_full(name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA);
        r->data.aaaa.address = a->data.ipv6;
        avahi_server_add(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE, r);
        avahi_record_unref(r);

        reverse = avahi_reverse_lookup_name_ipv6_arpa(&a->data.ipv6);
        avahi_server_add_ptr(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE, reverse, name);
        g_free(reverse);
    
        reverse = avahi_reverse_lookup_name_ipv6_int(&a->data.ipv6);
        avahi_server_add_ptr(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE, reverse, name);
        g_free(reverse);
    }
    
    g_free(n);

    return 0;
}

gint avahi_server_add_text_strlst(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    AvahiStringList *strlst) {

    AvahiRecord *r;
    
    g_assert(s);
    
    r = avahi_record_new_full(name ? name : s->host_name_fqdn, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT);
    r->data.txt.string_list = strlst;
    avahi_server_add(s, g, interface, protocol, flags, r);
    avahi_record_unref(r);

    return 0;
}

gint avahi_server_add_text_va(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    va_list va) {
    
    g_assert(s);

    avahi_server_add_text_strlst(s, g, interface, protocol, flags, name, avahi_string_list_new_va(va));
    return 0;
}

gint avahi_server_add_text(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    ...) {

    va_list va;
    
    g_assert(s);

    va_start(va, name);
    avahi_server_add_text_va(s, g, interface, protocol, flags, name, va);
    va_end(va);

    return 0;
}

static void escape_service_name(gchar *d, guint size, const gchar *s) {
    g_assert(d);
    g_assert(size);
    g_assert(s);

    while (*s && size >= 2) {
        if (*s == '.' || *s == '\\') {
            if (size < 3)
                break;

            *(d++) = '\\';
            size--;
        }
            
        *(d++) = *(s++);
        size--;
    }

    g_assert(size > 0);
    *(d++) = 0;
}

gint avahi_server_add_service_strlst(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    AvahiStringList *strlst) {

    gchar ptr_name[256], svc_name[256], ename[64], enum_ptr[256];
    AvahiRecord *r;
    
    g_assert(s);
    g_assert(type);
    g_assert(name);

    escape_service_name(ename, sizeof(ename), name);

    if (domain) {
        while (domain[0] == '.')
            domain++;
    } else
        domain = s->domain_name;

    if (!host)
        host = s->host_name_fqdn;

    snprintf(ptr_name, sizeof(ptr_name), "%s.%s", type, domain);
    snprintf(svc_name, sizeof(svc_name), "%s.%s.%s", ename, type, domain);
    
    avahi_server_add_ptr(s, g, interface, protocol, AVAHI_ENTRY_NULL, ptr_name, svc_name);

    r = avahi_record_new_full(svc_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_SRV);
    r->data.srv.priority = 0;
    r->data.srv.weight = 0;
    r->data.srv.port = port;
    r->data.srv.name = avahi_normalize_name(host);
    avahi_server_add(s, g, interface, protocol, AVAHI_ENTRY_UNIQUE, r);
    avahi_record_unref(r);

    avahi_server_add_text_strlst(s, g, interface, protocol, AVAHI_ENTRY_UNIQUE, svc_name, strlst);

    snprintf(enum_ptr, sizeof(enum_ptr), "_services._dns-sd._udp.%s", domain);
    avahi_server_add_ptr(s, g, interface, protocol, AVAHI_ENTRY_NULL, enum_ptr, ptr_name);

    return 0;
}

gint avahi_server_add_service_va(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    va_list va){

    g_assert(s);
    g_assert(type);
    g_assert(name);

    avahi_server_add_service_strlst(s, g, interface, protocol, type, name, domain, host, port, avahi_string_list_new_va(va));
    return 0;
}

gint avahi_server_add_service(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    ... ){

    va_list va;
    
    g_assert(s);
    g_assert(type);
    g_assert(name);

    va_start(va, port);
    avahi_server_add_service_va(s, g, interface, protocol, type, name, domain, host, port, va);
    va_end(va);
    return 0;
}

static void post_query_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, gpointer userdata) {
    AvahiKey *k = userdata;

    g_assert(m);
    g_assert(i);
    g_assert(k);

    avahi_interface_post_query(i, k, FALSE);
}

void avahi_server_post_query(AvahiServer *s, gint interface, guchar protocol, AvahiKey *key) {
    g_assert(s);
    g_assert(key);

    avahi_interface_monitor_walk(s->monitor, interface, protocol, post_query_callback, key);
}

void avahi_entry_group_change_state(AvahiEntryGroup *g, AvahiEntryGroupState state) {
    g_assert(g);

    if (g->state == state)
        return;

    g->state = state;
    
    if (g->callback) {
        g->callback(g->server, g, state, g->userdata);
        return;
    }
}

AvahiEntryGroup *avahi_entry_group_new(AvahiServer *s, AvahiEntryGroupCallback callback, gpointer userdata) {
    AvahiEntryGroup *g;
    
    g_assert(s);

    g = g_new(AvahiEntryGroup, 1);
    g->server = s;
    g->callback = callback;
    g->userdata = userdata;
    g->dead = FALSE;
    g->state = AVAHI_ENTRY_GROUP_UNCOMMITED;
    g->n_probing = 0;
    AVAHI_LLIST_HEAD_INIT(AvahiEntry, g->entries);

    AVAHI_LLIST_PREPEND(AvahiEntryGroup, groups, s->groups, g);
    return g;
}

void avahi_entry_group_free(AvahiEntryGroup *g) {
    AvahiEntry *e;
    
    g_assert(g);
    g_assert(g->server);

    for (e = g->entries; e; e = e->by_group_next) {
        avahi_goodbye_entry(g->server, e, TRUE);
        e->dead = TRUE;
    }

    g->dead = TRUE;
    
    g->server->need_group_cleanup = TRUE;
    g->server->need_entry_cleanup = TRUE;
}

gint avahi_entry_group_commit(AvahiEntryGroup *g) {
    g_assert(g);
    g_assert(!g->dead);

    if (g->state != AVAHI_ENTRY_GROUP_UNCOMMITED)
        return -1;

    avahi_entry_group_change_state(g, AVAHI_ENTRY_GROUP_REGISTERING);
    avahi_announce_group(g->server, g);
    avahi_entry_group_check_probed(g, FALSE);

    return 0;
}

gboolean avahi_entry_commited(AvahiEntry *e) {
    g_assert(e);
    g_assert(!e->dead);

    return !e->group ||
        e->group->state == AVAHI_ENTRY_GROUP_REGISTERING ||
        e->group->state == AVAHI_ENTRY_GROUP_ESTABLISHED;
}

AvahiEntryGroupState avahi_entry_group_get_state(AvahiEntryGroup *g) {
    g_assert(g);
    g_assert(!g->dead);

    return g->state;
}

void avahi_entry_group_set_data(AvahiEntryGroup *g, gpointer userdata) {
    g_assert(g);

    g->userdata = userdata;
}

gpointer avahi_entry_group_get_data(AvahiEntryGroup *g) {
    g_assert(g);

    return g->userdata;
}

const gchar* avahi_server_get_domain_name(AvahiServer *s) {
    g_assert(s);

    return s->domain_name;
}

const gchar* avahi_server_get_host_name(AvahiServer *s) {
    g_assert(s);

    return s->host_name;
}

const gchar* avahi_server_get_host_name_fqdn(AvahiServer *s) {
    g_assert(s);

    return s->host_name_fqdn;
}

gpointer avahi_server_get_data(AvahiServer *s) {
    g_assert(s);

    return s->userdata;
}

void avahi_server_set_data(AvahiServer *s, gpointer userdata) {
    g_assert(s);

    s->userdata = userdata;
}

AvahiServerState avahi_server_get_state(AvahiServer *s) {
    g_assert(s);

    return s->state;
}

AvahiServerConfig* avahi_server_config_init(AvahiServerConfig *c) {
    g_assert(c);

    memset(c, 0, sizeof(AvahiServerConfig));
    c->register_hinfo = TRUE;
    c->register_addresses = TRUE;
    c->use_ipv6 = TRUE;
    c->use_ipv4 = TRUE;
    c->host_name = NULL;
    c->domain_name = NULL;
    c->check_response_ttl = TRUE;
    c->announce_domain = TRUE;
    c->use_iff_running = FALSE;
    c->enable_reflector = FALSE;
    c->ipv_reflect = FALSE;
    c->register_workstation = TRUE;
    
    return c;
}

void avahi_server_config_free(AvahiServerConfig *c) {
    g_assert(c);

    g_free(c->host_name);
    g_free(c->domain_name);
}

AvahiServerConfig* avahi_server_config_copy(AvahiServerConfig *ret, const AvahiServerConfig *c) {
    g_assert(ret);
    g_assert(c);

    *ret = *c;

    ret->host_name = g_strdup(c->host_name);
    ret->domain_name = g_strdup(c->domain_name);

    return ret;
}
