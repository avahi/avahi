# $Id$
#
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

AM_CFLAGS= \
	-I$(top_srcdir)

# GLIB 2.0
AM_CFLAGS+=$(GLIB20_CFLAGS)
AM_LDADD=$(GLIB20_LIBS)

noinst_PROGRAMS = \
	c-plus-plus-test

c_plus_plus_test_SOURCES = \
	c-plus-plus-test.cc

c_plus_plus_CFLAGS = $(AM_CFLAGS)
c_plus_plus_test_LDADD = $(AM_LDADD) ../avahi-common/libavahi-common.la ../avahi-core/libavahi-core.la

EXTRA_DIST=c-plus-plus-test-gen.py

gen:
	python ./c-plus-plus-test-gen.py > c-plus-plus-test.cc
