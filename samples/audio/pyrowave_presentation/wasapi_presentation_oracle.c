/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Exercise the shipping private mapper with scripted native COM clock reads. */
#include "../../../audio/drivers/wasapi.c"
#include <stdio.h>

static UINT64 presentation_test_raw;
static HRESULT presentation_test_result;

static HRESULT STDMETHODCALLTYPE presentation_test_get_position(IAudioClock *clock,
      UINT64 *position, UINT64 *qpc)
{
   (void)clock;
   *position = presentation_test_raw;
   if (qpc) *qpc = 0;
   return presentation_test_result;
}

static void presentation_test_require(bool condition, const char *message)
{
   if (!condition)
   {
      fprintf(stderr, "FAIL: scripted WASAPI %s\n", message);
      exit(EXIT_FAILURE);
   }
}

void wasapi_presentation_unit(void)
{
   IAudioClockVtbl vtable;
   IAudioClock native_clock;
   wasapi_t *w = (wasapi_t*)calloc(1, sizeof(*w));
   audio_presentation_position_t position;
   presentation_test_require(w != NULL, "allocation");
   presentation_test_require(!wasapi_presentation_position(w, &position), "unsupported capability");
   memset(&vtable, 0, sizeof(vtable));
   vtable.GetPosition = presentation_test_get_position;
   native_clock.lpVtbl = &vtable;
   w->clock = &native_clock;
   w->clock_frequency = 10000000;
   w->rate = 48000;
   w->presentation = record_pyrowave_clock_new(2);
   presentation_test_require(record_pyrowave_clock_reset(w->presentation, 1, 1), "clock construction");
   retro_atomic_64_init(&w->presentation_position, 0);
   w->presentation_end = 480;
   w->presentation_written = 96;
   wasapi_presentation_commit(w, 192, 96);
   presentation_test_result = S_OK;
   presentation_test_raw = 480 * (uint64_t)10000000 / 48000;
   wasapi_presentation_poll(w);
   presentation_test_require(wasapi_presentation_position(w, &position)
         && position.valid && position.played == 0, "initial silent prefill");
   presentation_test_raw = 516 * (uint64_t)10000000 / 48000;
   wasapi_presentation_poll(w);
   wasapi_presentation_position(w, &position);
   presentation_test_require(position.valid && position.played == 36, "clock frequency conversion");
   presentation_test_raw = 600 * (uint64_t)10000000 / 48000;
   wasapi_presentation_poll(w);
   wasapi_presentation_position(w, &position);
   presentation_test_require(!position.valid && position.drained && position.played == 96,
         "explicit silence preserves only tail-drain proof");
   presentation_test_result = S_FALSE;
   wasapi_presentation_poll(w);
   wasapi_presentation_position(w, &position);
   presentation_test_require(!position.valid && !position.drained && !position.failed,
         "imprecise native clock observation");
   presentation_test_result = S_OK;
   presentation_test_raw = 597 * (uint64_t)10000000 / 48000;
   wasapi_presentation_poll(w);
   wasapi_presentation_position(w, &position);
   presentation_test_require(position.failed && !position.valid && !position.drained,
         "backwards hardware clock invalidates timeline");
   record_pyrowave_clock_free(w->presentation);
   w->presentation = record_pyrowave_clock_new(2);
   presentation_test_require(record_pyrowave_clock_reset(w->presentation, 1, 1), "second construction");
   w->presentation_failed = false;
   w->presentation_cursor = 0;
   presentation_test_raw = 675 * (uint64_t)10000000 / 48000;
   wasapi_presentation_poll(w);
   wasapi_presentation_position(w, &position);
   presentation_test_require(position.failed && !position.valid && !position.drained,
         "unaccounted engine silence invalidates timeline");
   record_pyrowave_clock_free(w->presentation);
   free(w);
   puts("WASAPI presentation mapper: prefill, clock units, explicit silence, imprecise/backwards/missed positions PASS");
}
