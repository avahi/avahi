#ifndef fooannouncehfoo
#define fooannouncehfoo

#include <glib.h>

typedef struct _flxAnnouncement flxAnnouncement;

#include "llist.h"
#include "iface.h"
#include "server.h"
#include "timeeventq.h"

struct _flxAnnouncement {
    flxServer *server;
    flxInterface *interface;
    flxServerEntry *entry;
    
    flxTimeEvent *time_event;
    guint n_announced;

    FLX_LLIST_FIELDS(flxAnnouncement, by_interface);
    FLX_LLIST_FIELDS(flxAnnouncement, by_entry);
};

void flx_announce_interface(flxServer *s, flxInterface *i);
void flx_announce_entry(flxServer *s, flxServerEntry *e);

void flx_goodbye_interface(flxServer *s, flxInterface *i, gboolean send);
void flx_goodbye_entry(flxServer *s, flxServerEntry *e, gboolean send);

void flx_goodbye_all(flxServer *s, gboolean send);

#endif
