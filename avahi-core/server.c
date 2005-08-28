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
#include <stdio.h>
#include <assert.h>

#include <avahi-common/domain.h>
#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "server.h"
#include "iface.h"
#include "socket.h"
#include "browse.h"
#include "log.h"
#include "util.h"

#define AVAHI_RR_HOLDOFF_MSEC 1000
#define AVAHI_RR_HOLDOFF_MSEC_RATE_LIMIT 60000
#define AVAHI_RR_RATE_LIMIT_COUNT 15

static void free_entry(AvahiServer*s, AvahiEntry *e) {
    AvahiEntry *t;

    assert(s);
    assert(e);

    avahi_goodbye_entry(s, e, 1);

    /* Remove from linked list */
    AVAHI_LLIST_REMOVE(AvahiEntry, entries, s->entries, e);

    /* Remove from hash table indexed by name */
    t = avahi_hashmap_lookup(s->entries_by_key, e->record->key);
    AVAHI_LLIST_REMOVE(AvahiEntry, by_key, t, e);
    if (t)
        avahi_hashmap_replace(s->entries_by_key, t->record->key, t);
    else
        avahi_hashmap_remove(s->entries_by_key, e->record->key);

    /* Remove from associated group */
    if (e->group)
        AVAHI_LLIST_REMOVE(AvahiEntry, by_group, e->group->entries, e);

    avahi_record_unref(e->record);
    avahi_free(e);
}

static void free_group(AvahiServer *s, AvahiSEntryGroup *g) {
    assert(s);
    assert(g);

    while (g->entries)
        free_entry(s, g->entries);

    if (g->register_time_event)
        avahi_time_event_free(g->register_time_event);
    
    AVAHI_LLIST_REMOVE(AvahiSEntryGroup, groups, s->groups, g);
    avahi_free(g);
}

static void cleanup_dead(AvahiServer *s) {
    assert(s);

    if (s->need_group_cleanup) {
        AvahiSEntryGroup *g, *next;
        
        for (g = s->groups; g; g = next) {
            next = g->groups_next;
            
            if (g->dead)
                free_group(s, g);
        }

        s->need_group_cleanup = 0;
    }

    if (s->need_entry_cleanup) {
        AvahiEntry *e, *next;
        
        for (e = s->entries; e; e = next) {
            next = e->entries_next;
            
            if (e->dead)
                free_entry(s, e);
        }

        s->need_entry_cleanup = 0;
    }

    if (s->need_browser_cleanup)
        avahi_browser_cleanup(s);
}

static void enum_aux_records(AvahiServer *s, AvahiInterface *i, const char *name, uint16_t type, void (*callback)(AvahiServer *s, AvahiRecord *r, int flush_cache, void* userdata), void* userdata) {
    AvahiKey *k;
    AvahiEntry *e;

    assert(s);
    assert(i);
    assert(name);
    assert(callback);

    assert(type != AVAHI_DNS_TYPE_ANY);

    if (!(k = avahi_key_new(name, AVAHI_DNS_CLASS_IN, type)))
        return; /** OOM */

    for (e = avahi_hashmap_lookup(s->entries_by_key, k); e; e = e->by_key_next)
        if (!e->dead && avahi_entry_is_registered(s, e, i)) 
            callback(s, e->record, e->flags & AVAHI_ENTRY_UNIQUE, userdata);

    avahi_key_unref(k);
}

void avahi_server_enumerate_aux_records(AvahiServer *s, AvahiInterface *i, AvahiRecord *r, void (*callback)(AvahiServer *s, AvahiRecord *r, int flush_cache, void* userdata), void* userdata) {
    assert(s);
    assert(i);
    assert(r);
    assert(callback);
    
    if (r->key->clazz == AVAHI_DNS_CLASS_IN) {
        if (r->key->type == AVAHI_DNS_TYPE_PTR) {
            enum_aux_records(s, i, r->data.ptr.name, AVAHI_DNS_TYPE_SRV, callback, userdata);
            enum_aux_records(s, i, r->data.ptr.name, AVAHI_DNS_TYPE_TXT, callback, userdata);
        } else if (r->key->type == AVAHI_DNS_TYPE_SRV) {
            enum_aux_records(s, i, r->data.srv.name, AVAHI_DNS_TYPE_A, callback, userdata);
            enum_aux_records(s, i, r->data.srv.name, AVAHI_DNS_TYPE_AAAA, callback, userdata);
        }
    }
}

void avahi_server_prepare_response(AvahiServer *s, AvahiInterface *i, AvahiEntry *e, int unicast_response, int auxiliary) {
    assert(s);
    assert(i);
    assert(e);

    avahi_record_list_push(s->record_list, e->record, e->flags & AVAHI_ENTRY_UNIQUE, unicast_response, auxiliary);
}

void avahi_server_prepare_matching_responses(AvahiServer *s, AvahiInterface *i, AvahiKey *k, int unicast_response) {
    AvahiEntry *e;
/*     char *txt; */
    
    assert(s);
    assert(i);
    assert(k);

/*     avahi_log_debug("Posting responses matching [%s]", txt = avahi_key_to_string(k)); */
/*     avahi_free(txt); */

    if (avahi_key_is_pattern(k)) {

        /* Handle ANY query */
        
        for (e = s->entries; e; e = e->entries_next)
            if (!e->dead && avahi_key_pattern_match(k, e->record->key) && avahi_entry_is_registered(s, e, i))
                avahi_server_prepare_response(s, i, e, unicast_response, 0);

    } else {

        /* Handle all other queries */
        
        for (e = avahi_hashmap_lookup(s->entries_by_key, k); e; e = e->by_key_next)
            if (!e->dead && avahi_entry_is_registered(s, e, i))
                avahi_server_prepare_response(s, i, e, unicast_response, 0);
    }
}

static void withdraw_entry(AvahiServer *s, AvahiEntry *e) {
    assert(s);
    assert(e);
    
    if (e->group) {
        AvahiEntry *k;
        
        for (k = e->group->entries; k; k = k->by_group_next) {
            if (!k->dead) {
                avahi_goodbye_entry(s, k, 0);
                k->dead = 1;
            }
        }

        e->group->n_probing = 0;

        avahi_s_entry_group_change_state(e->group, AVAHI_ENTRY_GROUP_COLLISION);
    } else {
        avahi_goodbye_entry(s, e, 0);
        e->dead = 1;
    }

    s->need_entry_cleanup = 1;
}

static void withdraw_rrset(AvahiServer *s, AvahiKey *key) {
    AvahiEntry *e;
    
    assert(s);
    assert(key);

   for (e = avahi_hashmap_lookup(s->entries_by_key, key); e; e = e->by_key_next)
       if (!e->dead)
           withdraw_entry(s, e);
}

static void incoming_probe(AvahiServer *s, AvahiRecord *record, AvahiInterface *i) {
    AvahiEntry *e, *n;
    char *t;
    int ours = 0, won = 0, lost = 0;
    
    assert(s);
    assert(record);
    assert(i);

    t = avahi_record_to_string(record);

/*     avahi_log_debug("incoming_probe()");  */

    for (e = avahi_hashmap_lookup(s->entries_by_key, record->key); e; e = n) {
        int cmp;
        n = e->by_key_next;

        if (e->dead)
            continue;
        
        if ((cmp = avahi_record_lexicographical_compare(e->record, record)) == 0) {
            ours = 1;
            break;
        } else {
            
            if (avahi_entry_is_probing(s, e, i)) {
                if (cmp > 0)
                    won = 1;
                else /* cmp < 0 */
                    lost = 1;
            }
        }
    }

    if (!ours) {

        if (won)
            avahi_log_debug("Recieved conflicting probe [%s]. Local host won.", t);
        else if (lost) {
            avahi_log_debug("Recieved conflicting probe [%s]. Local host lost. Withdrawing.", t);
            withdraw_rrset(s, record->key);
        }/*  else */
/*             avahi_log_debug("Not conflicting probe"); */
    }

    avahi_free(t);
}

static int handle_conflict(AvahiServer *s, AvahiInterface *i, AvahiRecord *record, int unique, const AvahiAddress *a) {
    int valid = 1, ours = 0, conflict = 0, withdraw_immediately = 0;
    AvahiEntry *e, *n, *conflicting_entry = NULL;
    
    assert(s);
    assert(i);
    assert(record);


/*     avahi_log_debug("CHECKING FOR CONFLICT: [%s]", t);   */

    for (e = avahi_hashmap_lookup(s->entries_by_key, record->key); e; e = n) {
        n = e->by_key_next;

        if (e->dead)
            continue;

        /* Check if the incoming is a goodbye record */
        if (avahi_record_is_goodbye(record)) {

            if (avahi_record_equal_no_ttl(e->record, record)) {
                char *t;

                /* Refresh */
                t = avahi_record_to_string(record); 
                avahi_log_debug("Recieved goodbye record for one of our records [%s]. Refreshing.", t);
                avahi_server_prepare_matching_responses(s, i, e->record->key, 0);

                valid = 0;
                avahi_free(t);
                break;
            }

            /* If the goodybe packet doesn't match one of our own RRs, we simply ignore it. */
            continue;
        }
        
        if (!(e->flags & AVAHI_ENTRY_UNIQUE) && !unique)
            continue;

        /* Either our entry or the other is intended to be unique, so let's check */
        
        if (avahi_record_equal_no_ttl(e->record, record)) {
            ours = 1; /* We have an identical record, so this is no conflict */
            
            /* Check wheter there is a TTL conflict */
            if (record->ttl <= e->record->ttl/2 &&
                avahi_entry_is_registered(s, e, i)) {
                char *t;
                /* Refresh */
                t = avahi_record_to_string(record); 
                
                avahi_log_debug("Recieved record with bad TTL [%s]. Refreshing.", t);
                avahi_server_prepare_matching_responses(s, i, e->record->key, 0);
                valid = 0;
                
                avahi_free(t);
            }
                
            /* There's no need to check the other entries of this RRset */
            break;

        } else {
            
            if (avahi_entry_is_registered(s, e, i)) {
                
                /* A conflict => we have to return to probe mode */
                conflict = 1;
                conflicting_entry = e;

            } else if (avahi_entry_is_probing(s, e, i)) {

                /* We are currently registering a matching record, but
                 * someone else already claimed it, so let's
                 * withdraw */
                conflict = 1;
                withdraw_immediately = 1;
            }
        }
    }

/*     avahi_log_debug("ours=%i conflict=%i", ours, conflict); */

    if (!ours && conflict) {
        char *t;
 
        valid = 0;

        t = avahi_record_to_string(record); 
 
        if (withdraw_immediately) {
            avahi_log_debug("Recieved conflicting record [%s] with local record to be. Withdrawing.", t);
            withdraw_rrset(s, record->key);
        } else {
            assert(conflicting_entry);
            avahi_log_debug("Recieved conflicting record [%s]. Resetting our record.", t);
            avahi_entry_return_to_initial_state(s, conflicting_entry, i);

            /* Local unique records are returned to probing
             * state. Local shared records are reannounced. */
        }

        avahi_free(t);
    }

    return valid;
}

static void append_aux_callback(AvahiServer *s, AvahiRecord *r, int flush_cache, void* userdata) {
    int *unicast_response = userdata;

    assert(s);
    assert(r);
    assert(unicast_response);
    
    avahi_record_list_push(s->record_list, r, flush_cache, *unicast_response, 1);
}

static void append_aux_records_to_list(AvahiServer *s, AvahiInterface *i, AvahiRecord *r, int unicast_response) {
    assert(s);
    assert(r);

    avahi_server_enumerate_aux_records(s, i, r, append_aux_callback, &unicast_response);
}

void avahi_server_generate_response(AvahiServer *s, AvahiInterface *i, AvahiDnsPacket *p, const AvahiAddress *a, uint16_t port, int legacy_unicast, int immediately) {

    assert(s);
    assert(i);
    assert(!legacy_unicast || (a && port > 0 && p));

    if (legacy_unicast) {
        AvahiDnsPacket *reply;
        AvahiRecord *r;

        if (!(reply = avahi_dns_packet_new_reply(p, 512 /* unicast DNS maximum packet size is 512 */ , 1, 1)))
            return; /* OOM */
        
        while ((r = avahi_record_list_next(s->record_list, NULL, NULL, NULL))) {

            append_aux_records_to_list(s, i, r, 0);
            
            if (avahi_dns_packet_append_record(reply, r, 0, 10))
                avahi_dns_packet_inc_field(reply, AVAHI_DNS_FIELD_ANCOUNT);
            else {
                char *t = avahi_record_to_string(r);
                avahi_log_warn("Record [%s] not fitting in legacy unicast packet, dropping.", t);
                avahi_free(t);
            }

            avahi_record_unref(r);
        }

        if (avahi_dns_packet_get_field(reply, AVAHI_DNS_FIELD_ANCOUNT) != 0)
            avahi_interface_send_packet_unicast(i, reply, a, port);

        avahi_dns_packet_free(reply);

    } else {
        int unicast_response, flush_cache, auxiliary;
        AvahiDnsPacket *reply = NULL;
        AvahiRecord *r;

        /* In case the query packet was truncated never respond
        immediately, because known answer suppression records might be
        contained in later packets */
        int tc = p && !!(avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_FLAGS) & AVAHI_DNS_FLAG_TC);
        
        while ((r = avahi_record_list_next(s->record_list, &flush_cache, &unicast_response, &auxiliary))) {
                        
            if (!avahi_interface_post_response(i, r, flush_cache, a, immediately || (flush_cache && !tc && !auxiliary)) && unicast_response) {

                append_aux_records_to_list(s, i, r, unicast_response);
                
                /* Due to some reasons the record has not been scheduled.
                 * The client requested an unicast response in that
                 * case. Therefore we prepare such a response */

                for (;;) {
                
                    if (!reply) {
                        assert(p);

                        if (!(reply = avahi_dns_packet_new_reply(p, i->hardware->mtu, 0, 0)))
                            break; /* OOM */
                    }
                
                    if (avahi_dns_packet_append_record(reply, r, flush_cache, 0)) {

                        /* Appending this record succeeded, so incremeant
                         * the specific header field, and return to the caller */
                        
                        avahi_dns_packet_inc_field(reply, AVAHI_DNS_FIELD_ANCOUNT);

                        break;
                    }

                    if (avahi_dns_packet_get_field(reply, AVAHI_DNS_FIELD_ANCOUNT) == 0) {
                        size_t size;

                        /* The record is too large for one packet, so create a larger packet */

                        avahi_dns_packet_free(reply);
                        size = avahi_record_get_estimate_size(r) + AVAHI_DNS_PACKET_HEADER_SIZE;
                        if (size > AVAHI_DNS_PACKET_MAX_SIZE)
                            size = AVAHI_DNS_PACKET_MAX_SIZE;

                        if (!(reply = avahi_dns_packet_new_reply(p, size, 0, 1)))
                            break; /* OOM */

                        if (!avahi_dns_packet_append_record(reply, r, flush_cache, 0)) {
                            char *t;
                            avahi_dns_packet_free(reply);
                            t = avahi_record_to_string(r);
                            avahi_log_warn("Record [%s] too large, doesn't fit in any packet!", t);
                            avahi_free(t);
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


static void reflect_response(AvahiServer *s, AvahiInterface *i, AvahiRecord *r, int flush_cache) {
    AvahiInterface *j;
    
    assert(s);
    assert(i);
    assert(r);

    if (!s->config.enable_reflector)
        return;

    for (j = s->monitor->interfaces; j; j = j->interface_next)
        if (j != i && (s->config.reflect_ipv || j->protocol == i->protocol))
            avahi_interface_post_response(j, r, flush_cache, NULL, 1);
}

static void* reflect_cache_walk_callback(AvahiCache *c, AvahiKey *pattern, AvahiCacheEntry *e, void* userdata) {
    AvahiServer *s = userdata;

    assert(c);
    assert(pattern);
    assert(e);
    assert(s);

    avahi_record_list_push(s->record_list, e->record, e->cache_flush, 0, 0);
    return NULL;
}

static void reflect_query(AvahiServer *s, AvahiInterface *i, AvahiKey *k) {
    AvahiInterface *j;
    
    assert(s);
    assert(i);
    assert(k);

    if (!s->config.enable_reflector)
        return;

    for (j = s->monitor->interfaces; j; j = j->interface_next)
        if (j != i && (s->config.reflect_ipv || j->protocol == i->protocol)) {
            /* Post the query to other networks */
            avahi_interface_post_query(j, k, 1);

            /* Reply from caches of other network. This is needed to
             * "work around" known answer suppression. */

            avahi_cache_walk(j->cache, k, reflect_cache_walk_callback, s);
        }
}

static void reflect_probe(AvahiServer *s, AvahiInterface *i, AvahiRecord *r) {
    AvahiInterface *j;
    
    assert(s);
    assert(i);
    assert(r);

    if (!s->config.enable_reflector)
        return;

    for (j = s->monitor->interfaces; j; j = j->interface_next)
        if (j != i && (s->config.reflect_ipv || j->protocol == i->protocol))
            avahi_interface_post_probe(j, r, 1);
}

static void handle_query_packet(AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *a, uint16_t port, int legacy_unicast, int from_local_iface) {
    size_t n;
    int is_probe;
    
    assert(s);
    assert(p);
    assert(i);
    assert(a);

/*     avahi_log_debug("query"); */

    assert(avahi_record_list_is_empty(s->record_list));

    is_probe = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT) > 0;
    
    /* Handle the questions */
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT); n > 0; n --) {
        AvahiKey *key;
        int unicast_response = 0;

        if (!(key = avahi_dns_packet_consume_key(p, &unicast_response))) {
            avahi_log_warn("Packet too short (1)");
            goto fail;
        }

        if (!legacy_unicast && !from_local_iface)
            reflect_query(s, i, key);

        if (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) == 0 &&
            !(avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_FLAGS) & AVAHI_DNS_FLAG_TC))
            /* Allow our own queries to be suppressed by incoming
             * queries only when they do not include known answers */
            avahi_query_scheduler_incoming(i->query_scheduler, key);
        
        avahi_server_prepare_matching_responses(s, i, key, unicast_response);
        avahi_key_unref(key);
    }

    if (!legacy_unicast) {

        /* Known Answer Suppression */
        for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT); n > 0; n --) {
            AvahiRecord *record;
            int unique = 0;
            
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
            int unique = 0;
            
            if (!(record = avahi_dns_packet_consume_record(p, &unique))) {
                avahi_log_warn("Packet too short (3)");
                goto fail;
            }
            
            if (!avahi_key_is_pattern(record->key)) {
                if (!from_local_iface)
                    reflect_probe(s, i, record);
                incoming_probe(s, record, i);
            }
            
            avahi_record_unref(record);
        }
    }

    if (!avahi_record_list_is_empty(s->record_list))
        avahi_server_generate_response(s, i, p, a, port, legacy_unicast, is_probe);

    return;
    
fail:
    avahi_record_list_flush(s->record_list);
}

static void handle_response_packet(AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *a, int from_local_iface) {
    unsigned n;
    
    assert(s);
    assert(p);
    assert(i);
    assert(a);

/*     avahi_log_debug("response"); */
    
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) +
             avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ARCOUNT); n > 0; n--) {
        AvahiRecord *record;
        int cache_flush = 0;
/*         char *txt; */
        
        if (!(record = avahi_dns_packet_consume_record(p, &cache_flush))) {
            avahi_log_warn("Packet too short (4)");
            break;
        }

        if (!avahi_key_is_pattern(record->key)) {

/*             avahi_log_debug("Handling response: %s", txt = avahi_record_to_string(record)); */
/*             avahi_free(txt); */
            
            if (handle_conflict(s, i, record, cache_flush, a)) {
                if (!from_local_iface)
                    reflect_response(s, i, record, cache_flush);
                avahi_cache_update(i->cache, record, cache_flush, a);
                avahi_response_scheduler_incoming(i->response_scheduler, record, cache_flush);
            }
        }
            
        avahi_record_unref(record);
    }

    /* If the incoming response contained a conflicting record, some
       records have been scheduling for sending. We need to flush them
       here. */
    if (!avahi_record_list_is_empty(s->record_list))
        avahi_server_generate_response(s, i, NULL, NULL, 0, 0, 1);
}

static AvahiLegacyUnicastReflectSlot* allocate_slot(AvahiServer *s) {
    unsigned n, idx = (unsigned) -1;
    AvahiLegacyUnicastReflectSlot *slot;
    
    assert(s);

    if (!s->legacy_unicast_reflect_slots)
        s->legacy_unicast_reflect_slots = avahi_new0(AvahiLegacyUnicastReflectSlot*, AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS);

    for (n = 0; n < AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS; n++, s->legacy_unicast_reflect_id++) {
        idx = s->legacy_unicast_reflect_id % AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS;
        
        if (!s->legacy_unicast_reflect_slots[idx])
            break;
    }

    if (idx == (unsigned) -1 || s->legacy_unicast_reflect_slots[idx])
        return NULL;

    if (!(slot = avahi_new(AvahiLegacyUnicastReflectSlot, 1)))
        return NULL; /* OOM */

    s->legacy_unicast_reflect_slots[idx] = slot;
    slot->id = s->legacy_unicast_reflect_id++;
    slot->server = s;
    
    return slot;
}

static void deallocate_slot(AvahiServer *s, AvahiLegacyUnicastReflectSlot *slot) {
    unsigned idx;

    assert(s);
    assert(slot);

    idx = slot->id % AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS;

    assert(s->legacy_unicast_reflect_slots[idx] == slot);

    avahi_time_event_free(slot->time_event);
    
    avahi_free(slot);
    s->legacy_unicast_reflect_slots[idx] = NULL;
}

static void free_slots(AvahiServer *s) {
    unsigned idx;
    assert(s);

    if (!s->legacy_unicast_reflect_slots)
        return;

    for (idx = 0; idx < AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS; idx ++)
        if (s->legacy_unicast_reflect_slots[idx])
            deallocate_slot(s, s->legacy_unicast_reflect_slots[idx]);

    avahi_free(s->legacy_unicast_reflect_slots);
    s->legacy_unicast_reflect_slots = NULL;
}

static AvahiLegacyUnicastReflectSlot* find_slot(AvahiServer *s, uint16_t id) {
    unsigned idx;
    
    assert(s);

    if (!s->legacy_unicast_reflect_slots)
        return NULL;
    
    idx = id % AVAHI_MAX_LEGACY_UNICAST_REFLECT_SLOTS;

    if (!s->legacy_unicast_reflect_slots[idx] || s->legacy_unicast_reflect_slots[idx]->id != id)
        return NULL;

    return s->legacy_unicast_reflect_slots[idx];
}

static void legacy_unicast_reflect_slot_timeout(AvahiTimeEvent *e, void *userdata) {
    AvahiLegacyUnicastReflectSlot *slot = userdata;

    assert(e);
    assert(slot);
    assert(slot->time_event == e);

    deallocate_slot(slot->server, slot);
}

static void reflect_legacy_unicast_query_packet(AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *a, uint16_t port) {
    AvahiLegacyUnicastReflectSlot *slot;
    AvahiInterface *j;

    assert(s);
    assert(p);
    assert(i);
    assert(a);
    assert(port > 0);
    assert(i->protocol == a->family);
    
    if (!s->config.enable_reflector)
        return;

/*     avahi_log_debug("legacy unicast reflector"); */
    
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
    slot->time_event = avahi_time_event_new(s->time_event_queue, &slot->elapse_time, legacy_unicast_reflect_slot_timeout, slot);

    /* Patch the packet with our new locally generatedt id */
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ID, slot->id);
    
    for (j = s->monitor->interfaces; j; j = j->interface_next)
        if (avahi_interface_relevant(j) &&
            j != i &&
            (s->config.reflect_ipv || j->protocol == i->protocol)) {

            if (j->protocol == AVAHI_PROTO_INET && s->fd_legacy_unicast_ipv4 >= 0) {
                avahi_send_dns_packet_ipv4(s->fd_legacy_unicast_ipv4, j->hardware->index, p, NULL, 0);
                } else if (j->protocol == AVAHI_PROTO_INET6 && s->fd_legacy_unicast_ipv6 >= 0)
                avahi_send_dns_packet_ipv6(s->fd_legacy_unicast_ipv6, j->hardware->index, p, NULL, 0);
        }

    /* Reset the id */
    avahi_dns_packet_set_field(p, AVAHI_DNS_FIELD_ID, slot->original_id);
}

static int originates_from_local_legacy_unicast_socket(AvahiServer *s, const struct sockaddr *sa) {
    AvahiAddress a;
    assert(s);
    assert(sa);

    if (!s->config.enable_reflector)
        return 0;
    
    avahi_address_from_sockaddr(sa, &a);

    if (!avahi_address_is_local(s->monitor, &a))
        return 0;
    
    if (sa->sa_family == AF_INET && s->fd_legacy_unicast_ipv4 >= 0) {
        struct sockaddr_in lsa;
        socklen_t l = sizeof(lsa);
        
        if (getsockname(s->fd_legacy_unicast_ipv4, &lsa, &l) != 0)
            avahi_log_warn("getsockname(): %s", strerror(errno));
        else
            return lsa.sin_port == ((const struct sockaddr_in*) sa)->sin_port;

    }

    if (sa->sa_family == AF_INET6 && s->fd_legacy_unicast_ipv6 >= 0) {
        struct sockaddr_in6 lsa;
        socklen_t l = sizeof(lsa);

        if (getsockname(s->fd_legacy_unicast_ipv6, &lsa, &l) != 0)
            avahi_log_warn("getsockname(): %s", strerror(errno));
        else
            return lsa.sin6_port == ((const struct sockaddr_in6*) sa)->sin6_port;
    }

    return 0;
}

static int is_mdns_mcast_address(const AvahiAddress *a) {
    AvahiAddress b;
    assert(a);

    avahi_address_parse(a->family == AVAHI_PROTO_INET ? AVAHI_IPV4_MCAST_GROUP : AVAHI_IPV6_MCAST_GROUP, a->family, &b);
    return avahi_address_cmp(a, &b) == 0;
}

static int originates_from_local_iface(AvahiServer *s, AvahiIfIndex iface, const AvahiAddress *a, uint16_t port) {
    assert(s);
    assert(iface != AVAHI_IF_UNSPEC);
    assert(a);

    /* If it isn't the MDNS port it can't be generated by us */
    if (port != AVAHI_MDNS_PORT)
        return 0;

    return avahi_interface_has_address(s->monitor, iface, a);
}

static void dispatch_packet(AvahiServer *s, AvahiDnsPacket *p, const struct sockaddr *sa, AvahiAddress *dest, AvahiIfIndex iface, int ttl) {
    AvahiInterface *i;
    AvahiAddress a;
    uint16_t port;
    int from_local_iface = 0;
    
    assert(s);
    assert(p);
    assert(sa);
    assert(dest);
    assert(iface > 0);

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

    /* We don't want to reflect local traffic, so we check if this packet is generated locally. */
    if (s->config.enable_reflector)
        from_local_iface = originates_from_local_iface(s, iface, &a, port);

    if (avahi_dns_packet_is_valid(p) < 0) {
        avahi_log_warn("Recieved invalid packet.");
        return;
    }

    if (avahi_dns_packet_is_query(p)) {
        int legacy_unicast = 0;

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
        
            legacy_unicast = 1;
        }

        if (legacy_unicast)
            reflect_legacy_unicast_query_packet(s, p, i, &a, port);
        
        handle_query_packet(s, p, i, &a, port, legacy_unicast, from_local_iface);
        
/*         avahi_log_debug("Handled query"); */
    } else {
        if (port != AVAHI_MDNS_PORT) {
            avahi_log_warn("Recieved repsonse with invalid source port %u on interface '%s.%i'", port, i->hardware->name, i->protocol);
            return;
        }

        if (ttl != 255 && s->config.check_response_ttl) {
            avahi_log_warn("Recieved response with invalid TTL %u on interface '%s.%i'.", ttl, i->hardware->name, i->protocol);
            return;
        }

        if (!is_mdns_mcast_address(dest) &&
            !avahi_interface_address_on_link(i, &a)) {
            avahi_log_warn("Recivied non-local response on interface '%s.%i'.", i->hardware->name, i->protocol);
            return;
        }
        
        if (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT) != 0 ||
            avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) == 0 ||
            avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT) != 0) {
            avahi_log_warn("Invalid response packet.");
            return;
        }

        handle_response_packet(s, p, i, &a, from_local_iface);
/*         avahi_log_debug("Handled response"); */
    }
}

static void dispatch_legacy_unicast_packet(AvahiServer *s, AvahiDnsPacket *p, const struct sockaddr *sa, AvahiIfIndex iface, int ttl) {
    AvahiInterface *i, *j;
    AvahiAddress a;
    uint16_t port;
    AvahiLegacyUnicastReflectSlot *slot;
    
    assert(s);
    assert(p);
    assert(sa);
    assert(iface > 0);

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

    if (avahi_dns_packet_is_valid(p) < 0 || avahi_dns_packet_is_query(p)) {
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

static void socket_event(AvahiWatch *w, int fd, AvahiWatchEvent events, void *userdata) {
    AvahiServer *s = userdata;
    AvahiAddress dest;
    AvahiDnsPacket *p;
    AvahiIfIndex iface;
    uint8_t ttl;
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;

    assert(w);
    assert(fd >= 0);

    if (events & AVAHI_WATCH_IN) {
    
        if (fd == s->fd_ipv4) {
            dest.family = AVAHI_PROTO_INET;
            if ((p = avahi_recv_dns_packet_ipv4(s->fd_ipv4, &sa, &dest.data.ipv4, &iface, &ttl))) {
                dispatch_packet(s, p, (struct sockaddr*) &sa, &dest, iface, ttl);
                avahi_dns_packet_free(p);
            }
        } else if (fd == s->fd_ipv6) {
            dest.family = AVAHI_PROTO_INET6;

            if ((p = avahi_recv_dns_packet_ipv6(s->fd_ipv6, &sa6, &dest.data.ipv6, &iface, &ttl))) {
                dispatch_packet(s, p, (struct sockaddr*) &sa6, &dest, iface, ttl);
                avahi_dns_packet_free(p);
            }
        } else if (fd == s->fd_legacy_unicast_ipv4) {
            dest.family = AVAHI_PROTO_INET;
            
            if ((p = avahi_recv_dns_packet_ipv4(s->fd_legacy_unicast_ipv4, &sa, &dest.data.ipv4, &iface, &ttl))) {
                dispatch_legacy_unicast_packet(s, p, (struct sockaddr*) &sa, iface, ttl);
                avahi_dns_packet_free(p);
            }
        } else if (fd == s->fd_legacy_unicast_ipv6) {
            dest.family = AVAHI_PROTO_INET6;
            
            if ((p = avahi_recv_dns_packet_ipv6(s->fd_legacy_unicast_ipv6, &sa6, &dest.data.ipv6, &iface, &ttl))) {
                dispatch_legacy_unicast_packet(s, p, (struct sockaddr*) &sa6, iface, ttl);
                avahi_dns_packet_free(p);
            }
        }

        cleanup_dead(s);
    } else {
        assert(0);
    }
}

static void server_set_state(AvahiServer *s, AvahiServerState state) {
    assert(s);

    if (s->state == state)
        return;
    
    s->state = state;

    if (s->callback)
        s->callback(s, state, s->userdata);
}

static void withdraw_host_rrs(AvahiServer *s) {
    assert(s);

    if (s->hinfo_entry_group)
        avahi_s_entry_group_reset(s->hinfo_entry_group);

    if (s->browse_domain_entry_group)
        avahi_s_entry_group_reset(s->browse_domain_entry_group);

    avahi_update_host_rrs(s->monitor, 1);
    s->n_host_rr_pending = 0;
}

void avahi_server_decrease_host_rr_pending(AvahiServer *s) {
    assert(s);
    
    assert(s->n_host_rr_pending > 0);

    if (--s->n_host_rr_pending == 0)
        server_set_state(s, AVAHI_SERVER_RUNNING);
}

void avahi_server_increase_host_rr_pending(AvahiServer *s) {
    assert(s);

    s->n_host_rr_pending ++;
}

void avahi_host_rr_entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state, void *userdata) {
    assert(s);
    assert(g);

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
    
    assert(s);
    
    if (!s->config.publish_hinfo)
        return;

    if (s->hinfo_entry_group)
        assert(avahi_s_entry_group_is_empty(s->hinfo_entry_group));
    else
        s->hinfo_entry_group = avahi_s_entry_group_new(s, avahi_host_rr_entry_group_callback, NULL);

    if (!s->hinfo_entry_group) {
        avahi_log_warn("Failed to create HINFO entry group: %s", avahi_strerror(s->error));
        return;
    }
    
    /* Fill in HINFO rr */
    if ((r = avahi_record_new_full(s->host_name_fqdn, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_HINFO, AVAHI_DEFAULT_TTL_HOST_NAME))) {
        uname(&utsname);
        r->data.hinfo.cpu = avahi_strdup(avahi_strup(utsname.machine));
        r->data.hinfo.os = avahi_strdup(avahi_strup(utsname.sysname));

        if (avahi_server_add(s, s->hinfo_entry_group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, AVAHI_ENTRY_UNIQUE, r) < 0) {
            avahi_log_warn("Failed to add HINFO RR: %s", avahi_strerror(s->error));
            return;
        }

        avahi_record_unref(r);
    }

    if (avahi_s_entry_group_commit(s->hinfo_entry_group) < 0)
        avahi_log_warn("Failed to commit HINFO entry group: %s", avahi_strerror(s->error));

}

static void register_localhost(AvahiServer *s) {
    AvahiAddress a;
    assert(s);
    
    /* Add localhost entries */
    avahi_address_parse("127.0.0.1", AVAHI_PROTO_INET, &a);
    avahi_server_add_address(s, NULL, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, AVAHI_ENTRY_NOPROBE|AVAHI_ENTRY_NOANNOUNCE, "localhost", &a);

    avahi_address_parse("::1", AVAHI_PROTO_INET6, &a);
    avahi_server_add_address(s, NULL, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, AVAHI_ENTRY_NOPROBE|AVAHI_ENTRY_NOANNOUNCE, "ip6-localhost", &a);
}

static void register_browse_domain(AvahiServer *s) {
    assert(s);

    if (!s->config.publish_domain)
        return;

    if (s->browse_domain_entry_group)
        assert(avahi_s_entry_group_is_empty(s->browse_domain_entry_group));
    else
        s->browse_domain_entry_group = avahi_s_entry_group_new(s, NULL, NULL);

    if (!s->browse_domain_entry_group) {
        avahi_log_warn("Failed to create browse domain entry group: %s", avahi_strerror(s->error));
        return;
    }
    
    if (avahi_server_add_ptr(s, s->browse_domain_entry_group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, AVAHI_DEFAULT_TTL, "b._dns-sd._udp.local", s->domain_name) < 0) {
        avahi_log_warn("Failed to add browse domain RR: %s", avahi_strerror(s->error));
        return;
    }

    if (avahi_s_entry_group_commit(s->browse_domain_entry_group) < 0)
        avahi_log_warn("Failed to commit browse domain entry group: %s", avahi_strerror(s->error));
}

static void register_stuff(AvahiServer *s) {
    assert(s);

    server_set_state(s, AVAHI_SERVER_REGISTERING);
    s->n_host_rr_pending ++;  /** Make sure that the state isn't changed tp AVAHI_SERVER_RUNNING too early */

    register_hinfo(s);
    register_browse_domain(s);
    avahi_update_host_rrs(s->monitor, 0);

    s->n_host_rr_pending --;
    
    if (s->n_host_rr_pending == 0)
        server_set_state(s, AVAHI_SERVER_RUNNING);
}

static void update_fqdn(AvahiServer *s) {
    char *n;
    
    assert(s);
    assert(s->host_name);
    assert(s->domain_name);

    if (!(n = avahi_strdup_printf("%s.%s", s->host_name, s->domain_name)))
        return; /* OOM */

    avahi_free(s->host_name_fqdn);
    s->host_name_fqdn = n;
}

int avahi_server_set_host_name(AvahiServer *s, const char *host_name) {
    assert(s);
    assert(host_name);

    if (host_name && !avahi_is_valid_host_name(host_name))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_HOST_NAME);

    withdraw_host_rrs(s);

    avahi_free(s->host_name);
    s->host_name = host_name ? avahi_normalize_name(host_name) : avahi_get_host_name();
    s->host_name[strcspn(s->host_name, ".")] = 0;
    update_fqdn(s);

    register_stuff(s);
    return AVAHI_OK;
}

int avahi_server_set_domain_name(AvahiServer *s, const char *domain_name) {
    assert(s);
    assert(domain_name);

    if (domain_name && !avahi_is_valid_domain_name(domain_name))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_DOMAIN_NAME);

    withdraw_host_rrs(s);

    avahi_free(s->domain_name);
    s->domain_name = domain_name ? avahi_normalize_name(domain_name) : avahi_strdup("local");
    update_fqdn(s);

    register_stuff(s);
    return AVAHI_OK;
}

static int valid_server_config(const AvahiServerConfig *sc) {

    if (sc->host_name && !avahi_is_valid_host_name(sc->host_name))
        return AVAHI_ERR_INVALID_HOST_NAME;
    
    if (sc->domain_name && !avahi_is_valid_domain_name(sc->domain_name))
        return AVAHI_ERR_INVALID_DOMAIN_NAME;

    return AVAHI_OK;
}

static int setup_sockets(AvahiServer *s) {
    assert(s);
    
    s->fd_ipv4 = s->config.use_ipv4 ? avahi_open_socket_ipv4() : -1;
    s->fd_ipv6 = s->config.use_ipv6 ? avahi_open_socket_ipv6() : -1;
    
    if (s->fd_ipv6 < 0 && s->fd_ipv4 < 0)
        return AVAHI_ERR_NO_NETWORK;

    if (s->fd_ipv4 < 0 && s->config.use_ipv4)
        avahi_log_notice("Failed to create IPv4 socket, proceeding in IPv6 only mode");
    else if (s->fd_ipv6 < 0 && s->config.use_ipv6)
        avahi_log_notice("Failed to create IPv6 socket, proceeding in IPv4 only mode");

    s->fd_legacy_unicast_ipv4 = s->fd_ipv4 >= 0 && s->config.enable_reflector ? avahi_open_legacy_unicast_socket_ipv4() : -1;
    s->fd_legacy_unicast_ipv6 = s->fd_ipv6 >= 0 && s->config.enable_reflector ? avahi_open_legacy_unicast_socket_ipv6() : -1;

    s->watch_ipv4 = s->watch_ipv6 = s->watch_legacy_unicast_ipv4 = s->watch_legacy_unicast_ipv6 = NULL;
    
    if (s->fd_ipv4 >= 0)
        s->watch_ipv4 = s->poll_api->watch_new(s->poll_api, s->fd_ipv4, AVAHI_WATCH_IN, socket_event, s);
    if (s->fd_ipv6 >= 0)
        s->watch_ipv6 = s->poll_api->watch_new(s->poll_api, s->fd_ipv6, AVAHI_WATCH_IN, socket_event, s);
    if (s->fd_legacy_unicast_ipv4 >= 0)
        s->watch_legacy_unicast_ipv4 = s->poll_api->watch_new(s->poll_api, s->fd_legacy_unicast_ipv4, AVAHI_WATCH_IN, socket_event, s);
    if (s->fd_legacy_unicast_ipv6 >= 0)
        s->watch_legacy_unicast_ipv6 = s->poll_api->watch_new(s->poll_api, s->fd_legacy_unicast_ipv6, AVAHI_WATCH_IN, socket_event, s);

    return 0;
}

AvahiServer *avahi_server_new(const AvahiPoll *poll_api, const AvahiServerConfig *sc, AvahiServerCallback callback, void* userdata, int *error) {
    AvahiServer *s;
    int e;
    
    if ((e = valid_server_config(sc)) < 0) {
        if (error)
            *error = e;
        return NULL;
    }
    
    if (!(s = avahi_new(AvahiServer, 1))) {
        if (error)
            *error = AVAHI_ERR_NO_MEMORY;

        return NULL;
    }

    s->poll_api = poll_api;

    if (sc)
        avahi_server_config_copy(&s->config, sc);
    else
        avahi_server_config_init(&s->config);

    if ((e = setup_sockets(s)) < 0) {
        if (error)
            *error = e;

        avahi_server_config_free(&s->config);
        avahi_free(s);
        
        return NULL;
    }

    s->n_host_rr_pending = 0;
    s->need_entry_cleanup = 0;
    s->need_group_cleanup = 0;
    s->need_browser_cleanup = 0;
    
    s->time_event_queue = avahi_time_event_queue_new(poll_api);
    
    s->callback = callback;
    s->userdata = userdata;
    
    s->entries_by_key = avahi_hashmap_new((AvahiHashFunc) avahi_key_hash, (AvahiEqualFunc) avahi_key_equal, NULL, NULL);
    AVAHI_LLIST_HEAD_INIT(AvahiEntry, s->entries);
    AVAHI_LLIST_HEAD_INIT(AvahiGroup, s->groups);

    s->record_browser_hashmap = avahi_hashmap_new((AvahiHashFunc) avahi_key_hash, (AvahiEqualFunc) avahi_key_equal, NULL, NULL);
    AVAHI_LLIST_HEAD_INIT(AvahiSRecordBrowser, s->record_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiSHostNameResolver, s->host_name_resolvers);
    AVAHI_LLIST_HEAD_INIT(AvahiSAddressResolver, s->address_resolvers);
    AVAHI_LLIST_HEAD_INIT(AvahiSDomainBrowser, s->domain_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiSServiceTypeBrowser, s->service_type_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiSServiceBrowser, s->service_browsers);
    AVAHI_LLIST_HEAD_INIT(AvahiSServiceResolver, s->service_resolvers);
    AVAHI_LLIST_HEAD_INIT(AvahiSDNSServerBrowser, s->dns_server_browsers);

    s->legacy_unicast_reflect_slots = NULL;
    s->legacy_unicast_reflect_id = 0;
    
    /* Get host name */
    s->host_name = s->config.host_name ? avahi_normalize_name(s->config.host_name) : avahi_get_host_name();
    s->host_name[strcspn(s->host_name, ".")] = 0;
    s->domain_name = s->config.domain_name ? avahi_normalize_name(s->config.domain_name) : avahi_strdup("local");
    s->host_name_fqdn = NULL;
    update_fqdn(s);

    s->record_list = avahi_record_list_new();

    s->state = AVAHI_SERVER_INVALID;

    s->monitor = avahi_interface_monitor_new(s);
    avahi_interface_monitor_sync(s->monitor);

    register_localhost(s);

    s->hinfo_entry_group = NULL;
    s->browse_domain_entry_group = NULL;
    register_stuff(s);

    s->error = AVAHI_OK;
    
    return s;
}

void avahi_server_free(AvahiServer* s) {
    assert(s);

    while(s->entries)
        free_entry(s, s->entries);

    avahi_interface_monitor_free(s->monitor);

    while (s->groups)
        free_group(s, s->groups);

    free_slots(s);

    while (s->dns_server_browsers)
        avahi_s_dns_server_browser_free(s->dns_server_browsers);
    while (s->host_name_resolvers)
        avahi_s_host_name_resolver_free(s->host_name_resolvers);
    while (s->address_resolvers)
        avahi_s_address_resolver_free(s->address_resolvers);
    while (s->domain_browsers)
        avahi_s_domain_browser_free(s->domain_browsers);
    while (s->service_type_browsers)
        avahi_s_service_type_browser_free(s->service_type_browsers);
    while (s->service_browsers)
        avahi_s_service_browser_free(s->service_browsers);
    while (s->service_resolvers)
        avahi_s_service_resolver_free(s->service_resolvers);
    while (s->record_browsers)
        avahi_s_record_browser_destroy(s->record_browsers);
    
    avahi_hashmap_free(s->record_browser_hashmap);
    avahi_hashmap_free(s->entries_by_key);

    avahi_time_event_queue_free(s->time_event_queue);

    avahi_record_list_free(s->record_list);

    if (s->watch_ipv4)
        s->poll_api->watch_free(s->watch_ipv4);
    if (s->watch_ipv6)
        s->poll_api->watch_free(s->watch_ipv6);
    if (s->watch_legacy_unicast_ipv4)
        s->poll_api->watch_free(s->watch_legacy_unicast_ipv4);
    if (s->watch_legacy_unicast_ipv6)
        s->poll_api->watch_free(s->watch_legacy_unicast_ipv6);
    
    if (s->fd_ipv4 >= 0)
        close(s->fd_ipv4);
    if (s->fd_ipv6 >= 0)
        close(s->fd_ipv6);
    if (s->fd_legacy_unicast_ipv4 >= 0)
        close(s->fd_legacy_unicast_ipv4);
    if (s->fd_legacy_unicast_ipv6 >= 0)
        close(s->fd_legacy_unicast_ipv6);

    avahi_free(s->host_name);
    avahi_free(s->domain_name);
    avahi_free(s->host_name_fqdn);

    avahi_server_config_free(&s->config);

    avahi_free(s);
}

static int check_record_conflict(AvahiServer *s, AvahiIfIndex interface, AvahiProtocol protocol, AvahiRecord *r, AvahiEntryFlags flags) {
    AvahiEntry *e;
    
    assert(s);
    assert(r);

    for (e = avahi_hashmap_lookup(s->entries_by_key, r->key); e; e = e->by_key_next) {
        if (e->dead)
            continue;

        if (!(flags & AVAHI_ENTRY_UNIQUE) && !(e->flags & AVAHI_ENTRY_UNIQUE))
            continue;
        
        if ((flags & AVAHI_ENTRY_ALLOWMUTIPLE) && (e->flags & AVAHI_ENTRY_ALLOWMUTIPLE) )
            continue;

        if ((interface <= 0 ||
             e->interface <= 0 ||
             e->interface == interface) &&
            (protocol == AVAHI_PROTO_UNSPEC ||
             e->protocol == AVAHI_PROTO_UNSPEC ||
             e->protocol == protocol))

            return -1;
    }

    return 0;
}

int avahi_server_add(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    AvahiRecord *r) {
    
    AvahiEntry *e, *t;
    
    assert(s);
    assert(r);

    if (r->ttl == 0)
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_TTL);

    if (avahi_key_is_pattern(r->key))
        return avahi_server_set_errno(s, AVAHI_ERR_IS_PATTERN);

    if (!avahi_record_is_valid(r))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_RECORD);

    if (check_record_conflict(s, interface, protocol, r, flags) < 0)
        return avahi_server_set_errno(s, AVAHI_ERR_LOCAL_COLLISION);

    if (!(e = avahi_new(AvahiEntry, 1)))
        return avahi_server_set_errno(s, AVAHI_ERR_NO_NETWORK);
        
    e->server = s;
    e->record = avahi_record_ref(r);
    e->group = g;
    e->interface = interface;
    e->protocol = protocol;
    e->flags = flags;
    e->dead = 0;

    AVAHI_LLIST_HEAD_INIT(AvahiAnnouncement, e->announcements);

    AVAHI_LLIST_PREPEND(AvahiEntry, entries, s->entries, e);

    /* Insert into hash table indexed by name */
    t = avahi_hashmap_lookup(s->entries_by_key, e->record->key);
    AVAHI_LLIST_PREPEND(AvahiEntry, by_key, t, e);
    avahi_hashmap_replace(s->entries_by_key, e->record->key, t);

    /* Insert into group list */
    if (g)
        AVAHI_LLIST_PREPEND(AvahiEntry, by_group, g->entries, e); 

    avahi_announce_entry(s, e);

    return 0;
}

const AvahiRecord *avahi_server_iterate(AvahiServer *s, AvahiSEntryGroup *g, void **state) {
    AvahiEntry **e = (AvahiEntry**) state;
    assert(s);
    assert(e);

    if (!*e)
        *e = g ? g->entries : s->entries;
    
    while (*e && (*e)->dead)
        *e = g ? (*e)->by_group_next : (*e)->entries_next;
        
    if (!*e)
        return NULL;

    return avahi_record_ref((*e)->record);
}

int avahi_server_dump(AvahiServer *s, AvahiDumpCallback callback, void* userdata) {
    AvahiEntry *e;
    
    assert(s);
    assert(callback);

    callback(";;; ZONE DUMP FOLLOWS ;;;", userdata);

    for (e = s->entries; e; e = e->entries_next) {
        char *t;
        char ln[256];

        if (e->dead)
            continue;
        
        if (!(t = avahi_record_to_string(e->record)))
            return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        
        snprintf(ln, sizeof(ln), "%s ; iface=%i proto=%i", t, e->interface, e->protocol);
        avahi_free(t);

        callback(ln, userdata);
    }

    avahi_dump_caches(s->monitor, callback, userdata);
    return AVAHI_OK;
}

int avahi_server_add_ptr(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    uint32_t ttl,
    const char *name,
    const char *dest) {

    AvahiRecord *r;
    int ret;

    assert(s);
    assert(dest);

    if (!(r = avahi_record_new_full(name ? name : s->host_name_fqdn, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR, ttl)))
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        
    r->data.ptr.name = avahi_normalize_name(dest);
    ret = avahi_server_add(s, g, interface, protocol, flags, r);
    avahi_record_unref(r);
    return ret;
}

int avahi_server_add_address(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    const char *name,
    AvahiAddress *a) {

    char *n = NULL;
    int ret = AVAHI_OK;
    assert(s);
    assert(a);

    if (name) {
        if (!(n = avahi_normalize_name(name)))
            return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);

        name = n;
    } else
        name = s->host_name_fqdn;

    if (!avahi_is_valid_domain_name(name)) {
        ret = avahi_server_set_errno(s, AVAHI_ERR_INVALID_HOST_NAME);
        goto fail;
    }
    
    if (a->family == AVAHI_PROTO_INET) {
        char *reverse;
        AvahiRecord  *r;

        if (!(r = avahi_record_new_full(name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A, AVAHI_DEFAULT_TTL_HOST_NAME))) {
            ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            goto fail;
        }
        
        r->data.a.address = a->data.ipv4;
        ret = avahi_server_add(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE | AVAHI_ENTRY_ALLOWMUTIPLE, r);
        avahi_record_unref(r);

        if (ret < 0)
            goto fail;
        
        if (!(reverse = avahi_reverse_lookup_name_ipv4(&a->data.ipv4))) {
            ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            goto fail;
        }

        ret = avahi_server_add_ptr(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE, AVAHI_DEFAULT_TTL_HOST_NAME, reverse, name);
        avahi_free(reverse);

    } else {
        char *reverse;
        AvahiRecord *r;

        assert(a->family == AVAHI_PROTO_INET6);
            
        if (!(r = avahi_record_new_full(name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA, AVAHI_DEFAULT_TTL_HOST_NAME))) {
            ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            goto fail;
        }
        
        r->data.aaaa.address = a->data.ipv6;
        ret = avahi_server_add(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE | AVAHI_ENTRY_ALLOWMUTIPLE, r);
        avahi_record_unref(r);

        if (ret < 0)
            goto fail;

        if (!(reverse = avahi_reverse_lookup_name_ipv6_arpa(&a->data.ipv6))) {
            ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            goto fail;
        }
            
        ret = avahi_server_add_ptr(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE, AVAHI_DEFAULT_TTL_HOST_NAME, reverse, name);
        avahi_free(reverse);

        if (ret < 0)
            goto fail;
    
        if (!(reverse = avahi_reverse_lookup_name_ipv6_int(&a->data.ipv6))) {
            ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            goto fail;
        }

        ret = avahi_server_add_ptr(s, g, interface, protocol, flags | AVAHI_ENTRY_UNIQUE, AVAHI_DEFAULT_TTL_HOST_NAME, reverse, name);
        avahi_free(reverse);
    }

fail:
    
    avahi_free(n);

    return ret;
}

static int server_add_txt_strlst_nocopy(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    uint32_t ttl,
    const char *name,
    AvahiStringList *strlst) {

    AvahiRecord *r;
    int ret;
    
    assert(s);
    
    if (!(r = avahi_record_new_full(name ? name : s->host_name_fqdn, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT, ttl)))
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
    
    r->data.txt.string_list = strlst;
    ret = avahi_server_add(s, g, interface, protocol, flags, r);
    avahi_record_unref(r);

    return ret;
}

int avahi_server_add_txt_strlst(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    uint32_t ttl,
    const char *name,
    AvahiStringList *strlst) {

    assert(s);

    return server_add_txt_strlst_nocopy(s, g, interface, protocol, flags, ttl, name, avahi_string_list_copy(strlst));
}

int avahi_server_add_txt_va(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    uint32_t ttl,
    const char *name,
    va_list va) {

    assert(s);

    return server_add_txt_strlst_nocopy(s, g, interface, protocol, flags, ttl, name, avahi_string_list_new_va(va));
}

int avahi_server_add_txt(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiEntryFlags flags,
    uint32_t ttl,
    const char *name,
    ...) {

    va_list va;
    int ret;
    
    assert(s);

    va_start(va, name);
    ret = avahi_server_add_txt_va(s, g, interface, protocol, flags, ttl, name, va);
    va_end(va);

    return ret;
}

static void escape_service_name(char *d, size_t size, const char *s) {
    assert(d);
    assert(size);
    assert(s);

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

    assert(size > 0);
    *(d++) = 0;
}

static int server_add_service_strlst_nocopy(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    AvahiStringList *strlst) {

    char ptr_name[256], svc_name[256], ename[64], enum_ptr[256];
    char *t = NULL, *d = NULL, *h = NULL;
    AvahiRecord *r = NULL;
    int ret = AVAHI_OK;
    
    assert(s);
    assert(type);
    assert(name);

    if (!avahi_is_valid_service_name(name))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_SERVICE_NAME);

    if (!avahi_is_valid_service_type(type))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_SERVICE_TYPE);

    if (domain && !avahi_is_valid_domain_name(domain))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_DOMAIN_NAME);

    if (host && !avahi_is_valid_domain_name(host))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_HOST_NAME);

    escape_service_name(ename, sizeof(ename), name);

    if (!domain)
        domain = s->domain_name;

    if (!host)
        host = s->host_name_fqdn;

    if (!(d = avahi_normalize_name(domain)) ||
        !(t = avahi_normalize_name(type)) ||
        !(h = avahi_normalize_name(host))) {
        ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    snprintf(ptr_name, sizeof(ptr_name), "%s.%s", t, d);
    snprintf(svc_name, sizeof(svc_name), "%s.%s.%s", ename, t, d);

    if ((ret = avahi_server_add_ptr(s, g, interface, protocol, AVAHI_ENTRY_NULL, AVAHI_DEFAULT_TTL, ptr_name, svc_name)) < 0)
        goto fail;

    if (!(r = avahi_record_new_full(svc_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_SRV, AVAHI_DEFAULT_TTL_HOST_NAME))) {
        ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }
    
    r->data.srv.priority = 0;
    r->data.srv.weight = 0;
    r->data.srv.port = port;
    r->data.srv.name = h;
    h = NULL;
    ret = avahi_server_add(s, g, interface, protocol, AVAHI_ENTRY_UNIQUE, r);
    avahi_record_unref(r);

    if (ret < 0)
        goto fail;

    ret = server_add_txt_strlst_nocopy(s, g, interface, protocol, AVAHI_ENTRY_UNIQUE, AVAHI_DEFAULT_TTL, svc_name, strlst);
    strlst = NULL;

    if (ret < 0)
        goto fail;

    snprintf(enum_ptr, sizeof(enum_ptr), "_services._dns-sd._udp.%s", d);
    ret = avahi_server_add_ptr(s, g, interface, protocol, AVAHI_ENTRY_NULL, AVAHI_DEFAULT_TTL, enum_ptr, ptr_name);

fail:
    
    avahi_free(d);
    avahi_free(t);
    avahi_free(h);

    avahi_string_list_free(strlst);
    
    return ret;
}

int avahi_server_add_service_strlst(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    AvahiStringList *strlst) {

    assert(s);
    assert(type);
    assert(name);

    return server_add_service_strlst_nocopy(s, g, interface, protocol, name, type, domain, host, port, avahi_string_list_copy(strlst));
}

int avahi_server_add_service_va(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    va_list va){

    assert(s);
    assert(type);
    assert(name);

    return server_add_service_strlst_nocopy(s, g, interface, protocol, name, type, domain, host, port, avahi_string_list_new_va(va));
}

int avahi_server_add_service(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    ... ){

    va_list va;
    int ret;
    
    assert(s);
    assert(type);
    assert(name);

    va_start(va, port);
    ret = avahi_server_add_service_va(s, g, interface, protocol, name, type, domain, host, port, va);
    va_end(va);
    return ret;
}

static void hexstring(char *s, size_t sl, const void *p, size_t pl) {
    static const char hex[] = "0123456789abcdef";
    int b = 0;
    const uint8_t *k = p;

    while (sl > 1 && pl > 0) {
        *(s++) = hex[(b ? *k : *k >> 4) & 0xF];

        if (b) {
            k++;
            pl--;
        }
        
        b = !b;

        sl--;
    }

    if (sl > 0)
        *s = 0;
}

int avahi_server_add_dns_server_address(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiDNSServerType type,
    const AvahiAddress *address,
    uint16_t port /** should be 53 */) {

    AvahiRecord *r;
    int ret;
    char n[64] = "ip-";

    assert(s);
    assert(address);
    assert(type == AVAHI_DNS_SERVER_UPDATE || type == AVAHI_DNS_SERVER_RESOLVE);
    assert(address->family == AVAHI_PROTO_INET || address->family == AVAHI_PROTO_INET6);

    if (port == 0)
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_PORT);
    
    if (domain && !avahi_is_valid_domain_name(domain))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_DOMAIN_NAME);

    if (address->family == AVAHI_PROTO_INET) {
        hexstring(n+3, sizeof(n)-3, &address->data, 4);
        r = avahi_record_new_full(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A, AVAHI_DEFAULT_TTL_HOST_NAME);
        r->data.a.address = address->data.ipv4;
    } else {
        hexstring(n+3, sizeof(n)-3, &address->data, 6);
        r = avahi_record_new_full(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA, AVAHI_DEFAULT_TTL_HOST_NAME);
        r->data.aaaa.address = address->data.ipv6;
    }

    if (!r)
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
    
    ret = avahi_server_add(s, g, interface, protocol, AVAHI_ENTRY_UNIQUE | AVAHI_ENTRY_ALLOWMUTIPLE, r);
    avahi_record_unref(r);

    if (ret < 0)
        return ret;
    
    return avahi_server_add_dns_server_name(s, g, interface, protocol, domain, type, n, port);
}

int avahi_server_add_dns_server_name(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiDNSServerType type,
    const char *name,
    uint16_t port /** should be 53 */) {

    int ret = -1;
    char t[256], *d = NULL, *n = NULL;
    AvahiRecord *r;
    
    assert(s);
    assert(name);
    assert(type == AVAHI_DNS_SERVER_UPDATE || type == AVAHI_DNS_SERVER_RESOLVE);

    if (port == 0)
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_PORT);

    if (!avahi_is_valid_domain_name(name))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_HOST_NAME);

    if (domain && !avahi_is_valid_domain_name(domain))
        return avahi_server_set_errno(s, AVAHI_ERR_INVALID_DOMAIN_NAME);

    
    if (!domain)
        domain = s->domain_name;

    if (!(n = avahi_normalize_name(name)) ||
        !(d = avahi_normalize_name(domain))) {
        avahi_free(n);
        avahi_free(d);
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
    }

    snprintf(t, sizeof(t), "%s.%s", type == AVAHI_DNS_SERVER_RESOLVE ? "_domain._udp" : "_dns-update._udp", d);
    avahi_free(d);
    
    if (!(r = avahi_record_new_full(t, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_SRV, AVAHI_DEFAULT_TTL_HOST_NAME))) {
        avahi_free(n);
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
    }
    
    r->data.srv.priority = 0;
    r->data.srv.weight = 0;
    r->data.srv.port = port;
    r->data.srv.name = n;
    ret = avahi_server_add(s, g, interface, protocol, AVAHI_ENTRY_NULL, r);
    avahi_record_unref(r);

    return ret;
}

static void post_query_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void* userdata) {
    AvahiKey *k = userdata;

    assert(m);
    assert(i);
    assert(k);

    avahi_interface_post_query(i, k, 0);
}

void avahi_server_post_query(AvahiServer *s, AvahiIfIndex interface, AvahiProtocol protocol, AvahiKey *key) {
    assert(s);
    assert(key);

    avahi_interface_monitor_walk(s->monitor, interface, protocol, post_query_callback, key);
}

void avahi_s_entry_group_change_state(AvahiSEntryGroup *g, AvahiEntryGroupState state) {
    assert(g);

    if (g->state == state)
        return;

    assert(state <= AVAHI_ENTRY_GROUP_COLLISION);

    g->state = state;
    
    if (g->callback)
        g->callback(g->server, g, state, g->userdata);
}

AvahiSEntryGroup *avahi_s_entry_group_new(AvahiServer *s, AvahiSEntryGroupCallback callback, void* userdata) {
    AvahiSEntryGroup *g;
    
    assert(s);

    if (!(g = avahi_new(AvahiSEntryGroup, 1))) {
        avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }
    
    g->server = s;
    g->callback = callback;
    g->userdata = userdata;
    g->dead = 0;
    g->state = AVAHI_ENTRY_GROUP_UNCOMMITED;
    g->n_probing = 0;
    g->n_register_try = 0;
    g->register_time_event = NULL;
    g->register_time.tv_sec = 0;
    g->register_time.tv_usec = 0;
    AVAHI_LLIST_HEAD_INIT(AvahiEntry, g->entries);

    AVAHI_LLIST_PREPEND(AvahiSEntryGroup, groups, s->groups, g);
    return g;
}

void avahi_s_entry_group_free(AvahiSEntryGroup *g) {
    AvahiEntry *e;
    
    assert(g);
    assert(g->server);

    for (e = g->entries; e; e = e->by_group_next) {
        if (!e->dead) {
            avahi_goodbye_entry(g->server, e, 1);
            e->dead = 1;
        }
    }

    if (g->register_time_event) {
        avahi_time_event_free(g->register_time_event);
        g->register_time_event = NULL;
    }

    g->dead = 1;
    
    g->server->need_group_cleanup = 1;
    g->server->need_entry_cleanup = 1;
}

static void entry_group_commit_real(AvahiSEntryGroup *g) {
    assert(g);

    gettimeofday(&g->register_time, NULL);

    avahi_s_entry_group_change_state(g, AVAHI_ENTRY_GROUP_REGISTERING);

    if (!g->dead) {
        avahi_announce_group(g->server, g);
        avahi_s_entry_group_check_probed(g, 0);
    }
}

static void entry_group_register_time_event_callback(AvahiTimeEvent *e, void* userdata) {
    AvahiSEntryGroup *g = userdata;
    assert(g);

/*     avahi_log_debug("Holdoff passed, waking up and going on."); */

    avahi_time_event_free(g->register_time_event);
    g->register_time_event = NULL;
    
    /* Holdoff time passed, so let's start probing */
    entry_group_commit_real(g);
}

int avahi_s_entry_group_commit(AvahiSEntryGroup *g) {
    struct timeval now;
    
    assert(g);
    assert(!g->dead);

    if (g->state != AVAHI_ENTRY_GROUP_UNCOMMITED && g->state != AVAHI_ENTRY_GROUP_COLLISION)
        return avahi_server_set_errno(g->server, AVAHI_ERR_BAD_STATE);

    g->n_register_try++;

    avahi_timeval_add(&g->register_time,
                      1000*(g->n_register_try >= AVAHI_RR_RATE_LIMIT_COUNT ?
                            AVAHI_RR_HOLDOFF_MSEC_RATE_LIMIT :
                            AVAHI_RR_HOLDOFF_MSEC));

    gettimeofday(&now, NULL);

    if (avahi_timeval_compare(&g->register_time, &now) <= 0) {
        /* Holdoff time passed, so let's start probing */
/*         avahi_log_debug("Holdoff passed, directly going on.");  */

        entry_group_commit_real(g);
    } else {
/*          avahi_log_debug("Holdoff not passed, sleeping.");  */

         /* Holdoff time has not yet passed, so let's wait */
        assert(!g->register_time_event);
        g->register_time_event = avahi_time_event_new(g->server->time_event_queue, &g->register_time, entry_group_register_time_event_callback, g);
        
        avahi_s_entry_group_change_state(g, AVAHI_ENTRY_GROUP_REGISTERING);
    }

    return AVAHI_OK;
}

void avahi_s_entry_group_reset(AvahiSEntryGroup *g) {
    AvahiEntry *e;
    assert(g);
    
    if (g->register_time_event) {
        avahi_time_event_free(g->register_time_event);
        g->register_time_event = NULL;
    }
    
    for (e = g->entries; e; e = e->by_group_next) {
        if (!e->dead) {
            avahi_goodbye_entry(g->server, e, 1);
            e->dead = 1;
        }
    }

    if (g->register_time_event) {
        avahi_time_event_free(g->register_time_event);
        g->register_time_event = NULL;
    }
    
    g->server->need_entry_cleanup = 1;
    g->n_probing = 0;

    avahi_s_entry_group_change_state(g, AVAHI_ENTRY_GROUP_UNCOMMITED);
}

int avahi_entry_is_commited(AvahiEntry *e) {
    assert(e);
    assert(!e->dead);

    return !e->group ||
        e->group->state == AVAHI_ENTRY_GROUP_REGISTERING ||
        e->group->state == AVAHI_ENTRY_GROUP_ESTABLISHED;
}

AvahiEntryGroupState avahi_s_entry_group_get_state(AvahiSEntryGroup *g) {
    assert(g);
    assert(!g->dead);

    return g->state;
}

void avahi_s_entry_group_set_data(AvahiSEntryGroup *g, void* userdata) {
    assert(g);

    g->userdata = userdata;
}

void* avahi_s_entry_group_get_data(AvahiSEntryGroup *g) {
    assert(g);

    return g->userdata;
}

int avahi_s_entry_group_is_empty(AvahiSEntryGroup *g) {
    AvahiEntry *e;
    assert(g);

    /* Look for an entry that is not dead */
    for (e = g->entries; e; e = e->by_group_next)
        if (!e->dead)
            return 0;

    return 1;
}

const char* avahi_server_get_domain_name(AvahiServer *s) {
    assert(s);

    return s->domain_name;
}

const char* avahi_server_get_host_name(AvahiServer *s) {
    assert(s);

    return s->host_name;
}

const char* avahi_server_get_host_name_fqdn(AvahiServer *s) {
    assert(s);

    return s->host_name_fqdn;
}

void* avahi_server_get_data(AvahiServer *s) {
    assert(s);

    return s->userdata;
}

void avahi_server_set_data(AvahiServer *s, void* userdata) {
    assert(s);

    s->userdata = userdata;
}

AvahiServerState avahi_server_get_state(AvahiServer *s) {
    assert(s);

    return s->state;
}

AvahiServerConfig* avahi_server_config_init(AvahiServerConfig *c) {
    assert(c);

    memset(c, 0, sizeof(AvahiServerConfig));
    c->use_ipv6 = 1;
    c->use_ipv4 = 1;
    c->host_name = NULL;
    c->domain_name = NULL;
    c->check_response_ttl = 0;
    c->publish_hinfo = 1;
    c->publish_addresses = 1;
    c->publish_workstation = 1;
    c->publish_domain = 1;
    c->use_iff_running = 0;
    c->enable_reflector = 0;
    c->reflect_ipv = 0;
    
    return c;
}

void avahi_server_config_free(AvahiServerConfig *c) {
    assert(c);

    avahi_free(c->host_name);
    avahi_free(c->domain_name);
}

AvahiServerConfig* avahi_server_config_copy(AvahiServerConfig *ret, const AvahiServerConfig *c) {
    char *d = NULL, *h = NULL;
    assert(ret);
    assert(c);

    if (c->host_name)
        if (!(h = avahi_strdup(c->host_name)))
            return NULL;

    if (c->domain_name)
        if (!(d = avahi_strdup(c->domain_name))) {
            avahi_free(h);
            return NULL;
        }
    
    *ret = *c;
    ret->host_name = h;
    ret->domain_name = d;

    return ret;
}

int avahi_server_errno(AvahiServer *s) {
    assert(s);
    
    return s->error;
}

/* Just for internal use */
int avahi_server_set_errno(AvahiServer *s, int error) {
    assert(s);

    return s->error = error;
}

