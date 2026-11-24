/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_pyrowave_audio_clock.h"

static void test_controller_audio_clock(void)
{
   record_pyrowave_audio_clock_t *c = record_pyrowave_audio_clock_new(7);
   audio_presentation_position_t device;
   record_pyrowave_clock_position_t source;
   unsigned i;
   memset(&device, 0, sizeof(device)); device.epoch = 3; device.written = 100;
   CHECK(c && record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(!source.valid && !record_pyrowave_audio_clock_drained(c));
   CHECK(record_pyrowave_audio_clock_begin(c, 1000, 441));
   CHECK(record_pyrowave_audio_clock_produced(c, 0, 441, 480));
   CHECK(!record_pyrowave_audio_clock_begin(c, 1441, 441));
   CHECK(!record_pyrowave_audio_clock_accepted(c, 100, 481));
   for (i = 0; i < 480; i += 7)
   {
      unsigned n = 480 - i < 7 ? 480 - i : 7;
      CHECK(record_pyrowave_audio_clock_writable(c));
      CHECK(record_pyrowave_audio_clock_accepted(c, 100 + i, n));
      device.written = 100 + i + n; device.played = 100 + i;
      device.valid = true;
      CHECK(record_pyrowave_audio_clock_observe(c, &device, &source));
      CHECK(source.valid && source.source_sample == 1000 + (uint64_t)i * 441 / 480);
   }
   CHECK(!record_pyrowave_audio_clock_pending(c));
   device.played = device.written;
   CHECK(record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(source.valid && source.source_sample == 1441 && record_pyrowave_audio_clock_drained(c));
   CHECK(record_pyrowave_audio_clock_begin(c, 1500, 10));
   CHECK(!record_pyrowave_audio_clock_drained(c));
   CHECK(record_pyrowave_audio_clock_produced(c, 0, 4, 6));
   CHECK(record_pyrowave_audio_clock_produced(c, 4, 6, 7));
   CHECK(record_pyrowave_audio_clock_accepted(c, 600, 8));
   CHECK(record_pyrowave_audio_clock_pending(c));
   device.written = 608; device.played = 607;
   CHECK(record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(source.valid && source.source_sample == 1504);
   CHECK(record_pyrowave_audio_clock_accepted(c, 608, 5));
   device.written = 700; device.played = 650; device.valid = false; device.drained = true;
   CHECK(record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(!source.valid && record_pyrowave_audio_clock_drained(c));
   device.epoch++;
   CHECK(!record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(record_pyrowave_audio_clock_failed(c) && !record_pyrowave_audio_clock_drained(c));
   record_pyrowave_audio_clock_free(c);
   c = record_pyrowave_audio_clock_new(8);
   device.epoch = 1; device.written = 0; device.played = 0; device.valid = true; device.drained = false;
   CHECK(record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(record_pyrowave_audio_clock_begin(c, 0, 1));
   CHECK(record_pyrowave_audio_clock_produced(c, 0, 1, 0));
   CHECK(!record_pyrowave_audio_clock_pending(c) && !record_pyrowave_audio_clock_drained(c));
   device.written = 100; device.played = 50;
   CHECK(record_pyrowave_audio_clock_observe(c, &device, &source));
   device.played = 49;
   CHECK(!record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(record_pyrowave_audio_clock_failed(c));
   record_pyrowave_audio_clock_free(c);
   c = record_pyrowave_audio_clock_new(9);
   device.written = device.played = 0;
   CHECK(record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(record_pyrowave_audio_clock_begin(c, 0, 1024));
   CHECK(record_pyrowave_audio_clock_produced(c, 0, 1024, 1024));
   for (i = 0; i < 256; i++)
   {
      CHECK(record_pyrowave_audio_clock_writable(c));
      CHECK(record_pyrowave_audio_clock_accepted(c, i, 1));
   }
   CHECK(!record_pyrowave_audio_clock_writable(c));
   CHECK(!record_pyrowave_audio_clock_accepted(c, 256, 1));
   CHECK(!record_pyrowave_audio_clock_failed(c));
   device.written = device.played = 256;
   CHECK(record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(source.valid && source.source_sample == 256);
   CHECK(record_pyrowave_audio_clock_writable(c));
   CHECK(record_pyrowave_audio_clock_accepted(c, 256, 768));
   device.written = device.played = 1024;
   CHECK(record_pyrowave_audio_clock_observe(c, &device, &source));
   CHECK(source.valid && source.source_sample == 1024 && record_pyrowave_audio_clock_drained(c));
   record_pyrowave_audio_clock_free(c);
}
