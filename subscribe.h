#ifndef foosubscribehfoo
#define foosubscribehfoo

typedef struct _flxSubscription flxSubscription;

#include "llist.h"
#include "server.h"

typedef enum {
    FLX_SUBSCRIPTION_NEW,
    FLX_SUBSCRIPTION_REMOVE,
    FLX_SUBSCRIPTION_CHANGE
} flxSubscriptionEvent;

typedef void (*flxSubscriptionCallback)(flxSubscription *s, flxRecord *record, gint interface, guchar protocol, flxSubscriptionEvent event, gpointer userdata);

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

flxSubscription *flx_subscription_new(flxServer *s, flxKey *key, gint interface, guchar protocol, flxSubscriptionCallback callback, gpointer userdata);
void flx_subscription_free(flxSubscription *s);

void flx_subscription_notify(flxServer *s, flxInterface *i, flxRecord *record, flxSubscriptionEvent event);

gboolean flx_is_subscribed(flxServer *s, flxKey *k);

#endif
