/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include "../../../formats/mp4/rmp4_video.c"
#include "rvp9_mp4_fixture.h"

static uint8_t packet[96], superframe[288];
static unsigned bitpos, lives, opens, closes, submitted, exported, released;
static int packet_mode, terminal_mode, partial_open, restart_error, drain_error;

static void bits(unsigned value, unsigned count)
{
   while (count--)
   {
      packet[bitpos >> 3] |= ((value >> count) & 1) << (7 - (bitpos & 7));
      bitpos++;
   }
}

static size_t coded(int key, int show)
{
   unsigned i;
   memset(packet, 0, sizeof(packet));
   bitpos = 0;
   bits(2, 2);
   bits(0, 2);
   bits(0, 1);
   bits(!key, 1);
   bits(show, 1);
   bits(1, 1);
   if (key)
   {
      bits(0x49, 8);
      bits(0x83, 8);
      bits(0x42, 8);
      bits(2, 3);
      bits(0, 1);
      bits(31, 16);
      bits(15, 16);
   }
   else
   {
      if (!show)
         bits(0, 1);
      bits(1, 8);
      for (i = 0; i < 3; i++)
      {
         bits(0, 3);
         bits(0, 1);
      }
      bits(1, 1);
   }
   bits(0, 1);
   if (!key)
   {
      bits(0, 1);
      bits(1, 1);
   }
   bits(0, 2);
   bits(0, 6);
   bits(0, 3);
   bits(0, 1);
   bits(0, 8);
   bits(0, 3);
   bits(0, 1);
   bits(0, 1);
   bits(1, 16);
   return (bitpos + 7) / 8 + 2;
}

typedef struct mock
{
   const uint8_t *retry;
   size_t size;
   unsigned refs, calls, pictures, drains, restarts;
   int output_busy;
} mock_t;

static void release(mock_t *m)
{
   if (!--m->refs)
   {
      lives--;
      free(m);
   }
}

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   (void)done;
   (void)value;
   released++;
   release((mock_t *)owner);
}

static int decode(void *owner, const uint8_t *data, size_t size,
      const rvp9_hw_picture_t *picture)
{
   mock_t *m = (mock_t *)owner;
   m->calls++;
   if (!m->retry)
   {
      m->retry = data;
      m->size = size;
      return 1;
   }
   assert(m->retry == data && m->size == size && picture->slot >= 0);
   m->retry = NULL;
   m->pictures++;
   submitted++;
   return 0;
}

static int output(void *owner, unsigned slot, video_hw_frame_t *frame)
{
   mock_t *m = (mock_t *)owner;
   if (terminal_mode && m->pictures >= 4 && m->drains < 2)
      return 1;
   if (!m->output_busy)
   {
      m->output_busy = 1;
      return 1;
   }
   m->output_busy = 0;
   m->refs++;
   memset(frame, 0, sizeof(*frame));
   frame->image = ++exported;
   frame->wait = slot + 1;
   frame->width = 32;
   frame->height = 16;
   frame->pixfmt = IMAGE_PIXFMT_NV12;
   frame->owner = m;
   frame->abandon = abandon;
   return 0;
}

static bool open_config(void *user, const rvp9_hdr *header,
      const video_color_metadata_t *color, const rvp9_config_t *config,
      unsigned slots, rvp9_hw_ops_t *ops)
{
   mock_t *m;
   (void)user;
   assert(header->w == 32 && header->h == 16 && slots == 10);
   assert(color && color->matrix == 1 && color->range == 1);
   assert(config && config->chroma == 0 && config->bit_depth == 8);
   m = (mock_t *)calloc(1, sizeof(*m));
   assert(m);
   m->refs = 1;
   lives++;
   opens++;
   ops->decoder = m;
   ops->decode = decode;
   ops->output = partial_open ? NULL : output;
   return !partial_open;
}

static void close_backend(rvp9_hw_ops_t *ops)
{
   closes++;
   release((mock_t *)ops->decoder);
   memset(ops, 0, sizeof(*ops));
}

static int drain(void *owner, bool discard)
{
   mock_t *m = (mock_t *)owner;
   (void)discard;
   m->drains++;
   return drain_error ? -1 : m->drains < 2;
}

static int restart(void *owner)
{
   mock_t *m = (mock_t *)owner;
   if (restart_error) return -1;
   if (++m->restarts & 1) return 1;
   m->retry = NULL;
   m->size = 0;
   m->pictures = m->drains = 0;
   m->output_busy = 0;
   return 0;
}

static void make_fixture(void)
{
   static const uint8_t config[12] = {1, 0, 0, 0, 0, 0, 0x80, 1, 13, 1, 0, 0};
   static const uint8_t show_existing = 0x88;
   size_t lengths[3], n = 0;
   unsigned i;
   for (i = 0; i < 3; i++)
   {
      lengths[i] = coded(i == 0, i != 1);
      memcpy(superframe + n, packet, lengths[i]);
      n += lengths[i];
   }
   superframe[n++] = 0xc2;
   for (i = 0; i < 3; i++)
      superframe[n++] = (uint8_t)lengths[i];
   superframe[n++] = 0xc2;
   sample_data[0] = superframe;
   sample_size[0] = n;
   sample_data[1] = &show_existing;
   sample_size[1] = 1;
   fixture(config, 1);
}

static void run(int native_packets, int terminal, int reuse)
{
   rvp9_hw_backend_t backend;
   rmp4_video_stream_t *stream;
   video_hw_frame_t held, frame;
   unsigned frames = 0, attempts = 0, count = submitted, seeks, input_wait = 0;
   unsigned started = opens;
   int64_t position;
   int ret, duration;
   memset(&backend, 0, sizeof(backend));
   packet_mode = native_packets;
   terminal_mode = terminal;
   backend.close = close_backend;
   backend.open_config = open_config;
   backend.packet_decoder = packet_mode != 0;
   backend.drain = terminal ? drain : NULL;
   backend.restart = reuse ? restart : NULL;
   rvp9_set_hw_backend(&backend);
   make_fixture();
   stream = rmp4_video_stream_open(clip, clip_size);
   assert(stream && rmp4_video_stream_set_hw(stream, 1));
   assert(stream->vp9_hw && !stream->vp9 && !stream->frame);
   rmp4_video_stream_set_avail(stream, clip_size - 1);
   memset(&held, 0, sizeof(held));
   while ((ret = rmp4_video_stream_skip(stream, &duration)) >= 0)
   {
      assert(++attempts < 100);
      if (ret == 2)
      {
         if (rmp4_video_stream_wait_reason(stream) == RMP4_VIDEO_WAIT_INPUT)
         {
            input_wait++;
            rmp4_video_stream_set_avail(stream, clip_size);
         }
         else
            assert(rmp4_video_stream_wait_reason(stream) == RMP4_VIDEO_WAIT_DECODER);
         assert(rmp4_video_stream_consumed(stream) <= clip_size);
         continue;
      }
      assert(ret == 1 && duration == 40);
      assert(rmp4_video_stream_hw_frame(stream, &frame));
      assert(!rmp4_video_stream_hw_frame(stream, &held));
      assert(frame.color.matrix == 1 && frame.color.transfer == 13
            && frame.color.range == 1 && frame.color.max_cll == 1000);
      if (!frames)
         held = frame;
      else
         video_hw_frame_abandon(&frame);
      frames++;
   }
   assert(rmp4_video_stream_completed(stream));
   assert(frames == 2 && input_wait == 1
         && submitted - count == (native_packets ? 4u : 3u));
   rmp4_video_stream_rewind(stream);
   assert(!rmp4_video_stream_completed(stream));
   assert(stream->vp9_hw && !stream->vp9);
   attempts = 0;
   do
   {
      ret = rmp4_video_stream_discard(stream, &duration);
      assert(++attempts < 100);
   } while (ret == 2);
   assert(ret == 1 && !rmp4_video_stream_hw_frame(stream, &frame));
   for (seeks = 0; seeks < 10; seeks++)
   {
      assert(rmp4_video_stream_seek_begin_ns(stream, seeks & 1 ? 40000000 : 0));
      attempts = 0;
      do
      {
         ret = rmp4_video_stream_seek_step(stream, &position);
         assert(++attempts < 100);
      } while (!ret);
      assert(ret == 1 && position == (seeks & 1 ? 40000 : 0));
      attempts = 0;
      do
      {
         ret = rmp4_video_stream_skip(stream, &duration);
         assert(++attempts < 100);
      } while (ret == 2);
      assert(ret == 1 && rmp4_video_stream_hw_frame(stream, &frame));
      video_hw_frame_abandon(&frame);
   }
   if (reuse)
   {
      assert(opens == started + 1);
      restart_error = 1;
      rmp4_video_stream_rewind(stream);
      assert(rmp4_video_stream_skip(stream, NULL) < 0);
      restart_error = 0;
   }
   rmp4_video_stream_rewind(stream);
   attempts = 0;
   do
   {
      ret = rmp4_video_stream_skip(stream, &duration);
      assert(++attempts < 100);
   } while (ret == 2);
   assert(ret == 1 && stream->hwf_untaken);
   rmp4_video_stream_close(stream);
   assert(lives == 1 && held.owner);
   video_hw_frame_abandon(&held);
   assert(!lives && opens == closes);
   rvp9_set_hw_backend(NULL);
}

static void terminal_failure(void)
{
   rvp9_hw_backend_t backend;
   rmp4_video_stream_t *stream;
   video_hw_frame_t frame;
   unsigned tries = 0;
   int ret;
   memset(&backend, 0, sizeof(backend));
   backend.close = close_backend;
   backend.open_config = open_config;
   backend.packet_decoder = true;
   backend.drain = drain;
   packet_mode = drain_error = 1;
   terminal_mode = 0;
   rvp9_set_hw_backend(&backend);
   make_fixture();
   stream = rmp4_video_stream_open(clip, clip_size);
   assert(stream && rmp4_video_stream_set_hw(stream, 1));
   while ((ret = rmp4_video_stream_skip(stream, NULL)) >= 0)
   {
      assert(++tries < 100);
      if (ret == 1 && rmp4_video_stream_hw_frame(stream, &frame))
         video_hw_frame_abandon(&frame);
   }
   assert(!rmp4_video_stream_completed(stream));
   rmp4_video_stream_close(stream);
   drain_error = 0;
   rvp9_set_hw_backend(NULL);
   assert(!lives);
}

int main(void)
{
   rvp9_hw_backend_t backend;
   rmp4_video_stream_t *stream;
   run(0, 0, 0);
   run(1, 0, 0);
   run(1, 1, 0);
   run(1, 0, 1);
   run(1, 1, 1);
   terminal_failure();
   make_fixture();
   memset(&backend, 0, sizeof(backend));
   backend.open_config = open_config;
   backend.close = close_backend;
   rvp9_set_hw_backend(&backend);
   stream = rmp4_video_stream_open(clip, clip_size);
   assert(stream && rmp4_video_stream_set_hw(stream, 1));
   {
      unsigned before = submitted;
      size_t first = (size_t)(rmp4_media_floor(stream->demux));
      clip[first + 4] ^= 0x10;
      assert(rmp4_video_stream_skip(stream, NULL) < 0 && submitted == before);
      clip[first + 4] ^= 0x10;
   }
   rmp4_video_stream_close(stream);
   assert(!lives);
   partial_open = 1;
   rvp9_set_hw_backend(&backend);
   stream = rmp4_video_stream_open(clip, clip_size);
   assert(stream && !rmp4_video_stream_set_hw(stream, 1));
   assert(stream->vp9 && !stream->vp9_hw && !lives);
   rmp4_video_stream_close(stream);
   rvp9_set_hw_backend(NULL);
   assert(opens == closes);
   puts("PASS: MP4 VP9 configuration admission, superframes, BUSY, terminal drain, rewind, discard and retained leases");
   return 0;
}
