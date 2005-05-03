#ifndef foosubscribehfoo
#define foosubscribehfoo

#include "llist.h"
#include "flx.h"
#include "subscribe.h"
#include "timeeventq.h"
#include "server.h"

struct _flxSubscription {
    flxServer *server;
    flxKey *key;
    gint interface;
    guchar protocol;
    gint n_query;
    guint sec_delay;

    flxTimeEvent *time_event;

    flxSubscriptionCallback callback;
    gpointer userdata;

    FLX_LLIST_FIELDS(flxSubscription, subscriptions);
    FLX_LLIST_FIELDS(flxSubscription, by_key);
};

void flx_subscription_notify(flxServer *s, flxInterface *i, flxRecord *record, flxSubscriptionEvent event);

gboolean flx_is_subscribed(flxServer *s, flxKey *k);

#endif
