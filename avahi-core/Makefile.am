# $Id$
#
# This file is part of avahi.
# 
# avahi is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# avahi is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with avahi; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

AM_CFLAGS=-I$(top_srcdir) -D_GNU_SOURCE

# GLIB 2.0
AM_CFLAGS+=$(GLIB20_CFLAGS)
AM_LDADD=$(GLIB20_LIBS)

# Import stuff from avahi-common
AM_CFLAGS+=-I$(top_srcdir)/avahi-common 
COMMON_LDADD=../avahi-common/libavahi-common.la
STATIC_COMMON_LDADD=../avahi-common/libavahi-common-static.a

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")'

avahiincludedir=$(includedir)/avahi-core 

avahiinclude_HEADERS =
	core.h

lib_LTLIBRARIES = \
	libavahi-core.la 

noinst_PROGRAMS = \
	prioq-test \
	avahi-test \
	conformance-test \
	avahi-reflector \
	dns-test

libavahi_core_la_SOURCES = \
	timeeventq.c timeeventq.h\
	iface.c iface.h \
	netlink.c netlink.h \
	server.c server.h \
	prioq.c prioq.h \
	cache.c cache.h\
	socket.c socket.h \
	response-sched.c response-sched.h \
	query-sched.c query-sched.h \
	probe-sched.c probe-sched.h \
	announce.c announce.h \
	browse.c browse.h \
	rrlist.c rrlist.h \
	resolve-host-name.c \
	resolve-address.c \
	browse-domain.c \
	browse-service-type.c \
	browse-service.c \
	resolve-service.c \
	dns.c dns.h \
	core.h \
	llist.h \
	log.c log.h
libavahi_core_la_CFLAGS = $(AM_CFLAGS)
libavahi_core_la_LIBADD = $(AM_LDADD) $(COMMON_LDADD)

prioq_test_SOURCES = \
	prioq-test.c  \
	prioq.c prioq.h
prioq_test_CFLAGS = $(AM_CFLAGS)
prioq_test_LDADD = $(AM_LDADD) $(STATIC_COMMON_LDADD)

avahi_test_SOURCES = \
	avahi-test.c \
	$(libavahi_core_la_SOURCES)
avahi_test_CFLAGS = $(AM_CFLAGS)
avahi_test_LDADD = $(AM_LDADD) $(STATIC_COMMON_LDADD)

conformance_test_SOURCES = \
	conformance-test.c \
	$(libavahi_core_la_SOURCES)
conformance_test_CFLAGS = $(AM_CFLAGS)
conformance_test_LDADD = $(AM_LDADD) $(STATIC_COMMON_LDADD)

avahi_reflector_SOURCES = \
	avahi-reflector.c \
	$(libavahi_core_la_SOURCES)
avahi_reflector_CFLAGS = $(AM_CFLAGS)
avahi_reflector_LDADD = $(AM_LDADD) $(STATIC_COMMON_LDADD)

dns_test_SOURCES = \
	dns.c dns.h \
	dns-test.c \
	log.c log.h
dns_test_CFLAGS = $(AM_CFLAGS)
dns_test_LDADD = $(AM_LDADD) $(STATIC_COMMON_LDADD)

valgrind: avahi-test
	libtool --mode=execute valgrind ./avahi-test

gdb: avahi-test
	libtool --mode=execute gdb ./avahi-test
