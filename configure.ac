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
AC_INIT([avahi],[0.2],[avahi (at) lists (dot) freedesktop (dot) org])
AC_CONFIG_SRCDIR([avahi-core/server.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign 1.9 -Wall])

AC_SUBST(PACKAGE_URL, [http://www.freedesktop.org/Software/Avahi])

if type -p stow > /dev/null && test -d /usr/local/stow ; then
   AC_MSG_NOTICE([*** Found /usr/local/stow: default install prefix set to /usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION} ***])
   ac_default_prefix="/usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION}"
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_GNU_SOURCE
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_GCC_TRADITIONAL

# libtool stuff
AC_PROG_LIBTOOL

#
# Detecting the linux distribution for specific things like init scripts.
#
AC_ARG_WITH(distro, AS_HELP_STRING([--with-distro=DISTRO],[Specify the Linux distribution to target: One of debian, gentoo, archlinux or none]))
if test "z$with_distro" = "z"; then
   #AC_CHECK_FILE(/etc/SuSE-release,with_distro="suse")
   AC_CHECK_FILE(/etc/gentoo-release,with_distro="gentoo")
   AC_CHECK_FILE(/etc/arch-release,with_distro="archlinux")
   AC_CHECK_FILE(/etc/debian_version,with_distro="debian")
fi
with_distro=`echo ${with_distro} | tr '[[:upper:]]' '[[:lower:]]' `

if test "z$with_distro" = "z"; then
   AC_MSG_ERROR([Linux distribution autodetection failed, you must specify the distribution to target using --with-distro=DISTRO])
   exit 1
else
case $with_distro in
   debian|gentoo|archlinux)
     ;;
   *)
     AC_MSG_ERROR([Your distribution (${with_distro}) is not yet supported, init scripts and dbus configuration will not be installed! (patches welcome)])
     ;;
esac
fi

#AM_CONDITIONAL(TARGET_SUSE, test x"$with_distro" = xsuse)
AM_CONDITIONAL(TARGET_GENTOO, test x"$with_distro" = xgentoo)
AM_CONDITIONAL(TARGET_DEBIAN, test x"$with_distro" = xdebian)
AM_CONDITIONAL(TARGET_ARCHLINUX, test x"$with_distro" = xarchlinux)

test_gcc_flag() {
   AC_LANG_CONFTEST([int main() {}])
   $CC -c conftest.c $CFLAGS $@ > /dev/null 2> /dev/null
   ret=$?
   rm -f conftest.o
   return $ret
}

# If using GCC specify some additional parameters
if test "x$GCC" = "xyes" ; then

    DESIRED_FLAGS="-Wall -W -Wextra -pedantic -pipe -Wformat -Wold-style-definition -Wdeclaration-after-statement -Wfloat-equal -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wmissing-noreturn -Wshadow -Wendif-labels -Wlarger-than-4000 -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Winline -Wno-unused-parameter"

    # Test whether rtnetlink.h can be included when compiled with -std=c99
    # some distributions (e.g. archlinux) have broken headers that dont
    # define __u64 with -std=c99
    AC_MSG_CHECKING([checking whether rtnetlink.h can be included with -std=c99])
    OLDCFLAGS="$CFLAGS"
    CFLAGS="-std=c99"
    AC_TRY_COMPILE([#include <linux/rtnetlink.h>], [],
        use_stdc99=yes, use_stdc99=no)

    if test x"$use_stdc99" = xyes; then
        DESIRED_FLAGS="-std=c99 $DESIRED_FLAGS"
        AC_MSG_RESULT([yes])
    else
        AC_MSG_RESULT([no])
    fi

    CFLAGS="$OLDCFLAGS"

    for flag in $DESIRED_FLAGS ; do
        AC_MSG_CHECKING([whether $CC accepts $flag])
        if test_gcc_flag $flag ; then 
           CFLAGS="$CFLAGS $flag"
           AC_MSG_RESULT([yes])
        else
           AC_MSG_RESULT([no])
        fi
    done 
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_HEADER_SYS_WAIT

# Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gethostname memchr memmove memset mkdir select socket strchr strcspn strdup strerror strrchr strspn strstr uname setresuid setreuid strcasecmp gettimeofday putenv])

AC_FUNC_CHOWN
AC_FUNC_STAT
AC_TYPE_MODE_T
AC_TYPE_PID_T

PKG_PROG_PKG_CONFIG

#
# Check for GLIB 2.0
#
AC_ARG_ENABLE(glib,
        AS_HELP_STRING([--disable-glib],[Disable use of GLib]),
        [case "${enableval}" in
                yes) HAVE_GLIB=yes ;;
                no)  HAVE_GLIB=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-glib) ;;
        esac],
        [HAVE_GLIB=yes])

if test "x$HAVE_GLIB" = "xyes" ; then
        PKG_CHECK_MODULES(GLIB20, [ glib-2.0 >= 2.4.0 ])
        AC_SUBST(GLIB20_CFLAGS)
        AC_SUBST(GLIB20_LIBS)
fi
AM_CONDITIONAL(HAVE_GLIB, test "x$HAVE_GLIB" = "xyes")

#
# Check for GTK+
#
AC_ARG_ENABLE(gtk,
        AS_HELP_STRING([--disable-gtk],[Disable use of GTK+]),
        [case "${enableval}" in
                yes) HAVE_GTK=yes ;;
                no)  HAVE_GTK=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-gtk) ;;
        esac],
        [HAVE_GTK=yes])

if test "x$HAVE_GTK" = "xyes" ; then
        # Check for GTK 2.0
        PKG_CHECK_MODULES(GTK20, [ gtk+-2.0 >= 2.4.0 ])
        AC_SUBST(GTK20_CFLAGS)
        AC_SUBST(GTK20_LIBS)
        
        # Check for GLADE 2.0
        PKG_CHECK_MODULES(GLADE20, [ libglade-2.0 >= 2.4.0 ])
        AC_SUBST(GLADE20_CFLAGS)
        AC_SUBST(GLADE20_LIBS)

	    interfacesdir="${datadir}/${PACKAGE}/interfaces/"
	    AC_SUBST(interfacesdir)
fi
AM_CONDITIONAL(HAVE_GTK, test "x$HAVE_GTK" = "xyes")

#
# D-BUS
#
AC_ARG_ENABLE(dbus,
        AS_HELP_STRING([--disable-dbus],[Disable use of D-BUS]),
        [case "${enableval}" in
                yes) HAVE_DBUS=yes ;;
                no)  HAVE_DBUS=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-dbus) ;;
        esac],
        [HAVE_DBUS=yes]) 

AC_ARG_WITH(dbus-sys, AS_HELP_STRING([--with-dbus-sys=<dir>], [where D-BUS system.d directory is]))

if test "x$HAVE_DBUS" = "xyes" ; then
	AC_DEFINE(HAVE_DBUS, 1, [Whether we have D-BUS or not])

    PKG_CHECK_MODULES(DBUS, [ dbus-1 >= 0.30 ])
    DBUS_CFLAGS="$DBUS_CFLAGS -DDBUS_API_SUBJECT_TO_CHANGE"
    AC_SUBST(DBUS_CFLAGS)
	AC_SUBST(DBUS_LIBS)

	if ! test -z "$with_dbus_sys" ; then
    	   DBUS_SYS_DIR="$with_dbus_sys"
	else
	       DBUS_SYS_DIR="${sysconfdir}/dbus-1/system.d"
	fi
	AC_SUBST(DBUS_SYS_DIR)
    
fi
AM_CONDITIONAL(HAVE_DBUS, test "x$HAVE_DBUS" = "xyes")

#
# Expat
#
AC_ARG_ENABLE(expat,
        AS_HELP_STRING([--disable-expat],[Disable use of Expat]),
        [case "${enableval}" in
                yes) HAVE_EXPAT=yes ;;
                no)  HAVE_EXPAT=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-expat) ;;
        esac],
        [HAVE_EXPAT=yes]) 

if test "x$HAVE_EXPAT" = "xyes" ; then
   AC_CHECK_LIB(expat, XML_ParserCreate, [ AC_CHECK_HEADERS(expat.h, have_expat=true, have_expat=false) ], have_expat=false)

   if ! $have_expat ; then
      AC_MSG_ERROR([*** libexpat not found ***])
   fi
fi
AM_CONDITIONAL(HAVE_EXPAT, test "x$HAVE_EXPAT" = "xyes")

#
# libdaemon
#
AC_ARG_ENABLE(libdaemon,
        AS_HELP_STRING([--disable-libdaemon],[Disable use of libdaemon]),
        [case "${enableval}" in
                yes) HAVE_LIBDAEMON=yes ;;
                no)  HAVE_LIBDAEMON=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-libdaemon) ;;
        esac],
        [HAVE_LIBDAEMON=yes]) 

if test "x$HAVE_LIBDAEMON" = "xyes" ; then
   PKG_CHECK_MODULES(LIBDAEMON, [ libdaemon >= 0.5 ])
   AC_SUBST(LIBDAEMON_CFLAGS)
   AC_SUBST(LIBDAEMON_LIBS)
fi
AM_CONDITIONAL(HAVE_LIBDAEMON, test "x$HAVE_LIBDAEMON" = "xyes")

#
# Python stuff
#
AC_ARG_ENABLE(python,
	AS_HELP_STRING([--disable-python], [Disable scripts that depends on python]),
	[case "${enableval}" in
	      yes) HAVE_PYTHON=yes ;;
	      no)  HAVE_PYTHON=no ;;
	      *) AC_MSG_ERROR([bad value ${enableval} for --enable-python]) ;;
	esac],[HAVE_PYTHON=yes])

if test "x$HAVE_PYTHON" = "xyes" ; then
   AM_PATH_PYTHON([2.4])
   AM_CHECK_PYMOD(gtk,,,[AC_MSG_ERROR(Could not find Python module pygtk)])
   AM_CHECK_PYMOD(dbus,,,[AC_MSG_ERROR(Could not find Python module dbus)])
fi
AM_CONDITIONAL(HAVE_PYTHON, [test "x$HAVE_PYTHON" = "xyes" ])

#
# Defining Avahi User and Group.
#
AC_ARG_WITH(avahi_user, AS_HELP_STRING([--with-avahi-user=<user>],[User for running the Avahi daemon (avahi)]))
if test -z "$with_avahi_user" ; then
    AVAHI_USER=avahi
else
    AVAHI_USER=$with_avahi_user
fi
AC_SUBST(AVAHI_USER)
AC_DEFINE_UNQUOTED(AVAHI_USER,"$AVAHI_USER", [User for running the Avahi daemon])

AC_ARG_WITH(avahi_group,AS_HELP_STRING([--with-avahi-group=<group>],[Group for Avahi (avahi)]))
if test -z "$with_avahi_group" ; then
    AVAHI_GROUP=avahi
else
    AVAHI_GROUP=$with_avahi_group
fi
AC_SUBST(AVAHI_GROUP)
AC_DEFINE_UNQUOTED(AVAHI_GROUP,"$AVAHI_GROUP", [Group for Avahi])

#
# Avahi runtime dir
#
avahi_runtime_dir="${localstatedir}/run"
avahi_socket="${avahi_runtime_dir}/avahi-daemon/socket"
AC_SUBST(avahi_runtime_dir)
AC_SUBST(avahi_socket)


#
# Doxygen
#
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(avahi, doxygen.cfg, doxygen)

#
# XMLTOMAN manpage generation
#
AC_ARG_ENABLE(xmltoman,
        AS_HELP_STRING([--disable-xmltoman],[Disable rebuilding of man pages with xmltoman]),
[case "${enableval}" in
  yes) xmltoman=yes ;;
  no)  xmltoman=no ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --disable-xmltoman]) ;;
esac],[xmltoman=yes])

if test x$xmltoman = xyes ; then
   AC_CHECK_PROG(have_xmltoman, xmltoman, yes, no)

   if test x$have_xmltoman = xno ; then
     if ! test -e man/avahi-daemon.8 ; then
        AC_MSG_ERROR([*** xmltoman was not found, it is required to build the manpages and they have not been pre-built])
        exit 1
     fi
     AC_MSG_WARN([*** Not rebuilding man pages as xmltoman is not found ***])
     xmltoman=no
   fi
fi

AM_CONDITIONAL([USE_XMLTOMAN], [test "x$xmltoman" = xyes])

# ==========================================================================
AC_CONFIG_FILES([
Makefile 
avahi-common/Makefile 
avahi-core/Makefile 
avahi-glib/Makefile 
avahi-daemon/Makefile 
avahi-daemon/avahi-dbus.conf
avahi-discover-standalone/Makefile 
avahi-client/Makefile 
initscript/Makefile 
initscript/debian/Makefile
initscript/gentoo/Makefile
initscript/archlinux/Makefile
avahi-dnsconfd/Makefile
avahi-utils/Makefile
avahi-utils/avahi/Makefile
examples/Makefile
common/Makefile
man/Makefile
tests/Makefile
])
AC_OUTPUT

# ==========================================================================
echo "
 ---{ $PACKAGE_NAME $VERSION }---

    prefix:                 ${prefix}
    sysconfdir:             ${sysconfdir}
    dbus-1 system.d dir:    ${DBUS_SYS_DIR}
    dbus-1 version:         `pkg-config dbus-1 --modversion`
    compiler:               ${CC}
    cflags:                 ${CFLAGS}
    Enable GLIB:            ${HAVE_GLIB}
    Enable GTK:             ${HAVE_GTK}
    Enable D-BUS:           ${HAVE_DBUS}
    Enable Expat:           ${HAVE_EXPAT}
    Enable libdaemon:       ${HAVE_LIBDAEMON}
    Have Python:            ${HAVE_PYTHON}
    Linux Distro:           ${with_distro}
    User for Avahi:         ${AVAHI_USER}
    Group for Avahi:        ${AVAHI_GROUP}
"

BUILD_DAEMON="no (!)"

if test "x$HAVE_EXPAT" = "xyes" -a "x$HAVE_LIBDAEMON" = "xyes" ; then
   BUILD_DAEMON=yes
fi

BUILD_UTILS="no (!)"

if test "x$BUILD_DAEMON" = "xyes" -a "x$HAVE_DBUS" = "xyes" -a "x$HAVE_PYTHON" = "xyes" ; then
   BUILD_UTILS=yes
fi

BUILD_CLIENT="no (!)"

if test "x$BUILD_DAEMON" = "xyes" -a "x$HAVE_DBUS" = "xyes" ; then
   BUILD_CLIENT=yes
fi

    
echo "
    Building libavahi-core              yes
    Building avahi-daemon:              ${BUILD_DAEMON}
    Building avahi-dnsconfd:            ${BUILD_DAEMON}
    Building avahi-utils:               ${BUILD_UTILS}
    Building libavahi-glib:             ${HAVE_GLIB}
    Building libavahi-client:           ${BUILD_CLIENT}
    Building avahi-discover-standalone: ${HAVE_GTK}
"
