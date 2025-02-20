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

int ssl_socket_send_all_blocking(void *state_data, const void *data_, size_t len, bool no_signal);

ssize_t ssl_socket_send_all_nonblocking(void *state_data, const void *data_, size_t len, bool no_signal);

int ssl_socket_receive_all_blocking(void *state_data, void *data_, size_t len);

ssize_t ssl_socket_receive_all_nonblocking(void *state_data, bool *error, void *data_, size_t len);

void ssl_socket_close(void *state_data);

void ssl_socket_free(void *state_data);

RETRO_END_DECLS

#endif
