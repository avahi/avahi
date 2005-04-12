#ifndef footxtlisthfoo
#define footxtlisthfoo

#include <glib.h>

typedef struct _flxStringList flxStringList;

struct _flxStringList {
    flxStringList *next;
    gchar text[1];
};

flxStringList *flx_string_list_new(const gchar *txt, ...);
flxStringList *flx_string_list_new_va(va_list va);

void flx_string_list_free(flxStringList *l);

flxStringList *flx_string_list_add(flxStringList *l, const gchar *text);
flxStringList *flx_string_list_add_many(flxStringList *r, ...);
flxStringList *flx_string_list_add_many_va(flxStringList *r, va_list va);

gchar* flx_string_list_to_string(flxStringList *l);

guint flx_string_list_serialize(flxStringList *l, gpointer data, guint size);
flxStringList *flx_string_list_parse(gconstpointer data, guint size);

gboolean flx_string_list_equal(flxStringList *a, flxStringList *b);

flxStringList *flx_string_list_copy(flxStringList *l);

#endif

