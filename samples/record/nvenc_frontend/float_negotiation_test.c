#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"
static audio_driver_state_t audio;
static settings_t settings;
audio_driver_state_t *audio_state_get_ptr(void) { return &audio; }
settings_t *config_get_ptr(void) { return &settings; }
void audio_driver_set_core_float(bool enabled) { audio.core_float = enabled; }
size_t audio_driver_sample_batch_float(const float *data, size_t frames)
{ (void)data; return frames; }
static bool query(unsigned cmd, void *data)
{
   switch (cmd)
   {
#include "float_fragment.c"
      default: return false;
   }
   return true;
}
int main(void)
{
   struct retro_audio_sample_float_callback cb;
   unsigned command = RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_FLOAT;
   memset(&audio, 0, sizeof(audio));
   memset(&settings, 0, sizeof(settings));
   settings.uints.audio_format_negotiation = AUDIO_FORMAT_NEGOTIATION_FLOAT;
   AUDIO_FLAGS_SET(&audio, AUDIO_FLAG_ACTIVE);
   assert(query(command, &cb) && cb.batch && audio.core_float);
   audio.core_float = false;
   audio.context_audio_data = &audio;
   assert(!query(command, &cb) && !audio.core_float);
   AUDIO_FLAGS_SET(&audio, AUDIO_FLAG_USE_FLOAT);
   assert(query(command, &cb));
   settings.bools.audio_fastpath_s16 = true;
   assert(!query(command, &cb));
   settings.bools.audio_fastpath_s16 = false;
   audio.context_audio_data = NULL;
   settings.uints.audio_format_negotiation = AUDIO_FORMAT_NEGOTIATION_INT16;
   assert(!query(command, &cb));
   assert(!query(command, NULL));
   puts("PASS: core float negotiation before device initialization and with live formats");
   return 0;
}
