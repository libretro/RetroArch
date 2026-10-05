/* JACK's public types, as audio/drivers/jack.c uses them, so this
 * harness builds where no JACK headers are installed. Layouts and
 * values are JACK's own, which are ABI. */
#ifndef JACK_RUNTIME_SHIM_TYPES_H
#define JACK_RUNTIME_SHIM_TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t jack_nframes_t;
typedef uint64_t jack_time_t;
typedef float    jack_default_audio_sample_t;

typedef struct _jack_client jack_client_t;
typedef struct _jack_port   jack_port_t;

#define JACK_DEFAULT_AUDIO_TYPE "32 bit float mono audio"

typedef enum JackOptions
{
   JackNullOption    = 0x00,
   JackNoStartServer = 0x01
} jack_options_t;

typedef enum JackStatus
{
   JackFailure = 0x01
} jack_status_t;

enum JackPortFlags
{
   JackPortIsInput    = 0x1,
   JackPortIsOutput   = 0x2,
   JackPortIsPhysical = 0x4
};

typedef enum JackLatencyCallbackMode
{
   JackCaptureLatency,
   JackPlaybackLatency
} jack_latency_callback_mode_t;

typedef struct _jack_latency_range
{
   jack_nframes_t min;
   jack_nframes_t max;
} jack_latency_range_t;

typedef int  (*JackProcessCallback)(jack_nframes_t nframes, void *arg);
typedef int  (*JackSampleRateCallback)(jack_nframes_t nframes, void *arg);
typedef int  (*JackBufferSizeCallback)(jack_nframes_t nframes, void *arg);
typedef int  (*JackXRunCallback)(void *arg);
typedef void (*JackShutdownCallback)(void *arg);

#endif
