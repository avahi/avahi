#!/bin/bash

set -eux
set -o pipefail

look_for_asan_ubsan_reports() {
    journalctl --sync
    set +o pipefail
    pids="$(
        journalctl -b -u avahi-daemon --grep 'SUMMARY: .*Sanitizer:' |
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

run() {
    if ! "$@"; then
        journalctl --sync
        journalctl -b -u avahi-daemon --no-pager
        exit 1
    fi
}

run systemctl start avahi-daemon

./avahi-client/check-nss-test
./avahi-client/client-test
(cd avahi-daemon && ./ini-file-parser-test)
./avahi-compat-howl/address-test
./avahi-core/avahi-test
./examples/glib-integration

systemd-run avahi-browse -varp
systemd-run avahi-publish -vs test _qotd._tcp 1234 a=1 b
systemd-run avahi-publish -s --subtype _beep._sub._qotd._tcp BOOP _qotd._tcp 1234

# https://github.com/lathiat/avahi/issues/455
# The idea is to produce a lot of arguments by splitting the output
# of the perl one-liner so it shouldn't be quoted.
# shellcheck disable=SC2046
avahi-publish -s T _qotd._tcp 22 $(perl -le 'print "A " x 100000')

h="$(hostname).local"
ipv4addr=$(avahi-resolve -v -4 -n "$h" | perl -alpe '$_ = $F[1]')
ipv6addr=$(avahi-resolve -v -6 -n "$h" | perl -alpe '$_ = $F[1]')

avahi-resolve -v -a "$ipv4addr"
avahi-resolve -v -a "$ipv6addr"

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
    printf "%s\n" "$cmd" | ncat -w1 -U /run/avahi-daemon/socket
done

timeout --foreground 180 bash -c 'while :; do
    radamsa -r CORPUS | ncat -w1 -i0.5 -U /run/avahi-daemon/socket
done >&/dev/null' || true

avahi-browse -varpt
avahi-browse -varpc
[[ -n "$(avahi-browse -vrpc _beep._sub._qotd._tcp)" ]]

avahi-set-host-name -v 'A\.B'
avahi-set-host-name -v "$(perl -e 'print(q/[/x63)')"
avahi-set-host-name -v "$(hostname)-new"

run dfuzzer -v -n org.freedesktop.Avahi

run systemctl kill --signal HUP avahi-daemon
run systemctl kill --signal USR1 avahi-daemon
run systemctl reload avahi-daemon

[[ -n "$(systemctl show --property StatusText avahi-daemon --value)" ]]

run systemctl stop avahi-daemon
if systemctl is-failed avahi-daemon; then
    journalctl --sync
    journalctl -u avahi-daemon --no-pager
    exit 1
fi

look_for_asan_ubsan_reports
