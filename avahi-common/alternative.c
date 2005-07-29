/* $Id$ */

/***
  This file is part of avahi.
 
  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.
 
  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "alternative.h"

gchar * avahi_alternative_host_name(const gchar *s) {
    const gchar *p, *e = NULL;
    gchar *c, *r;
    gint n;

    g_assert(s);
    
    for (p = s; *p; p++)
        if (!isdigit(*p))
            e = p+1;

    if (e && *e)
        n = atoi(e)+1;
    else
        n = 2;

    c = e ? g_strndup(s, e-s) : g_strdup(s);
    r = g_strdup_printf("%s%i", c, n);
    g_free(c);
    
    return r;
}

gchar *avahi_alternative_service_name(const gchar *s) {
    const gchar *e;
    g_assert(s);

    if ((e = strstr(s, " #"))) {
        const gchar *n, *p;
        e += 2;
    
        while ((n = strstr(e, " #")))
            e = n + 2;

        for (p = e; *p; p++)
            if (!isdigit(*p)) {
                e = NULL;
                break;
            }
    }
    
    if (e) {
        gchar *r, *c = g_strndup(s, e-s);
        r = g_strdup_printf("%s%i", c, atoi(e)+1);
        g_free(c);
        return r;
    } else
        return g_strdup_printf("%s #2", s);
}
