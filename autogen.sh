#!/bin/sh
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

VERSION=1.9

run_versioned() {
    local P
    type -p "$1-$2" &> /dev/null && P="$1-$2" \
	|| type -p "$1`echo $2 | tr -d '.'`" &> /dev/null && P="$1`echo $2 | tr -d '.'`" \
	|| local P="$1"

    shift 2
    "$P" "$@"
}

set -ex

if [ "x$1" = "xam" ] ; then
    run_versioned automake "$VERSION" -a -c --foreign
    ./config.status
else 
    rm -rf autom4te.cache
    rm -f config.cache

    run_versioned libtoolize 1.5 -c --force
    run_versioned aclocal "$VERSION" -I common
    run_versioned autoconf 2.59 -Wall
    run_versioned autoheader 2.59
    run_versioned automake "$VERSION" -a -c --foreign

    if test "x$NOCONFIGURE" = "x"; then
        ./configure "$@"
        make clean
    fi
fi

# on FreeBSD i must copy this file
# cp /usr/local/share/aclocal/libtool15.m4 common/
# cp /usr/local/share/aclocal/pkg.m4 common/
#./configure --disable-qt3 --disable-qt4 --disable-mono --disable-monodoc --disable-python --disable-dbus --disable-glib --disable-expat --disable-libdaemon --with-distro=none --disable-gtk --disable-xmltoman
