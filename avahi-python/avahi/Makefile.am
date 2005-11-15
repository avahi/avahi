# $Id$

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

EXTRA_DIST = __init__.py SimpleGladeApp.py ServiceTypeDatabase.py.in

if HAVE_PYTHON

avahidir = $(pythondir)/avahi

avahi_SCRIPTS = ServiceTypeDatabase.py

ServiceTypeDatabase.py: ServiceTypeDatabase.py.in
	sed -e 's,@PYTHON\@,$(PYTHON),g' \
		-e 's,@pkgdatadir\@,$(pkgdatadir),g' $< > $@
	chmod +x $@

if HAVE_PYGTK
if HAVE_PYTHON_DBUS

avahi_PYTHON = __init__.py SimpleGladeApp.py 

endif
endif
endif

CLEANFILES=*.pyc *.pyo ServiceTypeDatabase.py


