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

AM_CFLAGS=-I$(top_srcdir)

# GLIB 2.0
AM_CFLAGS+=$(GLIB20_CFLAGS)
AM_LDADD=$(GLIB20_LIBS)

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")'

avahi_commonincludedir=$(includedir)/avahi-common

avahi_commoninclude_HEADERS = \
	strlst.h \
	address.h \
	alternative.h \
	domain.h \
	cdecl.h \
	defs.h \
	malloc.h

if ENABLE_DBUS
avahi_commoninclude_HEADERS += dbus.h
endif

noinst_HEADERS = llist.h

noinst_PROGRAMS = \
	strlst-test \
	domain-test \
	alternative-test

lib_LTLIBRARIES = \
	libavahi-common.la 

libavahi_common_la_SOURCES = \
	malloc.c malloc.h \
	address.c address.h \
	alternative.c alternative.h \
	error.c error.h \
	strlst.c strlst.h \
	domain.c domain.h
libavahi_common_la_CFLAGS = $(AM_CFLAGS)
libavahi_common_la_LIBADD = $(AM_LDADD)

strlst_test_SOURCES = \
	strlst.c strlst.h \
	malloc.c malloc.h \
	strlst-test.c
strlst_test_CFLAGS = $(AM_CFLAGS)
strlst_test_LDADD = $(AM_LDADD)

alternative_test_SOURCES = \
	alternative.c alternative.h \
	malloc.c malloc.h \
	domain.c domain.h \
	alternative-test.c
alternative_test_CFLAGS = $(AM_CFLAGS)
alternative_test_LDADD = $(AM_LDADD)

domain_test_SOURCES = \
	domain.c domain.h \
	malloc.c malloc.h \
	domain-test.c
domain_test_CFLAGS = $(AM_CFLAGS)
domain_test_LDADD = $(AM_LDADD)

