#ifndef fooflxserverhfoo
#define fooflxserverhfoo

typedef struct _flxServerEntry flxServerEntry;

#include "flx.h"
#include "iface.h"
#include "prioq.h"
#include "llist.h"
#include "timeeventq.h"
#include "announce.h"
#include "subscribe.h"

struct _flxServerEntry {
    flxRecord *record;
    gint id;
    gint interface;
    guchar protocol;

    flxServerEntryFlags flags;

    FLX_LLIST_FIELDS(flxServerEntry, entry);
    FLX_LLIST_FIELDS(flxServerEntry, by_key);
    FLX_LLIST_FIELDS(flxServerEntry, by_id);
    
    FLX_LLIST_HEAD(flxAnnouncement, announcements);
};

struct _flxServer {
    GMainContext *context;
    flxInterfaceMonitor *monitor;

    gint current_id;
    
    FLX_LLIST_HEAD(flxServerEntry, entries);
    GHashTable *rrset_by_id;
    GHashTable *rrset_by_key;

    FLX_LLIST_HEAD(flxSubscription, subscriptions);
    GHashTable *subscription_hashtable;

    flxTimeEventQueue *time_event_queue;
    
    gchar *hostname;

    gint fd_ipv4, fd_ipv6;

    GPollFD pollfd_ipv4, pollfd_ipv6;
    GSource *source;
    
};

gboolean flx_server_entry_match_interface(flxServerEntry *e, flxInterface *i);

#endif
