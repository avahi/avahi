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

SUBDIRS=avahi

pythonscripts = \
	avahi-publish-address \
	avahi-publish-service \
	avahi-dump-all \
	avahi-discover \
	avahi-bookmarks

EXTRA_DIST = \
	avahi-publish-address.in \
	avahi-publish-service.in \
	avahi-dump-all.in \
	avahi-discover.in \
	avahi-bookmarks.in

if HAVE_PYTHON
bin_SCRIPTS = $(pythonscripts)
endif

avahi-publish-address: avahi-publish-address.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-publish-service: avahi-publish-service.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-dump-all: avahi-dump-all.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

avahi-discover: avahi-discover.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' \
		-e 's,@interfacesdir\@,$(interfacesdir),g' $< > $@
	chmod +x $@

avahi-bookmarks: avahi-bookmarks.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' $< > $@
	chmod +x $@

CLEANFILES = $(pythonscripts)
