#ifndef fooerrorhfoo
#define fooerrorhfoo

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

/** \file error.h Error codes and auxiliary functions */

#include <avahi-common/cdecl.h>

AVAHI_C_DECL_BEGIN

/** Error codes used by avahi */
enum { 
    AVAHI_OK = 0,                          /**< OK */
    AVAHI_ERR_FAILURE = -1,                /**< Generic error code */
    AVAHI_ERR_BAD_STATE = -2,              /**< Object was in a bad state */
    AVAHI_ERR_INVALID_HOST_NAME = -3,      /**< Invalid host name */
    AVAHI_ERR_INVALID_DOMAIN_NAME = -4,    /**< Invalid domain name */
    AVAHI_ERR_NO_NETWORK = -5,             /**< No suitable network protocol available */
    AVAHI_ERR_INVALID_TTL = -6,            /**< Invalid DNS TTL */
    AVAHI_ERR_IS_PATTERN = -7,             /**< RR key is pattern */
    AVAHI_ERR_LOCAL_COLLISION = -8,        /**< Local name collision */
    AVAHI_ERR_INVALID_RECORD = -9,         /**< Invalid RR */
    AVAHI_ERR_INVALID_SERVICE_NAME = -10,  /**< Invalid service name */
    AVAHI_ERR_INVALID_SERVICE_TYPE = -11,  /**< Invalid service type */
    AVAHI_ERR_INVALID_PORT = -12,          /**< Invalid port number */
    AVAHI_ERR_INVALID_KEY = -13,           /**< Invalid key */
    AVAHI_ERR_INVALID_ADDRESS = -14,       /**< Invalid address */
    AVAHI_ERR_TIMEOUT = -15,               /**< Timeout reached */
    AVAHI_ERR_TOO_MANY_CLIENTS = -16,      /**< Too many clients */
    AVAHI_ERR_TOO_MANY_OBJECTS = -17,      /**< Too many objects */
    AVAHI_ERR_TOO_MANY_ENTRIES = -18,      /**< Too many entries */
    AVAHI_ERR_OS = -19,                    /**< OS error */
    AVAHI_ERR_ACCESS_DENIED = -20,         /**< Access denied */
    AVAHI_ERR_INVALID_OPERATION = -21,     /**< Invalid operation */
    AVAHI_ERR_DBUS_ERROR = -22,            /**< An unexpected DBUS error occured */
    AVAHI_ERR_NOT_CONNECTED = -23,         /**< Could not get a connection to the daemon */
    AVAHI_ERR_NO_MEMORY = -24,             /**< Memory exhausted */
    AVAHI_ERR_INVALID_OBJECT = -25,        /**< The object passed to this function was invalid */
    AVAHI_ERR_NO_DAEMON = -26,             /**< Daemon not running */

    /****
     ****    IF YOU ADD A NEW ERROR CODE HERE, PLEASE DON'T FORGET TO ADD
     ****    IT TO THE STRING ARRAY IN avahi_strerror() IN error.c AND
     ****    TO THE ARRAY IN respond_error() IN dbus-protocol.c
     ****    AND FINALLY TO dbus.h!
     ****
     ****    Also remember to update the MAX value below.
     ****/
    
    AVAHI_ERR_MAX = -27
};

/** Return a human readable error string for the specified error code */
const char *avahi_strerror(int error);

AVAHI_C_DECL_END

#endif
