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

if HAVE_DBUS

avahi_compat_howldir=$(includedir)/avahi-compat-howl

avahi_compat_howl_HEADERS = \
	include/howl.h \
	include/rendezvous/rendezvous.h \
	include/rendezvous/text_record.h \
	include/corby/message.h \
	include/corby/object.h \
	include/corby/orb.h \
	include/corby/corby.h \
	include/corby/channel.h \
	include/corby/buffer.h \
	include/discovery/discovery.h \
	include/discovery/text_record.h \
	include/salt/socket.h \
	include/salt/address.h \
	include/salt/platform.h \
	include/salt/signal.h \
	include/salt/interface.h \
	include/salt/salt.h \
	include/salt/time.h \
	include/salt/debug.h

lib_LTLIBRARIES = libavahi-compat-howl.la 

noinst_PROGRAMS = address-test

libavahi_compat_howl_la_SOURCES = \
	$(avahi_compat_howl_HEADERS) \
	warn.c warn.h \
	unsupported.c \
	address.c \
	text.c
libavahi_compat_howl_la_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I $(srcdir)/include
libavahi_compat_howl_la_LDFLAGS = $(AM_LDFLAGS) -export-dynamic -version-info $(LIBAVAHI_COMPAT_HOWL_VERSION_INFO) $(PTHREAD_LIBS) ../avahi-common/libavahi-common.la ../avahi-client/libavahi-client.la

address_test_SOURCES = \
	$(avahi_compat_howl_HEADERS) \
	address.c \
	address-test.c \
	warn.c warn.h 
address_test_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I $(srcdir)/include
address_test_LDADD = $(AM_LDADD) ../avahi-common/libavahi-common.la

# You can test the compatibility layer by sticking in mDNSResponder's
# dns-sd.c source here, naming it "libdns_sd-test.c" and running "make
# libdns_sd-test" manually. We do not distribute that file due to
# licensing restrictions.

#libdns_sd-test: libdns_sd-test.c libavahi-compat-libdns_sd.la
#	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) $(PTHREAD_CFLAGS) -o libdns_sd-test.o -c libdns_sd-test.c 
#	$(LIBTOOL) --mode=link $(CC) $(CFLAGS) $(PTHREAD_CFLAGS) -o libdns_sd-test libdns_sd-test.o $(PTHREAD_LIBS) ../avahi-common/libavahi-common.la libavahi-compat-libdns_sd.la 

#CLEANFILES = libdns_sd-test.o libdns_sd-test

endif
