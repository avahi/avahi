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

pkgsysconfdir=$(sysconfdir)/avahi
servicedir=$(pkgsysconfdir)/services

man_MANS = \
	avahi-daemon.8

if HAVE_PYTHON

man_MANS += \
	avahi-browse.1 \
	avahi-publish-service.1 \
	avahi-publish-address.1 \
	avahi-resolve-address.1 \
	avahi-resolve-host-name.1

endif

noinst_DATA = \
	avahi-browse.1.xml \
	avahi-publish-service.1.xml \
	avahi-publish-address.1.xml \
	avahi-resolve-address.1.xml \
	avahi-resolve-host-name.1.xml \
	avahi-daemon.8.xml

EXTRA_DIST = \
	$(man_MANS) \
	avahi-browse.1.xml.in \
	avahi-publish-service.1.xml.in \
	avahi-publish-address.1.xml.in \
	avahi-resolve-address.1.xml.in \
	avahi-resolve-host-name.1.xml.in \
	avahi-daemon.8.xml.in \
	xmltoman.css \
	xmltoman.xsl \
	xmltoman.dtd

CLEANFILES = \
	$(noinst_DATA)

avahi-browse.1.xml: avahi-browse.1.xml.in Makefile
	sed -e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-publish-service.1.xml: avahi-publish-service.1.xml.in Makefile
	sed	-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-publish-address.1.xml: avahi-publish-address.1.xml.in Makefile
	sed	-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-resolve-address.1.xml: avahi-resolve-address.1.xml.in Makefile
	sed	-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-resolve-host-name.1.xml: avahi-resolve-host-name.1.xml.in Makefile
	sed	-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-daemon.8.xml: avahi-daemon.8.xml.in Makefile
	sed -e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@servicedir\@,$(servicedir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

if USE_XMLTOMAN

CLEANFILES += $(man_MANS)

avahi-browse.1: avahi-browse.1.xml Makefile
	xmltoman $< > $@

avahi-publish-service.1: avahi-publish-service.1.xml Makefile
	xmltoman $< > $@

avahi-publish-address.1: avahi-publish-address.1.xml Makefile
	xmltoman $< > $@

avahi-resolve-address.1: avahi-resolve-address.1.xml Makefile
	xmltoman $< > $@

avahi-resolve-host-name.1: avahi-resolve-host-name.1.xml Makefile
	xmltoman $< > $@

avahi-daemon.8: avahi-daemon.8.xml Makefile
	xmltoman $< > $@

endif
