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
#include <assert.h>

#include "alternative.h"
#include "malloc.h"

char * avahi_alternative_host_name(const char *s) {
    const char *e;
    char *r;

    assert(s);

    e = strrchr(s, '-');

    if (e) {
        const char *p;
        
        for (p = e+1; *p; p++)
            if (!isdigit(*p)) {
                e = NULL;
                break;
            }

        if (e && (*(e+1) == '0' || (*(e+1) == 0)))
            e = NULL;
    }

    if (e) {
        char *c;

        e++;
        
        if (!(c = avahi_strndup(s, e-s)))
            return NULL;

        r = avahi_strdup_printf("%s%i", c, atoi(e)+1);
        avahi_free(c);
        
    } else
        r = avahi_strdup_printf("%s-2", s);
    
    return r;
}

char *avahi_alternative_service_name(const char *s) {
    const char *e;
    char *r;
    
    assert(s);

    if ((e = strstr(s, " #"))) {
        const char *n, *p;
        e += 2;
    
        while ((n = strstr(e, " #")))
            e = n + 2;

        for (p = e; *p; p++)
            if (!isdigit(*p)) {
                e = NULL;
                break;
            }

        if (e && (*e == '0' || *e == 0))
            e = NULL;
    }
    
    if (e) {
        char *c;

        if (!(c = avahi_strndup(s, e-s)))
            return NULL;
        
        r = avahi_strdup_printf("%s%i", c, atoi(e)+1);
        avahi_free(c);
    } else
        r = avahi_strdup_printf("%s #2", s);

    return r;
}
