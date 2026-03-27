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
# License along with avahi; if not, see <https://www.gnu.org/licenses/>.

initddir = /Library/LaunchDaemons/

EXTRA_DIST = \
	org.freedesktop.avahi-daemon.plist.in \
	org.freedesktop.avahi-dnsconfd.plist.in

initd_SCRIPTS = \
	org.freedesktop.avahi-daemon.plist \
	org.freedesktop.avahi-dnsconfd.plist

CLEANFILES = \
	org.freedesktop.avahi-daemon.plist \
	org.freedesktop.avahi-dnsconfd.plist

org.freedesktop.avahi-daemon.plist: org.freedesktop.avahi-daemon.plist.in
	sed -e 's,@sbindir\@,$(sbindir),g' $< > $@

org.freedesktop.avahi-dnsconfd.plist: org.freedesktop.avahi-dnsconfd.plist.in
	sed -e 's,@sbindir\@,$(sbindir),g' $< > $@
