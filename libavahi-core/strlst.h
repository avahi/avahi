#ifndef footxtlisthfoo
#define footxtlisthfoo

#include <glib.h>

typedef struct _AvahiStringList AvahiStringList;

struct _AvahiStringList {
    AvahiStringList *next;
    guint size;
    guint8 text[1];
};

AvahiStringList *avahi_string_list_new(const gchar *txt, ...);
AvahiStringList *avahi_string_list_new_va(va_list va);

void avahi_string_list_free(AvahiStringList *l);

AvahiStringList *avahi_string_list_add(AvahiStringList *l, const gchar *text);
AvahiStringList *avahi_string_list_add_arbitrary(AvahiStringList *l, const guint8 *text, guint size);
AvahiStringList *avahi_string_list_add_many(AvahiStringList *r, ...);
AvahiStringList *avahi_string_list_add_many_va(AvahiStringList *r, va_list va);

gchar* avahi_string_list_to_string(AvahiStringList *l);

guint avahi_string_list_serialize(AvahiStringList *l, gpointer data, guint size);
AvahiStringList *avahi_string_list_parse(gconstpointer data, guint size);

gboolean avahi_string_list_equal(AvahiStringList *a, AvahiStringList *b);

AvahiStringList *avahi_string_list_copy(AvahiStringList *l);

#endif

