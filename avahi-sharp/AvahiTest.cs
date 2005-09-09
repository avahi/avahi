using System;
using System.Net;
using Gtk;
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

		Application.Run ();
	}

	private static void OnEntryGroupChanged (object o, EntryGroupState state)
	{
		Console.WriteLine ("Entry group status: " + state);

		if (state == EntryGroupState.Established) {
			DomainBrowser browser = new DomainBrowser (client);
			browser.DomainAdded += OnDomainAdded;
		}
	}

	private static void OnDomainAdded (object o, DomainInfo info)
	{
		Console.WriteLine ("Got domain added: " + info.Domain);
		ServiceTypeBrowser stb = new ServiceTypeBrowser (client, info.Domain);
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
		Console.WriteLine ("Service '{0}' at {1}:{2}", info.Name, info.Host, info.Port);
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
