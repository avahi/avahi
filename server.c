#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/utsname.h>

#include "server.h"
#include "util.h"
#include "iface.h"

static void add_default_entries(flxServer *s) {
    gint length = 0;
    struct utsname utsname;
    gchar *hinfo;
    flxAddress a;
    
    g_assert(s);
    
    /* Fill in HINFO rr */
    uname(&utsname);
    hinfo = g_strdup_printf("%s%c%s%n", g_strup(utsname.machine), 0, g_strup(utsname.sysname), &length);
    
    flx_server_add_full(s, 0, 0, AF_UNSPEC, TRUE,
                        s->hostname, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_HINFO, hinfo, length+1, FLX_DEFAULT_TTL);

    g_free(hinfo);

    /* Add localhost entries */
    flx_address_parse("127.0.0.1", AF_INET, &a);
    flx_server_add_address(s, 0, 0, AF_UNSPEC, TRUE, "localhost", &a);

    flx_address_parse("::1", AF_INET6, &a);
    flx_server_add_address(s, 0, 0, AF_UNSPEC, TRUE, "ip6-localhost", &a);
}

flxServer *flx_server_new(GMainContext *c) {
    gchar *hn, *e;
    flxServer *s;

    s = g_new(flxServer, 1);

    if (c)
        g_main_context_ref(s->context = c);
    else
        s->context = g_main_context_default();
    
    s->current_id = 1;
    s->rrset_by_id = g_hash_table_new(g_int_hash, g_int_equal);
    s->rrset_by_name = g_hash_table_new((GHashFunc) flx_key_hash, (GEqualFunc) flx_key_equal);

    FLX_LLIST_HEAD_INIT(flxEntry, s->entries);

    s->monitor = flx_interface_monitor_new(s);
    s->time_event_queue = flx_time_event_queue_new(s->context);
    
    /* Get host name */
    hn = flx_get_host_name();
    if ((e = strchr(hn, '.')))
        *e = 0;

    s->hostname = g_strdup_printf("%s.local.", hn);
    g_free(hn);

    add_default_entries(s);

    return s;
}

void flx_server_free(flxServer* s) {
    g_assert(s);

    flx_interface_monitor_free(s->monitor);
    
    flx_server_remove(s, 0);
    
    g_hash_table_destroy(s->rrset_by_id);
    g_hash_table_destroy(s->rrset_by_name);

    flx_time_event_queue_free(s->time_event_queue);
    g_main_context_unref(s->context);
    
    g_free(s->hostname);
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
    
    flxEntry *e, *t;
    g_assert(s);
    g_assert(r);

    e = g_new(flxEntry, 1);
    e->record = flx_record_ref(r);
    e->id = id;
    e->interface = interface;
    e->protocol = protocol;
    e->unique = unique;

    FLX_LLIST_PREPEND(flxEntry, entry, s->entries, e);

    /* Insert into hash table indexed by id */
    t = g_hash_table_lookup(s->rrset_by_id, &e->id);
    FLX_LLIST_PREPEND(flxEntry, by_id, t, e);
    g_hash_table_replace(s->rrset_by_id, &e->id, t);
    
    /* Insert into hash table indexed by name */
    t = g_hash_table_lookup(s->rrset_by_name, e->record->key);
    FLX_LLIST_PREPEND(flxEntry, by_name, t, e);
    g_hash_table_replace(s->rrset_by_name, e->record->key, t);
}

void flx_server_add_full(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    guint16 class,
    guint16 type,
    gconstpointer data,
    guint size,
    guint32 ttl) {
    
    flxRecord *r;
    g_assert(s);
    g_assert(data);
    g_assert(size);

    r = flx_record_new_full(name ? name : s->hostname, class, type, data, size, ttl);
    flx_server_add(s, id, interface, protocol, unique, r);
    flx_record_unref(r);
}

const flxRecord *flx_server_iterate(flxServer *s, gint id, void **state) {
    flxEntry **e = (flxEntry**) state;
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

static void free_entry(flxServer*s, flxEntry *e) {
    flxEntry *t;
    
    g_assert(e);

    /* Remove from linked list */
    FLX_LLIST_REMOVE(flxEntry, entry, s->entries, e);

    /* Remove from hash table indexed by id */
    t = g_hash_table_lookup(s->rrset_by_id, &e->id);
    FLX_LLIST_REMOVE(flxEntry, by_id, t, e);
    if (t)
        g_hash_table_replace(s->rrset_by_id, &t->id, t);
    else
        g_hash_table_remove(s->rrset_by_id, &e->id);
    
    /* Remove from hash table indexed by name */
    t = g_hash_table_lookup(s->rrset_by_name, e->record->key);
    FLX_LLIST_REMOVE(flxEntry, by_name, t, e);
    if (t)
        g_hash_table_replace(s->rrset_by_name, t->record->key, t);
    else
        g_hash_table_remove(s->rrset_by_name, e->record->key);

    flx_record_unref(e->record);
    g_free(e);
}

void flx_server_remove(flxServer *s, gint id) {
    g_assert(s);

    if (id <= 0) {
        while (s->entries)
            free_entry(s, s->entries);
    } else {
        flxEntry *e;

        while ((e = g_hash_table_lookup(s->rrset_by_id, &id)))
            free_entry(s, e);
    }
}

void flx_server_dump(flxServer *s, FILE *f) {
    flxEntry *e;
    g_assert(s);
    g_assert(f);

    for (e = s->entries; e; e = e->entry_next) {
        gchar *t;

        t = flx_record_to_string(e->record);
        fprintf(f, "%s\n", t);
        g_free(t);
    }
}

void flx_server_add_address(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    flxAddress *a) {

    gchar *n;
    g_assert(s);
    g_assert(a);

    n = name ? flx_normalize_name(name) : s->hostname;
    
    if (a->family == AF_INET) {
        gchar *r;
        
        flx_server_add_full(s, id, interface, protocol, unique, n, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_A, &a->ipv4, sizeof(a->ipv4), FLX_DEFAULT_TTL);

        r = flx_reverse_lookup_name_ipv4(&a->ipv4);
        g_assert(r);
        flx_server_add_full(s, id, interface, protocol, unique, r, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_PTR, n, strlen(n)+1, FLX_DEFAULT_TTL);
        g_free(r);
        
    } else {
        gchar *r;
            
        flx_server_add_full(s, id, interface, protocol, unique, n, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_AAAA, &a->ipv6, sizeof(a->ipv6), FLX_DEFAULT_TTL);

        r = flx_reverse_lookup_name_ipv6_arpa(&a->ipv6);
        g_assert(r);
        flx_server_add_full(s, id, interface, protocol, unique, r, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_PTR, n, strlen(n)+1, FLX_DEFAULT_TTL);
        g_free(r);
    
        r = flx_reverse_lookup_name_ipv6_int(&a->ipv6);
        g_assert(r);
        flx_server_add_full(s, id, interface, protocol, unique, r, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_PTR, n, strlen(n)+1, FLX_DEFAULT_TTL);
        g_free(r);
    }
    
    g_free(n);
}

void flx_server_add_text(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    const gchar *text) {
    
    g_assert(s);
    g_assert(text);

    flx_server_add_full(s, id, interface, protocol, unique, name, FLX_DNS_CLASS_IN, FLX_DNS_TYPE_TXT, text, strlen(text), FLX_DEFAULT_TTL);
}

void flx_server_send_query(flxServer *s, gint interface, guchar protocol, flxKey *k) {
    g_assert(s);
    g_assert(k);

    if (interface <= 0) {
        flxInterface *i;

        for (i = flx_interface_monitor_get_first(s->monitor); i; i = i->interface_next)
            flx_interface_send_query(i, protocol, k);
        
    } else {
        flxInterface *i;

        if (!(i = flx_interface_monitor_get_interface(s->monitor, interface)))
            return;

        flx_interface_send_query(i, protocol, k);
    }
}
