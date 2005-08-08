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
	bootstrap.sh \
	LICENSE \
	$(DX_CONFIG) \
	docs/INSTALL \
	docs/TODO \
	docs/README \
	docs/DBUS-API \
	docs/AUTHORS \
	docs/HACKING \
	docs/overview.dia

SUBDIRS = \
	common \
	avahi-common \
	avahi-core \
	avahi-discover-standalone \
	avahi-client \
	avahi-daemon \
	initscript \
	avahi-dnsconfd \
	avahi-utils \
	examples \
	man

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = avahi-core.pc

MOSTLYCLEANFILES = $(DX_CLEANFILES)

homepage:
	scp avahi-daemon/*.introspect avahi-daemon/introspect.dtd avahi-daemon/introspect.xsl\
	    man/*.xml man/xmltoman.dtd man/xmltoman.xsl \
	    fdo:public_html/
	    
