#include "subscribe.h"
#include "util.h"

static void elapse(flxTimeEvent *e, void *userdata) {
    flxSubscription *s = userdata;
    GTimeVal tv;
    gchar *t;
    
    g_assert(s);

    flx_server_post_query(s->server, s->interface, s->protocol, s->key);

    if (s->n_query++ <= 8)
        s->sec_delay *= 2;

    g_message("%i. Continuous querying for %s", s->n_query, t = flx_key_to_string(s->key));
    g_free(t);
    
    flx_elapse_time(&tv, s->sec_delay*1000, 0);
    flx_time_event_queue_update(s->server->time_event_queue, s->time_event, &tv);
}

static void scan_cache_callback(flxInterfaceMonitor *m, flxInterface *i, gpointer userdata) {
    flxSubscription *s = userdata;
    flxCacheEntry *e;

    g_assert(m);
    g_assert(i);
    g_assert(s);

    for (e = flx_cache_lookup_key(i->cache, s->key); e; e = e->by_name_next)
        s->callback(s, e->record, i->hardware->index, i->protocol, FLX_SUBSCRIPTION_NEW, s->userdata);
}

flxSubscription *flx_subscription_new(flxServer *server, flxKey *key, gint interface, guchar protocol, flxSubscriptionCallback callback, gpointer userdata) {
    flxSubscription *s, *t;
    GTimeVal tv;

    g_assert(server);
    g_assert(key);
    g_assert(callback);

    s = g_new(flxSubscription, 1);
    s->server = server;
    s->key = flx_key_ref(key);
    s->interface = interface;
    s->protocol = protocol;
    s->callback = callback;
    s->userdata = userdata;
    s->n_query = 1;
    s->sec_delay = 1;

    flx_server_post_query(s->server, s->interface, s->protocol, s->key);
    
    flx_elapse_time(&tv, s->sec_delay*1000, 0);
    s->time_event = flx_time_event_queue_add(server->time_event_queue, &tv, elapse, s);

    FLX_LLIST_PREPEND(flxSubscription, subscriptions, server->subscriptions, s);

    /* Add the new entry to the subscription hash table */
    t = g_hash_table_lookup(server->subscription_hashtable, key);
    FLX_LLIST_PREPEND(flxSubscription, by_key, t, s);
    g_hash_table_replace(server->subscription_hashtable, key, t);

    /* Scan the caches */
    flx_interface_monitor_walk(s->server->monitor, s->interface, s->protocol, scan_cache_callback, s);
    
    return s;
}

void flx_subscription_free(flxSubscription *s) {
    flxSubscription *t;
    
    g_assert(s);

    FLX_LLIST_REMOVE(flxSubscription, subscriptions, s->server->subscriptions, s);

    t = g_hash_table_lookup(s->server->subscription_hashtable, s->key);
    FLX_LLIST_REMOVE(flxSubscription, by_key, t, s);
    if (t)
        g_hash_table_replace(s->server->subscription_hashtable, t->key, t);
    else
        g_hash_table_remove(s->server->subscription_hashtable, s->key);
    
    flx_time_event_queue_remove(s->server->time_event_queue, s->time_event);
    flx_key_unref(s->key);

    
    g_free(s);
}

void flx_subscription_notify(flxServer *server, flxInterface *i, flxRecord *record, flxSubscriptionEvent event) {
    flxSubscription *s;
    
    g_assert(server);
    g_assert(record);

    for (s = g_hash_table_lookup(server->subscription_hashtable, record->key); s; s = s->by_key_next)
        if (flx_interface_match(i, s->interface, s->protocol))
            s->callback(s, record, i->hardware->index, i->protocol, event, s->userdata);
    
}
