#ifndef fooutilhfoo
#define fooutilhfoo

#include <glib.h>

gchar *flx_normalize_name(const gchar *s); /* g_free() the result! */
gchar *flx_get_host_name(void); /* g_free() the result! */

gint flx_timeval_compare(const GTimeVal *a, const GTimeVal *b);
glong flx_timeval_diff(const GTimeVal *a, const GTimeVal *b);

#endif
