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
	-DAVAHI_DAEMON_RUNTIME_DIR=\"$(avahi_runtime_dir)/avahi-daemon/\" \
	-DAVAHI_SOCKET=\"$(avahi_socket)\" \
	-DAVAHI_SERVICE_DIRECTORY=\"$(servicedir)\" \
	-DAVAHI_CONFIG_FILE=\"$(pkgsysconfdir)/avahi-daemon.conf\"

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

sbin_PROGRAMS = \
	avahi-daemon

avahi_daemon_SOURCES = \
	main.c main.h \
	simple-protocol.c simple-protocol.h \
	static-services.c static-services.h

pkgsysconf_DATA = \
	avahi-daemon.conf

service_DATA = \
	example.service

pkgdata_DATA = \
	avahi-service.dtd

EXTRA_DIST = avahi-service.dtd avahi-daemon.conf example.service dbus-test.py Server.introspect server-introspect-xml.c
BUILT_SOURCES = server-introspect-xml.c

if ENABLE_DBUS
avahi_daemon_SOURCES += dbus-protocol.c dbus-protocol.h

dbusservicedir = $(DBUS_SYS_DIR)
dbusservice_DATA = avahi-dbus.conf

EXTRA_DIST += $(dbusservice_DATA)
endif

avahi_daemon_CFLAGS = $(AM_CFLAGS)
avahi_daemon_LDADD = $(AM_LDADD) ../avahi-core/libavahi-core.la ../avahi-common/libavahi-common.la

xmllint:
	xmllint --noout --valid example.service
	xmllint --noout --valid Server.introspect
	xmllint --noout --valid EntryGroup.introspect

server-introspect-xml.c: Server.introspect
	python embedd-file.py -s $< server_introspect_xml > $@
