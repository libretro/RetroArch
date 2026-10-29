/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <boolean.h>

enum { AVAssetWriterStatusWriting = 1, AVEncoderBitRateKey = 1 };
typedef struct
{
   int status;
   bool preset_supported, default_supported;
   unsigned probes;
} writer_t;
typedef struct
{
   bool ready, accepted;
   unsigned calls;
} input_t;
typedef struct
{
   writer_t *assetWriter;
   input_t *videoInput, *audioInput, *pixelBufferAdaptor;
} record_avfoundation_t;
typedef struct { bool bitrate; } NSMutableDictionary;
static NSMutableDictionary fallback_settings;
static unsigned released, freed, removed;

static void release_sample(void *sample)
{
   assert(sample);
   released++;
}
#define CVPixelBufferRelease release_sample
#define CFRelease release_sample
static bool append_video(input_t *input, void *sample, int timestamp)
{
   assert(sample && timestamp == 7);
   input->calls++;
   return input->accepted;
}
static bool append_audio(input_t *input, void *sample)
{
   assert(sample);
   input->calls++;
   return input->accepted;
}
static bool can_apply(writer_t *writer, NSMutableDictionary *settings)
{
   writer->probes++;
   return settings->bitrate ? writer->preset_supported
         : writer->default_supported;
}
static NSMutableDictionary *copy_settings(NSMutableDictionary *settings)
{
   fallback_settings = *settings;
   return &fallback_settings;
}
static void remove_bitrate(NSMutableDictionary *settings, int key)
{
   assert(key == AVEncoderBitRateKey);
   settings->bitrate = false;
   removed++;
}
static void avfoundation_release_handle(record_avfoundation_t *handle)
{
   assert(handle);
   freed++;
}
static void trace(const char *format, ...)
{
   assert(format);
}
#define RARCH_ERR trace
#define RARCH_WARN trace
#include "legacy_append.inc"

int main(void)
{
   writer_t writer = {0};
   input_t input = {0};
   record_avfoundation_t handle;
   NSMutableDictionary settings;
   unsigned status, ready, accepted, audio, cases = 0;
   handle.assetWriter = &writer;
   handle.videoInput = handle.audioInput = handle.pixelBufferAdaptor = &input;
   for (audio = 0; audio < 2; audio++)
      for (status = 0; status < 5; status++)
         for (ready = 0; ready < 2; ready++)
            for (accepted = 0; accepted < 2; accepted++)
            {
               bool expected = status == AVAssetWriterStatusWriting
                     && ready && accepted;
               bool result;
               writer.status = (int)status;
               input.ready = ready != 0;
               input.accepted = accepted != 0;
               input.calls = released = 0;
               result = audio ? legacy_audio_submit(&handle, &input)
                     : legacy_video_submit(&handle, &input, 7);
               assert(result == expected && released == 1);
               assert(input.calls == (unsigned)(status
                           == AVAssetWriterStatusWriting && ready));
               cases++;
            }
   for (ready = 0; ready < 2; ready++)
      for (accepted = 0; accepted < 2; accepted++)
      {
         void *result;
         settings.bitrate = true;
         writer.preset_supported = ready != 0;
         writer.default_supported = accepted != 0;
         writer.probes = freed = removed = 0;
         result = legacy_audio_settings(&handle, &settings);
         assert(!!result == !!(ready || accepted));
         assert(settings.bitrate);
         assert(writer.probes == (ready ? 1u : 2u));
         assert(removed == (ready ? 0u : 1u));
         assert(freed == (unsigned)(!ready && !accepted));
         if (result)
            assert(((NSMutableDictionary*)result)->bitrate == (ready != 0));
      }
   printf("PASS: %u production append/result cases and AAC preset/default/refusal paths; Apple messages are stand-ins\n", cases);
   return 0;
}
