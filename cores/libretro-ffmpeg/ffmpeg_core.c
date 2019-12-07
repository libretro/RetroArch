#include <retro_common_api.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

#ifdef RARCH_INTERNAL
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#ifdef HAVE_SWRESAMPLE
#include <libswresample/swresample.h>
#endif

#ifdef HAVE_SSA
#include <ass/ass.h>
#endif

#ifdef __cplusplus
}
#endif

#ifdef HAVE_GL_FFT
#include "ffmpeg_fft.h"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include <glsym/glsym.h>
#endif

#include <rthreads/rthreads.h>
#include <queues/fifo_queue.h>
#include <string/stdstring.h>

#include <libretro.h>
#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#define CORE_PREFIX(s) libretro_ffmpeg_##s
#else
#define CORE_PREFIX(s) s
#endif

#ifndef PIX_FMT_RGB32
#define PIX_FMT_RGB32 AV_PIX_FMT_RGB32
#endif

#define PRINT_VERSION(s) log_cb(RETRO_LOG_INFO, "[FFMPEG] lib%s version:\t%d.%d.%d\n", #s, \
   s ##_version() >> 16 & 0xFF, \
   s ##_version() >> 8 & 0xFF, \
   s ##_version() & 0xFF);

static bool reset_triggered;
static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

retro_log_printf_t log_cb;
static retro_video_refresh_t CORE_PREFIX(video_cb);
static retro_audio_sample_t CORE_PREFIX(audio_cb);
static retro_audio_sample_batch_t CORE_PREFIX(audio_batch_cb);
static retro_environment_t CORE_PREFIX(environ_cb);
static retro_input_poll_t CORE_PREFIX(input_poll_cb);
static retro_input_state_t CORE_PREFIX(input_state_cb);

/* FFmpeg context data. */
static AVFormatContext *fctx;
static AVCodecContext *vctx;
static int video_stream_index;
static enum AVColorSpace colorspace;
static int sw_decoder_threads;

#if LIBAVUTIL_VERSION_MAJOR > 55
static enum AVHWDeviceType hw_decoder;
static bool hw_decoding_enabled;
static enum AVPixelFormat pix_fmt;
static bool force_sw_decoder;
#endif


#define MAX_STREAMS 8
static AVCodecContext *actx[MAX_STREAMS];
static AVCodecContext *sctx[MAX_STREAMS];
static int audio_streams[MAX_STREAMS];
static int audio_streams_num;
static int audio_streams_ptr;
static int subtitle_streams[MAX_STREAMS];
static int subtitle_streams_num;
static int subtitle_streams_ptr;

#ifdef HAVE_SSA
/* AAS/SSA subtitles. */
static ASS_Library *ass;
static ASS_Renderer *ass_render;
static ASS_Track *ass_track[MAX_STREAMS];
static uint8_t *ass_extra_data[MAX_STREAMS];
static size_t ass_extra_data_size[MAX_STREAMS];
#endif

struct attachment
{
   uint8_t *data;
   size_t size;
};
static struct attachment *attachments;
static size_t attachments_size;

#ifdef HAVE_GL_FFT
static fft_t *fft;
unsigned fft_width;
unsigned fft_height;
unsigned fft_multisample;
#endif

/* A/V timing. */
static uint64_t frame_cnt;
static uint64_t audio_frames;
static double pts_bias;

/* Threaded FIFOs. */
static volatile bool decode_thread_dead;
static fifo_buffer_t *video_decode_fifo;
static fifo_buffer_t *audio_decode_fifo;
static scond_t *fifo_cond;
static scond_t *fifo_decode_cond;
static slock_t *fifo_lock;
static slock_t *decode_thread_lock;
static sthread_t *decode_thread_handle;
static double decode_last_video_time;
static double decode_last_audio_time;

static uint32_t *video_frame_temp_buffer;

static bool main_sleeping;

/* Seeking. */
static bool do_seek;
static double seek_time;

/* GL stuff */
struct frame
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLuint tex;
#if !defined(HAVE_OPENGLES)
   GLuint pbo;
#endif
#endif
   double pts;
};

static struct frame frames[2];

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
static bool temporal_interpolation;
static bool use_gl;
static struct retro_hw_render_callback hw_render;
static GLuint prog;
static GLuint vbo;
static GLint vertex_loc;
static GLint tex_loc;
static GLint mix_loc;
#endif

static struct
{
   unsigned width;
   unsigned height;
   
   double interpolate_fps;
   unsigned sample_rate;

   float aspect;
} media;

#ifdef HAVE_SSA
static void ass_msg_cb(int level, const char *fmt, va_list args, void *data)
{
   char buffer[4096];
   (void)data;

   if (level < 6)
   {
      vsnprintf(buffer, sizeof(buffer), fmt, args);
      log_cb(RETRO_LOG_INFO, "[FFMPEG] %s\n", buffer);
   }
}
#endif

static void append_attachment(const uint8_t *data, size_t size)
{
   attachments = (struct attachment*)av_realloc(
         attachments, (attachments_size + 1) * sizeof(*attachments));

   attachments[attachments_size].data = (uint8_t*)av_malloc(size);
   attachments[attachments_size].size = size;
   memcpy(attachments[attachments_size].data, data, size);

   attachments_size++;
}

void CORE_PREFIX(retro_init)(void)
{
   reset_triggered = false;

   av_register_all();
}

void CORE_PREFIX(retro_deinit)(void)
{}

unsigned CORE_PREFIX(retro_api_version)(void)
{
   return RETRO_API_VERSION;
}

void CORE_PREFIX(retro_set_controller_port_device)(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void CORE_PREFIX(retro_get_system_info)(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "FFmpeg";
   info->library_version  = "v1";
   info->need_fullpath    = true;
   info->valid_extensions = "mkv|avi|f4v|f4f|3gp|ogm|flv|mp4|mp3|flac|ogg|m4a|webm|3g2|mov|wmv|mpg|mpeg|vob|asf|divx|m2p|m2ts|ps|ts|mxf|wma|wav";
}

void CORE_PREFIX(retro_get_system_av_info)(struct retro_system_av_info *info)
{
   unsigned width  = vctx ? media.width : 320;
   unsigned height = vctx ? media.height : 240;
   float aspect    = vctx ? media.aspect : 0.0;

   info->timing.fps = media.interpolate_fps;
   info->timing.sample_rate = actx[0] ? media.sample_rate : 32000.0;

#ifdef HAVE_GL_FFT
   if (audio_streams_num > 0 && video_stream_index < 0)
   {
      width = fft_width;
      height = fft_height;
      aspect = 16.0 / 9.0;
   }
#endif

   info->geometry.base_width   = width;
   info->geometry.base_height  = height;
   info->geometry.max_width    = width;
   info->geometry.max_height   = height;
   info->geometry.aspect_ratio = aspect;
}

void CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
#if LIBAVUTIL_VERSION_MAJOR > 55
      { "ffmpeg_hw_decoder", "Use Hardware decoder (restart); auto|off|"
         "cuda|d3d11va|drm|dxva2|mediacodec|opencl|qsv|vaapi|vdpau|videotoolbox" },
#endif
      { "ffmpeg_sw_decoder_threads", "Software decoder thread count (restart); 1|2|4|8|16" },
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
      { "ffmpeg_temporal_interp", "Temporal Interpolation; disabled|enabled" },
#ifdef HAVE_GL_FFT
      { "ffmpeg_fft_resolution", "FFT Resolution; 1280x720|1920x1080|2560x1440|3840x2160|640x360|320x180" },
      { "ffmpeg_fft_multisample", "FFT Multisample; 1x|2x|4x" },
#endif
#endif
      { "ffmpeg_color_space", "Colorspace; auto|BT.709|BT.601|FCC|SMPTE240M" },
      { NULL, NULL },
   };
   struct retro_log_callback log;

   CORE_PREFIX(environ_cb) = cb;

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = fallback_log;
}

void CORE_PREFIX(retro_set_audio_sample)(retro_audio_sample_t cb)
{
   CORE_PREFIX(audio_cb) = cb;
}

void CORE_PREFIX(retro_set_audio_sample_batch)(retro_audio_sample_batch_t cb)
{
   CORE_PREFIX(audio_batch_cb) = cb;
}

void CORE_PREFIX(retro_set_input_poll)(retro_input_poll_t cb)
{
   CORE_PREFIX(input_poll_cb) = cb;
}

void CORE_PREFIX(retro_set_input_state)(retro_input_state_t cb)
{
   CORE_PREFIX(input_state_cb) = cb;
}

void CORE_PREFIX(retro_set_video_refresh)(retro_video_refresh_t cb)
{
   CORE_PREFIX(video_cb) = cb;
}

void CORE_PREFIX(retro_reset)(void)
{
   reset_triggered = true;
}

static void print_ffmpeg_version()
{
   PRINT_VERSION(avformat)
   PRINT_VERSION(avcodec)
   PRINT_VERSION(avutil)
   PRINT_VERSION(swresample)
   PRINT_VERSION(swscale)
}

static void check_variables(bool firststart)
{
   struct retro_variable hw_var  = {0};
   struct retro_variable sw_threads_var = {0};
   struct retro_variable color_var  = {0};
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   struct retro_variable var        = {0};
#endif
#ifdef HAVE_GL_FFT
   struct retro_variable fft_var    = {0};
   struct retro_variable fft_ms_var = {0};
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   var.key = "ffmpeg_temporal_interp";

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (memcmp(var.value, "enabled", 7) == 0)
         temporal_interpolation = true;
      else if (memcmp(var.value, "disabled", 8) == 0)
         temporal_interpolation = false;
   }

#ifdef HAVE_GL_FFT
   fft_var.key = "ffmpeg_fft_resolution";

   fft_width       = 1280;
   fft_height      = 720;
   fft_multisample = 1;
   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &fft_var) && fft_var.value)
   {
      unsigned w, h;
      if (sscanf(fft_var.value, "%ux%u", &w, &h) == 2)
      {
         fft_width = w;
         fft_height = h;
      }
   }

   fft_ms_var.key = "ffmpeg_fft_multisample";

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &fft_ms_var) && fft_ms_var.value)
      fft_multisample = strtoul(fft_ms_var.value, NULL, 0);
#endif
#endif

   color_var.key = "ffmpeg_color_space";

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &color_var) && color_var.value)
   {
      slock_lock(decode_thread_lock);
      if (string_is_equal(color_var.value, "BT.709"))
         colorspace = AVCOL_SPC_BT709;
      else if (string_is_equal(color_var.value, "BT.601"))
         colorspace = AVCOL_SPC_BT470BG;
      else if (memcmp(color_var.value, "FCC", 3) == 0)
         colorspace = AVCOL_SPC_FCC;
      else if (string_is_equal(color_var.value, "SMPTE240M"))
         colorspace = AVCOL_SPC_SMPTE240M;
      else
         colorspace = AVCOL_SPC_UNSPECIFIED;
      slock_unlock(decode_thread_lock);
   }

#if LIBAVUTIL_VERSION_MAJOR > 55
   if (firststart)
   {
      hw_var.key = "ffmpeg_hw_decoder";

      force_sw_decoder = false;
      hw_decoder = AV_HWDEVICE_TYPE_NONE;

      if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &hw_var) && hw_var.value)
      {
         if (string_is_equal(hw_var.value, "off"))
            force_sw_decoder = true;
         else if (string_is_equal(hw_var.value, "cuda"))
            hw_decoder = AV_HWDEVICE_TYPE_CUDA;
         else if (string_is_equal(hw_var.value, "d3d11va"))
            hw_decoder = AV_HWDEVICE_TYPE_D3D11VA;
         else if (string_is_equal(hw_var.value, "drm"))
            hw_decoder = AV_HWDEVICE_TYPE_DRM;
         else if (string_is_equal(hw_var.value, "dxva2"))
            hw_decoder = AV_HWDEVICE_TYPE_DXVA2;
         else if (string_is_equal(hw_var.value, "mediacodec"))
            hw_decoder = AV_HWDEVICE_TYPE_MEDIACODEC;
         else if (string_is_equal(hw_var.value, "opencl"))
            hw_decoder = AV_HWDEVICE_TYPE_OPENCL;
         else if (string_is_equal(hw_var.value, "qsv"))
            hw_decoder = AV_HWDEVICE_TYPE_QSV;
         else if (string_is_equal(hw_var.value, "vaapi"))
            hw_decoder = AV_HWDEVICE_TYPE_VAAPI;
         else if (string_is_equal(hw_var.value, "vdpau"))
            hw_decoder = AV_HWDEVICE_TYPE_VDPAU;
         else if (string_is_equal(hw_var.value, "videotoolbox"))
            hw_decoder = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
      }
   }
#endif

   sw_threads_var.key = "ffmpeg_sw_decoder_threads";
   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &sw_threads_var) && sw_threads_var.value)
   {
      slock_lock(decode_thread_lock);
      sw_decoder_threads = strtoul(sw_threads_var.value, NULL, 0);
      slock_unlock(decode_thread_lock);
   }
}

static void seek_frame(int seek_frames)
{
   char msg[256];
   struct retro_message msg_obj = {0};

   if ((seek_frames < 0 && (unsigned)-seek_frames > frame_cnt) || reset_triggered)
      frame_cnt = 0;
   else
      frame_cnt += seek_frames;

   slock_lock(fifo_lock);

   do_seek        = true;
   seek_time      = frame_cnt / media.interpolate_fps;

   snprintf(msg, sizeof(msg), "Seek: %u s.", (unsigned)seek_time);
   msg_obj.msg    = msg;
   msg_obj.frames = 180;
   CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &msg_obj);

   if (seek_frames < 0)
   {
      log_cb(RETRO_LOG_INFO, "[FFMPEG] Resetting PTS.\n");
      frames[0].pts = 0.0;
      frames[1].pts = 0.0;
   }
   audio_frames = frame_cnt * media.sample_rate / media.interpolate_fps;

   if (video_decode_fifo)
      fifo_clear(video_decode_fifo);
   if (audio_decode_fifo)
      fifo_clear(audio_decode_fifo);
   scond_signal(fifo_decode_cond);

   while (!decode_thread_dead && do_seek)
      scond_wait(fifo_cond, fifo_lock);
   slock_unlock(fifo_lock);
}

void CORE_PREFIX(retro_run)(void)
{
   static bool last_left;
   static bool last_right;
   static bool last_up;
   static bool last_down;
   static bool last_l;
   static bool last_r;
   double min_pts;
   int16_t audio_buffer[2048];
   bool left, right, up, down, l, r;
   size_t to_read_frames        = 0;
   int seek_frames              = 0;
   bool updated                 = false;
#ifdef HAVE_GL_FFT
   unsigned old_fft_width       = fft_width;
   unsigned old_fft_height      = fft_height;
   unsigned old_fft_multisample = fft_multisample;
#endif

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables(false);

#ifdef HAVE_GL_FFT
   if (fft_width != old_fft_width || fft_height != old_fft_height)
   {
      struct retro_system_av_info info;
      CORE_PREFIX(retro_get_system_av_info)(&info);
      if (!CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info))
      {
         fft_width = old_fft_width;
         fft_height = old_fft_height;
      }
   }

   if (fft && (old_fft_multisample != fft_multisample))
      fft_init_multisample(fft, fft_width, fft_height, fft_multisample);
#endif

   CORE_PREFIX(input_poll_cb)();

   left = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_LEFT);
   right = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_RIGHT);
   up = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_UP) ||
      CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
   down = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_DOWN) ||
      CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
   l = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_L);
   r = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_R);

   if (left && !last_left)
      seek_frames -= 10 * media.interpolate_fps;
   if (right && !last_right)
      seek_frames += 10 * media.interpolate_fps;
   if (up && !last_up)
      seek_frames += 60 * media.interpolate_fps;
   if (down && !last_down)
      seek_frames -= 60 * media.interpolate_fps;

   if (l && !last_l && audio_streams_num > 0)
   {
      char msg[256];
      struct retro_message msg_obj = {0};

      slock_lock(decode_thread_lock);
      audio_streams_ptr = (audio_streams_ptr + 1) % audio_streams_num;
      slock_unlock(decode_thread_lock);

      snprintf(msg, sizeof(msg), "Audio Track #%d.", audio_streams_ptr);

      msg_obj.msg    = msg;
      msg_obj.frames = 180;
      CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &msg_obj);
   }
   else if (r && !last_r && subtitle_streams_num > 0)
   {
      char msg[256];
      struct retro_message msg_obj = {0};

      slock_lock(decode_thread_lock);
      subtitle_streams_ptr = (subtitle_streams_ptr + 1) % subtitle_streams_num;
      slock_unlock(decode_thread_lock);

      snprintf(msg, sizeof(msg), "Subtitle Track #%d.", subtitle_streams_ptr);
      msg_obj.msg    = msg;
      msg_obj.frames = 180;
      CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &msg_obj);
   }

   last_left  = left;
   last_right = right;
   last_up    = up;
   last_down  = down;
   last_l     = l;
   last_r     = r;

   if (reset_triggered)
   {
      seek_frames = -1;
      seek_frame(seek_frames);
      reset_triggered = false;
   }

   /* Push seek request to thread,
    * wait for seek to complete. */
   if (seek_frames)
      seek_frame(seek_frames);

   if (decode_thread_dead)
   {
      CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      return;
   }

   frame_cnt++;

   /* Have to decode audio before video
    * incase there are PTS fuckups due
    * to seeking. */
   if (audio_streams_num > 0)
   {
      /* Audio */
      double reading_pts;
      double expected_pts;
      double old_pts_bias;
      size_t to_read_bytes;
      uint64_t expected_audio_frames = frame_cnt * media.sample_rate / media.interpolate_fps;

      to_read_frames = expected_audio_frames - audio_frames;
      to_read_bytes = to_read_frames * sizeof(int16_t) * 2;

      slock_lock(fifo_lock);
      while (!decode_thread_dead && fifo_read_avail(audio_decode_fifo) < to_read_bytes)
      {
         main_sleeping = true;
         scond_signal(fifo_decode_cond);
         scond_wait(fifo_cond, fifo_lock);
         main_sleeping = false;
      }

      reading_pts  = decode_last_audio_time -
         (double)fifo_read_avail(audio_decode_fifo) / (media.sample_rate * sizeof(int16_t) * 2);
      expected_pts = (double)audio_frames / media.sample_rate;
      old_pts_bias = pts_bias;
      pts_bias     = reading_pts - expected_pts;

      if (pts_bias < old_pts_bias - 1.0)
      {
         log_cb(RETRO_LOG_INFO, "[FFMPEG] Resetting PTS (bias).\n");
         frames[0].pts = 0.0;
         frames[1].pts = 0.0;
      }

      if (!decode_thread_dead)
         fifo_read(audio_decode_fifo, audio_buffer, to_read_bytes);
      scond_signal(fifo_decode_cond);

      slock_unlock(fifo_lock);
      audio_frames += to_read_frames;
   }

   min_pts = frame_cnt / media.interpolate_fps + pts_bias;

   if (video_stream_index >= 0)
   {
      bool dupe = true; /* unused if GL enabled */

      /* Video */
      if (min_pts > frames[1].pts)
      {
         struct frame tmp = frames[1];
         frames[1] = frames[0];
         frames[0] = tmp;
      }

      while (!decode_thread_dead && min_pts > frames[1].pts)
      {
         size_t to_read_frame_bytes;
         int64_t pts = 0;

         slock_lock(fifo_lock);
         to_read_frame_bytes = media.width * media.height * sizeof(uint32_t) + sizeof(int64_t);

         while (!decode_thread_dead && fifo_read_avail(video_decode_fifo) < to_read_frame_bytes)
         {
            main_sleeping = true;
            scond_signal(fifo_decode_cond);
            scond_wait(fifo_cond, fifo_lock);
            main_sleeping = false;
         }

         if (!decode_thread_dead)
         {
            uint32_t *data = video_frame_temp_buffer;
            fifo_read(video_decode_fifo, &pts, sizeof(int64_t));
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
            if (use_gl)
            {
#ifndef HAVE_OPENGLES
               glBindBuffer(GL_PIXEL_UNPACK_BUFFER, frames[1].pbo);
#ifdef __MACH__
               data = (uint32_t*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
#else
               data = (uint32_t*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER,
                     0, media.width * media.height * sizeof(uint32_t), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
#endif
#endif

               fifo_read(video_decode_fifo, data, media.width * media.height * sizeof(uint32_t));

#ifndef HAVE_OPENGLES
               glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
#endif
               glBindTexture(GL_TEXTURE_2D, frames[1].tex);
#if defined(HAVE_OPENGLES)
               glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     media.width, media.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
#else
               glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     media.width, media.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
#endif
               glBindTexture(GL_TEXTURE_2D, 0);
#ifndef HAVE_OPENGLES
               glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif
            }
            else
#endif
            {
               fifo_read(video_decode_fifo, data, media.width * media.height * sizeof(uint32_t));
               dupe = false;
            }
         }

         scond_signal(fifo_decode_cond);
         slock_unlock(fifo_lock);

         frames[1].pts = av_q2d(fctx->streams[video_stream_index]->time_base) * pts;
      }

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
      if (use_gl)
      {
         float mix_factor = (min_pts - frames[0].pts) / (frames[1].pts - frames[0].pts);

         if (!temporal_interpolation)
            mix_factor = 1.0f;

         glBindFramebuffer(GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
         glClearColor(0, 0, 0, 1);
         glClear(GL_COLOR_BUFFER_BIT);
         glViewport(0, 0, media.width, media.height);
         glUseProgram(prog);

         glUniform1f(mix_loc, mix_factor);
         glActiveTexture(GL_TEXTURE1);
         glBindTexture(GL_TEXTURE_2D, frames[1].tex);
         glActiveTexture(GL_TEXTURE0);
         glBindTexture(GL_TEXTURE_2D, frames[0].tex);

         glBindBuffer(GL_ARRAY_BUFFER, vbo);
         glVertexAttribPointer(vertex_loc, 2, GL_FLOAT, GL_FALSE,
               4 * sizeof(GLfloat), (const GLvoid*)(0 * sizeof(GLfloat)));
         glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE,
               4 * sizeof(GLfloat), (const GLvoid*)(2 * sizeof(GLfloat)));
         glEnableVertexAttribArray(vertex_loc);
         glEnableVertexAttribArray(tex_loc);
         glBindBuffer(GL_ARRAY_BUFFER, 0);

         glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
         glDisableVertexAttribArray(vertex_loc);
         glDisableVertexAttribArray(tex_loc);

         glUseProgram(0);
         glActiveTexture(GL_TEXTURE1);
         glBindTexture(GL_TEXTURE_2D, 0);
         glActiveTexture(GL_TEXTURE0);
         glBindTexture(GL_TEXTURE_2D, 0);

         CORE_PREFIX(video_cb)(RETRO_HW_FRAME_BUFFER_VALID,
               media.width, media.height, media.width * sizeof(uint32_t));
      }
      else
#endif
      {
         CORE_PREFIX(video_cb)(dupe ? NULL : video_frame_temp_buffer,
               media.width, media.height, media.width * sizeof(uint32_t));
      }
   }
#if defined(HAVE_GL_FFT) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGLES))
   else if (fft)
   {
      unsigned       frames = to_read_frames;
      const int16_t *buffer = audio_buffer;

      while (frames)
      {
         unsigned to_read = frames;

         /* FFT size we use (1 << 11). Really shouldn't happen,
          * unless we use a crazy high sample rate. */
         if (to_read > (1 << 11))
            to_read = 1 << 11;

         fft_step_fft(fft, buffer, to_read);
         buffer += to_read * 2;
         frames -= to_read;
      }
      fft_render(fft, hw_render.get_current_framebuffer(), fft_width, fft_height);
      CORE_PREFIX(video_cb)(RETRO_HW_FRAME_BUFFER_VALID,
            fft_width, fft_height, fft_width * sizeof(uint32_t));
   }
#endif
   else
      CORE_PREFIX(video_cb)(NULL, 1, 1, sizeof(uint32_t));

   if (to_read_frames)
      CORE_PREFIX(audio_batch_cb)(audio_buffer, to_read_frames);
}

#if LIBAVUTIL_VERSION_MAJOR > 55
/*
 * Try to initialize a specific HW decoder defined by type.
 * Optionaly tests the pixel format list for a compatible pixel format.
 */
static enum AVPixelFormat init_hw_decoder(struct AVCodecContext *ctx,
                                    const enum AVHWDeviceType type,
                                    const enum AVPixelFormat *pix_fmts)
{
   int ret;
   enum AVPixelFormat decoder_pix_fmt = AV_PIX_FMT_NONE;
   struct AVCodec *codec = avcodec_find_decoder(fctx->streams[video_stream_index]->codec->codec_id);

   for (int i = 0;; i++)
   {
      const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
      if (!config)
      {
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Codec %s is not supported by HW video decoder %s.\n",
                  codec->name, av_hwdevice_get_type_name(type));
         break;
      }
      if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
         config->device_type == type)
      {
         log_cb(RETRO_LOG_INFO, "[FFMPEG] Selected HW decoder %s.\n",
                  av_hwdevice_get_type_name(type));
         log_cb(RETRO_LOG_INFO, "[FFMPEG] Selected HW pixel format %s.\n",
                  av_get_pix_fmt_name(config->pix_fmt));
         
         enum AVPixelFormat device_pix_fmt = config->pix_fmt;

         if (pix_fmts != NULL)
         {
            /* Look if codec can supports the pix format of the device */
            for (size_t i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++)
               if (pix_fmts[i] == device_pix_fmt)
               {
                  decoder_pix_fmt = pix_fmts[i];
                  goto exit;
               }
            log_cb(RETRO_LOG_ERROR, "[FFMPEG] Codec %s does not support device pixel format %s.\n",
                  codec->name, av_get_pix_fmt_name(config->pix_fmt));
         }
         else
         {
            decoder_pix_fmt = device_pix_fmt;
            goto exit;
         }
         
      }
   }

exit:
   if (decoder_pix_fmt != AV_PIX_FMT_NONE)
   {
      AVBufferRef *hw_device_ctx;
      if ((ret = av_hwdevice_ctx_create(&hw_device_ctx,
                                       type, NULL, NULL, 0)) < 0)
      {
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to create specified HW device: %s\n", av_err2str(ret));
         decoder_pix_fmt = AV_PIX_FMT_NONE;
      }
      else
         ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
   }

   return decoder_pix_fmt;
}

/* Automatically try to find a suitable HW decoder */
static enum AVPixelFormat auto_hw_decoder(AVCodecContext *ctx,
                                    const enum AVPixelFormat *pix_fmts)
{
   int ret;
   enum AVPixelFormat decoder_pix_fmt = AV_PIX_FMT_NONE;
   enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;

   while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
   {
      decoder_pix_fmt = init_hw_decoder(ctx, type, pix_fmts);
      if (decoder_pix_fmt != AV_PIX_FMT_NONE)
         break;
   }

   return decoder_pix_fmt;
}
#endif


static enum AVPixelFormat select_decoder(AVCodecContext *ctx,
                                    const enum AVPixelFormat *pix_fmts)
{
   enum AVPixelFormat format = AV_PIX_FMT_NONE;

#if LIBAVUTIL_VERSION_MAJOR > 55
   if (!force_sw_decoder)
   {
      if (hw_decoder == AV_HWDEVICE_TYPE_NONE)
      {
         format = auto_hw_decoder(ctx, pix_fmts);
      }
      else
         format = init_hw_decoder(ctx, hw_decoder, pix_fmts);
   }

   /* Fallback to SW rendering */
   if (format == AV_PIX_FMT_NONE)
   {
#endif

      log_cb(RETRO_LOG_INFO, "[FFMPEG] Using SW decoding.\n");

      ctx->thread_type = FF_THREAD_FRAME;
      ctx->thread_count = sw_decoder_threads;

      format = fctx->streams[video_stream_index]->codec->pix_fmt;

#if LIBAVUTIL_VERSION_MAJOR > 55
      hw_decoding_enabled = false;
   }
   else
      hw_decoding_enabled = true;
#endif

   return format;
}

#if LIBAVUTIL_VERSION_MAJOR > 55
/* Callback used by ffmpeg to configure the pixelformat to use. */
static enum AVPixelFormat get_format(AVCodecContext *ctx,
                                     const enum AVPixelFormat *pix_fmts)
{
   /* Look if we can reuse the current decoder */
   for (size_t i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++)
      if (pix_fmts[i] == pix_fmt)
      {
         return pix_fmt;
      }

   pix_fmt = select_decoder(ctx, pix_fmts);

   return pix_fmt;
}
#endif

static bool open_codec(AVCodecContext **ctx, enum AVMediaType type, unsigned index)
{
   int ret;

   AVCodec *codec = avcodec_find_decoder(fctx->streams[index]->codec->codec_id);
   if (!codec)
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Couldn't find suitable decoder\n");
      return false;
   }

   *ctx = fctx->streams[index]->codec;

   if (type == AVMEDIA_TYPE_VIDEO)
   {
      video_stream_index = index;

#if LIBAVUTIL_VERSION_MAJOR > 55
      vctx->get_format  = get_format;
      pix_fmt = select_decoder((*ctx), NULL);
#else
      select_decoder((*ctx), NULL);
#endif
   }

   if ((ret = avcodec_open2(*ctx, codec, NULL)) < 0)
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Could not open codec: %s\n", av_err2str(ret));
      return false;
   }

   return true;
}

static bool codec_is_image(enum AVCodecID id)
{
   switch (id)
   {
#ifdef OLD_FFMPEG_API
      case CODEC_ID_MJPEG:
      case CODEC_ID_PNG:
#else
      case AV_CODEC_ID_MJPEG:
      case AV_CODEC_ID_PNG:
#endif
         return true;

      default:
         break;
   }

   return false;
}

static bool codec_id_is_ttf(enum AVCodecID id)
{
   switch (id)
   {
#ifdef OLD_FFMPEG_API
      case CODEC_ID_TTF:
#else
      case AV_CODEC_ID_TTF:
#endif
         return true;

      default:
         break;
   }

   return false;
}

#ifdef HAVE_SSA
static bool codec_id_is_ass(enum AVCodecID id)
{
   switch (id)
   {
#ifdef OLD_FFMPEG_API
      case CODEC_ID_SSA:
#else
      case AV_CODEC_ID_ASS:
      case AV_CODEC_ID_SSA:
#endif
         return true;
      default:
         break;
   }

   return false;
}
#endif

static bool open_codecs(void)
{
   unsigned i;
   decode_thread_lock   = slock_new();

   video_stream_index   = -1;
   audio_streams_num    = 0;
   subtitle_streams_num = 0;

   slock_lock(decode_thread_lock);
   audio_streams_ptr    = 0;
   subtitle_streams_ptr = 0;
   slock_unlock(decode_thread_lock);

   memset(audio_streams,    0, sizeof(audio_streams));
   memset(subtitle_streams, 0, sizeof(subtitle_streams));

   for (i = 0; i < fctx->nb_streams; i++)
   {
      enum AVMediaType type = fctx->streams[i]->codec->codec_type;
      switch (type)
      {
         case AVMEDIA_TYPE_AUDIO:
            if (audio_streams_num < MAX_STREAMS)
            {
               if (!open_codec(&actx[audio_streams_num], type, i))
                  return false;
               audio_streams[audio_streams_num] = i;
               audio_streams_num++;
            }
            break;

         case AVMEDIA_TYPE_VIDEO:
            if (     !vctx
                  && !codec_is_image(fctx->streams[i]->codec->codec_id))
            {
               if (!open_codec(&vctx, type, i))
                  return false;
            }
            break;

         case AVMEDIA_TYPE_SUBTITLE:
#ifdef HAVE_SSA
            if (     subtitle_streams_num < MAX_STREAMS
                  && codec_id_is_ass(fctx->streams[i]->codec->codec_id))
            {
               int size;
               AVCodecContext **s = &sctx[subtitle_streams_num];

               subtitle_streams[subtitle_streams_num] = i;
               if (!open_codec(s, type, i))
                  return false;

               size = (*s)->extradata ? (*s)->extradata_size : 0;
               ass_extra_data_size[subtitle_streams_num] = size;

               if (size)
               {
                  ass_extra_data[subtitle_streams_num] = (uint8_t*)av_malloc(size);
                  memcpy(ass_extra_data[subtitle_streams_num], (*s)->extradata, size);
               }

               subtitle_streams_num++;
            }
#endif
            break;

         case AVMEDIA_TYPE_ATTACHMENT:
            {
               AVCodecContext *ctx = fctx->streams[i]->codec;
               if (codec_id_is_ttf(ctx->codec_id))
                  append_attachment(ctx->extradata, ctx->extradata_size);
            }
            break;

         default:
            break;
      }
   }

   return actx[0] || vctx;
}

static bool init_media_info(void)
{
   if (actx[0])
      media.sample_rate = actx[0]->sample_rate;

   media.interpolate_fps = 60.0;

   if (vctx)
   {
      media.width  = vctx->width;
      media.height = vctx->height;
      media.aspect = (float)vctx->width *
         av_q2d(vctx->sample_aspect_ratio) / vctx->height;
   }

#ifdef HAVE_SSA
   if (sctx[0])
   {
      unsigned i;

      ass = ass_library_init();
      ass_set_message_cb(ass, ass_msg_cb, NULL);

      for (i = 0; i < attachments_size; i++)
         ass_add_font(ass, (char*)"",
               (char*)attachments[i].data, attachments[i].size);

      ass_render = ass_renderer_init(ass);
      ass_set_frame_size(ass_render, media.width, media.height);
      ass_set_extract_fonts(ass, true);
      ass_set_fonts(ass_render, NULL, NULL, 1, NULL, 1);
      ass_set_hinting(ass_render, ASS_HINTING_LIGHT);

      for (i = 0; i < (unsigned)subtitle_streams_num; i++)
      {
         ass_track[i] = ass_new_track(ass);
         ass_process_codec_private(ass_track[i], (char*)ass_extra_data[i],
               ass_extra_data_size[i]);
      }
   }
#endif

   return true;
}

static void set_colorspace(struct SwsContext *sws,
      unsigned width, unsigned height,
      enum AVColorSpace default_color, int in_range)
{
   const int *coeffs = NULL;

   if (colorspace == AVCOL_SPC_UNSPECIFIED)
   {
      if (default_color != AVCOL_SPC_UNSPECIFIED)
         coeffs = sws_getCoefficients(default_color);
      else if (width >= 1280 || height > 576)
         coeffs = sws_getCoefficients(AVCOL_SPC_BT709);
      else
         coeffs = sws_getCoefficients(AVCOL_SPC_BT470BG);
   }
   else
      coeffs = sws_getCoefficients(colorspace);

   if (coeffs)
   {
      int in_full, out_full, brightness, contrast, saturation;
      const int *inv_table, *table;

      sws_getColorspaceDetails(sws, (int**)&inv_table, &in_full,
            (int**)&table, &out_full,
            &brightness, &contrast, &saturation);

      if (in_range != AVCOL_RANGE_UNSPECIFIED)
         in_full = in_range == AVCOL_RANGE_JPEG;

      inv_table = coeffs;
      sws_setColorspaceDetails(sws, inv_table, in_full,
            table, out_full,
            brightness, contrast, saturation);
   }
}

#ifdef HAVE_SSA
/* Straight CPU alpha blending.
 * Should probably do in GL. */
static void render_ass_img(AVFrame *conv_frame, ASS_Image *img)
{
   uint32_t *frame = (uint32_t*)conv_frame->data[0];
   int      stride = conv_frame->linesize[0] / sizeof(uint32_t);

   for (; img; img = img->next)
   {
      int x, y;
      unsigned r, g, b, a;
      uint32_t *dst         = NULL;
      const uint8_t *bitmap = NULL;

      if (img->w == 0 && img->h == 0)
         continue;

      bitmap = img->bitmap;
      dst    = frame + img->dst_x + img->dst_y * stride;

      r      = (img->color >> 24) & 0xff;
      g      = (img->color >> 16) & 0xff;
      b      = (img->color >>  8) & 0xff;
      a      = 255 - (img->color & 0xff);

      for (y = 0; y < img->h; y++,
            bitmap += img->stride, dst += stride)
      {
         for (x = 0; x < img->w; x++)
         {
            unsigned src_alpha = ((bitmap[x] * (a + 1)) >> 8) + 1;
            unsigned dst_alpha = 256 - src_alpha;

            uint32_t dst_color = dst[x];
            unsigned dst_r     = (dst_color >> 16) & 0xff;
            unsigned dst_g     = (dst_color >>  8) & 0xff;
            unsigned dst_b     = (dst_color >>  0) & 0xff;

            dst_r = (r * src_alpha + dst_r * dst_alpha) >> 8;
            dst_g = (g * src_alpha + dst_g * dst_alpha) >> 8;
            dst_b = (b * src_alpha + dst_b * dst_alpha) >> 8;

            dst[x] = (0xffu << 24) | (dst_r << 16) |
               (dst_g << 8) | (dst_b << 0);
         }
      }
   }
}
#endif


#ifdef HAVE_SSA
static void decode_video(AVCodecContext *ctx, AVPacket *pkt, AVFrame *conv_frame, size_t frame_size, struct SwsContext  **sws, ASS_Track *ass_track_active)
#else
static void decode_video(AVCodecContext *ctx, AVPacket *pkt, AVFrame *conv_frame, size_t frame_size, struct SwsContext  **sws)
#endif
{
   int ret;
   AVFrame *frame = NULL;
   AVFrame *sw_frame = NULL;
   AVFrame *tmp_frame = NULL;

   if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc()))
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Can not alloc frames\n");
      return;
   }

   if ((ret = avcodec_send_packet(ctx, pkt)) < 0)
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Can't decode video packet: %s\n", av_err2str(ret));
      return;
   }

   while(true)
   {
      ret = avcodec_receive_frame(ctx, frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      {
         break;
      }
      else if (ret < 0)
      {
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error while reading video frame: %s\n", av_err2str(ret));
         goto fail;
      }

#if LIBAVUTIL_VERSION_MAJOR > 55
      if (hw_decoding_enabled)
      {
         /* Copy data from VRAM to RAM */
         if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0)
         {
               log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error transferring the data to system memory: %s\n", av_err2str(ret));
               goto fail;
         }
         tmp_frame = sw_frame;
      }
      else
#endif
         tmp_frame = frame;

      *sws = sws_getCachedContext(*sws,
            media.width, media.height, tmp_frame->format,
            media.width, media.height, PIX_FMT_RGB32,
            SWS_BICUBIC, NULL, NULL, NULL);

      set_colorspace(*sws, media.width, media.height,
           av_frame_get_colorspace(tmp_frame), av_frame_get_color_range(tmp_frame));

      if ((ret = sws_scale(*sws, (const uint8_t * const*)tmp_frame->data,
            tmp_frame->linesize, 0, media.height,
            conv_frame->data, conv_frame->linesize)) < 0)
      {
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error while scaling image: %s\n", av_err2str(ret));
         goto fail;
      }

      size_t decoded_size;
      int64_t pts = frame->best_effort_timestamp;
      double video_time = pts * av_q2d(fctx->streams[video_stream_index]->time_base);

#ifdef HAVE_SSA
      if (ass_render && ass_track_active)
      {
         int change     = 0;
         ASS_Image *img = ass_render_frame(ass_render, ass_track_active,
               1000 * video_time, &change);

         /* Do it on CPU for now.
          * We're in a thread anyways, so shouldn't really matter. */
         render_ass_img(conv_frame, img);
      }
#endif

      decoded_size = frame_size + sizeof(pts);
      slock_lock(fifo_lock);

      while (!decode_thread_dead  && (video_decode_fifo != NULL)
            && fifo_write_avail(video_decode_fifo) < decoded_size)
      {
         if (!main_sleeping)
            scond_wait(fifo_decode_cond, fifo_lock);
         else
         {
            fifo_clear(video_decode_fifo);
            break;
         }
      }

      decode_last_video_time = video_time;
      if (!decode_thread_dead)
      {
         int stride;
         unsigned y;
         const uint8_t *src = NULL;

         fifo_write(video_decode_fifo, &pts, sizeof(pts));
         src    = conv_frame->data[0];
         stride = conv_frame->linesize[0];

         for (y = 0; y < media.height; y++, src += stride)
            fifo_write(video_decode_fifo, src, media.width * sizeof(uint32_t));
      }
      scond_signal(fifo_cond);
      slock_unlock(fifo_lock);

   fail:
      av_frame_unref(frame);
      av_frame_unref(sw_frame);
      if (ret < 0)
         break;
   }

   av_frame_free(&frame);
   av_frame_free(&sw_frame);
   return;
}

static int16_t *decode_audio(AVCodecContext *ctx, AVPacket *pkt,
      AVFrame *frame, int16_t *buffer, size_t *buffer_cap,
      SwrContext *swr)
{
   int ret;
   int64_t pts;
   size_t required_buffer;

   if ((ret = avcodec_send_packet(ctx, pkt)) < 0)
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Can't decode audio packet: %s\n", av_err2str(ret));
      return buffer;
   }

   while(true)
   {
      ret = avcodec_receive_frame(ctx, frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      {
         break;
      }
      else if (ret < 0)
      {
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error while reading audio frame: %s\n", av_err2str(ret));
         break;
      }
      
      required_buffer = frame->nb_samples * sizeof(int16_t) * 2;
      if (required_buffer > *buffer_cap)
      {
         buffer      = (int16_t*)av_realloc(buffer, required_buffer);
         *buffer_cap = required_buffer;
      }

      swr_convert(swr,
            (uint8_t**)&buffer,
            frame->nb_samples,
            (const uint8_t**)frame->data,
            frame->nb_samples);

      pts = av_frame_get_best_effort_timestamp(frame);
      slock_lock(fifo_lock);

      while (!decode_thread_dead && fifo_write_avail(audio_decode_fifo) < required_buffer)
      {
         if (!main_sleeping)
            scond_wait(fifo_decode_cond, fifo_lock);
         else
         {
            log_cb(RETRO_LOG_ERROR, "[FFMPEG] Thread: Audio deadlock detected.\n");
            fifo_clear(audio_decode_fifo);
            break;
         }
      }

      decode_last_audio_time = pts * av_q2d(
            fctx->streams[audio_streams[audio_streams_ptr]]->time_base);

      if (!decode_thread_dead)
         fifo_write(audio_decode_fifo, buffer, required_buffer);

      scond_signal(fifo_cond);
      slock_unlock(fifo_lock);
   }

   return buffer;
}


static void decode_thread_seek(double time)
{
   int64_t seek_to = time * AV_TIME_BASE;

   if (seek_to < 0)
      seek_to = 0;

   decode_last_video_time = time;
   decode_last_audio_time = time;

   if(avformat_seek_file(fctx, -1, INT64_MIN, seek_to, INT64_MAX, 0) < 0)
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] av_seek_frame() failed.\n");

   if (actx[audio_streams_ptr])
      avcodec_flush_buffers(actx[audio_streams_ptr]);
   if (vctx)
      avcodec_flush_buffers(vctx);
   if (sctx[subtitle_streams_ptr])
      avcodec_flush_buffers(sctx[subtitle_streams_ptr]);
#ifdef HAVE_SSA
   if (ass_track[subtitle_streams_ptr])
      ass_flush_events(ass_track[subtitle_streams_ptr]);
#endif
}

static void decode_thread(void *data)
{
   unsigned i;
   struct SwrContext *swr[audio_streams_num];
   struct SwsContext *sws  = NULL;
   AVFrame *aud_frame      = NULL;
   void *conv_frame_buf    = NULL;
   size_t frame_size       = 0;
   int16_t *audio_buffer   = NULL;
   size_t audio_buffer_cap = 0;
   AVFrame *conv_frame     = NULL;


   (void)data;

   for (i = 0; (int)i < audio_streams_num; i++)
   {
      swr[i] = swr_alloc();

      av_opt_set_int(swr[i], "in_channel_layout", actx[i]->channel_layout, 0);
      av_opt_set_int(swr[i], "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
      av_opt_set_int(swr[i], "in_sample_rate", actx[i]->sample_rate, 0);
      av_opt_set_int(swr[i], "out_sample_rate", media.sample_rate, 0);
      av_opt_set_int(swr[i], "in_sample_fmt", actx[i]->sample_fmt, 0);
      av_opt_set_int(swr[i], "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
      swr_init(swr[i]);
   }

   aud_frame = av_frame_alloc();

   if (video_stream_index >= 0)
   {
      frame_size = avpicture_get_size(PIX_FMT_RGB32, media.width, media.height);
      conv_frame = av_frame_alloc();
      conv_frame_buf = av_malloc(frame_size);
      avpicture_fill((AVPicture*)conv_frame, (const uint8_t*)conv_frame_buf,
            PIX_FMT_RGB32, media.width, media.height);
   }

   while (!decode_thread_dead)
   {
      bool seek;
      AVPacket pkt;
      int subtitle_stream;
      double seek_time_thread;
      int audio_stream, audio_stream_ptr;
      AVCodecContext *actx_active = NULL;
      AVCodecContext *sctx_active = NULL;
#ifdef HAVE_SSA
      ASS_Track *ass_track_active = NULL;
#endif

      slock_lock(fifo_lock);
      seek             = do_seek;
      seek_time_thread = seek_time;
      slock_unlock(fifo_lock);

      if (seek)
      {
         decode_thread_seek(seek_time_thread);

         slock_lock(fifo_lock);
         do_seek = false;
         seek_time = 0.0;

         if (video_decode_fifo)
            fifo_clear(video_decode_fifo);
         if (audio_decode_fifo)
            fifo_clear(audio_decode_fifo);

         scond_signal(fifo_cond);
         slock_unlock(fifo_lock);
      }

      memset(&pkt, 0, sizeof(pkt));
      if (av_read_frame(fctx, &pkt) < 0)
         break;

      slock_lock(decode_thread_lock);
      audio_stream                = audio_streams[audio_streams_ptr];
      audio_stream_ptr            = audio_streams_ptr;
      subtitle_stream             = subtitle_streams[subtitle_streams_ptr];
      actx_active                 = actx[audio_streams_ptr];
      sctx_active                 = sctx[subtitle_streams_ptr];
#ifdef HAVE_SSA
      ass_track_active            = ass_track[subtitle_streams_ptr];
#endif
      slock_unlock(decode_thread_lock);

      if (pkt.stream_index == video_stream_index)
      #ifdef HAVE_SSA
         decode_video(vctx, &pkt, conv_frame, frame_size, &sws, ass_track_active);
      #else
         decode_video(vctx, &pkt, conv_frame, frame_size, &sws);
      #endif
      else if (pkt.stream_index == audio_stream && actx_active)
      {
         audio_buffer = decode_audio(actx_active, &pkt, aud_frame,
         audio_buffer, &audio_buffer_cap,
         swr[audio_stream_ptr]);
      }
      else if (pkt.stream_index == subtitle_stream && sctx_active)
      {
         AVSubtitle sub;
         int finished = 0;

         memset(&sub, 0, sizeof(sub));

         while (!finished)
         {
            if (avcodec_decode_subtitle2(sctx_active, &sub, &finished, &pkt) < 0)
            {
               log_cb(RETRO_LOG_ERROR, "[FFMPEG] Decode subtitles failed.\n");
               break;
            }
         }

#ifdef HAVE_SSA
         for (i = 0; i < sub.num_rects; i++)
         {
            if (sub.rects[i]->ass && ass_track_active)
               ass_process_data(ass_track_active,
                     sub.rects[i]->ass, strlen(sub.rects[i]->ass));
         }
#endif

         avsubtitle_free(&sub);
      }

      av_free_packet(&pkt);
   }

   sws_freeContext(sws);

   for (i = 0; (int)i < audio_streams_num; i++)
      swr_free(&swr[i]);

   if (vctx->hw_device_ctx)
      av_buffer_unref(&vctx->hw_device_ctx);

   av_frame_free(&aud_frame);
   av_frame_free(&conv_frame);
   av_freep(&conv_frame_buf);
   av_freep(&audio_buffer);

   slock_lock(fifo_lock);
   decode_thread_dead = true;
   scond_signal(fifo_cond);
   slock_unlock(fifo_lock);
}

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
static void context_destroy(void)
{
#ifdef HAVE_GL_FFT
   if (fft)
   {
      fft_free(fft);
      fft = NULL;
   }
#endif
}

#include "gl_shaders/ffmpeg.glsl.vert.h"

/* OpenGL ES note about main() -  Get format as GL_RGBA/GL_UNSIGNED_BYTE.
 * Assume little endian, so we get ARGB -> BGRA byte order, and
 * we have to swizzle to .BGR. */
#ifdef HAVE_OPENGLES
#include "gl_shaders/ffmpeg_es.glsl.frag.h"
#else
#include "gl_shaders/ffmpeg.glsl.frag.h"
#endif

static void context_reset(void)
{
   static const GLfloat vertex_data[] = {
      -1, -1, 0, 0,
       1, -1, 1, 0,
      -1,  1, 0, 1,
       1,  1, 1, 1,
   };
   GLuint vert, frag;
   unsigned i;

#ifdef HAVE_GL_FFT
   if (audio_streams_num > 0 && video_stream_index < 0)
   {
      fft = fft_new(11, hw_render.get_proc_address);
      if (fft)
         fft_init_multisample(fft, fft_width, fft_height, fft_multisample);
   }

   /* Already inits symbols. */
   if (!fft)
#endif
      rglgen_resolve_symbols(hw_render.get_proc_address);

   prog = glCreateProgram();
   vert = glCreateShader(GL_VERTEX_SHADER);
   frag = glCreateShader(GL_FRAGMENT_SHADER);

   glShaderSource(vert, 1, &vertex_source, NULL);
   glShaderSource(frag, 1, &fragment_source, NULL);
   glCompileShader(vert);
   glCompileShader(frag);
   glAttachShader(prog, vert);
   glAttachShader(prog, frag);
   glLinkProgram(prog);

   glUseProgram(prog);

   glUniform1i(glGetUniformLocation(prog, "sTex0"), 0);
   glUniform1i(glGetUniformLocation(prog, "sTex1"), 1);
   vertex_loc = glGetAttribLocation(prog, "aVertex");
   tex_loc    = glGetAttribLocation(prog, "aTexCoord");
   mix_loc    = glGetUniformLocation(prog, "uMix");

   glUseProgram(0);

   for (i = 0; i < 2; i++)
   {
      glGenTextures(1, &frames[i].tex);

      glBindTexture(GL_TEXTURE_2D, frames[i].tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if !defined(HAVE_OPENGLES)
      glGenBuffers(1, &frames[i].pbo);
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, frames[i].pbo);
      glBufferData(GL_PIXEL_UNPACK_BUFFER, media.width
            * media.height * sizeof(uint32_t), NULL, GL_STREAM_DRAW);
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif
   }

   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER,
         sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindTexture(GL_TEXTURE_2D, 0);
}
#endif

void CORE_PREFIX(retro_unload_game)(void)
{
   unsigned i;

   if (decode_thread_handle)
   {
      slock_lock(fifo_lock);
      decode_thread_dead = true;
      scond_signal(fifo_decode_cond);
      slock_unlock(fifo_lock);
      sthread_join(decode_thread_handle);
   }
   decode_thread_handle = NULL;

   if (fifo_cond)
      scond_free(fifo_cond);
   if (fifo_decode_cond)
      scond_free(fifo_decode_cond);
   if (fifo_lock)
      slock_free(fifo_lock);
   if (decode_thread_lock)
      slock_free(decode_thread_lock);

   if (video_decode_fifo)
      fifo_free(video_decode_fifo);
   if (audio_decode_fifo)
      fifo_free(audio_decode_fifo);

   fifo_cond = NULL;
   fifo_decode_cond = NULL;
   fifo_lock = NULL;
   decode_thread_lock = NULL;
   video_decode_fifo = NULL;
   audio_decode_fifo = NULL;

   decode_last_video_time = 0.0;
   decode_last_audio_time = 0.0;

   frames[0].pts = frames[1].pts = 0.0;
   pts_bias = 0.0;
   frame_cnt = 0;
   audio_frames = 0;

   for (i = 0; i < MAX_STREAMS; i++)
   {
      if (sctx[i])
         avcodec_close(sctx[i]);
      if (actx[i])
         avcodec_close(actx[i]);
      sctx[i] = NULL;
      actx[i] = NULL;
   }

   if (vctx)
   {
      avcodec_close(vctx);
      vctx = NULL;
   }

   if (fctx)
   {
      avformat_close_input(&fctx);
      fctx = NULL;
   }

   for (i = 0; i < attachments_size; i++)
      av_freep(&attachments[i].data);
   av_freep(&attachments);
   attachments_size = 0;

#ifdef HAVE_SSA
   for (i = 0; i < MAX_STREAMS; i++)
   {
      if (ass_track[i])
         ass_free_track(ass_track[i]);
      ass_track[i] = NULL;

      av_freep(&ass_extra_data[i]);
      ass_extra_data_size[i] = 0;
   }
   if (ass_render)
      ass_renderer_done(ass_render);
   if (ass)
      ass_library_done(ass);

   ass_render = NULL;
   ass = NULL;
#endif

   av_freep(&video_frame_temp_buffer);
}

bool CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
   int ret;
   bool is_fft = false;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Seek -10 seconds" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Seek +60 seconds" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Seek -60 seconds" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Seek +10 seconds" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Cycle Audio Track" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Cycle Subtitle Track" },

      { 0 },
   };

   if (!info)
      return false;

   check_variables(true);

   CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (!CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Cannot set pixel format.");
      goto error;
   }

   if ((ret = avformat_open_input(&fctx, info->path, NULL, NULL)) < 0)
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to open input: %s\n", av_err2str(ret));
      goto error;
   }

   print_ffmpeg_version();

   if ((ret = avformat_find_stream_info(fctx, NULL)) < 0)
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to find stream info: %s\n", av_err2str(ret));
      goto error;
   }

   log_cb(RETRO_LOG_INFO, "[FFMPEG] Media information:\n");
   av_dump_format(fctx, 0, info->path, 0);

   if (!open_codecs())
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to find codec.");
      goto error;
   }

   if (!init_media_info())
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to init media info.");
      goto error;
   }

#ifdef HAVE_GL_FFT
   is_fft = video_stream_index < 0 && audio_streams_num > 0;
#endif

   if (video_stream_index >= 0 || is_fft)
   {
      video_decode_fifo = fifo_new(media.width
            * media.height * sizeof(uint32_t) * 32);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
      use_gl = true;
      hw_render.context_reset      = context_reset;
      hw_render.context_destroy    = context_destroy;
      hw_render.bottom_left_origin = is_fft;
      hw_render.depth              = is_fft;
      hw_render.stencil            = is_fft;
#if defined(HAVE_OPENGLES)
      hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES2;
#else
      hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
#endif
      if (!CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      {
         use_gl = false;
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Cannot initialize HW render.");
      }
#endif
   }
   if (audio_streams_num > 0)
   {
      unsigned buffer_seconds = video_stream_index >= 0 ? 20 : 1;
      audio_decode_fifo = fifo_new(buffer_seconds * media.sample_rate * sizeof(int16_t) * 2);
   }

   fifo_cond        = scond_new();
   fifo_decode_cond = scond_new();
   fifo_lock        = slock_new();

   slock_lock(fifo_lock);
   decode_thread_dead = false;
   slock_unlock(fifo_lock);

   decode_thread_handle = sthread_create(decode_thread, NULL);

   video_frame_temp_buffer = (uint32_t*)
      av_malloc(media.width * media.height * sizeof(uint32_t));

   pts_bias = 0.0;

   return true;

error:
   CORE_PREFIX(retro_unload_game)();
   return false;
}

unsigned CORE_PREFIX(retro_get_region)(void)
{
   return RETRO_REGION_NTSC;
}

bool CORE_PREFIX(retro_load_game_special)(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t CORE_PREFIX(retro_serialize_size)(void)
{
   return 0;
}

bool CORE_PREFIX(retro_serialize)(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool CORE_PREFIX(retro_unserialize)(const void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void *CORE_PREFIX(retro_get_memory_data)(unsigned id)
{
   (void)id;
   return NULL;
}

size_t CORE_PREFIX(retro_get_memory_size)(unsigned id)
{
   (void)id;
   return 0;
}

void CORE_PREFIX(retro_cheat_reset)(void)
{}

void CORE_PREFIX(retro_cheat_set)(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

#if defined(LIBRETRO_SWITCH)

#ifdef ARCH_X86
#include "../libswresample/resample.h"
void swri_resample_dsp_init(ResampleContext *c)
{}
#endif

#endif
