/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_socket.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _LIBRETRO_SDK_NET_SOCKET_SSL_H
#define _LIBRETRO_SDK_NET_SOCKET_SSL_H

#include <stdlib.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

void* ssl_socket_init(int fd, const char *domain);

int ssl_socket_connect(void *state_data, void *data, bool timeout_enable, bool nonblock);

/* The TLS library's own code for the most recent failure in
 * ssl_socket_init()/ssl_socket_connect(), as the library reports it
 * (mbedtls: negative, so -0x7780 style; BearSSL: its BR_ERR_* value),
 * or 0 when the last failure was in the socket layer or there was
 * none.  For logging: it is what turns "connect failed" into a
 * message the library's error table can decode. */
int ssl_socket_last_error(void *state_data);

int ssl_socket_send_all_blocking(void *state_data, const void *data_, size_t len, bool no_signal);

ssize_t ssl_socket_send_all_nonblocking(void *state_data, const void *data_, size_t len, bool no_signal);

int ssl_socket_receive_all_blocking(void *state_data, void *data_, size_t len);

ssize_t ssl_socket_receive_all_nonblocking(void *state_data, bool *error, void *data_, size_t len);

void ssl_socket_close(void *state_data);

void ssl_socket_free(void *state_data);

/* Certificate-verification policy hook. `mode` is a tls_verify_mode value
 * (0 = required, 1 = optional, 2 = disabled, see network/tls_config.h).
 * Called once at startup and whenever the setting changes; the active
 * backend snapshots the value at the next ssl_socket_connect. */
void ssl_socket_set_verify_mode(unsigned mode);

/* Weak logging hooks. The active SSL backend ships no-op defaults so
 * libretro-common still builds/links standalone; RetroArch overrides them
 * in network/tls_log.c to route into RARCH_ERR/RARCH_WARN without dragging
 * verbosity.h into vendored code. `mode_required` != 0 means a hard
 * (REQUIRED) failure; == 0 means a soft (OPTIONAL) failure. */
void ssl_socket_log_verify_fail(int mode_required, const char *domain,
      const char *verify_info);
void ssl_socket_log_verify_disabled(const char *domain);

RETRO_END_DECLS

#endif
