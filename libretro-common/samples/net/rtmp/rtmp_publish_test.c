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
#include <net/net_rtmp_publish.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t connected[] =
   "\2\0\7_result\0\77\360\0\0\0\0\0\0\5\3"
   "\0\4code\2\0\35NetConnection.Connect.Success\0\0\11";
static const uint8_t created[] =
{ 2, 0, 7, '_', 'r', 'e', 's', 'u', 'l', 't',
   0, 64, 0, 0, 0, 0, 0, 0, 0, 5, 0, 64, 49, 0, 0, 0, 0, 0, 0 };
static const uint8_t started[] =
   "\2\0\10onStatus\0\0\0\0\0\0\0\0\0\5\3"
   "\0\4code\2\0\27NetStream.Publish.Start\0\0\11";
static const uint8_t rejected[] =
   "\2\0\10onStatus\0\0\0\0\0\0\0\0\0\5\3"
   "\0\5level\2\0\5error\0\0\11";
static const uint8_t connection_error[] = "\2\0\6_error\0\77\360\0\0\0\0\0\0\5";

static net_rtmp_publish_t *make_publish(bool legacy)
{
   net_rtmp_publish_config_t c;
   char app[] = "live", url[] = "rtmp://localhost/live", name[] = "test";
   net_rtmp_publish_t *p;
   c.application = app;
   c.url = url;
   c.name = name;
   c.legacy_commands = legacy;
   p = net_rtmp_publish_new(&c);
   assert(p);
   memset(app, 0, sizeof(app));
   memset(url, 0, sizeof(url));
   memset(name, 0, sizeof(name));
   return p;
}

static void output(net_rtmp_publish_t *p, const char *command, uint32_t stream)
{
   net_rtmp_message_t message, again;
   const uint8_t *bytes;
   size_t size = strlen(command);
   assert(net_rtmp_publish_get(p, &message));
   assert(net_rtmp_publish_get(p, &again));
   assert(message.payload.data == again.payload.data && message.payload.size == again.payload.size);
   assert(message.stream_id == stream && message.type == 20 && message.timestamp == 0 && message.chunk_id == 3);
   bytes = (const uint8_t*)message.payload.data;
   assert(bytes[0] == 2 && bytes[1] == 0 && bytes[2] == size);
   assert(!memcmp(bytes + 3, command, size));
   if (!strcmp(command, "publish"))
      assert(message.payload.size >= 14
            && !memcmp(bytes + message.payload.size - 14, "\2\0\4test\2\0\4live", 14));
   if (!strcmp(command, "releaseStream") || !strcmp(command, "FCPublish"))
      assert(message.payload.size >= 7
            && !memcmp(bytes + message.payload.size - 7, "\2\0\4test", 7));
   assert(!net_rtmp_publish_free(p));
   assert(net_rtmp_publish_ack(p));
}

static void input(net_rtmp_publish_t *p, const void *data, size_t size, uint32_t stream)
{
   net_rtmp_message_t m;
   unsigned i;
   int result = NET_RTMP_PUBLISH_PROGRESS;
   m.payload.data = data;
   m.payload.size = size;
   m.stream_id = stream;
   m.type = 20;
   m.chunk_id = 3;
   m.timestamp = 0;
   assert(net_rtmp_publish_reply(p, &m));
   assert(!net_rtmp_publish_reply(p, &m));
   assert(!net_rtmp_publish_free(p));
   for (i = 0; i < 512 && result == NET_RTMP_PUBLISH_PROGRESS; i++)
      result = net_rtmp_publish_step(p);
   assert(result == NET_RTMP_PUBLISH_RELEASE_REPLY);
}

static void lifecycle(bool legacy)
{
   net_rtmp_publish_t *p = make_publish(legacy);
   net_rtmp_message_t m;
   enum net_rtmp_publish_error error;
   uint32_t stream;
   uint8_t reply[sizeof(created)];
   assert(net_rtmp_publish_step(p) == NET_RTMP_PUBLISH_BUSY);
   output(p, "connect", 0);
   input(p, connected, sizeof(connected) - 1, 0);
   if (legacy)
   {
      output(p, "releaseStream", 0);
      output(p, "FCPublish", 0);
   }
   output(p, "createStream", 0);
   input(p, connected, sizeof(connected) - 1, 0);
   assert(!net_rtmp_publish_get(p, &m));
   memcpy(reply, created, sizeof(reply));
   if (legacy)
      reply[12] = 16;
   input(p, reply, sizeof(reply), 0);
   output(p, "publish", 17);
   input(p, started, sizeof(started) - 1, 18);
   assert(net_rtmp_publish_status(p, &stream, &error) == NET_RTMP_PUBLISH_NEGOTIATING);
   input(p, started, sizeof(started) - 1, 17);
   assert(net_rtmp_publish_step(p) == NET_RTMP_PUBLISH_ESTABLISHED);
   assert(net_rtmp_publish_status(p, &stream, &error) == NET_RTMP_PUBLISH_READY);
   assert(stream == 17 && error == NET_RTMP_PUBLISH_ERROR_NONE);
   input(p, rejected, sizeof(rejected) - 1, 18);
   assert(net_rtmp_publish_step(p) == NET_RTMP_PUBLISH_ESTABLISHED);
   input(p, rejected, sizeof(rejected) - 1, 17);
   net_rtmp_publish_cancel(p);
   assert(net_rtmp_publish_step(p) == NET_RTMP_PUBLISH_ERROR);
   assert(net_rtmp_publish_status(p, &stream, &error) == NET_RTMP_PUBLISH_FAILED);
   assert(error == NET_RTMP_PUBLISH_ERROR_REJECTED);
   assert(net_rtmp_publish_free(p));
}

static void cancellation(void)
{
   net_rtmp_publish_t *p = make_publish(false);
   net_rtmp_message_t m, again;
   uint8_t saved[512];
   assert(net_rtmp_publish_get(p, &m) && m.payload.size < sizeof(saved));
   memcpy(saved, m.payload.data, m.payload.size);
   net_rtmp_publish_cancel(p);
   assert(net_rtmp_publish_step(p) == NET_RTMP_PUBLISH_ERROR);
   assert(net_rtmp_publish_get(p, &again));
   assert(again.payload.data == m.payload.data && !memcmp(again.payload.data, saved, m.payload.size));
   assert(!net_rtmp_publish_free(p) && net_rtmp_publish_ack(p));
   assert(net_rtmp_publish_free(p));
   p = make_publish(false);
   output(p, "connect", 0);
   m.payload.data = connected;
   m.payload.size = sizeof(connected) - 1;
   m.type = 20;
   m.stream_id = m.timestamp = 0;
   m.chunk_id = 3;
   assert(net_rtmp_publish_reply(p, &m));
   assert(net_rtmp_publish_step(p) == NET_RTMP_PUBLISH_PROGRESS);
   net_rtmp_publish_cancel(p);
   assert(!net_rtmp_publish_free(p));
   assert(net_rtmp_publish_step(p) == NET_RTMP_PUBLISH_RELEASE_REPLY);
   assert(net_rtmp_publish_step(p) == NET_RTMP_PUBLISH_ERROR);
   assert(net_rtmp_publish_free(p));
}

static void failures(void)
{
   net_rtmp_publish_t *p = make_publish(false);
   enum net_rtmp_publish_error error;
   uint8_t reply[sizeof(created)];
   output(p, "connect", 0);
   input(p, connection_error, sizeof(connection_error) - 1, 0);
   assert(net_rtmp_publish_status(p, NULL, &error) == NET_RTMP_PUBLISH_FAILED);
   assert(error == NET_RTMP_PUBLISH_ERROR_REJECTED && net_rtmp_publish_free(p));
   p = make_publish(false);
   output(p, "connect", 0);
   input(p, connected, sizeof(connected) - 1, 0);
   output(p, "createStream", 0);
   memcpy(reply, created, sizeof(reply));
   memset(reply + 21, 0, 8);
   input(p, reply, sizeof(reply), 0);
   assert(net_rtmp_publish_status(p, NULL, &error) == NET_RTMP_PUBLISH_FAILED);
   assert(error == NET_RTMP_PUBLISH_ERROR_PROTOCOL && net_rtmp_publish_free(p));
   p = make_publish(false);
   output(p, "connect", 0);
   input(p, "\377", 1, 0);
   assert(net_rtmp_publish_step(p) == NET_RTMP_PUBLISH_ERROR);
   assert(net_rtmp_publish_free(p));
   assert(!net_rtmp_publish_new(NULL) && net_rtmp_publish_free(NULL));
}

int main(void)
{
   lifecycle(false);
   lifecycle(true);
   cancellation();
   failures();
   puts("PASS: RTMP publish negotiation, stale replies, rejection and cancellation borrows");
   return 0;
}
