#ifndef fooannouncehfoo
#define fooannouncehfoo

#include <glib.h>

typedef struct _flxAnnouncement flxAnnouncement;

#include "llist.h"
#include "iface.h"
#include "server.h"
#include "timeeventq.h"

typedef enum {
    FLX_PROBING,
    FLX_WAITING,         /* wait for other records in group */
    FLX_ANNOUNCING,
    FLX_ESTABLISHED
} flxAnnouncementState;

struct _flxAnnouncement {
    flxServer *server;
    flxInterface *interface;
    flxEntry *entry;

    flxTimeEvent *time_event;

    flxAnnouncementState state;
    guint n_iteration;
    guint sec_delay;

    FLX_LLIST_FIELDS(flxAnnouncement, by_interface);
    FLX_LLIST_FIELDS(flxAnnouncement, by_entry);
};

void flx_announce_interface(flxServer *s, flxInterface *i);
void flx_announce_entry(flxServer *s, flxEntry *e);
void flx_announce_group(flxServer *s, flxEntryGroup *g);

void flx_entry_group_check_probed(flxEntryGroup *g, gboolean immediately);

gboolean flx_entry_registered(flxServer *s, flxEntry *e, flxInterface *i);
gboolean flx_entry_registering(flxServer *s, flxEntry *e, flxInterface *i);

void flx_goodbye_interface(flxServer *s, flxInterface *i, gboolean send);
void flx_goodbye_entry(flxServer *s, flxEntry *e, gboolean send);

void flx_goodbye_all(flxServer *s, gboolean send);

flxAnnouncement *flx_get_announcement(flxServer *s, flxEntry *e, flxInterface *i);

#endif
