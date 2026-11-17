/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HEVC_NATIVE_CAPTURE_HELPERS_ONLY
#include "hevc_native_capture_test.c"
#include "../../../record/record_gpu_worker_vulkan_hevc.h"
#include "../../../record/record_stream_vulkan.h"
#include "../../../record/record_gpu_output_vulkan.h"
#include "../../../record/record_gpu_session_vulkan_hevc.h"
#include <net/net_compat.h>
#include <net/net_socket_ssl.h>
#include <crypto/kdf.h>
#include <rthreads/rthreads.h>
#include <windows.h>
#include <retro_atomic.h>
#include "../../../record/record_gpu_dispatch_vulkan_backend.h"

static FILE *stream_raw;
static retro_atomic_int_t stream_packet_count;
static record_gpu_dispatch_vulkan_t *native_dispatch;
static unsigned stream_packets(void)
{ return (unsigned)retro_atomic_load_acquire_int(&stream_packet_count); }
static record_gpu_session_vulkan_t *stream_dispatch_start(
      const record_gpu_dispatch_vulkan_config_t *config,
      const record_gpu_output_vulkan_config_t *output, const char *path)
{
   return record_gpu_session_vulkan_new_hevc(&config->hevc.encoder,
         &config->hevc.ops, output, path);
}
static record_gpu_output_vulkan_t *native_output;
static record_gpu_session_vulkan_t *native_session;
static int stream_native_packet(record_gpu_worker_vulkan_t *worker, vulkan_encode_packet_t *packet)
{
   int result = record_gpu_worker_vulkan_step(worker, packet);
   if (result == RECORD_GPU_VULKAN_PACKET)
   {
      assert(packet->timestamp == stream_packets() && packet->duration == 1);
      assert(fwrite(packet->data, 1, packet->size, stream_raw) == packet->size);
      retro_atomic_fetch_add_seq_cst_int(&stream_packet_count, 1);
   }
   return result;
}
/* Test-only observation of the borrowed native packet, before network muxing. */
#define record_gpu_worker_vulkan_step stream_native_packet
#include "../../../record/record_stream_vulkan.c"
#undef record_gpu_worker_vulkan_step

static bool stream_session_headers(vulkan_encode_hevc_t *encoder,
      void *data, size_t capacity, size_t *size)
{
   if (!vulkan_encode_hevc_headers(encoder, data, capacity, size))
      return false;
   assert(fwrite(data, 1, *size, stream_raw) == *size);
   return true;
}
/* Observe the real session encoder's cold headers, not a separate seed. */
#define vulkan_encode_hevc_headers stream_session_headers
#include "../../../record/record_gpu_session_vulkan_hevc.c"
#undef vulkan_encode_hevc_headers

struct native_stream_fixture
{
   struct native_hevc_capture capture;
   record_gpu_dispatch_vulkan_config_t dispatch_config;
   record_gpu_output_config_t output;
   record_gpu_output_vulkan_config_t routed;
   net_rtmp_client_config_t client_config;
   char *trust;
   int16_t pcm[3200];
   uint8_t random[1528], headers[65536];
};
static bool stream_resolve(void (*work)(void*), void *job, void *userdata)
{
   sthread_t *thread = sthread_create(work, job);
   (void)userdata;
   if (!thread) return false;
   assert(sthread_detach(thread) == 0);
   return true;
}
static bool stream_trust(struct native_stream_fixture *n)
{
   const char *path = getenv("RTMPS_CA");
   FILE *file;
   long size;
   bool ok;
   if (!path) return true;
   file = fopen(path, "rb");
   if (!file) return false;
   ok = !fseek(file, 0, SEEK_END) && (size = ftell(file)) > 0
      && size < 65536 && !fseek(file, 0, SEEK_SET);
   if (ok)
   {
      n->trust = (char*)malloc((size_t)size);
      ok = n->trust && fread(n->trust, 1, (size_t)size, file) == (size_t)size;
      if (ok) ssl_socket_retro_set_trust_pem(n->trust, (size_t)size);
   }
   fclose(file);
   return ok;
}
static int stream_service(record_stream_gpu_t *stream)
{
   record_stream_wait_t wait;
   int result;
   if (native_dispatch)
   {
      enum record_gpu_dispatch_vulkan_state state = record_gpu_dispatch_vulkan_status(native_dispatch, NULL, NULL);
      assert(state != RECORD_VKD_FAILED);
      result = state == RECORD_VKD_DONE ? RECORD_GPU_OUTPUT_END : RECORD_GPU_OUTPUT_BUSY;
   }
   else if (native_session)
   {
      int fd;
      bool read, write;
      result = record_gpu_session_vulkan_step(native_session);
      (void)record_gpu_session_vulkan_network(native_session, &fd, &read, &write);
   }
   else if (native_output)
   {
      int fd;
      bool read, write;
      result = record_gpu_output_vulkan_step(native_output);
      (void)record_gpu_output_vulkan_network(native_output, &fd, &read, &write);
   }
   else
      result = record_stream_gpu_step(stream, 0, &wait);
   assert(result != RECORD_GPU_OUTPUT_ERROR);
   if (result == RECORD_GPU_OUTPUT_BUSY) Sleep(1); /* Fixture deadline/retry only. */
   return result;
}
int main(int argc, char **argv)
{
   struct native_stream_fixture *n = (struct native_stream_fixture*)calloc(1, sizeof(*n));
   struct native_encoder *c;
   vulkan_encode_hevc_t *encoder;
   const vulkan_encode_pool_t *pool;
   record_gpu_queue_t *queue;
   record_gpu_worker_vulkan_t *worker;
   record_stream_gpu_t *stream;
   net_rtmp_client_t *client;
   net_rtmp_client_view_t view;
   net_rtmp_codec_capability_t video = {NET_RTMP_CODEC_ENCODE, "hvc1"};
   net_rtmp_codec_capability_t audio = {NET_RTMP_CODEC_ENCODE, "mp4a"};
   net_rtmp_codec_caps_t caps;
   record_gpu_frame_t frame;
   FILE *pcm;
   char *pcm_path;
   size_t size, path_size;
   unsigned depth, color, rate, channels, index, i, sample, frames;
   bool direct, routed, session_route, dispatch_route;
   int result = 77;
   assert(n && argc == 8 && atoi(argv[1]) == 1 && !atoi(argv[6]));
   depth = atoi(argv[2]) == 3 ? 10 : 8;
   color = (unsigned)atoi(argv[4]);
   assert(color == 1 || (color == 2 && depth == 10));
   assert(getenv("NVENC_TEST_AUDIO_RATE"));
   rate = (unsigned)atoi(getenv("NVENC_TEST_AUDIO_RATE"));
   assert(rate == 44100 || rate == 48000);
   channels = rate == 44100 ? 1 : 2;
   frames = rate / 30;
   direct = atoi(argv[7]) == 1;
   retro_atomic_int_init(&stream_packet_count, 0);
   dispatch_route = getenv("VULKAN_STREAM_DISPATCH") != NULL;
   session_route = dispatch_route || getenv("VULKAN_STREAM_SESSION") != NULL;
   routed = session_route || getenv("VULKAN_STREAM_OUTPUT") != NULL;
   c = &n->capture.adapter.gpu.base;
   if (color == 2)
   {
      n->capture.output_mode = getenv("VULKAN_STREAM_SCRGB") ? 2 : 1;
      n->capture.rgb_format = n->capture.output_mode == 1
         ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_R16G16B16A16_SFLOAT;
   }
   encoder = native_adapter_new_mode(&n->capture.adapter, depth, true, direct, false);
   if (!encoder) goto done;
   if (color == 2)
   {
      assert(vulkan_encode_hevc_free(encoder));
      n->capture.adapter.config.session.video.color = VULKAN_ENCODE_HEVC_PQ;
      encoder = vulkan_encode_hevc_open(&n->capture.adapter.config, &n->capture.adapter.ops);
      assert(encoder);
   }
   pool = vulkan_encode_hevc_pool(encoder);
   assert(pool && pool->storage_images == direct);
   queue = NULL;
   worker = NULL;
   assert(network_init() && stream_trust(n));
   n->client_config.url = getenv("NVENC_TEST_STREAM_URL");
   assert(crypto_random_bytes(n->random, sizeof(n->random)) == 0);
   n->client_config.random = n->random;
   n->client_config.dispatch = stream_resolve;
   n->client_config.max_reply = 65536;
   n->client_config.streams = 8;
   caps.video = &video; caps.video_count = 1;
   caps.audio = &audio; caps.audio_count = 1;
   client = NULL;
   if (!dispatch_route)
   {
      client = net_rtmp_client_new_caps(&n->client_config, &caps);
      assert(client);
      for (i = 0; i < 30000; i++)
      {
         enum net_rtmp_client_state state = net_rtmp_client_step(client, 0, &view);
         assert(state != NET_RTMP_CLIENT_FAILED);
         if (state == NET_RTMP_CLIENT_READY) break;
         Sleep(1);
      }
      assert(i < 30000);
   }
   assert(vulkan_encode_hevc_headers(encoder, n->headers, sizeof(n->headers), &size));
   stream_raw = fopen(argv[3], "wb");
   assert(stream_raw);
   if (!session_route)
      assert(fwrite(n->headers, 1, size, stream_raw) == size);
   n->output.video.headers = n->headers;
   n->output.video.headers_size = size;
   n->output.video.codec = RECORD_MP4_HEVC;
   n->output.video.bit_depth = depth;
   n->output.video.timescale = 30;
   n->output.video.max_packet_size = 1048576;
   n->output.stream_color.primaries = color == 1 ? 1 : 9;
   n->output.stream_color.transfer = color == 1 ? 1 : 16;
   n->output.stream_color.matrix = color == 1 ? 1 : 9;
   n->output.stream_color.range = 1;
   n->output.audio_rate = rate;
   n->output.audio_layout = channels == 1 ? 4 : 3;
   n->output.audio_kbps = 128;
   n->output.audio_fifo_frames = rate;
   stream = NULL;
   if (!session_route)
   {
      queue = record_gpu_queue_new(2);
      worker = record_gpu_worker_vulkan_new_hevc(queue, encoder);
      assert(queue && worker);
   }
   if (routed)
   {
      n->routed.video = n->output.video;
      n->routed.stream_client = client;
      n->routed.video.color_primaries = n->output.stream_color.primaries;
      n->routed.video.transfer_characteristics = n->output.stream_color.transfer;
      n->routed.video.matrix_coefficients = n->output.stream_color.matrix;
      n->routed.video.full_range = n->output.stream_color.range == 2;
      n->routed.video.max_cll = n->output.stream_color.max_cll;
      n->routed.video.max_fall = n->output.stream_color.max_fall;
      n->routed.video.mastering = n->output.stream_color.mastering;
      n->routed.audio_rate = n->output.audio_rate;
      n->routed.audio_layout = n->output.audio_layout;
      n->routed.audio_kbps = n->output.audio_kbps;
      n->routed.audio_fifo_frames = n->output.audio_fifo_frames;
      n->routed.audio_follow_video = true;
      if (session_route)
      {
         assert(vulkan_encode_hevc_free(encoder));
         encoder = NULL;
         memset(&n->routed.video, 0, sizeof(n->routed.video));
         n->routed.video.max_packet_size = 1048576;
         if (dispatch_route)
         {
            n->dispatch_config.hevc.encoder = n->capture.adapter.config;
            n->dispatch_config.hevc.ops = n->capture.adapter.ops;
            n->routed.video.codec = RECORD_MP4_HEVC;
            native_dispatch = record_gpu_dispatch_vulkan_new_backend_stream(&n->dispatch_config,
                  stream_dispatch_start, &n->routed, n->client_config.url, NULL, false);
            assert(native_dispatch);
            for (i = 0; i < 30000; i++)
            {
               enum record_gpu_dispatch_vulkan_state state = record_gpu_dispatch_vulkan_status(native_dispatch, NULL, NULL);
               assert(state == RECORD_VKD_STARTING || state == RECORD_VKD_READY);
               if (state == RECORD_VKD_READY) break;
               Sleep(1);
            }
            assert(i < 30000);
            pool = record_gpu_dispatch_vulkan_pool(native_dispatch);
         }
         else
         {
            native_session = record_gpu_session_vulkan_new_hevc(&n->capture.adapter.config,
                  &n->capture.adapter.ops, &n->routed, n->client_config.url);
            assert(native_session);
            assert(record_gpu_session_vulkan_error(native_session, NULL) == RECORD_VULKAN_SESSION_NONE);
            pool = record_gpu_session_vulkan_pool(native_session);
            queue = record_gpu_session_vulkan_queue(native_session);
         }
         assert(pool && (queue || native_dispatch) && pool->storage_images == direct);
      }
      else
      {
         native_output = record_gpu_output_vulkan_new(worker, &n->routed, n->client_config.url);
         assert(native_output);
      }
   }
   else
   {
      stream = record_stream_vulkan_new(worker, &n->output, client);
      assert(stream);
   }
   load_capture(&n->capture);
   capture_new(&n->capture, pool);
   path_size = strlen(argv[3]) + sizeof(".av.pcm");
   pcm_path = (char*)malloc(path_size);
   assert(pcm_path);
   snprintf(pcm_path, path_size, "%s.av.pcm", argv[3]);
   pcm = fopen(pcm_path, "wb"); free(pcm_path); assert(pcm);
   for (index = 0; index < 24; index++)
   {
      for (i = 0; i < 10000; i++)
      {
         if ((native_dispatch ? record_gpu_dispatch_vulkan_acquire(native_dispatch, &frame.slot)
                  : record_gpu_queue_acquire(queue, &frame.slot)) == RECORD_GPU_QUEUE_OK) break;
         stream_service(stream);
      }
      assert(i < 10000 && frame.slot == index % 2);
      capture_frame(&n->capture, pool, index);
      frame.timestamp = index; frame.duration = 1;
      frame.ready_value = index + 1; frame.keyframe = index == 7;
      assert(native_dispatch ? record_gpu_dispatch_vulkan_submit(native_dispatch, &frame)
            : record_gpu_queue_submit(queue, &frame));
      for (sample = 0; sample < frames * channels; sample++)
         n->pcm[sample] = (int16_t)(6000 * sin((index * frames + sample / channels)
                  * (sample % channels ? 660.0 : 440.0) * 6.283185307179586 / rate));
      assert(fwrite(n->pcm, sizeof(int16_t), frames * channels, pcm) == frames * channels);
      for (i = 0; i < 10000; i++)
      {
         int pushed = native_dispatch ? record_gpu_dispatch_vulkan_audio(native_dispatch, n->pcm, frames)
            : native_session ? record_gpu_session_vulkan_audio(native_session, n->pcm, frames)
            : native_output ? record_gpu_output_vulkan_audio(native_output, n->pcm, frames)
            : record_stream_gpu_audio(stream, n->pcm, frames);
         if (pushed == RECORD_GPU_OUTPUT_PROGRESS) break;
         assert(pushed == RECORD_GPU_OUTPUT_BUSY); stream_service(stream);
      }
      assert(i < 10000);
      for (i = 0; i < 10000 && stream_packets() <= index; i++) stream_service(stream);
      assert(stream_packets() == index + 1);
   }
   /* Observed packets prove all capture/encode commands for those slots retired. */
   capture_free(&n->capture);
   if (native_dispatch)
   {
      assert(record_gpu_dispatch_vulkan_video_finish(native_dispatch));
      assert(record_gpu_dispatch_vulkan_audio_finish(native_dispatch));
   }
   else if (native_session)
   {
      assert(record_gpu_session_vulkan_video_finish(native_session));
      assert(record_gpu_session_vulkan_audio_finish(native_session));
   }
   else
   {
      assert(record_gpu_queue_finish(queue));
      assert(native_output ? record_gpu_output_vulkan_audio_finish(native_output)
            : record_stream_gpu_audio_finish(stream));
   }
   for (i = 0; i < 30000; i++) if (stream_service(stream) == RECORD_GPU_OUTPUT_END) break;
   assert(i < 30000 && stream_packets() == 24);
   if (native_dispatch)
   {
      uint64_t used;
      assert(record_gpu_dispatch_vulkan_audio_frames(native_dispatch, &used));
      assert(used == (uint64_t)24 * frames);
   }
   else assert((native_session ? record_gpu_session_vulkan_audio_frames(native_session)
         : native_output ? record_gpu_output_vulkan_audio_frames(native_output)
         : record_stream_gpu_audio_frames(stream)) == (uint64_t)24 * frames);
   if (native_dispatch)
   {
      for (i = 0; i < 10000; i++)
      {
         if (record_gpu_dispatch_vulkan_free(native_dispatch)) break;
         Sleep(1);
      }
      assert(i < 10000);
      native_dispatch = NULL;
   }
   else if (native_session)
   {
      assert(record_gpu_session_vulkan_free(native_session));
      native_session = NULL;
   }
   else
   {
      assert(native_output ? record_gpu_output_vulkan_free(native_output) : record_stream_gpu_free(stream));
      native_output = NULL;
      assert(record_gpu_worker_vulkan_free(worker));
      assert(vulkan_encode_hevc_free(encoder) && record_gpu_queue_free(queue));
   }
   if (client) assert(net_rtmp_client_free(client));
   assert(!fclose(stream_raw) && !fclose(pcm));
   printf("Vulkan HEVC stream depth=%u direct=%u color=%u output=%u session=%u dispatch=%u validation_errors=%u\n",
         depth, direct ? 1u : 0u, color, routed ? 1u : 0u, session_route ? 1u : 0u, dispatch_route ? 1u : 0u, c->validation_errors);
   result = c->validation_errors ? 1 : 0;
done:
   ssl_socket_retro_set_trust_pem(NULL, 0);
   free(n->trust);
   native_adapter_context_free(&n->capture.adapter);
   return result;
}
