/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_rtmp_reply.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t stream_reply[] =
{ 2, 0, 7, '_', 'r', 'e', 's', 'u', 'l', 't',
   0, 64, 0, 0, 0, 0, 0, 0, 0, 5, 0, 65, 239, 255, 255, 255, 224, 0, 0 };

static int parse(record_rtmp_reply_t *r, size_t size)
{
   size_t i;
   int result = RECORD_RTMP_REPLY_PROGRESS;
   for (i = 0; i < size * 3 + 16 && result == RECORD_RTMP_REPLY_PROGRESS; i++)
      result = record_rtmp_reply_step(r);
   assert(result != RECORD_RTMP_REPLY_PROGRESS);
   return result;
}

static void contract(void)
{
   record_rtmp_reply_t *r = record_rtmp_reply_new();
   record_rtmp_reply_info_t info;
   uint8_t wrapped[sizeof(stream_reply) + 1];
   size_t i;
   assert(r && !record_rtmp_reply_get(r, &info));
   assert(record_rtmp_reply_begin(r, 20, stream_reply, sizeof(stream_reply)));
   assert(!record_rtmp_reply_begin(r, 20, stream_reply, sizeof(stream_reply)));
   assert(parse(r, sizeof(stream_reply)) == RECORD_RTMP_REPLY_DONE);
   assert(record_rtmp_reply_get(r, &info));
   assert(info.command.data == stream_reply + 3 && info.command.size == 7);
   assert(info.transaction == 2 && info.stream_valid && info.stream == 0xffffffff);
   assert(record_rtmp_reply_step(r) == RECORD_RTMP_REPLY_DONE);
   record_rtmp_reply_release(r);
   wrapped[0] = 0;
   memcpy(wrapped + 1, stream_reply, sizeof(stream_reply));
   assert(record_rtmp_reply_begin(r, 17, wrapped, sizeof(wrapped)));
   assert(parse(r, sizeof(wrapped)) == RECORD_RTMP_REPLY_DONE);
   record_rtmp_reply_release(r);
   wrapped[0] = 1;
   assert(!record_rtmp_reply_begin(r, 17, wrapped, sizeof(wrapped)));
   for (i = 1; i < sizeof(stream_reply); i++)
   {
      assert(record_rtmp_reply_begin(r, 20, stream_reply, i));
      if (i == 20)
         assert(parse(r, i) == RECORD_RTMP_REPLY_DONE);
      else
      {
         assert(parse(r, i) == RECORD_RTMP_REPLY_ERROR);
         assert(record_rtmp_reply_step(r) == RECORD_RTMP_REPLY_ERROR);
         assert(!record_rtmp_reply_get(r, &info));
      }
      record_rtmp_reply_release(r);
   }
   assert(record_rtmp_reply_begin(r, 20, stream_reply, sizeof(stream_reply)));
   record_rtmp_reply_release(r);
   record_rtmp_reply_free(r);
}

static void write_uint(FILE *f, uint32_t value)
{
   uint8_t p[4];
   p[0] = (uint8_t)(value >> 24);
   p[1] = (uint8_t)(value >> 16);
   p[2] = (uint8_t)(value >> 8);
   p[3] = (uint8_t)value;
   assert(fwrite(p, 1, 4, f) == 4);
}

static void write_span(FILE *f, record_rtmp_span_t span)
{
   write_uint(f, (uint32_t)span.size);
   if (span.size)
      assert(fwrite(span.data, 1, span.size, f) == span.size);
}

static void decode(const char *input, const char *output)
{
   record_rtmp_reply_t *r = record_rtmp_reply_new();
   record_rtmp_reply_info_t info;
   FILE *f = fopen(input, "rb");
   uint8_t *data;
   long size;
   bool valid;
   assert(r && f && !fseek(f, 0, SEEK_END));
   size = ftell(f);
   assert(size > 1 && size <= 65537 && !fseek(f, 0, SEEK_SET));
   data = (uint8_t*)malloc((size_t)size);
   assert(data && fread(data, 1, (size_t)size, f) == (size_t)size && !fclose(f));
   valid = record_rtmp_reply_begin(r, data[0], data + 1, (size_t)size - 1)
      && parse(r, (size_t)size) == RECORD_RTMP_REPLY_DONE;
   f = fopen(output, "wb");
   assert(f && fputc(valid ? 1 : 0, f) != EOF);
   if (valid)
   {
      assert(record_rtmp_reply_get(r, &info));
      write_uint(f, info.transaction);
      write_uint(f, info.stream);
      assert(fputc(info.stream_valid ? 1 : 0, f) != EOF);
      write_span(f, info.command);
      write_span(f, info.code);
      write_span(f, info.level);
      write_span(f, info.description);
   }
   assert(!fclose(f));
   record_rtmp_reply_release(r);
   free(data);
   record_rtmp_reply_free(r);
}

int main(int argc, char **argv)
{
   if (argc == 3)
   {
      decode(argv[1], argv[2]);
      return 0;
   }
   assert(argc == 1);
   contract();
   puts("PASS: bounded AMF reply lifecycle, exact IDs, wrapper and truncation");
   return 0;
}
