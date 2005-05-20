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

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")'

lib_LTLIBRARIES = \
	libavahi-core.la 

noinst_PROGRAMS = \
	dns-test \
	domain-test \
	prioq-test \
	strlst-test \
	avahi-test \
	alternative-test \
	conformance-test

libavahi_core_la_SOURCES = \
	timeeventq.c timeeventq.h\
	iface.c iface.h \
	netlink.c netlink.h \
	server.c server.h \
	address.c address.h \
	util.c util.h \
	prioq.c prioq.h \
	cache.c cache.h\
	rr.c rr.h \
	dns.c dns.h \
	socket.c socket.h \
	response-sched.c response-sched.h \
	query-sched.c query-sched.h \
	probe-sched.c probe-sched.h \
	announce.c announce.h \
	resolve.c resolve.h \
	strlst.c strlst.h \
	rrlist.c rrlist.h \
	alternative.c alternative.h \
	resolve-host-name.c

prioq_test_SOURCES = \
	prioq-test.c  \
	prioq.c prioq.h
prioq_test_CFLAGS = $(AM_CFLAGS)
prioq_test_LDADD = $(AM_LDADD)

strlst_test_SOURCES = \
	strlst-test.c \
	strlst.c strlst.h
strlst_test_CFLAGS = $(AM_CFLAGS)
strlst_test_LDADD = $(AM_LDADD)

domain_test_SOURCES = \
	domain-test.c \
	util.c util.h
domain_test_CFLAGS = $(AM_CFLAGS)
domain_test_LDADD = $(AM_LDADD)

dns_test_SOURCES = \
	dns-test.c \
	util.c util.h \
	dns.c dns.h \
	rr.c rr.h \
	strlst.c strlst.h
dns_test_CFLAGS = $(AM_CFLAGS)
dns_test_LDADD = $(AM_LDADD)

avahi_test_SOURCES = \
	avahi-test.c \
	$(libavahi_core_la_SOURCES)
avahi_test_CFLAGS = $(AM_CFLAGS)
avahi_test_LDADD = $(AM_LDADD)

alternative_test_SOURCES = \
	alternative-test.c \
	alternative.c alternative.h
alternative_test_CFLAGS = $(AM_CFLAGS)
alternative_test_LDADD = $(AM_LDADD)

conformance_test_SOURCES = \
	conformance-test.c \
	$(libavahi_core_la_SOURCES)
conformance_test_CFLAGS = $(AM_CFLAGS)
conformance_test_LDADD = $(AM_LDADD)

valgrind: avahi-test
	libtool --mode=execute valgrind ./avahi-test

gdb: avahi-test
	libtool --mode=execute gdb ./avahi-test
