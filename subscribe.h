#ifndef foosubscribehfoo
#define foosubscribehfoo

#include "llist.h"
#include "Avahi.h"
#include "subscribe.h"
#include "timeeventq.h"
#include "server.h"

struct _AvahiSubscription {
    AvahiServer *server;
    AvahiKey *key;
    gint interface;
    guchar protocol;
    gint n_query;
    guint sec_delay;

    AvahiTimeEvent *time_event;

    AvahiSubscriptionCallback callback;
    gpointer userdata;

    AVAHI_LLIST_FIELDS(AvahiSubscription, subscriptions);
    AVAHI_LLIST_FIELDS(AvahiSubscription, by_key);
};

void avahi_subscription_notify(AvahiServer *s, AvahiInterface *i, AvahiRecord *record, AvahiSubscriptionEvent event);

gboolean avahi_is_subscribed(AvahiServer *s, AvahiKey *k);

#endif
