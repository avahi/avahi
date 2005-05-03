#ifndef fooflxserverhfoo
#define fooflxserverhfoo

#include "flx.h"
#include "iface.h"
#include "prioq.h"
#include "llist.h"
#include "timeeventq.h"
#include "announce.h"
#include "subscribe.h"

struct _flxEntry {
    flxServer *server;
    flxEntryGroup *group;

    gboolean dead;
    
    flxEntryFlags flags;
    flxRecord *record;
    gint interface;
    guchar protocol;

    FLX_LLIST_FIELDS(flxEntry, entries);
    FLX_LLIST_FIELDS(flxEntry, by_key);
    FLX_LLIST_FIELDS(flxEntry, by_group);
    
    FLX_LLIST_HEAD(flxAnnouncement, announcements);
};

struct _flxEntryGroup {
    flxServer *server;
    gboolean dead;

    flxEntryGroupState state;
    gpointer userdata;
    flxEntryGroupCallback callback;

    guint n_probing;
    
    FLX_LLIST_FIELDS(flxEntryGroup, groups);
    FLX_LLIST_HEAD(flxEntry, entries);
};

struct _flxServer {
    GMainContext *context;
    flxInterfaceMonitor *monitor;

    FLX_LLIST_HEAD(flxEntry, entries);
    GHashTable *entries_by_key;

    FLX_LLIST_HEAD(flxEntryGroup, groups);
    
    FLX_LLIST_HEAD(flxSubscription, subscriptions);
    GHashTable *subscription_hashtable;

    gboolean need_entry_cleanup, need_group_cleanup;
    
    flxTimeEventQueue *time_event_queue;
    
    gchar *hostname;

    gint fd_ipv4, fd_ipv6;

    GPollFD pollfd_ipv4, pollfd_ipv6;
    GSource *source;

    gboolean ignore_bad_ttl;
};

gboolean flx_server_entry_match_interface(flxEntry *e, flxInterface *i);

void flx_server_post_query(flxServer *s, gint interface, guchar protocol, flxKey *key);
void flx_server_post_response(flxServer *s, gint interface, guchar protocol, flxRecord *record, gboolean flush_cache);

void flx_entry_group_run_callback(flxEntryGroup *g, flxEntryGroupState state);

gboolean flx_entry_commited(flxEntry *e);

#endif
