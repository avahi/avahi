#!/bin/bash

set -eux
set -o pipefail

export ASAN_UBSAN=${ASAN_UBSAN:-false}

case "$1" in
    install-build-deps)
        sed -i -e '/^#\s*deb-src.*\smain\s\+restricted/s/^#//' /etc/apt/sources.list
        apt-get update -y
        apt-get build-dep -y avahi
        apt-get install -y libevent-dev qtbase5-dev
        apt-get install -y gcc clang
        ;;
    build)
        if [[ "$ASAN_UBSAN" == true ]]; then
            export CFLAGS="-fsanitize=address,undefined -g"
            export CXXFLAGS="$CFLAGS"
            export ASAN_OPTIONS=strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
            export UBSAN_OPTIONS=print_stacktrace=1:print_summary=1:halt_on_error=1

            # This kludge prevents autoconf from complaining about pthreads. The right fix would be
            # to replace that out-of-date acx_pthread.m4 with ax_pthread.m4 but it isn't clear how
            # safe it is to bump autoconf: https://github.com/lathiat/avahi/pull/377. Since it's a
            # niche use case and nobody else seems to have complained about it so far let's keep the
            # kludge until some sort of policy covering build dependencies is settled.
            if [[ "$CC" == clang ]]; then
                sed -i 's/check_inconsistencies=yes/check_inconsistencies=no/' common/acx_pthread.m4
            fi
        fi

        if ! ./bootstrap.sh --enable-tests --prefix=/usr; then
            cat config.log
            exit 1
        fi

        make -j"$(nproc)" V=1
        make check VERBOSE=1
        ;;
    *)
        printf '%s' "Unknown command '$1'" >&2
        exit 1
esac
