#ifndef fooflxhfoo
#define fooflxhfoo

#include <stdio.h>
#include <glib.h>

struct _flxServer;
typedef struct _flxServer flxServer;

#include "address.h"
#include "rr.h"

flxServer *flx_server_new(GMainContext *c);
void flx_server_free(flxServer* s);

gint flx_server_get_next_id(flxServer *s);

void flx_server_add(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    flxRecord *r);

void flx_server_add_full(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    guint16 class,
    guint16 type,
    gconstpointer data,
    guint size,
    guint32 ttl);

void flx_server_add_address(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    flxAddress *a);

void flx_server_add_text(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    const gchar *text);

void flx_server_remove(flxServer *s, gint id);

void flx_server_send_query(flxServer *s, gint interface, guchar protocol, flxKey *k);

const flxRecord *flx_server_iterate(flxServer *s, gint id, void **state);

void flx_server_dump(flxServer *s, FILE *f);

#endif
