#!/usr/bin/python2.4

import dbus
import gobject
try: import dbus.glib
except ImportError, e: pass

from time import sleep

bus = dbus.SystemBus()

server = dbus.Interface(bus.get_object("org.freedesktop.Avahi", '/org/freedesktop/Avahi/Server'), 'org.freedesktop.Avahi.Server')

def server_state_changed_callback(t):
    print "Server::StateChanged: ", t

server.connect_to_signal("StateChanged", server_state_changed_callback)

print server.ResolveHostName(0, 0, "ecstasy.local", 0)
print server.ResolveAddress(0, 0, "192.168.50.4")

print "Host name: %s" % server.GetHostName()
print "Domain name: %s" % server.GetDomainName()
print "FQDN: %s" % server.GetHostNameFqdn()

g = dbus.Interface(bus.get_object("org.freedesktop.Avahi", server.EntryGroupNew()), 'org.freedesktop.Avahi.EntryGroup')

def entry_group_state_changed_callback(t):
    print "EntryGroup::StateChanged: ", t

    if t == 1:
        print server.ResolveHostName(0, 0, "foo.local", 0)
        

g.connect_to_signal('StateChanged', entry_group_state_changed_callback)

g.AddService(0, 0, "Test Web Site", "_http._tcp", "", "", dbus.UInt16(4712), ["fuck=hallo", "gurke=mega"])
g.AddAddress(0, 0, "foo.local", "47.11.8.15")
g.Commit()

try:
    gobject.MainLoop().run()
except KeyboardInterrupt, k:
    pass

g.Free()

print "Quit"
