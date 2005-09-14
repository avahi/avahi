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

ASSEMBLY = avahi-sharp.dll

CLEANFILES = $(ASSEMBLY) $(ASSEMBLY).mdb

AVAHISOURCES = 				\
	$(srcdir)/AddressResolver.cs	\
	$(srcdir)/AssemblyInfo.cs	\
	$(srcdir)/Client.cs		\
	$(srcdir)/ClientException.cs	\
	$(srcdir)/DomainBrowser.cs	\
	$(srcdir)/EntryGroup.cs		\
	$(srcdir)/HostNameResolver.cs	\
	$(srcdir)/ServiceBrowser.cs	\
	$(srcdir)/ServiceResolver.cs	\
	$(srcdir)/ServiceTypeBrowser.cs	\
	$(srcdir)/Utility.cs

EXTRA_DIST =				\
	$(AVAHISOURCES)			\
	$(srcdir)/AvahiTest.cs 		\
	$(srcdir)/avahi.snk		\
	$(srcdir)/avahi-sharp.dll.config	\
	$(srcdir)/avahi-sharp-docs.source	\
	$(srcdir)/en/*.xml		\
	$(srcdir)/en/*/*.xml


$(ASSEMBLY): $(AVAHISOURCES)
	mcs -keyfile:$(srcdir)/avahi.snk -target:library -out:$@ -debug $(AVAHISOURCES) -r:Mono.Posix

if HAVE_MONO
all: $(ASSEMBLY)

if HAVE_MONODOC
update-docs: $(ASSEMBLY)
	$(MONODOCER) -assembly:$(ASSEMBLY) -path:en

avahi-sharp-docs.zip: $(srcdir)/en/*/*
	$(MDASSEMBLER) --out avahi-sharp-docs --ecma $(srcdir)/en

docdir = $(MONODOC_DIR)
doc_DATA = avahi-sharp-docs.zip avahi-sharp-docs.tree avahi-sharp-docs.source

endif

install-data-hook: $(ASSEMBLY)
	$(GACUTIL) /i $(ASSEMBLY) /package avahi-sharp /gacdir $(libdir) /root $(DESTDIR)$(libdir)

uninstall-hook: $(ASSEMBLY)
	$(GACUTIL) /u avahi-sharp /package avahi-sharp /gacdir $(libdir) /root $(DESTDIR)$(libdir)
endif
