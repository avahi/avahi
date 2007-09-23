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

EXTRA_DIST = \
	__init__.py \
	SimpleGladeApp.py \
	avahi-discover.in \
	avahi-discover.desktop.in

if HAVE_PYTHON
if HAVE_PYTHON_DBUS
if HAVE_PYGTK

pythonscripts =

desktopdir = $(datadir)/applications
desktop_DATA = 

avahi_discoverdir = $(pythondir)/avahi_discover
avahi_discover_PYTHON =

if HAVE_GDBM
pythonscripts += \
	avahi-discover
desktop_DATA += avahi-discover.desktop
avahi_discover_PYTHON += __init__.py SimpleGladeApp.py
endif

if HAVE_DBM
pythonscripts += \
	avahi-discover
desktop_DATA += avahi-discover.desktop
avahi_discover_PYTHON += __init__.py SimpleGladeApp.py
endif

avahi-discover.desktop: avahi-discover.desktop.in
	sed -e 's,@bindir\@,$(bindir),g' $< > $@

avahi-discover: avahi-discover.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' \
		-e 's,@interfacesdir\@,$(interfacesdir),g' $< > $@
	chmod +x $@

bin_SCRIPTS = $(pythonscripts)
CLEANFILES = $(pythonscripts) $(desktop_DATA) *.pyc *.pyo

endif
endif
endif
