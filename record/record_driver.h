#ifndef _RECORD_DRIVER_H
#define _RECORD_DRIVER_H

#include <boolean.h>
#include <retro_miscellaneous.h>

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
   STREAMING_MODE_KICK,
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

   char path[PATH_MAX_LENGTH];
   char config[PATH_MAX_LENGTH];
   char output_dir[DIR_MAX_LENGTH];
   char config_dir[DIR_MAX_LENGTH];

   bool enable;
   bool streaming_enable;
   bool use_output_dir;

   /* Multi-screen streaming support */
   struct {
      const record_driver_t *driver;
      void *data;
      bool active;
      char stream_url[256];
   } aux_streams[4]; /* Support for 4 auxiliary screens (indices 1-4) */
};

typedef struct recording recording_state_t;

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

/**
 * recording_init_aux:
 * Initialize a single auxiliary stream slot (idx 0..3, screen_id-1) with
 * the supplied DSU stream parameters. The slot gets its own record driver
 * instance that receives frames via recording_dump_frame_ext() when the
 * core submits auxiliary screens through video_refresh_ext.
 *
 * @param idx          Slot index (0..3 for screen_id 1..4).
 * @param url          Target URL (e.g. rtmp://..., udp://..., file path).
 *                     When NULL/empty the slot stays inactive.
 * @param bitrate_kbps Requested bitrate in kbps (0 = use quality preset).
 * @param width        Output width  (0 = core av_info base_width).
 * @param height       Output height (0 = core av_info base_height).
 * @param fps          Output fps    (0 = core av_info timing.fps).
 *
 * @return true on success.
 */
bool recording_init_aux(unsigned idx,
      const char *url,
      unsigned bitrate_kbps,
      unsigned width, unsigned height, unsigned fps);

/* Deinit and free a single auxiliary stream slot. */
bool recording_deinit_aux(unsigned idx);

recording_state_t *recording_state_get_ptr(void);

/**
 * recording_dump_frame_ext:
 * @data: Frame data
 * @width: Frame width
 * @height: Frame height
 * @pitch: Frame pitch
 * @is_idle: Whether core is idle
 * @screen_id: Screen identifier (0 = main, 1+ = auxiliary)
 *
 * Records a video frame from a specific screen for multi-screen streaming.
 */
void recording_dump_frame_ext(
      const void *data, unsigned width,
      unsigned height, size_t pitch, bool is_idle,
      unsigned screen_id);

extern const record_driver_t *record_drivers[];

#endif
