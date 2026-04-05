#include <retro_common_api.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef RARCH_INTERNAL
#ifdef HAVE_CONFIG_H
#include "../../config.h"
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

#include <features/features_cpu.h>
#include <retro_miscellaneous.h>
#include <rthreads/rthreads.h>
#include <rthreads/tpool.h>
#include <queues/fifo_queue.h>

#include <libretro.h>
#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#define CORE_PREFIX(s) libretro_ffmpeg_##s
#else
#define CORE_PREFIX(s) s
#endif

/* If libavutil is at least version 55,
 * and if libavcodec is at least version 57.80.100,
 * enable hardware acceleration */
#define ENABLE_HW_ACCEL ((LIBAVUTIL_VERSION_MAJOR >= 55) && \
      (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 80, 100)))

#define PRINT_VERSION(s) log_cb(RETRO_LOG_INFO, "[FFMPEG] lib%s version:\t%d.%d.%d\n", #s, \
   s ##_version() >> 16 & 0xFF, \
   s ##_version() >> 8 & 0xFF, \
   s ##_version() & 0xFF);

#define HAVE_CH_LAYOUT (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100))

/* reset_triggered and libretro_supports_bitmasks moved into ffmpeg_core_ctx_t */

typedef struct AVPacketNode AVPacketNode_t;

struct packet_buffer
{
   AVPacketNode_t *head;
   AVPacketNode_t *tail;
   size_t size;
};
typedef struct packet_buffer packet_buffer_t;

struct video_decoder_context
{
   int64_t pts;
   struct SwsContext *sws;
   AVFrame *source;
#if ENABLE_HW_ACCEL
   AVFrame *hw_source;
#endif
   AVFrame *target;
#ifdef HAVE_SSA
   ASS_Track *ass_track_active;
#endif
   uint8_t *frame_buf;
   int index;
};
typedef struct video_decoder_context video_decoder_context_t;

struct AVPacketNode
{
   AVPacket *data;
   struct AVPacketNode *next;
   struct AVPacketNode *previous;
};

enum kb_status
{
  KB_OPEN = 0,
  KB_IN_PROGRESS,
  KB_FINISHED
};

struct video_buffer
{
   int64_t head;
   int64_t tail;
   video_decoder_context_t *buffer;
   enum kb_status *status;
   slock_t *lock;
   scond_t *open_cond;
   scond_t *finished_cond;
   size_t capacity;
};
typedef struct video_buffer video_buffer_t;

static void video_buffer_destroy(video_buffer_t *video_buffer)
{
   unsigned i;
   if (!video_buffer)
      return;

   slock_free(video_buffer->lock);
   scond_free(video_buffer->open_cond);
   scond_free(video_buffer->finished_cond);
   free(video_buffer->status);
   if (video_buffer->buffer)
   {
      for (i = 0; i < video_buffer->capacity; i++)
      {
#if ENABLE_HW_ACCEL
         av_frame_free(&video_buffer->buffer[i].hw_source);
#endif
         av_frame_free(&video_buffer->buffer[i].source);
         if (video_buffer->buffer[i].target)
            av_freep(&video_buffer->buffer[i].target->data[0]);
         av_frame_free(&video_buffer->buffer[i].target);
         sws_freeContext(video_buffer->buffer[i].sws);
      }
   }
   free(video_buffer->buffer);
   free(video_buffer);
}

static video_buffer_t *video_buffer_create(
      size_t capacity, int frame_size, int width, int height)
{
   unsigned i;
   video_buffer_t *b = (video_buffer_t*)malloc(sizeof(video_buffer_t));
   if (!b)
      return NULL;

   b->buffer        = NULL;
   b->capacity      = capacity;
   b->lock          = NULL;
   b->open_cond     = NULL;
   b->finished_cond = NULL;
   b->head          = 0;
   b->tail          = 0;
   b->status        = (enum kb_status*)malloc(sizeof(enum kb_status) * capacity);

   if (!b->status)
      goto fail;

   for (i = 0; i < capacity; i++)
      b->status[i]  = KB_OPEN;

   b->lock          = slock_new();
   b->open_cond     = scond_new();
   b->finished_cond = scond_new();
   if (!b->lock || !b->open_cond || !b->finished_cond)
      goto fail;

   b->buffer        = (video_decoder_context_t*)
      malloc(sizeof(video_decoder_context_t) * capacity);
   if (!b->buffer)
      goto fail;

   for (i = 0; i < (unsigned)capacity; i++)
   {
      AVFrame* frame;
      b->buffer[i].index     = i;
      b->buffer[i].pts       = 0;
      b->buffer[i].sws       = sws_alloc_context();
      b->buffer[i].source    = av_frame_alloc();
#if ENABLE_HW_ACCEL
      b->buffer[i].hw_source = av_frame_alloc();
#endif
      b->buffer[i].target    = av_frame_alloc();

      frame = b->buffer[i].target;
      av_image_alloc(frame->data, frame->linesize,
            width, height, AV_PIX_FMT_RGB32, 1);

      if (!b->buffer[i].sws       ||
          !b->buffer[i].source    ||
#if ENABLE_HW_ACCEL
          !b->buffer[i].hw_source ||
#endif
          !b->buffer[i].target)
         goto fail;
   }
   return b;

fail:
   video_buffer_destroy(b);
   return NULL;
}

static void video_buffer_clear(video_buffer_t *video_buffer)
{
   unsigned i;
   if (!video_buffer)
      return;

   slock_lock(video_buffer->lock);

   scond_signal(video_buffer->open_cond);
   scond_signal(video_buffer->finished_cond);

   video_buffer->head = 0;
   video_buffer->tail = 0;
   for (i = 0; i < video_buffer->capacity; i++)
      video_buffer->status[i] = KB_OPEN;

   slock_unlock(video_buffer->lock);
}

static void video_buffer_get_open_slot(
      video_buffer_t *video_buffer, video_decoder_context_t **context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[video_buffer->head] == KB_OPEN)
   {
      *context = &video_buffer->buffer[video_buffer->head];
      video_buffer->status[video_buffer->head] = KB_IN_PROGRESS;
      video_buffer->head++;
      video_buffer->head %= video_buffer->capacity;
   }

   slock_unlock(video_buffer->lock);
}

static void video_buffer_return_open_slot(
      video_buffer_t *video_buffer, video_decoder_context_t *context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[context->index] == KB_IN_PROGRESS)
   {
      video_buffer->status[context->index] = KB_OPEN;
      video_buffer->head--;
      video_buffer->head %= video_buffer->capacity;
   }

   slock_unlock(video_buffer->lock);
}

static void video_buffer_open_slot(
      video_buffer_t *video_buffer,
      video_decoder_context_t *context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[context->index] == KB_FINISHED)
   {
      video_buffer->status[context->index] = KB_OPEN;
      video_buffer->tail++;
      video_buffer->tail %= (video_buffer->capacity);
      scond_signal(video_buffer->open_cond);
   }

   slock_unlock(video_buffer->lock);
}

static void video_buffer_get_finished_slot(
      video_buffer_t *video_buffer,
      video_decoder_context_t **context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[video_buffer->tail] == KB_FINISHED)
      *context = &video_buffer->buffer[video_buffer->tail];

   slock_unlock(video_buffer->lock);
}

static void video_buffer_finish_slot(
      video_buffer_t *video_buffer,
      video_decoder_context_t *context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[context->index] == KB_IN_PROGRESS)
   {
      video_buffer->status[context->index] = KB_FINISHED;
      scond_signal(video_buffer->finished_cond);
   }

   slock_unlock(video_buffer->lock);
}

static bool video_buffer_wait_for_finished_slot(video_buffer_t *video_buffer)
{
   slock_lock(video_buffer->lock);

   while (video_buffer->status[video_buffer->tail] != KB_FINISHED)
      scond_wait(video_buffer->finished_cond, video_buffer->lock);

   slock_unlock(video_buffer->lock);

   return true;
}

static bool video_buffer_has_open_slot(video_buffer_t *video_buffer)
{
   bool ret = false;

   slock_lock(video_buffer->lock);

   if (video_buffer->status[video_buffer->head] == KB_OPEN)
      ret = true;

   slock_unlock(video_buffer->lock);

   return ret;
}

bool video_buffer_has_finished_slot(video_buffer_t *video_buffer)
{
   bool ret = false;

   slock_lock(video_buffer->lock);

   if (video_buffer->status[video_buffer->tail] == KB_FINISHED)
      ret = true;

   slock_unlock(video_buffer->lock);

   return ret;
}

static packet_buffer_t *packet_buffer_create(void)
{
   packet_buffer_t *b = (packet_buffer_t*)malloc(sizeof(packet_buffer_t));
   if (!b)
      return NULL;

   memset(b, 0, sizeof(packet_buffer_t));

   return b;
}

static void packet_buffer_destroy(packet_buffer_t *packet_buffer)
{
   AVPacketNode_t *node;

   if (!packet_buffer)
      return;

   if (packet_buffer->head)
   {
      node = packet_buffer->head;
      while (node)
      {
         AVPacketNode_t *next = node->next;
         av_packet_free(&node->data);
         free(node);
         node = next;
      }
   }

   free(packet_buffer);
}

static void packet_buffer_clear(packet_buffer_t **packet_buffer)
{
   if (!packet_buffer)
      return;

   packet_buffer_destroy(*packet_buffer);
   *packet_buffer = packet_buffer_create();
}

static bool packet_buffer_empty(packet_buffer_t *packet_buffer)
{
   if (!packet_buffer)
      return true;

   return packet_buffer->size == 0;
}

static void packet_buffer_add_packet(packet_buffer_t *packet_buffer, AVPacket *pkt)
{
   AVPacketNode_t *new_head = (AVPacketNode_t *) malloc(sizeof(AVPacketNode_t));
   new_head->data = av_packet_alloc();

   av_packet_move_ref(new_head->data, pkt);

   if (packet_buffer->head)
   {
      new_head->next = packet_buffer->head;
      packet_buffer->head->previous = new_head;
   }
   else
   {
      new_head->next = NULL;
      packet_buffer->tail = new_head;
   }

   packet_buffer->head = new_head;
   packet_buffer->head->previous = NULL;
   packet_buffer->size++;
}

static void packet_buffer_get_packet(packet_buffer_t *packet_buffer,
   AVPacket *pkt)
{
   AVPacketNode_t *new_tail = NULL;

   if (!packet_buffer->tail)
      return;

   av_packet_move_ref(pkt, packet_buffer->tail->data);
   if (packet_buffer->tail->previous)
   {
      new_tail = packet_buffer->tail->previous;
      new_tail->next = NULL;
   }
   else
      packet_buffer->head = NULL;

   av_packet_free(&packet_buffer->tail->data);
   free(packet_buffer->tail);

   packet_buffer->tail = new_tail;
   packet_buffer->size--;
}

static int64_t packet_buffer_peek_start_pts(packet_buffer_t *packet_buffer)
{
   if (!packet_buffer->tail)
      return 0;

   return packet_buffer->tail->data->pts;
}

static int64_t packet_buffer_peek_end_pts(packet_buffer_t *packet_buffer)
{
   if (!packet_buffer->tail)
      return 0;

   return packet_buffer->tail->data->pts + packet_buffer->tail->data->duration;
}

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

#ifndef FFMPEG3
#define FFMPEG3 ((LIBAVUTIL_VERSION_INT < AV_VERSION_INT(56, 6, 100)) || \
      (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100)))
#endif
#ifndef FFMPEG8
#define FFMPEG8 (LIBAVCODEC_VERSION_MAJOR >= 62)
#endif

#define MAX_STREAMS 8
/* Sentinel return value indicating no frame available (EAGAIN/EOF) */
#define DECODE_NO_FRAME (-42)

struct attachment
{
   uint8_t *data;
   size_t size;
};

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

struct media_info
{
   double interpolate_fps;
   unsigned width;
   unsigned height;
   unsigned sample_rate;

   float aspect;

   struct
   {
      double time;
      unsigned hours;
      unsigned minutes;
      unsigned seconds;
   } duration;
};

/*
 * Consolidated core state.
 *
 * All mutable runtime state that was previously scattered across
 * individual file-scope variables is now grouped here.  A single
 * static instance (g_ctx) is defined below, and short #define
 * aliases keep the rest of the file unchanged.  In the future the
 * defines can be removed and g_ctx passed explicitly to eliminate
 * all hidden global state.
 */
typedef struct ffmpeg_core_ctx
{
   /* General */
   bool reset_triggered;
   bool libretro_supports_bitmasks;

   /* Demuxer / codecs */
   AVFormatContext *fctx;
   AVCodecContext *vctx;
   int video_stream_index;
   enum AVColorSpace color_space;

   /* Decoder configuration */
   unsigned sw_decoder_threads;
   unsigned sw_sws_threads;
   video_buffer_t *video_buffer;
   tpool_t *tpool;

#if ENABLE_HW_ACCEL
   enum AVHWDeviceType hw_decoder;
   bool hw_decoding_enabled;
   enum AVPixelFormat hw_pix_fmt;
   bool force_sw_decoder;
#endif

   /* Stream bookkeeping */
   AVCodecContext *actx[MAX_STREAMS];
   AVCodecContext *sctx[MAX_STREAMS];
   int audio_streams[MAX_STREAMS];
   int audio_streams_num;
   int audio_stream_idx;
   int subtitle_streams[MAX_STREAMS];
   int subtitle_streams_num;
   int subtitle_stream_idx;

#ifdef HAVE_SSA
   /* ASS/SSA subtitles */
   ASS_Library *ass_lib;
   ASS_Renderer *ass_render;
   ASS_Track *ass_track[MAX_STREAMS];
   uint8_t *ass_extra_data[MAX_STREAMS];
   size_t ass_extra_data_size[MAX_STREAMS];
   slock_t *ass_lock;
#endif

   /* Attachments (e.g. embedded fonts) */
   struct attachment *attachments;
   size_t attachments_size;

#ifdef HAVE_GL_FFT
   fft_t *fft;
   unsigned fft_width;
   unsigned fft_height;
   unsigned fft_multisample;
#endif

   /* A/V timing */
   uint64_t decoded_frame_cnt;
   uint64_t audio_frames;
   double pts_bias;

   /* Threaded FIFOs */
   volatile bool decode_thread_dead;
   fifo_buffer_t *audio_decode_fifo;
   scond_t *fifo_cond;
   scond_t *fifo_decode_cond;
   slock_t *fifo_lock;
   slock_t *decode_thread_lock;
   sthread_t *decode_thread_handle;
   double decode_last_audio_time;
   bool main_sleeping;

   uint32_t *video_frame_temp_buffer;

   /* Seeking */
   bool do_seek;
   double seek_time;

   /* Video frames (double-buffered for interpolation) */
   struct frame frames[2];

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   bool temporal_interpolation;
   bool use_gl;
   struct retro_hw_render_callback hw_render;
   GLuint prog;
   GLuint vbo;
   GLint vertex_loc;
   GLint tex_loc;
   GLint mix_loc;
#endif

   /* Media info */
   struct media_info media;
} ffmpeg_core_ctx_t;

static ffmpeg_core_ctx_t g_ctx;

/* ---- Backward-compatible aliases ----
 * These let existing code reference the old global names unchanged.
 * Remove these one-by-one when passing g_ctx explicitly. */
#define reset_triggered        (g_ctx.reset_triggered)
#define libretro_supports_bitmasks (g_ctx.libretro_supports_bitmasks)
#define fctx                   (g_ctx.fctx)
#define vctx                   (g_ctx.vctx)
#define video_stream_index     (g_ctx.video_stream_index)
#define sw_decoder_threads     (g_ctx.sw_decoder_threads)
#define sw_sws_threads         (g_ctx.sw_sws_threads)
#define video_buffer           (g_ctx.video_buffer)
#define tpool                  (g_ctx.tpool)
#if ENABLE_HW_ACCEL
#define hw_decoder             (g_ctx.hw_decoder)
#define hw_decoding_enabled    (g_ctx.hw_decoding_enabled)
#define force_sw_decoder       (g_ctx.force_sw_decoder)
#endif
#define actx                   (g_ctx.actx)
#define sctx                   (g_ctx.sctx)
#define audio_streams          (g_ctx.audio_streams)
#define audio_streams_num      (g_ctx.audio_streams_num)
#define subtitle_streams       (g_ctx.subtitle_streams)
#define subtitle_streams_num   (g_ctx.subtitle_streams_num)
#ifdef HAVE_SSA
#define ass_render             (g_ctx.ass_render)
#define ass_track              (g_ctx.ass_track)
#define ass_extra_data         (g_ctx.ass_extra_data)
#define ass_extra_data_size    (g_ctx.ass_extra_data_size)
#define ass_lock               (g_ctx.ass_lock)
#endif
#define attachments            (g_ctx.attachments)
#define attachments_size       (g_ctx.attachments_size)
#ifdef HAVE_GL_FFT
#define fft                    (g_ctx.fft)
#define fft_width              (g_ctx.fft_width)
#define fft_height             (g_ctx.fft_height)
#define fft_multisample        (g_ctx.fft_multisample)
#endif
#define audio_frames           (g_ctx.audio_frames)
#define pts_bias               (g_ctx.pts_bias)
#define decode_thread_dead     (g_ctx.decode_thread_dead)
#define audio_decode_fifo      (g_ctx.audio_decode_fifo)
#define fifo_cond              (g_ctx.fifo_cond)
#define fifo_decode_cond       (g_ctx.fifo_decode_cond)
#define fifo_lock              (g_ctx.fifo_lock)
#define decode_thread_lock     (g_ctx.decode_thread_lock)
#define decode_thread_handle   (g_ctx.decode_thread_handle)
#define decode_last_audio_time (g_ctx.decode_last_audio_time)
#define main_sleeping          (g_ctx.main_sleeping)
#define video_frame_temp_buffer (g_ctx.video_frame_temp_buffer)
#define do_seek                (g_ctx.do_seek)
#define seek_time              (g_ctx.seek_time)
#define frames                 (g_ctx.frames)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#define temporal_interpolation (g_ctx.temporal_interpolation)
#define use_gl                 (g_ctx.use_gl)
#define hw_render              (g_ctx.hw_render)
#define prog                   (g_ctx.prog)
#define vbo                    (g_ctx.vbo)
#define vertex_loc             (g_ctx.vertex_loc)
#define tex_loc                (g_ctx.tex_loc)
#define mix_loc                (g_ctx.mix_loc)
#endif
#define media                  (g_ctx.media)

#ifdef HAVE_SSA
static void render_ass_img(AVFrame *conv_frame, ASS_Image *img);
#endif

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

static void append_attachment(const uint8_t *data, size_t len)
{
   attachments = (struct attachment*)av_realloc(
         attachments, (attachments_size + 1) * sizeof(*attachments));

   attachments[attachments_size].data = (uint8_t*)av_malloc(len);
   attachments[attachments_size].size = len;
   memcpy(attachments[attachments_size].data, data, len);

   attachments_size++;
}

void CORE_PREFIX(retro_init)(void)
{
   reset_triggered = false;

#if FFMPEG3
   av_register_all();
#endif

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;
}

void CORE_PREFIX(retro_deinit)(void)
{
   if (video_buffer)
   {
      video_buffer_destroy(video_buffer);
      video_buffer = NULL;
   }

   if (tpool)
   {
      tpool_destroy(tpool);
      tpool = NULL;
   }

   /* Zero the entire context so every member starts clean on the
    * next retro_init() / retro_load_game() cycle.  This is safe
    * because all resources have already been freed above and in
    * retro_unload_game(). */
   memset(&g_ctx, 0, sizeof(g_ctx));
}

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
#if ENABLE_HW_ACCEL
      { "ffmpeg_hw_decoder", "Use Hardware decoder (restart); off|auto|"
         "cuda|d3d11va|drm|dxva2|mediacodec|opencl|qsv|vaapi|vdpau|videotoolbox" },
#endif
      { "ffmpeg_sw_decoder_threads", "Software decoder thread count (restart); auto|1|2|4|6|8|10|12|14|16" },
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

static void print_ffmpeg_version(void)
{
   PRINT_VERSION(avformat)
   PRINT_VERSION(avcodec)
   PRINT_VERSION(avutil)
#ifdef HAVE_SWRESAMPLE
   PRINT_VERSION(swresample)
#endif
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
      if (memcmp(color_var.value, "BT.709", 6) == 0)
         g_ctx.color_space = AVCOL_SPC_BT709;
      else if (memcmp(color_var.value, "BT.601", 6) == 0)
         g_ctx.color_space = AVCOL_SPC_BT470BG;
      else if (memcmp(color_var.value, "FCC", 3) == 0)
         g_ctx.color_space = AVCOL_SPC_FCC;
      else if (memcmp(color_var.value, "SMPTE240M", 9) == 0)
         g_ctx.color_space = AVCOL_SPC_SMPTE240M;
      else
         g_ctx.color_space = AVCOL_SPC_UNSPECIFIED;
      slock_unlock(decode_thread_lock);
   }

#if ENABLE_HW_ACCEL
   if (firststart)
   {
      hw_var.key = "ffmpeg_hw_decoder";

      force_sw_decoder = false;
      hw_decoder = AV_HWDEVICE_TYPE_NONE;

      if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &hw_var) && hw_var.value)
      {
         if (memcmp(hw_var.value, "off", 3) == 0)
            force_sw_decoder = true;
         else if (memcmp(hw_var.value, "cuda", 4) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_CUDA;
         else if (memcmp(hw_var.value, "d3d11va", 7) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_D3D11VA;
         else if (memcmp(hw_var.value, "drm", 3) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_DRM;
         else if (memcmp(hw_var.value, "dxva2", 5) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_DXVA2;
#if !FFMPEG3
         else if (memcmp(hw_var.value, "mediacodec", 10) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_MEDIACODEC;
         else if (memcmp(hw_var.value, "opencl", 6) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_OPENCL;
#endif
         else if (memcmp(hw_var.value, "qsv", 3) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_QSV;
         else if (memcmp(hw_var.value, "vaapi", 5) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_VAAPI;
         else if (memcmp(hw_var.value, "vdpau", 5) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_VDPAU;
         else if (memcmp(hw_var.value, "videotoolbox", 12) == 0)
            hw_decoder = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
      }
   }
#endif

   if (firststart)
   {
      sw_threads_var.key = "ffmpeg_sw_decoder_threads";
      if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &sw_threads_var) && sw_threads_var.value)
      {
         if (memcmp(sw_threads_var.value, "auto", 4) == 0)
         {
            sw_decoder_threads = cpu_features_get_core_amount();
         }
         else
         {
            sw_decoder_threads = (unsigned)strtoul(sw_threads_var.value, NULL, 0);
         }
         /* Scale the sws threads based on core count but use at least 2 and at most 4 threads */
         sw_sws_threads = MIN(MAX(2, sw_decoder_threads / 2), 4);
      }
   }
}

static void seek_frame(int seek_frames)
{
   char msg[256];
   struct retro_message_ext msg_obj = {0};
   int seek_frames_capped           = seek_frames;
   unsigned seek_hours              = 0;
   unsigned seek_minutes            = 0;
   unsigned seek_seconds            = 0;
   int8_t seek_progress             = -1;

   msg[0] = '\0';

   /* Handle resets + attempts to seek to a location
    * before the start of the video */
   if ((seek_frames < 0 && (unsigned)-seek_frames > g_ctx.decoded_frame_cnt) || reset_triggered)
      g_ctx.decoded_frame_cnt = 0;
   /* Handle backwards seeking */
   else if (seek_frames < 0)
      g_ctx.decoded_frame_cnt += seek_frames;
   /* Handle forwards seeking */
   else
   {
      double current_time     = (double)g_ctx.decoded_frame_cnt / media.interpolate_fps;
      double seek_step_time   = (double)seek_frames / media.interpolate_fps;
      double seek_target_time = current_time + seek_step_time;
      double seek_time_max    = media.duration.time - 1.0;

      seek_time_max = (seek_time_max > 0.0) ?
            seek_time_max : 0.0;

      /* Ensure that we don't attempt to seek past
       * the end of the file */
      if (seek_target_time > seek_time_max)
      {
         seek_step_time = seek_time_max - current_time;

         /* If seek would have taken us to the
          * end of the file, restart it instead
          * (less jarring for the user in case of
          * accidental seeking...) */
         if (seek_step_time < 0.0)
            seek_frames_capped = -1;
         else
            seek_frames_capped = (int)(seek_step_time * media.interpolate_fps);
      }

      if (seek_frames_capped < 0)
         g_ctx.decoded_frame_cnt  = 0;
      else
         g_ctx.decoded_frame_cnt += seek_frames_capped;
   }

   slock_lock(fifo_lock);
   do_seek        = true;
   seek_time      = g_ctx.decoded_frame_cnt / media.interpolate_fps;

   /* Convert seek time to a printable format */
   seek_seconds  = (unsigned)seek_time;
   seek_minutes  = seek_seconds / 60;
   seek_seconds %= 60;
   seek_hours    = seek_minutes / 60;
   seek_minutes %= 60;

   snprintf(msg, sizeof(msg), "%02d:%02d:%02d / %02d:%02d:%02d",
         seek_hours, seek_minutes, seek_seconds,
         media.duration.hours, media.duration.minutes, media.duration.seconds);

   /* Get current progress */
   if (media.duration.time > 0.0)
   {
      seek_progress = (int8_t)((100.0 * seek_time / media.duration.time) + 0.5);
      seek_progress = (seek_progress < -1)  ? -1  : seek_progress;
      seek_progress = (seek_progress > 100) ? 100 : seek_progress;
   }

   /* Send message to frontend */
   msg_obj.msg      = msg;
   msg_obj.duration = 2000;
   msg_obj.priority = 3;
   msg_obj.level    = RETRO_LOG_INFO;
   msg_obj.target   = RETRO_MESSAGE_TARGET_OSD;
   msg_obj.type     = RETRO_MESSAGE_TYPE_PROGRESS;
   msg_obj.progress = seek_progress;
   CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg_obj);

   if (seek_frames_capped < 0)
   {
      log_cb(RETRO_LOG_INFO, "[FFMPEG] Resetting PTS.\n");
      frames[0].pts = 0.0;
      frames[1].pts = 0.0;
   }
   audio_frames = g_ctx.decoded_frame_cnt * media.sample_rate / media.interpolate_fps;

   if (audio_decode_fifo)
      fifo_clear(audio_decode_fifo);
   scond_signal(fifo_decode_cond);

   while (!decode_thread_dead && do_seek)
   {
      main_sleeping = true;
      scond_wait(fifo_cond, fifo_lock);
      main_sleeping = false;
   }

   slock_unlock(fifo_lock);
}

static int seek_adjust(int target)
{
   switch (target)
   {
      case   0: return  10; /* The logic is kind of */
      case  10: return  30; /* silly on the face of */
      case  30: return  60; /* it, but we needed a */
      case  60: return  90; /* strong seek to get to */
      case  90: return 300; /* the middle or end of */
      case 300: return 310; /* a long film; the result */
      case 310: return 330; /* is this block which is */
      case 330: return 360; /* used to adjust the seek */
      case 360: return 390; /* strength without using */
      case 390: return  10; /* multiple variables. */
   }
   return 0;
}

void CORE_PREFIX(retro_run)(void)
{
   static bool last_left;
   static bool last_right;
   static bool last_up;
   static bool last_down;
   static bool last_l1;
   static bool last_l2;
   static bool last_r1;
   static bool last_r2;
   static int seek_l2;
   static int seek_r2;
   double min_pts;
   int16_t audio_buffer[media.sample_rate / 20];
   bool left, right, up, down, l1, l2, r1, r2;
   int16_t ret                  = 0;
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

   if (libretro_supports_bitmasks)
      ret = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD,
            0, RETRO_DEVICE_ID_JOYPAD_MASK);
   else
   {
      unsigned i;
      for (i = RETRO_DEVICE_ID_JOYPAD_B; i <= RETRO_DEVICE_ID_JOYPAD_R; i++)
         if (CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0, i))
            ret |= (1 << i);
   }

   if (CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP))
      ret |= (1 << RETRO_DEVICE_ID_JOYPAD_R);
   if (CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN))
      ret |= (1 << RETRO_DEVICE_ID_JOYPAD_L);

   left  = ret & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
   right = ret & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
   up    = ret & (1 << RETRO_DEVICE_ID_JOYPAD_UP);
   down  = ret & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
   l1     = ret & (1 << RETRO_DEVICE_ID_JOYPAD_L);
   l2     = ret & (1 << RETRO_DEVICE_ID_JOYPAD_L2);
   r1     = ret & (1 << RETRO_DEVICE_ID_JOYPAD_R);
   r2     = ret & (1 << RETRO_DEVICE_ID_JOYPAD_R2);

   if (l1 && !last_l1)
   {
      seek_frames -= 30 * media.interpolate_fps;
      seek_l2 = 0;
   }
   if (r1 && !last_r1)
   {
      seek_frames += 30 * media.interpolate_fps;
      seek_l2 = 0;
   }

   if (l2 && !last_l2)
   {
      seek_l2 = seek_adjust(seek_l2);
      seek_frames -= seek_l2 * media.interpolate_fps;
   }

   if (r2 && !last_r2)
   {
      seek_r2 = seek_adjust(seek_r2);
      seek_frames += seek_r2 * media.interpolate_fps;
   }

   if (((up && !last_up) || (down && !last_down)) && audio_streams_num > 0)
   {
      char msg[256];
      struct retro_message_ext msg_obj = {0};
      int adjustment = (up) ? (+1) : ((down) ? (-1) : (0));

      seek_l2 = 0;

      msg[0] = '\0';

      slock_lock(decode_thread_lock);
      g_ctx.audio_stream_idx = (((g_ctx.audio_stream_idx + adjustment) % audio_streams_num) + audio_streams_num) % audio_streams_num;
      slock_unlock(decode_thread_lock);

      snprintf(msg, sizeof(msg), "Audio Track #%d.", g_ctx.audio_stream_idx);

      msg_obj.msg      = msg;
      msg_obj.duration = 3000;
      msg_obj.priority = 1;
      msg_obj.level    = RETRO_LOG_INFO;
      msg_obj.target   = RETRO_MESSAGE_TARGET_ALL;
      msg_obj.type     = RETRO_MESSAGE_TYPE_NOTIFICATION;
      msg_obj.progress = -1;
      CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg_obj);
   }

   if (((right && !last_right) || (left && !last_left)) && subtitle_streams_num > 0)
   {
      char msg[256];
      struct retro_message_ext msg_obj = {0};
      int adjustment = (right) ? (+1) : ((left) ? (-1) : (0));

      seek_l2 = 0;

      msg[0] = '\0';

      slock_lock(decode_thread_lock);
      g_ctx.subtitle_stream_idx = (((g_ctx.subtitle_stream_idx + adjustment) % (subtitle_streams_num + 1)) + (subtitle_streams_num + 1)) % (subtitle_streams_num + 1);
      slock_unlock(decode_thread_lock);

      if(g_ctx.subtitle_stream_idx)
         snprintf(msg, sizeof(msg), "Subtitle Track #%d.", g_ctx.subtitle_stream_idx - 1);
      else
         snprintf(msg, sizeof(msg), "Subtitles Disabled.");

      msg_obj.msg      = msg;
      msg_obj.duration = 3000;
      msg_obj.priority = 1;
      msg_obj.level    = RETRO_LOG_INFO;
      msg_obj.target   = RETRO_MESSAGE_TARGET_ALL;
      msg_obj.type     = RETRO_MESSAGE_TYPE_NOTIFICATION;
      msg_obj.progress = -1;
      CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg_obj);
   }

   last_left  = left;
   last_right = right;
   last_up    = up;
   last_down  = down;
   last_l1     = l1;
   last_l2     = l2;
   last_r1     = r1;
   last_r2     = r2;

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

   g_ctx.decoded_frame_cnt++;

   /* Have to decode audio before video
    * in case there are PTS discontinuities
    * due to seeking. */
   if (audio_streams_num > 0)
   {
      /* Audio */
      double reading_pts;
      double expected_pts;
      double old_pts_bias;
      size_t to_read_bytes;
      uint64_t expected_audio_frames = g_ctx.decoded_frame_cnt * media.sample_rate / media.interpolate_fps;

      to_read_frames = expected_audio_frames - audio_frames;
      to_read_bytes = to_read_frames * sizeof(int16_t) * 2;

      slock_lock(fifo_lock);
      while (!decode_thread_dead && FIFO_READ_AVAIL(audio_decode_fifo) < to_read_bytes)
      {
         main_sleeping = true;
         scond_signal(fifo_decode_cond);
         scond_wait(fifo_cond, fifo_lock);
         main_sleeping = false;
      }

      reading_pts  = decode_last_audio_time -
         (double)FIFO_READ_AVAIL(audio_decode_fifo) / (media.sample_rate * sizeof(int16_t) * 2);
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

   min_pts = g_ctx.decoded_frame_cnt / media.interpolate_fps + pts_bias;

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

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
      if (use_gl)
      {
         float mix_factor;

         while (!decode_thread_dead && min_pts > frames[1].pts)
         {
            int64_t pts = 0;

            if (!decode_thread_dead)
               video_buffer_wait_for_finished_slot(video_buffer);

            if (!decode_thread_dead)
            {
               unsigned y;
               int stride, width;
               const uint8_t *src           = NULL;
               video_decoder_context_t *ctx = NULL;
               uint32_t               *data = NULL;

               video_buffer_get_finished_slot(video_buffer, &ctx);
               pts                          = ctx->pts;

#ifdef HAVE_SSA
               double video_time = ctx->pts * av_q2d(fctx->streams[video_stream_index]->time_base);
               slock_lock(ass_lock);
               if (ass_render && ctx->ass_track_active)
               {
                  int change     = 0;
                  ASS_Image *img = ass_render_frame(ass_render, ctx->ass_track_active,
                     1000 * video_time, &change);
                  render_ass_img(ctx->target, img);
               }
               slock_unlock(ass_lock);
#endif

#ifdef HAVE_OPENGLES
               data                         = video_frame_temp_buffer;
#else
               glBindBuffer(GL_PIXEL_UNPACK_BUFFER, frames[1].pbo);
#ifdef __MACH__
               data                         = (uint32_t*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
#else
               data                         = (uint32_t*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER,
                     0, media.width * media.height * sizeof(uint32_t), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
#endif
#endif
               src                          = ctx->target->data[0];
               stride                       = ctx->target->linesize[0];
               width                        = media.width * sizeof(uint32_t);
               for (y = 0; y < media.height; y++, src += stride, data += width/4)
                  memcpy(data, src, width);

#ifndef HAVE_OPENGLES
               glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
#endif

               glBindTexture(GL_TEXTURE_2D, frames[1].tex);
#if defined(HAVE_OPENGLES)
               glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     media.width, media.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, video_frame_temp_buffer);
#else
               glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     media.width, media.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
#endif
               glBindTexture(GL_TEXTURE_2D, 0);
#ifndef HAVE_OPENGLES
               glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif
               video_buffer_open_slot(video_buffer, ctx);
            }

            frames[1].pts = av_q2d(fctx->streams[video_stream_index]->time_base) * pts;
         }

         mix_factor = (min_pts - frames[0].pts) / (frames[1].pts - frames[0].pts);

         if (!temporal_interpolation)
            mix_factor = 1.0f;

         glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)hw_render.get_current_framebuffer());
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
         while (!decode_thread_dead && min_pts > frames[1].pts)
         {
            int64_t pts = 0;

            if (!decode_thread_dead)
               video_buffer_wait_for_finished_slot(video_buffer);

            if (!decode_thread_dead)
            {
               unsigned y;
               const uint8_t *src;
               int stride, width;
               uint32_t *data               = video_frame_temp_buffer;
               video_decoder_context_t *ctx = NULL;

               video_buffer_get_finished_slot(video_buffer, &ctx);
               pts                          = ctx->pts;
               src                          = ctx->target->data[0];
               stride                       = ctx->target->linesize[0];
               width                        = media.width * sizeof(uint32_t);
               for (y = 0; y < media.height; y++, src += stride, data += width/4)
                  memcpy(data, src, width);

               dupe                         = false;
               video_buffer_open_slot(video_buffer, ctx);
            }

            frames[1].pts = av_q2d(fctx->streams[video_stream_index]->time_base) * pts;
         }

         CORE_PREFIX(video_cb)(dupe ? NULL : video_frame_temp_buffer,
               media.width, media.height, media.width * sizeof(uint32_t));
      }
   }
#if defined(HAVE_GL_FFT) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGLES))
   else if (fft)
   {
      unsigned       fft_frames = to_read_frames;
      const int16_t *buffer = audio_buffer;

      while (fft_frames)
      {
         unsigned to_read = fft_frames;

         /* FFT size we use (1 << 11). Really shouldn't happen,
          * unless we use a crazy high sample rate. */
         if (to_read > (1 << 11))
            to_read = 1 << 11;

         fft_step_fft(fft, buffer, to_read);
         buffer += to_read * 2;
         fft_frames -= to_read;
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

#if ENABLE_HW_ACCEL
/*
 * Try to initialize a specific HW decoder defined by type.
 * Optionally tests the pixel format list for a compatible pixel format.
 */
static enum AVPixelFormat init_hw_decoder(struct AVCodecContext *ctx,
                                    const enum AVHWDeviceType type,
                                    const enum AVPixelFormat *pix_fmts)
{
#if !FFMPEG3
   int i;
#endif
   int ret = 0;
   enum AVPixelFormat decoder_pix_fmt = AV_PIX_FMT_NONE;
   const AVCodec *codec = avcodec_find_decoder(fctx->streams[video_stream_index]->codecpar->codec_id);

#if !FFMPEG3
   for (i = 0;; i++)
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
         enum AVPixelFormat device_pix_fmt = config->pix_fmt;
#else
   enum AVPixelFormat device_pix_fmt =
      pix_fmts ? ctx->get_format(ctx, pix_fmts) : decoder_pix_fmt;
#endif
         log_cb(RETRO_LOG_INFO, "[FFMPEG] Selected HW decoder %s.\n",
                  av_hwdevice_get_type_name(type));
         log_cb(RETRO_LOG_INFO, "[FFMPEG] Selected HW pixel format %s.\n",
                  av_get_pix_fmt_name(device_pix_fmt));

         if (pix_fmts != NULL)
         {
            /* Look if codec can supports the pix format of the device */
            for (size_t j = 0; pix_fmts[j] != AV_PIX_FMT_NONE; j++)
               if (pix_fmts[j] == device_pix_fmt)
               {
                  decoder_pix_fmt = pix_fmts[j];
                  goto exit;
               }
            log_cb(RETRO_LOG_ERROR, "[FFMPEG] Codec %s does not support device pixel format %s.\n",
                  codec->name, av_get_pix_fmt_name(device_pix_fmt));
         }
         else
         {
            decoder_pix_fmt = device_pix_fmt;
            goto exit;
         }
#if !FFMPEG3
      }
   }
#endif

exit:
   if (decoder_pix_fmt != AV_PIX_FMT_NONE)
   {
      AVBufferRef *hw_device_ctx;
      if ((ret = av_hwdevice_ctx_create(&hw_device_ctx,
                                       type, NULL, NULL, 0)) < 0)
      {
#ifdef __cplusplus
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to create specified HW device: %d\n", ret);
#else
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to create specified HW device: %s\n", av_err2str(ret));
#endif
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

#if ENABLE_HW_ACCEL
   if (!force_sw_decoder)
   {
      if (hw_decoder == AV_HWDEVICE_TYPE_NONE)
         format              = auto_hw_decoder(ctx, pix_fmts);
      else
         format              = init_hw_decoder(ctx, hw_decoder, pix_fmts);
   }

   /* Fallback to SW rendering */
   if (format == AV_PIX_FMT_NONE)
   {
#endif

      log_cb(RETRO_LOG_INFO, "[FFMPEG] Using SW decoding.\n");

      ctx->thread_type       = FF_THREAD_FRAME;
      ctx->thread_count      = sw_decoder_threads;
      log_cb(RETRO_LOG_INFO, "[FFMPEG] Configured software decoding threads: %d\n", sw_decoder_threads);

      format                 = (enum AVPixelFormat)fctx->streams[video_stream_index]->codecpar->format;

#if ENABLE_HW_ACCEL
      hw_decoding_enabled    = false;
   }
   else
      hw_decoding_enabled    = true;
#endif

   return format;
}

#if ENABLE_HW_ACCEL
/* Callback used by ffmpeg to configure the pixelformat to use. */
static enum AVPixelFormat get_format(AVCodecContext *ctx,
                                     const enum AVPixelFormat *pix_fmts)
{
   /* Look if we can reuse the current decoder */
   for (size_t i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++)
   {
      if (pix_fmts[i] == g_ctx.hw_pix_fmt)
         return g_ctx.hw_pix_fmt;
   }

   g_ctx.hw_pix_fmt = select_decoder(ctx, pix_fmts);

   return g_ctx.hw_pix_fmt;
}
#endif

static bool open_codec(AVCodecContext **ctx, enum AVMediaType type, unsigned index)
{
   int ret              = 0;
   const AVCodec *codec = avcodec_find_decoder(fctx->streams[index]->codecpar->codec_id);
   if (!codec)
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Couldn't find suitable decoder\n");
      return false;
   }

   *ctx = avcodec_alloc_context3(codec);
   avcodec_parameters_to_context((*ctx), fctx->streams[index]->codecpar);

   if (type == AVMEDIA_TYPE_VIDEO)
   {
      video_stream_index = index;

#if ENABLE_HW_ACCEL
      vctx->get_format  = get_format;
      g_ctx.hw_pix_fmt = select_decoder((*ctx), NULL);
#else
      select_decoder((*ctx), NULL);
#endif
   }

   if ((ret = avcodec_open2(*ctx, codec, NULL)) < 0)
   {
#ifdef __cplusplus
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Could not open codec: %d\n", ret);
#else
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Could not open codec: %s\n", av_err2str(ret));
#endif
      return false;
   }

   return true;
}

static bool codec_is_image(enum AVCodecID id)
{
   switch (id)
   {
      case AV_CODEC_ID_MJPEG:
      case AV_CODEC_ID_PNG:
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
      case AV_CODEC_ID_TTF:
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
      case AV_CODEC_ID_ASS:
      case AV_CODEC_ID_SSA:
      case AV_CODEC_ID_SUBRIP:
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
   g_ctx.audio_stream_idx    = 0;
   g_ctx.subtitle_stream_idx = 0;
   slock_unlock(decode_thread_lock);

   memset(audio_streams,    0, sizeof(audio_streams));
   memset(subtitle_streams, 0, sizeof(subtitle_streams));

   for (i = 0; i < fctx->nb_streams; i++)
   {
      enum AVMediaType type = fctx->streams[i]->codecpar->codec_type;
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
            if (!vctx
                  && !codec_is_image(fctx->streams[i]->codecpar->codec_id))
            {
               if (!open_codec(&vctx, type, i))
                  return false;
            }
            break;

         case AVMEDIA_TYPE_SUBTITLE:
#ifdef HAVE_SSA
            if (subtitle_streams_num < MAX_STREAMS
                  && codec_id_is_ass(fctx->streams[i]->codecpar->codec_id))
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
               AVCodecParameters *params = fctx->streams[i]->codecpar;
               if (codec_id_is_ttf(params->codec_id))
                  append_attachment(params->extradata, params->extradata_size);
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

   if (fctx)
   {
      if (fctx->duration != AV_NOPTS_VALUE)
      {
         int64_t duration        = fctx->duration + (fctx->duration <= INT64_MAX - 5000 ? 5000 : 0);
         media.duration.time     = (double)(duration / AV_TIME_BASE);
         media.duration.seconds  = (unsigned)media.duration.time;
         media.duration.minutes  = media.duration.seconds / 60;
         media.duration.seconds %= 60;
         media.duration.hours    = media.duration.minutes / 60;
         media.duration.minutes %= 60;
      }
      else
      {
         media.duration.time    = 0.0;
         media.duration.hours   = 0;
         media.duration.minutes = 0;
         media.duration.seconds = 0;
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Could not determine media duration.\n");
      }
   }

#ifdef HAVE_SSA
   if (sctx[0])
   {
      unsigned i;

      g_ctx.ass_lib = ass_library_init();
      ass_set_message_cb(g_ctx.ass_lib, ass_msg_cb, NULL);

      for (i = 0; i < attachments_size; i++)
         ass_add_font(g_ctx.ass_lib, (char*)"",
               (char*)attachments[i].data, attachments[i].size);

      ass_render = ass_renderer_init(g_ctx.ass_lib);
      ass_set_frame_size(ass_render, media.width, media.height);
      ass_set_extract_fonts(g_ctx.ass_lib, true);
      ass_set_fonts(ass_render, NULL, NULL, 1, NULL, 1);
      ass_set_hinting(ass_render, ASS_HINTING_LIGHT);

      for (i = 0; i < (unsigned)subtitle_streams_num; i++)
      {
         ass_track[i] = ass_new_track(g_ctx.ass_lib);
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

   if (g_ctx.color_space == AVCOL_SPC_UNSPECIFIED)
   {
      if (default_color != AVCOL_SPC_UNSPECIFIED)
         coeffs = sws_getCoefficients(default_color);
      else if (width >= 1280 || height > 576)
         coeffs = sws_getCoefficients(AVCOL_SPC_BT709);
      else
         coeffs = sws_getCoefficients(AVCOL_SPC_BT470BG);
   }
   else
      coeffs = sws_getCoefficients(g_ctx.color_space);

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

static void sws_worker_thread(void *arg)
{
   int ret = 0;
   AVFrame *tmp_frame = NULL;
   video_decoder_context_t *ctx = (video_decoder_context_t*) arg;

#if ENABLE_HW_ACCEL
   if (hw_decoding_enabled)
      tmp_frame = ctx->hw_source;
   else
#endif
      tmp_frame = ctx->source;

   ctx->sws = sws_getCachedContext(ctx->sws,
         media.width, media.height, (enum AVPixelFormat)tmp_frame->format,
         media.width, media.height, AV_PIX_FMT_RGB32,
         SWS_POINT, NULL, NULL, NULL);

   set_colorspace(ctx->sws, media.width, media.height,
         tmp_frame->colorspace,
         tmp_frame->color_range);

   if ((ret = sws_scale(ctx->sws, (const uint8_t *const*)tmp_frame->data,
         tmp_frame->linesize, 0, media.height,
         (uint8_t * const*)ctx->target->data, ctx->target->linesize)) < 0)
   {
#ifdef __cplusplus
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error while scaling image: %d\n", ret);
#else
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error while scaling image: %s\n", av_err2str(ret));
#endif
   }

   ctx->pts = ctx->source->best_effort_timestamp;

   av_frame_unref(ctx->source);
#if ENABLE_HW_ACCEL
   av_frame_unref(ctx->hw_source);
#endif

   video_buffer_finish_slot(video_buffer, ctx);
}

#ifdef HAVE_SSA
static void decode_video(AVCodecContext *ctx, AVPacket *pkt, size_t frame_size, ASS_Track *ass_track_active)
#else
static void decode_video(AVCodecContext *ctx, AVPacket *pkt, size_t frame_size)
#endif
{
   int ret = 0;
   video_decoder_context_t *decoder_ctx = NULL;

   /* Stop decoding thread until video_buffer is not full again */
   while (!decode_thread_dead && !video_buffer_has_open_slot(video_buffer))
   {
      if (main_sleeping)
      {
         if (!do_seek)
            log_cb(RETRO_LOG_ERROR, "[FFMPEG] Thread: Video deadlock detected.\n");
         tpool_wait(tpool);
         video_buffer_clear(video_buffer);
         return;
      }
   }

   if ((ret = avcodec_send_packet(ctx, pkt)) < 0)
   {
#ifdef __cplusplus
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Can't decode video packet: %d\n", ret);
#else
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Can't decode video packet: %s\n", av_err2str(ret));
#endif
      return;
   }

   while (!decode_thread_dead && video_buffer_has_open_slot(video_buffer))
   {
      video_buffer_get_open_slot(video_buffer, &decoder_ctx);

      ret = avcodec_receive_frame(ctx, decoder_ctx->source);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      {
         ret = DECODE_NO_FRAME;
         goto end;
      }
      else if (ret < 0)
      {
#ifdef __cplusplus
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error while reading video frame: %d\n", ret);
#else
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error while reading video frame: %s\n", av_err2str(ret));
#endif
         goto end;
      }

#if ENABLE_HW_ACCEL
      if (hw_decoding_enabled)
         /* Copy data from VRAM to RAM */
         if ((ret = av_hwframe_transfer_data(decoder_ctx->hw_source, decoder_ctx->source, 0)) < 0)
         {
#ifdef __cplusplus
               log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error transferring the data to system memory: %d\n", ret);
#else
               log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error transferring the data to system memory: %s\n", av_err2str(ret));
#endif
               goto end;
         }
#endif

#ifdef HAVE_SSA
      decoder_ctx->ass_track_active = ass_track_active;
#endif

      tpool_add_work(tpool, sws_worker_thread, decoder_ctx);

   end:
      if (ret < 0)
      {
         video_buffer_return_open_slot(video_buffer, decoder_ctx);
         break;
      }
   }

   return;
}

static int16_t *decode_audio(AVCodecContext *ctx, AVPacket *pkt,
      AVFrame *frame, int16_t *buffer, size_t *buffer_cap,
      SwrContext *swr)
{
   int ret = 0;
   int64_t pts = 0;
   size_t required_buffer = 0;

   if ((ret = avcodec_send_packet(ctx, pkt)) < 0)
   {
#ifdef __cplusplus
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Can't decode audio packet: %d\n", ret);
#else
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Can't decode audio packet: %s\n", av_err2str(ret));
#endif
      return buffer;
   }

   for (;;)
   {
      ret = avcodec_receive_frame(ctx, frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
         break;
      else if (ret < 0)
      {
#ifdef __cplusplus
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error while reading audio frame: %d\n", ret);
#else
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Error while reading audio frame: %s\n", av_err2str(ret));
#endif
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

      pts = frame->best_effort_timestamp;
      slock_lock(fifo_lock);

      while (!decode_thread_dead &&
            FIFO_WRITE_AVAIL(audio_decode_fifo) < required_buffer)
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
            fctx->streams[audio_streams[g_ctx.audio_stream_idx]]->time_base);

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

   decode_last_audio_time = time;

   if (avformat_seek_file(fctx, -1, INT64_MIN, seek_to, INT64_MAX, 0) < 0)
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] av_seek_frame() failed.\n");

   if (video_stream_index >= 0)
   {
      tpool_wait(tpool);
      video_buffer_clear(video_buffer);
   }

   if (actx[g_ctx.audio_stream_idx])
      avcodec_flush_buffers(actx[g_ctx.audio_stream_idx]);
   if (vctx)
      avcodec_flush_buffers(vctx);
   if (g_ctx.subtitle_stream_idx && sctx[g_ctx.subtitle_stream_idx - 1])
      avcodec_flush_buffers(sctx[g_ctx.subtitle_stream_idx - 1]);
#ifdef HAVE_SSA
   if (g_ctx.subtitle_stream_idx && ass_track[g_ctx.subtitle_stream_idx - 1])
      ass_flush_events(ass_track[g_ctx.subtitle_stream_idx - 1]);
#endif
}

/**
 * This function makes sure that we don't decode too many
 * packets and cause stalls in our decoding pipeline.
 * This could happen if we decode too many packets and
 * saturate our buffers. We have a window of "still okay"
 * to decode, that depends on the media fps.
 **/
static bool earlier_or_close_enough(double p1, double p2)
{
   return (p1 <= p2 || (p1-p2) < (1.0 / media.interpolate_fps) );
}

static void decode_thread(void *data)
{
   unsigned i;
   bool eof                = false;
   struct SwrContext *swr[(audio_streams_num > 0) ? audio_streams_num : 1];
   AVFrame *aud_frame      = NULL;
   size_t frame_size       = 0;
   int16_t *audio_buffer   = NULL;
   size_t audio_buffer_cap = 0;
   packet_buffer_t *audio_packet_buffer;
   packet_buffer_t *video_packet_buffer;
   double last_audio_end  = 0;

   (void)data;

   for (i = 0; (int)i < audio_streams_num; i++)
   {
      swr[i] = swr_alloc();

#if HAVE_CH_LAYOUT
      AVChannelLayout out_chlayout = AV_CHANNEL_LAYOUT_STEREO;
      av_opt_set_chlayout(swr[i], "in_chlayout", &actx[i]->ch_layout, 0);
      av_opt_set_chlayout(swr[i], "out_chlayout", &out_chlayout, 0);
#else
      av_opt_set_int(swr[i], "in_channel_layout", actx[i]->channel_layout, 0);
      av_opt_set_int(swr[i], "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
#endif
      av_opt_set_int(swr[i], "in_sample_rate", actx[i]->sample_rate, 0);
      av_opt_set_int(swr[i], "out_sample_rate", media.sample_rate, 0);
      av_opt_set_int(swr[i], "in_sample_fmt", actx[i]->sample_fmt, 0);
      av_opt_set_int(swr[i], "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
      swr_init(swr[i]);
   }

   aud_frame = av_frame_alloc();
   audio_packet_buffer = packet_buffer_create();
   video_packet_buffer = packet_buffer_create();

   if (video_stream_index >= 0)
   {
      frame_size = av_image_get_buffer_size(AV_PIX_FMT_RGB32, media.width, media.height, 1);
      video_buffer = video_buffer_create(4, (int)frame_size, media.width, media.height);
      tpool = tpool_create(sw_sws_threads);
      log_cb(RETRO_LOG_INFO, "[FFMPEG] Configured worker threads: %d\n", sw_sws_threads);
   }

   while (!decode_thread_dead)
   {
      bool seek;
      int subtitle_stream;
      double seek_time_thread;
      int audio_stream_index, audio_stream_ptr;

      double audio_timebase   = 0.0;
      double video_timebase   = 0.0;
      double next_video_end   = 0.0;
      double next_audio_start = 0.0;

      AVPacket *pkt = av_packet_alloc();
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
         do_seek          = false;
         eof              = false;
         seek_time        = 0.0;
         next_video_end   = 0.0;
         next_audio_start = 0.0;
         last_audio_end   = 0.0;

         if (audio_decode_fifo)
            fifo_clear(audio_decode_fifo);

         packet_buffer_clear(&audio_packet_buffer);
         packet_buffer_clear(&video_packet_buffer);

         scond_signal(fifo_cond);
         slock_unlock(fifo_lock);
      }

      slock_lock(decode_thread_lock);
      audio_stream_index          = audio_streams[g_ctx.audio_stream_idx];
      audio_stream_ptr            = g_ctx.audio_stream_idx;
      subtitle_stream             = g_ctx.subtitle_stream_idx ? subtitle_streams[g_ctx.subtitle_stream_idx - 1] : 0;
      actx_active                 = actx[g_ctx.audio_stream_idx];
      sctx_active                 = g_ctx.subtitle_stream_idx ? sctx[g_ctx.subtitle_stream_idx - 1] : 0;
#ifdef HAVE_SSA
      ass_track_active            = g_ctx.subtitle_stream_idx ? ass_track[g_ctx.subtitle_stream_idx - 1] : 0;
#endif
      audio_timebase = av_q2d(fctx->streams[audio_stream_index]->time_base);
      if (video_stream_index >= 0)
         video_timebase = av_q2d(fctx->streams[video_stream_index]->time_base);
      slock_unlock(decode_thread_lock);

      if (!packet_buffer_empty(audio_packet_buffer))
         next_audio_start = audio_timebase * packet_buffer_peek_start_pts(audio_packet_buffer);

      if (!packet_buffer_empty(video_packet_buffer))
         next_video_end = video_timebase * packet_buffer_peek_end_pts(video_packet_buffer);

      /*
       * Decode audio packet if:
       *  1. it's the start of file or it's audio only media
       *  2. there is a video packet for in the buffer
       *  3. EOF
       **/
      if (!packet_buffer_empty(audio_packet_buffer) &&
            (
               next_video_end == 0.0 ||
               (!eof && earlier_or_close_enough(next_audio_start, next_video_end)) ||
               eof
            )
         )
      {
         packet_buffer_get_packet(audio_packet_buffer, pkt);
         last_audio_end = audio_timebase * (pkt->pts + pkt->duration);
         audio_buffer = decode_audio(actx_active, pkt, aud_frame,
                                    audio_buffer, &audio_buffer_cap,
                                    swr[audio_stream_ptr]);
         av_packet_unref(pkt);
      }

      /*
       * Decode video packet if:
       *  1. we already decoded an audio packet
       *  2. there is no audio stream to play
       *  3. EOF
       **/
      if (!packet_buffer_empty(video_packet_buffer) &&
            (
               (!eof && earlier_or_close_enough(next_video_end, last_audio_end)) ||
               !actx_active ||
               eof
            )
         )
      {
         packet_buffer_get_packet(video_packet_buffer, pkt);

         #ifdef HAVE_SSA
         decode_video(vctx, pkt, frame_size, ass_track_active);
         #else
         decode_video(vctx, pkt, frame_size);
         #endif

         av_packet_unref(pkt);
      }

      if (packet_buffer_empty(audio_packet_buffer) && packet_buffer_empty(video_packet_buffer) && eof)
      {
         av_packet_free(&pkt);
         break;
      }

      /* Read the next frame and stage it in case of audio or video frame. */
      if (av_read_frame(fctx, pkt) < 0)
         eof = true;
      else if (pkt->stream_index == audio_stream_index && actx_active)
         packet_buffer_add_packet(audio_packet_buffer, pkt);
      else if (pkt->stream_index == video_stream_index)
         packet_buffer_add_packet(video_packet_buffer, pkt);
      else if (pkt->stream_index == subtitle_stream && sctx_active)
      {
         /**
          * Decode subtitle packets right away, since SSA/ASS can operate this way.
          * If we ever support other subtitles, we need to handle this with a
          * buffer too
          **/
         AVSubtitle sub;
         int finished = 0;

         memset(&sub, 0, sizeof(sub));

         while (!finished)
         {
            if (avcodec_decode_subtitle2(sctx_active, &sub, &finished, pkt) < 0)
            {
               log_cb(RETRO_LOG_ERROR, "[FFMPEG] Decode subtitles failed.\n");
               break;
            }
         }
         log_cb(RETRO_LOG_DEBUG, "[FFMPEG] [ASS] Decoded subtitle packet, num_rects=%d, pkt->pts=%lld, pkt->duration=%lld, sub.start=%u, sub.end=%u\n", 
            sub.num_rects, (long long)pkt->pts, (long long)pkt->duration, sub.start_display_time, sub.end_display_time);
#ifdef HAVE_SSA
         for (i = 0; i < sub.num_rects; i++)
         {
            slock_lock(ass_lock);
            ass_flush_events(ass_track_active);
            if (sub.rects[i]->ass && ass_track_active)
            {
               char dialogue_line[4096];
               
               /* Convert packet timing from stream timebase to milliseconds */
               double timebase_ms = av_q2d(fctx->streams[subtitle_stream]->time_base) * 1000.0;
               long long start_time = (long long)((pkt->pts >= 0 ? pkt->pts : 0) * timebase_ms) + sub.start_display_time;
               long long duration = (long long)(pkt->duration > 0 ? (pkt->duration * timebase_ms) : (sub.end_display_time - sub.start_display_time));
               
               snprintf(dialogue_line, sizeof(dialogue_line), "Dialogue: %s", sub.rects[i]->ass);
               
               ass_process_chunk(ass_track_active, dialogue_line, strlen(dialogue_line), start_time, duration);
            }
            slock_unlock(ass_lock);
         }
#endif
         avsubtitle_free(&sub);
         av_packet_unref(pkt);
      }
      av_packet_free(&pkt);
   }

   for (i = 0; (int)i < audio_streams_num; i++)
      swr_free(&swr[i]);

#if ENABLE_HW_ACCEL
   if (vctx && vctx->hw_device_ctx)
      av_buffer_unref(&vctx->hw_device_ctx);
#endif

   packet_buffer_destroy(audio_packet_buffer);
   packet_buffer_destroy(video_packet_buffer);

   av_frame_free(&aud_frame);
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

      tpool_wait(tpool);
      video_buffer_clear(video_buffer);
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
#ifdef HAVE_SSA
   if (ass_lock)
      slock_free(ass_lock);
#endif

   if (audio_decode_fifo)
      fifo_free(audio_decode_fifo);

   fifo_cond = NULL;
   fifo_decode_cond = NULL;
   fifo_lock = NULL;
   decode_thread_lock = NULL;
   audio_decode_fifo = NULL;
#ifdef HAVE_SSA
   ass_lock = NULL;
#endif

   decode_last_audio_time = 0.0;

   frames[0].pts = frames[1].pts = 0.0;
   pts_bias = 0.0;
   g_ctx.decoded_frame_cnt = 0;
   audio_frames = 0;

   for (i = 0; i < MAX_STREAMS; i++)
   {
#if FFMPEG8
      if (sctx[i])
         avcodec_free_context(&sctx[i]);
      if (actx[i])
         avcodec_free_context(&actx[i]);
#else
      if (sctx[i])
         avcodec_close(sctx[i]);
      if (actx[i])
         avcodec_close(actx[i]);
#endif
      sctx[i] = NULL;
      actx[i] = NULL;
   }

   if (vctx)
   {
#if FFMPEG8
      avcodec_free_context(&vctx);
#else
      avcodec_close(vctx);
#endif
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
   if (g_ctx.ass_lib)
      ass_library_done(g_ctx.ass_lib);

   ass_render = NULL;
   g_ctx.ass_lib = NULL;
#endif

   av_freep(&video_frame_temp_buffer);
}

bool CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
   int ret = 0;
   bool is_fft = false;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Decrement Subtitle Index" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Increment Subtitle Index" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Increment Audio Index" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Decrement Audio Index" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Seek -60 seconds" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Seek +60 seconds" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "Seek Decrementally" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "Seek Incrementally" },

      { 0 },
   };

   if (!info)
      return false;

   check_variables(true);

   CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (!CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Cannot set pixel format.\n");
      goto error;
   }

   if ((ret = avformat_open_input(&fctx, info->path, NULL, NULL)) < 0)
   {
#ifdef __cplusplus
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to open input: %d\n", ret);
#else
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to open input: %s\n", av_err2str(ret));
#endif
      goto error;
   }

   print_ffmpeg_version();

   if ((ret = avformat_find_stream_info(fctx, NULL)) < 0)
   {
#ifdef __cplusplus
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to find stream info: %d\n", ret);
#else
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to find stream info: %s\n", av_err2str(ret));
#endif
      goto error;
   }

   log_cb(RETRO_LOG_INFO, "[FFMPEG] Media information:\n");
   av_dump_format(fctx, 0, info->path, 0);

   if (!open_codecs())
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to find codec.\n");
      goto error;
   }

   if (!init_media_info())
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to init media info.\n");
      goto error;
   }

#ifdef HAVE_GL_FFT
   is_fft = video_stream_index < 0 && audio_streams_num > 0;
#endif

   if (video_stream_index >= 0 || is_fft)
   {
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
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Cannot initialize HW render.\n");
      }
#endif
   }
   if (audio_streams_num > 0)
   {
      /* audio fifo is 2 seconds deep */
      audio_decode_fifo = fifo_new(
         media.sample_rate * sizeof(int16_t) * 2 * 2
      );
   }

   fifo_cond        = scond_new();
   fifo_decode_cond = scond_new();
   fifo_lock        = slock_new();
#ifdef HAVE_SSA
   ass_lock         = slock_new();
#endif

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

typedef struct
{
   uint64_t frame_cnt;
   int audio_streams_ptr;
   int subtitle_streams_ptr;
} serialized_data;

size_t CORE_PREFIX(retro_serialize_size)(void)
{
   return sizeof(serialized_data);
}

bool CORE_PREFIX(retro_serialize)(void *data, size_t len)
{
   serialized_data info;

   slock_lock(decode_thread_lock);
   info.frame_cnt = g_ctx.decoded_frame_cnt;
   info.audio_streams_ptr = g_ctx.audio_stream_idx;
   info.subtitle_streams_ptr = g_ctx.subtitle_stream_idx;
   slock_unlock(decode_thread_lock);

   if (sizeof(serialized_data) <= len)
   {
      memcpy(data, &info, sizeof(serialized_data));

      return true;
   }

   return false;
}

bool CORE_PREFIX(retro_unserialize)(const void *data, size_t len)
{
   serialized_data info;
   info.frame_cnt = 0;
   info.audio_streams_ptr = 0;
   info.subtitle_streams_ptr = 0;

   if (sizeof(serialized_data) <= len)
   {
      memcpy(&info, data, sizeof(serialized_data));

      slock_lock(decode_thread_lock);
      g_ctx.audio_stream_idx = info.audio_streams_ptr;
      g_ctx.subtitle_stream_idx = info.subtitle_streams_ptr;
      slock_unlock(decode_thread_lock);

      seek_frame(info.frame_cnt - g_ctx.decoded_frame_cnt);

      return true;
   }

   return false;
}

void *CORE_PREFIX(retro_get_memory_data)(unsigned id) { return NULL; }
size_t CORE_PREFIX(retro_get_memory_size)(unsigned id) { return 0; }
void CORE_PREFIX(retro_cheat_reset)(void) { }
void CORE_PREFIX(retro_cheat_set)(unsigned a, bool b, const char *c) { }

#if defined(LIBRETRO_SWITCH)

#ifdef ARCH_X86
#include "../libswresample/resample.h"
void swri_resample_dsp_init(ResampleContext *c)
{}
#endif

#endif
