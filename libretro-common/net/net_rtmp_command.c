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
#include <net/net_rtmp_command.h>
#include <string.h>

struct command_writer
{
   uint8_t *data;
   size_t capacity, used;
   bool failed;
};

static void command_bytes(struct command_writer *w, const void *data, size_t size)
{
   if (w->failed || size > w->capacity - w->used)
   {
      w->failed = true;
      return;
   }
   memcpy(w->data + w->used, data, size);
   w->used += size;
}

static void command_marker(struct command_writer *w, uint8_t marker)
{
   command_bytes(w, &marker, 1);
}

static void command_string(struct command_writer *w, const char *text, bool property)
{
   size_t size = 0;
   uint8_t length[2];
   if (!text)
   {
      w->failed = true;
      return;
   }
   while (size <= 4096 && text[size])
      size++;
   if (size > 4096)
   {
      w->failed = true;
      return;
   }
   if (!property)
      command_marker(w, 2);
   length[0] = (uint8_t)(size >> 8);
   length[1] = (uint8_t)size;
   command_bytes(w, length, 2);
   command_bytes(w, text, size);
}

static void command_number(struct command_writer *w, uint32_t value)
{
   uint64_t bits = 0;
   uint32_t high = value;
   unsigned bit = 0, i;
   uint8_t encoded[9];
   if (value)
   {
      while (high >>= 1)
         bit++;
      bits = ((uint64_t)(1023 + bit) << 52)
         | (((uint64_t)value << (52 - bit)) & (((uint64_t)1 << 52) - 1));
   }
   encoded[0] = 0;
   for (i = 0; i < 8; i++)
      encoded[i + 1] = (uint8_t)(bits >> (56 - i * 8));
   command_bytes(w, encoded, sizeof(encoded));
}

static void command_property(struct command_writer *w, const char *name, const char *value)
{
   command_string(w, name, true);
   command_string(w, value, false);
}

bool net_rtmp_command_encode(enum net_rtmp_command command,
      const net_rtmp_command_config_t *c, void *output, size_t capacity, size_t *size)
{
   static const char *const names[] =
   { "connect", "createStream", "publish", "deleteStream", "releaseStream", "FCPublish" };
   struct command_writer w;
   uint32_t transaction;
   if (size)
      *size = 0;
   if (!size || !c || !output || !capacity || command < NET_RTMP_CONNECT
         || command > NET_RTMP_FC_PUBLISH)
      return false;
   transaction = command == NET_RTMP_CONNECT ? 1
      : command == NET_RTMP_PUBLISH || command == NET_RTMP_DELETE_STREAM ? 0 : c->transaction;
   if ((command == NET_RTMP_CREATE_STREAM || command == NET_RTMP_RELEASE_STREAM
            || command == NET_RTMP_FC_PUBLISH) && !transaction)
      return false;
   if (command == NET_RTMP_DELETE_STREAM && !c->stream)
      return false;
   if ((command == NET_RTMP_PUBLISH || command == NET_RTMP_RELEASE_STREAM
            || command == NET_RTMP_FC_PUBLISH) && (!c->name || !*c->name))
      return false;
   if (command == NET_RTMP_CONNECT && (!c->application || !*c->application || !c->url || !*c->url))
      return false;
   w.data = (uint8_t*)output;
   w.capacity = capacity;
   w.used = 0;
   w.failed = false;
   command_string(&w, names[command], false);
   command_number(&w, transaction);
   if (command == NET_RTMP_CONNECT)
   {
      command_marker(&w, 3);
      command_property(&w, "app", c->application);
      command_property(&w, "tcUrl", c->url);
      command_property(&w, "flashVer", "FMLE/3.0 (compatible; RetroArch)");
      command_string(&w, "fpad", true);
      command_bytes(&w, "\1\0", 2);
      command_string(&w, "objectEncoding", true);
      command_number(&w, 0);
      command_bytes(&w, "\0\0\11", 3);
   }
   else
   {
      command_marker(&w, 5);
      if (command == NET_RTMP_PUBLISH || command == NET_RTMP_RELEASE_STREAM
            || command == NET_RTMP_FC_PUBLISH)
         command_string(&w, c->name, false);
      if (command == NET_RTMP_PUBLISH)
         command_string(&w, "live", false);
      if (command == NET_RTMP_DELETE_STREAM)
         command_number(&w, c->stream);
   }
   if (w.failed)
      return false;
   *size = w.used;
   return true;
}
