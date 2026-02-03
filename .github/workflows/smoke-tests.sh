#!/usr/bin/env bash

set -eux
set -o pipefail

export ASAN_OPTIONS=${ASAN_OPTIONS:-}
export UBSAN_OPTIONS=${UBSAN_OPTIONS:-}
export LD_PRELOAD=${LD_PRELOAD:-}
export NSS_MDNS_BUILD_DIR=
export ASAN_RT_PATH=

runstatedir=/run
if [[ "$OS" =~ (freebsd|netbsd|omnios) ]]; then
    runstatedir=/var/run
fi
sysconfdir=/etc
if [[ "$OS" == freebsd ]]; then
    sysconfdir=/usr/local/etc
elif [[ "$OS" == netbsd ]]; then
    sysconfdir=/usr/pkg/etc
fi
avahi_daemon_conf="$sysconfdir/avahi/avahi-daemon.conf"
avahi_daemon_runtime_dir="$runstatedir/avahi-daemon"
avahi_daemon_pid_file="$avahi_daemon_runtime_dir/pid"
avahi_socket="$avahi_daemon_runtime_dir/socket"
valgrind_log_file="/tmp/valgrind.avahi-daemon.%p"

dump_journal() {
    if [[ "$WITH_SYSTEMD" == false ]]; then
        cat /var/log/messages
    else
        journalctl --sync
        journalctl -b -u "avahi-*" --no-pager
    fi
}

run() {
    local cmd=()

    if [[ "$VALGRIND" == true && "$1" =~ avahi ]]; then
        if [[ "$1" =~ ^\. ]]; then
            cmd+=("libtool" "--mode=execute")
        fi
        cmd+=("valgrind" "--track-fds=yes" "--track-origins=yes" "--exit-on-first-error=yes" "--error-exitcode=1")

        # https://github.com/avahi/avahi/issues/761
        if ! [[ "$1" =~ avahi-daemon ]]; then
            cmd+=("--leak-check=full")
        fi
    fi
    cmd+=("$@")

    if ! "${cmd[@]}"; then
        dump_journal
        exit 1
    fi
}

should_fail() {
    if "$@"; then
        dump_journal
        exit 1
    fi
}

dbus_call() {
    local method="$1"

    shift
    gdbus call --system --dest org.freedesktop.Avahi --object-path / --method "org.freedesktop.Avahi.Server.$method" -- "$@"
    gdbus call --system --dest org.freedesktop.Avahi --object-path / --method "org.freedesktop.Avahi.Server2.$method" -- "$@"
}

skip_nss_mdns() {
    if [[ "$NSS_MDNS" != true ]]; then
        return 0
    fi

    # -shared-libasan doesn't work on FreeBSD. It bails out with
    # AddressSanitizer: CHECK failed: asan_posix.cpp:121 "((0)) == ((pthread_key_create(&tsd_key, destructor)))" (0x0, 0x4e) (tid=100146)
    if [[ "$OS" == freebsd && "$ASAN_UBSAN" == true ]]; then
        return 0
    fi

    return 1
}

install_nss_mdns() {
    local _cflags="$CFLAGS" _cxxflags="$CXXFLAGS" _asan_options="$ASAN_OPTIONS" _ld_preload="$LD_PRELOAD"

    should_fail ./avahi-client/check-nss-test

    if skip_nss_mdns; then
        return
    fi

    if [[ "$DISTCHECK" == true ]]; then
        NSS_MDNS_BUILD_DIR=$(mktemp -d)
    else
        NSS_MDNS_BUILD_DIR="$(pwd)/nss-mdns"
    fi

    git clone https://github.com/avahi/nss-mdns "$NSS_MDNS_BUILD_DIR"
    pushd "$NSS_MDNS_BUILD_DIR"
    autoreconf -ivf
    configure_args=("--enable-tests")
    if [[ "$OS" == freebsd ]]; then
        configure_args+=(
            "--prefix=/usr/local"
            "--runstatedir=/var/run"
        )
    else
        configure_args+=(
            "--prefix=/usr"
            "--libdir=/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
            "--runstatedir=/run"
        )
    fi

    if [[ "$ASAN_UBSAN" == true ]]; then
        if [[ "$CC" == clang ]]; then
            # make check fails under UBSan
            # https://github.com/avahi/nss-mdns/issues/102
            # -shared-libasan is needed to get the nss modules to work
            CFLAGS+=" -fno-sanitize=pointer-overflow -shared-libasan"
            CXXFLAGS+=" -fno-sanitize=pointer-overflow -shared-libasan"

            ASAN_RT_PATH=$($CC --print-file-name="libclang_rt.asan-$($CC -dumpmachine | cut -d - -f 1).so")
            LD_PRELOAD="$ASAN_RT_PATH"
        fi
        ASAN_OPTIONS="detect_leaks=0"
    fi

    if ! ./configure "${configure_args[@]}"; then
        cat config.log
        exit 1
    fi

    $MAKE -j"$(nproc)" V=1

    if ! $MAKE check V=1; then
        cat test-suite.log
        exit 1
    fi

    if [[ "$DISTCHECK" == true ]]; then
        $MAKE distcheck V=1
    fi

    if [[ "$ASAN_UBSAN" == true && "$CC" == gcc ]]; then
        ASAN_RT_PATH=$(ldd "$NSS_MDNS_BUILD_DIR/check_util" | grep libasan.so | cut -d " " -f 3)
    fi

    $MAKE install
    popd

    chmod -R a+r "$NSS_MDNS_BUILD_DIR"
    CFLAGS="$_cflags" CXXFLAGS="$_cxxflags" ASAN_OPTIONS="$_asan_options" LD_PRELOAD="$_ld_preload"

    run ./avahi-client/check-nss-test
}

run_nss_tests() {
    local _asan_options="$ASAN_OPTIONS" _ld_preload="$LD_PRELOAD"

    if LD_PRELOAD="" avahi-daemon -c; then
        if skip_nss_mdns; then
            dbus_call IsNSSSupportAvailable | grep -F "(false,)"
        else
            dbus_call IsNSSSupportAvailable | grep -F "(true,)"
        fi
    fi

    if skip_nss_mdns; then
        return
    fi

    if [[ "$ASAN_UBSAN" == true ]]; then
        LD_PRELOAD="$ASAN_RT_PATH"
        ASAN_OPTIONS=detect_leaks=0
    fi

    CK_FORK=no run "$NSS_MDNS_BUILD_DIR/check_util"

    sed -i.bak '/^hosts/s/files/& mdns_minimal/' /etc/nsswitch.conf
    cat /etc/nsswitch.conf

    for h in ipv4.local ipv6.local ipv46.local; do
        run "$NSS_MDNS_BUILD_DIR/avahi-test" "$h"

        run "$NSS_MDNS_BUILD_DIR/nss-test" "$h"

        if LD_PRELOAD="" avahi-daemon -c; then
            run getent hosts "$h"
        else
            should_fail getent hosts "$h"
        fi
    done

    sed -i.bak 's/ mdns_minimal//' /etc/nsswitch.conf
    cat /etc/nsswitch.conf
    ASAN_OPTIONS="$_asan_options" LD_PRELOAD="$_ld_preload"
}

check_rlimit_nofile() {
    local _pid _rlimit_nofile _rlimit_nofile_conf

    _rlimit_nofile_conf=$(perl -lne 'print $1 if /^rlimit-nofile=(\d+)/' "$avahi_daemon_conf")
    if [[ -z "$_rlimit_nofile_conf" ]]; then
        return 0
    fi

    _pid=$(cat "$avahi_daemon_pid_file")
    if [[ "$OS" == freebsd ]]; then
        _rlimit_nofile=$(perl -lne 'print $1 if /^nofile +(\d+)/' "/proc/$_pid/rlimit")
    else
        _rlimit_nofile=$(perl -lne 'print $1 if /^Max open files +(\d+)/' "/proc/$_pid/limits")
    fi
    [[ "$_rlimit_nofile" == "$_rlimit_nofile_conf" ]]
}

install_nss_mdns

for p in avahi-{browse,daemon,publish,resolve,set-host-name}; do
    run "$p" -h
    run "$p" -V
done

run_nss_tests

if [[ "$WITH_SYSTEMD" == false ]]; then
    if [[ "$VALGRIND" == true ]]; then
        valgrind --log-file="$valgrind_log_file" --leak-check=full --track-origins=yes --track-fds=yes --error-exitcode=1 --trace-children=yes \
            -s --suppressions=.github/workflows/avahi-daemon.supp \
            avahi-daemon -D --debug --no-drop-root
    elif [[ "$ASAN_UBSAN" == true ]]; then
        ASAN_OPTIONS="$ASAN_OPTIONS:log_path=/tmp/asan.avahi-daemon" \
        UBSAN_OPTIONS="$UBSAN_OPTIONS:log_path=/tmp/ubsan.avahi-daemon" \
            avahi-daemon -D --debug --no-drop-root
    else
        avahi-daemon -D --debug
    fi
    avahi-dnsconfd -D
else
    run systemd-analyze verify --recursive-errors=yes avahi-daemon.service
    run systemctl start avahi-daemon

    run systemd-analyze verify --recursive-errors=yes avahi-dnsconfd.service
    run systemctl start avahi-dnsconfd
fi

run ./avahi-client/client-test
(cd avahi-daemon && run ./ini-file-parser-test)

if [[ ! "$OS" =~ (alpine|omnios) ]]; then
    run ./avahi-compat-howl/address-test
fi

run ./avahi-compat-libdns_sd/null-test

run ./avahi-core/avahi-test
run ./avahi-core/querier-test

for test_case in self_loop retransmit_cname one_normal one_loop two_normal two_loop two_loop_inner two_loop_inner2 three_normal three_loop diamond cname_answer_diamond cname_answer; do
    run ./avahi-core/cname-test $test_case
done

run ./examples/glib-integration

if [[ ! "$OS" =~ (freebsd|netbsd|omnios) ]]; then
    run ./tests/c-plus-plus-test
fi

run_nss_tests

# make sure avahi picks up services it's notified about
cat <<'EOL' >"$sysconfdir/avahi/services/test-notifications.service"
<service-group>
  <name>test-notifications</name>
  <service>
    <type>_qotd._tcp</type>
    <port>1</port>
  </service>
</service-group>
EOL
drill -p5353 @127.0.0.1 test-notifications._qotd._tcp.local ANY

check_rlimit_nofile

if [[ "$WITH_SYSTEMD" == false ]]; then
    run avahi-dnsconfd --kill

    pid=$(cat "$avahi_daemon_pid_file")
    run avahi-daemon --kill

    if [[ "$VALGRIND" == true ]]; then
        cat "${valgrind_log_file//%p/$pid}"
        if grep -qE 'ERROR SUMMARY:\s+[^0]' "${valgrind_log_file//%p/$pid}"; then
            exit 1
        fi
    fi

    exit 0
fi

systemd-run -u avahi-test-rr-test ./avahi-client/rr-test

run avahi-dnsconfd -h
run avahi-dnsconfd -c
run avahi-dnsconfd -r

run avahi-daemon -h
run avahi-daemon -c
run avahi-daemon -r

run avahi-browse -b
systemd-run -u avahi-test-browse-vd avahi-browse -vD
systemd-run -u avahi-test-browse-varp avahi-browse -varp
systemd-run -u avahi-test-browse-varpf avahi-browse -varpf
systemd-run -u avahi-test-browse-var avahi-browse -var
systemd-run -u avahi-test-publish-vs avahi-publish -vs test _qotd._tcp 1234 a=1 b
systemd-run -u avahi-test-publish-subtype avahi-publish -fs --subtype _beep._sub._qotd._tcp BOOP _qotd._tcp 1234
systemd-run -u avahi-test-publish-vsfh avahi-publish -vsf -H X.sub.local test-vsfh _http._tcp 1

# https://github.com/avahi/avahi/issues/455
# The idea is to produce a lot of arguments by splitting the output
# of the perl one-liner so it shouldn't be quoted.
# shellcheck disable=SC2046
run avahi-publish -s T _qotd._tcp 22 $(perl -le 'print "A " x 100000')

h="$(hostname).local"
ipv4addr=$(avahi-resolve -v -4 -n "$h" | perl -alpe '$_ = $F[1]')
ipv6addr=$(avahi-resolve -v -6 -n "$h" | perl -alpe '$_ = $F[1]')

run avahi-resolve -v -a "$ipv4addr"
run avahi-resolve -v -a "$ipv6addr"

systemd-run -u avahi-test-publish-address avahi-publish -fvaR  "$h" "$ipv4addr"

# ::1 isn't here due to https://github.com/avahi/avahi/issues/574
for s in 127.0.0.1 224.0.0.251 ff02::fb; do
   drill -p5353 "@$s" "$h" ANY
   drill -p5353 "@$s" "_services._dns-sd._udp.local" ANY
done

l=[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[
run drill -p5353 @224.0.0.251 "$l.local"
run drill -p5353 -x @224.0.0.251 192.0.2.3
run drill -p5353 @224.0.0.251 "$l._qotd._tcp.local" ANY
run avahi-resolve -n "$l.local"
run avahi-resolve -a 192.0.2.3
dbus_call ResolveService -1 -1 "$l" _qotd._tcp local -1 0
printf "RESOLVE-HOSTNAME %s.local\n" "$l" | socat - unix-connect:"$avahi_socket"
printf "RESOLVE-ADDRESS 192.0.2.3\n" | socat - unix-connect:"$avahi_socket"
systemd-run -u avahi-test-publish-long-label avahi-publish -s -H "$l.local" "$l" _qotd._udp 1

dbus_call ResolveAddress -1 -1 "$ipv4addr" 0
should_fail dbus_call ResolveAddress -1 -1 1.1.1.1 2

dbus_call ResolveHostName -1 -1 "$(hostname).local" -1 0
should_fail dbus_call ResolveHostName -1 -1 one.one.one.one -1 2

dbus_call ResolveService -1 -1 "$(hostname)" _ssh._tcp local -1 0
should_fail dbus_call ResolveService -1 -1 "$(hostname)" _ssh._tcp dns-sd.org -1 2

# https://github.com/avahi/avahi/issues/491
dbus_call GetAlternativeHostName "a-2147483647"

# https://github.com/avahi/avahi/issues/526
dbus_call GetAlternativeServiceName "a #2147483647"

printf "%s\n" "RESOLVE-ADDRESS $(perl -e 'print q/A/ x 1014')" | socat - unix-connect:"$avahi_socket"

if ! grep -q '^enable-wide-area=yes' "$avahi_daemon_conf"; then
    dbus_call ResolveAddress -1 -1 "$ipv4addr" 0
    should_fail dbus_call ResolveAddress -1 -1 "$ipv4addr" 1
    run avahi-daemon -c
    dbus_call ResolveAddress -1 -1 "$ipv4addr" 2
    should_fail dbus_call ResolveAddress -1 -1 "$ipv4addr" 3

    dbus_call ResolveHostName -1 -1 "$(hostname).local" -1 0
    should_fail dbus_call ResolveHostName -1 -1 "$(hostname).local" -1 1
    run avahi-daemon -c
    dbus_call ResolveHostName -1 -1 "$(hostname).local" -1 2
    should_fail dbus_call ResolveHostName -1 -1 "$(hostname).local" -1 3

    dbus_call ResolveService -1 -1 "$(hostname)" _ssh._tcp local -1 0
    should_fail dbus_call ResolveService -1 -1 "$(hostname)" _ssh._tcp local -1 1
    run avahi-daemon -c
    dbus_call ResolveService -1 -1 "$(hostname)" _ssh._tcp local -1 2
    should_fail dbus_call ResolveService -1 -1 "$(hostname)" _ssh._tcp local -1 3
fi

cmds=(
    "HELP"
    "RESOLVE-HOSTNAME $h"
    "RESOLVE-HOSTNAME-IPV6 $h"
    "RESOLVE-HOSTNAME-IPV4 $h"
    "RESOLVE-ADDRESS $ipv4addr"
    "RESOLVE-ADDRESS $ipv6addr"
    "BROWSE-DNS-SERVERS"
    "BROWSE-DNS-SERVERS-IPV4"
    "BROWSE-DNS-SERVERS-IPV6"
)

mkdir -p CORPUS
for cmd in "${cmds[@]}"; do
    printf "%s\n" "$cmd" >CORPUS/"$cmd"
    printf "%s\n" "$cmd" | socat - unix-connect:"$avahi_socket"
done

timeout --foreground 180 bash -c 'while :; do
    radamsa -r CORPUS | socat -T2 - unix-connect:'"$avahi_socket"'
done >&/dev/null' || true

run avahi-browse -varpt
run avahi-browse -varpc
[[ -n "$(avahi-browse -vrpc _beep._sub._qotd._tcp)" ]]

# sleep is used here to let the HINFO record browser see
# the ADD/REMOVE events. If the hostname changes too fast
# the ADD events coalesce and the last one wins.
run avahi-set-host-name -v "ecstasy"
sleep 2

run avahi-set-host-name -v 'A\.B'
run avahi-set-host-name -v "$(perl -e 'print(q/[/x63)')"
run avahi-set-host-name -v "$(hostname)-new"

run dfuzzer -v -n org.freedesktop.Avahi

run systemctl kill --signal HUP avahi-daemon
run systemctl kill --signal USR1 avahi-daemon
run systemctl reload avahi-daemon

[[ -n "$(systemctl show --property StatusText avahi-daemon --value)" ]]

run systemctl stop avahi-dnsconfd
# https://github.com/avahi/avahi/issues/563
# should_fail systemctl is-failed avahi-dnsconfd

run systemctl stop avahi-daemon
should_fail systemctl is-failed avahi-daemon

should_fail avahi-daemon -c
should_fail avahi-dnsconfd -c

run systemctl stop "avahi-test-*"
