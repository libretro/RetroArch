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
#include <net/net_rtmp_handshake.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static uint8_t random_bytes[1528], server[3076], client[3073];

static net_rtmp_handshake_t *setup(void)
{
   net_rtmp_handshake_t *h;
   net_rtmp_span_t s;
   unsigned i;
   for (i = 0; i < 1528; i++)
      random_bytes[i] = (uint8_t)(i * 13 + 7);
   h = net_rtmp_handshake_new(random_bytes, 0x12345678);
   assert(h && net_rtmp_handshake_get(h, &s));
   assert(s.size == 1537);
   memcpy(client, s.data, s.size);
   assert(client[0] == 3 && client[1] == 18 && client[4] == 120);
   assert(!memcmp(client + 5, "\0\0\0\0", 4));
   assert(!memcmp(client + 9, random_bytes, 1528));
   memset(random_bytes, 0, sizeof(random_bytes));
   memset(server, 0, sizeof(server));
   server[0] = 3;
   server[1] = 1;
   server[4] = 2;
   for (i = 9; i < 1537; i++)
      server[i] = (uint8_t)(i * 17 + 3);
   memcpy(server + 1537, client + 1, 1536);
   memset(server + 1541, 99, 4);
   server[3073] = 44;
   return h;
}

static void send_output(net_rtmp_handshake_t *h, size_t *sent)
{
   net_rtmp_span_t s, again;
   size_t n;
   while (net_rtmp_handshake_get(h, &s))
   {
      assert(!net_rtmp_handshake_consume(h, s.size + 1));
      assert(net_rtmp_handshake_consume(h, 0));
      assert(net_rtmp_handshake_get(h, &again));
      assert(again.data == s.data && again.size == s.size);
      n = s.size < 23 ? s.size : 23;
      assert(*sent + n <= sizeof(client));
      memcpy(client + *sent, s.data, n);
      assert(net_rtmp_handshake_consume(h, n));
      *sent += n;
   }
}

static void ordering(bool receive_first, bool fragmented)
{
   net_rtmp_handshake_t *h = setup();
   size_t sent = 0, pos = 0, n, used;
   int result;
   if (!receive_first)
   {
      send_output(h, &sent);
      assert(sent == 1537);
   }
   while (pos < 3073)
   {
      n = fragmented ? 1 + pos % 37 : sizeof(server) - pos;
      if (n > sizeof(server) - pos)
         n = sizeof(server) - pos;
      result = net_rtmp_handshake_feed(h, server + pos, n, &used, 0x87654321);
      assert(result != NET_RTMP_HANDSHAKE_ERROR && used && used <= n);
      pos += used;
      if (!receive_first)
         send_output(h, &sent);
   }
   assert(pos == 3073 && server[pos] == 44);
   if (receive_first)
   {
      assert(net_rtmp_handshake_status(h) == NET_RTMP_HANDSHAKE_PENDING);
      send_output(h, &sent);
   }
   assert(sent == sizeof(client));
   assert(!memcmp(client + 1537, server + 1, 4));
   assert(!memcmp(client + 1541, "\207\145\103\41", 4));
   assert(!memcmp(client + 1545, server + 9, 1528));
   assert(net_rtmp_handshake_status(h) == NET_RTMP_HANDSHAKE_DONE);
   assert(net_rtmp_handshake_feed(h, server + pos, 3, &used, 0) == NET_RTMP_HANDSHAKE_DONE);
   assert(used == 0);
   net_rtmp_handshake_free(h);
}

static void corrupt(unsigned offset)
{
   net_rtmp_handshake_t *h = setup();
   net_rtmp_span_t s;
   size_t pos = 0, used;
   int result = 0;
   server[offset] ^= 1;
   while (pos < 3073 && result != NET_RTMP_HANDSHAKE_ERROR)
   {
      result = net_rtmp_handshake_feed(h, server + pos, 3073 - pos, &used, 0);
      assert(used);
      pos += used;
   }
   assert(result == NET_RTMP_HANDSHAKE_ERROR);
   assert(net_rtmp_handshake_status(h) == NET_RTMP_HANDSHAKE_ERROR);
   assert(!net_rtmp_handshake_get(h, &s));
   assert(net_rtmp_handshake_feed(h, NULL, 0, &used, 0) == NET_RTMP_HANDSHAKE_ERROR);
   assert(!used);
   net_rtmp_handshake_free(h);
}

static void peer_output(net_rtmp_handshake_t *h)
{
   net_rtmp_span_t s;
   while (net_rtmp_handshake_get(h, &s))
   {
      assert(fwrite(s.data, 1, s.size, stdout) == s.size);
      assert(net_rtmp_handshake_consume(h, s.size));
   }
   assert(!fflush(stdout));
}

static int peer(void)
{
   net_rtmp_handshake_t *h;
   size_t i, pos, used, size;
   unsigned part;
#ifdef _WIN32
   assert(_setmode(_fileno(stdin), _O_BINARY) != -1);
   assert(_setmode(_fileno(stdout), _O_BINARY) != -1);
#endif
   for (i = 0; i < sizeof(random_bytes); i++)
      random_bytes[i] = (uint8_t)(i * 13 + 7);
   h = net_rtmp_handshake_new(random_bytes, 0);
   assert(h);
   peer_output(h);
   for (part = 0; part < 2; part++)
   {
      size = part ? 1536 : 1537;
      assert(fread(server, 1, size, stdin) == size);
      for (pos = 0; pos < size; pos += used)
      {
         assert(net_rtmp_handshake_feed(h, server + pos, size - pos,
                  &used, 0) != NET_RTMP_HANDSHAKE_ERROR);
         assert(used);
      }
      peer_output(h);
   }
   assert(net_rtmp_handshake_status(h) == NET_RTMP_HANDSHAKE_DONE);
   net_rtmp_handshake_free(h);
   return 0;
}

int main(int argc, char **argv)
{
   if (argc == 2 && !strcmp(argv[1], "--peer"))
      return peer();
   assert(argc == 1);
   assert(!net_rtmp_handshake_new(NULL, 0));
   ordering(false, false);
   ordering(false, true);
   ordering(true, false);
   ordering(true, true);
   corrupt(0);
   corrupt(5);
   corrupt(1537);
   corrupt(1545);
   corrupt(3072);
   puts("PASS: fragmented RTMP handshake, output ordering, echo and trailing bytes");
   return 0;
}
