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

#include "server.h"
#include "util.h"
#include "iface.h"
#include "socket.h"
#include "subscribe.h"

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
}

static void add_aux_records(AvahiServer *s, AvahiInterface *i, const gchar *name, guint16 type, gboolean unicast_response) {
    AvahiKey *k;

    g_assert(s);
    g_assert(i);
    g_assert(name);

    k = avahi_key_new(name, AVAHI_DNS_CLASS_IN, type);
    avahi_server_prepare_matching_responses(s, i, k, unicast_response);
    avahi_key_unref(k);
}

void avahi_server_prepare_response(AvahiServer *s, AvahiInterface *i, AvahiEntry *e, gboolean unicast_response) {
    g_assert(s);
    g_assert(i);
    g_assert(e);

    /* Append a record to a packet and all the records referred by it */

    avahi_record_list_push(s->record_list, e->record, e->flags & AVAHI_ENTRY_UNIQUE, unicast_response);
    
    if (e->record->key->class == AVAHI_DNS_CLASS_IN) {
        if (e->record->key->type == AVAHI_DNS_TYPE_PTR) {
            add_aux_records(s, i, e->record->data.ptr.name, AVAHI_DNS_TYPE_SRV, unicast_response);
            add_aux_records(s, i, e->record->data.ptr.name, AVAHI_DNS_TYPE_TXT, unicast_response);
        } else if (e->record->key->type == AVAHI_DNS_TYPE_SRV) {
            add_aux_records(s, i, e->record->data.srv.name, AVAHI_DNS_TYPE_A, unicast_response);
            add_aux_records(s, i, e->record->data.srv.name, AVAHI_DNS_TYPE_AAAA, unicast_response);
        }
    }
}

void avahi_server_prepare_matching_responses(AvahiServer *s, AvahiInterface *i, AvahiKey *k, gboolean unicast_response) {
    AvahiEntry *e;
    gchar *txt;
    
    g_assert(s);
    g_assert(i);
    g_assert(k);

    g_message("Posting responses matching [%s]", txt = avahi_key_to_string(k));
    g_free(txt);

    if (avahi_key_is_pattern(k)) {

        /* Handle ANY query */
        
        for (e = s->entries; e; e = e->entries_next)
            if (!e->dead && avahi_key_pattern_match(k, e->record->key) && avahi_entry_registered(s, e, i))
                avahi_server_prepare_response(s, i, e, unicast_response);

    } else {

        /* Handle all other queries */
        
        for (e = g_hash_table_lookup(s->entries_by_key, k); e; e = e->by_key_next)
            if (!e->dead && avahi_entry_registered(s, e, i))
                avahi_server_prepare_response(s, i, e, unicast_response);
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

        if (e->dead || !avahi_entry_probing(s, e, i))
            continue;
        
        if ((cmp = avahi_record_lexicographical_compare(e->record, record)) == 0) {
            ours = TRUE;
            break;
        } else if (cmp > 0)
            won = TRUE;
        else /* cmp < 0 */
            lost = TRUE;
    }

    t = avahi_record_to_string(record);

    if (!ours) {

        if (won)
            g_message("Recieved conflicting probe [%s]. Local host won.", t);
        else if (lost) {
            g_message("Recieved conflicting probe [%s]. Local host lost. Withdrawing.", t);
            withdraw_rrset(s, record->key);
        }
    }

    g_free(t);
}

static gboolean handle_conflict(AvahiServer *s, AvahiInterface *i, AvahiRecord *record, gboolean unique, const AvahiAddress *a) {
    gboolean valid = TRUE, ours = FALSE, conflict = FALSE, withdraw_immediately = FALSE;
    AvahiEntry *e, *n, *conflicting_entry = NULL;
    gchar *t;
    
    g_assert(s);
    g_assert(i);
    g_assert(record);

    t = avahi_record_to_string(record);

    g_message("CHECKING FOR CONFLICT: [%s]", t);  

    for (e = g_hash_table_lookup(s->entries_by_key, record->key); e; e = n) {
        n = e->by_key_next;

        if (e->dead || (!(e->flags & AVAHI_ENTRY_UNIQUE) && !unique))
            continue;

        /* Either our entry or the other is intended to be unique, so let's check */
        
        if (avahi_entry_registered(s, e, i)) {

            if (avahi_record_equal_no_ttl(e->record, record)) {
                ours = TRUE; /* We have an identical record, so this is no conflict */
                
                /* Check wheter there is a TTL conflict */
                if (record->ttl <= e->record->ttl/2) {
                    /* Refresh */
                    g_message("Recieved record with bad TTL [%s]. Refreshing.", t);
                    avahi_interface_post_response(i, e->record, FALSE, TRUE, NULL);
                    valid = FALSE;
                }
                
                /* There's no need to check the other entries of this RRset */
                break;
            } else {
                /* A conflict => we have to return to probe mode */
                conflict = TRUE;
                conflicting_entry = e;
            }

        } else if (avahi_entry_probing(s, e, i)) {

            if (!avahi_record_equal_no_ttl(record, e->record)) {
            
                /* We are currently registering a matching record, but
                 * someone else already claimed it, so let's
                 * withdraw */
                conflict = TRUE;
                withdraw_immediately = TRUE;
            }
        }
    }

/*     g_message("ours=%i conflict=%i", ours, conflict); */

    if (!ours && conflict) {
        valid = FALSE;

        if (withdraw_immediately) {
            g_message("Recieved conflicting record [%s] with local record to be. Withdrawing.", t);
            withdraw_rrset(s, record->key);
        } else {
            g_assert(conflicting_entry);
            g_message("Recieved conflicting record [%s]. Resetting our record.", t);
            avahi_entry_return_to_initial_state(s, conflicting_entry, i);

            /* Local unique records are returned to probin
             * state. Local shared records are reannounced. */
        }
    }

    g_free(t);

    return valid;
}

void avahi_server_generate_response(AvahiServer *s, AvahiInterface *i, AvahiDnsPacket *p, const AvahiAddress *a, guint16 port, gboolean legacy_unicast) {

    g_assert(s);
    g_assert(i);
    g_assert(!legacy_unicast || (a && port > 0 && p));

    if (legacy_unicast) {
        AvahiDnsPacket *reply;
        AvahiRecord *r;

        reply = avahi_dns_packet_new_reply(p, 512 /* unicast DNS maximum packet size is 512 */ , TRUE, TRUE);
        
        while ((r = avahi_record_list_pop(s->record_list, NULL, NULL))) {
            
            if (avahi_dns_packet_append_record(reply, r, FALSE, 10))
                avahi_dns_packet_inc_field(reply, AVAHI_DNS_FIELD_ANCOUNT);
            else {
                gchar *t = avahi_record_to_string(r);
                g_warning("Record [%s] not fitting in legacy unicast packet, dropping.", t);
                g_free(t);
            }

            avahi_record_unref(r);
        }

        if (avahi_dns_packet_get_field(reply, AVAHI_DNS_FIELD_ANCOUNT) != 0)
            avahi_interface_send_packet_unicast(i, reply, a, port);

        avahi_dns_packet_free(reply);

    } else {
        gboolean unicast_response, flush_cache;
        AvahiDnsPacket *reply = NULL;
        AvahiRecord *r;
        
        while ((r = avahi_record_list_pop(s->record_list, &flush_cache, &unicast_response))) {

            if (!avahi_interface_post_response(i, r, flush_cache, FALSE, a) && unicast_response) {
            
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
                        gchar *t = avahi_record_to_string(r);
                        g_warning("Record [%s] too large, doesn't fit in any packet!", t);
                        g_free(t);
                        break;
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
}

static void handle_query(AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *a, guint16 port, gboolean legacy_unicast) {
    guint n;
    
    g_assert(s);
    g_assert(p);
    g_assert(i);
    g_assert(a);

/*     g_message("query"); */

    g_assert(avahi_record_list_empty(s->record_list));
    
    /* Handle the questions */
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT); n > 0; n --) {
        AvahiKey *key;
        gboolean unicast_response = FALSE;

        if (!(key = avahi_dns_packet_consume_key(p, &unicast_response))) {
            g_warning("Packet too short (1)");
            avahi_record_list_flush(s->record_list);
            return;
       }

        avahi_packet_scheduler_incoming_query(i->scheduler, key);
        avahi_server_prepare_matching_responses(s, i, key, unicast_response);
        avahi_key_unref(key);
    }

    if (!avahi_record_list_empty(s->record_list))
        avahi_server_generate_response(s, i, p, a, port, legacy_unicast);
    
    /* Known Answer Suppression */
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT); n > 0; n --) {
        AvahiRecord *record;
        gboolean unique = FALSE;

        if (!(record = avahi_dns_packet_consume_record(p, &unique))) {
            g_warning("Packet too short (2)");
            return;
        }

        if (handle_conflict(s, i, record, unique, a))
            avahi_packet_scheduler_incoming_known_answer(i->scheduler, record, a);
        
        avahi_record_unref(record);
    }

    /* Probe record */
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT); n > 0; n --) {
        AvahiRecord *record;
        gboolean unique = FALSE;

        if (!(record = avahi_dns_packet_consume_record(p, &unique))) {
            g_warning("Packet too short (3)");
            return;
        }

        if (record->key->type != AVAHI_DNS_TYPE_ANY)
            incoming_probe(s, record, i);
        
        avahi_record_unref(record);
    }
}

static void handle_response(AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *a) {
    guint n;
    
    g_assert(s);
    g_assert(p);
    g_assert(i);
    g_assert(a);

/*     g_message("response"); */
    
    for (n = avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) +
             avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ARCOUNT); n > 0; n--) {
        AvahiRecord *record;
        gboolean cache_flush = FALSE;
        gchar *txt;
        
        if (!(record = avahi_dns_packet_consume_record(p, &cache_flush))) {
            g_warning("Packet too short (4)");
            break;
        }

        if (record->key->type != AVAHI_DNS_TYPE_ANY) {

            g_message("Handling response: %s", txt = avahi_record_to_string(record));
            g_free(txt);
            
            if (handle_conflict(s, i, record, cache_flush, a)) {
                avahi_cache_update(i->cache, record, cache_flush, a);
                avahi_packet_scheduler_incoming_response(i->scheduler, record, cache_flush);
            }
        }
            
        avahi_record_unref(record);
    }
}

static void dispatch_packet(AvahiServer *s, AvahiDnsPacket *p, struct sockaddr *sa, gint iface, gint ttl) {
    AvahiInterface *i;
    AvahiAddress a;
    guint16 port;
    
    g_assert(s);
    g_assert(p);
    g_assert(sa);
    g_assert(iface > 0);

    if (!(i = avahi_interface_monitor_get_interface(s->monitor, iface, sa->sa_family)) ||
        !avahi_interface_relevant(i)) {
        g_warning("Recieved packet from invalid interface.");
        return;
    }

    g_message("new packet recieved on interface '%s.%i'.", i->hardware->name, i->protocol);

    if (sa->sa_family == AF_INET6) {
        static const guint8 ipv4_in_ipv6[] = {
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0xFF, 0xFF };

        /* This is an IPv4 address encapsulated in IPv6, so let's ignore it. */

        if (memcmp(((struct sockaddr_in6*) sa)->sin6_addr.s6_addr, ipv4_in_ipv6, sizeof(ipv4_in_ipv6)) == 0)
            return;
    }

    if (avahi_dns_packet_check_valid(p) < 0) {
        g_warning("Recieved invalid packet.");
        return;
    }

    port = avahi_port_from_sockaddr(sa);
    avahi_address_from_sockaddr(sa, &a);

    if (avahi_dns_packet_is_query(p)) {
        gboolean legacy_unicast = FALSE;

        if (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ARCOUNT) != 0) {
            g_warning("Invalid query packet.");
            return;
        }

        if (port != AVAHI_MDNS_PORT) {
            /* Legacy Unicast */

            if ((avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) != 0 ||
                 avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT) != 0)) {
                g_warning("Invalid legacy unicast query packet.");
                return;
            }
        
            legacy_unicast = TRUE;
        }

        handle_query(s, p, i, &a, port, legacy_unicast);
        
        g_message("Handled query");
    } else {

        if (port != AVAHI_MDNS_PORT) {
            g_warning("Recieved repsonse with invalid source port %u on interface '%s.%i'", port, i->hardware->name, i->protocol);
            return;
        }

        if (ttl != 255) {
            g_warning("Recieved response with invalid TTL %u on interface '%s.%i'.", ttl, i->hardware->name, i->protocol);
            if (!s->ignore_bad_ttl)
                return;
        }

        if (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT) != 0 ||
            avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) == 0 ||
            avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT) != 0) {
            g_warning("Invalid response packet.");
            return;
        }

        handle_response(s, p, i, &a);
        g_message("Handled response");
    }
}

static void work(AvahiServer *s) {
    struct sockaddr_in6 sa6;
    struct sockaddr_in sa;
    AvahiDnsPacket *p;
    gint iface = -1;
    guint8 ttl;
        
    g_assert(s);

    if (s->pollfd_ipv4.revents & G_IO_IN) {
        if ((p = avahi_recv_dns_packet_ipv4(s->fd_ipv4, &sa, &iface, &ttl))) {
            dispatch_packet(s, p, (struct sockaddr*) &sa, iface, ttl);
            avahi_dns_packet_free(p);
        }
    }

    if (s->pollfd_ipv6.revents & G_IO_IN) {
        if ((p = avahi_recv_dns_packet_ipv6(s->fd_ipv6, &sa6, &iface, &ttl))) {
            dispatch_packet(s, p, (struct sockaddr*) &sa6, iface, ttl);
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
    g_assert(source);

    s = *((AvahiServer**) (((guint8*) source) + sizeof(GSource)));
    g_assert(s);
    
    return (s->pollfd_ipv4.revents | s->pollfd_ipv6.revents) & (G_IO_IN | G_IO_HUP | G_IO_ERR);
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

static void add_default_entries(AvahiServer *s) {
    struct utsname utsname;
    AvahiAddress a;
    AvahiRecord *r;
    
    g_assert(s);

    return ;
    
    /* Fill in HINFO rr */
    r = avahi_record_new_full(s->host_name_fqdn, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_HINFO);
    uname(&utsname);
    r->data.hinfo.cpu = g_strdup(g_strup(utsname.machine));
    r->data.hinfo.os = g_strdup(g_strup(utsname.sysname));
    avahi_server_add(s, NULL, 0, AF_UNSPEC, AVAHI_ENTRY_UNIQUE, r);
    avahi_record_unref(r);

    /* Add localhost entries */
    avahi_address_parse("127.0.0.1", AF_INET, &a);
    avahi_server_add_address(s, NULL, 0, AF_UNSPEC, AVAHI_ENTRY_NOPROBE|AVAHI_ENTRY_NOANNOUNCE, "localhost", &a);

    avahi_address_parse("::1", AF_INET6, &a);
    avahi_server_add_address(s, NULL, 0, AF_UNSPEC, AVAHI_ENTRY_NOPROBE|AVAHI_ENTRY_NOANNOUNCE, "ip6-localhost", &a);
}

AvahiServer *avahi_server_new(GMainContext *c) {
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

    s->ignore_bad_ttl = FALSE;
    s->need_entry_cleanup = s->need_group_cleanup = FALSE;
    
    s->fd_ipv4 = avahi_open_socket_ipv4();
    s->fd_ipv6 = -1 /* avahi_open_socket_ipv6() */ ;
    
    if (s->fd_ipv6 < 0 && s->fd_ipv4 < 0) {
        g_critical("Failed to create IP sockets.\n");
        g_free(s);
        return NULL;
    }

    if (s->fd_ipv4 < 0)
        g_message("Failed to create IPv4 socket, proceeding in IPv6 only mode");
    else if (s->fd_ipv6 < 0)
        g_message("Failed to create IPv6 socket, proceeding in IPv4 only mode");
    
    if (c)
        g_main_context_ref(s->context = c);
    else
        s->context = g_main_context_default();
    
    AVAHI_LLIST_HEAD_INIT(AvahiEntry, s->entries);
    s->entries_by_key = g_hash_table_new((GHashFunc) avahi_key_hash, (GEqualFunc) avahi_key_equal);
    AVAHI_LLIST_HEAD_INIT(AvahiGroup, s->groups);

    AVAHI_LLIST_HEAD_INIT(AvahiSubscription, s->subscriptions);
    s->subscription_hashtable = g_hash_table_new((GHashFunc) avahi_key_hash, (GEqualFunc) avahi_key_equal);

    /* Get host name */
    s->host_name = avahi_get_host_name();
    s->host_name[strcspn(s->host_name, ".")] = 0;

    s->domain = avahi_normalize_name("local.");

    s->host_name_fqdn = g_strdup_printf("%s.%s", s->host_name, s->domain);

    s->record_list = avahi_record_list_new();

    s->time_event_queue = avahi_time_event_queue_new(s->context, G_PRIORITY_DEFAULT+10); /* Slightly less priority than the FDs */
    s->monitor = avahi_interface_monitor_new(s);
    avahi_interface_monitor_sync(s->monitor);
    add_default_entries(s);
    
    /* Prepare IO source registration */
    s->source = g_source_new(&source_funcs, sizeof(GSource) + sizeof(AvahiServer*));
    *((AvahiServer**) (((guint8*) s->source) + sizeof(GSource))) = s;

    memset(&s->pollfd_ipv4, 0, sizeof(s->pollfd_ipv4));
    s->pollfd_ipv4.fd = s->fd_ipv4;
    s->pollfd_ipv4.events = G_IO_IN|G_IO_ERR|G_IO_HUP;
    g_source_add_poll(s->source, &s->pollfd_ipv4);
    
    memset(&s->pollfd_ipv6, 0, sizeof(s->pollfd_ipv6));
    s->pollfd_ipv6.fd = s->fd_ipv6;
    s->pollfd_ipv6.events = G_IO_IN|G_IO_ERR|G_IO_HUP;
    g_source_add_poll(s->source, &s->pollfd_ipv6);

    g_source_attach(s->source, s->context);

    return s;
}

void avahi_server_free(AvahiServer* s) {
    g_assert(s);

    while(s->entries)
        free_entry(s, s->entries);

    avahi_interface_monitor_free(s->monitor);

    while (s->groups)
        free_group(s, s->groups);

    while (s->subscriptions)
        avahi_subscription_free(s->subscriptions);
    g_hash_table_destroy(s->subscription_hashtable);

    g_hash_table_destroy(s->entries_by_key);

    avahi_time_event_queue_free(s->time_event_queue);

    avahi_record_list_free(s->record_list);
    
    if (s->fd_ipv4 >= 0)
        close(s->fd_ipv4);
    if (s->fd_ipv6 >= 0)
        close(s->fd_ipv6);

    g_free(s->host_name);
    g_free(s->domain);
    g_free(s->host_name_fqdn);

    g_source_destroy(s->source);
    g_source_unref(s->source);
    g_main_context_unref(s->context);

    g_free(s);
}

void avahi_server_add(
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

void avahi_server_add_ptr(
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
}

void avahi_server_add_address(
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
}

void avahi_server_add_text_strlst(
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
}

void avahi_server_add_text_va(
    AvahiServer *s,
    AvahiEntryGroup *g,
    gint interface,
    guchar protocol,
    AvahiEntryFlags flags,
    const gchar *name,
    va_list va) {
    
    g_assert(s);

    avahi_server_add_text_strlst(s, g, interface, protocol, flags, name, avahi_string_list_new_va(va));
}

void avahi_server_add_text(
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

void avahi_server_add_service_strlst(
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
        domain = "local";

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
}

void avahi_server_add_service_va(
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
}

void avahi_server_add_service(
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
    g_assert(g);
    g_assert(g->server);

    g->dead = TRUE;
    g->server->need_group_cleanup = TRUE;
}

void avahi_entry_group_commit(AvahiEntryGroup *g) {
    g_assert(g);
    g_assert(!g->dead);

    if (g->state != AVAHI_ENTRY_GROUP_UNCOMMITED)
        return;

    avahi_entry_group_change_state(g, AVAHI_ENTRY_GROUP_REGISTERING);
    avahi_announce_group(g->server, g);
    avahi_entry_group_check_probed(g, FALSE);
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

const gchar* avahi_server_get_domain(AvahiServer *s) {
    g_assert(s);

    return s->domain;
}

const gchar* avahi_server_get_host_name(AvahiServer *s) {
    g_assert(s);

    return s->host_name_fqdn;
}
