/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_gl.h"
#include "record_pyrowave_session_internal.h"
#include <stdlib.h>
#include <string.h>
#include <glsym/glsym.h>
#include <formats/rpyrowave_gpu.h>
#include <retro_atomic.h>

#if defined(HAVE_OPENGLES)
#define rpy_gl_buffer_storage glBufferStorageEXT
#else
#define rpy_gl_buffer_storage glBufferStorage
#endif

/* Literal values also compile against older SDKs; eligibility is runtime. */
#define RPY_GL_STORAGE 0x90d2
#define RPY_GL_UNIFORM 0x8a11
#define RPY_GL_COPY_READ 0x8f36
#define RPY_GL_COPY_WRITE 0x8f37
#define RPY_GL_STATIC_COPY 0x88e6
#define RPY_GL_STATIC_DRAW 0x88e4
#define RPY_GL_STORAGE_BARRIER 0x2000
#define RPY_GL_UPDATE_BARRIER 0x200
#define RPY_GL_CLIENT_BARRIER 0x4000
#define RPY_GL_READ_PERSISTENT_COHERENT 0xc1
#define RPY_GL_CLIENT_STORAGE 0x200
#define RPY_GL_DECODING 1
#define RPY_GL_DECODE_READY 2
#define RPY_GL_DECODE_LENT 3
#define RPY_GL_DISPLAYING 4

#if defined(HAVE_OPENGLES)
#define RPY_GL_APIENTRY GL_APIENTRY
#else
#define RPY_GL_APIENTRY APIENTRY
#endif

typedef void (RPY_GL_APIENTRY *record_pyrowave_gl_compute_t)(GLuint, GLuint, GLuint);
typedef void (RPY_GL_APIENTRY *record_pyrowave_gl_barrier_t)(GLbitfield);
typedef void (RPY_GL_APIENTRY *record_pyrowave_gl_bind_image_t)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);

typedef struct record_pyrowave_gl_slot
{
   uint8_t *mapped;
   record_gpu_frame_t frame;
   rpyrowave_packet_parameters_t parameters;
   GLsync fence;
   uint64_t ticket;
   retro_atomic_int_t ready;
   GLuint readback;
   GLuint texture;
   unsigned state;
} record_pyrowave_gl_slot_t;

struct record_pyrowave_gl
{
   record_pyrowave_gl_slot_t slot[RECORD_GPU_QUEUE_SLOTS];
   record_gpu_queue_t *queue;
   record_pyrowave_gl_compute_t dispatch;
   record_pyrowave_gl_barrier_t barrier;
   record_pyrowave_gl_bind_image_t image;
   uint8_t *constants;
   size_t constant_stride;
   size_t dwt_offset;
   size_t packet_offset;
   uint64_t next_ticket;
   uint64_t retired;
   rpyrowave_gpu_layout_t layout;
   record_pyrowave_gl_config_t config;
   GLuint buffers[5]; /* Atlas pair, blocks, workspace, immutable constants. */
   GLuint dummy_image;
   GLsync retirement;
   unsigned cursor;
   unsigned untracked;
   unsigned reservation;
   unsigned owned;
   unsigned usability;
   bool failed;
};

static uint32_t record_pyrowave_gl_load32(const uint8_t *p);

static bool record_pyrowave_gl_capable(void)
{
   const char *version = (const char*)glGetString(GL_VERSION);
   GLint count = 0;
   GLint i;
   int major;
   int minor;
   if (!version) return false;
#if !defined(HAVE_OPENGLES)
   if (!glGetStringi) return false;
#endif
#if defined(HAVE_OPENGLES)
   if (strncmp(version, "OpenGL ES ", 10)) return false;
   version += 10;
#endif
   if (version[0] < '0' || version[0] > '9' || version[1] != '.'
         || version[2] < '0' || version[2] > '9') return false;
   major = version[0] - '0';
   minor = version[2] - '0';
#if defined(HAVE_OPENGLES)
   if (major < 3 || (major == 3 && minor < 1)) return false;
#else
   if (major < 4 || (major == 4 && minor < 3)) return false;
   if (major > 4 || minor >= 4) return true;
#endif
   glGetIntegerv(0x821d, &count);
   if (count < 0 || count > 65536) return false;
   for (i = 0; i < count; i++)
   {
      const char *extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
#if defined(HAVE_OPENGLES)
      if (extension && !strcmp(extension, "GL_EXT_buffer_storage")) return true;
#else
      if (extension && !strcmp(extension, "GL_ARB_buffer_storage")) return true;
#endif
   }
   return false;
}

static int record_pyrowave_gl_dwt_constants(void *userdata,
      const rpyrowave_dwt_parameters_t *p, unsigned source, unsigned x, unsigned y, unsigned z)
{
   record_pyrowave_gl_t *c = (record_pyrowave_gl_t*)userdata;
   (void)source;
   (void)x;
   (void)y;
   (void)z;
   memcpy(c->constants + c->dwt_offset + c->cursor++ * c->constant_stride, p, sizeof(*p));
   return 1;
}

static int record_pyrowave_gl_packet_constants(void *userdata,
      const rpyrowave_packet_parameters_t *p, unsigned x, unsigned y)
{
   record_pyrowave_gl_t *c = (record_pyrowave_gl_t*)userdata;
   (void)x;
   (void)y;
   c->usability = p->usability;
   if (c->config.decode)
   {
      rpyrowave_packet_parameters_t inverse;
      if (p->operation != RPYROWAVE_PACKET_PACK) return 1;
      inverse = *p;
      inverse.operation = RPYROWAVE_PACKET_UNPACK;
      memcpy(c->constants + c->packet_offset + p->sequence * c->constant_stride,
            &inverse, sizeof(inverse));
      return 1;
   }
   memcpy(c->constants + c->packet_offset
         + (p->sequence * 6 + p->operation) * c->constant_stride, p, sizeof(*p));
   return 1;
}

static int record_pyrowave_gl_dwt_dispatch(void *userdata,
      const rpyrowave_dwt_parameters_t *p, unsigned source, unsigned x, unsigned y, unsigned z)
{
   record_pyrowave_gl_t *c = (record_pyrowave_gl_t*)userdata;
   glBindBufferBase(RPY_GL_STORAGE, 0, c->buffers[source]);
   glBindBufferBase(RPY_GL_STORAGE, 1, c->buffers[source ^ 1]);
   glBindBufferRange(RPY_GL_UNIFORM, 2, c->buffers[4],
         (GLintptr)(c->dwt_offset + c->cursor++ * c->constant_stride), sizeof(*p));
   c->dispatch(x, y, z);
   c->barrier(RPY_GL_STORAGE_BARRIER);
   return 1;
}

static int record_pyrowave_gl_packet_dispatch(void *userdata,
      const rpyrowave_packet_parameters_t *p, unsigned x, unsigned y)
{
   record_pyrowave_gl_t *c = (record_pyrowave_gl_t*)userdata;
   glBindBufferRange(RPY_GL_UNIFORM, 4, c->buffers[4], (GLintptr)(c->packet_offset
            + (c->config.decode ? p->sequence : p->sequence * 6 + p->operation) * c->constant_stride), sizeof(*p));
   c->dispatch(x, y, 1);
   c->barrier(RPY_GL_STORAGE_BARRIER);
   return 1;
}

static void record_pyrowave_gl_destroy(record_pyrowave_gl_t *c, bool lost)
{
   unsigned i;
   if (!lost)
   {
      if (c->retirement) glDeleteSync(c->retirement);
      for (i = 0; i < c->config.slots; i++)
      {
         record_pyrowave_gl_slot_t *s = &c->slot[i];
         if (s->fence) glDeleteSync(s->fence);
         if (s->mapped)
         {
            glBindBuffer(RPY_GL_COPY_READ, s->readback);
            glUnmapBuffer(RPY_GL_COPY_READ);
         }
         if (s->readback) glDeleteBuffers(1, &s->readback);
         if (s->texture) glDeleteTextures(1, &s->texture);
      }
      if (c->dummy_image) glDeleteTextures(1, &c->dummy_image);
      glDeleteBuffers(5, c->buffers);
   }
   free(c->constants);
   free(c);
}

record_pyrowave_gl_t *record_pyrowave_gl_new(const record_pyrowave_gl_config_t *config)
{
   record_pyrowave_gl_t *c;
   rpyrowave_color_parameters_t color;
   rpyrowave_gpu_block_t *blocks = NULL;
   size_t atlas_bytes;
   size_t readback_bytes;
   size_t constant_bytes;
   GLint alignment;
   GLint storage_blocks;
   GLint64 storage_size;
   unsigned i;
   record_pyrowave_gl_proc_t entry;
   if (!config || !config->slots || config->slots > RECORD_GPU_QUEUE_SLOTS
         || config->texture_mode > (unsigned)(config->decode ? RPYROWAVE_COLOR_SDR709 : RPYROWAVE_COLOR_SCRGB)
         || !config->programs[0] || !config->programs[1] || !config->programs[2]
         || !config->get_proc || !rpy_gl_buffer_storage
#if !defined(HAVE_OPENGLES)
         || !glMapBufferRange || !glUnmapBuffer
         || !glFenceSync || !glClientWaitSync || !glDeleteSync || !glWaitSync
         || !glCopyBufferSubData || !glBindBufferRange || !glBindBufferBase || !glGetInteger64v
#endif
         || !record_pyrowave_gl_capable()
         || rpyrowave_gpu_color_parameters_ex(&config->sequence, config->bit_depth, config->decode ? 1 : 0,
            config->decode ? 0 : (config->flip_y ? 1 : 0),
            config->decode ? config->sequence.width : config->texture_width,
            config->decode ? config->sequence.height : config->texture_height,
            config->decode ? 0 : config->x, config->decode ? 0 : config->y, config->scale_source ? 1 : 0, &color))
      return NULL;
   c = (record_pyrowave_gl_t*)calloc(1, sizeof(*c));
   if (!c) return NULL;
   c->config = *config;
   color.texture_mode = config->texture_mode;
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   c->next_ticket = 1;
   entry = config->get_proc("glDispatchCompute");
   memcpy(&c->dispatch, &entry, sizeof(entry));
   entry = config->get_proc("glMemoryBarrier");
   memcpy(&c->barrier, &entry, sizeof(entry));
   entry = config->get_proc("glBindImageTexture");
   memcpy(&c->image, &entry, sizeof(entry));
   if (!c->dispatch || !c->barrier || !c->image) goto error;
   if (rpyrowave_gpu_layout(&config->sequence, config->maximum_bytes, &c->layout)) goto error;
   atlas_bytes = (size_t)c->layout.image_width * c->layout.image_height;
   if (atlas_bytes > (size_t)0x7fffffff / (3 * sizeof(float))) goto error;
   atlas_bytes *= 3 * sizeof(float);
   if (c->layout.workspace_bytes > 0x7fffffffU || config->maximum_bytes > 0x7fffffe0U) goto error;
   glGetIntegerv(0x90db, &storage_blocks);
   glGetInteger64v(0x90de, &storage_size);
   if (storage_blocks < 4 || storage_size <= 0 || (uint64_t)storage_size < atlas_bytes
         || (uint64_t)storage_size < c->layout.workspace_bytes
         || (uint64_t)storage_size < c->layout.block_count * sizeof(*blocks)) goto error;
   readback_bytes = config->decode ? c->layout.workspace_bytes
      : 16 + ((size_t)config->maximum_bytes + 3) / 4 * 4;
   glGetIntegerv(0x8a34, &alignment);
   if (alignment < 1 || alignment > 65536) goto error;
   c->constant_stride = (sizeof(color) + alignment - 1) / alignment * alignment;
   c->dwt_offset = c->constant_stride;
   c->packet_offset = c->dwt_offset + 50 * c->constant_stride;
   constant_bytes = c->packet_offset + (config->decode ? 8 : 48) * c->constant_stride;
   c->constants = (uint8_t*)calloc(1, constant_bytes);
   blocks = (rpyrowave_gpu_block_t*)malloc(c->layout.block_count * sizeof(*blocks));
   c->queue = record_gpu_queue_new(config->slots);
   if (!c->constants || !blocks || !c->queue) goto error;
   memcpy(c->constants, &color, sizeof(color));
   if (rpyrowave_gpu_blocks(&c->layout, blocks, c->layout.block_count)
         || rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height,
            config->sequence.chroma_444, config->decode ? 1 : 0, record_pyrowave_gl_dwt_constants, c)) goto error;
   for (i = 0; i < 8; i++)
   {
      rpyrowave_sequence_t sequence = config->sequence;
      sequence.sequence = i;
      if (rpyrowave_gpu_encode_record(&c->layout, &sequence, config->maximum_bytes,
               -1, record_pyrowave_gl_packet_constants, c)) goto error;
   }
   glGenBuffers(5, c->buffers);
   for (i = 0; i < 2; i++)
   {
      glBindBuffer(RPY_GL_STORAGE, c->buffers[i]);
      glBufferData(RPY_GL_STORAGE, (GLsizeiptr)atlas_bytes, NULL, RPY_GL_STATIC_COPY);
   }
   glBindBuffer(RPY_GL_STORAGE, c->buffers[2]);
   glBufferData(RPY_GL_STORAGE, c->layout.block_count * sizeof(*blocks), blocks, RPY_GL_STATIC_DRAW);
   if (!config->decode)
   {
      glBindBuffer(RPY_GL_STORAGE, c->buffers[3]);
      glBufferData(RPY_GL_STORAGE, c->layout.workspace_bytes, NULL, RPY_GL_STATIC_COPY);
   }
   glBindBuffer(RPY_GL_UNIFORM, c->buffers[4]);
   glBufferData(RPY_GL_UNIFORM, (GLsizeiptr)constant_bytes, c->constants, RPY_GL_STATIC_DRAW);
   for (i = 0; i < config->slots; i++)
   {
      record_pyrowave_gl_slot_t *s = &c->slot[i];
      GLbitfield flags = RPY_GL_READ_PERSISTENT_COHERENT | (config->decode ? 2 : 0);
      retro_atomic_int_init(&s->ready, 0);
      glGenBuffers(1, &s->readback);
      glBindBuffer(RPY_GL_COPY_READ, s->readback);
      rpy_gl_buffer_storage(RPY_GL_COPY_READ, (GLsizeiptr)readback_bytes, NULL,
            flags | RPY_GL_CLIENT_STORAGE);
      s->mapped = (uint8_t*)glMapBufferRange(RPY_GL_COPY_READ, 0,
            (GLsizeiptr)readback_bytes, flags);
      if (!s->mapped) goto error;
      if (config->decode)
      {
         glGenTextures(1, &s->texture);
         glBindTexture(GL_TEXTURE_2D, s->texture);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
         /* Frontend samplers may request mipmaps; this lend owns level zero. */
         glTexParameteri(GL_TEXTURE_2D, 0x813d, 0);
         glTexImage2D(GL_TEXTURE_2D, 0, 0x8814, config->sequence.width,
               config->sequence.height, 0, GL_RGBA, GL_FLOAT, NULL);
      }
   }
   glGenTextures(1, &c->dummy_image);
   glBindTexture(GL_TEXTURE_2D, c->dummy_image);
   glTexImage2D(GL_TEXTURE_2D, 0, 0x8814, 1, 1, 0, GL_RGBA, GL_FLOAT, NULL);
   if (glGetError() != GL_NO_ERROR) goto error;
   free(blocks);
   free(c->constants);
   c->constants = NULL;
   return c;
error:
   free(blocks);
   record_gpu_queue_free(c->queue);
   record_pyrowave_gl_destroy(c, false);
   return NULL;
}

int record_pyrowave_gl_push(record_pyrowave_gl_t *c, unsigned texture,
      void *ready_sync, uint64_t timestamp, uint64_t duration, uint64_t *ticket)
{
   record_gpu_frame_t frame;
   record_pyrowave_gl_slot_t *s;
   rpyrowave_sequence_t sequence;
   unsigned slot;
   int result;
   if (!c || c->config.decode || !texture || !ticket || c->failed || !c->next_ticket) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_acquire(c->queue, &slot);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   c->reservation = slot;
   s = &c->slot[slot];
   retro_atomic_store_release_int(&s->ready, 0);
   s->ticket = c->next_ticket++;
   *ticket = s->ticket;
   if (ready_sync) glWaitSync((GLsync)ready_sync, 0, (GLuint64)-1);
   glUseProgram(c->config.programs[0]);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, texture);
   c->image(3, c->dummy_image, 0, GL_FALSE, 0, 0x88b9, 0x8814);
   glBindBufferBase(RPY_GL_STORAGE, 1, c->buffers[1]);
   glBindBufferBase(RPY_GL_STORAGE, 2, c->buffers[0]);
   glBindBufferRange(RPY_GL_UNIFORM, 4, c->buffers[4], 0, sizeof(rpyrowave_color_parameters_t));
   c->dispatch((c->layout.image_width + 7) / 8, (c->layout.image_height + 7) / 8, 3);
   c->barrier(RPY_GL_STORAGE_BARRIER);
   glUseProgram(c->config.programs[1]);
   c->cursor = 0;
   rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height,
         c->layout.chroma_444, 0, record_pyrowave_gl_dwt_dispatch, c);
   glUseProgram(c->config.programs[2]);
   glBindBufferBase(RPY_GL_STORAGE, 0, c->buffers[0]);
   glBindBufferBase(RPY_GL_STORAGE, 1, c->buffers[2]);
   glBindBufferBase(RPY_GL_STORAGE, 2, c->buffers[3]);
   glBindBufferBase(RPY_GL_STORAGE, 3, c->buffers[1]);
   sequence = c->config.sequence;
   sequence.sequence = (unsigned)(s->ticket & 7);
   rpyrowave_gpu_encode_record(&c->layout, &sequence, c->config.maximum_bytes,
         -1, record_pyrowave_gl_packet_dispatch, c);
   c->barrier(RPY_GL_UPDATE_BARRIER);
   glBindBuffer(RPY_GL_COPY_READ, c->buffers[3]);
   glBindBuffer(RPY_GL_COPY_WRITE, s->readback);
   glCopyBufferSubData(RPY_GL_COPY_READ, RPY_GL_COPY_WRITE, 0, 0, 16);
   glCopyBufferSubData(RPY_GL_COPY_READ, RPY_GL_COPY_WRITE, c->layout.wire_offset,
         16, (GLsizeiptr)(((size_t)c->config.maximum_bytes + 3) / 4 * 4));
   c->barrier(RPY_GL_CLIENT_BARRIER);
   s->fence = glFenceSync(0x9117, 0);
   glFlush();
   if (!s->fence)
   {
      c->untracked = 1;
      c->failed = true;
      return RECORD_GPU_QUEUE_ERROR;
   }
   frame.timestamp = timestamp;
   frame.duration = duration;
   frame.ready_value = s->ticket;
   frame.slot = slot;
   frame.keyframe = true;
   if (!record_gpu_queue_submit(c->queue, &frame))
   {
      c->failed = true;
      return RECORD_GPU_QUEUE_ERROR;
   }
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_gl_poll(record_pyrowave_gl_t *c)
{
   unsigned i;
   bool valid = true;
   if (!c) return false;
   for (i = 0; i < c->config.slots; i++)
   {
      record_pyrowave_gl_slot_t *s = &c->slot[i];
      GLenum status;
      if (!s->fence || (c->config.decode && s->state != RPY_GL_DECODING
               && s->state != RPY_GL_DISPLAYING)) continue;
      /* Zero timeout is one readiness query, never a CPU wait. */
      status = glClientWaitSync(s->fence, 0, 0);
      if (status == 0x911d) c->failed = true;
      if (status != 0x911a && status != 0x911c) continue;
      if (c->config.decode)
      {
         if (s->state == RPY_GL_DECODING && !record_pyrowave_gl_load32(s->mapped + 12))
         {
            s->state = RPY_GL_DECODE_READY;
            continue;
         }
         else
         {
            if (s->state == RPY_GL_DECODING) valid = false;
            glDeleteSync(s->fence);
            s->fence = NULL;
            s->state = 0;
            record_gpu_queue_release(c->queue, i);
         }
         continue;
      }
      glDeleteSync(s->fence);
      s->fence = NULL;
      if (s->ticket > c->retired) c->retired = s->ticket;
      if (c->reservation == i)
      {
         record_gpu_queue_cancel(c->queue);
         c->reservation = RECORD_GPU_QUEUE_SLOTS;
      }
      else retro_atomic_store_release_int(&s->ready, 1);
   }
   return valid && !c->failed;
}

uint64_t record_pyrowave_gl_source_retired(const record_pyrowave_gl_t *c)
{
   return c ? c->retired : 0;
}

bool record_pyrowave_gl_idle(const record_pyrowave_gl_t *c)
{
   unsigned i;
   if (!c || c->untracked) return false;
   for (i = 0; i < c->config.slots; i++) if (c->slot[i].fence) return false;
   return true;
}

bool record_pyrowave_gl_finish(record_pyrowave_gl_t *c)
{
   return c && record_gpu_queue_finish(c->queue);
}

int record_pyrowave_gl_take(record_pyrowave_gl_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || c->config.decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK) c->owned |= 1U << frame->slot;
   return result;
}

static uint32_t record_pyrowave_gl_load32(const uint8_t *p)
{
   return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int record_pyrowave_gl_packet(record_pyrowave_gl_t *c, unsigned slot, record_pyrowave_packet_t *packet)
{
   const uint8_t *mapped;
   uint32_t bytes;
   if (!c || c->config.decode || !packet || slot >= c->config.slots || !(c->owned & (1U << slot)))
      return RECORD_GPU_QUEUE_ERROR;
   if (!retro_atomic_load_acquire_int(&c->slot[slot].ready)) return RECORD_GPU_QUEUE_BUSY;
   mapped = c->slot[slot].mapped;
   bytes = record_pyrowave_gl_load32(mapped + 4);
   if (record_pyrowave_gl_load32(mapped + 12) || bytes < 8 || bytes > c->config.maximum_bytes)
      return RECORD_GPU_QUEUE_ERROR;
   packet->data = mapped + 16;
   packet->bytes = bytes;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_gl_release(record_pyrowave_gl_t *c, unsigned slot)
{
   if (!c || c->config.decode || slot >= c->config.slots || !(c->owned & (1U << slot))
         || !retro_atomic_load_acquire_int(&c->slot[slot].ready)) return false;
   retro_atomic_store_release_int(&c->slot[slot].ready, 0);
   c->owned &= ~(1U << slot);
   return record_gpu_queue_release(c->queue, slot);
}

int record_pyrowave_gl_receive_acquire(record_pyrowave_gl_t *c, void **data, size_t *capacity)
{
   int result;
   if (data) *data = NULL;
   if (capacity) *capacity = 0;
   if (!c || !c->config.decode || !data || !capacity) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_acquire(c->queue, &c->reservation);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   *data = c->slot[c->reservation].mapped + c->layout.wire_offset;
   *capacity = c->config.maximum_bytes;
   return RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_gl_receive_submit(record_pyrowave_gl_t *c,
      size_t bytes, uint64_t timestamp, uint64_t duration)
{
   record_pyrowave_gl_slot_t *s;
   if (!c || !c->config.decode || c->reservation >= c->config.slots || !c->next_ticket
         || !duration || timestamp > (uint64_t)-1 - duration) return RECORD_GPU_QUEUE_ERROR;
   s = &c->slot[c->reservation];
   if (rpyrowave_gpu_index_frame(&c->layout, s->mapped, c->layout.workspace_bytes,
            bytes, 0, &s->parameters) || s->parameters.usability != c->usability)
      return RECORD_GPU_QUEUE_ERROR;
   s->frame.timestamp = timestamp;
   s->frame.duration = duration;
   s->frame.ready_value = c->next_ticket;
   s->frame.slot = c->reservation;
   s->frame.keyframe = true;
   if (!record_gpu_queue_submit(c->queue, &s->frame)) return RECORD_GPU_QUEUE_ERROR;
   c->next_ticket++;
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_gl_receive_cancel(record_pyrowave_gl_t *c)
{
   if (!c || !c->config.decode || !record_gpu_queue_cancel(c->queue)) return false;
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

int record_pyrowave_gl_receive_peek(record_pyrowave_gl_t *c, record_gpu_frame_t *frame)
{
   return c && c->config.decode ? record_gpu_queue_peek(c->queue, frame) : RECORD_GPU_QUEUE_ERROR;
}

int record_pyrowave_gl_receive_drop(record_pyrowave_gl_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || !c->config.decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK && !record_gpu_queue_release(c->queue, frame->slot))
      return RECORD_GPU_QUEUE_ERROR;
   return result;
}

int record_pyrowave_gl_decode(record_pyrowave_gl_t *c)
{
   record_gpu_frame_t frame;
   record_pyrowave_gl_slot_t *s;
   int result;
   if (!c || !c->config.decode || c->failed) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, &frame);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   s = &c->slot[frame.slot];
   s->state = RPY_GL_DECODING;
   c->barrier(RPY_GL_CLIENT_BARRIER);
   glUseProgram(c->config.programs[2]);
   glBindBufferBase(RPY_GL_STORAGE, 0, c->buffers[1]);
   glBindBufferBase(RPY_GL_STORAGE, 1, c->buffers[2]);
   glBindBufferBase(RPY_GL_STORAGE, 2, s->readback);
   glBindBufferBase(RPY_GL_STORAGE, 3, c->buffers[0]);
   rpyrowave_gpu_decode_record(&c->layout, &s->parameters, record_pyrowave_gl_packet_dispatch, c);
   glUseProgram(c->config.programs[1]);
   c->cursor = 0;
   rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height,
         c->layout.chroma_444, 1, record_pyrowave_gl_dwt_dispatch, c);
   glUseProgram(c->config.programs[0]);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, c->dummy_image);
   c->image(3, s->texture, 0, GL_FALSE, 0, 0x88b9, 0x8814);
   glBindBufferBase(RPY_GL_STORAGE, 1, c->buffers[0]);
   glBindBufferBase(RPY_GL_STORAGE, 2, c->buffers[1]);
   glBindBufferRange(RPY_GL_UNIFORM, 4, c->buffers[4], 0, sizeof(rpyrowave_color_parameters_t));
   c->dispatch((c->layout.width + 7) / 8, (c->layout.height + 7) / 8, 1);
   c->barrier(RPY_GL_CLIENT_BARRIER | 0x20 | 0x8);
   s->fence = glFenceSync(0x9117, 0);
   glFlush();
   if (!s->fence)
   {
      c->untracked = 1;
      c->failed = true;
      return RECORD_GPU_QUEUE_ERROR;
   }
   return RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_gl_image(record_pyrowave_gl_t *c, record_pyrowave_gl_image_t *image)
{
   unsigned i;
   unsigned selected = RECORD_GPU_QUEUE_SLOTS;
   if (!c || !c->config.decode || !image) return RECORD_GPU_QUEUE_ERROR;
   for (i = 0; i < c->config.slots; i++)
      if (c->slot[i].state == RPY_GL_DECODE_READY && (selected == RECORD_GPU_QUEUE_SLOTS
               || c->slot[i].frame.ready_value < c->slot[selected].frame.ready_value)) selected = i;
   if (selected == RECORD_GPU_QUEUE_SLOTS) return RECORD_GPU_QUEUE_BUSY;
   image->frame = c->slot[selected].frame;
   image->ready_sync = c->slot[selected].fence;
   image->texture = c->slot[selected].texture;
   c->slot[selected].state = RPY_GL_DECODE_LENT;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_gl_image_drop(record_pyrowave_gl_t *c, unsigned slot)
{
   if (!c || !c->config.decode || slot >= c->config.slots
         || c->slot[slot].state != RPY_GL_DECODE_LENT) return false;
   glDeleteSync(c->slot[slot].fence);
   c->slot[slot].fence = NULL;
   c->slot[slot].state = 0;
   return record_gpu_queue_release(c->queue, slot);
}

bool record_pyrowave_gl_present(record_pyrowave_gl_t *c, unsigned slot, void *display_sync)
{
   record_pyrowave_gl_slot_t *s;
   if (!c || !c->config.decode || c->failed || slot >= c->config.slots
         || c->slot[slot].state != RPY_GL_DECODE_LENT) return false;
   s = &c->slot[slot];
   if (display_sync) glWaitSync((GLsync)display_sync, 0, (GLuint64)-1);
   glDeleteSync(s->fence);
   s->state = RPY_GL_DISPLAYING;
   s->fence = glFenceSync(0x9117, 0);
   glFlush();
   if (!s->fence)
   {
      c->untracked = 1;
      c->failed = true;
      return false;
   }
   return true;
}

bool record_pyrowave_gl_free(record_pyrowave_gl_t *c)
{
   unsigned i;
   if (!c) return true;
   if (c->untracked) return false;
   for (i = 0; i < c->config.slots; i++) if (c->slot[i].fence) return false;
   if (!record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_gl_destroy(c, false);
   return true;
}

bool record_pyrowave_gl_quiesce(record_pyrowave_gl_t *c)
{
   GLenum status;
   if (!c) return true;
   if (!c->retirement)
   {
      /* Cold recovery only: order a marker after every encode/decode/draw,
       * including work whose original completion marker could not be made. */
      c->retirement = glFenceSync(0x9117, 0);
      if (!c->retirement) return false;
      glFlush();
   }
   status = glClientWaitSync(c->retirement, 0, 0);
   return status == 0x911a || status == 0x911c;
}

bool record_pyrowave_gl_retire(record_pyrowave_gl_t *c)
{
   record_gpu_frame_t frame;
   unsigned i;
   if (!c) return true;
   if (!record_pyrowave_gl_quiesce(c)) return false;
   if (c->reservation != RECORD_GPU_QUEUE_SLOTS) record_gpu_queue_cancel(c->queue);
   for (i = 0; i < c->config.slots; i++)
      if (record_gpu_queue_take(c->queue, &frame) != RECORD_GPU_QUEUE_OK) break;
   for (i = 0; i < c->config.slots; i++) record_gpu_queue_release(c->queue, i);
   if (!record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_gl_destroy(c, false);
   return true;
}

bool record_pyrowave_gl_abandon(record_pyrowave_gl_t *c)
{
   record_gpu_frame_t frame;
   unsigned i;
   if (!c) return true;
   if (c->reservation != RECORD_GPU_QUEUE_SLOTS) record_gpu_queue_cancel(c->queue);
   for (i = 0; i < c->config.slots; i++)
      if (record_gpu_queue_take(c->queue, &frame) != RECORD_GPU_QUEUE_OK) break;
   for (i = 0; i < c->config.slots; i++) record_gpu_queue_release(c->queue, i);
   if (!record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_gl_destroy(c, true);
   return true;
}

RECORD_PYROWAVE_SESSION_BINDING(record_pyrowave_gl, record_pyrowave_gl_t, record_pyrowave_gl_receive_acquire)
