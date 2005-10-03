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

#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "warn.h"

static pthread_mutex_t linkage_mutex = PTHREAD_MUTEX_INITIALIZER;
static int linkage_warning = 0;

static void get_exe_name(char *t, size_t l) {
    int k;
    char fn[1024];

    /* Yes, I know, this is not portable. But who cares? It's
     * for cosmetics only, anyway. */
    
    snprintf(fn, sizeof(fn), "/proc/%lu/exe", (unsigned long) getpid());

    if ((k = readlink(fn, t, l-1)) < 0)
        snprintf(t, l, "(unknown)");
    else {
        char *slash;

        assert((size_t) k <= l-1);
        t[k] = 0;

        if ((slash = strrchr(t, '/')))
            memmove(t, slash+1, strlen(slash)+1);
    }
}

void avahi_warn_linkage(void) {
    int w;
    
    pthread_mutex_lock(&linkage_mutex);
    w = linkage_warning;
    linkage_warning = 1;
    pthread_mutex_unlock(&linkage_mutex);

    if (!w && !getenv("AVAHI_BONJOUR_NOWARN")) {
        char exename[256];
        get_exe_name(exename, sizeof(exename));
        
        fprintf(stderr, "*** WARNING: The application '%s' uses the Bonjour compatiblity layer of Avahi. Please fix it to use the native API! ***\n", exename);
    }
}

void avahi_warn_unsupported(const char *function) {
    char exename[256];
    get_exe_name(exename, sizeof(exename));

    fprintf(stderr, "*** WARNING: The application '%s' called '%s()' which is not supported in the Bonjour compatiblity layer of Avahi. Please fix it to use the native API! ***\n", exename, function);
}



