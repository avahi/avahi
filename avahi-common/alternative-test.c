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

#include <stdio.h>

#include "alternative.h"
#include "malloc.h"

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[]) {
    char *r = NULL;
    int i, k;

    for (k = 0; k < 2; k++) {
        
        for (i = 0; i < 20; i++) {
            char *n;
            
            n = i == 0 ? avahi_strdup("gurke") : (k ? avahi_alternative_service_name(r) : avahi_alternative_host_name(r));
            avahi_free(r);
            r = n;
            
            printf("%s\n", r);
        }
    }

    avahi_free(r);
    return 0;
}
