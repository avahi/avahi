#!/bin/bash

set -eux
set -o pipefail

sed -i -e '/^#\s*deb-src.*\smain\s\+restricted/s/^#//' /etc/apt/sources.list
apt-get update -y
apt-get build-dep -y avahi
apt-get install -y libevent-dev qtbase5-dev
