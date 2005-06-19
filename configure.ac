# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

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

AC_PREREQ(2.57)
AC_INIT([avahi],[0.1],[mzninuv (at) 0pointer (dot) de])
AC_CONFIG_SRCDIR([avahi-core])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign -Wall])

AC_SUBST(PACKAGE_URL, [http://www.freedesktop.org/Software/Avahi])

if type -p stow > /dev/null && test -d /usr/local/stow ; then
   AC_MSG_NOTICE([*** Found /usr/local/stow: default install prefix set to /usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION} ***])
   ac_default_prefix="/usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION}"
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# libtool stuff
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_CHECK_FUNCS([gethostname memset select socket strchr strcspn strerror uname])

AC_CHECK_FUNCS(setresuid)
AC_CHECK_FUNCS(setreuid)

# Check for GLIB 2.0
PKG_CHECK_MODULES(GLIB20, [ glib-2.0 >= 2.4.0 ])
AC_SUBST(GLIB20_CFLAGS)
AC_SUBST(GLIB20_LIBS)

AC_ARG_ENABLE(gtk,
        AC_HELP_STRING([--enable-gtk],[use GTK+ (default=yes)]),
        [case "${enableval}" in
                yes) ENABLE_GTK=yes ;;
                no)  ENABLE_GTK=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-gtk) ;;
        esac],
        [ENABLE_GTK=yes]) dnl Default value

if test "x$ENABLE_GTK" = "xyes"; then
        # Check for GTK 2.0
        PKG_CHECK_MODULES(GTK20, [ gtk+-2.0 >= 2.4.0 ])
        AC_SUBST(GTK20_CFLAGS)
        AC_SUBST(GTK20_LIBS)
        
        # Check for GLADE 2.0
        PKG_CHECK_MODULES(GLADE20, [ libglade-2.0 >= 2.4.0 ])
        AC_SUBST(GLADE20_CFLAGS)
        AC_SUBST(GLADE20_LIBS)
fi
AM_CONDITIONAL(ENABLE_GTK, test "x$ENABLE_GTK" = "xyes")

AC_ARG_ENABLE(doxygen,
        AC_HELP_STRING([--enable-doxygen],[use doxygen to generate API docs (default=yes)]),
        [case "${enableval}" in
                yes) ENABLE_DOXYGEN=yes ;;
                no)  ENABLE_DOXYGEN=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-gtk) ;;
        esac],
        [ENABLE_DOXYGEN=yes]) dnl Default value

AM_CONDITIONAL(ENABLE_DOXYGEN, test "x$ENABLE_DOXYGEN" = "xyes")

AC_ARG_ENABLE(dbus,
        AC_HELP_STRING([--enable-dbus],[use DBus (default=yes)]),
        [case "${enableval}" in
                yes) ENABLE_DBUS=yes ;;
                no)  ENABLE_DBUS=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-dbus) ;;
        esac],
	[ENABLE_DBUS=yes]) dnl Default value

AC_DEFINE(ENABLE_DBUS, 1, [Whether to use DBUS or not])

if test "x$ENABLE_DBUS" = "xyes"; then
        PKG_CHECK_MODULES(DBUS, [ dbus-glib-1 >= 0.23])
        AC_SUBST(DBUS_CFLAGS)
	AC_SUBST(DBUS_LIBS)

	if pkg-config dbus-1 --atleast-version=0.30 ; then
		AC_DEFINE(DBUS_USE_NEW_API, 1, [Whether to use the new API for DBUS 0.30])
	fi
fi

AM_CONDITIONAL(ENABLE_DBUS, test "x$ENABLE_DBUS" = "xyes")

AC_CHECK_LIB(expat, XML_ParserCreate, [ AC_CHECK_HEADERS(expat.h, have_expat=true, have_expat=false) ], have_expat=false)

if ! $have_expat ; then
   AC_MSG_ERROR([*** libexpat not found ***])
fi

PKG_CHECK_MODULES(LIBDAEMON, [ libdaemon >= 0.5 ])
AC_SUBST(LIBDAEMON_CFLAGS)
AC_SUBST(LIBDAEMON_LIBS)

# If using GCC specify some additional parameters
if test "x$GCC" = "xyes" ; then
   CFLAGS="$CFLAGS -pipe -W -Wall -pedantic"

   AC_LANG_CONFTEST([int main() {}])
   $CC -c conftest.c -std=c99 -Wno-unused-parameter $CFLAGS > /dev/null 2> /dev/null && CFLAGS="$CFLAGS -std=c99 -Wno-unused-parameter"
   rm -f conftest.o
fi

AC_CONFIG_FILES([Makefile avahi-core.pc doxygen/Makefile doxygen/doxygen.conf avahi-common/Makefile avahi-core/Makefile avahi-daemon/Makefile avahi-discover/Makefile avahi-client/Makefile])
AC_OUTPUT
