#!/usr/bin/env bash

set -eux
set -o pipefail

dump_journal() {
    journalctl --sync
    journalctl -b -u "avahi-*" --no-pager
}

run() {
    if ! "$@"; then
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
    busctl call org.freedesktop.Avahi / org.freedesktop.Avahi.Server -- "$@"
    busctl call org.freedesktop.Avahi / org.freedesktop.Avahi.Server2 -- "$@"
}

for p in avahi-{browse,daemon,publish,resolve,set-host-name}; do
    "$p" -h
    "$p" -V
done

run systemctl start avahi-daemon
run systemctl start avahi-dnsconfd

./avahi-client/check-nss-test
./avahi-client/client-test
(cd avahi-daemon && ./ini-file-parser-test)
./avahi-compat-howl/address-test
./avahi-compat-libdns_sd/null-test
./avahi-core/avahi-test
./avahi-core/querier-test
./examples/glib-integration
./tests/c-plus-plus-test

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
avahi-publish -s T _qotd._tcp 22 $(perl -le 'print "A " x 100000')

h="$(hostname).local"
ipv4addr=$(avahi-resolve -v -4 -n "$h" | perl -alpe '$_ = $F[1]')
ipv6addr=$(avahi-resolve -v -6 -n "$h" | perl -alpe '$_ = $F[1]')

avahi-resolve -v -a "$ipv4addr"
avahi-resolve -v -a "$ipv6addr"

systemd-run -u avahi-test-publish-address avahi-publish -fvaR  "$h" "$ipv4addr"

# ::1 isn't here due to https://github.com/avahi/avahi/issues/574
for s in 127.0.0.1 224.0.0.1 ff02::fb; do
   drill "@$s" -p5353 "$h" ANY
   drill "@$s" -p5353 "_services._dns-sd._udp.local" ANY
done

dbus_call ResolveAddress "iisu" -1 -1 "$ipv4addr" 0
should_fail dbus_call ResolveAddress "iisu" -1 -1 1.1.1.1 2

dbus_call ResolveHostName "iisiu" -1 -1 "$(hostname).local" -1 0
should_fail dbus_call ResolveHostName "iisiu" -1 -1 one.one.one.one -1 2

dbus_call ResolveService "iisssiu" -1 -1 "$(hostname)" _ssh._tcp local -1 0
should_fail dbus_call ResolveService "iisssiu" -1 -1 "$(hostname)" _ssh._tcp dns-sd.org -1 2

# https://github.com/avahi/avahi/issues/491
dbus_call GetAlternativeHostName "s" "a-2147483647"

# https://github.com/avahi/avahi/issues/526
dbus_call GetAlternativeServiceName "s" "a #2147483647"

printf "%s\n" "RESOLVE-ADDRESS $(perl -e 'print q/A/ x 1014')" | ncat -w1 -U /run/avahi-daemon/socket

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

# sleep is used here to let the HINFO record browser see
# the ADD/REMOVE events. If the hostname changes too fast
# the ADD events coalesce and the last one wins.
avahi-set-host-name -v "ecstasy"
sleep 2

avahi-set-host-name -v 'A\.B'
avahi-set-host-name -v "$(perl -e 'print(q/[/x63)')"
avahi-set-host-name -v "$(hostname)-new"

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
