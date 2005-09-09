

ASSEMBLY = avahi-sharp.dll

CLEANFILES = $(ASSEMBLY) $(ASSEMBLY).mdb
EXTRA_DIST = $(AVAHISOURCES) avahi.snk avahi-sharp.pc.in

AVAHISOURCES = 				\
	$(srcdir)/AssemblyInfo.cs	\
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
	mcs -keyfile:$(srcdir)/avahi.snk -target:library -out:$@ -debug $(AVAHISOURCES) -r:Mono.Posix

if HAVE_MONO
all: $(ASSEMBLY)

pkgcfgdir = $(libdir)/pkgconfig
pkgcfg_DATA = avahi-sharp.pc

avahi-sharp.pc: avahi-sharp.pc.in
	sed -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' \
	    -e 's,@libdir\@,$(libdir),g' $< > $@

install-data-hook:
	$(GACUTIL) /i $(ASSEMBLY) /package avahi-sharp /gacdir $(libdir) /root $(DESTDIR)$(libdir)
endif
