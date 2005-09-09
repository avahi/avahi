
using System;
using System.Runtime.InteropServices;

namespace Avahi
{
    public class ClientException : ApplicationException
    {
        private int code;

        [DllImport ("avahi-common")]
        private static extern IntPtr avahi_strerror (int code);
        
        public int ErrorCode
        {
            get { return code; }
        }
        
        internal ClientException (int code) : base (GetErrorString (code))
        {
            this.code = code;
        }

        private static string GetErrorString (int code)
        {
            IntPtr str = avahi_strerror (code);
            return Utility.PtrToString (str);
        }
    }
}
