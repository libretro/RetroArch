/////
// API header for external RetroArch audio driver plugins.
//
//

#ifndef __SSNES_AUDIO_DRIVER_PLUGIN_H
#define __SSNES_AUDIO_DRIVER_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef SSNES_DLL_IMPORT
#define SSNES_API_EXPORT __declspec(dllimport) 
#else
#define SSNES_API_EXPORT __declspec(dllexport) 
#endif
#define SSNES_API_CALLTYPE __cdecl
#else
#define SSNES_API_EXPORT
#define SSNES_API_CALLTYPE
#endif

#ifndef SSNES_TRUE
#define SSNES_TRUE 1
#endif

#ifndef SSNES_FALSE
#define SSNES_FALSE 0
#endif

#ifndef SSNES_OK
#define SSNES_OK 1
#endif

#ifndef SSNES_ERROR
#define SSNES_ERROR 0
#endif

#define SSNES_AUDIO_API_VERSION 2

typedef struct ssnes_audio_driver_info
{
   // A hint for a subdevice of the audio driver.
   // This is driver independent, and not relevant for all
   // audio drivers. I.e. ALSA driver might use "hw:0",
   // OSS "/dev/audio", etc.
   const char *device;

   // Audio sample rate.
   unsigned sample_rate;

   // Maximum audio latency requested for output,
   // measured in milliseconds.
   // If driver is not able to provide this latency, it can
   // be disregarded.
   unsigned latency;
} ssnes_audio_driver_info_t;

typedef struct ssnes_audio_driver
{
   // Initializes the device.
   void *(*init)(const ssnes_audio_driver_info_t *info);

   // Write data in buffer to audio driver.
   // A frame here is defined as one combined sample of left and right
   // channels. (I.e. 44.1kHz, 16-bit stereo has 88.2k samples/s, and
   // 44.1k frames/s.)
   //
   // Samples are interleaved in format LRLRLRLRLR ...
   // If the driver returns true in use_float(), a floating point
   // format will be used, with range [-1.0, 1.0].
   // If not, signed 16-bit samples in native byte ordering will be used.
   // 
   // This function returns the number of frames successfully written.
   // If an error occurs, -1 should be returned.
   // Note that non-blocking behavior that cannot write at this time
   // should return 0 as returning -1 will terminate the driver.
   //
   // Unless said otherwise with set_nonblock_state(), all writes
   // are blocking, and it should block till it has written all frames.
   int (*write)(void *data, const void *buffer, unsigned frames);

   // Temporarily pauses the audio driver.
   int (*stop)(void *data);

   // Resumes audio driver from the paused state.
   int (*start)(void *data);

   // If state is true, nonblocking operation is assumed.
   // This is typically used for fast-forwarding. If driver cannot
   // implement nonblocking writes, this can be disregarded, but should
   // log a message to stderr.
   void (*set_nonblock_state)(void *data, int state);

   // Stops and frees the audio driver.
   void (*free)(void *data);

   // If true is returned, the audio driver is capable of using
   // floating point data. This will likely increase performance as the
   // resampler unit uses floating point. The sample range is
   // [-1.0, 1.0].
   int (*use_float)(void *data);

   // The driver might be forced to use a certain output frequency
   // (i.e. Jack), and thus to avoid double resampling, the driver
   // can request RetroArch to resample to a different sample rate.
   // This function can be set to NULL if the driver does not
   // desire to override the sample rate.
   unsigned (*sample_rate)(void *data);

   // Human readable identification string for the driver.
   const char *ident;

   // Must be set to SSNES_AUDIO_API_VERSION.
   // Used for detecting API mismatch.
   int api_version;
} ssnes_audio_driver_t;

SSNES_API_EXPORT const ssnes_audio_driver_t* SSNES_API_CALLTYPE
   ssnes_audio_driver_init(void);

#ifdef __cplusplus
}
#endif

#endif
