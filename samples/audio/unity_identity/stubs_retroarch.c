#define audio_init_thread unused_audio_init_thread
#define audio_thread_parked unused_audio_thread_parked
#define command_event unused_command_event
#define midi_driver_synth_active unused_midi_driver_synth_active
#define midi_driver_render_audio unused_midi_driver_render_audio
#include "../pipeline_clocked/stubs_retroarch.c"
#undef audio_init_thread
#undef audio_thread_parked
#undef command_event
#undef midi_driver_synth_active
#undef midi_driver_render_audio

/* An in-process synth the cases switch on: sounding, it renders a ramp
 * the test can predict; silent, it renders nothing. */
bool test_synth_on, test_synth_silent;
bool midi_driver_synth_active(void) { return test_synth_on; }
bool midi_driver_render_audio(float *out, size_t frames, unsigned rate)
{
   size_t i;
   (void)rate;
   if (!test_synth_on || test_synth_silent)
      return false;
   for (i = 0; i < frames * 2; i++)
      out[i] = (float)((int)(i % 64) - 32) / 256.0f;
   return true;
}

/* These fixtures have no wrapper worker: it is always parked. */
bool audio_thread_parked(void *data)
{
   (void)data;
   return true;
}

/* Run the real initialization and pipeline, scheduling passes explicitly. */
bool audio_init_thread(const audio_driver_t **out_driver, void **out_data,
      const char *device, unsigned out_rate, unsigned *new_rate,
      unsigned latency, bool raise_priority, bool prefer_fast_cores,
      const audio_driver_t *driver)
{
   (void)raise_priority;
   (void)prefer_fast_cores;
   *out_driver = driver;
   *out_data = driver->init(device, out_rate, latency, new_rate);
   return *out_data != NULL;
}

bool command_event(enum event_command action, void *data)
{
   (void)data;
   if (action == CMD_EVENT_DSP_FILTER_INIT)
      return true;
   unreachable("command_event");
   return false;
}
