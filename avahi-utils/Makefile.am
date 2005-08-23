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

pythonscripts = \
	avahi-publish-address \
	avahi-publish-service \
	avahi-browse \
	avahi-discover \
	avahi-bookmarks \
	avahi-resolve-host-name \
	avahi-resolve-address

EXTRA_DIST = \
	avahi-publish-address.in \
	avahi-publish-service.in \
	avahi-browse.in \
	avahi-discover.in \
	avahi-bookmarks.in \
	avahi-resolve-host-name.in \
	avahi-resolve-address.in \
	avahi-discover.desktop.in

if HAVE_PYTHON
if HAVE_DBUS
SUBDIRS=avahi

desktopdir = $(datadir)/applications
desktop_DATA = avahi-discover.desktop

bin_SCRIPTS = $(pythonscripts)

avahi-discover.desktop: avahi-discover.desktop.in
	sed -e 's,@bindir\@,$(bindir),g' $< > $@
	chmod +x $@

avahi-publish-address: avahi-publish-address.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-publish-service: avahi-publish-service.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-browse: avahi-browse.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-discover: avahi-discover.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' \
		-e 's,@interfacesdir\@,$(interfacesdir),g' $< > $@
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

CLEANFILES = $(pythonscripts) $(desktop_DATA)

endif
endif
