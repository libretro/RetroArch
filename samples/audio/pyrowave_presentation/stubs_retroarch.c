/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdarg.h>
#include <stdio.h>
#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"
#include "../../../defaults.h"

struct defaults g_defaults;
static bool presentation_requested;
settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}
audio_driver_state_t *audio_state_get_ptr(void)
{
   static audio_driver_state_t state;
   return &state;
}
void audio_driver_request_presentation(bool enabled) { presentation_requested = enabled; }
bool audio_driver_presentation_requested(void) { return presentation_requested; }
void audio_driver_set_device_latency(size_t frames) { (void)frames; }
uint32_t audio_driver_requested_layout(void) { return AUDIO_LAYOUT_STEREO; }

#define PRESENTATION_LOG(name) \
   void name(const char *format, ...) \
   { \
      va_list args; \
      va_start(args, format); \
      vprintf(format, args); \
      va_end(args); \
   }
PRESENTATION_LOG(RARCH_LOG)
PRESENTATION_LOG(RARCH_WARN)
PRESENTATION_LOG(RARCH_ERR)
PRESENTATION_LOG(RARCH_DBG)
