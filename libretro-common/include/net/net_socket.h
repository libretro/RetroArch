/* Copyright  (C) 2010-2018 The RetroArch team
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

#ifndef _LIBRETRO_SDK_NET_SOCKET_H
#define _LIBRETRO_SDK_NET_SOCKET_H

#include <stdint.h>
#include <boolean.h>
#include <string.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum socket_domain
{
   SOCKET_DOMAIN_INET = 0
};

enum socket_type
{
   SOCKET_TYPE_DATAGRAM = 0,
   SOCKET_TYPE_STREAM,
   SOCKET_TYPE_SEQPACKET
};

enum socket_protocol
{
   SOCKET_PROTOCOL_NONE = 0,
   SOCKET_PROTOCOL_TCP,
   SOCKET_PROTOCOL_UDP
};

typedef struct socket_target
{
   unsigned port;
   const char *server;
   enum socket_domain domain;
   enum socket_protocol prot;
} socket_target_t;

int socket_init(void **address, uint16_t port, const char *server, enum socket_type type);

int socket_next(void **address);

int socket_close(int fd);

bool socket_nonblock(int fd);

int socket_select(int nfds, fd_set *readfs, fd_set *writefds,
      fd_set *errorfds, struct timeval *timeout);

int socket_send_all_blocking(int fd, const void *data_, size_t size, bool no_signal);

ssize_t socket_send_all_nonblocking(int fd, const void *data_, size_t size,
      bool no_signal);

int socket_receive_all_blocking(int fd, void *data_, size_t size);

ssize_t socket_receive_all_nonblocking(int fd, bool *error,
      void *data_, size_t size);

bool socket_bind(int fd, void *data);

int socket_connect(int fd, void *data, bool timeout_enable);

int socket_create(
      const char *name,
      enum socket_domain domain_type,
      enum socket_type socket_type,
      enum socket_protocol protocol_type);

void socket_set_target(void *data, socket_target_t *in_addr);

RETRO_END_DECLS

#endif
