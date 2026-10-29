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
#include <net/net_rtmp_session.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

static uint8_t random_bytes[1528], input[65536], output[65536];
static size_t output_size;

static net_rtmp_session_t *make_session(const char *url, bool legacy)
{
   net_rtmp_publish_config_t c;
   unsigned i;
   c.application = "live";
   c.url = url;
   c.name = "test";
   c.legacy_commands = legacy;
   for (i = 0; i < sizeof(random_bytes); i++)
      random_bytes[i] = (uint8_t)(i * 17 + 3);
   return net_rtmp_session_new(&c, random_bytes, 0, sizeof(input), 8);
}

static bool pump(net_rtmp_session_t *s)
{
   unsigned iteration;
   bool any = false;
   for (iteration = 0; iteration < 100000; iteration++)
   {
      net_rtmp_span_t span, again;
      bool progress = net_rtmp_session_step(s);
      if (net_rtmp_session_get(s, &span))
      {
         size_t take = span.size < 11 ? span.size : 11;
         assert(net_rtmp_session_consume(s, 0));
         assert(net_rtmp_session_get(s, &again));
         assert(span.data == again.data && span.size == again.size);
         assert(output_size + take <= sizeof(output));
         memcpy(output + output_size, span.data, take);
         output_size += take;
         assert(net_rtmp_session_consume(s, take));
         progress = true;
      }
      any = any || progress;
      if (!progress)
         return any;
   }
   assert(!"session made unbounded local progress");
   return false;
}

static void write_uint(uint32_t value)
{
   uint8_t bytes[4];
   bytes[0] = (uint8_t)(value >> 24);
   bytes[1] = (uint8_t)(value >> 16);
   bytes[2] = (uint8_t)(value >> 8);
   bytes[3] = (uint8_t)value;
   assert(fwrite(bytes, 1, 4, stdout) == 4);
}

static void emit(net_rtmp_session_t *s)
{
   pump(s);
   write_uint((uint32_t)net_rtmp_session_status(s, NULL, NULL));
   write_uint((uint32_t)output_size);
   assert(fwrite(output, 1, output_size, stdout) == output_size);
   output_size = 0;
   assert(!fflush(stdout));
}

static void peer(const char *url, bool legacy)
{
   net_rtmp_session_t *s = make_session(url, legacy);
   uint8_t header[4];
   size_t size, at, used;
#ifdef _WIN32
   assert(_setmode(_fileno(stdin), _O_BINARY) != -1);
   assert(_setmode(_fileno(stdout), _O_BINARY) != -1);
#endif
   assert(s);
   emit(s);
   while (net_rtmp_session_status(s, NULL, NULL) != NET_RTMP_SESSION_READY)
   {
      assert(fread(header, 1, 4, stdin) == 4);
      size = ((uint32_t)header[0] << 24) | ((uint32_t)header[1] << 16)
         | ((uint32_t)header[2] << 8) | header[3];
      assert(size && size <= sizeof(input));
      assert(fread(input, 1, size, stdin) == size);
      for (at = 0; at < size; at += used)
      {
         size_t take = size - at < 7 ? size - at : 7;
         assert(net_rtmp_session_feed(s, input + at, take, &used, 0));
         if (!used)
            assert(pump(s));
         else
            pump(s);
      }
      emit(s);
   }
   assert(net_rtmp_session_free(s));
}

static void cancellation(void)
{
   net_rtmp_session_t *s = make_session("rtmp://localhost/live", false);
   net_rtmp_span_t span, again;
   uint64_t sent;
   assert(s && net_rtmp_session_get(s, &span));
   assert(!net_rtmp_session_discard(s));
   assert(net_rtmp_session_consume(s, 7));
   net_rtmp_session_cancel(s);
   assert(!net_rtmp_session_free(s));
   assert(net_rtmp_session_get(s, &again));
   assert(again.data == (const uint8_t*)span.data + 7);
   assert(again.size == span.size - 7);
   assert(net_rtmp_session_status(s, NULL, &sent) == NET_RTMP_SESSION_FAILED);
   assert(sent == 7);
   assert(net_rtmp_session_discard(s));
   assert(!net_rtmp_session_get(s, &again));
   assert(net_rtmp_session_free(s));
   assert(!net_rtmp_session_new(NULL, random_bytes, 0, 4096, 8));
   assert(!net_rtmp_session_new(NULL, NULL, 0, 0, 0));
}

static net_rtmp_session_t *negotiating(void)
{
   net_rtmp_session_t *s = make_session("rtmp://localhost/live", false);
   net_rtmp_span_t span;
   size_t at, used;
   assert(s && net_rtmp_session_get(s, &span));
   assert(span.size == 1537);
   memset(input, 0, 3073);
   input[0] = 3;
   memcpy(input + 1537, (const uint8_t*)span.data + 1, 1536);
   assert(net_rtmp_session_consume(s, span.size));
   for (at = 0; at < 3073; at += used)
   {
      assert(net_rtmp_session_feed(s, input + at, 3073 - at, &used, 0));
      assert(used);
   }
   assert(net_rtmp_session_get(s, &span) && span.size == 1536);
   assert(net_rtmp_session_consume(s, span.size));
   assert(net_rtmp_session_step(s));
   assert(net_rtmp_session_status(s, NULL, NULL) == NET_RTMP_SESSION_NEGOTIATING);
   assert(net_rtmp_session_step(s));
   return s;
}

static void internal_retirement(void)
{
   static const uint8_t connected[] =
      "\2\0\7_result\0\77\360\0\0\0\0\0\0\5\3"
      "\0\4code\2\0\35NetConnection.Connect.Success\0\0\11";
   net_rtmp_session_t *s = negotiating();
   net_rtmp_span_t span;
   size_t at, used, size = sizeof(connected) - 1;
   assert(net_rtmp_session_get(s, &span));
   assert(!net_rtmp_session_free(s));
   net_rtmp_session_cancel(s);
   assert(net_rtmp_session_discard(s));
   assert(net_rtmp_session_free(s));
   s = negotiating();
   output_size = 0;
   pump(s);
   memset(input, 0, 12);
   input[0] = 3;
   input[6] = (uint8_t)size;
   input[7] = 20;
   memcpy(input + 12, connected, size);
   for (at = 0; at < size + 12; at += used)
   {
      assert(net_rtmp_session_feed(s, input + at, size + 12 - at, &used, 0));
      assert(used);
   }
   assert(net_rtmp_session_step(s));
   net_rtmp_session_cancel(s);
   assert(net_rtmp_session_free(s));
   output_size = 0;
}

static void rejection(void)
{
   static const uint8_t rejected[] = "\2\0\6_error\0\77\360\0\0\0\0\0\0\5";
   net_rtmp_session_t *s = negotiating();
   size_t at, used, size = sizeof(rejected) - 1;
   output_size = 0;
   pump(s);
   memset(input, 0, 12);
   input[0] = 3;
   input[6] = (uint8_t)size;
   input[7] = 20;
   memcpy(input + 12, rejected, size);
   for (at = 0; at < size + 12; at += used)
   {
      assert(net_rtmp_session_feed(s, input + at, size + 12 - at, &used, 0));
      assert(used);
   }
   pump(s);
   assert(net_rtmp_session_status(s, NULL, NULL) == NET_RTMP_SESSION_FAILED);
   assert(net_rtmp_session_free(s));
   output_size = 0;
}

int main(int argc, char **argv)
{
   if (argc == 4 && !strcmp(argv[1], "--peer"))
   {
      peer(argv[2], !strcmp(argv[3], "legacy"));
      return 0;
   }
   assert(argc == 1);
   cancellation();
   internal_retirement();
   rejection();
   puts("PASS RTMP session cancellation, partial output and failed initialization");
   return 0;
}
