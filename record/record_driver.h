#ifndef _RECORD_DRIVER_H
#define _RECORD_DRIVER_H

#include <boolean.h>

enum ffemu_pix_format
{
   FFEMU_PIX_RGB565 = 0,
   FFEMU_PIX_BGR24,
   FFEMU_PIX_ARGB8888
};

enum streaming_mode
{
   STREAMING_MODE_TWITCH = 0,
   STREAMING_MODE_YOUTUBE,
   STREAMING_MODE_FACEBOOK,
   STREAMING_MODE_LOCAL,
   STREAMING_MODE_CUSTOM
};

enum record_config_type
{
   RECORD_CONFIG_TYPE_RECORDING_CUSTOM = 0,
   RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_GIF,
   RECORD_CONFIG_TYPE_RECORDING_APNG,
   RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   RECORD_CONFIG_TYPE_STREAMING_NETPLAY
};

/* Parameters passed to ffemu_new() */
struct record_params
{
   /* Framerate per second of input video. */
   double fps;
   /* Sample rate of input audio. */
   double samplerate;

   /* Filename to dump to. */
   const char *filename;

   /* Path to config. Optional. */
   const char *config;

   const char *audio_resampler;

   /* Desired output resolution. */
   unsigned out_width;
   unsigned out_height;

   /* Total size of framebuffer used in input. */
   unsigned fb_width;
   unsigned fb_height;

   /* Audio channels. */
   unsigned channels;

   unsigned video_record_scale_factor;
   unsigned video_stream_scale_factor;
   unsigned video_record_threads;
   unsigned streaming_mode;

   /* Aspect ratio of input video. Parameters are passed to the muxer,
    * the video itself is not scaled.
    */
   float aspect_ratio;

   enum record_config_type preset;

   /* Input pixel format. */
   enum ffemu_pix_format pix_fmt;

   bool video_gpu_record;
};

struct record_video_data
{
   const void *data;
   unsigned width;
   unsigned height;
   int pitch;
   bool is_dupe;
};

struct record_audio_data
{
   const void *data;
   size_t frames;
};

typedef struct record_driver
{
   void *(*init)(const struct record_params *params);
   void  (*free)(void *data);
   bool  (*push_video)(void *data,
         const struct record_video_data *video_data);
   bool  (*push_audio)(void *data,
         const struct record_audio_data *audio_data);
   bool  (*finalize)(void *data);
   const char *ident;
} record_driver_t;


struct recording
{
   const record_driver_t *driver;
   void *data;

   size_t gpu_width;
   size_t gpu_height;

   unsigned width;
   unsigned height;

   char path[8192];
   char config[8192];
   char output_dir[8192];
   char config_dir[8192];

   bool enable;
   bool streaming_enable;
   bool use_output_dir;
};

typedef struct recording recording_state_t;

extern const record_driver_t record_ffmpeg;

/**
 * config_get_record_driver_options:
 *
 * Get an enumerated list of all record driver names, separated by '|'.
 *
 * Returns: string listing of all record driver names, separated by '|'.
 **/
const char* config_get_record_driver_options(void);

void recording_driver_update_streaming_url(void);

bool recording_deinit(void);

/**
 * recording_init:
 *
 * Initializes recording.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool recording_init(void);

void streaming_set_state(bool state);

recording_state_t *recording_state_get_ptr(void);

extern const record_driver_t *record_drivers[];

#endif
