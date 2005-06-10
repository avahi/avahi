# $Id$
#
# This file is part of nss-mdns.
#
# nss-mdns is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# nss-mdns is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with nss-mdns; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

AM_CFLAGS=-D_GNU_SOURCE -DMDNS_ALLOW_FILE=\"$(sysconfdir)/mdns.allow\"
#-DNDEBUG=1

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")'

noinst_PROGRAMS = nss-test mdns-test

lib_LTLIBRARIES = libnss_mdns.la libnss_mdns4.la libnss_mdns6.la

mdns_test_SOURCES = query.c dns.c util.c mdns-test.c \
		    query.h dns.h util.h

nss_test_SOURCES = nss-test.c

libnss_mdns_la_SOURCES = query.c dns.c util.c nss.c \
			 query.h dns.h util.h

libnss_mdns_la_CFLAGS=$(AM_CFLAGS) 
libnss_mdns_la_LDFLAGS=-avoid-version -module -export-dynamic -shrext .so.2

libnss_mdns4_la_SOURCES=$(libnss_mdns_la_SOURCES)
libnss_mdns4_la_CFLAGS=$(libnss_mdns_la_CFLAGS) -DNSS_IPV4_ONLY=1
libnss_mdns4_la_LDFLAGS=$(libnss_mdns_la_LDFLAGS)

libnss_mdns6_la_SOURCES=$(libnss_mdns_la_SOURCES)
libnss_mdns6_la_CFLAGS=$(libnss_mdns_la_CFLAGS) -DNSS_IPV6_ONLY=1
libnss_mdns6_la_LDFLAGS=$(libnss_mdns_la_LDFLAGS)

install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/libnss_mdns.la 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns4.la 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns6.la 

