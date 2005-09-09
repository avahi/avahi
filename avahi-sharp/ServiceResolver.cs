using System;
using System.Collections;
using System.Net;
using System.Runtime.InteropServices;
using Mono.Unix;

namespace Avahi
{

    internal delegate void ServiceResolverCallback (IntPtr resolver, int iface, Protocol proto,
                                                    ResolverEvent revent, IntPtr name, IntPtr type,
                                                    IntPtr domain, IntPtr host, IntPtr address,
                                                    UInt16 port, IntPtr txt, IntPtr userdata);

    public class ServiceResolver : IDisposable
    {
        private IntPtr handle;
        private ServiceInfo currentInfo;
        private Client client;
        private int iface;
        private Protocol proto;
        private string name;
        private string type;
        private string domain;
        private Protocol aproto;

        private ArrayList foundListeners = new ArrayList ();
        private ArrayList timeoutListeners = new ArrayList ();
        
        [DllImport ("avahi-client")]
        private static extern IntPtr avahi_service_resolver_new (IntPtr client, int iface, Protocol proto,
                                                                 IntPtr name, IntPtr type, IntPtr domain,
                                                                 Protocol aproto, ServiceResolverCallback cb,
                                                                 IntPtr userdata);

        [DllImport ("avahi-client")]
        private static extern void avahi_service_resolver_free (IntPtr handle);

        public event ServiceInfoHandler Found
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

        public ServiceInfo Service
        {
            get { return currentInfo; }
        }

        public ServiceResolver (Client client, string name, string type, string domain) : this (client, -1,
                                                                                                Protocol.Unspecified,
                                                                                                name, type, domain,
                                                                                                Protocol.Unspecified)
        {
        }

        public ServiceResolver (Client client, ServiceInfo service) : this (client, service.NetworkInterface,
                                                                            service.Protocol, service.Name,
                                                                            service.ServiceType, service.Domain,
                                                                            Protocol.Unspecified)
        {
        }
        
        public ServiceResolver (Client client, int iface, Protocol proto, string name,
                                string type, string domain, Protocol aproto)
        {
            this.client = client;
            this.iface = iface;
            this.proto = proto;
            this.name = name;
            this.type = type;
            this.domain = domain;
            this.aproto = aproto;
            
            
        }

        ~ServiceResolver ()
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

            IntPtr namePtr = Utility.StringToPtr (name);
            IntPtr typePtr = Utility.StringToPtr (type);
            IntPtr domainPtr = Utility.StringToPtr (domain);
            handle = avahi_service_resolver_new (client.Handle, iface, proto, namePtr, typePtr, domainPtr,
                                                 aproto, OnServiceResolverCallback, IntPtr.Zero);
            Utility.Free (namePtr);
            Utility.Free (typePtr);
            Utility.Free (domainPtr);
        }

        private void Stop (bool force)
        {
            if (handle != IntPtr.Zero && (force || (foundListeners.Count == 0 && timeoutListeners.Count == 0))) {
                avahi_service_resolver_free (handle);
                handle = IntPtr.Zero;
            }
        }

        private void OnServiceResolverCallback (IntPtr resolver, int iface, Protocol proto,
                                                ResolverEvent revent, IntPtr name, IntPtr type,
                                                IntPtr domain, IntPtr host, IntPtr address,
                                                UInt16 port, IntPtr txt, IntPtr userdata)
        {
            ServiceInfo info;
            info.NetworkInterface = iface;
            info.Protocol = proto;
            info.Domain = Utility.PtrToString (domain);
            info.ServiceType = Utility.PtrToString (type);
            info.Name = Utility.PtrToString (name);
            info.Host = Utility.PtrToString (host);
            info.Address = Utility.PtrToAddress (address);
            info.Port = port;
            info.Text = null;

            if (revent == ResolverEvent.Found) {
                currentInfo = info;

                foreach (ServiceInfoHandler handler in foundListeners)
                    handler (this, info);
            } else {
                currentInfo = ServiceInfo.Zero;
                
                foreach (EventHandler handler in timeoutListeners)
                    handler (this, new EventArgs ());
            }
        }
    }
}
