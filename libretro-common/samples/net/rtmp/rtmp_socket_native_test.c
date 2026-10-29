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
#include <net/net_rtmp_socket.h>
#include <net/net_socket.h>
#include <features/features_cpu.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t random_bytes[1528];

int main(int argc, char **argv)
{
   net_rtmp_publish_config_t config;
   net_rtmp_session_t *session;
   net_rtmp_socket_t *transport;
   void *address = NULL;
   retro_time_t deadline;
   uint64_t sent;
   uint32_t stream;
   unsigned i, port;
   int fd, result;
   assert(argc == 4 && network_init());
   port = (unsigned)strtoul(argv[2], NULL, 10);
   assert(port && port <= 65535);
   fd = socket_init(&address, (uint16_t)port, "127.0.0.1", SOCKET_TYPE_STREAM, AF_INET);
   assert(fd >= 0 && address);
   result = socket_connect(fd, address);
   freeaddrinfo_retro((struct addrinfo*)address);
   if (result < 0)
   {
      socket_close(fd);
      return 77;
   }
   config.application = "live";
   config.url = argv[1];
   config.name = "test";
   config.legacy_commands = !strcmp(argv[3], "legacy");
   for (i = 0; i < sizeof(random_bytes); i++)
      random_bytes[i] = (uint8_t)(i * 13 + 1);
   session = net_rtmp_session_new(&config, random_bytes, 0, 65536, 8);
   assert(session);
   transport = net_rtmp_socket_new(session, fd);
   assert(transport);
   deadline = cpu_features_get_time_usec() + 10000000;
   while (net_rtmp_session_status(session, &stream, &sent) != NET_RTMP_SESSION_READY)
   {
      bool read = true, write = false;
      assert(cpu_features_get_time_usec() < deadline);
      result = net_rtmp_socket_step(transport, 0);
      assert(result != NET_RTMP_SOCKET_ERROR);
      if (result == NET_RTMP_SOCKET_IDLE)
         socket_wait(fd, &read, &write, 10);
   }
   assert(stream && sent > 3073);
   net_rtmp_socket_free(transport);
   socket_close(fd);
   assert(net_rtmp_session_free(session));
   puts("PASS native nonblocking TCP publish session");
   return 0;
}
