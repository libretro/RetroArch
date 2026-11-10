/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#define socket_nonblock fake_nonblock
#define socket_receive_all_nonblocking fake_receive
#define socket_send_all_nonblocking fake_send
#ifdef HAVE_SSL
#define ssl_socket_receive_all_nonblocking fake_tls_receive
#define ssl_socket_send_all_nonblocking fake_tls_send
#define ssl_socket_flush_nonblocking fake_tls_flush
#endif
#include "../../../net/net_rtmp_socket.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t wire[4096], random_bytes[1528], incoming[3073];
static size_t sent, write_limit, incoming_size;
static unsigned reads, writes;
static const void *stalled;
static bool configure_fail, eof, send_fail;
#ifdef HAVE_SSL
static unsigned tls_reads, tls_writes, flushes;
static int flush_result;
static unsigned tls_token;
#endif

bool fake_nonblock(int fd)
{
   assert(fd == 99);
   return !configure_fail;
}

ssize_t fake_receive(int fd, bool *error, void *data, size_t size)
{
   assert(fd == 99 && size <= 4096);
   reads++;
   if (eof)
   {
      *error = true;
      return -1;
   }
   if (incoming_size)
   {
      size_t count = incoming_size;
      assert(count <= size);
      memcpy(data, incoming, count);
      incoming_size = 0;
      return (ssize_t)count;
   }
   return 0;
}

ssize_t fake_send(int fd, const void *data, size_t size, bool no_signal)
{
   size_t count = size < write_limit ? size : write_limit;
   assert(fd == 99 && no_signal && size <= 4096);
   writes++;
   if (stalled)
      assert(stalled == data);
   stalled = count ? NULL : data;
   if (send_fail)
      return -1;
   assert(sent + count <= sizeof(wire));
   memcpy(wire + sent, data, count);
   sent += count;
   return (ssize_t)count;
}

#ifdef HAVE_SSL
ssize_t fake_tls_receive(void *ssl, bool *error, void *data, size_t size)
{
   assert(ssl == &tls_token);
   tls_reads++;
   return fake_receive(99, error, data, size);
}

ssize_t fake_tls_send(void *ssl, const void *data, size_t size, bool no_signal)
{
   assert(ssl == &tls_token);
   tls_writes++;
   return fake_send(99, data, size, no_signal);
}

int fake_tls_flush(void *ssl)
{
   assert(ssl == &tls_token);
   flushes++;
   return flush_result;
}
#endif

static net_rtmp_session_t *make_session(void)
{
   net_rtmp_publish_config_t c;
   c.application = "live";
   c.url = "rtmp://localhost/live";
   c.name = "test";
   c.legacy_commands = false;
   sent = reads = writes = 0;
   stalled = NULL;
   write_limit = 7;
   incoming_size = 0;
   configure_fail = eof = send_fail = false;
   memset(incoming, 0, sizeof(incoming));
   return net_rtmp_session_new(&c, random_bytes, 0, 4096, 8);
}

static void backpressure(void)
{
   net_rtmp_session_t *session = make_session();
   net_rtmp_socket_t *socket = net_rtmp_socket_new(session, 99);
   unsigned iterations, count;
   assert(session && socket);
   write_limit = 0;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_IDLE);
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_IDLE);
   assert(sent == 0 && !net_rtmp_session_free(session));
   write_limit = 7;
   for (iterations = 0; sent < 1537 && iterations < 1000; iterations++)
      assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_PROGRESS);
   assert(sent == 1537 && wire[0] == 3);
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_IDLE);
   eof = true;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_ERROR);
   count = reads + writes;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_ERROR);
   assert(count == reads + writes);
   net_rtmp_socket_free(socket);
   assert(net_rtmp_session_free(session));
}

static void failures(void)
{
   net_rtmp_session_t *session = make_session();
   net_rtmp_socket_t *socket;
   assert(session);
   configure_fail = true;
   assert(!net_rtmp_socket_new(session, 99));
   assert(net_rtmp_session_status(session, NULL, NULL) == NET_RTMP_SESSION_HANDSHAKE);
   configure_fail = false;
   socket = net_rtmp_socket_new(session, 99);
   assert(socket);
   send_fail = true;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_ERROR);
   net_rtmp_socket_free(socket);
   assert(net_rtmp_session_free(session));
   session = make_session();
   socket = net_rtmp_socket_new(session, 99);
   assert(session && socket);
   incoming[0] = 4;
   incoming_size = 1;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_ERROR);
   net_rtmp_socket_free(socket);
   assert(net_rtmp_session_free(session));
}

static void retained_input(void)
{
   net_rtmp_session_t *session = make_session();
   net_rtmp_socket_t *socket = net_rtmp_socket_new(session, 99);
   unsigned i;
   assert(session && socket);
   incoming[0] = 3;
   incoming_size = sizeof(incoming);
   for (i = 0; i < 3; i++)
      assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_PROGRESS);
   assert(reads == 1);
   net_rtmp_socket_free(socket);
   assert(net_rtmp_session_free(session));
}

#ifdef HAVE_SSL
static void tls_backpressure(void)
{
   net_rtmp_session_t *session = make_session();
   net_rtmp_socket_t *socket;
   unsigned before;
   assert(session && !net_rtmp_socket_new_tls(session, 99, NULL));
   socket = net_rtmp_socket_new_tls(session, 99, &tls_token);
   assert(socket);
   tls_reads = tls_writes = flushes = 0;
   flush_result = 0;
   write_limit = 0;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_IDLE);
   assert(tls_reads == 1 && tls_writes == 1 && !flushes);
   write_limit = 7;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_PROGRESS);
   assert(sent == 7 && tls_writes == 2);
   before = tls_writes;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_IDLE);
   assert(tls_writes == before && flushes == 1 && sent == 7);
   assert(!net_rtmp_socket_flush(socket));
   assert(tls_writes == before && flushes == 2);
   flush_result = 1;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_PROGRESS);
   assert(tls_writes == before + 1 && sent == 14);
   flush_result = -1;
   assert(net_rtmp_socket_flush(socket) == -1);
   before = reads + writes + flushes;
   assert(net_rtmp_socket_step(socket, 0) == NET_RTMP_SOCKET_ERROR);
   assert(net_rtmp_socket_flush(socket) == -1);
   assert(reads + writes + flushes == before);
   assert(net_rtmp_session_status(session, NULL, NULL) == NET_RTMP_SESSION_FAILED);
   net_rtmp_socket_free(socket);
   assert(net_rtmp_session_free(session));
   puts("PASS RTMPS borrowed TLS, stalled write retry, bounded flush and cancellation");
}
#endif

int main(void)
{
#ifdef HAVE_SSL
   tls_backpressure();
#endif
   backpressure();
   failures();
   retained_input();
   puts("PASS RTMP socket backpressure, short writes, retained input, EOF and retirement");
   return 0;
}
