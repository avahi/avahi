#!/bin/bash

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

set -eux

sed -i 's/check_inconsistencies=yes/check_inconsistencies=no/' common/acx_pthread.m4

./autogen.sh \
    --disable-stack-protector --disable-qt3 --disable-qt4 --disable-qt5 --disable-gtk \
    --disable-gtk3 --disable-dbus --disable-gdbm --disable-libdaemon --disable-python \
    --disable-manpages --disable-mono --disable-monodoc --disable-glib --disable-gobject \
    --disable-libevent

make -j"$(nproc)" V=1

for f in fuzz/fuzz-*.c; do
    fuzz_target=$(basename "$f" .c)
    $CC -c $CFLAGS -I. \
        "fuzz/$fuzz_target.c" \
        -o "$fuzz_target.o"

    $CXX $CXXFLAGS \
        "$fuzz_target.o" \
        -o "$OUT/$fuzz_target" \
        $LIB_FUZZING_ENGINE \
        "avahi-core/.libs/libavahi-core.a" "avahi-common/.libs/libavahi-common.a"
done

for t in consume-{key,record}; do
    wget -O "$OUT/fuzz-${t}_seed_corpus.zip" \
        "https://storage.googleapis.com/avahi-backup.clusterfuzz-external.appspot.com/corpus/libFuzzer/avahi_packet_${t//-/_}_fuzzer/public.zip"
done
