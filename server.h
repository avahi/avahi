#ifndef fooAvahiserverhfoo
#define fooAvahiserverhfoo

#include "avahi.h"
#include "iface.h"
#include "prioq.h"
#include "llist.h"
#include "timeeventq.h"
#include "announce.h"
#include "subscribe.h"

struct _AvahiEntry {
    AvahiServer *server;
    AvahiEntryGroup *group;

    gboolean dead;
    
    AvahiEntryFlags flags;
    AvahiRecord *record;
    gint interface;
    guchar protocol;

    AVAHI_LLIST_FIELDS(AvahiEntry, entries);
    AVAHI_LLIST_FIELDS(AvahiEntry, by_key);
    AVAHI_LLIST_FIELDS(AvahiEntry, by_group);
    
    AVAHI_LLIST_HEAD(AvahiAnnouncement, announcements);
};

struct _AvahiEntryGroup {
    AvahiServer *server;
    gboolean dead;

    AvahiEntryGroupState state;
    gpointer userdata;
    AvahiEntryGroupCallback callback;

    guint n_probing;
    
    AVAHI_LLIST_FIELDS(AvahiEntryGroup, groups);
    AVAHI_LLIST_HEAD(AvahiEntry, entries);
};

struct _AvahiServer {
    GMainContext *context;
    AvahiInterfaceMonitor *monitor;

    AVAHI_LLIST_HEAD(AvahiEntry, entries);
    GHashTable *entries_by_key;

    AVAHI_LLIST_HEAD(AvahiEntryGroup, groups);
    
    AVAHI_LLIST_HEAD(AvahiSubscription, subscriptions);
    GHashTable *subscription_hashtable;

    gboolean need_entry_cleanup, need_group_cleanup;
    
    AvahiTimeEventQueue *time_event_queue;
    
    gchar *hostname;

    gint fd_ipv4, fd_ipv6;

    GPollFD pollfd_ipv4, pollfd_ipv6;
    GSource *source;

    gboolean ignore_bad_ttl;
};

gboolean avahi_server_entry_match_interface(AvahiEntry *e, AvahiInterface *i);

void avahi_server_post_query(AvahiServer *s, gint interface, guchar protocol, AvahiKey *key);
void avahi_server_post_response(AvahiServer *s, gint interface, guchar protocol, AvahiRecord *record, gboolean flush_cache);

void avahi_entry_group_change_state(AvahiEntryGroup *g, AvahiEntryGroupState state);

gboolean avahi_entry_commited(AvahiEntry *e);

#endif
