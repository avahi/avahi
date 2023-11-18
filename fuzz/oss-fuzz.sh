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

# This script can be run using docker and the OSS-Fuzz toolchain:
# https://google.github.io/oss-fuzz/advanced-topics/reproducing/#building-using-docker
#
# It has to use a few variables like OUT and LIB_FUZZING_ENGINE:
# https://google.github.io/oss-fuzz/getting-started/new-project-guide/#buildsh
#
# The script can also be run locally without the docker machinery by installing
# clang and running `./fuzz/oss-fuzz.sh`. The fuzz targets are built with
# ASan/UBSan and put in the out directory.

set -eux

flags="-O1 -fno-omit-frame-pointer -g -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -fsanitize=address,undefined -fsanitize=fuzzer-no-link"

export CC=${CC:-clang}
export CFLAGS=${CFLAGS:-$flags}

export CXX=${CXX:-clang++}
export CXXFLAGS=${CXXFLAGS:-$flags}

cd "$(dirname -- "$0")/.."

export OUT=${OUT:-"$(pwd)/out"}
mkdir -p "$OUT"

export LIB_FUZZING_ENGINE=${LIB_FUZZING_ENGINE:--fsanitize=fuzzer}

sed -i 's/check_inconsistencies=yes/check_inconsistencies=no/' common/acx_pthread.m4

./autogen.sh \
    --disable-stack-protector --disable-qt3 --disable-qt4 --disable-qt5 --disable-gtk \
    --disable-gtk3 --disable-dbus --disable-gdbm --disable-libdaemon --disable-python \
    --disable-manpages --disable-mono --disable-monodoc --disable-glib --disable-gobject \
    --disable-libevent --disable-libsystemd

make -j"$(nproc)" V=1

for f in fuzz/fuzz-*.c; do
    fuzz_target=$(basename "$f" .c)

    # CFLAGS have to be split
    # shellcheck disable=SC2086
    $CC -c $CFLAGS -I. \
        "fuzz/$fuzz_target.c" \
        -o "$fuzz_target.o"

    # CXXFLAGS have to be split
    # shellcheck disable=SC2086
    $CXX $CXXFLAGS \
        "$fuzz_target.o" \
        -o "$OUT/$fuzz_target" \
        $LIB_FUZZING_ENGINE \
        "avahi-core/.libs/libavahi-core.a" "avahi-common/.libs/libavahi-common.a"
done

# Let's take the systemd public corpus here. It has been accumulating since 2018
# so it should be good enough for our purposes.
wget -O "$OUT/fuzz-packet_seed_corpus.zip" \
    https://storage.googleapis.com/systemd-backup.clusterfuzz-external.appspot.com/corpus/libFuzzer/systemd_fuzz-dns-packet/public.zip
