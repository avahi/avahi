
using System;
using System.Collections;
using System.Runtime.InteropServices;

namespace Avahi
{
    internal enum ResolverEvent {
        Found,
        Timeout
    }
    
    internal enum BrowserEvent {
        Added,
        Removed
    }
    
    internal delegate void ClientHandler (IntPtr client, ClientState state, IntPtr userData);

    public enum Protocol {
        Unspecified = 0,
        IPv4 = 2,
        IPv6 = 10
    }
    
    public enum ClientState {
        Invalid,
        Registering,
        Running,
        Collision,
        Disconnected = 100
    }
    
    public class Client : IDisposable
    {
        private IntPtr handle;

        [DllImport ("avahi-client")]
        private static extern IntPtr avahi_client_new (IntPtr poll, ClientHandler handler,
                                                       IntPtr userData, out int error);

        [DllImport ("avahi-client")]
        private static extern void avahi_client_free (IntPtr handle);

        [DllImport ("avahi-client")]
        private static extern IntPtr avahi_client_get_version_string (IntPtr handle);

        [DllImport ("avahi-client")]
        private static extern IntPtr avahi_client_get_host_name (IntPtr handle);

        [DllImport ("avahi-client")]
        private static extern IntPtr avahi_client_get_domain_name (IntPtr handle);

        [DllImport ("avahi-client")]
        private static extern IntPtr avahi_client_get_host_name_fqdn (IntPtr handle);

        [DllImport ("avahi-client")]
        private static extern ClientState avahi_client_get_state (IntPtr handle);

        [DllImport ("avahi-client")]
        private static extern int avahi_client_errno (IntPtr handle);
        
        [DllImport ("avahi-glib")]
        private static extern IntPtr avahi_glib_poll_new (IntPtr context, int priority);

        [DllImport ("avahi-glib")]
        private static extern IntPtr avahi_glib_poll_get (IntPtr gpoll);

        internal IntPtr Handle
        {
            get { return handle; }
        }
        
        public string Version
        {
            get { return Utility.PtrToString (avahi_client_get_version_string (handle)); }
        }

        public string HostName
        {
            get { return Utility.PtrToString (avahi_client_get_host_name (handle)); }
        }

        public string DomainName
        {
            get { return Utility.PtrToString (avahi_client_get_domain_name (handle)); }
        }

        public string HostNameFqdn
        {
            get { return Utility.PtrToString (avahi_client_get_host_name_fqdn (handle)); }
        }

        public ClientState State
        {
            get { return (ClientState) avahi_client_get_state (handle); }
        }

        internal int LastError
        {
            get { return avahi_client_errno (handle); }
        }

        public Client ()
        {
            IntPtr gpoll = avahi_glib_poll_new (IntPtr.Zero, 0);
            IntPtr poll = avahi_glib_poll_get (gpoll);

            int error;
            handle = avahi_client_new (poll, OnClientCallback, IntPtr.Zero, out error);
            if (error != 0)
                throw new ClientException (error);
        }

        ~Client ()
        {
            Dispose ();
        }

        public void Dispose ()
        {
            if (handle != IntPtr.Zero) {
                avahi_client_free (handle);
                handle = IntPtr.Zero;
            }
        }

        internal void CheckError ()
        {
            int error = LastError;

            if (error != 0)
                throw new ClientException (error);
        }
        
        private void OnClientCallback (IntPtr client, ClientState state, IntPtr userData)
        {
            Console.WriteLine ("Got new state: " + state);
        }
    }
}
