#ifndef fooflxhfoo
#define fooflxhfoo

#include <stdio.h>
#include <glib.h>

typedef struct _flxServer flxServer;
typedef struct _flxEntry flxEntry;
typedef struct _flxEntryGroup flxEntryGroup;

#include "address.h"
#include "rr.h"

typedef enum {
    FLX_ENTRY_NULL = 0,
    FLX_ENTRY_UNIQUE = 1,
    FLX_ENTRY_NOPROBE = 2,
    FLX_ENTRY_NOANNOUNCE = 4
} flxEntryFlags;

typedef enum {
    FLX_ENTRY_GROUP_UNCOMMITED,
    FLX_ENTRY_GROUP_REGISTERING,
    FLX_ENTRY_GROUP_ESTABLISHED,
    FLX_ENTRY_GROUP_COLLISION
} flxEntryGroupState;

typedef void (*flxEntryGroupCallback) (flxServer *s, flxEntryGroup *g, flxEntryGroupState state, gpointer userdata);

flxServer *flx_server_new(GMainContext *c);
void flx_server_free(flxServer* s);

const flxRecord *flx_server_iterate(flxServer *s, flxEntryGroup *g, void **state);
void flx_server_dump(flxServer *s, FILE *f);

flxEntryGroup *flx_entry_group_new(flxServer *s, flxEntryGroupCallback callback, gpointer userdata);
void flx_entry_group_free(flxEntryGroup *g);
void flx_entry_group_commit(flxEntryGroup *g);
flxEntryGroupState flx_entry_group_get_state(flxEntryGroup *g);

void flx_server_add(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    flxRecord *r);

void flx_server_add_ptr(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    const gchar *dest);

void flx_server_add_address(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    flxAddress *a);

void flx_server_add_text(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    ... /* text records, terminated by NULL */);

void flx_server_add_text_va(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    va_list va);

void flx_server_add_text_strlst(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    flxEntryFlags flags,
    const gchar *name,
    flxStringList *strlst);

void flx_server_add_service(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    ...  /* text records, terminated by NULL */);

void flx_server_add_service_va(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    va_list va);

void flx_server_add_service_strlst(
    flxServer *s,
    flxEntryGroup *g,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    flxStringList *strlst);

typedef enum {
    FLX_SUBSCRIPTION_NEW,
    FLX_SUBSCRIPTION_REMOVE,
    FLX_SUBSCRIPTION_CHANGE
} flxSubscriptionEvent;

typedef struct _flxSubscription flxSubscription;

typedef void (*flxSubscriptionCallback)(flxSubscription *s, flxRecord *record, gint interface, guchar protocol, flxSubscriptionEvent event, gpointer userdata);

flxSubscription *flx_subscription_new(flxServer *s, flxKey *key, gint interface, guchar protocol, flxSubscriptionCallback callback, gpointer userdata);
void flx_subscription_free(flxSubscription *s);

#endif
