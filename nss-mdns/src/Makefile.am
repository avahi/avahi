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

AM_CFLAGS = \
	-DMDNS_ALLOW_FILE=\"$(sysconfdir)/mdns.allow\" \
	-DRESOLV_CONF_FILE=\"$(sysconfdir)/resolv.conf\" \
	-DAVAHI_SOCKET=\"$(localstatedir)/run/avahi-daemon/socket\"

#AM_CFLAGS += -DNDEBUG=1 -Os

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")'

lib_LTLIBRARIES= \
	libnss_mdns.la \
	libnss_mdns4.la \
	libnss_mdns6.la \
	libnss_mdns_minimal.la \
	libnss_mdns4_minimal.la \
	libnss_mdns6_minimal.la

noinst_PROGRAMS= \
	nss-test

libnss_mdns_la_SOURCES= \
	nss.c \
	util.c util.h

libnss_mdns_la_CFLAGS=$(AM_CFLAGS) 
libnss_mdns_la_LDFLAGS=-avoid-version -module -export-dynamic -shrext .so.2

if ENABLE_AVAHI
libnss_mdns_la_SOURCES += \
	avahi.c	avahi.h

noinst_PROGRAMS += avahi-test
endif

if ENABLE_LEGACY
libnss_mdns_la_SOURCES += \
	query.c query.h \
	dns.c dns.h

noinst_PROGRAMS += mdns-test
endif

libnss_mdns_minimal_la_SOURCES=$(libnss_mdns_la_SOURCES)
libnss_mdns_minimal_la_CFLAGS=$(libnss_mdns_la_CFLAGS) -DMDNS_MINIMAL
libnss_mdns_minimal_la_LDFLAGS=$(libnss_mdns_la_LDFLAGS)

libnss_mdns4_la_SOURCES=$(libnss_mdns_la_SOURCES)
libnss_mdns4_la_CFLAGS=$(libnss_mdns_la_CFLAGS) -DNSS_IPV4_ONLY=1
libnss_mdns4_la_LDFLAGS=$(libnss_mdns_la_LDFLAGS)

libnss_mdns4_minimal_la_SOURCES=$(libnss_mdns_la_SOURCES)
libnss_mdns4_minimal_la_CFLAGS=$(libnss_mdns_la_CFLAGS) -DNSS_IPV4_ONLY=1 -DMDNS_MINIMAL
libnss_mdns4_minimal_la_LDFLAGS=$(libnss_mdns_la_LDFLAGS)

libnss_mdns6_la_SOURCES=$(libnss_mdns_la_SOURCES)
libnss_mdns6_la_CFLAGS=$(libnss_mdns_la_CFLAGS) -DNSS_IPV6_ONLY=1
libnss_mdns6_la_LDFLAGS=$(libnss_mdns_la_LDFLAGS)

libnss_mdns6_minimal_la_SOURCES=$(libnss_mdns_la_SOURCES)
libnss_mdns6_minimal_la_CFLAGS=$(libnss_mdns_la_CFLAGS) -DNSS_IPV6_ONLY=1 -DMDNS_MINIMAL
libnss_mdns6_minimal_la_LDFLAGS=$(libnss_mdns_la_LDFLAGS)

avahi_test_SOURCES = \
	avahi.c avahi.h \
	util.c util.h \
	avahi-test.c

mdns_test_SOURCES = \
	query.c query.h \
	dns.c dns.h \
	util.c util.h \
	mdns-test.c

nss_test_SOURCES = \
	nss-test.c

install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/libnss_mdns.la 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns_minimal.la 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns4.la 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns4_minimal.la 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns6.la 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns6_minimal.la 

uninstall-hook:
	rm -f $(DESTDIR)$(libdir)/libnss_mdns.so.2 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns_minimal.so.2 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns4.so.2 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns4_minimal.so.2 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns6.so.2 
	rm -f $(DESTDIR)$(libdir)/libnss_mdns6_minimal.so.2 
