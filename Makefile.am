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

ACLOCAL_AMFLAGS = -I common

include $(srcdir)/common/doxygen.mk

EXTRA_DIST = \
	autogen.sh \
	bootstrap.sh \
	LICENSE \
	$(DX_CONFIG) \
	docs/INSTALL \
	docs/TODO \
	docs/NEWS \
	docs/README \
	docs/DBUS-API \
	docs/AUTHORS \
	docs/HACKING \
	docs/API-CHANGES-0.6 \
	docs/COMPAT-LAYERS \
	docs/MALLOC \
	docs/overview.dia \
	docs/server-states.dia \
	docs/avahi-poll.dia \
	avahi-core.pc.in \
	avahi-client.pc.in \
	avahi-glib.pc.in \
	avahi-qt3.pc.in \
	avahi-qt4.pc.in \
	avahi-sharp.pc.in \
	avahi-compat-libdns_sd.pc.in \
	avahi-compat-howl.pc.in

SUBDIRS = \
	common \
	avahi-common \
	avahi-glib \
	avahi-core \
	avahi-discover-standalone \
	avahi-qt \
	avahi-client \
	avahi-daemon \
	avahi-sharp \
	initscript \
	avahi-dnsconfd \
	avahi-utils \
	avahi-python \
	examples \
	man \
	tests \
	service-type-database \
	avahi-compat-libdns_sd \
	avahi-compat-howl 


DX_INPUT = \
	$(srcdir)/avahi-core/core.h \
	$(srcdir)/avahi-core/lookup.h \
	$(srcdir)/avahi-core/publish.h \
	$(srcdir)/avahi-common/address.h \
	$(srcdir)/avahi-core/rr.h \
	$(srcdir)/avahi-common/strlst.h \
	$(srcdir)/avahi-common/alternative.h \
	$(srcdir)/avahi-core/log.h \
	$(srcdir)/avahi-common/defs.h \
	$(srcdir)/avahi-client/client.h \
	$(srcdir)/avahi-client/lookup.h \
	$(srcdir)/avahi-client/publish.h \
	$(srcdir)/avahi-common/error.h \
	$(srcdir)/avahi-common/malloc.h \
	$(srcdir)/avahi-common/domain.h \
	$(srcdir)/avahi-common/watch.h \
	$(srcdir)/avahi-common/simple-watch.h \
	$(srcdir)/avahi-common/thread-watch.h \
	$(srcdir)/avahi-glib/glib-watch.h \
	$(srcdir)/avahi-glib/glib-malloc.h \
	$(srcdir)/avahi-common/timeval.h \
	$(srcdir)/avahi-qt/qt-watch.h

DX_EXAMPLE_PATH = $(srcdir)/examples

DX_EXAMPLE_PATTERNS = *.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = avahi-core.pc

avahi-core.pc: avahi-core.pc.in
	sed -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@libdir\@,$(libdir),g' \
	    -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' $< > $@

CLEANFILES = avahi-core.pc

if HAVE_DBUS

pkgconfig_DATA += avahi-client.pc

avahi-client.pc: avahi-client.pc.in
	sed -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@libdir\@,$(libdir),g' \
	    -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' $< > $@

CLEANFILES += avahi-client.pc

if ENABLE_COMPAT_HOWL

pkgconfig_DATA += avahi-compat-howl.pc

avahi-compat-howl.pc: avahi-compat-howl.pc.in
	sed -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@libdir\@,$(libdir),g' \
	    -e 's,@HOWL_COMPAT_VERSION\@,$(HOWL_COMPAT_VERSION),g' $< > $@

CLEANFILES += avahi-compat-howl.pc

endif

if ENABLE_COMPAT_LIBDNS_SD

pkgconfig_DATA += avahi-compat-libdns_sd.pc

avahi-compat-libdns_sd.pc: avahi-compat-libdns_sd.pc.in
	sed -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@libdir\@,$(libdir),g' \
	    -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' $< > $@

CLEANFILES += avahi-compat-libdns_sd.pc

endif

if HAVE_MONO

pkgconfig_DATA += avahi-sharp.pc

avahi-sharp.pc: avahi-sharp.pc.in
	sed -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' \
	    -e 's,@libdir\@,$(libdir),g' $< > $@

CLEANFILES += avahi-sharp.pc

endif

endif

if HAVE_GLIB

pkgconfig_DATA += avahi-glib.pc

avahi-glib.pc: avahi-glib.pc.in
	sed -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@libdir\@,$(libdir),g' \
	    -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' $< > $@

CLEANFILES += avahi-glib.pc

endif

if HAVE_QT3

pkgconfig_DATA += avahi-qt3.pc

avahi-qt3.pc: avahi-qt3.pc.in
	sed -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@libdir\@,$(libdir),g' \
	    -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' $< > $@

CLEANFILES += avahi-qt3.pc

endif

if HAVE_QT4

pkgconfig_DATA += avahi-qt4.pc

avahi-qt4.pc: avahi-qt4.pc.in
	sed -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@libdir\@,$(libdir),g' \
	    -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' $< > $@

CLEANFILES += avahi-qt4.pc

endif

MOSTLYCLEANFILES = $(DX_CLEANFILES)

DISTCHECK_CONFIGURE_FLAGS = \
	--disable-monodoc

homepage:
	$(MAKE) -C man
	scp avahi-daemon/*.introspect avahi-daemon/introspect.dtd avahi-daemon/introspect.xsl\
	    man/*.xml man/xmltoman.dtd man/xmltoman.xsl \
	    tango:www/avahi.org/tree/download/
	rm -rf doxygen
	$(MAKE) doxygen-run
	ssh tango rm -rf www/avahi.org/tree/download/doxygen
	scp -r doxygen/html tango:www/avahi.org/tree/download/doxygen
