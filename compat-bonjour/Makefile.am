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

avahi_compat_bonjourdir=$(includedir)/avahi-compat-bonjour

avahi_compat_bonjour_HEADERS = \
	dns_sd.h


lib_LTLIBRARIES = \
	libavahi-compat-bonjour.la 

libavahi_compat_bonjour_la_SOURCES = \
	unsupported.c \
	compat.c \
	dns_sd.h

libavahi_compat_bonjour_la_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
libavahi_compat_bonjour_la_LDFLAGS = $(AM_LDFLAGS) -export-dynamic -version-info $(LIBAVAHI_COMPAT_BONJOUR_VERSION_INFO) $(PTHREAD_LIBS)
