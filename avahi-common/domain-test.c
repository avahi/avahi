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

#include "util.h"
#include "malloc.h"

int main(int argc, char *argv[]) {
    char *s;
    
    printf("host name: %s\n", s = avahi_get_host_name());
    avahi_free(s);

    printf("%s\n", s = avahi_normalize_name("foo.foo."));
    avahi_free(s);
    
    printf("%s\n", s = avahi_normalize_name("\\f\\o\\\\o\\..\\f\\ \\o\\o."));
    avahi_free(s);

    printf("%i\n", avahi_domain_equal("\\aaa bbb\\.cccc\\\\.dee.fff.", "aaa\\ bbb\\.cccc\\\\.dee.fff"));
    printf("%i\n", avahi_domain_equal("\\A", "a"));

    printf("%i\n", avahi_domain_equal("a", "aaa"));

    printf("%u = %u\n", avahi_domain_hash("\\Aaaab\\\\."), avahi_domain_hash("aaaa\\b\\\\"));
    
    return 0;
}
