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
introspectiondir=$(pkgdatadir)/introspection
dbusservicedir=$(DBUS_SYS_DIR)

AM_CFLAGS= \
	-I$(top_srcdir) \
	-DAVAHI_DAEMON_RUNTIME_DIR=\"$(avahi_runtime_dir)/avahi-daemon/\" \
	-DAVAHI_SOCKET=\"$(avahi_socket)\" \
	-DAVAHI_SERVICE_DIR=\"$(servicedir)\" \
	-DAVAHI_CONFIG_FILE=\"$(pkgsysconfdir)/avahi-daemon.conf\" \
	-DAVAHI_DBUS_INTROSPECTION_DIR=\"$(introspectiondir)\" 

AM_LDADD=-lexpat

# libdaemon
AM_CFLAGS+=$(LIBDAEMON_CFLAGS)
AM_LDADD+=$(LIBDAEMON_LIBS)

if ENABLE_DBUS
AM_CFLAGS+=$(DBUS_CFLAGS)
AM_LDADD+=$(DBUS_LIBS)
endif

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")'

sbin_PROGRAMS = \
	avahi-daemon

noinst_PROGRAMS = \
	ini-file-parser-test

avahi_daemon_SOURCES = \
	main.c main.h \
	simple-protocol.c simple-protocol.h \
	static-services.c static-services.h \
	ini-file-parser.c ini-file-parser.h

avahi_daemon_CFLAGS = $(AM_CFLAGS)
avahi_daemon_LDADD = $(AM_LDADD) ../avahi-common/libavahi-common.la ../avahi-core/libavahi-core.la 

ini_file_parser_test_SOURCES = \
	ini-file-parser.c ini-file-parser.h \
	ini-file-parser-test.c

ini_file_parser_test_CFLAGS = $(AM_CFLAGS)
ini_file_parser_test_LDADD = $(AM_LDADD) ../avahi-common/libavahi-common.la ../avahi-core/libavahi-core.la 

pkgsysconf_DATA = \
	avahi-daemon.conf

service_DATA = \
	ssh.service

pkgdata_DATA = \
	avahi-service.dtd

EXTRA_DIST = \
	avahi-service.dtd \
	avahi-daemon.conf \
	example.service \
	dbus-test.py \
	avahi-dbus.conf \
	Server.introspect \
	EntryGroup.introspect \
	DomainBrowser.introspect \
	ServiceTypeBrowser.introspect \
	ServiceBrowser.introspect \
	ssh.service \
	example.service \
	introspect.dtd \
	introspect.xsl

if ENABLE_DBUS

avahi_daemon_SOURCES += \
	dbus-protocol.c \
	dbus-protocol.h

avahi_daemon_LDADD += \
	../avahi-common/libdbus-watch-glue.la

dbusservice_DATA = avahi-dbus.conf

introspection_DATA = \
	Server.introspect \
	EntryGroup.introspect \
	DomainBrowser.introspect \
	ServiceTypeBrowser.introspect \
	ServiceBrowser.introspect

endif



xmllint:
	xmllint --noout --valid example.service
	xmllint --noout --valid Server.introspect
	xmllint --noout --valid EntryGroup.introspect
	xmllint --noout --valid DomainBrowser.introspect
	xmllint --noout --valid ServiceTypeBrowser.introspect
	xmllint --noout --valid ServiceBrowser.introspect
