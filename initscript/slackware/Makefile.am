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

initddir = $(sysconfdir)/rc.d

EXTRA_DIST = avahi-daemon.in avahi-dnsconfd.in

initd_SCRIPTS = rc.avahidaemon rc.avahidnsconfd

CLEANFILES = rc.avahidaemon rc.avahidnsconfd

rc.avahidaemon: avahi-daemon.in
	sed -e 's,@sbindir\@,$(sbindir),g' $< > $@
	chmod +x $@

rc.avahidnsconfd: avahi-dnsconfd.in
	sed -e 's,@sbindir\@,$(sbindir),g' $< > $@
	chmod +x $@
