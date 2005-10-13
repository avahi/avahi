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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <howl.h>

#include "warn.h"

sw_result sw_text_record_init(
    sw_text_record * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}

sw_result sw_text_record_fina(
    sw_text_record self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}

sw_result sw_text_record_add_string(
    sw_text_record self,
    sw_const_string string) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}

sw_result sw_text_record_add_key_and_string_value(
    sw_text_record self,
    sw_const_string key,
    sw_const_string val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}

sw_result sw_text_record_add_key_and_binary_value(
    sw_text_record self,
    sw_const_string key,
    sw_octets val,
    sw_uint32 len) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}

sw_octets sw_text_record_bytes(sw_text_record self) {
    AVAHI_WARN_UNSUPPORTED;
    return NULL;
}

sw_uint32 sw_text_record_len(sw_text_record self) {
    AVAHI_WARN_UNSUPPORTED;
    return 0;
}

sw_result sw_text_record_iterator_init(
    sw_text_record_iterator * self,
    sw_octets text_record,
    sw_uint32 text_record_len) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}

sw_result sw_text_record_iterator_fina(
    sw_text_record_iterator self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}

sw_result sw_text_record_iterator_next(
    sw_text_record_iterator self,
    char key[255],
    sw_uint8 val[255],
    sw_uint32 * val_len) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}

sw_result sw_text_record_string_iterator_init(
    sw_text_record_string_iterator * self,
    sw_const_string text_record_string) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}

sw_result sw_text_record_string_iterator_fina(
    sw_text_record_string_iterator self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}    

sw_result sw_text_record_string_iterator_next(
    sw_text_record_string_iterator self,
    char key[255],
    char val[255]) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_DISCOVERY_E_NOT_SUPPORTED;
}
