#!/bin/bash

set -eux
set -o pipefail

export ASAN_UBSAN=${ASAN_UBSAN:-false}
export BUILD_ONLY=${BUILD_ONLY:-false}
export CFLAGS=${CFLAGS:-}
export COVERAGE=${COVERAGE:-false}
export DISTCHECK=${DISTCHECK:-false}
export VALGRIND=${VALGRIND:-false}

case "$1" in
    install-build-deps)
        sed -i -e '/^#\s*deb-src.*\smain\s\+restricted/s/^#//' /etc/apt/sources.list
        apt-get update -y
        apt-get build-dep -y avahi
        apt-get install -y libevent-dev qtbase5-dev libsystemd-dev
        apt-get install -y gcc clang lcov

        apt-get install -y valgrind ncat

        apt-get install -y libglib2.0-dev meson
        git clone https://github.com/dbus-fuzzer/dfuzzer
        (cd dfuzzer && meson build && ninja -C build install)

        git clone https://gitlab.com/akihe/radamsa
        (cd radamsa && make -j"$(nproc)" && make install)
        ;;
    build)
        if [[ "$ASAN_UBSAN" == true ]]; then
            export CFLAGS+=" -fsanitize=address,undefined -g"
            export ASAN_OPTIONS=strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
            export UBSAN_OPTIONS=print_stacktrace=1:print_summary=1:halt_on_error=1

            # avahi fails to compile with clang and ASan/UBSan with
            #     configure: error: Missing POSIX Threads support
            # because acx_pthread is out of date. The kludge should be
            # removed once acx_pthread gets updated.
            if [[ "$CC" == clang ]]; then
                sed -i 's/check_inconsistencies=yes/check_inconsistencies=no/' common/acx_pthread.m4
            fi

        fi

        if [[ "$COVERAGE" == true ]]; then
            export CFLAGS+=" --coverage"
        fi

        # Some parts of avahi like avahi-qt and c-plus-plus-test are built with CXX so
        # it should match CC. To avoid weird side-effects CXXFLAGS should fully match
        # CFLAGS as well so they are fully replaced with CFLAGS here.
        if [[ "$CC" == gcc ]]; then
            export CXX=g++
        elif [[ "$CC" == clang ]]; then
            export CXX=clang++
        fi
        export CXXFLAGS="$CFLAGS"

        ./bootstrap.sh --enable-compat-howl --enable-compat-libdns_sd --enable-tests \
            --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu
        make -j"$(nproc)" V=1

        if [[ "$BUILD_ONLY" == true ]]; then
            exit 0
        fi

        if [[ "$DISTCHECK" == true ]]; then
            make distcheck
        fi

        make check VERBOSE=1

        sed -i '/^ExecStart=/s/$/ --debug /' avahi-daemon/avahi-daemon.service

        if [[ "$VALGRIND" == true ]]; then
            sed -i '
                /^ExecStart/s/=/=valgrind --leak-check=full --track-origins=yes --track-fds=yes --error-exitcode=1 /
            ' avahi-daemon/avahi-daemon.service
            sed -i '/^ExecStart=/s/$/ --no-chroot --no-drop-root --no-proc-title/' avahi-daemon/avahi-daemon.service
        fi

        if [[ "$COVERAGE" == true ]]; then
            sed -i '/^ExecStart=/s/$/ --no-chroot --no-drop-root/' avahi-daemon/avahi-daemon.service
        fi

        if [[ "$ASAN_UBSAN" == true ]]; then
            sed -i '/^ExecStart=/s/$/ --no-drop-root --no-proc-title/' avahi-daemon/avahi-daemon.service
            sed -i "/^\[Service\]/aEnvironment=ASAN_OPTIONS=$ASAN_OPTIONS" avahi-daemon/avahi-daemon.service
            sed -i "/^\[Service\]/aEnvironment=UBSAN_OPTIONS=$UBSAN_OPTIONS" avahi-daemon/avahi-daemon.service
        fi

        # publish-workstation=yes triggers https://github.com/lathiat/avahi/issues/485
        # so it isn't set to yes here.
        sed -i '
            s/^#\(add-service-cookie=\).*/\1yes/;
            s/^\(publish-hinfo=\).*/\1yes/;
        ' avahi-daemon/avahi-daemon.conf

        printf "2001:db8::1 static-host-test.local\n" >>avahi-daemon/hosts

        sudo make install
        sudo ldconfig
        sudo adduser --system --group avahi
        sudo systemctl reload dbus
        sudo .github/workflows/smoke-tests.sh

        if [[ "$COVERAGE" == true ]]; then
            lcov --directory . --capture --initial --output-file coverage.info.initial
            lcov --directory . --capture --output-file coverage.info.run --no-checksum --rc lcov_branch_coverage=1
            lcov -a coverage.info.initial -a coverage.info.run --rc lcov_branch_coverage=1 -o coverage.info.raw
            lcov --extract coverage.info.raw "$(pwd)/*" --rc lcov_branch_coverage=1 --output-file coverage.info
            exit 0
        fi
        ;;
    *)
        printf '%s' "Unknown command '$1'" >&2
        exit 1
esac
