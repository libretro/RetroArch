/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
 */
#define main packet_replay_main
#include "stream_receiver_test.c"
#undef main
#include "../../../record/record_aac_stream_timing.h"

struct worker_replay
{
   uint64_t accepted;
   record_mediacodec_encoder_t *handle;
   record_mediacodec_packet_t *packets;
   unsigned count, index, calls;
   bool audio, format, eos, held;
};
static struct worker_replay playback[2];
static int16_t *expected_pcm;
static size_t expected_frames;
static struct worker_replay *replay(record_mediacodec_encoder_t *s)
{
   assert(s == playback[0].handle || s == playback[1].handle);
   return &playback[s == playback[1].handle ? 1 : 0];
}
int record_mediacodec_encoder_audio_submit(record_mediacodec_encoder_t *s,
                                           const int16_t *samples, size_t frames,
                                           uint64_t timestamp, size_t *accepted)
{
   struct worker_replay *r = replay(s);
   size_t count = frames > 317 ? 317 : frames;
   assert(r->audio && !r->eos && timestamp == r->accepted);
   assert(timestamp <= expected_frames &&
          count <= expected_frames - (size_t)timestamp);
   assert(!memcmp(samples, expected_pcm + (size_t)timestamp * 2,
                  count * 2 * sizeof(*samples)));
   *accepted = 0;
   if (++r->calls % 3 == 0)
      return 0;
   r->accepted += count;
   *accepted = count;
   return 1;
}
int record_mediacodec_encoder_audio_finish(record_mediacodec_encoder_t *s,
                                           uint64_t end)
{
   struct worker_replay *r = replay(s);
   assert(r->audio && end == expected_frames && r->accepted == end);
   r->eos = true;
   return 1;
}
bool record_mediacodec_encoder_finish(record_mediacodec_encoder_t *s)
{
   struct worker_replay *r = replay(s);
   assert(!r->audio);
   r->eos = true;
   return true;
}
void *record_mediacodec_encoder_format(record_mediacodec_encoder_t *s)
{
   assert(replay(s)->format);
   return s;
}
int record_mediacodec_encoder_pump(record_mediacodec_encoder_t *s,
                                   record_mediacodec_packet_t *packet)
{
   struct worker_replay *r = replay(s);
   unsigned limit = r->count;
   /* The native pump returns its same borrowed packet until release,
    * including while the worker waits for calibration or both formats. */
   if (!r->format)
   {
      r->format = true;
      return RECORD_MC_FORMAT;
   }
   if (r->audio && !r->eos && r->accepted / 1024 < limit)
      limit = (unsigned)(r->accepted / 1024);
   if (r->index < limit)
   {
      r->held = true;
      *packet = r->packets[r->index];
      return RECORD_MC_PACKET;
   }
   return r->eos && r->index == r->count ? RECORD_MC_END : RECORD_MC_BUSY;
}
bool record_mediacodec_encoder_release(record_mediacodec_encoder_t *s)
{
   struct worker_replay *r = replay(s);
   assert(r->held && r->index < r->count);
   r->held = false;
   r->index++;
   return true;
}
static void load_packets(FILE *file)
{
   uint8_t header[20];
   size_t size;
   while ((size = fread(header, 1, sizeof(header), file)) != 0)
   {
      struct worker_replay *r;
      record_mediacodec_packet_t *p;
      assert(size == sizeof(header) && word(header) < 2);
      r = &playback[word(header)];
      assert(r->count < 1024);
      p = &r->packets[r->count++];
      p->flags = word(header + 4);
      p->pts_us = (int64_t)(word(header + 8) | (uint64_t)word(header + 12) << 32);
      p->size = word(header + 16);
      assert(p->size && p->size < 1048576);
      p->data = (uint8_t *)malloc(p->size);
      assert(p->data);
      assert(fread((void *)p->data, 1, p->size, file) == p->size);
   }
   assert(feof(file));
}
int main(int argc, char **argv)
{
   static uint8_t random[1528];
   record_mediacodec_encoder_t video = {0}, audio = {0};
   record_mediacodec_stream_config_t mux;
   record_aac_stream_timing_t timing;
   net_rtmp_client_config_t config;
   net_rtmp_client_view_t wait;
   record_stream_mediacodec_t *stream;
   record_mediacodec_worker_t *worker;
   record_audio_queue_t *pcm = NULL;
   record_aac_probe_t *probe = NULL;
   net_rtmp_client_t *client;
   sthread_t *resolver = NULL;
   enum net_rtmp_client_state state = NET_RTMP_CLIENT_RESOLVING, previous;
   retro_time_t deadline;
   FILE *file;
   long bytes;
   size_t frames, at, count;
   unsigned i, n, delay = 0, source_epoch, source_frames = 0;
   int result, output;
   char *pem = NULL;
   int64_t pem_size = 0;
   const char *ca = getenv("RTMPS_CA");
   if (argc == 3 && !strcmp(argv[1], "--dump-probe"))
   {
      const int16_t *input;
      probe = record_aac_probe_new(48000, 2);
      assert(probe);
      input = record_aac_probe_input(probe, 0, &frames);
      file = fopen(argv[2], "wb");
      assert(file && input && frames == RECORD_AAC_PROBE_FRAMES);
      assert(fwrite(input, 2 * sizeof(*input), frames, file) == frames);
      fclose(file);
      record_aac_probe_free(probe);
      return 0;
   }
   if (argc == 2 && !strcmp(argv[1], "--epochs"))
   {
      /* Fixture construction only. Runtime worker must independently measure
       * this FFmpeg encoder delay through the actual probe. */
      assert(record_aac_stream_timing(48000, 1024, &timing));
      printf("%u %u %u %u\n", timing.input_epoch, timing.coded_origin,
             timing.audio_epoch, timing.video_epoch_us);
      return 0;
   }
   assert(argc == 6 && network_init());
   source_epoch = (unsigned)strtoul(argv[5], NULL, 10);
   file = fopen(argv[4], "rb");
   assert(file && !fseek(file, 0, SEEK_END));
   bytes = ftell(file);
   assert(bytes > 0 && bytes % 4 == 0 && bytes < 1048576);
   expected_frames = (size_t)bytes / 4;
   assert(expected_frames >= source_epoch);
   expected_pcm = (int16_t *)malloc((size_t)bytes);
   assert(expected_pcm && !fseek(file, 0, SEEK_SET));
   assert(fread(expected_pcm, 1, (size_t)bytes, file) == (size_t)bytes);
   fclose(file);
   file = fopen(argv[2], "rb");
   assert(file);
   load_config(file, &video);
   load_config(file, &audio);
   playback[0].handle = &video;
   playback[1].handle = &audio;
   playback[1].audio = true;
   for (i = 0; i < 2; i++)
   {
      playback[i].packets = (record_mediacodec_packet_t *)calloc(
          1024, sizeof(record_mediacodec_packet_t));
      assert(playback[i].packets);
   }
   load_packets(file);
   fclose(file);
   if (ca)
   {
      assert(filestream_read_file(ca, (void **)&pem, &pem_size) && pem_size > 0);
      ssl_socket_retro_set_trust_pem(pem, (size_t)pem_size);
   }
   for (i = 0; i < sizeof(random); i++)
      random[i] = (uint8_t)(i * 13 + 1);
   memset(&config, 0, sizeof(config));
   config.url = argv[1];
   config.random = random;
   config.dispatch = resolve_dispatch;
   config.userdata = &resolver;
   config.max_reply = 65536;
   config.streams = 8;
   config.legacy_commands = !strcmp(argv[3], "legacy");
   client = net_rtmp_client_new(&config);
   assert(client);
   deadline = cpu_features_get_time_usec() + 10000000;
   do
   {
      assert(cpu_features_get_time_usec() < deadline);
      previous = state;
      state = net_rtmp_client_step(client, 0, &wait);
      if (state == NET_RTMP_CLIENT_FAILED)
      {
         assert(previous == NET_RTMP_CLIENT_RESOLVING ||
                previous == NET_RTMP_CLIENT_CONNECTING);
         net_rtmp_client_cancel(client);
         assert(net_rtmp_client_free(client));
         if (resolver)
            sthread_join(resolver);
         for (i = 0; i < 2; i++)
         {
            for (n = 0; n < playback[i].count; n++)
               free((void *)playback[i].packets[n].data);
            free(playback[i].packets);
         }
         free(video.data);
         free(audio.data);
         free(expected_pcm);
         ssl_socket_retro_set_trust_pem(NULL, 0);
         free(pem);
         return 78;
      }
      if (!wait.again && state != NET_RTMP_CLIENT_READY)
      {
         if (wait.fd >= 0)
            socket_wait(wait.fd, &wait.read, &wait.write, 1);
         else
            retro_sleep(1);
      }
   } while (state != NET_RTMP_CLIENT_READY);
   receiver_mux_config(&mux);
   stream = record_stream_mediacodec_new(&video, audio.bytes ? &audio : NULL, client,
                                         &mux);
   assert(stream);
   if (audio.bytes)
   {
      source_frames = (unsigned)(expected_frames - source_epoch);
      pcm = record_audio_queue_new(2, 4096, 524288);
      assert(pcm);
      for (at = 0; at < source_frames; at += count)
      {
         count = source_frames - at;
         if (count > 4096)
            count = 4096;
         assert(record_audio_queue_push(pcm, expected_pcm + (source_epoch + at) * 2,
                                        count, at) == RECORD_AUDIO_OK);
      }
      assert(record_audio_queue_finish(pcm));
      probe = record_aac_probe_new(48000, 2);
      assert(probe);
   }
   worker = record_mediacodec_worker_new_output(
       &video, audio.bytes ? &audio : NULL, pcm, record_stream_mediacodec_ops(),
       stream, audio.bytes ? 2 : 0, audio.bytes ? 48000 : 0);
   assert(worker);
   if (audio.bytes)
      assert(record_mediacodec_worker_calibrate_live(worker, probe));
   assert(record_mediacodec_worker_finish(worker, 400000, source_frames));
   deadline = cpu_features_get_time_usec() + 20000000;
   do
   {
      assert(cpu_features_get_time_usec() < deadline);
      output = record_stream_mediacodec_step(stream, 0, &wait);
      assert(output >= 0);
      result = record_mediacodec_worker_step(worker);
      assert(result != RECORD_MC_WORKER_ERROR);
      if (!output && result == RECORD_MC_WORKER_BUSY && !wait.again)
      {
         if (wait.fd >= 0)
            socket_wait(wait.fd, &wait.read, &wait.write, 1);
         else
            retro_sleep(1);
      }
   } while (result != RECORD_MC_WORKER_END);
   if (audio.bytes)
      assert(record_aac_probe_delay(probe, &delay) && delay == 1024);
   assert(record_mediacodec_worker_free(worker));
   assert(record_stream_mediacodec_free(stream) && net_rtmp_client_free(client));
   if (resolver)
      sthread_join(resolver);
   assert(record_audio_queue_free(pcm));
   record_aac_probe_free(probe);
   for (i = 0; i < 2; i++)
   {
      assert(playback[i].index == playback[i].count && !playback[i].held);
      for (n = 0; n < playback[i].count; n++)
         free((void *)playback[i].packets[n].data);
      free(playback[i].packets);
   }
   free(video.data);
   free(audio.data);
   free(expected_pcm);
   ssl_socket_retro_set_trust_pem(NULL, 0);
   free(pem);
   printf("PASS video=%u encoded_audio=%u worker=1 delay=%u source_frames=%u "
          "native_mediacodec=0\n",
          playback[0].count, playback[1].count, delay, source_frames);
   return 0;
}
