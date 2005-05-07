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

#include "dns.h"
#include "util.h"

int main(int argc, char *argv[]) {
    gchar t[256], *a, *b, *c, *d;
    AvahiDnsPacket *p;

    p = avahi_dns_packet_new(8000);

    avahi_dns_packet_append_name(p, a = "hello.hello.hello.de.");
    avahi_dns_packet_append_name(p, b = "this is a test.hello.de."); 
    avahi_dns_packet_append_name(p, c = "this\\.is\\.a\\.test\\.with\\.dots.hello.de."); 
    avahi_dns_packet_append_name(p, d = "this\\\\is another\\ \\test.hello.de."); 

    avahi_hexdump(AVAHI_DNS_PACKET_DATA(p), p->size);

    avahi_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(avahi_domain_equal(a, t));
    
    avahi_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(avahi_domain_equal(b, t));

    avahi_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(avahi_domain_equal(c, t));

    avahi_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(avahi_domain_equal(d, t));
    
    avahi_dns_packet_free(p);
    return 0;
}
