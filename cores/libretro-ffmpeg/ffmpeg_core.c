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

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include <glsym/glsym.h>

#include <math.h>
#include <retro_inline.h>
#include <filters.h>
#include <math/complex.h>
#include <gfx/math/matrix_4x4.h>
#include <gfx/math/vector_2.h>
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

/* RESET_TRIGGERED_STR and LIBRETRO_SUPPORTS_BITMASKS_STR moved into ffmpeg_core_ctx_t */

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

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)

#if GL_DEBUG
#define GL_CHECK_ERROR() do { \
   if (glGetError() != GL_NO_ERROR) \
   { \
      log_cb(RETRO_LOG_ERROR, "GL error at line: %d\n", __LINE__); \
      abort(); \
   } \
} while(0)
#else
#define GL_CHECK_ERROR()
#endif

#ifndef M_HALF_PI
#define M_HALF_PI 1.57079632679489661923132169163975144
#endif

struct target
{
   GLuint tex;
   GLuint fbo;
};

struct Pass
{
   struct target target;
   GLuint parameter_tex;
};

struct GLFFT
{
   GLuint ms_rb_color;
   GLuint ms_rb_ds;
   GLuint ms_fbo;

   struct Pass *passes;
   unsigned passes_size;

   GLuint input_tex;
   GLuint window_tex;
   GLuint prog_real;
   GLuint prog_complex;
   GLuint prog_resolve;
   GLuint prog_blur;

   GLuint quad;
   GLuint vao;

   unsigned output_ptr;

   struct target output, resolve, blur;

   struct Block
   {
      GLuint prog;
      GLuint vao;
      GLuint vbo;
      GLuint ibo;
      unsigned elems;
   } block;

   GLuint pbo;
   GLshort *sliding;
   unsigned sliding_size;

   unsigned steps;
   unsigned size;
   unsigned block_size;
   unsigned depth;
};

typedef struct GLFFT hwfft_t;

#endif /* HAVE_OPENGL || HAVE_OPENGLES3 */

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

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   hwfft_t *fft;
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
   int seek_l2;
   int seek_r2;

   /* Input edge-detection (previous frame state) */
   bool last_left;
   bool last_right;
   bool last_up;
   bool last_down;
   bool last_l1;
   bool last_l2;
   bool last_r1;
   bool last_r2;

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
#define RESET_TRIGGERED_STR        (g_ctx.reset_triggered)
#define LIBRETRO_SUPPORTS_BITMASKS_STR (g_ctx.libretro_supports_bitmasks)
#define FCTX_STR                   (g_ctx.fctx)
#define VCTX_STR                   (g_ctx.vctx)
#define VIDEO_STREAM_INDEX_STR     (g_ctx.video_stream_index)
#define SW_DECODER_THREADS_STR     (g_ctx.sw_decoder_threads)
#define SW_SWS_THREADS_STR         (g_ctx.sw_sws_threads)
#define VIDEO_BUFFER_STR           (g_ctx.video_buffer)
#define TPOOL_STR                  (g_ctx.tpool)
#if ENABLE_HW_ACCEL
#define HW_DECODER_STR             (g_ctx.hw_decoder)
#define HW_DECODING_ENABLED_STR    (g_ctx.hw_decoding_enabled)
#define FORCE_SW_DECODER_STR       (g_ctx.force_sw_decoder)
#endif
#define ACTX_STR                   (g_ctx.actx)
#define SCTX_STR                   (g_ctx.sctx)
#define AUDIO_STREAMS_STR          (g_ctx.audio_streams)
#define AUDIO_STREAMS_NUM_STR      (g_ctx.audio_streams_num)
#define SUBTITLE_STREAMS_STR       (g_ctx.subtitle_streams)
#define SUBTITLE_STREAMS_NUM_STR   (g_ctx.subtitle_streams_num)
#ifdef HAVE_SSA
#define ASS_RENDER_STR             (g_ctx.ass_render)
#define ASS_TRACK_STR              (g_ctx.ass_track)
#define ASS_EXTRA_DATA_STR         (g_ctx.ass_extra_data)
#define ASS_EXTRA_DATA_SIZE_STR    (g_ctx.ass_extra_data_size)
#define ASS_LOCK_STR               (g_ctx.ass_lock)
#endif
#define ATTACHMENTS_STR            (g_ctx.attachments)
#define ATTACHMENTS_SIZE_STR       (g_ctx.attachments_size)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
#define FFT_STR                    (g_ctx.fft)
#define FFT_WIDTH_STR              (g_ctx.fft_width)
#define FFT_HEIGHT_STR             (g_ctx.fft_height)
#define FFT_MULTISAMPLE_STR        (g_ctx.fft_multisample)
#endif
#define AUDIO_FRAMES_STR           (g_ctx.audio_frames)
#define PTS_BIAS_STR               (g_ctx.pts_bias)
#define DECODE_THREAD_DEAD_STR     (g_ctx.decode_thread_dead)
#define AUDIO_DECODE_FIFO_STR      (g_ctx.audio_decode_fifo)
#define FIFO_COND_STR              (g_ctx.fifo_cond)
#define FIFO_DECODE_COND_STR       (g_ctx.fifo_decode_cond)
#define FIFO_LOCK_STR              (g_ctx.fifo_lock)
#define DECODE_THREAD_LOCK_STR     (g_ctx.decode_thread_lock)
#define DECODE_THREAD_HANDLE_STR   (g_ctx.decode_thread_handle)
#define DECODE_LAST_AUDIO_TIME_STR (g_ctx.decode_last_audio_time)
#define MAIN_SLEEPING_STR          (g_ctx.main_sleeping)
#define VIDEO_FRAME_TEMP_BUFFER_STR (g_ctx.video_frame_temp_buffer)
#define DO_SEEK_STR                (g_ctx.do_seek)
#define SEEK_TIME_STR              (g_ctx.seek_time)
#define SEEK_L2_STR                (g_ctx.seek_l2)
#define SEEK_R2_STR                (g_ctx.seek_r2)
#define LAST_LEFT_STR              (g_ctx.last_left)
#define LAST_RIGHT_STR             (g_ctx.last_right)
#define LAST_UP_STR                (g_ctx.last_up)
#define LAST_DOWN_STR              (g_ctx.last_down)
#define LAST_L1_STR                (g_ctx.last_l1)
#define LAST_L2_STR                (g_ctx.last_l2)
#define LAST_R1_STR                (g_ctx.last_r1)
#define LAST_R2_STR                (g_ctx.last_r2)
#define FRAMES_STR                 (g_ctx.frames)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#define TEMPORAL_INTERPOLATION_STR (g_ctx.temporal_interpolation)
#define USE_GL_STR                 (g_ctx.use_gl)
#define HW_RENDER_STR              (g_ctx.hw_render)
#define PROG_STR                   (g_ctx.prog)
#define VBO_STR                    (g_ctx.vbo)
#define VERTEX_LOC_STR             (g_ctx.vertex_loc)
#define TEX_LOC_STR                (g_ctx.tex_loc)
#define MIX_LOC_STR                (g_ctx.mix_loc)
#endif
#define MEDIA_STR                  (g_ctx.media)

/* ---- GL FFT implementation (merged from ffmpeg_fft.c) ---- */
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)

#ifndef CG
#if defined(HAVE_OPENGLES)
#define CG(src)   "" #src
#else
#define CG(src)   "" #src
#endif
#endif

#ifndef GLSL
#if defined(HAVE_OPENGLES)
#define GLSL(src) "precision mediump float;\n" #src
#else
#define GLSL(src) "" #src
#endif
#endif

#ifndef GLSL_300
#define GLSL_300(src)   "#version 300 es\n"   #src
#endif

static const char *fft_vertex_program_heightmap = GLSL_300(
   layout(location = 0) in vec2 aVertex;
   uniform sampler2D sHeight;
   uniform mat4 uMVP;
   uniform ivec2 uOffset;
   uniform vec4 uHeightmapParams;
   uniform float uAngleScale;
   out vec3 vWorldPos;
   out vec3 vHeight;

   void main() {
     vec2 tex_coord = vec2(aVertex.x + float(uOffset.x) + 0.5, -aVertex.y + float(uOffset.y) + 0.5) / vec2(textureSize(sHeight, 0));

     vec3 world_pos = vec3(aVertex.x, 0.0, aVertex.y);
     world_pos.xz += uHeightmapParams.xy;

     float angle = world_pos.x * uAngleScale;
     world_pos.xz *= uHeightmapParams.zw;

     float lod = log2(world_pos.z + 1.0) - 6.0;
     vec4 heights = textureLod(sHeight, tex_coord, lod);

     float cangle = cos(angle);
     float sangle = sin(angle);

     int c = int(-sign(world_pos.x) + 1.0);
     float height = mix(heights[c], heights[1], abs(angle) / 3.141592653);
     height = height * 80.0 - 40.0;

     vec3 up = vec3(-sangle, cangle, 0.0);

     float base_y = 80.0 - 80.0 * cangle;
     float base_x = 80.0 * sangle;
     world_pos.xy = vec2(base_x, base_y);
     world_pos += up * height;

     vWorldPos = world_pos;
     vHeight = vec3(height, heights.yw * 80.0 - 40.0);
     gl_Position = uMVP * vec4(world_pos, 1.0);
   }
);
static const char *fft_fragment_program_heightmap = GLSL_300(
   precision mediump float;
   out vec4 FragColor;
   in vec3 vWorldPos;
   in vec3 vHeight;

   vec3 colormap(vec3 height) {
      return 1.0 / (1.0 + exp(-0.08 * height));
   }

   void main() {
      vec3 color = mix(vec3(1.0, 0.7, 0.7) * colormap(vHeight), vec3(0.1, 0.15, 0.1), clamp(vWorldPos.z / 400.0, 0.0, 1.0));
      color = mix(color, vec3(0.1, 0.15, 0.1), clamp(1.0 - vWorldPos.z / 2.0, 0.0, 1.0));
      FragColor = vec4(color, 1.0);
   }
);
static const char *fft_vertex_program = GLSL_300(
   precision mediump float;
   layout(location = 0) in vec2 aVertex;
   layout(location = 1) in vec2 aTexCoord;
   uniform vec4 uOffsetScale;
   out vec2 vTex;
   void main() {
      vTex = uOffsetScale.xy + aTexCoord * uOffsetScale.zw;
      gl_Position = vec4(aVertex, 0.0, 1.0);
   }
);
static const char *fft_fragment_program_resolve = GLSL_300(
   precision mediump float;
   precision highp int;
   precision highp usampler2D;
   precision highp isampler2D;
   in vec2 vTex;
   out vec4 FragColor;
   uniform usampler2D sFFT;

   vec4 get_heights(highp uvec2 h) {
     vec2 l = unpackHalf2x16(h.x);
     vec2 r = unpackHalf2x16(h.y);
     vec2 channels[4] = vec2[4](
        l, 0.5 * (l + r), r, 0.5 * (l - r));
     vec4 amps;
     for (int i = 0; i < 4; i++)
        amps[i] = dot(channels[i], channels[i]);

     return 9.0 * log(amps + 0.0001) - 22.0;
   }

   void main() {
      uvec2 h = textureLod(sFFT, vTex, 0.0).rg;
      vec4 height = get_heights(h);
      height = (height + 40.0) / 80.0;
      FragColor = height;
   }
);
static const char *fft_fragment_program_real = GLSL_300(
   precision mediump float;
   precision highp int;
   precision highp usampler2D;
   precision highp isampler2D;

   in vec2 vTex;
   uniform isampler2D sTexture;
   uniform usampler2D sParameterTexture;
   uniform usampler2D sWindow;
   uniform int uViewportOffset;
   out uvec2 FragColor;

   vec2 compMul(vec2 a, vec2 b) { return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }

   void main() {
      uvec2 params = texture(sParameterTexture, vec2(vTex.x, 0.5)).rg;
      uvec2 coord  = uvec2((params.x >> 16u) & 0xffffu, params.x & 0xffffu);
      int ycoord   = int(gl_FragCoord.y) - uViewportOffset;
      vec2 twiddle = unpackHalf2x16(params.y);

      float window_a = float(texelFetch(sWindow, ivec2(coord.x, 0), 0).r) / float(0x10000);
      float window_b = float(texelFetch(sWindow, ivec2(coord.y, 0), 0).r) / float(0x10000);

      vec2 a = window_a * vec2(texelFetch(sTexture, ivec2(int(coord.x), ycoord), 0).rg) / vec2(0x8000);
      vec2 a_l = vec2(a.x, 0.0);
      vec2 a_r = vec2(a.y, 0.0);
      vec2 b = window_b * vec2(texelFetch(sTexture, ivec2(int(coord.y), ycoord), 0).rg) / vec2(0x8000);
      vec2 b_l = vec2(b.x, 0.0);
      vec2 b_r = vec2(b.y, 0.0);
      b_l = compMul(b_l, twiddle);
      b_r = compMul(b_r, twiddle);

      vec2 res_l = a_l + b_l;
      vec2 res_r = a_r + b_r;
      FragColor = uvec2(packHalf2x16(res_l), packHalf2x16(res_r));
   }
);
static const char *fft_fragment_program_complex = GLSL_300(
   precision mediump float;
   precision highp int;
   precision highp usampler2D;
   precision highp isampler2D;

   in vec2 vTex;
   uniform usampler2D sTexture;
   uniform usampler2D sParameterTexture;
   uniform int uViewportOffset;
   out uvec2 FragColor;

   vec2 compMul(vec2 a, vec2 b) { return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }

   void main() {
      uvec2 params = texture(sParameterTexture, vec2(vTex.x, 0.5)).rg;
      uvec2 coord  = uvec2((params.x >> 16u) & 0xffffu, params.x & 0xffffu);
      int ycoord   = int(gl_FragCoord.y) - uViewportOffset;
      vec2 twiddle = unpackHalf2x16(params.y);

      uvec2 x = texelFetch(sTexture, ivec2(int(coord.x), ycoord), 0).rg;
      uvec2 y = texelFetch(sTexture, ivec2(int(coord.y), ycoord), 0).rg;
      vec4 a = vec4(unpackHalf2x16(x.x), unpackHalf2x16(x.y));
      vec4 b = vec4(unpackHalf2x16(y.x), unpackHalf2x16(y.y));
      b.xy = compMul(b.xy, twiddle);
      b.zw = compMul(b.zw, twiddle);

      vec4 res = a + b;
      FragColor = uvec2(packHalf2x16(res.xy), packHalf2x16(res.zw));
   }
);
static const char *fft_fragment_program_blur = GLSL_300(
   precision mediump float;
   precision highp int;
   precision highp usampler2D;
   precision highp isampler2D;
   in vec2 vTex;
   out vec4 FragColor;
   uniform sampler2D sHeight;

   void main() {
      float k = 0.0;
      float t;
      vec4 res = vec4(0.0);
      \n#define kernel(x, y) t = exp(-0.35 * float((x) * (x) + (y) * (y))); k += t; res += t * textureLodOffset(sHeight, vTex, 0.0, ivec2(x, y))\n
       kernel(-1, -2);
       kernel(-1, -1);
       kernel(-1,  0);
       kernel( 0, -2);
       kernel( 0, -1);
       kernel( 0,  0);
       kernel( 1, -2);
       kernel( 1, -1);
       kernel( 1,  0);
       FragColor = res / k;
   }
);

static GLuint hwfft_compile_shader(hwfft_t *fft, GLenum type, const char *source)
{
   GLint status  = 0;
   GLuint shader = glCreateShader(type);

   glShaderSource(shader, 1, (const GLchar**)&source, NULL);
   glCompileShader(shader);

   glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

   if (!status)
   {
      char log_info[8 * 1024];
      GLsizei log_len;

      log_cb(RETRO_LOG_ERROR, "Failed to compile.\n");
      glGetShaderInfoLog(shader, sizeof(log_info), &log_len, log_info);
      log_cb(RETRO_LOG_ERROR, "ERROR: %s\n", log_info);
      return 0;
   }

   return shader;
}

static GLuint hwfft_compile_program(hwfft_t *fft,
      const char *vertex_source, const char *fragment_source)
{
   GLint status = 0;
   GLuint prog  = glCreateProgram();
   GLuint vert  = hwfft_compile_shader(fft, GL_VERTEX_SHADER, vertex_source);
   GLuint frag  = hwfft_compile_shader(fft, GL_FRAGMENT_SHADER, fragment_source);

   glAttachShader(prog, vert);
   glAttachShader(prog, frag);
   glLinkProgram(prog);

   glGetProgramiv(prog, GL_LINK_STATUS, &status);

   if (!status)
   {
      char log_info[8 * 1024];
      GLsizei log_len;

      log_cb(RETRO_LOG_ERROR, "Failed to link.\n");
      glGetProgramInfoLog(prog, sizeof(log_info), &log_len, log_info);
      log_cb(RETRO_LOG_ERROR, "ERROR: %s\n", log_info);
   }

   glDeleteShader(vert);
   glDeleteShader(frag);
   return prog;
}

typedef float stub_matrix4x4[4][4];

static INLINE unsigned log2i(unsigned x)
{
   unsigned res;

   for (res = 0; x; x >>= 1)
      res++;
   return res - 1;
}

static INLINE unsigned bitinverse(unsigned x, unsigned size)
{
   unsigned i;
   unsigned size_log2 = (size == 0) ? 0 : log2i(size);
   unsigned ret       = 0;

   for (i = 0; i < size_log2; i++)
      ret |= ((x >> i) & 0x1) << (size_log2 - 1 - i);
   return ret;
}

static fft_complex_t hwfft_exp_imag(float phase)
{
   fft_complex_t out;
   out.real = cosf(phase);
   out.imag = sinf(phase);
   return out;
}

void hwfft_build_params(hwfft_t *fft, GLuint *buffer,
      unsigned step, unsigned size)
{
   unsigned i, j;
   unsigned step_size = 1 << step;

   for (i = 0; i < size; i += step_size << 1)
   {
      for (j = i; j < i + step_size; j++)
      {
         vec2_t tmp;
         int s                 = j - i;
         float phase           = -1.0f * (float)s / step_size;
         unsigned a            = j;
         unsigned b            = j + step_size;
         fft_complex_t twiddle = hwfft_exp_imag(M_PI * phase);

         unsigned read_a       = (step == 0) ? bitinverse(a, size) : a;
         unsigned read_b       = (step == 0) ? bitinverse(b, size) : b;

         tmp[0]                = twiddle.real;
         tmp[1]                = twiddle.imag;

         buffer[2 * a + 0]     = (read_a << 16) | read_b;
         buffer[2 * a + 1]     = vec2_packHalf2x16(tmp[0], tmp[1]);
         buffer[2 * b + 0]     = (read_a << 16) | read_b;
         buffer[2 * b + 1]     = vec2_packHalf2x16(-tmp[0], -tmp[1]);
      }
   }
}

static void hwfft_init_quad_vao(hwfft_t *fft)
{
   static const GLbyte quad_buffer[] = {
      -1, -1, 1, -1, -1, 1, 1, 1,
       0,  0, 1,  0,  0, 1, 1, 1,
   };
   glGenBuffers(1, &fft->quad);
   glBindBuffer(GL_ARRAY_BUFFER, fft->quad);
   glBufferData(GL_ARRAY_BUFFER,
         sizeof(quad_buffer), quad_buffer, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   glGenVertexArrays(1, &fft->vao);
   glBindVertexArray(fft->vao);
   glBindBuffer(GL_ARRAY_BUFFER, fft->quad);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glVertexAttribPointer(0, 2, GL_BYTE, GL_FALSE, 0, 0);
   glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, 0,
         (const GLvoid*)((uintptr_t)(8)));
   glBindVertexArray(0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void hwfft_init_texture(hwfft_t *fft, GLuint *tex, GLenum format,
      unsigned width, unsigned height, unsigned levels, GLenum mag, GLenum min)
{
   glGenTextures(1, tex);
   glBindTexture(GL_TEXTURE_2D, *tex);
   glTexStorage2D(GL_TEXTURE_2D, levels, format, width, height);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
   glBindTexture(GL_TEXTURE_2D, 0);
}

static void hwfft_init_target(hwfft_t *fft, struct target *target, GLenum format,
      unsigned width, unsigned height, unsigned levels, GLenum mag, GLenum min)
{
   hwfft_init_texture(fft, &target->tex, format, width, height, levels, mag, min);
   glGenFramebuffers(1, &target->fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, target->fbo);

   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      target->tex, 0);

   if (format == GL_RGBA8)
   {
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);
   }
   else if (format == GL_RG16I)
   {
      static const GLint v[] = { 0, 0, 0, 0 };
      glClearBufferiv(GL_COLOR, 0, v);
   }
   else
   {
      static const GLuint v[] = { 0, 0, 0, 0 };
      glClearBufferuiv(GL_COLOR, 0, v);
   }
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#define KAISER_BETA 12.0

static void hwfft_init(hwfft_t *fft)
{
   unsigned i;
   double window_mod;
   GLushort *window             = NULL;
   static const GLfloat unity[] = { 0.0f, 0.0f, 1.0f, 1.0f };

   fft->prog_real    = hwfft_compile_program(fft, fft_vertex_program, fft_fragment_program_real);
   fft->prog_complex = hwfft_compile_program(fft, fft_vertex_program, fft_fragment_program_complex);
   fft->prog_resolve = hwfft_compile_program(fft, fft_vertex_program, fft_fragment_program_resolve);
   fft->prog_blur    = hwfft_compile_program(fft, fft_vertex_program, fft_fragment_program_blur);
   GL_CHECK_ERROR();

   glUseProgram(fft->prog_real);
   glUniform1i(glGetUniformLocation(fft->prog_real, "sTexture"), 0);
   glUniform1i(glGetUniformLocation(fft->prog_real, "sParameterTexture"), 1);
   glUniform1i(glGetUniformLocation(fft->prog_real, "sWindow"), 2);
   glUniform4fv(glGetUniformLocation(fft->prog_real, "uOffsetScale"), 1, unity);

   glUseProgram(fft->prog_complex);
   glUniform1i(glGetUniformLocation(fft->prog_complex, "sTexture"), 0);
   glUniform1i(glGetUniformLocation(fft->prog_complex, "sParameterTexture"), 1);
   glUniform4fv(glGetUniformLocation(fft->prog_complex, "uOffsetScale"), 1, unity);

   glUseProgram(fft->prog_resolve);
   glUniform1i(glGetUniformLocation(fft->prog_resolve, "sFFT"), 0);
   glUniform4fv(glGetUniformLocation(fft->prog_resolve, "uOffsetScale"), 1, unity);

   glUseProgram(fft->prog_blur);
   glUniform1i(glGetUniformLocation(fft->prog_blur, "sHeight"), 0);
   glUniform4fv(glGetUniformLocation(fft->prog_blur, "uOffsetScale"), 1, unity);

   GL_CHECK_ERROR();

   hwfft_init_texture(fft, &fft->window_tex, GL_R16UI,
         fft->size, 1, 1, GL_NEAREST, GL_NEAREST);
   GL_CHECK_ERROR();

   window = (GLushort*)calloc(fft->size, sizeof(GLushort));

   window_mod = 1.0 / besseli0(KAISER_BETA);

   for (i = 0; i < fft->size; i++)
   {
      double phase = (double)(i - (int)(fft->size) / 2) / ((int)(fft->size) / 2);
      double     w = besseli0(KAISER_BETA * sqrtf(1 - phase * phase));
      window[i]    = round(0xffff * w * window_mod);
   }
   glBindTexture(GL_TEXTURE_2D, fft->window_tex);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
         fft->size, 1, GL_RED_INTEGER, GL_UNSIGNED_SHORT, &window[0]);
   glBindTexture(GL_TEXTURE_2D, 0);

   GL_CHECK_ERROR();
   hwfft_init_texture(fft, &fft->input_tex, GL_RG16I,
         fft->size, 1, 1, GL_NEAREST, GL_NEAREST);
   hwfft_init_target(fft, &fft->output, GL_RG32UI,
         fft->size, fft->depth, 1, GL_NEAREST, GL_NEAREST);
   hwfft_init_target(fft, &fft->resolve, GL_RGBA8,
         fft->size, fft->depth, 1, GL_NEAREST, GL_NEAREST);
   hwfft_init_target(fft, &fft->blur, GL_RGBA8,
         fft->size, fft->depth,
         log2i(MAX(fft->size, fft->depth)) + 1,
         GL_NEAREST, GL_LINEAR_MIPMAP_LINEAR);

   GL_CHECK_ERROR();

   for (i = 0; i < fft->steps; i++)
   {
      GLuint *param_buffer = NULL;
      hwfft_init_target(fft, &fft->passes[i].target,
            GL_RG32UI, fft->size, 1, 1, GL_NEAREST, GL_NEAREST);
      hwfft_init_texture(fft, &fft->passes[i].parameter_tex,
            GL_RG32UI, fft->size, 1, 1, GL_NEAREST, GL_NEAREST);

      param_buffer = (GLuint*)calloc(2 * fft->size, sizeof(GLuint));

      hwfft_build_params(fft, &param_buffer[0], i, fft->size);

      glBindTexture(GL_TEXTURE_2D, fft->passes[i].parameter_tex);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            fft->size, 1, GL_RG_INTEGER, GL_UNSIGNED_INT, &param_buffer[0]);
      glBindTexture(GL_TEXTURE_2D, 0);

      free(param_buffer);
   }

   GL_CHECK_ERROR();
   glGenBuffers(1, &fft->pbo);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, fft->pbo);
   glBufferData(GL_PIXEL_UNPACK_BUFFER,
         fft->size * 2 * sizeof(GLshort), 0, GL_DYNAMIC_DRAW);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

   free(window);
}

static void hwfft_init_block(hwfft_t *fft)
{
   unsigned x, y;
   unsigned block_vertices_size = 0;
   unsigned block_indices_size  = 0;
   int pos                      = 0;
   GLuint *bp                   = NULL;
   GLushort *block_vertices     = NULL;
   GLuint   *block_indices      = NULL;

   fft->block.prog              = hwfft_compile_program(fft,
         fft_vertex_program_heightmap, fft_fragment_program_heightmap);

   glUseProgram(fft->block.prog);
   glUniform1i(glGetUniformLocation(fft->block.prog, "sHeight"), 0);

   block_vertices_size = 2 * fft->block_size * fft->depth;
   block_vertices      = (GLushort*)calloc(block_vertices_size, sizeof(GLushort));

   for (y = 0; y < fft->depth; y++)
   {
      for (x = 0; x < fft->block_size; x++)
      {
         block_vertices[2 * (y * fft->block_size + x) + 0] = x;
         block_vertices[2 * (y * fft->block_size + x) + 1] = y;
      }
   }
   glGenBuffers(1, &fft->block.vbo);
   glBindBuffer(GL_ARRAY_BUFFER, fft->block.vbo);
   glBufferData(GL_ARRAY_BUFFER,
         block_vertices_size * sizeof(GLushort),
         &block_vertices[0], GL_STATIC_DRAW);

   fft->block.elems = (2 * fft->block_size - 1) * (fft->depth - 1) + 1;

   block_indices_size = fft->block.elems;
   block_indices = (GLuint*)calloc(block_indices_size, sizeof(GLuint));

   bp = &block_indices[0];

   for (y = 0; y < fft->depth - 1; y++)
   {
      int x;
      int step_odd  = (-(int)(fft->block_size)) + ((y & 1) ? -1 : 1);
      int step_even = fft->block_size;

      for (x = 0; x < 2 * (int)(fft->block_size) - 1; x++)
      {
         *bp++ = pos;
         pos += (x & 1) ? step_odd : step_even;
      }
   }
   *bp++ = pos;

   glGenVertexArrays(1, &fft->block.vao);
   glBindVertexArray(fft->block.vao);

   glGenBuffers(1, &fft->block.ibo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fft->block.ibo);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER,
         block_indices_size * sizeof(GLuint),
         &block_indices[0], GL_STATIC_DRAW);

   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_UNSIGNED_SHORT, GL_FALSE, 0, 0);
   glBindVertexArray(0);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

   free(block_vertices);
   free(block_indices);
}

static bool hwfft_context_reset(hwfft_t *fft, unsigned fft_steps,
      rglgen_proc_address_t proc, unsigned fft_depth)
{
   rglgen_resolve_symbols(proc);

   fft->steps       = fft_steps;
   fft->depth       = fft_depth;
   fft->size        = 1 << fft_steps;
   fft->block_size  = fft->size / 4 + 1;

   fft->passes_size = fft_steps;
   fft->passes      = (struct Pass*)calloc(fft->passes_size, sizeof(struct Pass));

   if (!fft->passes)
      return false;

   fft->sliding_size = 2 * fft->size;
   fft->sliding      = (GLshort*)calloc(fft->sliding_size, sizeof(GLshort));

   if (!fft->sliding)
      return false;

   GL_CHECK_ERROR();
   hwfft_init_quad_vao(fft);
   GL_CHECK_ERROR();
   hwfft_init(fft);
   GL_CHECK_ERROR();
   hwfft_init_block(fft);
   GL_CHECK_ERROR();

   return true;
}

/* GLFFT requires either GLES3 or
 * desktop GL with ES3_compat (supported by MESA on Linux) extension. */
static hwfft_t *hwfft_new(unsigned fft_steps, rglgen_proc_address_t proc)
{
   hwfft_t *fft    = NULL;
#ifdef HAVE_OPENGLES3
   const char *ver = (const char*)(glGetString(GL_VERSION));
   if (ver)
   {
      unsigned major, minor;
      if (sscanf(ver, "OpenGL ES %u.%u", &major, &minor) != 2 || major < 3)
         return NULL;
   }
   else
      return NULL;
#else
   const char *exts = (const char*)(glGetString(GL_EXTENSIONS));
   if (!exts || !strstr(exts, "ARB_ES3_compatibility"))
      return NULL;
#endif
   fft = (hwfft_t*)calloc(1, sizeof(*fft));

   if (!fft)
      return NULL;

   if (!hwfft_context_reset(fft, fft_steps, proc, 256))
      goto error;

   return fft;

error:
   if (fft)
      free(fft);
   return NULL;
}

static void hwfft_init_multisample(hwfft_t *fft, unsigned width, unsigned height, unsigned samples)
{
   if (fft->ms_rb_color)
      glDeleteRenderbuffers(1, &fft->ms_rb_color);
   fft->ms_rb_color = 0;
   if (fft->ms_rb_ds)
      glDeleteRenderbuffers(1, &fft->ms_rb_ds);
   fft->ms_rb_ds    = 0;
   if (fft->ms_fbo)
      glDeleteFramebuffers(1, &fft->ms_fbo);
   fft->ms_fbo      = 0;

   if (samples > 1)
   {
      glGenRenderbuffers(1, &fft->ms_rb_color);
      glGenRenderbuffers(1, &fft->ms_rb_ds);
      glGenFramebuffers (1, &fft->ms_fbo);

      glBindRenderbuffer(GL_RENDERBUFFER, fft->ms_rb_color);
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
            GL_RGBA8, width, height);
      glBindRenderbuffer(GL_RENDERBUFFER, fft->ms_rb_ds);
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
            GL_DEPTH24_STENCIL8, width, height);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);

      glBindFramebuffer(GL_FRAMEBUFFER, fft->ms_fbo);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_RENDERBUFFER, fft->ms_rb_color);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, fft->ms_rb_ds);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
         hwfft_init_multisample(fft, 0, 0, 0);
   }

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void hwfft_context_destroy(hwfft_t *fft)
{
   hwfft_init_multisample(fft, 0, 0, 0);
   if (fft->passes)
      free(fft->passes);
   fft->passes = NULL;
   if (fft->sliding)
      free(fft->sliding);
   fft->sliding = NULL;
}

static void hwfft_free(hwfft_t *fft)
{
   if (!fft)
      return;

   hwfft_context_destroy(fft);
   if (fft)
      free(fft);
   fft = NULL;
}

static void hwfft_step(hwfft_t *fft, const GLshort *audio_buffer, unsigned frames)
{
   unsigned i;
   GLfloat resolve_offset[4];
   GLshort *buffer = NULL;
   GLshort *slide  = (GLshort*)&fft->sliding[0];

   glEnable(GL_DEPTH_TEST);
   glEnable(GL_CULL_FACE);
   glBindVertexArray(fft->vao);

   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, fft->window_tex);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, fft->input_tex);
   glUseProgram(fft->prog_real);

   memmove(slide, slide + frames * 2,
         (fft->sliding_size - 2 * frames) * sizeof(GLshort));
   memcpy(slide + fft->sliding_size - frames * 2, audio_buffer,
         2 * frames * sizeof(GLshort));

   /* Upload audio data to GPU. */
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, fft->pbo);

   buffer = (GLshort*)(glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
            2 * fft->size * sizeof(GLshort),
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

   if (buffer)
   {
      memcpy(buffer, slide, fft->sliding_size * sizeof(GLshort));
      glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
   }
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fft->size, 1,
         GL_RG_INTEGER, GL_SHORT, NULL);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

   /* Perform FFT of new block. */
   glViewport(0, 0, fft->size, 1);

   for (i = 0; i < fft->steps; i++)
   {
      if (i == fft->steps - 1)
      {
         glBindFramebuffer(GL_FRAMEBUFFER, fft->output.fbo);
         glUniform1i(glGetUniformLocation(i == 0
                  ? fft->prog_real : fft->prog_complex, "uViewportOffset"),
               fft->output_ptr);
         glViewport(0, fft->output_ptr, fft->size, 1);
      }
      else
      {
         glUniform1i(glGetUniformLocation(i == 0
                  ? fft->prog_real : fft->prog_complex, "uViewportOffset"), 0);
         glBindFramebuffer(GL_FRAMEBUFFER, fft->passes[i].target.fbo);
         glClear(GL_COLOR_BUFFER_BIT);
      }

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, fft->passes[i].parameter_tex);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, fft->passes[i].target.tex);

      if (i == 0)
         glUseProgram(fft->prog_complex);
   }
   glActiveTexture(GL_TEXTURE0);

   /* Resolve new chunk to heightmap. */
   glViewport(0, fft->output_ptr, fft->size, 1);
   glUseProgram(fft->prog_resolve);
   glBindFramebuffer(GL_FRAMEBUFFER, fft->resolve.fbo);

   resolve_offset[0] = 0.0f;
   resolve_offset[1] = (float)(fft->output_ptr) / fft->depth;
   resolve_offset[2] = 1.0f;
   resolve_offset[3] = 1.0f / fft->depth;

   glUniform4fv(glGetUniformLocation(fft->prog_resolve, "uOffsetScale"),
         1, resolve_offset);
   glBindTexture(GL_TEXTURE_2D, fft->output.tex);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   /* Re-blur damaged regions of heightmap. */
   glUseProgram(fft->prog_blur);
   glBindTexture(GL_TEXTURE_2D, fft->resolve.tex);
   glBindFramebuffer(GL_FRAMEBUFFER, fft->blur.fbo);
   glUniform4fv(glGetUniformLocation(fft->prog_blur, "uOffsetScale"),
         1, resolve_offset);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   /* Mipmap the heightmap. */
   glBindTexture(GL_TEXTURE_2D, fft->blur.tex);
   glGenerateMipmap(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, 0);

   fft->output_ptr++;
   fft->output_ptr &= fft->depth - 1;

   glBindVertexArray(0);
   glUseProgram(0);
   GL_CHECK_ERROR();
}

static void hwfft_render(hwfft_t *fft, GLuint backbuffer, unsigned width, unsigned height)
{
   vec3_t eye, center, up;
   stub_matrix4x4 mvp_real;
   math_matrix_4x4 mvp_lookat, mvp, mvp_persp;

   eye[0]               = 0.0f;
   eye[1]               = 80.0f;
   eye[2]               = -60.0f;

   up[0]                = 0.0f;
   up[1]                = 1.0f;
   up[2]                = 0.0f;

   center[0]            = 0.0f;
   center[1]            = 0.0f;
   center[2]            = 1.0f;

   vec3_add(center, eye);

   matrix_4x4_projection(mvp_persp, (float)M_HALF_PI, (float)width / height, 1.0f, 500.0f);
   matrix_4x4_lookat(mvp_lookat, eye, center, up);
   matrix_4x4_multiply(mvp, mvp_lookat, mvp_persp);

   /* Render scene. */
   glBindFramebuffer(GL_FRAMEBUFFER, fft->ms_fbo ? fft->ms_fbo : backbuffer);
   glViewport(0, 0, width, height);
   glClearColor(0.1f, 0.15f, 0.1f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

   glUseProgram(fft->block.prog);

   mvp_real[0][0] = MAT_ELEM_4X4(mvp, 0, 0);
   mvp_real[0][1] = MAT_ELEM_4X4(mvp, 0, 1);
   mvp_real[0][2] = MAT_ELEM_4X4(mvp, 0, 2);
   mvp_real[0][3] = MAT_ELEM_4X4(mvp, 0, 3);
   mvp_real[1][0] = MAT_ELEM_4X4(mvp, 1, 0);
   mvp_real[1][1] = MAT_ELEM_4X4(mvp, 1, 1);
   mvp_real[1][2] = MAT_ELEM_4X4(mvp, 1, 2);
   mvp_real[1][3] = MAT_ELEM_4X4(mvp, 1, 3);
   mvp_real[2][0] = MAT_ELEM_4X4(mvp, 2, 0);
   mvp_real[2][1] = MAT_ELEM_4X4(mvp, 2, 1);
   mvp_real[2][2] = MAT_ELEM_4X4(mvp, 2, 2);
   mvp_real[2][3] = MAT_ELEM_4X4(mvp, 2, 3);
   mvp_real[3][0] = MAT_ELEM_4X4(mvp, 3, 0);
   mvp_real[3][1] = MAT_ELEM_4X4(mvp, 3, 1);
   mvp_real[3][2] = MAT_ELEM_4X4(mvp, 3, 2);
   mvp_real[3][3] = MAT_ELEM_4X4(mvp, 3, 3);

   glUniformMatrix4fv(glGetUniformLocation(fft->block.prog, "uMVP"),
         1, GL_FALSE, (&mvp_real[0][0]));

   glUniform2i(glGetUniformLocation(fft->block.prog, "uOffset"),
         ((-(int)(fft->block_size)) + 1) / 2, fft->output_ptr);
   glUniform4f(glGetUniformLocation(fft->block.prog, "uHeightmapParams"),
         -(fft->block_size - 1.0f) / 2.0f, 0.0f, 3.0f, 2.0f);
   glUniform1f(glGetUniformLocation(fft->block.prog, "uAngleScale"),
         M_PI / ((fft->block_size - 1) / 2));

   glBindVertexArray(fft->block.vao);
   glBindTexture(GL_TEXTURE_2D, fft->blur.tex);
   glDrawElements(GL_TRIANGLE_STRIP, fft->block.elems, GL_UNSIGNED_INT, NULL);
   glBindVertexArray(0);
   glUseProgram(0);

   if (fft->ms_fbo)
   {
      static const GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_STENCIL_ATTACHMENT };

      glBindFramebuffer(GL_READ_FRAMEBUFFER, fft->ms_fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backbuffer);
      glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);

      glBindFramebuffer(GL_FRAMEBUFFER, fft->ms_fbo);
      glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, attachments);
      GL_CHECK_ERROR();
   }

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   GL_CHECK_ERROR();
}

#endif /* HAVE_OPENGL || HAVE_OPENGLES3 */

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
   ATTACHMENTS_STR = (struct attachment*)av_realloc(
         ATTACHMENTS_STR, (ATTACHMENTS_SIZE_STR + 1) * sizeof(*ATTACHMENTS_STR));

   ATTACHMENTS_STR[ATTACHMENTS_SIZE_STR].data = (uint8_t*)av_malloc(len);
   ATTACHMENTS_STR[ATTACHMENTS_SIZE_STR].size = len;
   memcpy(ATTACHMENTS_STR[ATTACHMENTS_SIZE_STR].data, data, len);

   ATTACHMENTS_SIZE_STR++;
}

void CORE_PREFIX(retro_init)(void)
{
   RESET_TRIGGERED_STR = false;

#if FFMPEG3
   av_register_all();
#endif

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      LIBRETRO_SUPPORTS_BITMASKS_STR = true;
}

void CORE_PREFIX(retro_deinit)(void)
{
   if (VIDEO_BUFFER_STR)
   {
      video_buffer_destroy(VIDEO_BUFFER_STR);
      VIDEO_BUFFER_STR = NULL;
   }

   if (TPOOL_STR)
   {
      tpool_destroy(TPOOL_STR);
      TPOOL_STR = NULL;
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
   unsigned width  = VCTX_STR ? MEDIA_STR.width : 320;
   unsigned height = VCTX_STR ? MEDIA_STR.height : 240;
   float aspect    = VCTX_STR ? MEDIA_STR.aspect : 0.0;

   info->timing.fps = MEDIA_STR.interpolate_fps;
   info->timing.sample_rate = ACTX_STR[0] ? MEDIA_STR.sample_rate : 32000.0;

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   if (AUDIO_STREAMS_NUM_STR > 0 && VIDEO_STREAM_INDEX_STR < 0)
   {
      width = FFT_WIDTH_STR;
      height = FFT_HEIGHT_STR;
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
#endif
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
      { "ffmpeg_fft_resolution", "FFT Resolution; 1280x720|1920x1080|2560x1440|3840x2160|640x360|320x180" },
      { "ffmpeg_fft_multisample", "FFT Multisample; 1x|2x|4x" },
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
   RESET_TRIGGERED_STR = true;
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
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   struct retro_variable fft_var    = {0};
   struct retro_variable fft_ms_var = {0};
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   var.key = "ffmpeg_temporal_interp";

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (memcmp(var.value, "enabled", 7) == 0)
         TEMPORAL_INTERPOLATION_STR = true;
      else if (memcmp(var.value, "disabled", 8) == 0)
         TEMPORAL_INTERPOLATION_STR = false;
   }
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   fft_var.key = "ffmpeg_fft_resolution";

   FFT_WIDTH_STR       = 1280;
   FFT_HEIGHT_STR      = 720;
   FFT_MULTISAMPLE_STR = 1;
   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &fft_var) && fft_var.value)
   {
      unsigned w, h;
      if (sscanf(fft_var.value, "%ux%u", &w, &h) == 2)
      {
         FFT_WIDTH_STR = w;
         FFT_HEIGHT_STR = h;
      }
   }

   fft_ms_var.key = "ffmpeg_fft_multisample";

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &fft_ms_var) && fft_ms_var.value)
      FFT_MULTISAMPLE_STR = strtoul(fft_ms_var.value, NULL, 0);
#endif

   color_var.key = "ffmpeg_color_space";

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &color_var) && color_var.value)
   {
      slock_lock(DECODE_THREAD_LOCK_STR);
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
      slock_unlock(DECODE_THREAD_LOCK_STR);
   }

#if ENABLE_HW_ACCEL
   if (firststart)
   {
      hw_var.key = "ffmpeg_hw_decoder";

      FORCE_SW_DECODER_STR = false;
      HW_DECODER_STR = AV_HWDEVICE_TYPE_NONE;

      if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &hw_var) && hw_var.value)
      {
         if (memcmp(hw_var.value, "off", 3) == 0)
            FORCE_SW_DECODER_STR = true;
         else if (memcmp(hw_var.value, "cuda", 4) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_CUDA;
         else if (memcmp(hw_var.value, "d3d11va", 7) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_D3D11VA;
         else if (memcmp(hw_var.value, "drm", 3) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_DRM;
         else if (memcmp(hw_var.value, "dxva2", 5) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_DXVA2;
#if !FFMPEG3
         else if (memcmp(hw_var.value, "mediacodec", 10) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_MEDIACODEC;
         else if (memcmp(hw_var.value, "opencl", 6) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_OPENCL;
#endif
         else if (memcmp(hw_var.value, "qsv", 3) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_QSV;
         else if (memcmp(hw_var.value, "vaapi", 5) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_VAAPI;
         else if (memcmp(hw_var.value, "vdpau", 5) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_VDPAU;
         else if (memcmp(hw_var.value, "videotoolbox", 12) == 0)
            HW_DECODER_STR = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
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
            SW_DECODER_THREADS_STR = cpu_features_get_core_amount();
         }
         else
         {
            SW_DECODER_THREADS_STR = (unsigned)strtoul(sw_threads_var.value, NULL, 0);
         }
         /* Scale the sws threads based on core count but use at least 2 and at most 4 threads */
         SW_SWS_THREADS_STR = MIN(MAX(2, SW_DECODER_THREADS_STR / 2), 4);
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
   if ((seek_frames < 0 && (unsigned)-seek_frames > g_ctx.decoded_frame_cnt) || RESET_TRIGGERED_STR)
      g_ctx.decoded_frame_cnt = 0;
   /* Handle backwards seeking */
   else if (seek_frames < 0)
      g_ctx.decoded_frame_cnt += seek_frames;
   /* Handle forwards seeking */
   else
   {
      double current_time     = (double)g_ctx.decoded_frame_cnt / MEDIA_STR.interpolate_fps;
      double seek_step_time   = (double)seek_frames / MEDIA_STR.interpolate_fps;
      double seek_target_time = current_time + seek_step_time;
      double seek_time_max    = MEDIA_STR.duration.time - 1.0;

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
            seek_frames_capped = (int)(seek_step_time * MEDIA_STR.interpolate_fps);
      }

      if (seek_frames_capped < 0)
         g_ctx.decoded_frame_cnt  = 0;
      else
         g_ctx.decoded_frame_cnt += seek_frames_capped;
   }

   slock_lock(FIFO_LOCK_STR);
   DO_SEEK_STR        = true;
   SEEK_TIME_STR      = g_ctx.decoded_frame_cnt / MEDIA_STR.interpolate_fps;

   /* Convert seek time to a printable format */
   seek_seconds  = (unsigned)SEEK_TIME_STR;
   seek_minutes  = seek_seconds / 60;
   seek_seconds %= 60;
   seek_hours    = seek_minutes / 60;
   seek_minutes %= 60;

   snprintf(msg, sizeof(msg), "%02d:%02d:%02d / %02d:%02d:%02d",
         seek_hours, seek_minutes, seek_seconds,
         MEDIA_STR.duration.hours, MEDIA_STR.duration.minutes, MEDIA_STR.duration.seconds);

   /* Get current progress */
   if (MEDIA_STR.duration.time > 0.0)
   {
      seek_progress = (int8_t)((100.0 * SEEK_TIME_STR / MEDIA_STR.duration.time) + 0.5);
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
      FRAMES_STR[0].pts = 0.0;
      FRAMES_STR[1].pts = 0.0;
   }
   AUDIO_FRAMES_STR = g_ctx.decoded_frame_cnt * MEDIA_STR.sample_rate / MEDIA_STR.interpolate_fps;

   if (AUDIO_DECODE_FIFO_STR)
      fifo_clear(AUDIO_DECODE_FIFO_STR);
   scond_signal(FIFO_DECODE_COND_STR);

   while (!DECODE_THREAD_DEAD_STR && DO_SEEK_STR)
   {
      MAIN_SLEEPING_STR = true;
      scond_wait(FIFO_COND_STR, FIFO_LOCK_STR);
      MAIN_SLEEPING_STR = false;
   }

   slock_unlock(FIFO_LOCK_STR);
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
   double min_pts;
   int16_t audio_buffer[MEDIA_STR.sample_rate / 20];
   bool left, right, up, down, l1, l2, r1, r2;
   int16_t ret                  = 0;
   size_t to_read_frames        = 0;
   int seek_frames              = 0;
   bool updated                 = false;
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   unsigned old_fft_width       = FFT_WIDTH_STR;
   unsigned old_fft_height      = FFT_HEIGHT_STR;
   unsigned old_fft_multisample = FFT_MULTISAMPLE_STR;
#endif

   if (CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables(false);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   if (FFT_WIDTH_STR != old_fft_width || FFT_HEIGHT_STR != old_fft_height)
   {
      struct retro_system_av_info info;
      CORE_PREFIX(retro_get_system_av_info)(&info);
      if (!CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info))
      {
         FFT_WIDTH_STR = old_fft_width;
         FFT_HEIGHT_STR = old_fft_height;
      }
   }

   if (FFT_STR && (old_fft_multisample != FFT_MULTISAMPLE_STR))
      hwfft_init_multisample(FFT_STR, FFT_WIDTH_STR, FFT_HEIGHT_STR, FFT_MULTISAMPLE_STR);
#endif

   CORE_PREFIX(input_poll_cb)();

   if (LIBRETRO_SUPPORTS_BITMASKS_STR)
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

   if (l1 && !LAST_L1_STR)
   {
      seek_frames -= 30 * MEDIA_STR.interpolate_fps;
      SEEK_L2_STR = 0;
   }
   if (r1 && !LAST_R1_STR)
   {
      seek_frames += 30 * MEDIA_STR.interpolate_fps;
      SEEK_L2_STR = 0;
   }

   if (l2 && !LAST_L2_STR)
   {
      SEEK_L2_STR = seek_adjust(SEEK_L2_STR);
      seek_frames -= SEEK_L2_STR * MEDIA_STR.interpolate_fps;
   }

   if (r2 && !LAST_R2_STR)
   {
      SEEK_R2_STR = seek_adjust(SEEK_R2_STR);
      seek_frames += SEEK_R2_STR * MEDIA_STR.interpolate_fps;
   }

   if (((up && !LAST_UP_STR) || (down && !LAST_DOWN_STR)) && AUDIO_STREAMS_NUM_STR > 0)
   {
      char msg[256];
      struct retro_message_ext msg_obj = {0};
      int adjustment = (up) ? (+1) : ((down) ? (-1) : (0));

      SEEK_L2_STR = 0;

      msg[0] = '\0';

      slock_lock(DECODE_THREAD_LOCK_STR);
      g_ctx.audio_stream_idx = (((g_ctx.audio_stream_idx + adjustment) % AUDIO_STREAMS_NUM_STR) + AUDIO_STREAMS_NUM_STR) % AUDIO_STREAMS_NUM_STR;
      slock_unlock(DECODE_THREAD_LOCK_STR);

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

   if (((right && !LAST_RIGHT_STR) || (left && !LAST_LEFT_STR)) && SUBTITLE_STREAMS_NUM_STR > 0)
   {
      char msg[256];
      struct retro_message_ext msg_obj = {0};
      int adjustment = (right) ? (+1) : ((left) ? (-1) : (0));

      SEEK_L2_STR = 0;

      msg[0] = '\0';

      slock_lock(DECODE_THREAD_LOCK_STR);
      g_ctx.subtitle_stream_idx = (((g_ctx.subtitle_stream_idx + adjustment) % (SUBTITLE_STREAMS_NUM_STR + 1)) + (SUBTITLE_STREAMS_NUM_STR + 1)) % (SUBTITLE_STREAMS_NUM_STR + 1);
      slock_unlock(DECODE_THREAD_LOCK_STR);

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

   LAST_LEFT_STR  = left;
   LAST_RIGHT_STR = right;
   LAST_UP_STR    = up;
   LAST_DOWN_STR  = down;
   LAST_L1_STR     = l1;
   LAST_L2_STR     = l2;
   LAST_R1_STR     = r1;
   LAST_R2_STR     = r2;

   if (RESET_TRIGGERED_STR)
   {
      seek_frames = -1;
      seek_frame(seek_frames);
      RESET_TRIGGERED_STR = false;
   }

   /* Push seek request to thread,
    * wait for seek to complete. */
   if (seek_frames)
      seek_frame(seek_frames);

   if (DECODE_THREAD_DEAD_STR)
   {
      CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      return;
   }

   g_ctx.decoded_frame_cnt++;

   /* Have to decode audio before video
    * in case there are PTS discontinuities
    * due to seeking. */
   if (AUDIO_STREAMS_NUM_STR > 0)
   {
      /* Audio */
      double reading_pts;
      double expected_pts;
      double old_pts_bias;
      size_t to_read_bytes;
      uint64_t expected_audio_frames = g_ctx.decoded_frame_cnt * MEDIA_STR.sample_rate / MEDIA_STR.interpolate_fps;

      to_read_frames = expected_audio_frames - AUDIO_FRAMES_STR;
      to_read_bytes = to_read_frames * sizeof(int16_t) * 2;

      slock_lock(FIFO_LOCK_STR);
      while (!DECODE_THREAD_DEAD_STR && FIFO_READ_AVAIL(AUDIO_DECODE_FIFO_STR) < to_read_bytes)
      {
         MAIN_SLEEPING_STR = true;
         scond_signal(FIFO_DECODE_COND_STR);
         scond_wait(FIFO_COND_STR, FIFO_LOCK_STR);
         MAIN_SLEEPING_STR = false;
      }

      reading_pts  = DECODE_LAST_AUDIO_TIME_STR -
         (double)FIFO_READ_AVAIL(AUDIO_DECODE_FIFO_STR) / (MEDIA_STR.sample_rate * sizeof(int16_t) * 2);
      expected_pts = (double)AUDIO_FRAMES_STR / MEDIA_STR.sample_rate;
      old_pts_bias = PTS_BIAS_STR;
      PTS_BIAS_STR     = reading_pts - expected_pts;

      if (PTS_BIAS_STR < old_pts_bias - 1.0)
      {
         log_cb(RETRO_LOG_INFO, "[FFMPEG] Resetting PTS (bias).\n");
         FRAMES_STR[0].pts = 0.0;
         FRAMES_STR[1].pts = 0.0;
      }

      if (!DECODE_THREAD_DEAD_STR)
         fifo_read(AUDIO_DECODE_FIFO_STR, audio_buffer, to_read_bytes);
      scond_signal(FIFO_DECODE_COND_STR);

      slock_unlock(FIFO_LOCK_STR);
      AUDIO_FRAMES_STR += to_read_frames;
   }

   min_pts = g_ctx.decoded_frame_cnt / MEDIA_STR.interpolate_fps + PTS_BIAS_STR;

   if (VIDEO_STREAM_INDEX_STR >= 0)
   {
      bool dupe = true; /* unused if GL enabled */

      /* Video */
      if (min_pts > FRAMES_STR[1].pts)
      {
         struct frame tmp = FRAMES_STR[1];
         FRAMES_STR[1] = FRAMES_STR[0];
         FRAMES_STR[0] = tmp;
      }

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
      if (USE_GL_STR)
      {
         float mix_factor;

         while (!DECODE_THREAD_DEAD_STR && min_pts > FRAMES_STR[1].pts)
         {
            int64_t pts = 0;

            if (!DECODE_THREAD_DEAD_STR)
               video_buffer_wait_for_finished_slot(VIDEO_BUFFER_STR);

            if (!DECODE_THREAD_DEAD_STR)
            {
               unsigned y;
               int stride, width;
               const uint8_t *src           = NULL;
               video_decoder_context_t *ctx = NULL;
               uint32_t               *data = NULL;

               video_buffer_get_finished_slot(VIDEO_BUFFER_STR, &ctx);
               pts                          = ctx->pts;

#ifdef HAVE_SSA
               double video_time = ctx->pts * av_q2d(FCTX_STR->streams[VIDEO_STREAM_INDEX_STR]->time_base);
               slock_lock(ASS_LOCK_STR);
               if (ASS_RENDER_STR && ctx->ass_track_active)
               {
                  int change     = 0;
                  ASS_Image *img = ass_render_frame(ASS_RENDER_STR, ctx->ass_track_active,
                     1000 * video_time, &change);
                  render_ass_img(ctx->target, img);
               }
               slock_unlock(ASS_LOCK_STR);
#endif

#ifdef HAVE_OPENGLES
               data                         = VIDEO_FRAME_TEMP_BUFFER_STR;
#else
               glBindBuffer(GL_PIXEL_UNPACK_BUFFER, FRAMES_STR[1].pbo);
#ifdef __MACH__
               data                         = (uint32_t*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
#else
               data                         = (uint32_t*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER,
                     0, MEDIA_STR.width * MEDIA_STR.height * sizeof(uint32_t), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
#endif
#endif
               src                          = ctx->target->data[0];
               stride                       = ctx->target->linesize[0];
               width                        = MEDIA_STR.width * sizeof(uint32_t);
               for (y = 0; y < MEDIA_STR.height; y++, src += stride, data += width/4)
                  memcpy(data, src, width);

#ifndef HAVE_OPENGLES
               glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
#endif

               glBindTexture(GL_TEXTURE_2D, FRAMES_STR[1].tex);
#if defined(HAVE_OPENGLES)
               glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     MEDIA_STR.width, MEDIA_STR.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, VIDEO_FRAME_TEMP_BUFFER_STR);
#else
               glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     MEDIA_STR.width, MEDIA_STR.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
#endif
               glBindTexture(GL_TEXTURE_2D, 0);
#ifndef HAVE_OPENGLES
               glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif
               video_buffer_open_slot(VIDEO_BUFFER_STR, ctx);
            }

            FRAMES_STR[1].pts = av_q2d(FCTX_STR->streams[VIDEO_STREAM_INDEX_STR]->time_base) * pts;
         }

         mix_factor = (min_pts - FRAMES_STR[0].pts) / (FRAMES_STR[1].pts - FRAMES_STR[0].pts);

         if (!TEMPORAL_INTERPOLATION_STR)
            mix_factor = 1.0f;

         glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)HW_RENDER_STR.get_current_framebuffer());
         glClearColor(0, 0, 0, 1);
         glClear(GL_COLOR_BUFFER_BIT);
         glViewport(0, 0, MEDIA_STR.width, MEDIA_STR.height);
         glUseProgram(PROG_STR);

         glUniform1f(MIX_LOC_STR, mix_factor);
         glActiveTexture(GL_TEXTURE1);
         glBindTexture(GL_TEXTURE_2D, FRAMES_STR[1].tex);
         glActiveTexture(GL_TEXTURE0);
         glBindTexture(GL_TEXTURE_2D, FRAMES_STR[0].tex);

         glBindBuffer(GL_ARRAY_BUFFER, VBO_STR);
         glVertexAttribPointer(VERTEX_LOC_STR, 2, GL_FLOAT, GL_FALSE,
               4 * sizeof(GLfloat), (const GLvoid*)(0 * sizeof(GLfloat)));
         glVertexAttribPointer(TEX_LOC_STR, 2, GL_FLOAT, GL_FALSE,
               4 * sizeof(GLfloat), (const GLvoid*)(2 * sizeof(GLfloat)));
         glEnableVertexAttribArray(VERTEX_LOC_STR);
         glEnableVertexAttribArray(TEX_LOC_STR);
         glBindBuffer(GL_ARRAY_BUFFER, 0);

         glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
         glDisableVertexAttribArray(VERTEX_LOC_STR);
         glDisableVertexAttribArray(TEX_LOC_STR);

         glUseProgram(0);
         glActiveTexture(GL_TEXTURE1);
         glBindTexture(GL_TEXTURE_2D, 0);
         glActiveTexture(GL_TEXTURE0);
         glBindTexture(GL_TEXTURE_2D, 0);

         CORE_PREFIX(video_cb)(RETRO_HW_FRAME_BUFFER_VALID,
               MEDIA_STR.width, MEDIA_STR.height, MEDIA_STR.width * sizeof(uint32_t));
      }
      else
#endif
      {
         while (!DECODE_THREAD_DEAD_STR && min_pts > FRAMES_STR[1].pts)
         {
            int64_t pts = 0;

            if (!DECODE_THREAD_DEAD_STR)
               video_buffer_wait_for_finished_slot(VIDEO_BUFFER_STR);

            if (!DECODE_THREAD_DEAD_STR)
            {
               unsigned y;
               const uint8_t *src;
               int stride, width;
               uint32_t *data               = VIDEO_FRAME_TEMP_BUFFER_STR;
               video_decoder_context_t *ctx = NULL;

               video_buffer_get_finished_slot(VIDEO_BUFFER_STR, &ctx);
               pts                          = ctx->pts;
               src                          = ctx->target->data[0];
               stride                       = ctx->target->linesize[0];
               width                        = MEDIA_STR.width * sizeof(uint32_t);
               for (y = 0; y < MEDIA_STR.height; y++, src += stride, data += width/4)
                  memcpy(data, src, width);

               dupe                         = false;
               video_buffer_open_slot(VIDEO_BUFFER_STR, ctx);
            }

            FRAMES_STR[1].pts = av_q2d(FCTX_STR->streams[VIDEO_STREAM_INDEX_STR]->time_base) * pts;
         }

         CORE_PREFIX(video_cb)(dupe ? NULL : VIDEO_FRAME_TEMP_BUFFER_STR,
               MEDIA_STR.width, MEDIA_STR.height, MEDIA_STR.width * sizeof(uint32_t));
      }
   }
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   else if (FFT_STR)
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

         hwfft_step(FFT_STR, buffer, to_read);
         buffer += to_read * 2;
         fft_frames -= to_read;
      }
      hwfft_render(FFT_STR, HW_RENDER_STR.get_current_framebuffer(), FFT_WIDTH_STR, FFT_HEIGHT_STR);
      CORE_PREFIX(video_cb)(RETRO_HW_FRAME_BUFFER_VALID,
            FFT_WIDTH_STR, FFT_HEIGHT_STR, FFT_WIDTH_STR * sizeof(uint32_t));
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
   const AVCodec *codec = avcodec_find_decoder(FCTX_STR->streams[VIDEO_STREAM_INDEX_STR]->codecpar->codec_id);

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
   if (!FORCE_SW_DECODER_STR)
   {
      if (HW_DECODER_STR == AV_HWDEVICE_TYPE_NONE)
         format              = auto_hw_decoder(ctx, pix_fmts);
      else
         format              = init_hw_decoder(ctx, HW_DECODER_STR, pix_fmts);
   }

   /* Fallback to SW rendering */
   if (format == AV_PIX_FMT_NONE)
   {
#endif

      log_cb(RETRO_LOG_INFO, "[FFMPEG] Using SW decoding.\n");

      ctx->thread_type       = FF_THREAD_FRAME;
      ctx->thread_count      = SW_DECODER_THREADS_STR;
      log_cb(RETRO_LOG_INFO, "[FFMPEG] Configured software decoding threads: %d\n", SW_DECODER_THREADS_STR);

      format                 = (enum AVPixelFormat)FCTX_STR->streams[VIDEO_STREAM_INDEX_STR]->codecpar->format;

#if ENABLE_HW_ACCEL
      HW_DECODING_ENABLED_STR    = false;
   }
   else
      HW_DECODING_ENABLED_STR    = true;
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
   const AVCodec *codec = avcodec_find_decoder(FCTX_STR->streams[index]->codecpar->codec_id);
   if (!codec)
   {
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Couldn't find suitable decoder\n");
      return false;
   }

   *ctx = avcodec_alloc_context3(codec);
   avcodec_parameters_to_context((*ctx), FCTX_STR->streams[index]->codecpar);

   if (type == AVMEDIA_TYPE_VIDEO)
   {
      VIDEO_STREAM_INDEX_STR = index;

#if ENABLE_HW_ACCEL
      VCTX_STR->get_format  = get_format;
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
   DECODE_THREAD_LOCK_STR   = slock_new();

   VIDEO_STREAM_INDEX_STR   = -1;
   AUDIO_STREAMS_NUM_STR    = 0;
   SUBTITLE_STREAMS_NUM_STR = 0;

   slock_lock(DECODE_THREAD_LOCK_STR);
   g_ctx.audio_stream_idx    = 0;
   g_ctx.subtitle_stream_idx = 0;
   slock_unlock(DECODE_THREAD_LOCK_STR);

   memset(AUDIO_STREAMS_STR,    0, sizeof(AUDIO_STREAMS_STR));
   memset(SUBTITLE_STREAMS_STR, 0, sizeof(SUBTITLE_STREAMS_STR));

   for (i = 0; i < FCTX_STR->nb_streams; i++)
   {
      enum AVMediaType type = FCTX_STR->streams[i]->codecpar->codec_type;
      switch (type)
      {
         case AVMEDIA_TYPE_AUDIO:
            if (AUDIO_STREAMS_NUM_STR < MAX_STREAMS)
            {
               if (!open_codec(&ACTX_STR[AUDIO_STREAMS_NUM_STR], type, i))
                  return false;
               AUDIO_STREAMS_STR[AUDIO_STREAMS_NUM_STR] = i;
               AUDIO_STREAMS_NUM_STR++;
            }
            break;

         case AVMEDIA_TYPE_VIDEO:
            if (!VCTX_STR
                  && !codec_is_image(FCTX_STR->streams[i]->codecpar->codec_id))
            {
               if (!open_codec(&VCTX_STR, type, i))
                  return false;
            }
            break;

         case AVMEDIA_TYPE_SUBTITLE:
#ifdef HAVE_SSA
            if (SUBTITLE_STREAMS_NUM_STR < MAX_STREAMS
                  && codec_id_is_ass(FCTX_STR->streams[i]->codecpar->codec_id))
            {
               int size;
               AVCodecContext **s = &SCTX_STR[SUBTITLE_STREAMS_NUM_STR];

               SUBTITLE_STREAMS_STR[SUBTITLE_STREAMS_NUM_STR] = i;
               if (!open_codec(s, type, i))
                  return false;

               size = (*s)->extradata ? (*s)->extradata_size : 0;
               ASS_EXTRA_DATA_SIZE_STR[SUBTITLE_STREAMS_NUM_STR] = size;

               if (size)
               {
                  ASS_EXTRA_DATA_STR[SUBTITLE_STREAMS_NUM_STR] = (uint8_t*)av_malloc(size);
                  memcpy(ASS_EXTRA_DATA_STR[SUBTITLE_STREAMS_NUM_STR], (*s)->extradata, size);
               }

               SUBTITLE_STREAMS_NUM_STR++;
            }
#endif
            break;

         case AVMEDIA_TYPE_ATTACHMENT:
            {
               AVCodecParameters *params = FCTX_STR->streams[i]->codecpar;
               if (codec_id_is_ttf(params->codec_id))
                  append_attachment(params->extradata, params->extradata_size);
            }
            break;

         default:
            break;
      }
   }

   return ACTX_STR[0] || VCTX_STR;
}

static bool init_media_info(void)
{
   if (ACTX_STR[0])
      MEDIA_STR.sample_rate = ACTX_STR[0]->sample_rate;

   MEDIA_STR.interpolate_fps = 60.0;

   if (VCTX_STR)
   {
      MEDIA_STR.width  = VCTX_STR->width;
      MEDIA_STR.height = VCTX_STR->height;
      MEDIA_STR.aspect = (float)VCTX_STR->width *
         av_q2d(VCTX_STR->sample_aspect_ratio) / VCTX_STR->height;
   }

   if (FCTX_STR)
   {
      if (FCTX_STR->duration != AV_NOPTS_VALUE)
      {
         int64_t duration        = FCTX_STR->duration + (FCTX_STR->duration <= INT64_MAX - 5000 ? 5000 : 0);
         MEDIA_STR.duration.time     = (double)(duration / AV_TIME_BASE);
         MEDIA_STR.duration.seconds  = (unsigned)MEDIA_STR.duration.time;
         MEDIA_STR.duration.minutes  = MEDIA_STR.duration.seconds / 60;
         MEDIA_STR.duration.seconds %= 60;
         MEDIA_STR.duration.hours    = MEDIA_STR.duration.minutes / 60;
         MEDIA_STR.duration.minutes %= 60;
      }
      else
      {
         MEDIA_STR.duration.time    = 0.0;
         MEDIA_STR.duration.hours   = 0;
         MEDIA_STR.duration.minutes = 0;
         MEDIA_STR.duration.seconds = 0;
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Could not determine media duration.\n");
      }
   }

#ifdef HAVE_SSA
   if (SCTX_STR[0])
   {
      unsigned i;

      g_ctx.ass_lib = ass_library_init();
      ass_set_message_cb(g_ctx.ass_lib, ass_msg_cb, NULL);

      for (i = 0; i < ATTACHMENTS_SIZE_STR; i++)
         ass_add_font(g_ctx.ass_lib, (char*)"",
               (char*)ATTACHMENTS_STR[i].data, ATTACHMENTS_STR[i].size);

      ASS_RENDER_STR = ass_renderer_init(g_ctx.ass_lib);
      ass_set_frame_size(ASS_RENDER_STR, MEDIA_STR.width, MEDIA_STR.height);
      ass_set_extract_fonts(g_ctx.ass_lib, true);
      ass_set_fonts(ASS_RENDER_STR, NULL, NULL, 1, NULL, 1);
      ass_set_hinting(ASS_RENDER_STR, ASS_HINTING_LIGHT);

      for (i = 0; i < (unsigned)SUBTITLE_STREAMS_NUM_STR; i++)
      {
         ASS_TRACK_STR[i] = ass_new_track(g_ctx.ass_lib);
         ass_process_codec_private(ASS_TRACK_STR[i], (char*)ASS_EXTRA_DATA_STR[i],
               ASS_EXTRA_DATA_SIZE_STR[i]);
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
   if (HW_DECODING_ENABLED_STR)
      tmp_frame = ctx->hw_source;
   else
#endif
      tmp_frame = ctx->source;

   ctx->sws = sws_getCachedContext(ctx->sws,
         MEDIA_STR.width, MEDIA_STR.height, (enum AVPixelFormat)tmp_frame->format,
         MEDIA_STR.width, MEDIA_STR.height, AV_PIX_FMT_RGB32,
         SWS_POINT, NULL, NULL, NULL);

   set_colorspace(ctx->sws, MEDIA_STR.width, MEDIA_STR.height,
         tmp_frame->colorspace,
         tmp_frame->color_range);

   if ((ret = sws_scale(ctx->sws, (const uint8_t *const*)tmp_frame->data,
         tmp_frame->linesize, 0, MEDIA_STR.height,
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

   video_buffer_finish_slot(VIDEO_BUFFER_STR, ctx);
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
   while (!DECODE_THREAD_DEAD_STR && !video_buffer_has_open_slot(VIDEO_BUFFER_STR))
   {
      if (MAIN_SLEEPING_STR)
      {
         if (!DO_SEEK_STR)
            log_cb(RETRO_LOG_ERROR, "[FFMPEG] Thread: Video deadlock detected.\n");
         tpool_wait(TPOOL_STR);
         video_buffer_clear(VIDEO_BUFFER_STR);
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

   while (!DECODE_THREAD_DEAD_STR && video_buffer_has_open_slot(VIDEO_BUFFER_STR))
   {
      video_buffer_get_open_slot(VIDEO_BUFFER_STR, &decoder_ctx);

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
      if (HW_DECODING_ENABLED_STR)
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

      tpool_add_work(TPOOL_STR, sws_worker_thread, decoder_ctx);

   end:
      if (ret < 0)
      {
         video_buffer_return_open_slot(VIDEO_BUFFER_STR, decoder_ctx);
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
      slock_lock(FIFO_LOCK_STR);

      while (!DECODE_THREAD_DEAD_STR &&
            FIFO_WRITE_AVAIL(AUDIO_DECODE_FIFO_STR) < required_buffer)
      {
         if (!MAIN_SLEEPING_STR)
            scond_wait(FIFO_DECODE_COND_STR, FIFO_LOCK_STR);
         else
         {
            log_cb(RETRO_LOG_ERROR, "[FFMPEG] Thread: Audio deadlock detected.\n");
            fifo_clear(AUDIO_DECODE_FIFO_STR);
            break;
         }
      }

      DECODE_LAST_AUDIO_TIME_STR = pts * av_q2d(
            FCTX_STR->streams[AUDIO_STREAMS_STR[g_ctx.audio_stream_idx]]->time_base);

      if (!DECODE_THREAD_DEAD_STR)
         fifo_write(AUDIO_DECODE_FIFO_STR, buffer, required_buffer);

      scond_signal(FIFO_COND_STR);
      slock_unlock(FIFO_LOCK_STR);
   }

   return buffer;
}


static void decode_thread_seek(double time)
{
   int64_t seek_to = time * AV_TIME_BASE;

   if (seek_to < 0)
      seek_to = 0;

   DECODE_LAST_AUDIO_TIME_STR = time;

   if (avformat_seek_file(FCTX_STR, -1, INT64_MIN, seek_to, INT64_MAX, 0) < 0)
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] av_seek_frame() failed.\n");

   if (VIDEO_STREAM_INDEX_STR >= 0)
   {
      tpool_wait(TPOOL_STR);
      video_buffer_clear(VIDEO_BUFFER_STR);
   }

   if (ACTX_STR[g_ctx.audio_stream_idx])
      avcodec_flush_buffers(ACTX_STR[g_ctx.audio_stream_idx]);
   if (VCTX_STR)
      avcodec_flush_buffers(VCTX_STR);
   if (g_ctx.subtitle_stream_idx && SCTX_STR[g_ctx.subtitle_stream_idx - 1])
      avcodec_flush_buffers(SCTX_STR[g_ctx.subtitle_stream_idx - 1]);
#ifdef HAVE_SSA
   if (g_ctx.subtitle_stream_idx && ASS_TRACK_STR[g_ctx.subtitle_stream_idx - 1])
      ass_flush_events(ASS_TRACK_STR[g_ctx.subtitle_stream_idx - 1]);
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
   return (p1 <= p2 || (p1-p2) < (1.0 / MEDIA_STR.interpolate_fps) );
}

static void decode_thread(void *data)
{
   unsigned i;
   bool eof                = false;
   struct SwrContext *swr[(AUDIO_STREAMS_NUM_STR > 0) ? AUDIO_STREAMS_NUM_STR : 1];
   AVFrame *aud_frame      = NULL;
   size_t frame_size       = 0;
   int16_t *audio_buffer   = NULL;
   size_t audio_buffer_cap = 0;
   packet_buffer_t *audio_packet_buffer;
   packet_buffer_t *video_packet_buffer;
   double last_audio_end  = 0;

   (void)data;

   for (i = 0; (int)i < AUDIO_STREAMS_NUM_STR; i++)
   {
      swr[i] = swr_alloc();

#if HAVE_CH_LAYOUT
      AVChannelLayout out_chlayout = AV_CHANNEL_LAYOUT_STEREO;
      av_opt_set_chlayout(swr[i], "in_chlayout", &ACTX_STR[i]->ch_layout, 0);
      av_opt_set_chlayout(swr[i], "out_chlayout", &out_chlayout, 0);
#else
      av_opt_set_int(swr[i], "in_channel_layout", ACTX_STR[i]->channel_layout, 0);
      av_opt_set_int(swr[i], "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
#endif
      av_opt_set_int(swr[i], "in_sample_rate", ACTX_STR[i]->sample_rate, 0);
      av_opt_set_int(swr[i], "out_sample_rate", MEDIA_STR.sample_rate, 0);
      av_opt_set_int(swr[i], "in_sample_fmt", ACTX_STR[i]->sample_fmt, 0);
      av_opt_set_int(swr[i], "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
      swr_init(swr[i]);
   }

   aud_frame = av_frame_alloc();
   audio_packet_buffer = packet_buffer_create();
   video_packet_buffer = packet_buffer_create();

   if (VIDEO_STREAM_INDEX_STR >= 0)
   {
      frame_size = av_image_get_buffer_size(AV_PIX_FMT_RGB32, MEDIA_STR.width, MEDIA_STR.height, 1);
      VIDEO_BUFFER_STR = video_buffer_create(4, (int)frame_size, MEDIA_STR.width, MEDIA_STR.height);
      TPOOL_STR = tpool_create(SW_SWS_THREADS_STR);
      log_cb(RETRO_LOG_INFO, "[FFMPEG] Configured worker threads: %d\n", SW_SWS_THREADS_STR);
   }

   while (!DECODE_THREAD_DEAD_STR)
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

      slock_lock(FIFO_LOCK_STR);
      seek             = DO_SEEK_STR;
      seek_time_thread = SEEK_TIME_STR;
      slock_unlock(FIFO_LOCK_STR);

      if (seek)
      {
         decode_thread_seek(seek_time_thread);

         slock_lock(FIFO_LOCK_STR);
         DO_SEEK_STR          = false;
         eof              = false;
         SEEK_TIME_STR        = 0.0;
         next_video_end   = 0.0;
         next_audio_start = 0.0;
         last_audio_end   = 0.0;

         if (AUDIO_DECODE_FIFO_STR)
            fifo_clear(AUDIO_DECODE_FIFO_STR);

         packet_buffer_clear(&audio_packet_buffer);
         packet_buffer_clear(&video_packet_buffer);

         scond_signal(FIFO_COND_STR);
         slock_unlock(FIFO_LOCK_STR);
      }

      slock_lock(DECODE_THREAD_LOCK_STR);
      audio_stream_index          = AUDIO_STREAMS_STR[g_ctx.audio_stream_idx];
      audio_stream_ptr            = g_ctx.audio_stream_idx;
      subtitle_stream             = g_ctx.subtitle_stream_idx ? SUBTITLE_STREAMS_STR[g_ctx.subtitle_stream_idx - 1] : 0;
      actx_active                 = ACTX_STR[g_ctx.audio_stream_idx];
      sctx_active                 = g_ctx.subtitle_stream_idx ? SCTX_STR[g_ctx.subtitle_stream_idx - 1] : 0;
#ifdef HAVE_SSA
      ass_track_active            = g_ctx.subtitle_stream_idx ? ASS_TRACK_STR[g_ctx.subtitle_stream_idx - 1] : 0;
#endif
      audio_timebase = av_q2d(FCTX_STR->streams[audio_stream_index]->time_base);
      if (VIDEO_STREAM_INDEX_STR >= 0)
         video_timebase = av_q2d(FCTX_STR->streams[VIDEO_STREAM_INDEX_STR]->time_base);
      slock_unlock(DECODE_THREAD_LOCK_STR);

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
         decode_video(VCTX_STR, pkt, frame_size, ass_track_active);
         #else
         decode_video(VCTX_STR, pkt, frame_size);
         #endif

         av_packet_unref(pkt);
      }

      if (packet_buffer_empty(audio_packet_buffer) && packet_buffer_empty(video_packet_buffer) && eof)
      {
         av_packet_free(&pkt);
         break;
      }

      /* Read the next frame and stage it in case of audio or video frame. */
      if (av_read_frame(FCTX_STR, pkt) < 0)
         eof = true;
      else if (pkt->stream_index == audio_stream_index && actx_active)
         packet_buffer_add_packet(audio_packet_buffer, pkt);
      else if (pkt->stream_index == VIDEO_STREAM_INDEX_STR)
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
            slock_lock(ASS_LOCK_STR);
            ass_flush_events(ass_track_active);
            if (sub.rects[i]->ass && ass_track_active)
            {
               char dialogue_line[4096];
               
               /* Convert packet timing from stream timebase to milliseconds */
               double timebase_ms = av_q2d(FCTX_STR->streams[subtitle_stream]->time_base) * 1000.0;
               long long start_time = (long long)((pkt->pts >= 0 ? pkt->pts : 0) * timebase_ms) + sub.start_display_time;
               long long duration = (long long)(pkt->duration > 0 ? (pkt->duration * timebase_ms) : (sub.end_display_time - sub.start_display_time));
               
               snprintf(dialogue_line, sizeof(dialogue_line), "Dialogue: %s", sub.rects[i]->ass);
               
               ass_process_chunk(ass_track_active, dialogue_line, strlen(dialogue_line), start_time, duration);
            }
            slock_unlock(ASS_LOCK_STR);
         }
#endif
         avsubtitle_free(&sub);
         av_packet_unref(pkt);
      }
      av_packet_free(&pkt);
   }

   for (i = 0; (int)i < AUDIO_STREAMS_NUM_STR; i++)
      swr_free(&swr[i]);

#if ENABLE_HW_ACCEL
   if (VCTX_STR && VCTX_STR->hw_device_ctx)
      av_buffer_unref(&VCTX_STR->hw_device_ctx);
#endif

   packet_buffer_destroy(audio_packet_buffer);
   packet_buffer_destroy(video_packet_buffer);

   av_frame_free(&aud_frame);
   av_freep(&audio_buffer);

   slock_lock(FIFO_LOCK_STR);
   DECODE_THREAD_DEAD_STR = true;
   scond_signal(FIFO_COND_STR);
   slock_unlock(FIFO_LOCK_STR);
}

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
static void context_destroy(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   if (FFT_STR)
   {
      hwfft_free(FFT_STR);
      FFT_STR = NULL;
   }
#endif
}

static const char *vertex_source = GLSL(
      attribute vec2 aVertex;
      attribute vec2 aTexCoord;
      varying vec2 vTex;

      void main() {
         gl_Position = vec4(aVertex, 0.0, 1.0); vTex = aTexCoord;
      }
);

/* OpenGL ES note about main() -  Get format as GL_RGBA/GL_UNSIGNED_BYTE.
 * Assume little endian, so we get ARGB -> BGRA byte order, and
 * we have to swizzle to .BGR. */
#ifdef HAVE_OPENGLES
static const char *fragment_source = GLSL(
      varying vec2 vTex;
      uniform sampler2D sTex0;
      uniform sampler2D sTex1;
      uniform float uMix;

      void main() {
         gl_FragColor = vec4(pow(mix(pow(texture2D(sTex0, vTex).bgr, vec3(2.2)), pow(texture2D(sTex1, vTex).bgr, vec3(2.2)), uMix), vec3(1.0 / 2.2)), 1.0);
      }
);
#else
static const char *fragment_source = GLSL(
      varying vec2 vTex;
      uniform sampler2D sTex0;
      uniform sampler2D sTex1;
      uniform float uMix;

      void main() {
         gl_FragColor = vec4(pow(mix(pow(texture2D(sTex0, vTex).rgb, vec3(2.2)), pow(texture2D(sTex1, vTex).rgb, vec3(2.2)), uMix), vec3(1.0 / 2.2)), 1.0);
     }
);
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

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   if (AUDIO_STREAMS_NUM_STR > 0 && VIDEO_STREAM_INDEX_STR < 0)
   {
      FFT_STR = hwfft_new(11, HW_RENDER_STR.get_proc_address);
      if (FFT_STR)
         hwfft_init_multisample(FFT_STR, FFT_WIDTH_STR, FFT_HEIGHT_STR, FFT_MULTISAMPLE_STR);
   }

   /* Already inits symbols. */
   if (!FFT_STR)
#endif
      rglgen_resolve_symbols(HW_RENDER_STR.get_proc_address);

   PROG_STR = glCreateProgram();
   vert = glCreateShader(GL_VERTEX_SHADER);
   frag = glCreateShader(GL_FRAGMENT_SHADER);

   glShaderSource(vert, 1, &vertex_source, NULL);
   glShaderSource(frag, 1, &fragment_source, NULL);
   glCompileShader(vert);
   glCompileShader(frag);
   glAttachShader(PROG_STR, vert);
   glAttachShader(PROG_STR, frag);
   glLinkProgram(PROG_STR);

   glUseProgram(PROG_STR);

   glUniform1i(glGetUniformLocation(PROG_STR, "sTex0"), 0);
   glUniform1i(glGetUniformLocation(PROG_STR, "sTex1"), 1);
   VERTEX_LOC_STR = glGetAttribLocation(PROG_STR, "aVertex");
   TEX_LOC_STR    = glGetAttribLocation(PROG_STR, "aTexCoord");
   MIX_LOC_STR    = glGetUniformLocation(PROG_STR, "uMix");

   glUseProgram(0);

   for (i = 0; i < 2; i++)
   {
      glGenTextures(1, &FRAMES_STR[i].tex);

      glBindTexture(GL_TEXTURE_2D, FRAMES_STR[i].tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if !defined(HAVE_OPENGLES)
      glGenBuffers(1, &FRAMES_STR[i].pbo);
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, FRAMES_STR[i].pbo);
      glBufferData(GL_PIXEL_UNPACK_BUFFER, MEDIA_STR.width
            * MEDIA_STR.height * sizeof(uint32_t), NULL, GL_STREAM_DRAW);
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif
   }

   glGenBuffers(1, &VBO_STR);
   glBindBuffer(GL_ARRAY_BUFFER, VBO_STR);
   glBufferData(GL_ARRAY_BUFFER,
         sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindTexture(GL_TEXTURE_2D, 0);
}
#endif

void CORE_PREFIX(retro_unload_game)(void)
{
   unsigned i;

   if (DECODE_THREAD_HANDLE_STR)
   {
      slock_lock(FIFO_LOCK_STR);

      tpool_wait(TPOOL_STR);
      video_buffer_clear(VIDEO_BUFFER_STR);
      DECODE_THREAD_DEAD_STR = true;
      scond_signal(FIFO_DECODE_COND_STR);

      slock_unlock(FIFO_LOCK_STR);
      sthread_join(DECODE_THREAD_HANDLE_STR);
   }
   DECODE_THREAD_HANDLE_STR = NULL;

   if (FIFO_COND_STR)
      scond_free(FIFO_COND_STR);
   if (FIFO_DECODE_COND_STR)
      scond_free(FIFO_DECODE_COND_STR);
   if (FIFO_LOCK_STR)
      slock_free(FIFO_LOCK_STR);
   if (DECODE_THREAD_LOCK_STR)
      slock_free(DECODE_THREAD_LOCK_STR);
#ifdef HAVE_SSA
   if (ASS_LOCK_STR)
      slock_free(ASS_LOCK_STR);
#endif

   if (AUDIO_DECODE_FIFO_STR)
      fifo_free(AUDIO_DECODE_FIFO_STR);

   FIFO_COND_STR = NULL;
   FIFO_DECODE_COND_STR = NULL;
   FIFO_LOCK_STR = NULL;
   DECODE_THREAD_LOCK_STR = NULL;
   AUDIO_DECODE_FIFO_STR = NULL;
#ifdef HAVE_SSA
   ASS_LOCK_STR = NULL;
#endif

   DECODE_LAST_AUDIO_TIME_STR = 0.0;

   FRAMES_STR[0].pts = FRAMES_STR[1].pts = 0.0;
   PTS_BIAS_STR = 0.0;
   g_ctx.decoded_frame_cnt = 0;
   AUDIO_FRAMES_STR = 0;

   for (i = 0; i < MAX_STREAMS; i++)
   {
#if FFMPEG8
      if (SCTX_STR[i])
         avcodec_free_context(&SCTX_STR[i]);
      if (ACTX_STR[i])
         avcodec_free_context(&ACTX_STR[i]);
#else
      if (SCTX_STR[i])
         avcodec_close(SCTX_STR[i]);
      if (ACTX_STR[i])
         avcodec_close(ACTX_STR[i]);
#endif
      SCTX_STR[i] = NULL;
      ACTX_STR[i] = NULL;
   }

   if (VCTX_STR)
   {
#if FFMPEG8
      avcodec_free_context(&VCTX_STR);
#else
      avcodec_close(VCTX_STR);
#endif
      VCTX_STR = NULL;
   }

   if (FCTX_STR)
   {
      avformat_close_input(&FCTX_STR);
      FCTX_STR = NULL;
   }

   for (i = 0; i < ATTACHMENTS_SIZE_STR; i++)
      av_freep(&ATTACHMENTS_STR[i].data);
   av_freep(&ATTACHMENTS_STR);
   ATTACHMENTS_SIZE_STR = 0;

#ifdef HAVE_SSA
   for (i = 0; i < MAX_STREAMS; i++)
   {
      if (ASS_TRACK_STR[i])
         ass_free_track(ASS_TRACK_STR[i]);
      ASS_TRACK_STR[i] = NULL;

      av_freep(&ASS_EXTRA_DATA_STR[i]);
      ASS_EXTRA_DATA_SIZE_STR[i] = 0;
   }
   if (ASS_RENDER_STR)
      ass_renderer_done(ASS_RENDER_STR);
   if (g_ctx.ass_lib)
      ass_library_done(g_ctx.ass_lib);

   ASS_RENDER_STR = NULL;
   g_ctx.ass_lib = NULL;
#endif

   av_freep(&VIDEO_FRAME_TEMP_BUFFER_STR);
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

   if ((ret = avformat_open_input(&FCTX_STR, info->path, NULL, NULL)) < 0)
   {
#ifdef __cplusplus
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to open input: %d\n", ret);
#else
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to open input: %s\n", av_err2str(ret));
#endif
      goto error;
   }

   print_ffmpeg_version();

   if ((ret = avformat_find_stream_info(FCTX_STR, NULL)) < 0)
   {
#ifdef __cplusplus
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to find stream info: %d\n", ret);
#else
      log_cb(RETRO_LOG_ERROR, "[FFMPEG] Failed to find stream info: %s\n", av_err2str(ret));
#endif
      goto error;
   }

   log_cb(RETRO_LOG_INFO, "[FFMPEG] Media information:\n");
   av_dump_format(FCTX_STR, 0, info->path, 0);

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

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
   is_fft = VIDEO_STREAM_INDEX_STR < 0 && AUDIO_STREAMS_NUM_STR > 0;
#endif

   if (VIDEO_STREAM_INDEX_STR >= 0 || is_fft)
   {
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
      USE_GL_STR = true;
      HW_RENDER_STR.context_reset      = context_reset;
      HW_RENDER_STR.context_destroy    = context_destroy;
      HW_RENDER_STR.bottom_left_origin = is_fft;
      HW_RENDER_STR.depth              = is_fft;
      HW_RENDER_STR.stencil            = is_fft;
#if defined(HAVE_OPENGLES)
      HW_RENDER_STR.context_type = is_fft
         ? RETRO_HW_CONTEXT_OPENGLES3 : RETRO_HW_CONTEXT_OPENGLES2;
#else
      HW_RENDER_STR.context_type = RETRO_HW_CONTEXT_OPENGL;
#endif
      if (!CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_HW_RENDER, &HW_RENDER_STR))
      {
         USE_GL_STR = false;
         log_cb(RETRO_LOG_ERROR, "[FFMPEG] Cannot initialize HW render.\n");
      }
#endif
   }
   if (AUDIO_STREAMS_NUM_STR > 0)
   {
      /* audio fifo is 2 seconds deep */
      AUDIO_DECODE_FIFO_STR = fifo_new(
         MEDIA_STR.sample_rate * sizeof(int16_t) * 2 * 2
      );
   }

   FIFO_COND_STR        = scond_new();
   FIFO_DECODE_COND_STR = scond_new();
   FIFO_LOCK_STR        = slock_new();
#ifdef HAVE_SSA
   ASS_LOCK_STR         = slock_new();
#endif

   slock_lock(FIFO_LOCK_STR);
   DECODE_THREAD_DEAD_STR = false;
   slock_unlock(FIFO_LOCK_STR);

   DECODE_THREAD_HANDLE_STR = sthread_create(decode_thread, NULL);

   VIDEO_FRAME_TEMP_BUFFER_STR = (uint32_t*)
      av_malloc(MEDIA_STR.width * MEDIA_STR.height * sizeof(uint32_t));

   PTS_BIAS_STR = 0.0;

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

   slock_lock(DECODE_THREAD_LOCK_STR);
   info.frame_cnt = g_ctx.decoded_frame_cnt;
   info.audio_streams_ptr = g_ctx.audio_stream_idx;
   info.subtitle_streams_ptr = g_ctx.subtitle_stream_idx;
   slock_unlock(DECODE_THREAD_LOCK_STR);

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

      slock_lock(DECODE_THREAD_LOCK_STR);
      g_ctx.audio_stream_idx = info.audio_streams_ptr;
      g_ctx.subtitle_stream_idx = info.subtitle_streams_ptr;
      slock_unlock(DECODE_THREAD_LOCK_STR);

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
