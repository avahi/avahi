

all: avahi-sharp.dll

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
	Utility.cs

avahi-sharp.dll: $(AVAHISOURCES)
	mcs -target:library -o $@ -debug $(AVAHISOURCES) -r:Mono.Posix

CLEANFILES = avahi-sharp.dll avahi-sharp.dll.mdb
