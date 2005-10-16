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

#include "howl.h"
#include "warn.h"

sw_string sw_strdup(sw_const_string str) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
    return NULL;
}

sw_opaque _sw_debug_malloc(
    sw_size_t size,
    sw_const_string function,
    sw_const_string file,
    sw_uint32 line) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
    return NULL;
}

sw_opaque _sw_debug_realloc(
   sw_opaque_t mem,
   sw_size_t size,
   sw_const_string function,
   sw_const_string file,
   sw_uint32 line) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
    return NULL;
}

void _sw_debug_free(
    sw_opaque_t mem,
    sw_const_string function,
    sw_const_string file,
    sw_uint32 line) {
    AVAHI_WARN_UNSUPPORTED;
}

sw_const_string sw_strerror(/* howl sucks */) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
    return NULL;
}

sw_result sw_timer_init(sw_timer * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_timer_fina(sw_timer self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_time_init(sw_time * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_time_init_now(sw_time * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_time_fina(sw_time self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_time sw_time_add(
    sw_time self,
    sw_time y) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

sw_time sw_time_sub(
  sw_time self,
  sw_time y) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

sw_int32 sw_time_cmp(
  sw_time self,
  sw_time y) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

sw_result sw_salt_init(
    sw_salt * self,
    int argc,
    char ** argv) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_salt_fina(sw_salt self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_salt_register_socket(
    sw_salt self,
    struct _sw_socket * socket,
    sw_socket_event events,
    sw_socket_handler handler,
    sw_socket_handler_func func,
    sw_opaque extra) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_salt_unregister_socket(
    sw_salt self,
    struct _sw_socket * socket) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}


sw_result sw_salt_register_timer(
    sw_salt self,
    struct _sw_timer * timer,
    sw_time timeout,
    sw_timer_handler handler,
    sw_timer_handler_func func,
    sw_opaque extra) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_salt_unregister_timer(
    sw_salt self,
    struct _sw_timer * timer) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_salt_register_network_interface(
    sw_salt self,
    struct _sw_network_interface * netif,
    sw_network_interface_handler handler,
    sw_network_interface_handler_func func,
    sw_opaque extra) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_salt_unregister_network_interface_handler(sw_salt self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_salt_register_signal(
    sw_salt self,
    struct _sw_signal * signal,
    sw_signal_handler handler,
    sw_signal_handler_func func,
    sw_opaque extra) {

    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_salt_unregister_signal(
    sw_salt self,
    struct _sw_signal * signal) {

    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

void sw_print_assert(
    int code,
    sw_const_string assert_string,
    sw_const_string file,
    sw_const_string func,
    int line) {
    AVAHI_WARN_UNSUPPORTED;
}

void sw_print_debug(
    int level,
    sw_const_string format,
    ...) {
    AVAHI_WARN_UNSUPPORTED;
}

sw_result sw_tcp_socket_init(sw_socket * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_tcp_socket_init_with_desc(
    sw_socket * self,
    sw_sockdesc_t desc) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_udp_socket_init(
    sw_socket * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_multicast_socket_init(
    sw_socket * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_fina(sw_socket self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_bind(
    sw_socket self,
    sw_ipv4_address address,
    sw_port port) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_join_multicast_group(
    sw_socket self,
    sw_ipv4_address local_address,
    sw_ipv4_address multicast_address,
    sw_uint32 ttl) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_leave_multicast_group(sw_socket self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_listen(
    sw_socket self,
    int qsize) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_connect(
    sw_socket self,
    sw_ipv4_address address,
    sw_port port) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_accept(
    sw_socket self,
    sw_socket * socket) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_send(
    sw_socket self,
    sw_octets buffer,
    sw_size_t len,
    sw_size_t * bytesWritten) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_sendto(
    sw_socket self,
    sw_octets buffer,
    sw_size_t len,
    sw_size_t * bytesWritten,
    sw_ipv4_address to,
    sw_port port) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_recv(
    sw_socket self,
    sw_octets buffer,
    sw_size_t max,
    sw_size_t * len) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_recvfrom(
    sw_socket self,
    sw_octets buffer,
    sw_size_t max,
    sw_size_t * len,
    sw_ipv4_address * from,
    sw_port * port,
    sw_ipv4_address * dest,
    sw_uint32 * interface_index) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_set_blocking_mode(
    sw_socket self,
    sw_bool blocking_mode) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_set_options(
    sw_socket self,
    sw_socket_options options) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_ipv4_address sw_socket_ipv4_address(sw_socket self) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

sw_port sw_socket_port(sw_socket self) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

sw_sockdesc_t sw_socket_desc(sw_socket self) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

sw_result sw_socket_close(sw_socket self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_init(sw_socket_options * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_fina(sw_socket_options self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_set_debug(
    sw_socket_options self,
    sw_bool val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_set_nodelay(
    sw_socket_options self,
    sw_bool val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_set_dontroute(
    sw_socket_options self,
    sw_bool val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_set_keepalive(
    sw_socket_options self,
    sw_bool val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_set_linger(
    sw_socket_options self,
    sw_bool onoff,
    sw_uint32 linger) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_set_reuseaddr(
    sw_socket_options self,
    sw_bool val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_set_rcvbuf(
    sw_socket_options self,
    sw_uint32 val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_socket_options_set_sndbuf(
    sw_socket_options self,
    sw_uint32 val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

int sw_socket_error_code(void) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

sw_result sw_corby_orb_init(
    sw_corby_orb * self,
    sw_salt salt,
    const sw_corby_orb_config * config,
    sw_corby_orb_observer observer,
    sw_corby_orb_observer_func func,
    sw_opaque_t extra) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_fina(sw_corby_orb self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_register_servant(
    sw_corby_orb self,
    sw_corby_servant servant,
    sw_corby_servant_cb cb,
    sw_const_string oid,
    struct _sw_corby_object ** object,
    sw_const_string protocol_name) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_unregister_servant(
    sw_corby_orb self,
    sw_const_string oid) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_register_bidirectional_object(
    sw_corby_orb self,
    struct _sw_corby_object * object) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_register_channel(
    sw_corby_orb self,
    struct _sw_corby_channel * channel) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_corby_orb_delegate sw_corby_orb_get_delegate(sw_corby_orb self) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

sw_result sw_corby_orb_set_delegate(
    sw_corby_orb self,
    sw_corby_orb_delegate delegate) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_set_observer(
    sw_corby_orb self,
    sw_corby_orb_observer observer,
    sw_corby_orb_observer_func func,
    sw_opaque_t extra) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_protocol_to_address(
    sw_corby_orb self,
    sw_const_string tag,
    sw_string addr,
    sw_port * port) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_protocol_to_url(
    sw_corby_orb self,
    sw_const_string tag,
    sw_const_string name,
    sw_string url,
    sw_size_t url_len) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_read_channel(
    sw_corby_orb self,
    struct _sw_corby_channel * channel) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_orb_dispatch_message(
    sw_corby_orb self,
    struct _sw_corby_channel * channel,
    struct _sw_corby_message * message,
    struct _sw_corby_buffer * buffer,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_message_init(sw_corby_message * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_message_fina(sw_corby_message self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_init(sw_corby_buffer * self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_init_with_size(
    sw_corby_buffer * self,
    sw_size_t size) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_init_with_delegate(
    sw_corby_buffer * self,
    sw_corby_buffer_delegate delegate,
    sw_corby_buffer_overflow_func overflow,
    sw_corby_buffer_underflow_func underflow,
    sw_opaque_t extra) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_init_with_size_and_delegate(
    sw_corby_buffer * self,
    sw_size_t size,
    sw_corby_buffer_delegate delegate,
    sw_corby_buffer_overflow_func overflow,
    sw_corby_buffer_underflow_func underflow,
    sw_opaque_t extra) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_fina(sw_corby_buffer self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}    

void sw_corby_buffer_reset(sw_corby_buffer self) {
    AVAHI_WARN_UNSUPPORTED;
}

sw_result sw_corby_buffer_set_octets(
    sw_corby_buffer self,
    sw_octets octets,
    sw_size_t size) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_octets sw_corby_buffer_octets(sw_corby_buffer self) {
    AVAHI_WARN_UNSUPPORTED;
    return NULL;
}

sw_size_t sw_corby_buffer_bytes_used(sw_corby_buffer self) {
    AVAHI_WARN_UNSUPPORTED;
    return 0;
}

sw_size_t sw_corby_buffer_size(sw_corby_buffer self) {
    AVAHI_WARN_UNSUPPORTED;
    return 0;
}

sw_result sw_corby_buffer_put_int8(sw_corby_buffer self, sw_int8 val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_put_uint8(
    sw_corby_buffer self,
    sw_uint8 val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_put_int16(
    sw_corby_buffer self,
    sw_int16 val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_put_uint16(
    sw_corby_buffer self,
    sw_uint16 val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_put_int32(
    sw_corby_buffer self,
    sw_int32 val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_put_uint32(
    sw_corby_buffer self,
    sw_uint32 val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_put_octets(
    sw_corby_buffer self,
    sw_const_octets val,
    sw_size_t size) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_put_sized_octets(
    sw_corby_buffer self,
    sw_const_octets val,
    sw_uint32 len) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}
    
sw_result sw_corby_buffer_put_cstring(
    sw_corby_buffer self,
    sw_const_string val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_put_object(
    sw_corby_buffer self,
    const struct _sw_corby_object * object) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_put_pad(
    sw_corby_buffer self,
    sw_corby_buffer_pad pad) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_int8(
    sw_corby_buffer self,
    sw_int8 * val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_uint8(
    sw_corby_buffer self,
    sw_uint8 * val) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_int16(
    sw_corby_buffer self,
    sw_int16 * val,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}
    
sw_result sw_corby_buffer_get_uint16(
    sw_corby_buffer self,
    sw_uint16 * val,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}
    
sw_result sw_corby_buffer_get_int32(
    sw_corby_buffer self,
    sw_int32 * val,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_uint32(
    sw_corby_buffer self,
    sw_uint32 * val,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_octets(
    sw_corby_buffer self,
    sw_octets octets,
    sw_size_t size) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_allocate_and_get_sized_octets(
    sw_corby_buffer self,
    sw_octets * val,
    sw_uint32 * size,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_zerocopy_sized_octets(
    sw_corby_buffer self,
    sw_octets * val,
    sw_uint32 * size,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_sized_octets(
    sw_corby_buffer self,
    sw_octets val,
    sw_uint32 * len,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_allocate_and_get_cstring(
    sw_corby_buffer self,
    sw_string * val,
    sw_uint32 * len,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_zerocopy_cstring(
    sw_corby_buffer self,
    sw_string * val,
    sw_uint32 * len,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_cstring(
    sw_corby_buffer self,
    sw_string val,
    sw_uint32 * len,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_buffer_get_object(
    sw_corby_buffer self,
    struct _sw_corby_object ** object,
    sw_uint8 endian) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_channel_start_request(
    sw_corby_channel self,
    sw_const_corby_profile profile,
    struct _sw_corby_buffer ** buffer,
    sw_const_string op,
    sw_uint32 oplen,
    sw_bool reply_expected) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}
    
sw_result sw_corby_channel_start_reply(
    sw_corby_channel self,
    struct _sw_corby_buffer ** buffer,
    sw_uint32 request_id,
    sw_corby_reply_status status) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_channel_send(
    sw_corby_channel self,
    struct _sw_corby_buffer * buffer,
    sw_corby_buffer_observer observer,
    sw_corby_buffer_written_func func,
    sw_opaque_t extra) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_channel_recv(
    sw_corby_channel self,
    sw_salt * salt,
    struct _sw_corby_message ** message,
    sw_uint32 * request_id,
    sw_string * op,
    sw_uint32 * op_len,
    struct _sw_corby_buffer ** buffer,
    sw_uint8 * endian,
    sw_bool block) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_channel_last_recv_from(
    sw_corby_channel self,
    sw_ipv4_address * from,
    sw_port * from_port) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_channel_ff(
    sw_corby_channel self,
    struct _sw_corby_buffer * buffer) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_socket sw_corby_channel_socket(sw_corby_channel self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_channel_retain(sw_corby_channel self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_channel_set_delegate(
    sw_corby_channel self,
    sw_corby_channel_delegate delegate) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}    

sw_corby_channel_delegate sw_corby_channel_get_delegate(
    sw_corby_channel self) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

void sw_corby_channel_set_app_data(
    sw_corby_channel self,
    sw_opaque app_data) {
    AVAHI_WARN_UNSUPPORTED;
}

sw_opaque sw_corby_channel_get_app_data(sw_corby_channel self) {
    AVAHI_WARN_UNSUPPORTED_ABORT;
}

sw_result sw_corby_channel_fina(sw_corby_channel self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_object_init_from_url(
    sw_corby_object * self,
    struct _sw_corby_orb * orb,
    sw_const_string url,
    sw_socket_options options,
    sw_uint32 bufsize) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_object_fina(
    sw_corby_object self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_object_start_request(
    sw_corby_object self,
    sw_const_string op,
    sw_uint32 op_len,
    sw_bool reply_expected,
    sw_corby_buffer * buffer) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_object_send(
    sw_corby_object self,
    sw_corby_buffer buffer,
    sw_corby_buffer_observer observer,
    sw_corby_buffer_written_func func,
    sw_opaque extra) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}
    
sw_result sw_corby_object_recv(
    sw_corby_object self,
    sw_corby_message * message,
    sw_corby_buffer * buffer,
    sw_uint8 * endian,
    sw_bool block) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_object_channel(
    sw_corby_object self,
    sw_corby_channel * channel) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_corby_object_set_channel(
    sw_corby_object self,
    sw_corby_channel channel) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_discovery_publish_host(
    sw_discovery self,
    sw_uint32 interface_index,
    sw_const_string name,
    sw_const_string domain,
    sw_ipv4_address address,
    sw_discovery_publish_reply reply,
    sw_opaque extra,
    sw_discovery_oid * oid) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_discovery_publish_update(
    sw_discovery self,
    sw_discovery_oid oid,
    sw_octets text_record,
    sw_uint32 text_record_len) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_discovery_query_record(
    sw_discovery self,
    sw_uint32 interface_index,
    sw_uint32 flags,
    sw_const_string fullname,
    sw_uint16 rrtype,
    sw_uint16 rrclass,
    sw_discovery_query_record_reply reply,
    sw_opaque extra,
    sw_discovery_oid * oid) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_text_record_string_iterator_init(
    sw_text_record_string_iterator * self,
    sw_const_string text_record_string) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}

sw_result sw_text_record_string_iterator_fina(
    sw_text_record_string_iterator self) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}    

sw_result sw_text_record_string_iterator_next(
    sw_text_record_string_iterator self,
    char key[255],
    char val[255]) {
    AVAHI_WARN_UNSUPPORTED;
    return SW_E_NO_IMPL;
}
