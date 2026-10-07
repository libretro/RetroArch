/* JACK's client API, the entry points audio/drivers/jack.c calls,
 * with JACK's own prototypes. Declared only: the driver reaches them
 * through the library it loads, and this harness links none. */
#ifndef JACK_RUNTIME_SHIM_JACK_H
#define JACK_RUNTIME_SHIM_JACK_H

#include <jack/types.h>

jack_client_t *jack_client_open(const char *client_name,
      jack_options_t options, jack_status_t *status, ...);
int jack_client_close(jack_client_t *client);
int jack_activate(jack_client_t *client);
int jack_deactivate(jack_client_t *client);
int jack_connect(jack_client_t *client, const char *source_port,
      const char *destination_port);
void jack_free(void *ptr);
jack_nframes_t jack_get_buffer_size(jack_client_t *client);
int jack_get_cycle_times(const jack_client_t *client,
      jack_nframes_t *current_frames, jack_time_t *current_usecs,
      jack_time_t *next_usecs, float *period_usecs);
const char **jack_get_ports(jack_client_t *client,
      const char *port_name_pattern, const char *type_name_pattern,
      unsigned long flags);
jack_nframes_t jack_get_sample_rate(jack_client_t *client);
void jack_on_shutdown(jack_client_t *client,
      JackShutdownCallback shutdown_callback, void *arg);
void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t nframes);
void jack_port_get_latency_range(jack_port_t *port,
      jack_latency_callback_mode_t mode, jack_latency_range_t *range);
const char *jack_port_name(const jack_port_t *port);
jack_port_t *jack_port_register(jack_client_t *client,
      const char *port_name, const char *port_type,
      unsigned long flags, unsigned long buffer_size);
int jack_set_buffer_size_callback(jack_client_t *client,
      JackBufferSizeCallback bufsize_callback, void *arg);
int jack_set_process_callback(jack_client_t *client,
      JackProcessCallback process_callback, void *arg);
int jack_set_sample_rate_callback(jack_client_t *client,
      JackSampleRateCallback srate_callback, void *arg);
int jack_set_xrun_callback(jack_client_t *client,
      JackXRunCallback xrun_callback, void *arg);

#endif
