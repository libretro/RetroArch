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
#include <stdlib.h>
#include <string.h>

enum publish_wait { PUBLISH_CONNECT, PUBLISH_CREATE, PUBLISH_START };
struct net_rtmp_publish
{
   net_rtmp_command_config_t config;
   net_rtmp_reply_t *reply;
   char *strings;
   size_t output_size;
   uint32_t stream, input_stream, create_transaction;
   enum net_rtmp_publish_state state;
   enum net_rtmp_publish_error error;
   enum net_rtmp_command command;
   enum publish_wait wait;
   uint8_t output[9000];
   bool pending, borrowed, input, legacy;
};

static bool publish_length(const char *s, size_t *length)
{
   size_t n = 0;
   if (!s || !*s)
      return false;
   while (n <= 4096 && s[n])
      n++;
   *length = n;
   return n <= 4096;
}

static void publish_fail(net_rtmp_publish_t *p, enum net_rtmp_publish_error error)
{
   if (p->state != NET_RTMP_PUBLISH_FAILED)
   {
      p->state = NET_RTMP_PUBLISH_FAILED;
      p->error = error;
   }
}

static bool publish_command(net_rtmp_publish_t *p, enum net_rtmp_command command)
{
   if (!net_rtmp_command_encode(command, &p->config,
            p->output, sizeof(p->output), &p->output_size))
   {
      publish_fail(p, NET_RTMP_PUBLISH_ERROR_PROTOCOL);
      return false;
   }
   p->command = command;
   p->pending = true;
   return true;
}

net_rtmp_publish_t *net_rtmp_publish_new(const net_rtmp_publish_config_t *c)
{
   net_rtmp_publish_t *p;
   size_t app, url, name;
   if (!c || !publish_length(c->application, &app) || !publish_length(c->url, &url)
         || !publish_length(c->name, &name))
      return NULL;
   p = (net_rtmp_publish_t*)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   p->strings = (char*)malloc(app + url + name + 3);
   p->reply = net_rtmp_reply_new();
   if (!p->strings || !p->reply)
   {
      net_rtmp_publish_free(p);
      return NULL;
   }
   memcpy(p->strings, c->application, app + 1);
   memcpy(p->strings + app + 1, c->url, url + 1);
   memcpy(p->strings + app + url + 2, c->name, name + 1);
   p->config.application = p->strings;
   p->config.url = p->strings + app + 1;
   p->config.name = p->strings + app + url + 2;
   p->legacy = c->legacy_commands;
   p->create_transaction = p->legacy ? 4 : 2;
   p->config.transaction = 1;
   if (!publish_command(p, NET_RTMP_CONNECT))
   {
      net_rtmp_publish_free(p);
      return NULL;
   }
   return p;
}

bool net_rtmp_publish_get(net_rtmp_publish_t *p, net_rtmp_message_t *message)
{
   if (!p || !message || !p->pending || (p->state == NET_RTMP_PUBLISH_FAILED && !p->borrowed))
      return false;
   message->payload.data = p->output;
   message->payload.size = p->output_size;
   message->timestamp = 0;
   message->stream_id = p->command == NET_RTMP_PUBLISH ? p->stream : 0;
   message->chunk_id = 3;
   message->type = 20;
   p->borrowed = true;
   return true;
}

bool net_rtmp_publish_ack(net_rtmp_publish_t *p)
{
   if (!p || !p->pending || !p->borrowed)
      return false;
   p->pending = p->borrowed = false;
   if (p->state == NET_RTMP_PUBLISH_FAILED)
      return true;
   switch (p->command)
   {
      case NET_RTMP_CONNECT:
         p->wait = PUBLISH_CONNECT;
         break;
      case NET_RTMP_RELEASE_STREAM:
         p->config.transaction = 3;
         return publish_command(p, NET_RTMP_FC_PUBLISH);
      case NET_RTMP_FC_PUBLISH:
         p->config.transaction = p->create_transaction;
         return publish_command(p, NET_RTMP_CREATE_STREAM);
      case NET_RTMP_CREATE_STREAM:
         p->wait = PUBLISH_CREATE;
         break;
      case NET_RTMP_PUBLISH:
         p->wait = PUBLISH_START;
         break;
      default:
         publish_fail(p, NET_RTMP_PUBLISH_ERROR_PROTOCOL);
         return false;
   }
   return true;
}

bool net_rtmp_publish_reply(net_rtmp_publish_t *p, const net_rtmp_message_t *message)
{
   if (!p || !message || p->input || p->pending || p->state == NET_RTMP_PUBLISH_FAILED
         || (message->type != 20 && message->type != 17))
      return false;
   if (!net_rtmp_reply_begin(p->reply, message->type, message->payload.data, message->payload.size))
   {
      publish_fail(p, NET_RTMP_PUBLISH_ERROR_PROTOCOL);
      return false;
   }
   p->input_stream = message->stream_id;
   p->input = true;
   return true;
}

static bool publish_text(net_rtmp_span_t text, const char *value)
{
   size_t size = strlen(value);
   return text.size == size && !memcmp(text.data, value, size);
}

static void publish_transition(net_rtmp_publish_t *p, const net_rtmp_reply_info_t *r)
{
   bool result = publish_text(r->command, "_result");
   bool error = publish_text(r->command, "_error") || publish_text(r->level, "error");
   uint32_t expected = p->wait == PUBLISH_CONNECT ? 1 : p->create_transaction;
   if (p->state == NET_RTMP_PUBLISH_NEGOTIATING && p->wait != PUBLISH_START
         && !p->input_stream && r->transaction == expected)
   {
      if (error)
         publish_fail(p, NET_RTMP_PUBLISH_ERROR_REJECTED);
      else if (result && p->wait == PUBLISH_CONNECT)
      {
         if (!publish_text(r->code, "NetConnection.Connect.Success"))
            publish_fail(p, NET_RTMP_PUBLISH_ERROR_PROTOCOL);
         else
         {
            p->config.transaction = p->legacy ? 2 : p->create_transaction;
            publish_command(p, p->legacy ? NET_RTMP_RELEASE_STREAM : NET_RTMP_CREATE_STREAM);
         }
      }
      else if (result)
      {
         if (!r->stream_valid || !r->stream)
            publish_fail(p, NET_RTMP_PUBLISH_ERROR_PROTOCOL);
         else
         {
            p->stream = r->stream;
            p->config.stream = r->stream;
            publish_command(p, NET_RTMP_PUBLISH);
         }
      }
      return;
   }
   if (p->stream && p->input_stream == p->stream && !r->transaction
         && (publish_text(r->command, "onStatus") || publish_text(r->command, "_error")))
   {
      if (error)
         publish_fail(p, NET_RTMP_PUBLISH_ERROR_REJECTED);
      else if (p->wait == PUBLISH_START && publish_text(r->code, "NetStream.Publish.Start"))
         p->state = NET_RTMP_PUBLISH_READY;
   }
}

int net_rtmp_publish_step(net_rtmp_publish_t *p)
{
   net_rtmp_reply_info_t info;
   int result;
   if (!p)
      return NET_RTMP_PUBLISH_ERROR;
   if (p->input)
   {
      if (p->state != NET_RTMP_PUBLISH_FAILED)
      {
         result = net_rtmp_reply_step(p->reply);
         if (result == NET_RTMP_REPLY_PROGRESS)
            return NET_RTMP_PUBLISH_PROGRESS;
         if (result == NET_RTMP_REPLY_ERROR || !net_rtmp_reply_get(p->reply, &info))
            publish_fail(p, NET_RTMP_PUBLISH_ERROR_PROTOCOL);
         else
            publish_transition(p, &info);
      }
      net_rtmp_reply_release(p->reply);
      p->input = false;
      return NET_RTMP_PUBLISH_RELEASE_REPLY;
   }
   if (p->state == NET_RTMP_PUBLISH_FAILED)
      return NET_RTMP_PUBLISH_ERROR;
   return p->state == NET_RTMP_PUBLISH_READY ? NET_RTMP_PUBLISH_ESTABLISHED
      : NET_RTMP_PUBLISH_BUSY;
}

enum net_rtmp_publish_state net_rtmp_publish_status(const net_rtmp_publish_t *p,
      uint32_t *stream, enum net_rtmp_publish_error *error)
{
   if (stream)
      *stream = p ? p->stream : 0;
   if (error)
      *error = p ? p->error : NET_RTMP_PUBLISH_ERROR_PROTOCOL;
   return p ? p->state : NET_RTMP_PUBLISH_FAILED;
}

void net_rtmp_publish_cancel(net_rtmp_publish_t *p)
{
   if (p)
      publish_fail(p, NET_RTMP_PUBLISH_ERROR_CANCELLED);
}

bool net_rtmp_publish_free(net_rtmp_publish_t *p)
{
   if (!p)
      return true;
   if (p->borrowed || p->input)
      return false;
   net_rtmp_reply_free(p->reply);
   free(p->strings);
   free(p);
   return true;
}
