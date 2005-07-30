
EXTRA_DIST=75avahi-daemon.in avahi-dnsconfd.in

dbusinitddir = $(sysconfdir)/dbus-1/event.d
dbusinitd_SCRIPTS = 75avahi-daemon

initddir = $(sysconfdir)/init.d
initd_SCRIPTS = avahi-dnsconfd

CLEANFILES=75avahi-daemon avahi-dnsconfd

75avahi-daemon: 75avahi-daemon.in
	sed -e 's,@sbindir\@,$(sbindir),g' $< > $@
	chmod +x $@

avahi-dnsconfd: avahi-dnsconfd.in
	sed -e 's,@sbindir\@,$(sbindir),g' $< > $@
	chmod +x $@

#if ENABLE_DBUS
#install-data-local:
#	$(LN_S) -f $(initddir)/avahi-daemon \
#		$(sysconfdir)/dbus-1/event.d/75avahi-daemon
#endif
