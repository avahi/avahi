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

if ENABLE_AUTOIPD
if HAVE_LIBDAEMON

pkgsysconfdir=$(sysconfdir)/avahi

AM_CFLAGS= \
	-I$(top_srcdir)

# This cool debug trap works on i386/gcc only
AM_CFLAGS+='-DDEBUG_TRAP=__asm__("int $$3")' \
	-DAVAHI_RUNTIME_DIR=\"$(avahi_runtime_dir)/\" \
	-DAVAHI_IPCONF_SCRIPT=\"$(pkgsysconfdir)/avahi-autoipd.action\" \
	-DAVAHI_IPDATA_DIR=\"$(localstatedir)/lib/avahi-autoipd\"

sbin_PROGRAMS = avahi-autoipd

avahi_autoipd_SOURCES = \
	main.c main.h \
	 ../avahi-daemon/setproctitle.c  ../avahi-daemon/setproctitle.h \
	iface.h iface-linux.c \
	../avahi-common/malloc.h ../avahi-common/malloc.c \
	../avahi-common/timeval.h ../avahi-common/timeval.c

avahi_autoipd_CFLAGS = $(AM_CFLAGS) $(LIBDAEMON_CFLAGS)
avahi_autoipd_LDADD = $(AM_LDADD) $(LIBDAEMON_LIBS)

pkgsysconf_SCRIPTS=avahi-autoipd.action

endif
endif

EXTRA_DIST=avahi-autoipd.action