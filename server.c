#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/utsname.h>

#include "server.h"
#include "util.h"
#include "iface.h"

static gint timeval_cmp(const GTimeVal *a, const GTimeVal *b) {
    g_assert(a);
    g_assert(b);

    if (a->tv_sec < b->tv_sec)
        return -1;

    if (a->tv_sec > b->tv_sec)
        return 1;

    if (a->tv_usec < b->tv_usec)
        return -1;

    if (a->tv_usec > b->tv_usec)
        return 1;

    return 0;
}

static gint query_job_instance_compare(gpointer a, gpointer b) {
    flxQueryJobInstance *j = a, *k = b;
    g_assert(j);
    g_assert(k);

    return timeval_cmp(&j->job->time, &k->job->time);
}

static gint response_job_instance_compare(gpointer a, gpointer b) {
    flxResponseJobInstance *j = a, *k = b;
    g_assert(j);
    g_assert(k);

    return timeval_cmp(&j->job->time, &k->job->time);
}

flxServer *flx_server_new(GMainContext *c) {
    gchar *hn, *e, *hinfo;
    struct utsname utsname;
    gint length;
    flxAddress a;
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

    s->query_job_queue = flx_prio_queue_new(query_job_instance_compare);
    s->response_job_queue = flx_prio_queue_new(response_job_instance_compare);
    
    s->monitor = flx_interface_monitor_new(s);

    /* Get host name */
    hn = flx_get_host_name();
    if ((e = strchr(hn, '.')))
        *e = 0;

    s->hostname = g_strdup_printf("%s.local.", hn);
    g_free(hn);

    /* Fill in HINFO rr */
    s->hinfo_rr_id = flx_server_get_next_id(s);

    uname(&utsname);
    hinfo = g_strdup_printf("%s%c%s%n", g_strup(utsname.machine), 0, g_strup(utsname.sysname), &length);
    
    flx_server_add(s, s->hinfo_rr_id, 0, AF_UNSPEC,
                   s->hostname, FLX_DNS_TYPE_HINFO, hinfo, length+1);


    /* Add localhost entries */
    flx_address_parse("127.0.0.1", AF_INET, &a);
    flx_server_add_address(s, 0, 0, AF_UNSPEC, "localhost", &a);

    flx_address_parse("::1", AF_INET6, &a);
    flx_server_add_address(s, 0, 0, AF_UNSPEC, "ip6-localhost", &a);

    return s;
}

void flx_server_free(flxServer* s) {
    g_assert(s);

    while (s->query_job_queue->last)
        flx_server_remove_query_job_instance(s, s->query_job_queue->last->data);
    
    flx_prio_queue_free(s->query_job_queue);
    flx_prio_queue_free(s->response_job_queue);

    flx_interface_monitor_free(s->monitor);
    
    flx_server_remove(s, 0);
    
    g_hash_table_destroy(s->rrset_by_id);
    g_hash_table_destroy(s->rrset_by_name);
    g_main_context_unref(s->context);

    g_free(s->hostname);
    g_free(s);
}

gint flx_server_get_next_id(flxServer *s) {
    g_assert(s);

    return s->current_id++;
}

void flx_server_add_rr(flxServer *s, gint id, gint interface, guchar protocol, const flxRecord *rr) {
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
    e->protocol = protocol;

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

void flx_server_add(flxServer *s, gint id, gint interface, guchar protocol, const gchar *name, guint16 type, gconstpointer data, guint size) {
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
    rr.ttl = FLX_DEFAULT_TTL;
    flx_server_add_rr(s, id, interface, protocol, &rr);
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
        fprintf(f, "%i.%u: %-40s %-8s %-8s ", e->interface, e->protocol, e->rr.name, dns_class_to_string(e->rr.class), dns_type_to_string(e->rr.type));

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
                
            } else if (e->rr.type == FLX_DNS_TYPE_TXT) {
                size_t l;

                l = e->rr.size;
                if (l > sizeof(t)-1)
                    l = sizeof(t)-1;

                memcpy(t, e->rr.data, l);
                t[l] = 0;
            }
        }
            
        fprintf(f, "%s\n", t);
    }
}

void flx_server_add_address(flxServer *s, gint id, gint interface, guchar protocol, const gchar *name, flxAddress *a) {
    gchar *n;
    g_assert(s);
    g_assert(a);

    n = flx_normalize_name(name ? name : s->hostname);
    
    if (a->family == AF_INET) {
        gchar *r;
        
        flx_server_add(s, id, interface, protocol, n, FLX_DNS_TYPE_A, &a->ipv4, sizeof(a->ipv4));

        r = flx_reverse_lookup_name_ipv4(&a->ipv4);
        g_assert(r);
        flx_server_add(s, id, interface, protocol, r, FLX_DNS_TYPE_PTR, n, strlen(n)+1);
        g_free(r);
        
    } else {
        gchar *r;
            
        flx_server_add(s, id, interface, protocol, n, FLX_DNS_TYPE_AAAA, &a->ipv6, sizeof(a->ipv6));

        r = flx_reverse_lookup_name_ipv6_arpa(&a->ipv6);
        g_assert(r);
        flx_server_add(s, id, interface, protocol, r, FLX_DNS_TYPE_PTR, n, strlen(n)+1);
        g_free(r);
    
        r = flx_reverse_lookup_name_ipv6_int(&a->ipv6);
        g_assert(r);
        flx_server_add(s, id, interface, protocol, r, FLX_DNS_TYPE_PTR, n, strlen(n)+1);
        g_free(r);
    }
    
    g_free(n);
}

void flx_server_add_text(flxServer *s, gint id, gint interface, guchar protocol, const gchar *name, const gchar *text) {
    gchar *n;
    g_assert(s);
    g_assert(text);

    n = flx_normalize_name(name ? name : s->hostname);
    flx_server_add(s, id, interface, protocol, n, FLX_DNS_TYPE_TXT, text, strlen(text));
    g_free(n);
}


flxQueryJob* flx_query_job_new(void) {
    flxQueryJob *job = g_new(flxQueryJob, 1);
    job->query.name = NULL;
    job->query.class = 0;
    job->query.type = 0;
    job->ref = 1;
    job->time.tv_sec = 0;
    job->time.tv_usec = 0;
    return job;
}

flxQueryJob* flx_query_job_ref(flxQueryJob *job) {
    g_assert(job);
    g_assert(job->ref >= 1);
    job->ref++;
    return job;
}

void flx_query_job_unref(flxQueryJob *job) {
    g_assert(job);
    g_assert(job->ref >= 1);
    if (!(--job->ref))
        g_free(job);
}

static gboolean query_job_exists(flxServer *s, gint interface, guchar protocol, flxQuery *q) {
    flxPrioQueueNode *n;
    g_assert(s);
    g_assert(q);

    for (n = s->query_job_queue->root; n; n = n->next)
        if (flx_query_equal(&((flxQueryJobInstance*) n->data)->job->query, q))
            return TRUE;

    return FALSE;
}

static void post_query_job(flxServer *s, gint interface, guchar protocol, flxQueryJob *job) {
    g_assert(s);
    g_assert(job);

    if (interface <= 0) {
        const flxInterface *i;
        
        for (i = flx_interface_monitor_get_first(s->monitor); i; i = i->next)
            post_query_job(s, i->index, protocol, job);
    } else if (protocol == AF_UNSPEC) {
        post_query_job(s, interface, AF_INET, job);
        post_query_job(s, interface, AF_INET6, job);
    } else {
        flxQueryJobInstance *i;

        if (query_job_exists(s, interface, protocol, &job->query))
            return;
        
        i = g_new(flxQueryJobInstance, 1);
        i->job = flx_query_job_ref(job);
        i->interface = interface;
        i->protocol = protocol;
        i->node = flx_prio_queue_put(s->query_job_queue, i);
    }
}

void flx_server_post_query_job(flxServer *s, gint interface, guchar protocol, const GTimeVal *tv, const flxQuery *q) {
    flxQueryJob *job;
    g_assert(s);
    g_assert(q);

    job = flx_query_job_new();
    job->query.name = g_strdup(q->name);
    job->query.class = q->class;
    job->query.type = q->type;
    if (tv)
        job->time = *tv;
    post_query_job(s, interface, protocol, job);
}

void flx_server_drop_query_job(flxServer *s, gint interface, guchar protocol, const flxQuery *q) {
    flxPrioQueueNode *n, *next;
    g_assert(s);
    g_assert(interface > 0);
    g_assert(protocol != AF_UNSPEC);
    g_assert(q);

    for (n = s->query_job_queue->root; n; n = next) {
        next = n->next;
    
        if (flx_query_equal(&((flxQueryJobInstance*) n->data)->job->query, q))
            flx_server_remove_query_job_instance(s, n->data);
    }
}

void flx_server_remove_query_job_instance(flxServer *s, flxQueryJobInstance *i) {
    g_assert(s);
    g_assert(i);
    g_assert(i->node);

    flx_prio_queue_remove(s->query_job_queue, i->node);
    flx_query_job_unref(i->job);
    g_free(i);
}

gboolean flx_query_equal(const flxQuery *a, const flxQuery *b) {
    return strcmp(a->name, b->name) == 0 && a->type == b->type && a->class == b->class;
}

