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

EXTRA_DIST = \
	avahi-publish-address.in \
	avahi-publish-service.in \
	avahi-bookmarks.in \
	avahi-resolve-host-name.in \
	avahi-resolve-address.in \
	avahi-discover.in \
	avahi-discover.desktop.in

SUBDIRS=avahi

if HAVE_PYTHON
if HAVE_PYTHON_DBUS
if HAVE_PYGTK

pythonscripts = \
	avahi-publish-address \
	avahi-publish-service \
	avahi-bookmarks \
	avahi-resolve-host-name \
	avahi-resolve-address \
	avahi-discover

desktopdir = $(datadir)/applications
desktop_DATA = avahi-discover.desktop

avahi-discover.desktop: avahi-discover.desktop.in
	sed -e 's,@bindir\@,$(bindir),g' $< > $@
	chmod +x $@

avahi-discover: avahi-discover.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' \
		-e 's,@interfacesdir\@,$(interfacesdir),g' $< > $@
	chmod +x $@

avahi-publish-address: avahi-publish-address.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-publish-service: avahi-publish-service.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-bookmarks: avahi-bookmarks.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-resolve-host-name: avahi-resolve-host-name.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-resolve-address: avahi-resolve-address.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

bin_SCRIPTS = $(pythonscripts)
CLEANFILES = $(pythonscripts) $(desktop_DATA)

endif
endif
endif

if HAVE_DBUS

bin_PROGRAMS = avahi-browse

avahi_browse_SOURCES = avahi-browse.c sigint.c sigint.h
avahi_browse_CFLAGS = $(AM_CFLAGS)
avahi_browse_LDADD = $(AM_LDADD) ../avahi-client/libavahi-client.la ../avahi-common/libavahi-common.la

if HAVE_GDBM
avahi_browse_SOURCES += stdb.h stdb.c
avahi_browse_CFLAGS += -DDATABASE_FILE=\"$(pkgdatadir)/service-types.db\"
avahi_browse_LDADD += -lgdbm
endif

endif
