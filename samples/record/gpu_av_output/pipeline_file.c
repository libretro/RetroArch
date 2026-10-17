/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>
#include <formats/rac3.h>
#include "mock_worker.h"
#include "../../../record/record_gpu_output_d3d12.h"
#ifdef _WIN32
#include <windows.h>
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #x); goto done; } } while (0)
static unsigned read32(FILE *f)
{
   uint8_t b[4];
   if (fread(b, 1, 4, f) != 4) return 0;
   return b[0] | (unsigned)b[1] << 8 | (unsigned)b[2] << 16 | (unsigned)b[3] << 24;
}
int main(int argc, char **argv)
{
   record_gpu_output_config_t c;
   record_gpu_worker_d3d12_t w;
   record_gpu_output_d3d12_t *p = NULL;
   rac3_encoder_t *reference = NULL;
   FILE *audio = NULL;
   uint8_t *headers = NULL, *packet = NULL;
   int16_t *samples = NULL;
   float *pcm = NULL;
   unsigned h[10], i, n, channels = 0, loops;
   size_t total = 0, sent = 0, frame, channel, size, chunk;
   uint64_t wanted;
   int result = RECORD_GPU_OUTPUT_BUSY, ok = 0;
   bool closed = false;
   if (argc != 4) return 1;
   memset(&c, 0, sizeof(c)); memset(&w, 0, sizeof(w));
   w.in = fopen(argv[1], "rb"); CHECK(w.in != NULL);
   for (i = 0; i < 10; i++) h[i] = read32(w.in);
   CHECK(h[4] && h[5] && h[6] && h[6] <= 256);
   n = read32(w.in); CHECK(n && n <= 65536);
   headers = (uint8_t*)malloc(n); CHECK(headers != NULL);
   CHECK(fread(headers, 1, n, w.in) == n);
   c.video.headers = headers; c.video.headers_size = n; c.video.codec = (enum record_mp4_codec)h[0];
   c.video.bit_depth = h[1]; c.video.width = h[2]; c.video.height = h[3]; c.video.timescale = h[4];
   c.video.max_packet_size = w.size = 1024 * 1024;
   w.frames = h[6]; w.duration = h[5];
   w.data = (uint8_t*)malloc(w.size); CHECK(w.data != NULL);
   audio = fopen(argv[3], "wb"); CHECK(audio != NULL);
   if (h[7])
   {
      rac3_frame_info_t info;
      c.audio_rate = h[7]; c.audio_layout = h[8]; c.audio_kbps = h[9]; c.audio_fifo_frames = 4096;
      reference = rac3_encoder_new(h[7], h[8], h[9]); CHECK(reference != NULL);
      pcm = (float*)calloc(1536 * 6, sizeof(*pcm)); packet = (uint8_t*)malloc(RAC3_MAX_FRAME_BYTES);
      CHECK(pcm != NULL && packet != NULL);
      size = rac3_encode_frame(reference, pcm, packet, RAC3_MAX_FRAME_BYTES); CHECK(size != 0);
      CHECK(rac3_parse_frame_info(packet, size, &info) == RAC3_OK); channels = info.channels;
      rac3_encoder_free(reference); reference = rac3_encoder_new(h[7], h[8], h[9]); CHECK(reference != NULL);
      wanted = ((uint64_t)h[6] * h[5] * h[7] + h[4] - 1) / h[4];
      CHECK(wanted && wanted <= h[7] * 10u); total = (size_t)wanted;
      samples = (int16_t*)malloc(total * channels * sizeof(*samples)); CHECK(samples != NULL);
      for (frame = 0; frame < total; frame++)
         for (channel = 0; channel < channels; channel++)
            samples[frame * channels + channel] = (int16_t)((int)((frame * 37 + channel * 1009) % 60001) - 30000);
      for (frame = 0; frame < total; frame += 1536)
      {
         chunk = total - frame; if (chunk > 1536) chunk = 1536;
         memset(pcm, 0, 1536 * channels * sizeof(*pcm));
         for (channel = 0; channel < chunk * channels; channel++)
            pcm[channel] = samples[frame * channels + channel] * (1.0f / 32768.0f);
         size = rac3_encode_frame(reference, pcm, packet, RAC3_MAX_FRAME_BYTES); CHECK(size != 0);
         CHECK(fwrite(packet, 1, size, audio) == size);
      }
   }
   p = record_gpu_output_d3d12_new(&w, &c, argv[2]); CHECK(p != NULL);
   memset(headers, 0, n); free(headers); headers = NULL;
   for (loops = 0; loops < 1000000; loops++)
   {
      if (sent < total)
      {
         chunk = 1 + (loops * 191u) % 1023; if (chunk > total - sent) chunk = total - sent;
         result = record_gpu_output_d3d12_audio(p, samples + sent * channels, chunk);
         CHECK(result != RECORD_GPU_OUTPUT_ERROR);
         if (result == RECORD_GPU_OUTPUT_PROGRESS) sent += chunk;
      }
      if (!closed && sent == total)
      { CHECK(record_gpu_output_d3d12_audio_finish(p)); closed = true; }
      result = record_gpu_output_d3d12_step(p);
      CHECK(result != RECORD_GPU_OUTPUT_ERROR);
      if (result == RECORD_GPU_OUTPUT_END) break;
#ifdef _WIN32
      if (result == RECORD_GPU_OUTPUT_BUSY) Sleep(1);
#endif
   }
   CHECK(result == RECORD_GPU_OUTPUT_END && sent == total && w.made == w.frames && !w.held);
   CHECK(record_gpu_output_d3d12_free(p)); p = NULL; ok = 1;
done:
   if (p)
   {
      record_gpu_output_d3d12_audio_finish(p); record_gpu_output_d3d12_abort(p);
      for (i = 0; i < 10000 && record_gpu_output_d3d12_step(p) == RECORD_GPU_OUTPUT_BUSY; i++)
      {
#ifdef _WIN32
         Sleep(1);
#endif
      }
      if (!record_gpu_output_d3d12_free(p)) abort();
   }
   if (w.in) fclose(w.in);
   if (audio && fclose(audio)) ok = 0;
   rac3_encoder_free(reference); free(w.data); free(headers); free(samples); free(pcm); free(packet);
   return !ok;
}
