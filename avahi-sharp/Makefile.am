

ASSEMBLY = avahi-sharp.dll

CLEANFILES = $(ASSEMBLY) $(ASSEMBLY).mdb
EXTRA_DIST = $(AVAHISOURCES) avahi.snk

AVAHISOURCES = 				\
	$(srcdir)/AddressResolver.cs	\
	$(srcdir)/Client.cs		\
	$(srcdir)/ClientException.cs	\
	$(srcdir)/DomainBrowser.cs	\
	$(srcdir)/EntryGroup.cs		\
	$(srcdir)/HostNameResolver.cs	\
	$(srcdir)/ServiceBrowser.cs	\
	$(srcdir)/ServiceResolver.cs	\
	$(srcdir)/ServiceTypeBrowser.cs	\
	$(srcdir)/Utility.cs

$(ASSEMBLY): $(AVAHISOURCES)
	mcs -keyfile:$(srcdir)/avahi.snk -target:library -o $@ -debug $(AVAHISOURCES) -r:Mono.Posix

if HAVE_MONO
all: $(ASSEMBLY)

install-data-hook:
	$(GACUTIL) /i $(ASSEMBLY) /package avahi-sharp /gacdir $(libdir) /root $(DESTDIR)$(libdir)
endif
