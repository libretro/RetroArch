/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <formats/rac3.h>
#include "../../../record/record_mux_mp4.h"
#ifdef _WIN32
#include <windows.h>
#include "../../../record/record_file_win32.h"
typedef record_file_win32_t output_file_t;
#else
typedef FILE output_file_t;
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #x); goto done; } } while (0)
static int read32(FILE *f, unsigned *out)
{
   uint8_t p[4];
   if (fread(p, 1, 4, f) != 4) return 0;
   *out = p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
   return 1;
}
static bool output(record_mux_mp4_t *m, output_file_t *out)
{
   const void *data; size_t size;
#ifdef _WIN32
   unsigned i;
   int result;
   if (!record_mux_mp4_get(m, &data, &size)
         || record_file_win32_push(out, data, size) != RECORD_FILE_OK)
      return false;
   for (i = 0; i < 10000; i++)
   {
      result = record_file_win32_step(out);
      if (result != RECORD_FILE_BUSY)
         return result == RECORD_FILE_OK && record_mux_mp4_release(m);
      Sleep(1); /* Test deadline only. */
   }
   fprintf(stderr, "Timed out with borrowed mux output still in flight\n");
   abort();
#else
   return record_mux_mp4_get(m, &data, &size) && fwrite(data, 1, size, out) == size
      && record_mux_mp4_release(m);
#endif
}
int main(int argc, char **argv)
{
   record_mp4_config_t c;
   record_mux_mp4_t *m = NULL;
   rac3_encoder_t *encoder = NULL;
   uint8_t *headers = NULL, *packet = NULL, *audio = NULL;
   float *pcm = NULL;
   FILE *in = NULL, *raw_audio = NULL;
   output_file_t *out = NULL;
   unsigned h[10], i, n, key, sample, channel, channels = 0;
   size_t audio_size = 0;
   uint64_t audio_pts = 0;
   int ok = 0;
   if (argc != 4) return 1;
   memset(&c, 0, sizeof(c));
   in = fopen(argv[1], "rb"); CHECK(in != NULL);
   for (i = 0; i < 10; i++) CHECK(read32(in, &h[i]));
   c.codec = (enum record_mp4_codec)h[0]; c.bit_depth = h[1]; c.width = h[2]; c.height = h[3];
   c.timescale = h[4]; c.max_packet_size = 1024 * 1024;
   CHECK(h[5] && h[6] && h[6] <= 256);
   CHECK(read32(in, &n) && n && n <= 65536);
   headers = (uint8_t*)malloc(n); CHECK(headers != NULL);
   CHECK(fread(headers, 1, n, in) == n); c.headers = headers; c.headers_size = n;
   packet = (uint8_t*)malloc(c.max_packet_size); CHECK(packet != NULL);
   raw_audio = fopen(argv[3], "wb"); CHECK(raw_audio != NULL);
   if (h[7])
   {
      rac3_frame_info_t info;
      encoder = rac3_encoder_new(h[7], h[8], h[9]); CHECK(encoder != NULL);
      audio = (uint8_t*)malloc(RAC3_MAX_FRAME_BYTES); CHECK(audio != NULL);
      pcm = (float*)calloc(1536 * 6, sizeof(*pcm)); CHECK(pcm != NULL);
      audio_size = rac3_encode_frame(encoder, pcm, audio, RAC3_MAX_FRAME_BYTES); CHECK(audio_size != 0);
      CHECK(rac3_parse_frame_info(audio, audio_size, &info) == RAC3_OK); channels = info.channels;
      c.ac3_header = audio; c.ac3_header_size = audio_size;
   }
   m = record_mux_mp4_new(&c); CHECK(m != NULL);
   memset(headers, 0, n); free(headers); headers = NULL;
#ifdef _WIN32
   out = record_file_win32_new(argv[2]);
#else
   out = fopen(argv[2], "wb");
#endif
   CHECK(out != NULL); CHECK(output(m, out));
   for (i = 0; i < h[6]; i++)
   {
      CHECK(read32(in, &key) && read32(in, &n) && n && n <= c.max_packet_size);
      CHECK(fread(packet, 1, n, in) == n);
      CHECK(record_mux_mp4_push(m, 1, packet, n, (uint64_t)i * h[5], h[5], key != 0) == RECORD_MP4_OK);
      CHECK(output(m, out));
      while (encoder && audio_pts * c.timescale < (uint64_t)(i + 1) * h[5] * h[7])
      {
         if (audio_pts)
         {
            for (sample = 0; sample < 1536; sample++)
               for (channel = 0; channel < channels; channel++)
                  pcm[sample * channels + channel] = (float)(0.1 * sin((audio_pts + sample) * 0.01 * (channel + 1)));
            audio_size = rac3_encode_frame(encoder, pcm, audio, RAC3_MAX_FRAME_BYTES); CHECK(audio_size != 0);
         }
         CHECK(record_mux_mp4_push(m, 2, audio, audio_size, audio_pts, 1536, true) == RECORD_MP4_OK);
         CHECK(output(m, out));
         CHECK(fwrite(audio, 1, audio_size, raw_audio) == audio_size); audio_pts += 1536;
      }
   }
   CHECK(record_mux_mp4_finish(m)); ok = 1;
done:
   if (in) fclose(in);
#ifdef _WIN32
   if (out && ok && !record_file_win32_finish(out)) ok = 0;
   if (!record_file_win32_free(out)) abort();
#else
   if (out && fclose(out)) ok = 0;
#endif
   if (raw_audio && fclose(raw_audio)) ok = 0;
   record_mux_mp4_free(m); rac3_encoder_free(encoder);
   free(headers); free(packet); free(audio); free(pcm);
   return !ok;
}
