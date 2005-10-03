# $Id$

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

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")'

avahi_compat_bonjourdir=$(includedir)/avahi-compat-bonjour

avahi_compat_bonjour_HEADERS = dns_sd.h

lib_LTLIBRARIES = libavahi-compat-bonjour.la 

noinst_PROGRAMS = txt-test

libavahi_compat_bonjour_la_SOURCES = \
	dns_sd.h \
	compat.c \
	txt.c \
	unsupported.c \
	warn.c warn.h
libavahi_compat_bonjour_la_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
libavahi_compat_bonjour_la_LDFLAGS = $(AM_LDFLAGS) -export-dynamic -version-info $(LIBAVAHI_COMPAT_BONJOUR_VERSION_INFO) $(PTHREAD_LIBS) ../avahi-common/libavahi-common.la ../avahi-client/libavahi-client.la

txt_test_SOURCES = \
	dns_sd.h \
	txt.c \
	txt-test.c \
	warn.c warn.h
txt_test_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
txt_test_LDADD = $(AM_LDADD) ../avahi-common/libavahi-common.la

# You can test the compatibility layer by sticking in mDNSResponder's
# dns-sd.c source here, naming it "bonjour-test.c" and running "make
# bonjour-test" manually. We do not distribute that file due to
# licensing restrictions.

bonjour-test: bonjour-test.c libavahi-compat-bonjour.la
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) $(PTHREAD_CFLAGS) -o bonjour-test.o -c bonjour-test.c 
	$(LIBTOOL) --mode=link $(CC) $(CFLAGS) $(PTHREAD_CFLAGS) -o bonjour-test bonjour-test.o $(PTHREAD_LIBS) ../avahi-common/libavahi-common.la libavahi-compat-bonjour.la 

CLEANFILES = bonjour-test.o bonjour-test
