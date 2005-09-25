/* $Id$ */

/***
  This file is part of avahi.

  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

using System;
using System.Text;
using System.Net;
using Avahi;

public class AvahiTest {
	private static Client client;

	public static void Main () {
		client = new Client ();

		EntryGroup eg = new EntryGroup (client);
		eg.StateChanged += OnEntryGroupChanged;
		eg.AddService ("foobar2", "_daap._tcp", client.DomainName,
			       444, new string[] { "foo", "bar", "baz" });
		eg.Commit ();
		Console.WriteLine ("Press enter to quit");
		Console.ReadLine ();
	}

	private static void OnEntryGroupChanged (object o, EntryGroupState state)
	{
		Console.WriteLine ("Entry group status: " + state);

		/*
		if (state == EntryGroupState.Established) {
			DomainBrowser browser = new DomainBrowser (client);
			browser.DomainAdded += OnDomainAdded;
		}
		*/

		BrowseServiceTypes ("dns-sd.org");
	}

	private static void OnDomainAdded (object o, DomainInfo info)
	{
		Console.WriteLine ("Got domain added: " + info.Domain);
		BrowseServiceTypes (info.Domain);
	}

	private static void BrowseServiceTypes (string domain)
	{
		ServiceTypeBrowser stb = new ServiceTypeBrowser (client, domain);
		stb.ServiceTypeAdded += OnServiceTypeAdded;
	}

	private static void OnServiceTypeAdded (object o, ServiceTypeInfo info)
	{
		Console.WriteLine ("Got service type: " + info.ServiceType);
		ServiceBrowser sb = new ServiceBrowser (client, info.ServiceType, info.Domain);
		sb.ServiceAdded += OnServiceAdded;
	}

	private static void OnServiceAdded (object o, ServiceInfo info)
	{
		// Console.WriteLine ("Got service: " + info.Name);
		ServiceResolver resolver = new ServiceResolver (client, info);
		resolver.Found += OnServiceResolved;
	}

	private static void OnServiceResolved (object o, ServiceInfo info)
	{
		Console.WriteLine ("Service '{0}' at {1}:{2}", info.Name, info.HostName, info.Port);
		foreach (byte[] bytes in info.Text) {
			Console.WriteLine ("Text: " + Encoding.UTF8.GetString (bytes));
		}
		AddressResolver ar = new AddressResolver (client, info.Address);
		ar.Found += OnAddressResolved;
	}

	private static void OnAddressResolved (object o, string host, IPAddress address)
	{
		Console.WriteLine ("Resolved {0} to {1}", address, host);
		HostNameResolver hr = new HostNameResolver (client, host);
		hr.Found += OnHostNameResolved;
	}

	private static void OnHostNameResolved (object o, string host, IPAddress address)
	{
		Console.WriteLine ("Resolved {0} to {1}", host, address);
	}
}
