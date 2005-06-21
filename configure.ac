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
AC_CHECK_FUNCS([gethostname memchr memmove memset mkdir select socket strchr strcspn strerror strrchr strstr uname setresuid setreuid])

AC_FUNC_CHOWN
AC_FUNC_STAT
AC_TYPE_MODE_T
AC_TYPE_PID_T

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

	DATADIRNAME=share
	if test "x${prefix}" = "xNONE"; then
	   AC_DEFINE_UNQUOTED(INTERFACES_DIR, "${ac_default_prefix}/${DATADIRNAME}/${PACKAGE}/interfaces/", [path where glade files will be installed])
	   interfacesdir="${ac_default_prefix}/${DATADIRNAME}/${PACKAGE}/interfaces/"
	   AC_SUBST(interfacesdir)
	else
	   AC_DEFINE_UNQUOTED(INTERFACES_DIR, "${prefix}/${DATADIRNAME}/${PACKAGE}/interfaces/", [path where glade files will be installed])
	   interfacesdir="${prefix}/${DATADIRNAME}/${PACKAGE}/interfaces/"
	   AC_SUBST(interfacesdir)
	fi

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


if test "x$ENABLE_DBUS" = "xyes"; then
	AC_DEFINE(ENABLE_DBUS, 1, [Whether to use DBUS or not])
        PKG_CHECK_MODULES(DBUS, [ dbus-glib-1 >= 0.23])

	AC_ARG_WITH(dbus-sys, [  --with-dbus-sys=<dir>   where D-BUS system.d directory is])

	if ! test -z "$with_dbus_sys" ; then
    	   DBUS_SYS_DIR="$with_dbus_sys"
	else
	   DBUS_SYS_DIR="${sysconfdir}/dbus-1/system.d"
	fi
	AC_SUBST(DBUS_SYS_DIR)
	AC_DEFINE_UNQUOTED(DBUS_SYSTEMD_DIR, "$DBUS_SYS_DIR", [Where system.d dir for DBUS is])

	if pkg-config dbus-1 --atleast-version=0.30 ; then
	   AC_DEFINE(DBUS_USE_NEW_API, 1, [Whether to use the new API for DBUS 0.30])
	fi

        AC_SUBST(DBUS_CFLAGS)
	AC_SUBST(DBUS_LIBS)
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

#
# Detecting the linux distro for specific things like initscripts.
#
AC_ARG_WITH(distro, AC_HELP_STRING([--with-distro=DISTRO], [Specify the Linux distribution to target: One of redhat, suse, gentoo, debian, or slackware]))
if test "z$with_distro" = "z"; then
   AC_CHECK_FILE(/etc/redhat-release,with_distro="redhat")
   AC_CHECK_FILE(/etc/SuSE-release,with_distro="suse")
   AC_CHECK_FILE(/etc/fedora-release,with_distro="redhat")
   AC_CHECK_FILE(/etc/gentoo-release,with_distro="gentoo")
   AC_CHECK_FILE(/etc/debian_version,with_distro="debian")
   AC_CHECK_FILE(/etc/slackware-version,with_distro="slackware")
fi
with_distro=`echo ${with_distro} | tr '[[:upper:]]' '[[:lower:]]' `

if test "z$with_distro" = "z"; then
   echo "Linux distribution autodetection failed, you must specify the distribution to target using --with-distro=DISTRO"
   exit 1
else
case $with_distro in
   debian)
     ;;
   *)
     echo "Your distribution (${with_distro}) is not yet supported!  (patches welcome)"
     exit 1
     ;;
esac

fi
AM_CONDITIONAL(TARGET_REDHAT, test x"$with_distro" = xredhat)
AM_CONDITIONAL(TARGET_SUSE, test x"$with_distro" = xsuse)
AM_CONDITIONAL(TARGET_GENTOO, test x"$with_distro" = xgentoo)
AM_CONDITIONAL(TARGET_DEBIAN, test x"$with_distro" = xdebian)
AM_CONDITIONAL(TARGET_SLACKWARE, test x"$with_distro" = xslackware)

#
# Defining Avahi User and Group.
#
AC_ARG_WITH(avahi_user,[  --with-avahi-user=<user>  User for running the Avahi daemon (avahi)])
if test -z "$with_avahi_user" ; then
    AVAHI_USER=avahi
else
    AVAHI_USER=$with_avahi_user
fi
AC_SUBST(AVAHI_USER)
AC_DEFINE_UNQUOTED(AVAHI_USER,"$AVAHI_USER", [User for running the Avahi daemon])

AC_ARG_WITH(avahi_group,[  --with-avahi-group=<grp>  Group for Avahi (avahi)])
if test -z "$with_avahi_group" ; then
    AVAHI_GROUP=avahi
else
    AVAHI_GROUP=$with_avahi_group
fi
AC_SUBST(AVAHI_GROUP)
AC_DEFINE_UNQUOTED(AVAHI_GROUP,"$AVAHI_GROUP", [Group for Avahi])

AC_CONFIG_FILES([
Makefile 
avahi-core.pc 
doxygen/Makefile 
doxygen/doxygen.conf 
avahi-common/Makefile 
avahi-core/Makefile 
avahi-daemon/Makefile 
avahi-daemon/avahi-dbus.conf
avahi-discover/Makefile 
avahi-client/Makefile 
initscript/Makefile 
initscript/Debian/Makefile
initscript/Debian/15avahi
])
AC_OUTPUT

dnl ==========================================================================
echo "
                   $PACKAGE_NAME $VERSION
                  ============

        prefix:                   ${prefix}
        sysconfdir:               ${sysconfdir}
        dbus-1 system.d dir:      ${DBUS_SYS_DIR}
	dbus-1 version:           `pkg-config dbus-1 --modversion`
        compiler:                 ${CC}
        cflags:                   ${CFLAGS}
	Linux Distro:             ${with_distro}
        User for Avahi:           ${AVAHI_USER}
        Group for Avahi:          ${AVAHI_GROUP}
"

echo "NOTE: Remember to create user ${AVAHI_USER} and group ${AVAHI_GROUP} before make install"
echo
