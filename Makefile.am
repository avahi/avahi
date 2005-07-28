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
include aminclude.am

EXTRA_DIST = bootstrap.sh LICENSE $(DX_CONFIG)
SUBDIRS = avahi-common avahi-core avahi-discover avahi-client avahi-daemon initscript avahi-dnsconfd

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = avahi-core.pc

MOSTLYCLEANFILES = $(DX_CLEANFILES)

all: doxygen-doc

.PHONY: distcleancheck
