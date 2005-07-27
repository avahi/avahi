#!/usr/bin/python2.4

import dbus
import dbus.glib
import gtk

from time import sleep

bus = dbus.SystemBus()

server = dbus.Interface(bus.get_object("org.freedesktop.Avahi", '/org/freedesktop/Avahi/Server'), 'org.freedesktop.Avahi.Server')

print "Host name: %s" % server.GetHostName()
print "Domain name: %s" % server.GetDomainName()
print "FQDN: %s" % server.GetHostNameFqdn()

g = dbus.Interface(bus.get_object("org.freedesktop.Avahi", server.EntryGroupNew()), 'org.freedesktop.Avahi.EntryGroup')

def state_changed_callback(t):
    print "StateChanged: ", t

g.connect_to_signal('StateChanged', state_changed_callback)
g.AddService(0, 0, "_http._tcp", "foo", "", "", dbus.UInt16(4712), ["fuck=hallo", "gurke=mega"])
g.AddAddress(0, 0, "foo.local", "47.11.8.15")
g.Commit()

try:
    gtk.main()
except KeyboardInterrupt, k:
    pass

g.Free()

print "Quit"
