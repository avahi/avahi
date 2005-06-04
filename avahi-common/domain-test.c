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

#include "util.h"

int main(int argc, char *argv[]) {
    gchar *s;
    
    g_message("host name: %s", s = avahi_get_host_name());
    g_free(s);

    g_message("%s", s = avahi_normalize_name("foo.foo."));
    g_free(s);
    
    g_message("%s", s = avahi_normalize_name("\\f\\o\\\\o\\..\\f\\ \\o\\o."));
    g_free(s);

    g_message("%i", avahi_domain_equal("\\aaa bbb\\.cccc\\\\.dee.fff.", "aaa\\ bbb\\.cccc\\\\.dee.fff"));
    g_message("%i", avahi_domain_equal("\\A", "a"));

    g_message("%i", avahi_domain_equal("a", "aaa"));

    g_message("%u = %u", avahi_domain_hash("\\Aaaab\\\\."), avahi_domain_hash("aaaa\\b\\\\"));
    
    return 0;
}
