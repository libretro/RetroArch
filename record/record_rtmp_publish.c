/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_rtmp_publish.h"
#include <stdlib.h>
#include <string.h>

enum publish_wait { PUBLISH_CONNECT, PUBLISH_CREATE, PUBLISH_START };
struct record_rtmp_publish
{
   record_rtmp_command_config_t config;
   record_rtmp_reply_t *reply;
   char *strings;
   size_t output_size;
   uint32_t stream, input_stream, create_transaction;
   enum record_rtmp_publish_state state;
   enum record_rtmp_publish_error error;
   enum record_rtmp_command command;
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

static void publish_fail(record_rtmp_publish_t *p, enum record_rtmp_publish_error error)
{
   if (p->state != RECORD_RTMP_PUBLISH_FAILED)
   {
      p->state = RECORD_RTMP_PUBLISH_FAILED;
      p->error = error;
   }
}

static bool publish_command(record_rtmp_publish_t *p, enum record_rtmp_command command)
{
   if (!record_rtmp_command_encode(command, &p->config,
            p->output, sizeof(p->output), &p->output_size))
   {
      publish_fail(p, RECORD_RTMP_PUBLISH_ERROR_PROTOCOL);
      return false;
   }
   p->command = command;
   p->pending = true;
   return true;
}

record_rtmp_publish_t *record_rtmp_publish_new(const record_rtmp_publish_config_t *c)
{
   record_rtmp_publish_t *p;
   size_t app, url, name;
   if (!c || !publish_length(c->application, &app) || !publish_length(c->url, &url)
         || !publish_length(c->name, &name))
      return NULL;
   p = (record_rtmp_publish_t*)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   p->strings = (char*)malloc(app + url + name + 3);
   p->reply = record_rtmp_reply_new();
   if (!p->strings || !p->reply)
   {
      record_rtmp_publish_free(p);
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
   if (!publish_command(p, RECORD_RTMP_CONNECT))
   {
      record_rtmp_publish_free(p);
      return NULL;
   }
   return p;
}

bool record_rtmp_publish_get(record_rtmp_publish_t *p, record_rtmp_message_t *message)
{
   if (!p || !message || !p->pending || (p->state == RECORD_RTMP_PUBLISH_FAILED && !p->borrowed))
      return false;
   message->payload.data = p->output;
   message->payload.size = p->output_size;
   message->timestamp = 0;
   message->stream_id = p->command == RECORD_RTMP_PUBLISH ? p->stream : 0;
   message->chunk_id = 3;
   message->type = 20;
   p->borrowed = true;
   return true;
}

bool record_rtmp_publish_ack(record_rtmp_publish_t *p)
{
   if (!p || !p->pending || !p->borrowed)
      return false;
   p->pending = p->borrowed = false;
   if (p->state == RECORD_RTMP_PUBLISH_FAILED)
      return true;
   switch (p->command)
   {
      case RECORD_RTMP_CONNECT:
         p->wait = PUBLISH_CONNECT;
         break;
      case RECORD_RTMP_RELEASE_STREAM:
         p->config.transaction = 3;
         return publish_command(p, RECORD_RTMP_FC_PUBLISH);
      case RECORD_RTMP_FC_PUBLISH:
         p->config.transaction = p->create_transaction;
         return publish_command(p, RECORD_RTMP_CREATE_STREAM);
      case RECORD_RTMP_CREATE_STREAM:
         p->wait = PUBLISH_CREATE;
         break;
      case RECORD_RTMP_PUBLISH:
         p->wait = PUBLISH_START;
         break;
      default:
         publish_fail(p, RECORD_RTMP_PUBLISH_ERROR_PROTOCOL);
         return false;
   }
   return true;
}

bool record_rtmp_publish_reply(record_rtmp_publish_t *p, const record_rtmp_message_t *message)
{
   if (!p || !message || p->input || p->pending || p->state == RECORD_RTMP_PUBLISH_FAILED
         || (message->type != 20 && message->type != 17))
      return false;
   if (!record_rtmp_reply_begin(p->reply, message->type, message->payload.data, message->payload.size))
   {
      publish_fail(p, RECORD_RTMP_PUBLISH_ERROR_PROTOCOL);
      return false;
   }
   p->input_stream = message->stream_id;
   p->input = true;
   return true;
}

static bool publish_text(record_rtmp_span_t text, const char *value)
{
   size_t size = strlen(value);
   return text.size == size && !memcmp(text.data, value, size);
}

static void publish_transition(record_rtmp_publish_t *p, const record_rtmp_reply_info_t *r)
{
   bool result = publish_text(r->command, "_result");
   bool error = publish_text(r->command, "_error") || publish_text(r->level, "error");
   uint32_t expected = p->wait == PUBLISH_CONNECT ? 1 : p->create_transaction;
   if (p->state == RECORD_RTMP_PUBLISH_NEGOTIATING && p->wait != PUBLISH_START
         && !p->input_stream && r->transaction == expected)
   {
      if (error)
         publish_fail(p, RECORD_RTMP_PUBLISH_ERROR_REJECTED);
      else if (result && p->wait == PUBLISH_CONNECT)
      {
         if (!publish_text(r->code, "NetConnection.Connect.Success"))
            publish_fail(p, RECORD_RTMP_PUBLISH_ERROR_PROTOCOL);
         else
         {
            p->config.transaction = p->legacy ? 2 : p->create_transaction;
            publish_command(p, p->legacy ? RECORD_RTMP_RELEASE_STREAM : RECORD_RTMP_CREATE_STREAM);
         }
      }
      else if (result)
      {
         if (!r->stream_valid || !r->stream)
            publish_fail(p, RECORD_RTMP_PUBLISH_ERROR_PROTOCOL);
         else
         {
            p->stream = r->stream;
            p->config.stream = r->stream;
            publish_command(p, RECORD_RTMP_PUBLISH);
         }
      }
      return;
   }
   if (p->stream && p->input_stream == p->stream && !r->transaction
         && (publish_text(r->command, "onStatus") || publish_text(r->command, "_error")))
   {
      if (error)
         publish_fail(p, RECORD_RTMP_PUBLISH_ERROR_REJECTED);
      else if (p->wait == PUBLISH_START && publish_text(r->code, "NetStream.Publish.Start"))
         p->state = RECORD_RTMP_PUBLISH_READY;
   }
}

int record_rtmp_publish_step(record_rtmp_publish_t *p)
{
   record_rtmp_reply_info_t info;
   int result;
   if (!p)
      return RECORD_RTMP_PUBLISH_ERROR;
   if (p->input)
   {
      if (p->state != RECORD_RTMP_PUBLISH_FAILED)
      {
         result = record_rtmp_reply_step(p->reply);
         if (result == RECORD_RTMP_REPLY_PROGRESS)
            return RECORD_RTMP_PUBLISH_PROGRESS;
         if (result == RECORD_RTMP_REPLY_ERROR || !record_rtmp_reply_get(p->reply, &info))
            publish_fail(p, RECORD_RTMP_PUBLISH_ERROR_PROTOCOL);
         else
            publish_transition(p, &info);
      }
      record_rtmp_reply_release(p->reply);
      p->input = false;
      return RECORD_RTMP_PUBLISH_RELEASE_REPLY;
   }
   if (p->state == RECORD_RTMP_PUBLISH_FAILED)
      return RECORD_RTMP_PUBLISH_ERROR;
   return p->state == RECORD_RTMP_PUBLISH_READY ? RECORD_RTMP_PUBLISH_ESTABLISHED
      : RECORD_RTMP_PUBLISH_BUSY;
}

enum record_rtmp_publish_state record_rtmp_publish_status(const record_rtmp_publish_t *p,
      uint32_t *stream, enum record_rtmp_publish_error *error)
{
   if (stream)
      *stream = p ? p->stream : 0;
   if (error)
      *error = p ? p->error : RECORD_RTMP_PUBLISH_ERROR_PROTOCOL;
   return p ? p->state : RECORD_RTMP_PUBLISH_FAILED;
}

void record_rtmp_publish_cancel(record_rtmp_publish_t *p)
{
   if (p)
      publish_fail(p, RECORD_RTMP_PUBLISH_ERROR_CANCELLED);
}

bool record_rtmp_publish_free(record_rtmp_publish_t *p)
{
   if (!p)
      return true;
   if (p->borrowed || p->input)
      return false;
   record_rtmp_reply_free(p->reply);
   free(p->strings);
   free(p);
   return true;
}
