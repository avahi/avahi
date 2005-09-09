using System;
using System.Collections;
using System.Runtime.InteropServices;

namespace Avahi
{
    internal delegate void ServiceTypeBrowserCallback (IntPtr browser, int iface, Protocol proto, BrowserEvent bevent,
                                                       IntPtr type, IntPtr domain, IntPtr userdata);
    
    public struct ServiceTypeInfo
    {
        public int NetworkInterface;
        public Protocol Protocol;
        public string Domain;
        public string ServiceType;
    }

    public delegate void ServiceTypeInfoHandler (object o, ServiceTypeInfo info);
    
    public class ServiceTypeBrowser : IDisposable
    {
        private IntPtr handle;
        private ArrayList infos = new ArrayList ();
        private Client client;
        private int iface;
        private Protocol proto;
        private string domain;

        private ArrayList addListeners = new ArrayList ();
        private ArrayList removeListeners = new ArrayList ();
        
        [DllImport ("avahi-client")]
        private static extern IntPtr avahi_service_type_browser_new (IntPtr client, int iface, int proto,
                                                                     IntPtr domain, ServiceTypeBrowserCallback cb,
                                                                     IntPtr userdata);

        [DllImport ("avahi-client")]
        private static extern void avahi_service_type_browser_free (IntPtr handle);

        public event ServiceTypeInfoHandler ServiceTypeAdded
        {
            add {
                addListeners.Add (value);
                Start ();
            }
            remove {
                addListeners.Remove (value);
                Stop (false);
            }
        }
        
        public event ServiceTypeInfoHandler ServiceTypeRemoved
        {
            add {
                removeListeners.Add (value);
                Start ();
            }
            remove {
                removeListeners.Remove (value);
                Stop (false);
            }
        }

        public ServiceTypeInfo[] ServiceTypes
        {
            get { return (ServiceTypeInfo[]) infos.ToArray (typeof (ServiceTypeInfo)); }
        }

        public ServiceTypeBrowser (Client client) : this (client, client.DomainName)
        {
        }

        public ServiceTypeBrowser (Client client, string domain) : this (client, -1, Protocol.Unspecified, domain)
        {
        }
        
        public ServiceTypeBrowser (Client client, int iface, Protocol proto, string domain)
        {
            this.client = client;
            this.iface = iface;
            this.proto = proto;
            this.domain = domain;
        }

        ~ServiceTypeBrowser ()
        {
            Dispose ();
        }

        public void Dispose ()
        {
            Stop (true);
        }

        private void Start ()
        {
            if (handle != IntPtr.Zero || (addListeners.Count == 0 && removeListeners.Count == 0))
                return;

            IntPtr domainPtr = Utility.StringToPtr (domain);
            handle = avahi_service_type_browser_new (client.Handle, iface, (int) proto, domainPtr,
                                                     OnServiceTypeBrowserCallback, IntPtr.Zero);
            Utility.Free (domainPtr);
        }

        private void Stop (bool force)
        {
            if (handle != IntPtr.Zero && (force || (addListeners.Count == 0 && removeListeners.Count == 0))) {
                avahi_service_type_browser_free (handle);
                handle = IntPtr.Zero;
            }
        }

        private void OnServiceTypeBrowserCallback (IntPtr browser, int iface, Protocol proto, BrowserEvent bevent,
                                                   IntPtr type, IntPtr domain, IntPtr userdata)
        {

            ServiceTypeInfo info;
            info.NetworkInterface = iface;
            info.Protocol = proto;
            info.Domain = Utility.PtrToString (domain);
            info.ServiceType = Utility.PtrToString (type);

            infos.Add (info);
            
            if (bevent == BrowserEvent.Added) {
                infos.Add (info);

                foreach (ServiceTypeInfoHandler handler in addListeners)
                    handler (this, info);
            } else {
                infos.Remove (info);

                foreach (ServiceTypeInfoHandler handler in removeListeners)
                    handler (this, info);
            }
        }
    }
}
