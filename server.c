#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include "server.h"
#include "util.h"

flxServer *flx_server_new(GMainContext *c) {
    flxServer *s = g_new(flxServer, 1);

    if (c) {
        g_main_context_ref(c);
        s->context = c;
    } else
        s->context = g_main_context_default();
    
    s->current_id = 1;
    s->rrset_by_id = g_hash_table_new(g_int_hash, g_int_equal);
    s->rrset_by_name = g_hash_table_new(g_str_hash, g_str_equal);
    s->entries = NULL;

    s->monitor = flx_interface_monitor_new(s->context);
    
    return s;
}

void flx_server_free(flxServer* s) {
    g_assert(s);

    flx_interface_monitor_free(s->monitor);

    flx_server_remove(s, 0);
    
    g_hash_table_destroy(s->rrset_by_id);
    g_hash_table_destroy(s->rrset_by_name);
    g_main_context_unref(s->context);
    g_free(s);
}

gint flx_server_get_next_id(flxServer *s) {
    g_assert(s);

    return s->current_id++;
}

void flx_server_add_rr(flxServer *s, gint id, gint interface, const flxRecord *rr) {
    flxEntry *e;
    g_assert(s);
    g_assert(rr);
    g_assert(rr->name);
    g_assert(rr->data);
    g_assert(rr->size);

    e = g_new(flxEntry, 1);
    flx_record_copy_normalize(&e->rr, rr);
    e->id = id;
    e->interface = interface;

    /* Insert into linked list */
    e->prev = NULL;
    if ((e->next = s->entries))
        e->next->prev = e;
    s->entries = e;

    /* Insert into hash table indexed by id */
    e->prev_by_id = NULL;
    if ((e->next_by_id = g_hash_table_lookup(s->rrset_by_id, &id)))
        e->next_by_id->prev = e;
    g_hash_table_replace(s->rrset_by_id, &e->id, e);

    /* Insert into hash table indexed by name */
    e->prev_by_name = NULL;
    if ((e->next_by_name = g_hash_table_lookup(s->rrset_by_name, e->rr.name)))
        e->next_by_name->prev = e;
    g_hash_table_replace(s->rrset_by_name, e->rr.name, e);
}

void flx_server_add(flxServer *s, gint id, const gchar *name, gint interface, guint16 type, gconstpointer data, guint size) {
    flxRecord rr;
    g_assert(s);
    g_assert(name);
    g_assert(data);
    g_assert(size);

    rr.name = (gchar*) name;
    rr.type = type;
    rr.class = FLX_DNS_CLASS_IN;
    rr.data = (gpointer) data;
    rr.size = size;
    rr.interface = interface;
    rr.ttl = FLX_DEFAULT_TTL;
    flx_server_add_rr(s, id, 0, &rr);
}

const flxRecord *flx_server_iterate(flxServer *s, gint id, void **state) {
    flxEntry **e = (flxEntry**) state;
    g_assert(s);
    g_assert(e);

    if (e)
        *e = id > 0 ? (*e)->next_by_id : (*e)->next;
    else
        *e = id > 0 ? g_hash_table_lookup(s->rrset_by_id, &id) : s->entries;
        
    if (!*e)
        return NULL;

    return &(*e)->rr;
}

static void free_entry(flxServer*s, flxEntry *e) {
    g_assert(e);

    /* Remove from linked list */
    if (e->prev)
        e->prev->next = e->next;
    else
        s->entries = e->next;
    
    if (e->next)
        e->next->prev = e->prev;

    /* Remove from hash table indexed by id */
    if (e->prev_by_id)
        e->prev_by_id = e->next_by_id;
    else {
        if (e->next_by_id)
            g_hash_table_replace(s->rrset_by_id, &e->next_by_id->id, e->next_by_id);
        else
            g_hash_table_remove(s->rrset_by_id, &e->id);
    }

    if (e->next_by_id)
        e->next_by_id->prev_by_id = e->prev_by_id;

    /* Remove from hash table indexed by name */
    if (e->prev_by_name)
        e->prev_by_name = e->next_by_name;
    else {
        if (e->next_by_name)
            g_hash_table_replace(s->rrset_by_name, &e->next_by_name->rr.name, e->next_by_name);
        else
            g_hash_table_remove(s->rrset_by_name, &e->rr.name);
    }
    
    if (e->next_by_name)
        e->next_by_name->prev_by_name = e->prev_by_name;
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

flxRecord *flx_record_copy_normalize(flxRecord *ret_dest, const flxRecord*src) {
    g_assert(ret_dest);
    g_assert(src);

    *ret_dest = *src;
    ret_dest->name = flx_normalize_name(src->name);
    ret_dest->data = g_memdup(src->data, src->size);

    return ret_dest;    
}

static const gchar *dns_class_to_string(guint16 class) {
    if (class == FLX_DNS_CLASS_IN)
        return "IN";

    return NULL;
}

static const gchar *dns_type_to_string(guint16 type) {
    switch (type) {
        case FLX_DNS_TYPE_A:
            return "A";
        case FLX_DNS_TYPE_AAAA:
            return "AAAA";
        case FLX_DNS_TYPE_PTR:
            return "PTR";
        case FLX_DNS_TYPE_HINFO:
            return "HINFO";
        case FLX_DNS_TYPE_TXT:
            return "TXT";
        default:
            return NULL;
    }
}

void flx_server_dump(flxServer *s, FILE *f) {
    flxEntry *e;
    g_assert(s);
    g_assert(f);

    for (e = s->entries; e; e = e->next) {
        char t[256];
        fprintf(f, "%-40s %-8s %-8s ", e->rr.name, dns_class_to_string(e->rr.class), dns_type_to_string(e->rr.type));

        t[0] = 0;
        
        if (e->rr.class == FLX_DNS_CLASS_IN) {
            if (e->rr.type == FLX_DNS_TYPE_A)
                inet_ntop(AF_INET, e->rr.data, t, sizeof(t));
            else if (e->rr.type == FLX_DNS_TYPE_AAAA)
                inet_ntop(AF_INET6, e->rr.data, t, sizeof(t));
            else if (e->rr.type == FLX_DNS_TYPE_PTR)
                g_strlcpy(t, e->rr.data, sizeof(t));
            else if (e->rr.type == FLX_DNS_TYPE_HINFO) {
                char *s2;

                if ((s2 = memchr(e->rr.data, 0, e->rr.size))) {
                    s2++;
                    if (memchr(s2, 0, e->rr.size - ((char*) s2 - (char*) e->rr.data)))
                        snprintf(t, sizeof(t), "'%s' '%s'", (char*) e->rr.data, s2);
                }
                
            }
        }
            
        fprintf(f, "%s\n", t);
    }
}

