#!/bin/bash

set -eux
set -o pipefail

export ASAN_UBSAN=${ASAN_UBSAN:-false}
export CFLAGS=${CFLAGS:-}
export WERROR=${WERROR:-false}

autogen_args=" --enable-compat-howl --enable-compat-libdns_sd --enable-tests"

# CFLAGS can be used to turn off warnings globally but it doesn't make sense to
# ignore, say, format-truncation everywhere when only one component triggers it.
# This function patches Makefiles to disable warnings where it's actually necessary
# to let the CI keep catching them in other places. It's a kludge and it should
# be removed once all warnings avahi triggers are fixed.
ignore_warnings() {
    local _component="$1"
    shift
    for warning in "$@"; do
        sed -i 's/^\(AM_CFLAGS=\)\(.*\)/\1-Wno-error='"$warning"' \2/' "$_component/Makefile.am"
    done
}

case "$1" in
    install-build-deps)
        sed -i -e '/^#\s*deb-src.*\smain\s\+restricted/s/^#//' /etc/apt/sources.list
        apt-get update -y
        apt-get build-dep -y avahi
        apt-get install -y libevent-dev qtbase5-dev
        apt-get install -y gcc clang
        apt-get install -y autoconf-archive
        ;;
    build)
        if [[ "$ASAN_UBSAN" == true ]]; then
            CFLAGS+=" -fsanitize=address,undefined -g"
            export ASAN_OPTIONS=strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
            export UBSAN_OPTIONS=print_stacktrace=1:print_summary=1:halt_on_error=1
        fi

        if [[ "$WERROR" == true ]]; then
            # acx_pthread isn't compatible with CFLAGS=-Werror at all so it's just replaced
            # with ax_pthread from autoconf-archive.
            rm -rf common/acx_pthread.m4

            CFLAGS+=" -Werror"

            # The following warnings are ignored because it's hard to fix them all in one fell swoop.
            # Ideally they should be fixed one by one and removed from here to let compilers catch them
            # automatically on PRs.
            if [[ "$CC" == gcc ]]; then
                ignore_warnings avahi-autoipd unused-result
                ignore_warnings avahi-common unused-result
                ignore_warnings avahi-compat-howl cast-function-type missing-prototypes pedantic pointer-sign
                ignore_warnings avahi-compat-howl/samples discarded-qualifiers pointer-sign shadow unused-parameter unused-variable
                ignore_warnings avahi-compat-libdns_sd pedantic
                ignore_warnings avahi-core format-truncation
                ignore_warnings avahi-daemon stringop-truncation unused-result
                ignore_warnings avahi-dnsconfd unused-result
                ignore_warnings avahi-glib deprecated-declarations
                ignore_warnings avahi-ui deprecated-declarations implicit-fallthrough maybe-uninitialized
                ignore_warnings avahi-utils unused-result

                # avahi-gobject triggers a lot of warnings like
                # "Deprecated pre-processor symbol: replace with "G_ADD_PRIVATE""
                # and judging by https://gitlab.gnome.org/GNOME/glib/-/issues/2247
                # they can't be disabled when code is built with gcc with -Werror.
                autogen_args+=" --disable-gobject"
            elif [[ "$CC" == clang ]]; then
                CFLAGS+=" -Wno-error=strict-prototypes"

                ignore_warnings avahi-autoipd cast-align
                ignore_warnings avahi-client varargs
                ignore_warnings avahi-compat-howl cast-align missing-prototypes
                ignore_warnings avahi-compat-howl/samples cast-align incompatible-pointer-types-discards-qualifiers pointer-sign shadow unused-parameter unused-variable
                ignore_warnings avahi-core cast-align logical-not-parentheses varargs
                ignore_warnings avahi-daemon cast-align
                ignore_warnings avahi-glib deprecated-declarations enum-conversion
                ignore_warnings avahi-gobject deprecated enum-conversion
                ignore_warnings avahi-ui deprecated-declarations
            fi
        fi

        if ! ./autogen.sh $autogen_args --prefix=/usr; then
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
