# $Id$

# This file is part of nss-mdns.
#
# nss-mdns is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# nss-mdns is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with nss-mdns; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.

noinst_DATA = README.html README
EXTRA_DIST = $(noinst_DATA) style.css

MAINTAINERCLEANFILES = README.html
CLEANFILES =

if USE_LYNX
README: README.html
	lynx --dump $^ | sed 's,file://localhost/.*/doc/README.html,README,' > $@

CLEANFILES += README
endif

tidy: README.html
	tidy -qe < README.html ; true

.PHONY: tidy

