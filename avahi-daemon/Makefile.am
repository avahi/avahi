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

pkgsysconfdir=$(sysconfdir)/avahi
servicedir=$(pkgsysconfdir)/services

AM_CFLAGS= \
	-I$(top_srcdir) \
	-D_GNU_SOURCE \
	-DAVAHI_SERVICE_DIRECTORY=\"$(servicedir)\" \
	-DAVAHI_CONFIG_FILE=\"$(pkgsysconfdir)/avahi.conf\"
AM_LDADD=-lexpat

# GLIB 2.0
AM_CFLAGS+=$(GLIB20_CFLAGS)
AM_LDADD+=$(GLIB20_LIBS)

# libdaemon
AM_CFLAGS+=$(LIBDAEMON_CFLAGS)
AM_LDADD+=$(LIBDAEMON_LIBS)


if ENABLE_DBUS
# DBUS
AM_CFLAGS+=$(DBUS_CFLAGS)
AM_LDADD+=$(DBUS_LIBS)
endif

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")'

bin_PROGRAMS = \
	avahi

avahi_SOURCES = \
	main.c main.h \
	simple-protocol.c simple-protocol.h \
	static-services.c static-services.h

pkgsysconf_DATA = \
	avahi.conf

service_DATA = \
	example.service

pkgdata_DATA = \
	avahi-service.dtd

if ENABLE_DBUS
avahi_SOURCES += dbus-protocol.c dbus-protocol.h
endif

avahi_CFLAGS = $(AM_CFLAGS)
avahi_LDADD = $(AM_LDADD) ../avahi-core/libavahi-core.la ../avahi-common/libavahi-common.la

xmllint:
	xmllint --noout --valid example.service
