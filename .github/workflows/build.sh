#!/usr/bin/env bash

set -eux
set -o pipefail

export ASAN_UBSAN=${ASAN_UBSAN:-false}
export BUILD_ONLY=${BUILD_ONLY:-false}
export CFLAGS=${CFLAGS:-}
export COVERAGE=${COVERAGE:-false}
export DISTCHECK=${DISTCHECK:-false}
export VALGRIND=${VALGRIND:-false}

look_for_asan_ubsan_reports() {
    journalctl --sync
    set +o pipefail
    pids="$(
        journalctl -b -u 'avahi-*' --grep 'SUMMARY: .*Sanitizer:' |
        sed -r -n 's/.* .+\[([0-9]+)\]: SUMMARY:.*/\1/p'
    )"
    set -o pipefail

    if [[ -n "$pids" ]]; then
        for pid in $pids; do
           journalctl -b _PID="$pid" --no-pager
        done
        return 1
    fi
}

case "$1" in
    install-build-deps)
        sed -i 's/^\(Types: deb\)$/\1 deb-src/' /etc/apt/sources.list.d/ubuntu.sources
        apt-get update -y
        apt-get build-dep -y avahi
        apt-get install -y libevent-dev qtbase5-dev libsystemd-dev
        apt-get install -y gcc clang lcov
        apt-get install -y mono-mcs monodoc-base libmono-posix4.0-ci

        apt-get install -y valgrind ncat ldnsutils

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

                # https://github.com/avahi/avahi/issues/584
                CFLAGS+=' -fno-sanitize=function'
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

        if [[ $(uname -s) != FreeBSD ]]; then
            prefix="/usr"
            libdir="/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
            MAKE="make"
            sed_i_arg="-i"
        else
            prefix="/usr/local"
            libdir="$prefix/lib"
            MAKE="gmake"
            sed_i_arg='-i ""'
        fi

        ./bootstrap.sh \
            --enable-compat-howl \
            --enable-compat-libdns_sd \
            --enable-core-docs \
            --enable-tests \
            --libdir="$libdir" \
            --localstatedir=/var \
            --prefix=$prefix \
            --runstatedir=/run \
            --sysconfdir=/etc

        $MAKE -j"$(nproc)" V=1

        if [[ "$BUILD_ONLY" == true ]]; then
            exit 0
        fi

        if [[ "$DISTCHECK" == true ]]; then
            $MAKE distcheck
        fi

        $MAKE check VERBOSE=1

        if [[ $(uname -s) != FreeBSD ]]; then
            sed -i '/^ExecStart=/s/$/ --debug /' avahi-daemon/avahi-daemon.service
        fi

        # avahi-dnsconfd is used to test the DNS server browser only.
        # It shouldn't actually change any settings so the action just
        # logs what it receives from avahi-daemon.
        cat <<'EOL' >avahi-dnsconfd/avahi-dnsconfd.action
#!/bin/bash

printf "%s\n" "<$1> <$2> <$3> <$4>" | systemd-cat
EOL

        if [[ "$VALGRIND" == true ]]; then
            sed -i '
                /^ExecStart/s/=/=valgrind --leak-check=full --track-origins=yes --track-fds=yes --error-exitcode=1 /
            ' avahi-daemon/avahi-daemon.service
            sed -i '/^ExecStart=/s/$/ --no-chroot --no-drop-root --no-proc-title/' avahi-daemon/avahi-daemon.service

            sed -i '
                /^ExecStart/s/=/=valgrind --leak-check=full --track-origins=yes --track-fds=yes --error-exitcode=1 /
            ' avahi-dnsconfd/avahi-dnsconfd.service
        fi

        if [[ "$COVERAGE" == true ]]; then
            sed -i '/^ExecStart=/s/$/ --no-chroot --no-drop-root/' avahi-daemon/avahi-daemon.service
        fi

        if [[ "$ASAN_UBSAN" == true ]]; then
            sed -i '/^ExecStart=/s/$/ --no-drop-root --no-proc-title/' avahi-daemon/avahi-daemon.service
            sed -i "/^\[Service\]/aEnvironment=ASAN_OPTIONS=$ASAN_OPTIONS" avahi-daemon/avahi-daemon.service
            sed -i "/^\[Service\]/aEnvironment=UBSAN_OPTIONS=$UBSAN_OPTIONS" avahi-daemon/avahi-daemon.service

            sed -i "/^\[Service\]/aEnvironment=ASAN_OPTIONS=$ASAN_OPTIONS" avahi-dnsconfd/avahi-dnsconfd.service
            sed -i "/^\[Service\]/aEnvironment=UBSAN_OPTIONS=$UBSAN_OPTIONS" avahi-dnsconfd/avahi-dnsconfd.service
        fi

        # publish-workstation=yes triggers https://github.com/avahi/avahi/issues/485
        # so it isn't set to yes here.
        sed $sed_i_arg '
            s/^#\(add-service-cookie=\).*/\1yes/;
            s/^#\(publish-dns-servers=\)/\1/;
            s/^#\(publish-resolv-conf-dns-servers=\).*/\1yes/;
            s/^\(publish-hinfo=\).*/\1yes/;
        ' avahi-daemon/avahi-daemon.conf

        printf "2001:db8::1 static-host-test.local\n" >>avahi-daemon/hosts

        $MAKE install
        ldconfig

        if [[ $(uname -s) != FreeBSD ]]; then
            adduser --system --group avahi
            systemctl reload dbus
        else
            service dbus onerestart
        fi

        if [[ $(uname -s) != FreeBSD ]]; then
            if ! .github/workflows/smoke-tests.sh; then
                look_for_asan_ubsan_reports
                exit 1
            fi
        fi

        if [[ "$COVERAGE" == true ]]; then
            lcov --ignore-errors source --directory . --capture --initial --output-file coverage.info.initial
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
