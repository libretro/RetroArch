/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Native Windows endpoint test. Only zero-valued PCM is submitted. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"

extern audio_driver_t audio_wasapi;
void wasapi_presentation_unit(void);

static float presentation_silence[960];

static void presentation_require(bool condition, const char *message)
{
   if (!condition)
   {
      fprintf(stderr, "FAIL: %s\n", message);
      exit(EXIT_FAILURE);
   }
}

static void presentation_observe(void *device, uint64_t written,
      uint64_t *previous, unsigned *valid)
{
   audio_presentation_position_t position;
   presentation_require(audio_wasapi.presentation_position(device, &position), "presentation unsupported");
   presentation_require(!position.failed, "native presentation timeline became uncertain");
   presentation_require(position.epoch == 1 && position.written == written, "accepted-write count");
   if (position.valid || position.drained)
   {
      presentation_require(position.played >= *previous && position.played <= written, "presentation range");
      *previous = position.played;
      if (position.valid) (*valid)++;
   }
}

static void presentation_drain(void *device, uint64_t written,
      uint64_t *previous, unsigned *valid)
{
   audio_presentation_position_t position;
   unsigned i;
   for (i = 0; i < 200; i++)
   {
      presentation_observe(device, written, previous, valid);
      presentation_require(audio_wasapi.presentation_position(device, &position), "drain query");
      if (position.drained)
      {
         presentation_require(!position.valid && position.played == written, "silence is not a source position");
         return;
      }
      Sleep(5);
   }
   presentation_require(false, "accepted tail did not reach the endpoint");
}

int main(int argc, char **argv)
{
   settings_t *settings = config_get_ptr();
   void *device;
   unsigned rate = 0, i, valid = 0;
   size_t frame_bytes;
   uint64_t written = 0, previous = 0;
   wasapi_presentation_unit();
   if (argc > 1 && !strcmp(argv[1], "--unit")) return EXIT_SUCCESS;
   settings->bools.audio_wasapi_exclusive_mode = argc > 1 && !strcmp(argv[1], "--exclusive");
   settings->bools.audio_wasapi_mmcss = true;
   settings->bools.audio_sync = false;
   settings->uints.audio_format_negotiation = AUDIO_FORMAT_NEGOTIATION_FLOAT;
   audio_driver_request_presentation(true);
   device = audio_wasapi.init(NULL, 48000, 64, &rate);
   presentation_require(device != NULL, "native WASAPI initialization");
   presentation_require(rate == 48000, "native test requires a 48000 Hz stream");
   frame_bytes = 2 * (audio_wasapi.use_float(device) ? sizeof(float) : sizeof(int16_t));
   audio_wasapi.set_nonblock_state(device, true);
   presentation_require(audio_wasapi.write(device, presentation_silence, frame_bytes - 1) < 0,
         "fractional PCM frame must not be accepted");
   {
      void *region = NULL;
      size_t bytes = audio_wasapi.write_begin(device, frame_bytes - 1, &region);
      presentation_require(!bytes && !region, "fractional lend is unavailable");
      bytes = audio_wasapi.write_begin(device, frame_bytes, &region);
      presentation_require(bytes == frame_bytes && region != NULL, "aligned direct lend");
      presentation_require(audio_wasapi.write_end(device, frame_bytes - 1) < 0,
            "fractional commit must abandon the lend");
   }
   presentation_observe(device, written, &previous, &valid);
   for (i = 0; i < 200; i++)
   {
      size_t bytes = 480 * frame_bytes;
      ssize_t accepted;
      if (i & 1)
      {
         void *region = NULL;
         bytes = audio_wasapi.write_begin(device, bytes, &region);
         if (bytes) memset(region, 0, bytes);
         accepted = audio_wasapi.write_end(device, bytes);
      }
      else
         accepted = audio_wasapi.write(device, presentation_silence, bytes);
      presentation_require(accepted >= 0 && (size_t)accepted <= bytes
            && (size_t)accepted % frame_bytes == 0, "native partial-write contract");
      written += (uint64_t)accepted / frame_bytes;
      presentation_observe(device, written, &previous, &valid);
      Sleep(10);
   }
   presentation_drain(device, written, &previous, &valid);
   Sleep(40);
   presentation_drain(device, written, &previous, &valid);
   presentation_require(audio_wasapi.stop(device), "native stop");
   Sleep(40);
   presentation_require(audio_wasapi.start(device, false), "native restart");
   for (i = 0; i < 20; i++)
   {
      ssize_t accepted = audio_wasapi.write(device, presentation_silence, 480 * frame_bytes);
      presentation_require(accepted >= 0, "native write after restart");
      written += (uint64_t)accepted / frame_bytes;
      presentation_observe(device, written, &previous, &valid);
      Sleep(10);
   }
   presentation_drain(device, written, &previous, &valid);
   presentation_require(valid > 50 && written > 48000, "actual clocked PCM playback");
   audio_wasapi.free(device);
   printf("Native WASAPI %s: %lu valid positions, %lu accepted frames, silence gaps and stop/start PASS\n",
         settings->bools.audio_wasapi_exclusive_mode ? "exclusive" : "shared",
         (unsigned long)valid, (unsigned long)written);
   return EXIT_SUCCESS;
}
