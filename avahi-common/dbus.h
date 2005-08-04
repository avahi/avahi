#ifndef foodbushfoo
#define foodbushfoo

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

AVAHI_C_DECL_BEGIN

#define AVAHI_DBUS_NAME "org.freedesktop.Avahi"
#define AVAHI_DBUS_INTERFACE_SERVER AVAHI_DBUS_NAME".Server"
#define AVAHI_DBUS_PATH_SERVER "/"
#define AVAHI_DBUS_INTERFACE_ENTRY_GROUP AVAHI_DBUS_NAME".EntryGroup"
#define AVAHI_DBUS_INTERFACE_DOMAIN_BROWSER AVAHI_DBUS_NAME".DomainBrowser"
#define AVAHI_DBUS_INTERFACE_SERVICE_TYPE_BROWSER AVAHI_DBUS_NAME".ServiceTypeBrowser"
#define AVAHI_DBUS_INTERFACE_SERVICE_BROWSER AVAHI_DBUS_NAME".ServiceBrowser"

#define AVAHI_DBUS_ERROR_INVALID_SERVICE "org.freedesktop.Avahi.InvalidServiceError"
#define AVAHI_DBUS_ERROR_INVALID_ADDRESS "org.freedesktop.Avahi.InvalidAddressError"
#define AVAHI_DBUS_ERROR_TIMEOUT "org.freedesktop.Avahi.TimeoutError"
#define AVAHI_DBUS_ERROR_TOO_MANY_CLIENTS "org.freedesktop.Avahi.TooManyClientsError"
#define AVAHI_DBUS_ERROR_TOO_MANY_OBJECTS "org.freedesktop.Avahi.TooManyObjectsError"
#define AVAHI_DBUS_ERROR_TOO_MANY_ENTRIES "org.freedesktop.Avahi.TooManyEntriesError"
#define AVAHI_DBUS_ERROR_OS "org.freedesktop.Avahi.OSError"

AVAHI_C_DECL_END

#endif
