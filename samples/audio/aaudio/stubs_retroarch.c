/* The frontend entry points audio/drivers/aaudio.c reaches for. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../../../configuration.h"
#include "../../../audio/audio_driver.h"

size_t   stub_device_latency;
uint32_t stub_requested_layout = AUDIO_LAYOUT_STEREO;

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

uint32_t audio_driver_requested_layout(void) { return stub_requested_layout; }

unsigned audio_layout_channels(uint32_t layout)
{
   unsigned n = 0;
   for (; layout; layout &= layout - 1)
      n++;
   return n;
}

void audio_driver_set_device_latency(size_t frames) { stub_device_latency = frames; }

static void stub_log(const char *fmt, va_list ap) { printf("      "); vprintf(fmt, ap); }

void RARCH_LOG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); stub_log(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); stub_log(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); stub_log(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }

/* The platform's view of its outputs, scripted. */
#include <lists/string_list.h>
#include "../../../audio/common/android_audio.h"

int32_t  stub_caps_id;
unsigned stub_caps_channels;
bool     stub_caps_ac3;

struct string_list *frontend_android_audio_outputs_new(void)
{
   union string_list_elem_attr attr;
   struct string_list *list = string_list_new();
   attr.i = 0;
   if (list)
   {
      string_list_append(list, "3 (Pixel, speaker)", attr);
      string_list_append(list, "7 (USB DAC, USB device)", attr);
   }
   return list;
}

bool frontend_android_audio_device_caps(int32_t id,
      unsigned *max_channels, bool *ac3)
{
   *max_channels = 0;
   *ac3          = false;
   if (!stub_caps_id || id != stub_caps_id)
      return false;
   *max_channels = stub_caps_channels;
   *ac3          = stub_caps_ac3;
   return true;
}
