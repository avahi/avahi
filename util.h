#ifndef fooutilhfoo
#define fooutilhfoo

#include <glib.h>

gchar *flx_normalize_name(const gchar *s); /* g_free() the result! */
gchar *flx_get_host_name(void); /* g_free() the result! */

gint flx_timeval_compare(const GTimeVal *a, const GTimeVal *b);
glong flx_timeval_diff(const GTimeVal *a, const GTimeVal *b);

gint flx_set_cloexec(gint fd);
gint flx_set_nonblock(gint fd);
gint flx_wait_for_write(gint fd);

GTimeVal *flx_elapse_time(GTimeVal *tv, guint msec, guint jitter);

gint flx_age(const GTimeVal *a);

guint flx_domain_hash(const gchar *p);
gboolean flx_domain_equal(const gchar *a, const gchar *b);

#endif
