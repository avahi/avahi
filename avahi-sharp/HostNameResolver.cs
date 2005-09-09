using System;
using System.Collections;
using System.Net;
using System.Runtime.InteropServices;
using Mono.Unix;

namespace Avahi
{

    internal delegate void HostNameResolverCallback (IntPtr resolver, int iface, Protocol proto,
                                                     ResolverEvent revent, IntPtr hostname, IntPtr address,
                                                     IntPtr userdata);

    public class HostNameResolver : IDisposable
    {
        private IntPtr handle;
        private Client client;
        private int iface;
        private Protocol proto;
        private string hostname;
        private Protocol aproto;

        private IPAddress currentAddress;
        private string currentHost;

        private ArrayList foundListeners = new ArrayList ();
        private ArrayList timeoutListeners = new ArrayList ();
        
        [DllImport ("avahi-client")]
        private static extern IntPtr avahi_host_name_resolver_new (IntPtr client, int iface, Protocol proto,
                                                                   IntPtr hostname, Protocol aproto,
                                                                   HostNameResolverCallback cb, IntPtr userdata);

        [DllImport ("avahi-client")]
        private static extern void avahi_host_name_resolver_free (IntPtr handle);

        public event HostAddressHandler Found
        {
            add {
                foundListeners.Add (value);
                Start ();
            }
            remove {
                foundListeners.Remove (value);
                Stop (false);
            }
        }
        
        public event EventHandler Timeout
        {
            add {
                timeoutListeners.Add (value);
                Start ();
            }
            remove {
                timeoutListeners.Remove (value);
                Stop (false);
            }
        }

        public IPAddress Address
        {
            get { return currentAddress; }
        }

        public string HostName
        {
            get { return currentHost; }
        }

        public HostNameResolver (Client client, string hostname) : this (client, -1, Protocol.Unspecified,
                                                                         hostname, Protocol.Unspecified)
        {
        }

        public HostNameResolver (Client client, int iface, Protocol proto, string hostname,
                                 Protocol aproto)
        {
            this.client = client;
            this.iface = iface;
            this.proto = proto;
            this.hostname = hostname;
            this.aproto = aproto;
        }

        ~HostNameResolver ()
        {
            Dispose ();
        }

        public void Dispose ()
        {
            Stop (true);
        }

        private void Start ()
        {
            if (handle != IntPtr.Zero || (foundListeners.Count == 0 && timeoutListeners.Count == 0))
                return;

            IntPtr hostPtr = Utility.StringToPtr (hostname);
            handle = avahi_host_name_resolver_new (client.Handle, iface, proto, hostPtr, aproto,
                                                   OnHostNameResolverCallback, IntPtr.Zero);
            Utility.Free (hostPtr);
        }

        private void Stop (bool force)
        {
            if (handle != IntPtr.Zero && (force || (foundListeners.Count == 0 && timeoutListeners.Count == 0))) {
                avahi_host_name_resolver_free (handle);
                handle = IntPtr.Zero;
            }
        }

        private void OnHostNameResolverCallback (IntPtr resolver, int iface, Protocol proto,
                                                 ResolverEvent revent, IntPtr hostname, IntPtr address,
                                                 IntPtr userdata)
        {
            if (revent == ResolverEvent.Found) {
                currentAddress = Utility.PtrToAddress (address);
                currentHost = Utility.PtrToString (hostname);

                foreach (HostAddressHandler handler in foundListeners)
                    handler (this, currentHost, currentAddress);
            } else {
                currentAddress = null;
                currentHost = null;
                
                foreach (EventHandler handler in timeoutListeners)
                    handler (this, new EventArgs ());
            }
        }
    }
}
