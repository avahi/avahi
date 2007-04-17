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
	avahi-bookmarks.in \
	avahi-discover.in \
	avahi-discover.desktop.in

SUBDIRS=avahi

if HAVE_PYTHON
if HAVE_PYTHON_DBUS
if HAVE_PYGTK

pythonscripts = \
	avahi-bookmarks

desktopdir = $(datadir)/applications
desktop_DATA = 

if HAVE_GDBM
pythonscripts += \
	avahi-discover
desktop_DATA += avahi-discover.desktop
endif

if HAVE_DBM
pythonscripts += \
	avahi-discover
desktop_DATA += avahi-discover.desktop
endif

avahi-discover.desktop: avahi-discover.desktop.in
	sed -e 's,@bindir\@,$(bindir),g' $< > $@

avahi-discover: avahi-discover.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' \
		-e 's,@interfacesdir\@,$(interfacesdir),g' $< > $@
	chmod +x $@

avahi-bookmarks: avahi-bookmarks.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

bin_SCRIPTS = $(pythonscripts)
CLEANFILES = $(pythonscripts) $(desktop_DATA)

endif
endif
endif
