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

static void send_media(net_rtmp_session_t *session, net_rtmp_socket_t *transport,
      int fd, const char *path)
{
   FILE *file = fopen(path, "rb");
   uint8_t header[13], previous[4];
   uint8_t *payload = (uint8_t*)malloc(1048576);
   net_rtmp_span_t span;
   retro_time_t deadline = cpu_features_get_time_usec() + 10000000;
   size_t size;
   uint32_t timestamp, last_size;
   unsigned count = 0;
   assert(file && payload);
   assert(fread(header, 1, 13, file) == 13);
   assert(!memcmp(header, "FLV\1", 4) && header[8] == 9);
   while ((size = fread(header, 1, 11, file)) != 0)
   {
      assert(size == 11 && ++count < 4096);
      size = ((size_t)header[1] << 16) | ((size_t)header[2] << 8) | header[3];
      timestamp = ((uint32_t)header[7] << 24) | ((uint32_t)header[4] << 16)
         | ((uint32_t)header[5] << 8) | header[6];
      assert(size && size <= 1048576 && !header[8] && !header[9] && !header[10]);
      assert(fread(payload, 1, size, file) == size);
      assert(fread(previous, 1, 4, file) == 4);
      last_size = ((uint32_t)previous[0] << 24) | ((uint32_t)previous[1] << 16)
         | ((uint32_t)previous[2] << 8) | previous[3];
      assert(last_size == size + 11);
      span.data = payload;
      span.size = size;
      while (!net_rtmp_session_media(session, header[0], timestamp, &span, 1))
      {
         assert(cpu_features_get_time_usec() < deadline);
         assert(net_rtmp_socket_step(transport, 0) != NET_RTMP_SOCKET_ERROR);
      }
      while (!net_rtmp_session_media_release(session))
      {
         bool read = true, write = false;
         int result;
         if (cpu_features_get_time_usec() >= deadline)
         {
            uint64_t sent;
            net_rtmp_session_status(session, NULL, &sent);
            fprintf(stderr, "media deadline: tag %u, sent %lu\n", count,
                  (unsigned long)sent);
            assert(!"media transfer deadline");
         }
         result = net_rtmp_socket_step(transport, 0);
         assert(result != NET_RTMP_SOCKET_ERROR);
         if (result == NET_RTMP_SOCKET_IDLE)
            socket_wait(fd, &read, &write, 10);
      }
   }
   assert(!ferror(file) && count && !fclose(file));
   free(payload);
}

static void finish_transport(int fd)
{
   uint8_t input[256];
   bool error = false;
   retro_time_t deadline = cpu_features_get_time_usec() + 5000000;
#ifdef _WIN32
   assert(!shutdown(fd, SD_SEND));
#else
   assert(!shutdown(fd, SHUT_WR));
#endif
   while (!error)
   {
      bool read = true, write = false;
      assert(cpu_features_get_time_usec() < deadline);
      socket_receive_all_nonblocking(fd, &error, input, sizeof(input));
      if (!error)
         socket_wait(fd, &read, &write, 10);
   }
}

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
   assert((argc == 4 || argc == 5) && network_init());
   port = (unsigned)strtoul(argv[2], NULL, 10);
   assert(port && port <= 65535);
   fd = socket_init(&address, (uint16_t)port, "127.0.0.1", SOCKET_TYPE_STREAM, AF_INET);
   assert(fd >= 0 && address);
   result = socket_connect_begin(fd, address);
   deadline = cpu_features_get_time_usec() + 5000000;
   while (result == SOCKET_CONNECT_PENDING)
   {
      bool read = false, write = true;
      assert(cpu_features_get_time_usec() < deadline);
      result = socket_connect_step(fd, address);
      if (result == SOCKET_CONNECT_PENDING)
         socket_wait(fd, &read, &write, 10);
   }
   freeaddrinfo_retro((struct addrinfo*)address);
   if (result != SOCKET_CONNECT_READY)
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
   if (argc == 5)
   {
      assert(net_rtmp_session_chunk_size(session, 4096));
      send_media(session, transport, fd, argv[4]);
      finish_transport(fd);
   }
   net_rtmp_socket_free(transport);
   socket_close(fd);
   assert(net_rtmp_session_free(session));
   puts("PASS native nonblocking TCP publish session");
   return 0;
}
