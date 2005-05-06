#ifndef fooutilhfoo
#define fooutilhfoo

#include <glib.h>

gchar *avahi_normalize_name(const gchar *s); /* g_free() the result! */
gchar *avahi_get_host_name(void); /* g_free() the result! */

gint avahi_timeval_compare(const GTimeVal *a, const GTimeVal *b);
glong avahi_timeval_diff(const GTimeVal *a, const GTimeVal *b);

gint avahi_set_cloexec(gint fd);
gint avahi_set_nonblock(gint fd);
gint avahi_wait_for_write(gint fd);

GTimeVal *avahi_elapse_time(GTimeVal *tv, guint msec, guint jitter);

gint avahi_age(const GTimeVal *a);

guint avahi_domain_hash(const gchar *p);
gboolean avahi_domain_cmp(const gchar *a, const gchar *b);
gboolean avahi_domain_equal(const gchar *a, const gchar *b);

void avahi_hexdump(gconstpointer p, guint size);

#endif
