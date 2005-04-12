#ifndef fooflxhfoo
#define fooflxhfoo

#include <stdio.h>
#include <glib.h>

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

void flx_server_add_ptr(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    const gchar *dest);

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
    ... /* text records, terminated by NULL */);

void flx_server_add_text_va(
    flxServer *s,
    gint id,
    gint interface,
    guchar protocol,
    gboolean unique,
    const gchar *name,
    va_list va);

void flx_server_add_service(
    flxServer *s,
    gint id,
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
    gint id,
    gint interface,
    guchar protocol,
    const gchar *type,
    const gchar *name,
    const gchar *domain,
    const gchar *host,
    guint16 port,
    va_list va);


void flx_server_remove(flxServer *s, gint id);

void flx_server_post_query(flxServer *s, gint interface, guchar protocol, flxKey *key);
void flx_server_post_response(flxServer *s, gint interface, guchar protocol, flxRecord *record);

const flxRecord *flx_server_iterate(flxServer *s, gint id, void **state);

void flx_server_dump(flxServer *s, FILE *f);

#endif
