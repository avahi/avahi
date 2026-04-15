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

    if [[ "$VALGRIND" == true && "$1" =~ (avahi|ini-file-parser-test) ]]; then
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

    if [[ "$WITH_DBUS" == true ]]; then
        should_fail ./avahi-client/check-nss-test
    fi

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

    $MAKE check VERBOSE=1 CK_FORK=no

    if [[ "$DISTCHECK" == true ]]; then
        $MAKE distcheck V=1
    fi

    if [[ "$ASAN_UBSAN" == true && "$CC" == gcc ]]; then
        ASAN_RT_PATH=$(ldd "$NSS_MDNS_BUILD_DIR/check_util" | grep libasan.so | cut -d " " -f 3)
    fi

    $MAKE install
    popd

    CFLAGS="$_cflags" CXXFLAGS="$_cxxflags" ASAN_OPTIONS="$_asan_options" LD_PRELOAD="$_ld_preload"

    if [[ "$WITH_DBUS" == true ]]; then
        run ./avahi-client/check-nss-test
    fi
}

run_nss_tests() {
    local _asan_options="$ASAN_OPTIONS" _ld_preload="$LD_PRELOAD"

    if [[ "$WITH_DBUS" == true ]] && LD_PRELOAD="" avahi-daemon -c; then
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

# CVE-2025-59529: Test simple protocol client connection limits.
# Restarts the daemon with rlimit-nofile=100 so the limits are
# reachable with a small number of connections:
#   max_clients = min(4096, 100 * 3/4) = 75
#   max_uid_clients = 75 / 2 = 37
test_simple_protocol_limits() {
    local _i _refused _pid
    # _saved_rlimit and _pids are intentionally NOT local so that
    # _simple_protocol_limits_cleanup can access them from the EXIT
    # trap even if set -e kills the script outside this function.
    _saved_rlimit=""
    _pids=()

    _saved_rlimit=$(perl -lne 'print $1 if /^(#?rlimit-nofile=.*)/' "$avahi_daemon_conf")
    sed -i.bak 's|^#*rlimit-nofile=.*|rlimit-nofile=100|' "$avahi_daemon_conf"

    trap '_simple_protocol_limits_cleanup' EXIT

    if [[ "$WITH_SYSTEMD" == true ]]; then
        # systemd's LimitNOFILE overrides the daemon config
        mkdir -p /run/systemd/system/avahi-daemon.service.d
        printf '[Service]\nLimitNOFILE=100\n' \
            > /run/systemd/system/avahi-daemon.service.d/test-rlimit.conf
        systemctl daemon-reload
        run systemctl restart avahi-daemon
    elif [[ "$VALGRIND" == true ]]; then
        avahi-daemon --kill 2>/dev/null || true
        sleep 1
        # Set RLIMIT_NOFILE before Valgrind starts; Valgrind
        # intercepts setrlimit() so the daemon config alone
        # is not enough.
        (ulimit -n 100 && \
        valgrind --log-file="$valgrind_log_file" --leak-check=full \
            --track-origins=yes --track-fds=yes --error-exitcode=1 \
            --trace-children=yes \
            -s --suppressions=.github/workflows/avahi-daemon.supp \
            avahi-daemon -D --debug --no-drop-root --no-proc-title)
    elif [[ "$ASAN_UBSAN" == true ]]; then
        avahi-daemon --kill 2>/dev/null || true
        sleep 1
        ASAN_OPTIONS="$ASAN_OPTIONS:log_path=/tmp/asan.avahi-daemon" \
        UBSAN_OPTIONS="$UBSAN_OPTIONS:log_path=/tmp/ubsan.avahi-daemon" \
            avahi-daemon -D --debug --no-drop-root
    else
        avahi-daemon --kill 2>/dev/null || true
        sleep 1
        avahi-daemon -D --debug
    fi
    if [[ "$WITH_SYSTEMD" == false ]]; then
        avahi-dnsconfd -D 2>/dev/null || true
    fi
    sleep 2

    _pid=$(cat "$avahi_daemon_pid_file")
    kill -0 "$_pid"

    printf "%s\n" "HELP" | socat -t3 - "unix-connect:$avahi_socket,shut-none" \
        | grep -q "Available commands"

    # --- Per-UID limit ---
    # Open 40 connections as a non-root user. With max_uid_clients=37,
    # connections 38+ are refused for that UID.
    # Root (uid=0) is exempt from per-UID limits, so a privilege drop
    # is required. runuser is Linux-specific; skip on other platforms.
    if command -v runuser >/dev/null 2>&1; then
        # Take a baseline count so we only assert on new refusals
        # from this flood, not stale entries from prior test runs.
        local _baseline_uid
        _baseline_uid=$(_simple_protocol_limits_count "too many uid clients")

        _pids=()
        for _i in $(seq 1 40); do
            runuser -u avahi -- \
                socat -T30 PIPE "unix-connect:$avahi_socket,shut-none" &
            _pids+=("$!")
        done
        # socat connect() is synchronous and the daemon processes
        # accept() in its event loop immediately; 3s is sufficient.
        sleep 3

        kill -0 "$_pid"

        _refused=$(( $(_simple_protocol_limits_count "too many uid clients") - _baseline_uid ))
        if [[ "$_refused" -le 0 ]]; then
            dump_journal || true
            exit 1
        fi

        for _i in "${_pids[@]}"; do
            kill "$_i" 2>/dev/null || true
        done
        _pids=()
        # Allow the daemon to process disconnections and free slots
        sleep 2

        printf "%s\n" "HELP" \
            | socat -t3 - "unix-connect:$avahi_socket,shut-none" \
            | grep -q "Available commands"
    fi

    # --- Total client limit ---
    # Open 80 connections as root. Root bypasses per-UID limits but
    # the global max_clients=75 still applies.
    local _baseline_total
    _baseline_total=$(_simple_protocol_limits_count "too many clients$")

    _pids=()
    for _i in $(seq 1 80); do
        socat -T30 PIPE "unix-connect:$avahi_socket,shut-none" &
        _pids+=("$!")
    done
    sleep 3

    kill -0 "$_pid"

    # Verify refusal was logged. The trailing $ anchor distinguishes
    # "too many clients" (global) from "too many uid clients: N" (per-UID).
    # On non-systemd BSD, debug messages may not reach syslog depending
    # on the syslog.conf, so only assert the count on systemd or Linux.
    _refused=$(( $(_simple_protocol_limits_count "too many clients$") - _baseline_total ))
    if [[ "$WITH_SYSTEMD" == true || "$OS" == ubuntu ]] && [[ "$_refused" -le 0 ]]; then
        dump_journal || true
        exit 1
    fi

    for _i in "${_pids[@]}"; do
        kill "$_i" 2>/dev/null || true
    done
    _pids=()
    sleep 2

    # Daemon accepts connections after both floods
    printf "%s\n" "HELP" | socat -t3 - "unix-connect:$avahi_socket,shut-none" \
        | grep -q "Available commands"

    # Cleanup runs via the EXIT trap; clear it on success so it
    # does not interfere with the rest of the script.
    _simple_protocol_limits_cleanup
    trap - EXIT

    # Restart daemon with original config for remaining tests
    if [[ "$WITH_SYSTEMD" == true ]]; then
        run systemctl restart avahi-daemon
    elif [[ "$VALGRIND" == true ]]; then
        avahi-daemon --kill 2>/dev/null || true
        sleep 1
        valgrind --log-file="$valgrind_log_file" --leak-check=full \
            --track-origins=yes --track-fds=yes --error-exitcode=1 \
            --trace-children=yes \
            -s --suppressions=.github/workflows/avahi-daemon.supp \
            avahi-daemon -D --debug --no-drop-root --no-proc-title
    elif [[ "$ASAN_UBSAN" == true ]]; then
        avahi-daemon --kill 2>/dev/null || true
        sleep 1
        ASAN_OPTIONS="$ASAN_OPTIONS:log_path=/tmp/asan.avahi-daemon" \
        UBSAN_OPTIONS="$UBSAN_OPTIONS:log_path=/tmp/ubsan.avahi-daemon" \
            avahi-daemon -D --debug --no-drop-root
    else
        avahi-daemon --kill 2>/dev/null || true
        sleep 1
        avahi-daemon -D --debug
    fi
    if [[ "$WITH_SYSTEMD" == false ]]; then
        avahi-dnsconfd -D 2>/dev/null || true
    fi
    sleep 2
}

# Helpers for test_simple_protocol_limits. Defined at top level
# because nested functions have no precedent in this file.

_simple_protocol_limits_cleanup() {
    for _i in "${_pids[@]}"; do
        kill "$_i" 2>/dev/null || true
    done
    _pids=()

    if [[ -n "${_saved_rlimit:-}" ]]; then
        sed -i.bak "s|^rlimit-nofile=100|$_saved_rlimit|" "$avahi_daemon_conf"
    else
        sed -i.bak '/^rlimit-nofile=100/d' "$avahi_daemon_conf"
    fi
    rm -f "${avahi_daemon_conf}.bak"

    rm -f /run/systemd/system/avahi-daemon.service.d/test-rlimit.conf 2>/dev/null
    systemctl daemon-reload 2>/dev/null || true
}

_simple_protocol_limits_count() {
    local _pattern="$1" _count=0

    if [[ "$WITH_SYSTEMD" == true ]]; then
        journalctl --sync
        _count=$(journalctl -b -u avahi-daemon --no-pager \
            | grep -c "$_pattern") || _count=0
    elif [[ -f /var/log/syslog ]]; then
        _count=$(grep -c "$_pattern" /var/log/syslog) || _count=0
    elif [[ -f /var/log/messages ]]; then
        _count=$(grep -c "$_pattern" /var/log/messages) || _count=0
    elif [[ -f /var/adm/messages ]]; then
        _count=$(grep -c "$_pattern" /var/adm/messages) || _count=0
    fi

    printf "%d" "$_count"
}



install_nss_mdns

run avahi-daemon -h
run avahi-daemon -V

if [[ "$WITH_DBUS" == true ]]; then
    for p in avahi-{browse,publish,resolve,set-host-name}; do
        run "$p" -h
        run "$p" -V
    done
fi

run_nss_tests

if [[ "$WITH_SYSTEMD" == false ]]; then
    if [[ "$VALGRIND" == true ]]; then
        valgrind --log-file="$valgrind_log_file" --leak-check=full --track-origins=yes --track-fds=yes --error-exitcode=1 --trace-children=yes \
            -s --suppressions=.github/workflows/avahi-daemon.supp \
            avahi-daemon -D --debug --no-drop-root --no-proc-title
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

(cd avahi-daemon && run ./ini-file-parser-test)

if [[ "$WITH_DBUS" == true ]]; then
    run ./avahi-client/client-test

    if [[ ! "$OS" =~ (alpine|omnios) ]]; then
        run ./avahi-compat-howl/address-test
    fi

    if [[ "$OS" != freebsd || "$VALGRIND" != true ]]; then
        run ./avahi-compat-libdns_sd/null-test
    fi

    run ./examples/glib-integration

    if [[ ! "$OS" =~ (freebsd|netbsd|omnios) ]]; then
        run ./tests/c-plus-plus-test
    fi
fi

run ./avahi-core/avahi-test
run ./avahi-core/querier-test

for test_case in self_loop retransmit_cname one_normal one_loop two_normal two_loop two_loop_inner two_loop_inner2 three_normal three_loop diamond cname_answer_diamond cname_answer; do
    run ./avahi-core/cname-test $test_case
done

run_nss_tests

# make sure avahi picks up services it's notified about
cat <<'EOL' >"$sysconfdir/avahi/services/test-notifications.service"
<service-group>
  <name>test-notifications</name>
  <service protocol="any">
    <domain-name>local</domain-name>
    <host-name>ipv46.local</host-name>
    <port>4321</port>
    <subtype>_test._sub._qotd._tcp</subtype>
    <txt-record>k1=v1</txt-record>
    <txt-record value-format="binary-hex">k2=7632</txt-record>
    <txt-record value-format="binary-base64">k3=djM=</txt-record>
    <type>_qotd._tcp</type>
  </service>
</service-group>
EOL
drill -p5353 @127.0.0.1 test-notifications._qotd._tcp.local ANY
drill -p5353 @127.0.0.1 test-notifications._qotd._tcp.local SRV | grep -F '4321 ipv46.local'
drill -p5353 @127.0.0.1 test-notifications._qotd._tcp.local TXT | grep -F '"k1=v1" "k2=v2" "k3=v3"'
drill -p5353 @127.0.0.1 _test._sub._qotd._tcp.local PTR | grep -F test-notifications._qotd._tcp.local

if [[ "$WITH_DBUS" == true ]]; then
    avahi-browse -arpt | grep -F 'test-notifications;_qotd._tcp;local;ipv46.local;192.0.2.2;4321;"k1=v1" "k2=v2" "k3=v3"'
    gdbus call --system --dest org.freedesktop.Avahi --object-path / --method org.freedesktop.Avahi.Server2.ResolveService \
        -- -1 0 test-notifications _qotd._tcp local -1 0 |
        grep -F '[byte 0x6b, 0x31, 0x3d, 0x76, 0x31], [0x6b, 0x32, 0x3d, 0x76, 0x32], [0x6b, 0x33, 0x3d, 0x76, 0x33]'
fi

check_rlimit_nofile

test_simple_protocol_limits

l=$(hostname | sed 's/\.local$//')
h="$l.local"
run drill -p5353 @127.0.0.1 "$h" HINFO
run drill -p5353 @127.0.0.1 _domain._udp.local ANY
drill -Q -p5353 @127.0.0.1 "$l._ssh._tcp.local" TXT | grep org.freedesktop.Avahi.cookie

if [[ "$WITH_SYSTEMD" == false ]]; then
    avahi-dnsconfd --kill 2>/dev/null || true

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
