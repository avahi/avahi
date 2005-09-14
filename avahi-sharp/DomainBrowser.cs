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
using System.Collections;
using System.Runtime.InteropServices;

namespace Avahi
{
    internal delegate void DomainBrowserCallback (IntPtr browser, int iface, Protocol proto, BrowserEvent bevent,
                                                  IntPtr domain, IntPtr userdata);

    public enum DomainBrowserType {
        Register,
        RegisterDefault,
        Browse,
        BrowseDefault,
        BrowseLegacy
    }

    public struct DomainInfo
    {
        public int NetworkInterface;
        public Protocol Protocol;
        public string Domain;
    }

    public delegate void DomainInfoHandler (object o, DomainInfo info);
    
    public class DomainBrowser : IDisposable
    {
        private IntPtr handle;
        private ArrayList infos = new ArrayList ();
        private Client client;
        private int iface;
        private Protocol proto;
        private string domain;
        private DomainBrowserType btype;
        private DomainBrowserCallback cb;

        private ArrayList addListeners = new ArrayList ();
        private ArrayList removeListeners = new ArrayList ();
        
        [DllImport ("avahi-client")]
        private static extern IntPtr avahi_domain_browser_new (IntPtr client, int iface, int proto,
                                                               IntPtr domain, int btype, DomainBrowserCallback cb,
                                                               IntPtr userdata);

        [DllImport ("avahi-client")]
        private static extern void avahi_domain_browser_free (IntPtr handle);

        public event DomainInfoHandler DomainAdded
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
        
        public event DomainInfoHandler DomainRemoved
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

        public DomainInfo[] Domains
        {
            get { return (DomainInfo[]) infos.ToArray (typeof (DomainInfo)); }
        }

        public DomainBrowser (Client client) : this (client, -1, Protocol.Unspecified, client.DomainName,
                                                     DomainBrowserType.Browse) {
        }
        
        public DomainBrowser (Client client, int iface, Protocol proto, string domain, DomainBrowserType btype)
        {
            this.client = client;
            this.iface = iface;
            this.proto = proto;
            this.domain = domain;
            this.btype = btype;
            cb = OnDomainBrowserCallback;
        }

        ~DomainBrowser ()
        {
            Dispose ();
        }

        public void Dispose ()
        {
            Stop (true);
        }

        private void Start ()
        {
            if (client.Handle == IntPtr.Zero && handle != IntPtr.Zero ||
                (addListeners.Count == 0 && removeListeners.Count == 0))
                return;

            lock (client) {
                IntPtr domainPtr = Utility.StringToPtr (domain);
                handle = avahi_domain_browser_new (client.Handle, iface, (int) proto, domainPtr, (int) btype,
                                                   cb, IntPtr.Zero);
                Utility.Free (domainPtr);
            }
        }

        private void Stop (bool force)
        {
            if (client.Handle != IntPtr.Zero && handle != IntPtr.Zero &&
                (force || (addListeners.Count == 0 && removeListeners.Count == 0))) {
                lock (client) {
                    avahi_domain_browser_free (handle);
                    handle = IntPtr.Zero;
                }
            }
        }

        private void OnDomainBrowserCallback (IntPtr browser, int iface, Protocol proto, BrowserEvent bevent,
                                              IntPtr domain, IntPtr userdata)
        {

            DomainInfo info;
            info.NetworkInterface = iface;
            info.Protocol = proto;
            info.Domain = Utility.PtrToString (domain);

            infos.Add (info);
            
            if (bevent == BrowserEvent.Added) {
                infos.Add (info);

                foreach (DomainInfoHandler handler in addListeners)
                    handler (this, info);
            } else {
                infos.Remove (info);

                foreach (DomainInfoHandler handler in removeListeners)
                    handler (this, info);
            }
        }
    }
}
