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

static void free_entry(flxServer*s, flxEntry *e) {
    flxEntry *t;

    g_assert(s);
    g_assert(e);

    flx_goodbye_entry(s, e, TRUE);

    /* Remove from linked list */
    FLX_LLIST_REMOVE(flxEntry, entries, s->entries, e);

    /* Remove from hash table indexed by name */
    t = g_hash_table_lookup(s->entries_by_key, e->record->key);
    FLX_LLIST_REMOVE(flxEntry, by_key, t, e);
    if (t)
        g_hash_table_replace(s->entries_by_key, t->record->key, t);
    else
        g_hash_table_remove(s->entries_by_key, e->record->key);

    /* Remove from associated group */
    if (e->group)
        FLX_LLIST_REMOVE(flxEntry, by_group, e->group->entries, e);

    flx_record_unref(e->record);
    g_free(e);
}

static void free_group(flxServer *s, flxEntryGroup *g) {
    g_assert(s);
    g_assert(g);

    while (g->entries)
        free_entry(s, g->entries);

    FLX_LLIST_REMOVE(flxEntryGroup, groups, s->groups, g);
    g_free(g);
}

static void cleanup_dead(flxServer *s) {
    flxEntryGroup *g, *ng;
    flxEntry *e, *ne;
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

static void handle_query_key(flxServer *s, flxKey *k, flxInterface *i, const flxAddress *a) {
    flxEntry *e;
    gchar *txt;
    
    g_assert(s);
    g_assert(k);
    g_assert(i);
    g_assert(a);

    g_message("Handling query: %s", txt = flx_key_to_string(k));
    g_free(txt);

    flx_packet_scheduler_incoming_query(i->scheduler, k);

    if (k->type == FLX_DNS_TYPE_ANY) {

        /* Handle ANY query */
        
        for (e = s->entries; e; e = e->entries_next)
            if (!e->dead && flx_key_pattern_match(k, e->record->key) && flx_entry_registered(s, e, i))
                flx_interface_post_response(i, a, e->record, e->flags & FLX_ENTRY_UNIQUE, FALSE);
    } else {

        /* Handle all other queries */
        
        for (e = g_hash_table_lookup(s->entries_by_key, k); e; e = e->by_key_next)
            if (!e->dead && flx_entry_registered(s, e, i))
                flx_interface_post_response(i, a, e->record, e->flags & FLX_ENTRY_UNIQUE, FALSE);
    }
}

static void withdraw_entry(flxServer *s, flxEntry *e) {
    g_assert(s);
    g_assert(e);

    
    if (e->group) {
        flxEntry *k;
        
        for (k = e->group->entries; k; k = k->by_group_next) {
            flx_goodbye_entry(s, k, FALSE);
            k->dead = TRUE;
        }
        
        flx_entry_group_change_state(e->group, FLX_ENTRY_GROUP_COLLISION);
    } else {
        flx_goodbye_entry(s, e, FALSE);
        e->dead = TRUE;
    }

    s->need_entry_cleanup = TRUE;
}

static void incoming_probe(flxServer *s, flxRecord *record, flxInterface *i) {
    flxEntry *e, *n;
    gchar *t;
    
    g_assert(s);
    g_assert(record);
    g_assert(i);

    t = flx_record_to_string(record);

/*     g_message("PROBE: [%s]", t); */

    
    for (e = g_hash_table_lookup(s->entries_by_key, record->key); e; e = n) {
        n = e->by_key_next;

        if (e->dead || flx_record_equal_no_ttl(record, e->record))
            continue;

        if (flx_entry_registering(s, e, i)) {
            gint cmp;

            if ((cmp = flx_record_lexicographical_compare(record, e->record)) > 0) {
                withdraw_entry(s, e);
                g_message("Recieved conflicting probe [%s]. Local host lost. Withdrawing.", t);
            } else if (cmp < 0)
                g_message("Recieved conflicting probe [%s]. Local host won.", t);

        }
    }

    g_free(t);
}

static void handle_query(flxServer *s, flxDnsPacket *p, flxInterface *i, const flxAddress *a) {
    guint n;
    
    g_assert(s);
    g_assert(p);
    g_assert(i);
    g_assert(a);

    /* Handle the questions */
    for (n = flx_dns_packet_get_field(p, FLX_DNS_FIELD_QDCOUNT); n > 0; n --) {
        flxKey *key;

        if (!(key = flx_dns_packet_consume_key(p))) {
            g_warning("Packet too short (1)");
            return;
        }

        handle_query_key(s, key, i, a);
        flx_key_unref(key);
    }

    /* Known Answer Suppresion */
    for (n = flx_dns_packet_get_field(p, FLX_DNS_FIELD_ANCOUNT); n > 0; n --) {
        flxRecord *record;
        gboolean unique = FALSE;

        if (!(record = flx_dns_packet_consume_record(p, &unique))) {
            g_warning("Packet too short (2)");
            return;
        }

        flx_packet_scheduler_incoming_known_answer(i->scheduler, record, a);
        flx_record_unref(record);
    }

    /* Probe record */
    for (n = flx_dns_packet_get_field(p, FLX_DNS_FIELD_NSCOUNT); n > 0; n --) {
        flxRecord *record;
        gboolean unique = FALSE;

        if (!(record = flx_dns_packet_consume_record(p, &unique))) {
            g_warning("Packet too short (3)");
            return;
        }

        if (record->key->type != FLX_DNS_TYPE_ANY)
            incoming_probe(s, record, i);
        
        flx_record_unref(record);
    }
}

static gboolean handle_conflict(flxServer *s, flxInterface *i, flxRecord *record, gboolean unique, const flxAddress *a) {
    gboolean valid = TRUE;
    flxEntry *e, *n;
    gchar *t;
    
    g_assert(s);
    g_assert(i);
    g_assert(record);

    t = flx_record_to_string(record);

/*     g_message("CHECKING FOR CONFLICT: [%s]", t); */

    for (e = g_hash_table_lookup(s->entries_by_key, record->key); e; e = n) {
        n = e->by_key_next;

        if (e->dead)
            continue;
        
        if (flx_entry_registered(s, e, i)) {

            gboolean equal = flx_record_equal_no_ttl(record, e->record);
                
            /* Check whether there is a unique record conflict */
            if (!equal && ((e->flags & FLX_ENTRY_UNIQUE) || unique)) {
                gint cmp;
                
                /* The lexicographically later data wins. */
                if ((cmp = flx_record_lexicographical_compare(record, e->record)) > 0) {
                    g_message("Recieved conflicting record [%s]. Local host lost. Withdrawing.", t);
                    withdraw_entry(s, e);
                } else if (cmp < 0) {
                    /* Tell the other host that our entry is lexicographically later */

                    g_message("Recieved conflicting record [%s]. Local host won. Refreshing.", t);

                    valid = FALSE;
                    flx_interface_post_response(i, a, e->record, e->flags & FLX_ENTRY_UNIQUE, TRUE);
                }
                
                /* Check wheter there is a TTL conflict */
            } else if (equal && record->ttl <= e->record->ttl/2) {
                /* Correct the TTL */
                valid = FALSE;
                flx_interface_post_response(i, a, e->record, e->flags & FLX_ENTRY_UNIQUE, TRUE);
                g_message("Recieved record with bad TTL [%s]. Refreshing.", t);
            }
            
        } else if (flx_entry_registering(s, e, i)) {

            if (!flx_record_equal_no_ttl(record, e->record) && ((e->flags & FLX_ENTRY_UNIQUE) || unique)) {

                /* We are currently registering a matching record, but
                 * someone else already claimed it, so let's
                 * withdraw */
                
                g_message("Recieved conflicting record [%s] with local record to be. Withdrawing.", t);
                withdraw_entry(s, e);
            }
        }
    }

    g_free(t);

    return valid;
}

static void handle_response(flxServer *s, flxDnsPacket *p, flxInterface *i, const flxAddress *a) {
    guint n;
    
    g_assert(s);
    g_assert(p);
    g_assert(i);
    g_assert(a);
    
    for (n = flx_dns_packet_get_field(p, FLX_DNS_FIELD_ANCOUNT) +
             flx_dns_packet_get_field(p, FLX_DNS_FIELD_ARCOUNT); n > 0; n--) {
        flxRecord *record;
        gboolean cache_flush = FALSE;
        gchar *txt;
        
        if (!(record = flx_dns_packet_consume_record(p, &cache_flush))) {
            g_warning("Packet too short (4)");
            return;
        }

        if (record->key->type != FLX_DNS_TYPE_ANY) {

            g_message("Handling response: %s", txt = flx_record_to_string(record));
            g_free(txt);
            
            if (handle_conflict(s, i, record, cache_flush, a)) {
                flx_cache_update(i->cache, record, cache_flush, a);
                flx_packet_scheduler_incoming_response(i->scheduler, record);
            }
        }
            
        flx_record_unref(record);
    }
}

static void dispatch_packet(flxServer *s, flxDnsPacket *p, struct sockaddr *sa, gint iface, gint ttl) {
    flxInterface *i;
    flxAddress a;
    
    g_assert(s);
    g_assert(p);
    g_assert(sa);
    g_assert(iface > 0);

    g_message("new packet recieved.");

    if (!(i = flx_interface_monitor_get_interface(s->monitor, iface, sa->sa_family))) {
        g_warning("Recieved packet from invalid interface.");
        return;
    }

    if (ttl != 255) {
        g_warning("Recieved packet with invalid TTL on interface '%s.%i'.", i->hardware->name, i->protocol);
        if (!s->ignore_bad_ttl)
            return;
    }

    if (sa->sa_family == AF_INET6) {
        static const unsigned char ipv4_in_ipv6[] = {
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0xFF, 0xFF };

        if (memcmp(((struct sockaddr_in6*) sa)->sin6_addr.s6_addr, ipv4_in_ipv6, sizeof(ipv4_in_ipv6)) == 0) {

            /* This is an IPv4 address encapsulated in IPv6, so let's ignore it. */
            return;
        }
    }

    if (flx_dns_packet_check_valid(p) < 0) {
        g_warning("Recieved invalid packet.");
        return;
    }

    flx_address_from_sockaddr(sa, &a);

    if (flx_dns_packet_is_query(p)) {

        if (flx_dns_packet_get_field(p, FLX_DNS_FIELD_QDCOUNT) == 0 ||
            flx_dns_packet_get_field(p, FLX_DNS_FIELD_ARCOUNT) != 0) {
            g_warning("Invalid query packet.");
            return;
        }
                
        handle_query(s, p, i, &a);    
        g_message("Handled query");
    } else {
        if (flx_dns_packet_get_field(p, FLX_DNS_FIELD_QDCOUNT) != 0 ||
            flx_dns_packet_get_field(p, FLX_DNS_FIELD_ANCOUNT) == 0 ||
            flx_dns_packet_get_field(p, FLX_DNS_FIELD_NSCOUNT) != 0) {
            g_warning("Invalid response packet.");
            return;
        }

        handle_response(s, p, i, &a);
        g_message("Handled response");
    }
}

static void work(flxServer *s) {
    struct sockaddr_in6 sa6;
    struct sockaddr_in sa;
    flxDnsPacket *p;
    gint iface = -1;
    guint8 ttl;
        
    g_assert(s);

    if (s->pollfd_ipv4.revents & G_IO_IN) {
        if ((p = flx_recv_dns_packet_ipv4(s->fd_ipv4, &sa, &iface, &ttl))) {
            dispatch_packet(s, p, (struct sockaddr*) &sa, iface, ttl);
            flx_dns_packet_free(p);
        }
    }

    if (s->pollfd_ipv6.revents & G_IO_IN) {
        if ((p = flx_recv_dns_packet_ipv6(s->fd_ipv6, &sa6, &iface, &ttl))) {
            dispatch_packet(s, p, (struct sockaddr*) &sa6, iface, ttl);
            flx_dns_packet_free(p);
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
    flxServer* s;
    g_assert(source);

    s = *((flxServer**) (((guint8*) source) + sizeof(GSource)));
    g_assert(s);
    
    return (s->pollfd_ipv4.revents | s->pollfd_ipv6.revents) & (G_IO_IN | G_IO_HUP | G_IO_ERR);
}

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer user_data) {
    flxServer* s;
    g_assert(source);

    s = *((flxServer**) (((guint8*) source) + sizeof(GSource)));
    g_assert(s);

    work(s);
    cleanup_dead(s);

    return TRUE;
}

static void add_default_entries(flxServer *s) {
    gint length = 0;
    struct utsname utsname;
    gchar *hinfo;
    flxAddress a;
    flxRecord *r;
    
    g_assert(s);
    
    /* Fill in HINFO rr */
    r = flx_record_new_full(s->hostname, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_HINFO);
    uname(&utsname);
    r->data.hinfo.cpu = g_strdup(g_strup(utsname.machine));
    r->data.hinfo.os = g_strdup(g_strup(utsname.sysname));
    flx_server_add(s, NULL, 0, AF_UNSPEC, FLX_ENTRY_UNIQUE, r);
    flx_record_unref(r);

    /* Add localhost entries */
    flx_address_parse("127.0.0.1", AF_INET, &a);
    flx_server_add_address(s, NULL, 0, AF_UNSPEC, FLX_ENTRY_UNIQUE|FLX_ENTRY_NOPROBE|FLX_ENTRY_NOANNOUNCE, "localhost", &a);

    flx_address_parse("::1", AF_INET6, &a);
    flx_server_add_address(s, NULL, 0, AF_UNSPEC, FLX_ENTRY_UNIQUE|FLX_ENTRY_NOPROBE|FLX_ENTRY_NOANNOUNCE, "ip6-localhost", &a);
}

flxServer *flx_server_new(GMainContext *c) {
    gchar *hn, *e;
    flxServer *s;
    
    static GSourceFuncs source_funcs = {
        prepare_func,
        check_func,
        dispatch_func,
        NULL,
        NULL,
        NULL
    };

    s = g_new(flxServer, 1);

    s->ignore_bad_ttl = FALSE;
    s->need_entry_cleanup = s->need_group_cleanup = FALSE;
    
    s->fd_ipv4 = flx_open_socket_ipv4();
    s->fd_ipv6 = flx_open_socket_ipv6();
    
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
    
    FLX_LLIST_HEAD_INIT(flxEntry, s->entries);
    s->entries_by_key = g_hash_table_new((GHashFunc) flx_key_hash, (GEqualFunc) flx_key_equal);
    FLX_LLIST_HEAD_INIT(flxGroup, s->groups);

    FLX_LLIST_HEAD_INIT(flxSubscription, s->subscriptions);
    s->subscription_hashtable = g_hash_table_new((GHashFunc) flx_key_hash, (GEqualFunc) flx_key_equal);

    /* Get host name */
    hn = flx_get_host_name();
    hn[strcspn(hn, ".")] = 0;

    s->hostname = g_strdup_printf("%s.local.", hn);
    g_free(hn);

    s->time_event_queue = flx_time_event_queue_new(s->context, G_PRIORITY_DEFAULT+10); /* Slightly less priority than the FDs */
    s->monitor = flx_interface_monitor_new(s);
    flx_interface_monitor_sync(s->monitor);
    add_default_entries(s);
    
    /* Prepare IO source registration */
    s->source = g_source_new(&source_funcs, sizeof(GSource) + sizeof(flxServer*));
    *((flxServer**) (((guint8*) s->source) + sizeof(GSource))) = s;

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

void flx_server_free(flxServer* s) {
    g_assert(s);

    while(s->entries)
        free_entry(s, s->entries);

    flx_interface_monitor_free(s->monitor);

    while (s->groups)
        free_group(s, s->groups);

    while (s->subscriptions)
        flx_subscription_free(s->subscriptions);
    g_hash_table_destroy(s->subscription_hashtable);

    g_hash_table_destroy(s->entries_by_key);

    flx_time_event_queue_free(s->time_event_queue);

    if (s->fd_ipv4 >= 0)
        close(s->fd_ipv4);
    if (s->fd_ipv6 >= 0)
        close(s->fd_ipv6);
    
    g_free(s->hostname);

    g_source_destroy(s->source);
    g_source_unref(s->source);
    g_main_context_unref(s->context);

    g_free(s);
}

void flx_server_add(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    flxRecord *r) {
    
    flxEntry *e, *t;
    g_assert(s);
    g_assert(r);

    g_assert(r->key->type != FLX_DNS_TYPE_ANY);

    e = g_new(flxEntry, 1);
    e->server = s;
    e->record = flx_record_ref(r);
    e->group = g;
    e->interface = interface;
    e->protocol = protocol;
    e->flags = flags;
    e->dead = FALSE;

    FLX_LLIST_HEAD_INIT(flxAnnouncement, e->announcements);

    FLX_LLIST_PREPEND(flxEntry, entries, s->entries, e);

    /* Insert into hash table indexed by name */
    t = g_hash_table_lookup(s->entries_by_key, e->record->key);
    FLX_LLIST_PREPEND(flxEntry, by_key, t, e);
    g_hash_table_replace(s->entries_by_key, e->record->key, t);

    /* Insert into group list */
    if (g)
        FLX_LLIST_PREPEND(flxEntry, by_group, g->entries, e); 

    flx_announce_entry(s, e);
}
const flxRecord *flx_server_iterate(flxServer *s, flxEntryGroup *g, void **state) {
    flxEntry **e = (flxEntry**) state;
    g_assert(s);
    g_assert(e);

    if (!*e)
        *e = g ? g->entries : s->entries;
    
    while (*e && (*e)->dead)
        *e = g ? (*e)->by_group_next : (*e)->entries_next;
        
    if (!*e)
        return NULL;

    return flx_record_ref((*e)->record);
}

void flx_server_dump(flxServer *s, FILE *f) {
    flxEntry *e;
    g_assert(s);
    g_assert(f);

    fprintf(f, "\n;;; ZONE DUMP FOLLOWS ;;;\n");

    for (e = s->entries; e; e = e->entries_next) {
        gchar *t;

        if (e->dead)
            continue;
        
        t = flx_record_to_string(e->record);
        fprintf(f, "%s ; iface=%i proto=%i\n", t, e->interface, e->protocol);
        g_free(t);
    }

    flx_dump_caches(s->monitor, f);
}

void flx_server_add_ptr(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    const gchar *dest) {

    flxRecord *r;

    g_assert(dest);

    r = flx_record_new_full(name ? name : s->hostname, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_PTR);
    r->data.ptr.name = flx_normalize_name(dest);
    flx_server_add(s, g, interface, protocol, flags, r);
    flx_record_unref(r);
}

void flx_server_add_address(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    flxAddress *a) {

    gchar *n = NULL;
    g_assert(s);
    g_assert(a);

    name = name ? (n = flx_normalize_name(name)) : s->hostname;
    
    if (a->family == AF_INET) {
        gchar *reverse;
        flxRecord  *r;

        r = flx_record_new_full(name, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_A);
        r->data.a.address = a->data.ipv4;
        flx_server_add(s, g, interface, protocol, flags, r);
        flx_record_unref(r);
        
        reverse = flx_reverse_lookup_name_ipv4(&a->data.ipv4);
        g_assert(reverse);
        flx_server_add_ptr(s, g, interface, protocol, flags, reverse, name);
        g_free(reverse);
        
    } else {
        gchar *reverse;
        flxRecord *r;
            
        r = flx_record_new_full(name, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_AAAA);
        r->data.aaaa.address = a->data.ipv6;
        flx_server_add(s, g, interface, protocol, flags, r);
        flx_record_unref(r);

        reverse = flx_reverse_lookup_name_ipv6_arpa(&a->data.ipv6);
        g_assert(reverse);
        flx_server_add_ptr(s, g, interface, protocol, flags, reverse, name);
        g_free(reverse);
    
        reverse = flx_reverse_lookup_name_ipv6_int(&a->data.ipv6);
        g_assert(reverse);
        flx_server_add_ptr(s, g, interface, protocol, flags, reverse, name);
        g_free(reverse);
    }
    
    g_free(n);
}

void flx_server_add_text_strlst(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    flxStringList *strlst) {

    flxRecord *r;
    
    g_assert(s);
    
    r = flx_record_new_full(name ? name : s->hostname, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_TXT);
    r->data.txt.string_list = strlst;
    flx_server_add(s, g, interface, protocol, flags, r);
    flx_record_unref(r);
}

void flx_server_add_text_va(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    va_list va) {
    
    g_assert(s);

    flx_server_add_text_strlst(s, g, interface, protocol, flags, name, flx_string_list_new_va(va));
}

void flx_server_add_text(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    ...) {

    va_list va;
    
    g_assert(s);

    va_start(va, name);
    flx_server_add_text_va(s, g, interface, protocol, flags, name, va);
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

void flx_server_add_service_strlst(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    flxStringList *strlst) {

    gchar ptr_name[256], svc_name[256], ename[64], enum_ptr[256];
    flxRecord *r;
    
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
        host = s->hostname;

    snprintf(ptr_name, sizeof(ptr_name), "%s.%s", type, domain);
    snprintf(svc_name, sizeof(svc_name), "%s.%s.%s", ename, type, domain);
    
    flx_server_add_ptr(s, g, interface, protocol, FLX_ENTRY_NULL, ptr_name, svc_name);

    r = flx_record_new_full(svc_name, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_SRV);
    r->data.srv.priority = 0;
    r->data.srv.weight = 0;
    r->data.srv.port = port;
    r->data.srv.name = flx_normalize_name(host);
    flx_server_add(s, g, interface, protocol, FLX_ENTRY_UNIQUE, r);
    flx_record_unref(r);

    flx_server_add_text_strlst(s, g, interface, protocol, FLX_ENTRY_UNIQUE, svc_name, strlst);

    snprintf(enum_ptr, sizeof(enum_ptr), "_services._dns-sd._udp.%s", domain);
    flx_server_add_ptr(s, g, interface, protocol, FLX_ENTRY_NULL, enum_ptr, ptr_name);
}

void flx_server_add_service_va(
    flxServer *s,
    flxEntryGroup *g,
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

    flx_server_add_service(s, g, interface, protocol, type, name, domain, host, port, flx_string_list_new_va(va));
}

void flx_server_add_service(
    flxServer *s,
    flxEntryGroup *g,
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
    flx_server_add_service_va(s, g, interface, protocol, type, name, domain, host, port, va);
    va_end(va);
}

static void post_query_callback(flxInterfaceMonitor *m, flxInterface *i, gpointer userdata) {
    flxKey *k = userdata;

    g_assert(m);
    g_assert(i);
    g_assert(k);

    flx_interface_post_query(i, k, FALSE);
}

void flx_server_post_query(flxServer *s, gint interface, guchar protocol, flxKey *key) {
    g_assert(s);
    g_assert(key);

    flx_interface_monitor_walk(s->monitor, interface, protocol, post_query_callback, key);
}

struct tmpdata {
    flxRecord *record;
    gboolean flush_cache;
};

static void post_response_callback(flxInterfaceMonitor *m, flxInterface *i, gpointer userdata) {
    struct tmpdata *tmpdata = userdata;

    g_assert(m);
    g_assert(i);
    g_assert(tmpdata);

    flx_interface_post_response(i, NULL, tmpdata->record, tmpdata->flush_cache, FALSE);
}

void flx_server_post_response(flxServer *s, gint interface, guchar protocol, flxRecord *record, gboolean flush_cache) {
    struct tmpdata tmpdata;
    
    g_assert(s);
    g_assert(record);

    tmpdata.record = record;
    tmpdata.flush_cache = flush_cache;

    flx_interface_monitor_walk(s->monitor, interface, protocol, post_response_callback, &tmpdata);
}

void flx_entry_group_change_state(flxEntryGroup *g, flxEntryGroupState state) {
    g_assert(g);

    g->state = state;
    
    if (g->callback) {
        g->callback(g->server, g, state, g->userdata);
        return;
    }
}

flxEntryGroup *flx_entry_group_new(flxServer *s, flxEntryGroupCallback callback, gpointer userdata) {
    flxEntryGroup *g;
    
    g_assert(s);

    g = g_new(flxEntryGroup, 1);
    g->server = s;
    g->callback = callback;
    g->userdata = userdata;
    g->dead = FALSE;
    g->state = FLX_ENTRY_GROUP_UNCOMMITED;
    g->n_probing = 0;
    FLX_LLIST_HEAD_INIT(flxEntry, g->entries);

    FLX_LLIST_PREPEND(flxEntryGroup, groups, s->groups, g);
    return g;
}

void flx_entry_group_free(flxEntryGroup *g) {
    g_assert(g);
    g_assert(g->server);

    g->dead = TRUE;
    g->server->need_group_cleanup = TRUE;
}

void flx_entry_group_commit(flxEntryGroup *g) {
    flxEntry *e;
    
    g_assert(g);
    g_assert(!g->dead);

    if (g->state != FLX_ENTRY_GROUP_UNCOMMITED)
        return;

    flx_entry_group_change_state(g, FLX_ENTRY_GROUP_REGISTERING);
    flx_announce_group(g->server, g);
    flx_entry_group_check_probed(g, FALSE);
}

gboolean flx_entry_commited(flxEntry *e) {
    g_assert(e);
    g_assert(!e->dead);

    return !e->group ||
        e->group->state == FLX_ENTRY_GROUP_REGISTERING ||
        e->group->state == FLX_ENTRY_GROUP_ESTABLISHED;
}

flxEntryGroupState flx_entry_group_get_state(flxEntryGroup *g) {
    g_assert(g);
    g_assert(!g->dead);

    return g->state;
}
