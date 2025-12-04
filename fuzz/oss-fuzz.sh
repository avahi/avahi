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

export OUT=${OUT:-"$(pwd)/out"}
mkdir -p "$OUT"

export LIB_FUZZING_ENGINE=${LIB_FUZZING_ENGINE:--fsanitize=fuzzer}

export MERGE_WITH_OSS_FUZZ_CORPORA=${MERGE_WITH_OSS_FUZZ_CORPORA:-no}

if [[ -n "${FUZZING_ENGINE:-}" ]]; then
    apt-get update
    apt-get install -y autoconf gettext libtool m4 automake pkg-config libexpat-dev zipmerge

    if [[ "$ARCHITECTURE" == i386 ]]; then
        apt-get install -y libexpat-dev:i386
    fi

    if [[ "$SANITIZER" == undefined ]]; then
        additional_ubsan_checks=pointer-overflow,alignment
        UBSAN_FLAGS="-fsanitize=$additional_ubsan_checks -fno-sanitize-recover=$additional_ubsan_checks"
        CFLAGS="$CFLAGS $UBSAN_FLAGS"
        CXXFLAGS="$CXXFLAGS $UBSAN_FLAGS"
    fi

    # The following kludge gets around MSan false positives like https://github.com/avahi/avahi/issues/787
    # by reusing the homegrown strlcpy function even when glibc comes with strlcpy.
    # It should be removed when https://github.com/llvm/llvm-project/issues/114377 is fixed.
    if grep -qF strlcpy /usr/include/string.h && [[ "$SANITIZER" == memory ]]; then
        sed -i '
            s/#ifndef HAVE_STRLCPY/#ifdef HAVE_STRLCPY/
            s/strlcpy/msan_friendly_strlcpy/
            ' avahi-common/domain.c
    fi
fi

sed -i 's/check_inconsistencies=yes/check_inconsistencies=no/' common/acx_pthread.m4

if ! ./autogen.sh \
    --disable-stack-protector --disable-qt3 --disable-qt4 --disable-qt5 --disable-gtk \
    --disable-gtk3 --disable-dbus --disable-gdbm --disable-libdaemon --disable-python \
    --disable-manpages --disable-mono --disable-monodoc --disable-glib --disable-gobject \
    --disable-libevent --disable-libsystemd; then
    cat config.log
    exit 1
fi

make -j"$(nproc)" V=1

for f in fuzz/fuzz-*.c; do
    fuzz_target=$(basename "$f" .c)
    additional_obj_files=

    # CFLAGS have to be split
    # shellcheck disable=SC2086
    $CC -c $CFLAGS -I. \
        "fuzz/$fuzz_target.c" \
        -o "$fuzz_target.o"

    if [[ "$fuzz_target" == "fuzz-ini-file-parser" ]]; then
        # CFLAGS have to be split
        # shellcheck disable=SC2086
        $CC -c $CFLAGS -I. avahi-daemon/ini-file-parser.c -o ini-file-parser.o
        additional_obj_files+=" ini-file-parser.o"
    fi

    # CXXFLAGS have to be split
    # shellcheck disable=SC2086
    $CXX $CXXFLAGS \
        "$fuzz_target.o" \
        -o "$OUT/$fuzz_target" \
        $LIB_FUZZING_ENGINE \
        $additional_obj_files \
        "avahi-core/.libs/libavahi-core.a" "avahi-common/.libs/libavahi-common.a"
done

# Let's take the systemd public corpus here. It has been accumulating since 2018
# so it should be good enough for our purposes.
wget -O "$OUT/fuzz-packet_seed_corpus.zip" \
    https://storage.googleapis.com/systemd-backup.clusterfuzz-external.appspot.com/corpus/libFuzzer/systemd_fuzz-dns-packet/public.zip

if [[ "$MERGE_WITH_OSS_FUZZ_CORPORA" == "yes" ]]; then
    for f in "$OUT/"fuzz-*; do
        [[ -x "$f" ]] || continue
        fuzzer=$(basename "$f")
        t=$(mktemp)
        if wget -O "$t" "https://storage.googleapis.com/avahi-backup.clusterfuzz-external.appspot.com/corpus/libFuzzer/avahi_${fuzzer}/public.zip"; then
            zipmerge "$OUT/${fuzzer}_seed_corpus.zip" "$t"
        fi
        rm -rf "$t"
    done
fi
