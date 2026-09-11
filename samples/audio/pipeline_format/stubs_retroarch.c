#define audio_init_thread unused_audio_init_thread
#define command_event unused_command_event
#include "../pipeline_clocked/stubs_retroarch.c"
#undef audio_init_thread
#undef command_event

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
