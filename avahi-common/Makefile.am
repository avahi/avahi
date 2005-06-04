# $Id: Makefile.am 90 2005-05-23 16:15:12Z lennart $
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

avahi_commonincludedir=$(includedir)/avahi-common

avahi_commoninclude_HEADERS =
	strlst.h \
	address.h \
	alternative.h \
	rr.h \
	util.h \
	dns.h

noinst_PROGRAMS = \
	strlst-test \
	dns-test \
	domain-test \
	alternative-test

lib_LTLIBRARIES = \
	libavahi-common.la 

libavahi_common_la_SOURCES = \
	address.c address.h \
	strlst.c strlst.h \
	alternative.c alternative.h \
	rr.c rr.h \
	dns.c dns.h \
	util.c util.h

strlst_test_SOURCES = \
	strlst.c strlst.h \
        strlst-test.c
strlst_test_CFLAGS = $(AM_CFLAGS)
strlst_test_LDADD = $(AM_LDADD)

dns_test_SOURCES = \
	util.c util.h \
	dns.c dns.h \
	rr.c rr.h \
	strlst.c strlst \
	dns-test.c
dns_test_CFLAGS = $(AM_CFLAGS)
dns_test_LDADD = $(AM_LDADD)

alternative_test_SOURCES = \
	alternative.c alternative.h \
        alternative-test.c
alternative_test_CFLAGS = $(AM_CFLAGS)
alternative_test_LDADD = $(AM_LDADD)

domain_test_SOURCES = \
	util.c util.h \
	domain-test.c
domain_test_CFLAGS = $(AM_CFLAGS)
domain_test_LDADD = $(AM_LDADD)
