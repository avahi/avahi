#!/bin/bash

# This file is part of avahi.
#
# avahi is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# avahi is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with avahi; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

set -eux

COVERITY_SCAN_TOOL_BASE="/tmp/coverity-scan-analysis"
COVERITY_SCAN_PROJECT_NAME="avahi-daemon"

function coverity_install_script {
    local platform tool_url tool_archive

    platform=$(uname)
    tool_url="https://scan.coverity.com/download/${platform}"
    tool_archive="/tmp/cov-analysis-${platform}.tgz"

    set +x # this is supposed to hide COVERITY_SCAN_TOKEN
    echo -e "\033[33;1mDownloading Coverity Scan Analysis Tool...\033[0m"
    wget -nv -O "$tool_archive" "$tool_url" --post-data "project=$COVERITY_SCAN_PROJECT_NAME&token=$COVERITY_SCAN_TOKEN"
    set -x

    mkdir -p "$COVERITY_SCAN_TOOL_BASE"
    pushd "$COVERITY_SCAN_TOOL_BASE"
    tar xzf "$tool_archive"
    popd
}

function run_coverity {
    local results_dir tool_dir results_archive sha response status_code

    results_dir="cov-int"
    tool_dir=$(find "$COVERITY_SCAN_TOOL_BASE" -type d -name 'cov-analysis*')
    results_archive="analysis-results.tgz"
    sha=$(git rev-parse --short HEAD)

    ./bootstrap.sh --enable-compat-howl --enable-compat-libdns_sd --enable-tests \
        --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu
    COVERITY_UNSUPPORTED=1 "$tool_dir/bin/cov-build" --dir "$results_dir" make V=1
    "$tool_dir/bin/cov-import-scm" --dir "$results_dir" --scm git --log "$results_dir/scm_log.txt"

    tar czf "$results_archive" "$results_dir"

    set +x # this is supposed to hide COVERITY_SCAN_TOKEN
    echo -e "\033[33;1mUploading Coverity Scan Analysis results...\033[0m"
    response=$(curl \
               --silent --write-out "\n%{http_code}\n" \
               --form project="$COVERITY_SCAN_PROJECT_NAME" \
               --form token="$COVERITY_SCAN_TOKEN" \
               --form email="$COVERITY_SCAN_NOTIFICATION_EMAIL" \
               --form file="@$results_archive" \
               --form version="$sha" \
               --form description="Daily build" \
               https://scan.coverity.com/builds)
    status_code=$(printf "%s" "$response" | sed -n '$p')
    if [ "$status_code" != "200" ]; then
        echo -e "\033[33;1mCoverity Scan upload failed.\033[0m"
        return 1
    fi
    set -x
}

coverity_install_script
run_coverity
