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

if BUILD_MANPAGES

man_MANS = \
	avahi-daemon.8 \
	avahi-dnsconfd.8 \
	avahi-daemon.conf.5 \
	avahi-dnsconfd.action.8 \
	avahi.service.5 \
	avahi.hosts.5

noinst_DATA = \
	avahi-browse.1.xml \
	avahi-publish.1.xml \
	avahi-resolve.1.xml \
	avahi-set-host-name.1.xml \
	avahi-daemon.8.xml \
	avahi-discover.1.xml \
	avahi-bookmarks.1.xml \
	avahi-dnsconfd.8.xml \
	avahi-daemon.conf.5.xml \
	avahi-dnsconfd.action.8.xml \
	avahi.service.5.xml \
	avahi.hosts.5.xml \
	avahi-autoipd.8.xml \
	avahi-autoipd.action.8.xml \
	bssh.1.xml

CLEANFILES = \
	$(noinst_DATA)

if HAVE_DBUS

man_MANS += \
	avahi-browse.1 \
	avahi-resolve.1 \
	avahi-publish.1 \
	avahi-set-host-name.1

if HAVE_GTK
man_MANS += \
	bssh.1
endif

if HAVE_PYTHON
man_MANS += \
	avahi-bookmarks.1
if HAVE_GTK
man_MANS += \
	avahi-discover.1
endif
endif
endif

if ENABLE_AUTOIPD
if HAVE_LIBDAEMON

man_MANS += \
	avahi-autoipd.8 \
	avahi-autoipd.action.8

endif
endif

avahi-browse.1.xml: avahi-browse.1.xml.in Makefile
	sed -e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-publish.1.xml: avahi-publish.1.xml.in Makefile
	sed	-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-resolve.1.xml: avahi-resolve.1.xml.in Makefile
	sed	-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-set-host-name.1.xml: avahi-set-host-name.1.xml.in Makefile
	sed	-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-daemon.8.xml: avahi-daemon.8.xml.in Makefile
	sed -e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@servicedir\@,$(servicedir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-discover.1.xml: avahi-discover.1.xml.in Makefile
	sed	-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-bookmarks.1.xml: avahi-bookmarks.1.xml.in Makefile
	sed	-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-dnsconfd.8.xml: avahi-dnsconfd.8.xml.in Makefile
	sed	-e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-daemon.conf.5.xml: avahi-daemon.conf.5.xml.in Makefile
	sed	-e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-dnsconfd.action.8.xml: avahi-dnsconfd.action.8.xml.in Makefile
	sed	-e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi.service.5.xml: avahi.service.5.xml.in Makefile
	sed	-e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@servicedir\@,$(servicedir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi.hosts.5.xml: avahi.hosts.5.xml.in Makefile
	sed	-e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-autoipd.action.8.xml: avahi-autoipd.action.8.xml.in Makefile
	sed	-e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

avahi-autoipd.8.xml: avahi-autoipd.8.xml.in Makefile
	sed	-e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

bssh.1.xml: bssh.1.xml.in Makefile
	sed	-e 's,@pkgsysconfdir\@,$(pkgsysconfdir),g' \
		-e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
        -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' $< > $@

if USE_XMLTOMAN

CLEANFILES += $(man_MANS)

avahi-browse.1: avahi-browse.1.xml Makefile
	xmltoman $< > $@

avahi-publish.1: avahi-publish.1.xml Makefile
	xmltoman $< > $@

avahi-resolve.1: avahi-resolve.1.xml Makefile
	xmltoman $< > $@

avahi-set-host-name.1: avahi-set-host-name.1.xml Makefile
	xmltoman $< > $@

avahi-daemon.8: avahi-daemon.8.xml Makefile
	xmltoman $< > $@

avahi-discover.1: avahi-discover.1.xml Makefile
	xmltoman $< > $@

avahi-bookmarks.1: avahi-bookmarks.1.xml Makefile
	xmltoman $< > $@

avahi-dnsconfd.8: avahi-dnsconfd.8.xml Makefile
	xmltoman $< > $@

avahi-daemon.conf.5: avahi-daemon.conf.5.xml Makefile
	xmltoman $< > $@

avahi-dnsconfd.action.8: avahi-dnsconfd.action.8.xml Makefile
	xmltoman $< > $@

avahi.service.5: avahi.service.5.xml Makefile
	xmltoman $< > $@

avahi.hosts.5: avahi.hosts.5.xml Makefile
	xmltoman $< > $@

avahi-autoipd.action.8: avahi-autoipd.action.8.xml Makefile
	xmltoman $< > $@

avahi-autoipd.8: avahi-autoipd.8.xml Makefile
	xmltoman $< > $@

bssh.1: bssh.1.xml Makefile
	xmltoman $< > $@

xmllint: $(noinst_DATA)
	for f in $(noinst_DATA) ; do \
			xmllint --noout --valid "$$f" || exit 1 ; \
	done

endif

endif

EXTRA_DIST = \
	$(man_MANS) \
	avahi-browse.1.xml.in \
	avahi-publish.1.xml.in \
	avahi-resolve.1.xml.in \
	avahi-set-host-name.1.xml.in \
	avahi-daemon.8.xml.in \
	avahi-discover.1.xml.in \
	avahi-bookmarks.1.xml.in \
	avahi-dnsconfd.8.xml.in \
	avahi-daemon.conf.5.xml.in \
	avahi-dnsconfd.action.8.xml.in \
	avahi.service.5.xml.in \
	avahi.hosts.5.xml.in \
	avahi-autoipd.action.8.xml.in \
	avahi-autoipd.8.xml.in \
	bssh.1.xml.in \
	xmltoman.css \
	xmltoman.xsl \
	xmltoman.dtd


if HAVE_DBUS

BSSH_LN =
if HAVE_GTK
if HAVE_GLIB
BSSH_LN += $(LN_S) bssh.1 bvnc.1 &&
endif
endif
install-exec-local:
	mkdir -p $(DESTDIR)/$(mandir)/man1 && \
		cd $(DESTDIR)/$(mandir)/man1 && \
		rm -f avahi-resolve-host-name.1 avahi-resolve-address.1 avahi-browse-domains.1 avahi-publish-address.1 avahi-publish-service.1 bvnc.1 && \
		$(BSSH_LN) \
		$(LN_S) avahi-resolve.1 avahi-resolve-host-name.1 && \
		$(LN_S) avahi-resolve.1 avahi-resolve-address.1 && \
		$(LN_S) avahi-browse.1 avahi-browse-domains.1 && \
		$(LN_S) avahi-publish.1 avahi-publish-address.1 && \
		$(LN_S) avahi-publish.1 avahi-publish-service.1


endif
