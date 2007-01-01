# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# $Id$

# This file is part of nss-mdns.
#
# nss-mdns is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version. 
#
# nss-mdns is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with nss-mdns; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

AC_PREREQ(2.57)
AC_INIT([nss-mdns],[0.8],[mzaffzqaf (at) 0pointer (dot) de])
AC_CONFIG_SRCDIR([src/query.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign 1.9 -Wall])

AC_SUBST(PACKAGE_URL, [http://0pointer.de/lennart/projects/nss-mdns/])

ac_default_prefix="/"

AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

test_gcc_flag() {
    AC_LANG_CONFTEST([int main() {}])
    $CC -c conftest.c $CFLAGS $@ > /dev/null 2> /dev/null
    ret=$?
    rm -f conftest.o
    return $ret
}

# If using GCC specify some additional parameters
if test "x$GCC" = "xyes" ; then

    # We use gnu99 instead of c99 because many have interpreted the standard
    # in a way that int64_t isn't defined on non-64 bit platforms.
    DESIRED_FLAGS="-std=gnu99 -Wall -W -Wextra -pedantic -pipe -Wformat -Wold-style-definition -Wdeclaration-after-statement -Wfloat-equal -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wmissing-noreturn -Wshadow -Wendif-labels -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Winline -Wno-unused-parameter"

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

# libtool stuff
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h nss.h sys/ioctl.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_CHECK_FUNCS([gethostbyaddr gethostbyname gettimeofday inet_ntoa memset select socket strcspn strdup strerror strncasecmp strcasecmp strspn])

# If using GCC specify some additional parameters
if test "x$GCC" = "xyes" ; then
   CFLAGS="$CFLAGS -pipe -W -Wall -pedantic"

   AC_LANG_CONFTEST([int main() {}])
   $CC -c conftest.c -std=c99 -Wno-unused-parameter $CFLAGS > /dev/null 2> /dev/null && CFLAGS="$CFLAGS -std=c99 -Wno-unused-parameter"
   rm -f conftest.o
fi

# LYNX documentation generation
AC_ARG_ENABLE(lynx,
        AS_HELP_STRING(--disable-lynx,Turn off lynx usage for documentation generation),
[case "${enableval}" in
  yes) lynx=yes ;;
  no)  lynx=no ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --disable-lynx) ;;
esac],[lynx=yes])

if test x$lynx = xyes ; then
   AC_CHECK_PROG(have_lynx, lynx, yes, no)

   if test x$have_lynx = xno ; then
     lynx=no
     AC_CHECK_PROG(have_links, links, yes, no)
     if test x$have_links = xno ; then
       AC_MSG_WARN([*** neither lynx nor links found, plain text README will not be built ***])
     else
       links=yes
     fi
   fi
fi

AM_CONDITIONAL([USE_LYNX], [test "x$lynx" = xyes])
AM_CONDITIONAL([USE_LINKS], [test "x$links" = xyes])

### Enable avahi support?

AC_ARG_ENABLE(avahi,
        AC_HELP_STRING([--enable-avahi],[Enable lookups with Avahi (default=yes)]),
        [case "${enableval}" in
                yes) ENABLE_AVAHI=yes ;;
                no)  ENABLE_AVAHI=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-avahi) ;;
        esac],
        [ENABLE_AVAHI=yes]) dnl Default value

AM_CONDITIONAL([ENABLE_AVAHI], [test "x$ENABLE_AVAHI" = "xyes"])

if test "x$ENABLE_AVAHI" = "xyes" ; then
   AC_DEFINE(ENABLE_AVAHI, [1], [Try to make use of a running avahi mDNS server before resolving on our own behalf])
fi

### Enable legacy unicast mDNS support?

AC_ARG_ENABLE(legacy,
        AC_HELP_STRING([--enable-legacy],[Enable legacy unicast mDNS lookups when Avahi isn't available (default=no)]),
        [case "${enableval}" in
                yes) ENABLE_LEGACY=yes ;;
                no)  ENABLE_LEGACY=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-legacy) ;;
        esac],
        [ENABLE_LEGACY=no]) dnl Default value

if test "x$ENABLE_AVAHI" = "xno" -a "x$ENABLE_LEGACY" = "xno"; then
	AC_MSG_ERROR(Can't disable legacy unicast mDNS lookups if Avahi support is disabled)
fi

if test "x$ENABLE_LEGACY" = "xyes" ; then
   AC_DEFINE(ENABLE_LEGACY, [1], [Enable legacy unicast mDNS lookups when Avahi isn't available])
fi

AM_CONDITIONAL([ENABLE_LEGACY], [test "x$ENABLE_LEGACY" = "xyes"])

### Honour search domains from /etc/resolv.conf?

AC_ARG_ENABLE(search-domains,
        AC_HELP_STRING([--enable-search-domains],[Honour search domains from /etc/resolv.conf (default=no)]),
        [case "${enableval}" in
                yes) HONOUR_SEARCH_DOMAINS=yes ;;
                no)  HONOUR_SEARCH_DOMAINS=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-search-domains) ;;
        esac],
        [HONOUR_SEARCH_DOMAINS=no]) dnl Default value

if test "x$HONOUR_SEARCH_DOMAINS" = "xyes" ; then
   AC_DEFINE(HONOUR_SEARCH_DOMAINS, [1], [Honour search domains from /etc/resolv.conf])
fi


AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile doc/README.html ])
AC_OUTPUT
