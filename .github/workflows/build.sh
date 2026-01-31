#!/usr/bin/env bash

set -eux
set -o pipefail

export ASAN_UBSAN=${ASAN_UBSAN:-false}
export BUILD_ONLY=${BUILD_ONLY:-false}
export CFLAGS=${CFLAGS:-"-g -O0"}
export COVERAGE=${COVERAGE:-false}
export DISTCHECK=${DISTCHECK:-false}
export VALGRIND=${VALGRIND:-false}
export MAKE=make
export NSS_MDNS=true
export WITH_SYSTEMD=false
export OS=

if source /etc/os-release; then
    OS="$ID"
else
    OS=$(uname -s | tr "[:upper:]" "[:lower:]")
fi

if [[ "$OS" =~ (alpine|netbsd|omnios) ]]; then
    NSS_MDNS=false
fi

if [[ "$OS" =~ (freebsd|netbsd|omnios) ]]; then
    MAKE=gmake
fi

if [[ "$OS" == omnios ]]; then
    PATH="/opt/local/sbin:/opt/local/bin:$PATH"
fi

asan_ubsan_reports_detected() {
    local _btraces

    if [[ "$WITH_SYSTEMD" == false ]]; then
        _btraces=$(mktemp)
        find /tmp/ \( -name 'asan.avahi-daemon*' -or -name 'ubsan.avahi-daemon*' \) -exec cat {} \; >"$_btraces"
        if [[ -s "$_btraces" ]]; then
            cat "$_btraces"
            return 0
        else
            return 1
        fi
    fi

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
        return 0
    fi

    return 1
}

coredumps_detected() {
    if [[ "$WITH_SYSTEMD" == false ]]; then
        return 1
    fi

    if coredumpctl list --no-pager avahi-daemon; then
        coredumpctl debug --no-pager --debugger-arguments="-batch -ex 'bt full'" avahi-daemon
        return 0
    fi

    return 1
}

install_dfuzzer() {
    git clone https://github.com/dbus-fuzzer/dfuzzer
    (cd dfuzzer && meson setup build && ninja -C build install)
}

install_radamsa() {
    git clone https://gitlab.com/akihe/radamsa
    pushd radamsa
    # make is replaced with ${MAKE} to get around
    # make[1]: illegal argument to -j -- must be positive integer!
    sed -i.bak "s/make/${MAKE}/" Makefile
    # CFLAGS is reset because it fails under UBSan with:
    # bin/ol -O1 -o radamsa.c rad/main.scm
    #     ol.c:21585:26: runtime error: left shift of 16777215 by 8 places cannot be represented in type 'int'
    CFLAGS="" $MAKE -j"$(nproc)"
    $MAKE install
    popd
    # At least radamsa.c is generated with the "-rw-------." permissions so
    # cross-platform-actions/action trips on that with:
    #  /usr/bin/rsync -auz runner@cross_platform_actions_host:/home/runner/work/ /home/runner/work
    #   rsync: [sender] send_files failed to open "/home/runner/work/avahi/avahi/radamsa/radamsa.c": Permission denied (13)
    chmod -R a+r radamsa
}

trim_sandbox() {
    sed -i.bak 's/^\(Lock\|Memory\|NoNew\|Private\|Protect\|Restart\|Restrict\|SystemCall\)/#\1/' "$1"
}

case "$1" in
    install-build-deps)
        sed -i 's/^\(Types: deb\)$/\1 deb-src/' "/etc/apt/sources.list.d/$OS.sources"
        apt-get update -y
        apt-get build-dep -y avahi
        apt-get install -y libevent-dev qtbase5-dev libsystemd-dev systemd-dev
        apt-get install -y gcc clang lcov
        apt-get install -y mono-mcs monodoc-base libmono-posix4.0-ci

        apt-get install -y libtool-bin valgrind socat ldnsutils
        apt-get install -y gdb systemd-coredump

        apt-get install -y libglib2.0-dev meson curl

        apt-get install -y check
        install_dfuzzer
        install_radamsa
        ;;
    install-build-deps-freebsd)
        # Use latest package set
        mkdir -p /usr/local/etc/pkg/repos/
        cp /etc/pkg/FreeBSD.conf /usr/local/etc/pkg/repos/FreeBSD.conf
        sed -i.bak -e 's|/quarterly|/latest|' /usr/local/etc/pkg/repos/FreeBSD.conf

        pkg install -y gettext-runtime gettext-tools gmake intltool \
            gobject-introspection pkgconf expat libdaemon dbus-glib dbus gdbm \
            libevent glib automake libtool libinotify qt5-core qt5-buildtools \
            gtk3 py311-pygobject py311-dbus py311-gdbm mono git socat \
            valgrind dfuzzer check radamsa
        # some deps pull in avahi itself, remove it
        pkg remove -fy avahi-app
        ;;
    install-build-deps-Alpine)
        apk add autoconf automake clang coreutils dbus dbus-dev drill expat-dev gcc g++ \
            gdbm-dev gettext-dev git glib-dev gobject-introspection-dev gtk+3.0-dev \
            gzip libdaemon-dev libevent-dev libtool make meson mono-dev musl-dbg musl-dev \
            py3-dbus py3-gobject3-dev py3-setuptools python3-dev python3-gdbm \
            qt5-qtbase-dev socat tar valgrind xmltoman

        install_dfuzzer
        install_radamsa
        ;;
    install-build-deps-netbsd)
        PKG_PATH="https://cdn.NetBSD.org/pub/pkgsrc/packages/NetBSD/$(uname -p)/$(uname -r)/All/" \
        PKG_RCD_SCRIPTS=yes \
            pkg_add -u autoconf automake dbus drill expat gettext git glib gmake intltool libdaemon libtool \
            meson pkgconf socat
        install_dfuzzer
        install_radamsa
        ;;
    install-build-deps-omnios)
        # https://pkgsrc.smartos.org/install-on-illumos/
        BOOTSTRAP_TAR="bootstrap-trunk-x86_64-20240116.tar.gz"
        BOOTSTRAP_SHA="4d92a333587d9dcc669ff64264451ca65da701b7"

        curl -O "https://pkgsrc.smartos.org/packages/SmartOS/bootstrap/${BOOTSTRAP_TAR}"
        [[ "${BOOTSTRAP_SHA}" == "$(/bin/digest -a sha1 ${BOOTSTRAP_TAR})" ]]
        tar -zxpf "${BOOTSTRAP_TAR}" -C /

        pkg_add -u autoconf automake drill expat gettext git glib2 gmake intltool libdaemon libtool \
            meson pkgconf socat
        pkg install gcc14
        install_dfuzzer
        ;;
    build)
        if [[ "$OS" == freebsd ]]; then
            # Do what USES="localbase:ldflags" do in FreeBSD Ports, namely:
            # - Add /usr/local/include to the compiler's search path
            # - Add /usr/local/lib to the linker's search path
            export CFLAGS+=" -I/usr/local/include"
            export CPPFLAGS+=" -I/usr/local/include"
            export LDFLAGS+=" -L/usr/local/lib"
        elif [[ "$OS" == omnios ]]; then
            export CFLAGS+=" -I/opt/local/include"
            export CPPFLAGS+=" -I/opt/local/include"
            export LDFLAGS+=" -L/opt/local/lib"
        fi

        if [[ "$ASAN_UBSAN" == true ]]; then
            export CFLAGS+=" -U_FORTIFY_SOURCE -fsanitize=address,undefined -g -fno-omit-frame-pointer"
            export ASAN_OPTIONS=strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
            export UBSAN_OPTIONS=print_stacktrace=1:print_summary=1:halt_on_error=1

            # avahi fails to compile with clang and ASan/UBSan with
            #     configure: error: Missing POSIX Threads support
            # because acx_pthread is out of date. The kludge should be
            # removed once acx_pthread gets updated.
            if [[ "$CC" == clang ]]; then
                sed -i.bak 's/check_inconsistencies=yes/check_inconsistencies=no/' common/acx_pthread.m4

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

        autogen_args=(
            "--enable-compat-howl"
            "--enable-compat-libdns_sd"
            "--enable-core-docs"
            "--enable-tests"
            "--localstatedir=/var"
        )

        if [[ "$OS" == freebsd ]]; then
            autogen_args+=(
                "--prefix=/usr/local"
                "--libdir=/usr/local/lib"
                "--runstatedir=/var/run"
                "--sysconfdir=/usr/local/etc"
                "--disable-libsystemd"
                "--disable-manpages"
            )
        elif [[ "$OS" == alpine ]]; then
            autogen_args+=(
                "--prefix=/usr"
                "--runstatedir=/run"
                "--sysconfdir=/etc"
                "--disable-libsystemd"
                "--disable-compat-howl"
                "--with-distro=none"
            )
        elif [[ "$OS" == netbsd ]]; then
            autogen_args+=(
                "--prefix=/usr/pkg"
                "--runstatedir=/var/run"
                "--with-distro=none"
                "--enable-tests"
                "--disable-autoipd"
                "--disable-gdbm"
                "--disable-gobject"
                "--disable-gtk"
                "--disable-gtk3"
                "--disable-libevent"
                "--disable-libsystemd"
                "--disable-manpages"
                "--disable-mono"
                "--disable-python"
                "--disable-qt3"
                "--disable-qt4"
                "--disable-qt5"
            )
        elif [[ "$OS" == omnios ]]; then
            autogen_args+=(
                "--libdir=/usr/lib/64"
                "--prefix=/usr"
                "--runstatedir=/var/run"
                "--sysconfdir=/etc"
                "--with-distro=none"
                "--enable-tests"
                "--disable-autoipd"
                "--disable-compat-howl"
                "--disable-gdbm"
                "--disable-gobject"
                "--disable-gtk"
                "--disable-gtk3"
                "--disable-libevent"
                "--disable-libsystemd"
                "--disable-manpages"
                "--disable-mono"
                "--disable-python"
                "--disable-qt3"
                "--disable-qt4"
                "--disable-qt5"
            )
        else
            autogen_args+=(
                "--prefix=/usr"
                "--libdir=/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
                "--runstatedir=/run"
                "--sysconfdir=/etc"
            )
        fi

        if [[ "$OS" =~ (netbsd|omnios) ]]; then
            # On NetBSD and OmniOS autogen.sh fails with
            # config.status: error: cannot find input file: 'po/Makefile.in.in'
            # so autoreconf/configure is used until it's fixed one way or another
            autoreconf -ivf
            if ! ./configure "${autogen_args[@]}"; then
                cat config.log
                exit 1
            fi
        elif ! ./autogen.sh "${autogen_args[@]}"; then
            cat config.log
            exit 1
        fi

        if grep -q "^HAVE_SYSTEMD_TRUE=''" config.log; then
            WITH_SYSTEMD=true
        fi

        $MAKE -j"$(nproc)" V=1

        if [[ "$BUILD_ONLY" == true ]]; then
            exit 0
        fi

        if [[ "$DISTCHECK" == true ]]; then
            # Due to a build system bug DISTCHECK_CONFIGURE_FLAGS
            # changes the behavior of `make distcheck` even when
            # it's empty. To keep testing the most common use case
            # DISTCHECK_CONFIGURE_FLAGS isn't passed on Linux
            if [[ "$OS" == freebsd ]]; then
                $MAKE distcheck \
                    DISTCHECK_CONFIGURE_FLAGS="--disable-libsystemd --disable-manpages"
            elif [[ "$OS" == alpine ]]; then
                $MAKE distcheck \
                    DISTCHECK_CONFIGURE_FLAGS="--disable-libsystemd --with-distro=none"
            else
                $MAKE distcheck
            fi
        fi

        if [[ "$VALGRIND" == true ]]; then
            $MAKE check VERBOSE=1 LOG_COMPILER="valgrind --leak-check=full --track-origins=yes --track-fds=yes --error-exitcode=1"
        else
            $MAKE check VERBOSE=1
        fi

        # It's just a kludge using the existing valgrind target
        # to run at least something under Valgrind on FreeBSD. It was
        # enough to trigger a memory leak/conditional jumps/moves
        # depending on uninitialised values there though. It can be
        # removed when the smoke tests are run on FreeBSD.
        if [[ "$VALGRIND" == true ]]; then
            (cd avahi-core && $MAKE valgrind)
        fi

        if [[ "$WITH_SYSTEMD" == true ]]; then
            if [[ "$ASAN_UBSAN" == true || "$VALGRIND" == true || "$COVERAGE" == true ]]; then
                trim_sandbox avahi-daemon/avahi-daemon.service
            fi

            sed -i.bak '/^ExecStart=/s/$/ --debug /' avahi-daemon/avahi-daemon.service
        fi

        # avahi-dnsconfd is used to test the DNS server browser only.
        # It shouldn't actually change any settings so the action just
        # logs what it receives from avahi-daemon.
        cat <<'EOL' >avahi-dnsconfd/avahi-dnsconfd.action
#!/usr/bin/env bash

printf "%s\n" "<$1> <$2> <$3> <$4>" | logger
EOL

        if [[ "$VALGRIND" == true && "$WITH_SYSTEMD" == true ]]; then
            sed -i.bak "
                /^ExecStart/s!=!=valgrind -s --suppressions='$(pwd)/.github/workflows/avahi-daemon.supp' --leak-check=full --track-origins=yes --track-fds=yes --error-exitcode=1 !
            " avahi-daemon/avahi-daemon.service
            sed -i.bak '/^ExecStart=/s/$/ --no-chroot --no-drop-root --no-proc-title/' avahi-daemon/avahi-daemon.service

            sed -i.bak '
                /^ExecStart/s/=/=valgrind --leak-check=full --track-origins=yes --track-fds=yes --error-exitcode=1 /
            ' avahi-dnsconfd/avahi-dnsconfd.service
        fi

        if [[ "$COVERAGE" == true ]]; then
            sed -i '/^ExecStart=/s/$/ --no-chroot --no-drop-root/' avahi-daemon/avahi-daemon.service
        fi

        if [[ "$ASAN_UBSAN" == true && "$WITH_SYSTEMD" == true ]]; then
            sed -i.bak '/^ExecStart=/s/$/ --no-drop-root --no-proc-title/' avahi-daemon/avahi-daemon.service
            sed -i.bak "/^\[Service\]/aEnvironment=ASAN_OPTIONS=$ASAN_OPTIONS" avahi-daemon/avahi-daemon.service
            sed -i.bak "/^\[Service\]/aEnvironment=UBSAN_OPTIONS=$UBSAN_OPTIONS" avahi-daemon/avahi-daemon.service

            sed -i.bak "/^\[Service\]/aEnvironment=ASAN_OPTIONS=$ASAN_OPTIONS" avahi-dnsconfd/avahi-dnsconfd.service
            sed -i.bak "/^\[Service\]/aEnvironment=UBSAN_OPTIONS=$UBSAN_OPTIONS" avahi-dnsconfd/avahi-dnsconfd.service
        fi

        # publish-workstation=yes triggers https://github.com/avahi/avahi/issues/485
        # so it isn't set to yes here.
        sed -i.bak '
            s/^#\(add-service-cookie=\).*/\1yes/;
            s/^#\(publish-dns-servers=\)/\1/;
            s/^#\(publish-resolv-conf-dns-servers=\).*/\1yes/;
            s/^\(publish-hinfo=\).*/\1yes/;
        ' avahi-daemon/avahi-daemon.conf

        # Valgrind jobs are skipped here because Valgrind emulates limits for
        # file descriptors and prevents setrlimit from working
        # https://sourceware.org/git/?p=valgrind.git;a=blob;f=NEWS.older;h=6de0e84dacaa14f97a100a614c1bb2a9f242161c;hb=HEAD#l2590
        # The setrlimit system call now simply updates the emulated limits as best
        # as possible - the hard limit is not allowed to move at all and just
        # returns EPERM if you try and change it.
        # Ubuntu jobs are skipped here to test the default config there.
        if [[ "$OS" =~ (alpine|freebsd) && "$VALGRIND" != true ]]; then
            sed -i.bak '
                s/^#\(rlimit-nofile=\)/\1/;
            ' avahi-daemon/avahi-daemon.conf
        fi

        label=[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[
        cat <<EOL >>avahi-daemon/hosts
192.0.2.1 ipv4.local
2001:db8::1 ipv6.local
192.0.2.2 ipv46.local
2001:db8::2 ipv46.local
192.0.2.3 $label.local
EOL

        $MAKE install
        if [[ ! "$OS" =~ (netbsd|omnios) ]]; then
            ldconfig
        fi

        sysconfdir=/etc
        if [[ "$OS" == freebsd ]]; then
            sysconfdir=/usr/local/etc
        elif [[ "$OS" == netbsd ]]; then
            sysconfdir=/usr/pkg/etc
        fi

        cat <<EOL >"$sysconfdir/avahi/services/long-label.service"
<service-group>
  <name>$label</name>
  <service>
    <type>_qotd._tcp</type>
    <port>1</port>
  </service>
</service-group>
EOL

        # smoke tests require systemd, so don't run them on FreeBSD
        # https://github.com/avahi/avahi/issues/727
        if [[ "$OS" == alpine ]]; then
            addgroup -S avahi
            adduser -S -G avahi avahi
            syslogd
            mkdir /run/dbus
            dbus-daemon --system --fork
        elif [[ "$OS" == freebsd ]]; then
            mount -t procfs proc /proc
            hostname freebsd
            service dbus onerestart
        elif [[ "$OS" == netbsd ]]; then
            groupadd avahi
            useradd -m -g avahi avahi
            service dbus onerestart
        elif [[ "$OS" == omnios ]]; then
            groupadd avahi
            useradd -m -g avahi avahi
            svcadm restart dbus
        else
            adduser --system --group avahi
            systemctl reload dbus
        fi

        exit_code=0
        if ! .github/workflows/smoke-tests.sh; then
            ((++exit_code))
        fi

        if asan_ubsan_reports_detected; then
            ((++exit_code))
        fi

        if coredumps_detected; then
            ((++exit_code))
        fi

        if [[ "$exit_code" -ne 0 ]]; then
            exit "$exit_code"
        fi

        if [[ "$COVERAGE" == true ]]; then
            lcov --capture --directory . --initial --branch-coverage --ignore-errors source --output-file coverage.info.initial
            lcov --capture --directory . --branch-coverage --ignore-errors inconsistent --output-file coverage.info.run
            lcov --add-tracefile coverage.info.initial --add-tracefile coverage.info.run --branch-coverage --ignore-errors inconsistent --output-file coverage.info.raw
            lcov --extract coverage.info.raw "$(pwd)/*" --branch-coverage --ignore-errors inconsistent --output-file coverage.info
            lcov --summary coverage.info --fail-under-lines 50 --branch-coverage --ignore-errors inconsistent
            exit 0
        fi
        ;;
    *)
        printf '%s' "Unknown command '$1'" >&2
        exit 1
esac
