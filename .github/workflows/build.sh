#!/bin/bash

set -eux
set -o pipefail

case "$1" in
    install-build-deps)
        sed -i -e '/^#\s*deb-src.*\smain\s\+restricted/s/^#//' /etc/apt/sources.list
        apt-get update -y
        apt-get build-dep -y avahi
        apt-get install -y libevent-dev qtbase5-dev
        apt-get install -y gcc clang
        ;;
    build)
        ./bootstrap.sh --enable-tests
        make -j"$(nproc)" V=1
        make check VERBOSE=1
        ;;
    *)
        printf '%s' "Unknown command '$1'" >&2
        exit 1
esac
