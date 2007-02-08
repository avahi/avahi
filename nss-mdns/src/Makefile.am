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

EXTRA_DIST = map-file

AM_CFLAGS = \
	-DMDNS_ALLOW_FILE=\"$(sysconfdir)/mdns.allow\" \
	-DRESOLV_CONF_FILE=\"$(sysconfdir)/resolv.conf\" \
	-DAVAHI_SOCKET=\"$(localstatedir)/run/avahi-daemon/socket\"

#AM_CFLAGS += -DNDEBUG=1 -Os

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")'

AM_LDFLAGS=-avoid-version -module -export-dynamic

lib_LTLIBRARIES = 

if FREEBSD_NSS
lib_LTLIBRARIES += \
	nss_mdns.la \
	nss_mdns4.la \
	nss_mdns6.la \
	nss_mdns_minimal.la \
	nss_mdns4_minimal.la \
	nss_mdns6_minimal.la
else
lib_LTLIBRARIES += \
	libnss_mdns.la \
	libnss_mdns4.la \
	libnss_mdns6.la \
	libnss_mdns_minimal.la \
	libnss_mdns4_minimal.la \
	libnss_mdns6_minimal.la
endif

noinst_PROGRAMS = nss-test
sources = util.c util.h

if ENABLE_AVAHI
sources += avahi.c avahi.h
noinst_PROGRAMS += avahi-test
endif

if ENABLE_LEGACY
sources += query.c query.h dns.c dns.h
noinst_PROGRAMS += mdns-test
endif

# GNU Libc
libnss_mdns_la_SOURCES= $(sources) nss.c
libnss_mdns_la_CFLAGS=$(AM_CFLAGS) 
libnss_mdns_la_LDFLAGS=$(AM_LDFLAGS) -shrext .so.2 -Wl,-version-script=$(srcdir)/map-file

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

# FreeBSD
nss_mdns_la_SOURCES=$(sources) bsdnss.c
nss_mdns_la_CFLAGS=$(AM_CFLAGS)
nss_mdns_la_LDFLAGS=$(AM_LDFLAGS) -shrext .so.1

nss_mdns_minimal_la_SOURCES=$(nss_mdns_la_SOURCES)
nss_mdns_minimal_la_CFLAGS=$(nss_mdns_la_CFLAGS) -DMDNS_MINIMAL
nss_mdns_minimal_la_LDFLAGS=$(nss_mdns_la_LDFLAGS)

nss_mdns4_la_SOURCES=$(nss_mdns_la_SOURCES)
nss_mdns4_la_CFLAGS=$(nss_mdns_la_CFLAGS) -DNSS_IPV4_ONLY=1
nss_mdns4_la_LDFLAGS=$(nss_mdns_la_LDFLAGS)

nss_mdns4_minimal_la_SOURCES=$(nss_mdns_la_SOURCES)
nss_mdns4_minimal_la_CFLAGS=$(nss_mdns_la_CFLAGS) -DNSS_IPV4_ONLY=1 -DMDNS_MINIMAL
nss_mdns4_minimal_la_LDFLAGS=$(nss_mdns_la_LDFLAGS)

nss_mdns6_la_SOURCES=$(nss_mdns_la_SOURCES)
nss_mdns6_la_CFLAGS=$(nss_mdns_la_CFLAGS) -DNSS_IPV6_ONLY=1
nss_mdns6_la_LDFLAGS=$(nss_mdns_la_LDFLAGS)

nss_mdns6_minimal_la_SOURCES=$(nss_mdns_la_SOURCES)
nss_mdns6_minimal_la_CFLAGS=$(nss_mdns_la_CFLAGS) -DNSS_IPV6_ONLY=1 -DMDNS_MINIMAL
nss_mdns6_minimal_la_LDFLAGS=$(nss_mdns_la_LDFLAGS)

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
