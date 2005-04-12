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

static void handle_query_key(flxServer *s, flxKey *k, flxInterface *i, const flxAddress *a) {
    flxServerEntry *e;
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
        
        for (e = s->entries; e; e = e->entry_next)
            if (flx_key_pattern_match(k, e->record->key))
                if (flx_interface_match(i, e->interface, e->protocol))
                    flx_interface_post_response(i, a, e->record, FALSE);
    } else {

        /* Handle all other queries */
        
        for (e = g_hash_table_lookup(s->rrset_by_key, k); e; e = e->by_key_next)
            if (flx_interface_match(i, e->interface, e->protocol))
                flx_interface_post_response(i, a, e->record, FALSE);
    }
}

static void handle_query(flxServer *s, flxDnsPacket *p, flxInterface *i, const flxAddress *a) {
    guint n;
    
    g_assert(s);
    g_assert(p);
    g_assert(i);
    g_assert(a);

    for (n = flx_dns_packet_get_field(p, DNS_FIELD_QDCOUNT); n > 0; n --) {
        flxKey *key;

        if (!(key = flx_dns_packet_consume_key(p))) {
            g_warning("Packet too short");
            return;
        }

        handle_query_key(s, key, i, a);
        flx_key_unref(key);
    }

    /* Known Answer Suppresion */
    for (n = flx_dns_packet_get_field(p, DNS_FIELD_ANCOUNT); n > 0; n --) {
        flxRecord *record;
        gboolean unique = FALSE;

        if (!(record = flx_dns_packet_consume_record(p, &unique))) {
            g_warning("Packet too short (2)");
            return;
        }

        flx_packet_scheduler_incoming_known_answer(i->scheduler, record, a);
        flx_record_unref(record);
    }
}

static void handle_response(flxServer *s, flxDnsPacket *p, flxInterface *i, const flxAddress *a) {
    guint n;
    
    g_assert(s);
    g_assert(p);
    g_assert(i);
    g_assert(a);
    
    for (n = flx_dns_packet_get_field(p, DNS_FIELD_ANCOUNT) +
             flx_dns_packet_get_field(p, DNS_FIELD_ARCOUNT); n > 0; n--) {
        flxRecord *record;
        gboolean cache_flush = FALSE;
        gchar *txt;
        
        if (!(record = flx_dns_packet_consume_record(p, &cache_flush))) {
            g_warning("Packet too short (3)");
            return;
        }

        if (record->key->type == FLX_DNS_TYPE_ANY)
            continue;
        
        g_message("Handling response: %s", txt = flx_record_to_string(record));
        g_free(txt);
        
        flx_cache_update(i->cache, record, cache_flush, a);
        
        flx_packet_scheduler_incoming_response(i->scheduler, record);
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

        if (flx_dns_packet_get_field(p, DNS_FIELD_QDCOUNT) == 0 ||
            flx_dns_packet_get_field(p, DNS_FIELD_ARCOUNT) != 0 ||
            flx_dns_packet_get_field(p, DNS_FIELD_NSCOUNT) != 0) {
            g_warning("Invalid query packet.");
            return;
        }
                
        handle_query(s, p, i, &a);    
        g_message("Handled query");
    } else {
        if (flx_dns_packet_get_field(p, DNS_FIELD_QDCOUNT) != 0 ||
            flx_dns_packet_get_field(p, DNS_FIELD_ANCOUNT) == 0 ||
            flx_dns_packet_get_field(p, DNS_FIELD_NSCOUNT) != 0) {
            g_warning("Invalid response packet.");
            return;
        }

        handle_response(s, p, i, &a);
        g_message("Handled response");
    }
}

static gboolean work(flxServer *s) {
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

    return TRUE;
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
    
    return work(s);
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
    flx_server_add(s, 0, 0, AF_UNSPEC, TRUE, r);
    flx_record_unref(r);

    /* Add localhost entries */
    flx_address_parse("127.0.0.1", AF_INET, &a);
    flx_server_add_address(s, 0, 0, AF_UNSPEC, TRUE, "localhost", &a);

    flx_address_parse("::1", AF_INET6, &a);
    flx_server_add_address(s, 0, 0, AF_UNSPEC, TRUE, "ip6-localhost", &a);
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

    s->fd_ipv4 = flx_open_socket_ipv4();
    s->fd_ipv6 = flx_open_socket_ipv6();
    
    if (s->fd_ipv6 < 0 && s->fd_ipv4 < 0) {
        g_critical("Failed to create sockets.\n");
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
    
    s->current_id = 1;

    FLX_LLIST_HEAD_INIT(flxServerEntry, s->entries);
    s->rrset_by_id = g_hash_table_new(g_int_hash, g_int_equal);
    s->rrset_by_key = g_hash_table_new((GHashFunc) flx_key_hash, (GEqualFunc) flx_key_equal);

    FLX_LLIST_HEAD_INIT(flxSubscription, s->subscriptions);
    s->subscription_hashtable = g_hash_table_new((GHashFunc) flx_key_hash, (GEqualFunc) flx_key_equal);

    s->monitor = flx_interface_monitor_new(s);
    s->time_event_queue = flx_time_event_queue_new(s->context);
    
    /* Get host name */
    hn = flx_get_host_name();
    if ((e = strchr(hn, '.')))
        *e = 0;

    s->hostname = g_strdup_printf("%s.local.", hn);
    g_free(hn);

    add_default_entries(s);

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

    flx_interface_monitor_free(s->monitor);
    
    flx_server_remove(s, 0);

    while (s->subscriptions)
        flx_subscription_free(s->subscriptions);
    g_hash_table_destroy(s->subscription_hashtable);
    
    g_hash_table_destroy(s->rrset_by_id);
    g_hash_table_destroy(s->rrset_by_key);

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

gint flx_server_get_next_id(flxServer *s) {
    g_assert(s);

    return s->current_id++;
}

void flx_server_add(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    flxRecord *r) {
    
    flxServerEntry *e, *t;
    g_assert(s);
    g_assert(r);

    g_assert(r->key->type != FLX_DNS_TYPE_ANY);

    e = g_new(flxServerEntry, 1);
    e->record = flx_record_ref(r);
    e->id = id;
    e->interface = interface;
    e->protocol = protocol;
    e->unique = unique;

    FLX_LLIST_HEAD_INIT(flxAnnouncement, e->announcements);

    FLX_LLIST_PREPEND(flxServerEntry, entry, s->entries, e);

    /* Insert into hash table indexed by id */
    t = g_hash_table_lookup(s->rrset_by_id, &e->id);
    FLX_LLIST_PREPEND(flxServerEntry, by_id, t, e);
    g_hash_table_replace(s->rrset_by_id, &e->id, t);
    
    /* Insert into hash table indexed by name */
    t = g_hash_table_lookup(s->rrset_by_key, e->record->key);
    FLX_LLIST_PREPEND(flxServerEntry, by_key, t, e);
    g_hash_table_replace(s->rrset_by_key, e->record->key, t);

    flx_announce_entry(s, e);
}
const flxRecord *flx_server_iterate(flxServer *s, gint id, void **state) {
    flxServerEntry **e = (flxServerEntry**) state;
    g_assert(s);
    g_assert(e);

    if (e)
        *e = id > 0 ? (*e)->by_id_next : (*e)->entry_next;
    else
        *e = id > 0 ? g_hash_table_lookup(s->rrset_by_id, &id) : s->entries;
        
    if (!*e)
        return NULL;

    return flx_record_ref((*e)->record);
}

static void free_entry(flxServer*s, flxServerEntry *e) {
    flxServerEntry *t;
    
    g_assert(e);

    flx_goodbye_entry(s, e, TRUE);

    /* Remove from linked list */
    FLX_LLIST_REMOVE(flxServerEntry, entry, s->entries, e);

    /* Remove from hash table indexed by id */
    t = g_hash_table_lookup(s->rrset_by_id, &e->id);
    FLX_LLIST_REMOVE(flxServerEntry, by_id, t, e);
    if (t)
        g_hash_table_replace(s->rrset_by_id, &t->id, t);
    else
        g_hash_table_remove(s->rrset_by_id, &e->id);
    
    /* Remove from hash table indexed by name */
    t = g_hash_table_lookup(s->rrset_by_key, e->record->key);
    FLX_LLIST_REMOVE(flxServerEntry, by_key, t, e);
    if (t)
        g_hash_table_replace(s->rrset_by_key, t->record->key, t);
    else
        g_hash_table_remove(s->rrset_by_key, e->record->key);

    flx_record_unref(e->record);
    g_free(e);
}

void flx_server_remove(flxServer *s, gint id) {
    g_assert(s);

    if (id <= 0) {
        while (s->entries)
            free_entry(s, s->entries);
    } else {
        flxServerEntry *e;

        while ((e = g_hash_table_lookup(s->rrset_by_id, &id)))
            free_entry(s, e);
    }
}

void flx_server_dump(flxServer *s, FILE *f) {
    flxServerEntry *e;
    g_assert(s);
    g_assert(f);

    fprintf(f, "\n;;; ZONE DUMP FOLLOWS ;;;\n");

    for (e = s->entries; e; e = e->entry_next) {
        gchar *t;

        t = flx_record_to_string(e->record);
        fprintf(f, "%s\n", t);
        g_free(t);
    }

    flx_dump_caches(s->monitor, f);
}

void flx_server_add_ptr(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    const gchar *dest) {

    flxRecord *r;

    g_assert(dest);

    r = flx_record_new_full(name ? name : s->hostname, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_PTR);
    r->data.ptr.name = flx_normalize_name(dest);
    flx_server_add(s, id, interface, protocol, unique, r);
    flx_record_unref(r);

}

void flx_server_add_address(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
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
        flx_server_add(s, id, interface, protocol, unique, r);
        flx_record_unref(r);
        
        reverse = flx_reverse_lookup_name_ipv4(&a->data.ipv4);
        g_assert(reverse);
        flx_server_add_ptr(s, id, interface, protocol, unique, reverse, name);
        g_free(reverse);
        
    } else {
        gchar *reverse;
        flxRecord *r;
            
        r = flx_record_new_full(name, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_AAAA);
        r->data.aaaa.address = a->data.ipv6;
        flx_server_add(s, id, interface, protocol, unique, r);
        flx_record_unref(r);

        reverse = flx_reverse_lookup_name_ipv6_arpa(&a->data.ipv6);
        g_assert(reverse);
        flx_server_add_ptr(s, id, interface, protocol, unique, reverse, name);
        g_free(reverse);
    
        reverse = flx_reverse_lookup_name_ipv6_int(&a->data.ipv6);
        g_assert(reverse);
        flx_server_add_ptr(s, id, interface, protocol, unique, reverse, name);
        g_free(reverse);
    }
    
    g_free(n);
}

void flx_server_add_text_va(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    va_list va) {

    flxRecord *r;
    
    g_assert(s);
    
    r = flx_record_new_full(name ? name : s->hostname, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_TXT);
    r->data.txt.string_list = flx_string_list_new_va(va);
    flx_server_add(s, id, interface, protocol, unique, r);
    flx_record_unref(r);
}

void flx_server_add_text(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    ...) {

    va_list va;
    
    g_assert(s);

    va_start(va, name);
    flx_server_add_text_va(s, id, interface, protocol, unique, name, va);
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


void flx_server_add_service_va(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    va_list va) {

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
    
    flx_server_add_ptr(s, id, interface, protocol, FALSE, ptr_name, svc_name);

    r = flx_record_new_full(svc_name, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_SRV);
    r->data.srv.priority = 0;
    r->data.srv.weight = 0;
    r->data.srv.port = port;
    r->data.srv.name = flx_normalize_name(host);
    flx_server_add(s, id, interface, protocol, TRUE, r);
    flx_record_unref(r);

    flx_server_add_text_va(s, id, interface, protocol, FALSE, svc_name, va);

    snprintf(enum_ptr, sizeof(enum_ptr), "_services._dns-sd._udp.%s", domain);
    flx_server_add_ptr(s, id, interface, protocol, FALSE, enum_ptr, ptr_name);
}

void flx_server_add_service(
    flxServer *s,
    gint id,
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
    flx_server_add_service_va(s, id, interface, protocol, type, name, domain, host, port, va);
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

static void post_response_callback(flxInterfaceMonitor *m, flxInterface *i, gpointer userdata) {
    flxRecord *r = userdata;

    g_assert(m);
    g_assert(i);
    g_assert(r);

    flx_interface_post_response(i, NULL, r, FALSE);
}

void flx_server_post_response(flxServer *s, gint interface, guchar protocol, flxRecord *record) {
    g_assert(s);
    g_assert(record);

    flx_interface_monitor_walk(s->monitor, interface, protocol, post_response_callback, record);
}
