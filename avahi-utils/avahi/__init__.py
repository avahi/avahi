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

# Some definitions matching those in core.h
import socket, dbus

SERVER_INVALID, SERVER_REGISTERING, SERVER_RUNNING, SERVER_COLLISION = range(0, 4)

ENTRY_GROUP_UNCOMMITED, ENTRY_GROUP_REGISTERING, ENTRY_GROUP_ESTABLISHED, ENTRY_GROUP_COLLISION = range(0, 4)

DOMAIN_BROWSER_REGISTER, DOMAIN_BROWSER_REGISTER_DEFAULT, DOMAIN_BROWSER_BROWSE, DOMAIN_BROWSER_BROWSE_DEFAULT, DOMAIN_BROWSER_BROWSE_LEGACY = range(0, 5)

PROTO_INET, PROTO_INET6, PROTO_UNSPEC = socket.AF_INET, socket.AF_INET6, socket.AF_UNSPEC

IF_UNSPEC = -1

DBUS_NAME = "org.freedesktop.Avahi"
DBUS_INTERFACE_SERVER = DBUS_NAME + ".Server"
DBUS_PATH_SERVER = "/"
DBUS_INTERFACE_ENTRY_GROUP = DBUS_NAME + ".EntryGroup"
DBUS_INTERFACE_DOMAIN_BROWSER = DBUS_NAME + ".DomainBrowser"
DBUS_INTERFACE_SERVICE_TYPE_BROWSER = DBUS_NAME + ".ServiceTypeBrowser"
DBUS_INTERFACE_SERVICE_BROWSER = DBUS_NAME + ".ServiceBrowser"

def byte_array_to_string(s):
    r = ""
    
    for c in s:
        
        if c >= 32 and c < 127:
            r += "%c" % c
        else:
            r += "."

    return r

def txt_array_to_string_array(t):
    l = []

    for s in t:
        l.append(byte_array_to_string(s))

    return l


def string_to_byte_array(s):
    r = []

    for c in s:
        r.append(dbus.Byte(ord(c)))

    return r

def string_array_to_txt_array(t):
    l = []

    for s in t:
        l.append(string_to_byte_array(s))

    return l
