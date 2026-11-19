/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_metal_internal.h"
#include "record_pyrowave_session_internal.h"
#include <formats/rpyrowave_gpu.h>
#include <retro_atomic.h>
#include <stdlib.h>
#include <string.h>

#define RPY_METAL_STRIDE 256
#define RPY_METAL_CONSTANT_SLOTS 150
#define RPY_METAL_DWT_SLOT 2
#define RPY_METAL_PACKET_SLOT 102

enum record_pyrowave_metal_state
{
   RPY_METAL_IDLE,
   RPY_METAL_ENCODING,
   RPY_METAL_DECODING,
   RPY_METAL_READY,
   RPY_METAL_LENT,
   RPY_METAL_PRESENTING
};

typedef struct record_pyrowave_metal_slot
{
   void *buffer;
   void *texture;
   void *commands;
   uint8_t *mapped;
   record_gpu_frame_t frame;
   rpyrowave_packet_parameters_t parameters;
   retro_atomic_int_t ready;
   unsigned state;
   bool invalid;
   bool source_pending;
} record_pyrowave_metal_slot_t;

struct record_pyrowave_metal
{
   record_pyrowave_metal_api_t api;
   record_pyrowave_metal_config_t config;
   rpyrowave_gpu_layout_t layout;
   record_pyrowave_metal_slot_t slot[RECORD_GPU_QUEUE_SLOTS];
   record_gpu_queue_t *queue;
   void *pipeline[3];
   void *atlas[2];
   void *blocks;
   void *workspace;
   void *constants;
   void *dummy;
   void *encoder;
   uint8_t *constant_data;
   uint64_t next_ticket;
   uint64_t retired;
   unsigned cursor;
   unsigned reservation;
   uint32_t owned;
   uint32_t usability;
   bool owns_api;
   bool failed;
};

static void record_pyrowave_metal_destroy(record_pyrowave_metal_t *c)
{
   record_pyrowave_metal_api_t *a = &c->api;
   void *pool = a->pool_push(a->user);
   unsigned i;
   for (i = 0; i < c->config.slots; i++)
   {
      a->release(a->user, c->slot[i].commands);
      a->release(a->user, c->slot[i].buffer);
      a->release(a->user, c->slot[i].texture);
   }
   for (i = 0; i < 3; i++) a->release(a->user, c->pipeline[i]);
   for (i = 0; i < 2; i++) a->release(a->user, c->atlas[i]);
   a->release(a->user, c->blocks);
   a->release(a->user, c->workspace);
   a->release(a->user, c->constants);
   a->release(a->user, c->dummy);
   a->release(a->user, c->config.device);
   a->release(a->user, c->config.queue);
   a->pool_pop(a->user, pool);
   if (c->owns_api && a->destroy) a->destroy(a->user);
   free(c);
}

static int record_pyrowave_metal_dwt_constants(void *user,
      const rpyrowave_dwt_parameters_t *p, unsigned source, unsigned x, unsigned y, unsigned z)
{
   record_pyrowave_metal_t *c = (record_pyrowave_metal_t*)user;
   unsigned slot = RPY_METAL_DWT_SLOT + p->inverse * 50 + c->cursor++;
   (void)source;
   (void)x;
   (void)y;
   (void)z;
   memcpy(c->constant_data + slot * RPY_METAL_STRIDE, p, sizeof(*p));
   return 1;
}

static int record_pyrowave_metal_packet_constants(void *user,
      const rpyrowave_packet_parameters_t *p, unsigned x, unsigned y)
{
   record_pyrowave_metal_t *c = (record_pyrowave_metal_t*)user;
   unsigned slot = RPY_METAL_PACKET_SLOT + p->sequence * 6;
   (void)x;
   (void)y;
   memcpy(c->constant_data + (slot + p->operation) * RPY_METAL_STRIDE, p, sizeof(*p));
   if (p->operation == RPYROWAVE_PACKET_INITIALIZE)
   {
      rpyrowave_packet_parameters_t decode = *p;
      c->usability = p->usability;
      decode.operation = RPYROWAVE_PACKET_UNPACK;
      memcpy(c->constant_data + (slot + decode.operation) * RPY_METAL_STRIDE, &decode, sizeof(decode));
   }
   return 1;
}

static int record_pyrowave_metal_dwt_dispatch(void *user,
      const rpyrowave_dwt_parameters_t *p, unsigned source, unsigned x, unsigned y, unsigned z)
{
   record_pyrowave_metal_t *c = (record_pyrowave_metal_t*)user;
   record_pyrowave_metal_api_t *a = &c->api;
   unsigned slot = RPY_METAL_DWT_SLOT + p->inverse * 50 + c->cursor++;
   a->compute_buffer(a->user, c->encoder, c->atlas[source], 0, 0);
   a->compute_buffer(a->user, c->encoder, c->atlas[source ^ 1], 0, 1);
   a->compute_buffer(a->user, c->encoder, c->constants, slot * RPY_METAL_STRIDE, 2);
   a->compute_dispatch(a->user, c->encoder, x, y, z, 8, 8);
   a->compute_barrier(a->user, c->encoder, false);
   return 1;
}

static int record_pyrowave_metal_packet_dispatch(void *user,
      const rpyrowave_packet_parameters_t *p, unsigned x, unsigned y)
{
   record_pyrowave_metal_t *c = (record_pyrowave_metal_t*)user;
   record_pyrowave_metal_api_t *a = &c->api;
   unsigned slot = RPY_METAL_PACKET_SLOT + p->sequence * 6 + p->operation;
   a->compute_buffer(a->user, c->encoder, c->constants, slot * RPY_METAL_STRIDE, 4);
   a->compute_dispatch(a->user, c->encoder, x, y, 1, 128, 1);
   a->compute_barrier(a->user, c->encoder, false);
   return 1;
}

static void record_pyrowave_metal_color(record_pyrowave_metal_t *c, void *source, void *destination, unsigned inverse)
{
   record_pyrowave_metal_api_t *a = &c->api;
   a->compute_pipeline(a->user, c->encoder, c->pipeline[0]);
   a->compute_buffer(a->user, c->encoder, c->atlas[inverse ? 0 : 1], 0, 1);
   a->compute_buffer(a->user, c->encoder, c->atlas[inverse ? 1 : 0], 0, 2);
   a->compute_buffer(a->user, c->encoder, c->constants, inverse * RPY_METAL_STRIDE, 4);
   a->compute_texture(a->user, c->encoder, source, 0);
   a->compute_texture(a->user, c->encoder, destination, 3);
   a->compute_dispatch(a->user, c->encoder,
         ((inverse ? c->layout.width : c->layout.image_width) + 7) / 8,
         ((inverse ? c->layout.height : c->layout.image_height) + 7) / 8, inverse ? 1 : 3, 8, 8);
   a->compute_barrier(a->user, c->encoder, inverse != 0);
}

static void record_pyrowave_metal_packet_bind(record_pyrowave_metal_t *c, void *workspace, unsigned inverse)
{
   record_pyrowave_metal_api_t *a = &c->api;
   a->compute_pipeline(a->user, c->encoder, c->pipeline[2]);
   a->compute_buffer(a->user, c->encoder, c->atlas[inverse ? 1 : 0], 0, 0);
   a->compute_buffer(a->user, c->encoder, c->blocks, 0, 1);
   a->compute_buffer(a->user, c->encoder, workspace, 0, 2);
   a->compute_buffer(a->user, c->encoder, c->atlas[inverse ? 0 : 1], 0, 3);
}

static bool record_pyrowave_metal_api_valid(const record_pyrowave_metal_api_t *a)
{
   return a && a->retain && a->release && a->buffer_limit && a->buffer_new && a->buffer_data
      && a->buffer_flush && a->texture_new && a->pipeline_new && a->source_valid
      && a->commands_valid && a->status && a->compute_begin && a->compute_pipeline
      && a->compute_buffer && a->compute_texture && a->compute_dispatch && a->compute_barrier
      && a->end && a->blit_begin && a->blit_copy && a->synchronize && a->pool_push && a->pool_pop;
}

record_pyrowave_metal_t *record_pyrowave_metal_new_with_api(
      const record_pyrowave_metal_config_t *config, const record_pyrowave_metal_api_t *api)
{
   record_pyrowave_metal_t *c;
   rpyrowave_color_parameters_t color;
   rpyrowave_sequence_t sequence;
   void *pool;
   size_t atlas_bytes;
   size_t staging_bytes;
   size_t block_bytes;
   size_t limit;
   unsigned i;
   if (!config || !record_pyrowave_metal_api_valid(api) || !config->device || !config->queue
         || config->texture_mode > (unsigned)(config->decode ? RPYROWAVE_COLOR_SDR709 : RPYROWAVE_COLOR_SCRGB)
         || !config->slots || config->slots > RECORD_GPU_QUEUE_SLOTS
         || !config->shaders[0] || !config->shaders[1] || !config->shaders[2]) return NULL;
   c = (record_pyrowave_metal_t*)calloc(1, sizeof(*c));
   if (!c) return NULL;
   c->api = *api;
   c->config = *config;
   c->config.device = api->retain(api->user, config->device);
   c->config.queue = api->retain(api->user, config->queue);
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   c->next_ticket = 1;
   pool = api->pool_push(api->user);
   if (!c->config.device || !c->config.queue
         || rpyrowave_gpu_layout(&config->sequence, config->maximum_bytes, &c->layout)) goto error;
   if (rpyrowave_gpu_color_parameters(&config->sequence, config->bit_depth, 0, config->flip_y ? 1 : 0,
            config->decode ? config->sequence.width : config->texture_width,
            config->decode ? config->sequence.height : config->texture_height,
            config->decode ? 0 : config->x, config->decode ? 0 : config->y, &color)) goto error;
   color.texture_mode = config->texture_mode;
   limit = api->buffer_limit(api->user, config->device);
   atlas_bytes = (size_t)c->layout.image_width * c->layout.image_height;
   if (atlas_bytes > (size_t)-1 / (3 * sizeof(float))) goto error;
   atlas_bytes *= 3 * sizeof(float);
   block_bytes = (size_t)c->layout.block_count * sizeof(rpyrowave_gpu_block_t);
   staging_bytes = config->decode ? c->layout.workspace_bytes
      : 16 + ((size_t)config->maximum_bytes + 3) / 4 * 4;
   if (limit < atlas_bytes || limit < c->layout.workspace_bytes || limit < block_bytes
         || limit < staging_bytes || limit < RPY_METAL_CONSTANT_SLOTS * RPY_METAL_STRIDE) goto error;
   c->queue = record_gpu_queue_new(config->slots);
   c->blocks = api->buffer_new(api->user, config->device, block_bytes, true);
   c->constants = api->buffer_new(api->user, config->device,
         RPY_METAL_CONSTANT_SLOTS * RPY_METAL_STRIDE, true);
   if (!c->queue || !c->blocks || !c->constants) goto error;
   c->constant_data = (uint8_t*)api->buffer_data(api->user, c->constants);
   if (!c->constant_data) goto error;
   memset(c->constant_data, 0, RPY_METAL_CONSTANT_SLOTS * RPY_METAL_STRIDE);
   memcpy(c->constant_data, &color, sizeof(color));
   color.inverse = 1;
   memcpy(c->constant_data + RPY_METAL_STRIDE, &color, sizeof(color));
   {
      rpyrowave_gpu_block_t *blocks = (rpyrowave_gpu_block_t*)api->buffer_data(api->user, c->blocks);
      if (!blocks || rpyrowave_gpu_blocks(&c->layout, blocks, c->layout.block_count)) goto error;
   }
   api->buffer_flush(api->user, c->blocks, block_bytes);
   for (i = 0; i < 2; i++)
   {
      c->cursor = 0;
      if (rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height,
               c->layout.chroma_444, i, record_pyrowave_metal_dwt_constants, c)) goto error;
      c->atlas[i] = api->buffer_new(api->user, config->device, atlas_bytes, false);
      if (!c->atlas[i]) goto error;
   }
   sequence = config->sequence;
   c->usability = 0;
   for (i = 0; i < 8; i++)
   {
      sequence.sequence = i;
      if (rpyrowave_gpu_encode_record(&c->layout, &sequence, config->maximum_bytes,
               -1, record_pyrowave_metal_packet_constants, c)) goto error;
   }
   api->buffer_flush(api->user, c->constants, RPY_METAL_CONSTANT_SLOTS * RPY_METAL_STRIDE);
   c->constant_data = NULL;
   for (i = 0; i < 3; i++)
   {
      c->pipeline[i] = api->pipeline_new(api->user, config->device, config->shaders[i], i == 2 ? 128 : 64);
      if (!c->pipeline[i]) goto error;
   }
   c->dummy = api->texture_new(api->user, config->device, 1, 1);
   if (!config->decode) c->workspace = api->buffer_new(api->user, config->device, c->layout.workspace_bytes, false);
   if (!c->dummy || (!config->decode && !c->workspace)) goto error;
   for (i = 0; i < config->slots; i++)
   {
      record_pyrowave_metal_slot_t *s = &c->slot[i];
      retro_atomic_int_init(&s->ready, 0);
      s->buffer = api->buffer_new(api->user, config->device, staging_bytes, true);
      if (!s->buffer || !(s->mapped = (uint8_t*)api->buffer_data(api->user, s->buffer))) goto error;
      if (config->decode)
      {
         s->texture = api->texture_new(api->user, config->device, c->layout.width, c->layout.height);
         if (!s->texture) goto error;
      }
   }
   api->pool_pop(api->user, pool);
   return c;
error:
   record_gpu_queue_free(c->queue);
   api->pool_pop(api->user, pool);
   record_pyrowave_metal_destroy(c);
   return NULL;
}

record_pyrowave_metal_t *record_pyrowave_metal_new(const record_pyrowave_metal_config_t *config)
{
   record_pyrowave_metal_api_t api;
   record_pyrowave_metal_t *c;
   if (!config || !record_pyrowave_metal_native_api(&api, config->device, config->queue)) return NULL;
   c = record_pyrowave_metal_new_with_api(config, &api);
   if (c) c->owns_api = true;
   else if (api.destroy) api.destroy(api.user);
   return c;
}

static void record_pyrowave_metal_retire_sources(record_pyrowave_metal_t *c)
{
   uint64_t retired = c->next_ticket - 1;
   unsigned i;
   for (i = 0; i < c->config.slots; i++)
      if (c->slot[i].source_pending && c->slot[i].frame.ready_value - 1 < retired)
         retired = c->slot[i].frame.ready_value - 1;
   c->retired = retired;
}

int record_pyrowave_metal_push(record_pyrowave_metal_t *c, void *commands,
      void *source, uint64_t timestamp, uint64_t duration, uint64_t *ticket)
{
   record_pyrowave_metal_slot_t *s;
   record_pyrowave_metal_api_t *a;
   rpyrowave_sequence_t sequence;
   void *pool;
   void *blit;
   unsigned slot;
   int result;
   if (!c || c->config.decode || !ticket || c->failed || !c->next_ticket) return RECORD_GPU_QUEUE_ERROR;
   a = &c->api;
   if (!a->commands_valid(a->user, commands, c->config.device, c->config.queue)
         || !a->source_valid(a->user, source, c->config.device,
            c->config.texture_width, c->config.texture_height)) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_acquire(c->queue, &slot);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   c->reservation = slot;
   s = &c->slot[slot];
   s->commands = a->retain(a->user, commands);
   if (!s->commands)
   {
      record_gpu_queue_cancel(c->queue);
      c->reservation = RECORD_GPU_QUEUE_SLOTS;
      return RECORD_GPU_QUEUE_ERROR;
   }
   s->frame.timestamp = timestamp;
   s->frame.duration = duration;
   s->frame.ready_value = c->next_ticket++;
   s->frame.slot = slot;
   s->frame.keyframe = true;
   s->source_pending = true;
   s->state = RPY_METAL_ENCODING;
   s->invalid = false;
   retro_atomic_store_release_int(&s->ready, 0);
   *ticket = s->frame.ready_value;
   pool = a->pool_push(a->user);
   c->encoder = a->compute_begin(a->user, commands);
   if (!c->encoder) s->invalid = true;
   else
   {
      record_pyrowave_metal_color(c, source, c->dummy, 0);
      a->compute_pipeline(a->user, c->encoder, c->pipeline[1]);
      c->cursor = 0;
      rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height,
            c->layout.chroma_444, 0, record_pyrowave_metal_dwt_dispatch, c);
      record_pyrowave_metal_packet_bind(c, c->workspace, 0);
      sequence = c->config.sequence;
      sequence.sequence = (unsigned)(s->frame.ready_value & 7);
      rpyrowave_gpu_encode_record(&c->layout, &sequence, c->config.maximum_bytes,
            -1, record_pyrowave_metal_packet_dispatch, c);
      a->end(a->user, c->encoder);
      c->encoder = NULL;
      blit = a->blit_begin(a->user, commands);
      if (!blit) s->invalid = true;
      else
      {
         a->blit_copy(a->user, blit, c->workspace, 0, s->buffer, 0, 16);
         a->blit_copy(a->user, blit, c->workspace, c->layout.wire_offset, s->buffer, 16,
               ((size_t)c->config.maximum_bytes + 3) / 4 * 4);
         a->synchronize(a->user, blit, s->buffer);
         a->end(a->user, blit);
      }
   }
   a->pool_pop(a->user, pool);
   if (!record_gpu_queue_submit(c->queue, &s->frame))
   {
      c->failed = true;
      return RECORD_GPU_QUEUE_ERROR;
   }
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return s->invalid ? RECORD_GPU_QUEUE_ERROR : RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_metal_take(record_pyrowave_metal_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || c->config.decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK) c->owned |= (uint32_t)1 << frame->slot;
   return result;
}

static uint32_t record_pyrowave_metal_load32(const uint8_t *p)
{
   return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int record_pyrowave_metal_packet(record_pyrowave_metal_t *c, unsigned slot, record_pyrowave_packet_t *packet)
{
   int ready;
   uint32_t bytes;
   const uint8_t *mapped;
   if (!c || c->config.decode || !packet || slot >= c->config.slots
         || !(c->owned & ((uint32_t)1 << slot))) return RECORD_GPU_QUEUE_ERROR;
   ready = retro_atomic_load_acquire_int(&c->slot[slot].ready);
   if (!ready) return RECORD_GPU_QUEUE_BUSY;
   if (ready < 0) return RECORD_GPU_QUEUE_ERROR;
   mapped = c->slot[slot].mapped;
   bytes = record_pyrowave_metal_load32(mapped + 4);
   if (record_pyrowave_metal_load32(mapped + 12) || bytes < 8 || bytes > c->config.maximum_bytes)
      return RECORD_GPU_QUEUE_ERROR;
   packet->data = mapped + 16;
   packet->bytes = bytes;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_metal_release(record_pyrowave_metal_t *c, unsigned slot)
{
   if (!c || c->config.decode || slot >= c->config.slots
         || !(c->owned & ((uint32_t)1 << slot))
         || !retro_atomic_load_acquire_int(&c->slot[slot].ready)) return false;
   retro_atomic_store_release_int(&c->slot[slot].ready, 0);
   c->owned &= ~((uint32_t)1 << slot);
   return record_gpu_queue_release(c->queue, slot);
}

uint64_t record_pyrowave_metal_source_retired(const record_pyrowave_metal_t *c)
{
   return c && !c->config.decode ? c->retired : 0;
}

int record_pyrowave_metal_receive_acquire(record_pyrowave_metal_t *c,
      unsigned *slot, void **data, size_t *capacity)
{
   int result;
   unsigned index;
   if (!c || !c->config.decode || !slot || !data || !capacity || !c->next_ticket)
      return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_acquire(c->queue, &index);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   c->reservation = index;
   *slot = index;
   *data = c->slot[index].mapped + c->layout.wire_offset;
   *capacity = c->config.maximum_bytes;
   return RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_metal_receive_submit(record_pyrowave_metal_t *c,
      size_t bytes, uint64_t timestamp, uint64_t duration)
{
   record_pyrowave_metal_slot_t *s;
   int result;
   if (!c || !c->config.decode || c->reservation >= c->config.slots || !c->next_ticket)
      return RECORD_GPU_QUEUE_ERROR;
   s = &c->slot[c->reservation];
   result = rpyrowave_gpu_index_frame(&c->layout, s->mapped, c->layout.workspace_bytes,
         bytes, 0, &s->parameters);
   if (result || s->parameters.usability != c->usability) return RECORD_GPU_QUEUE_ERROR;
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

bool record_pyrowave_metal_receive_cancel(record_pyrowave_metal_t *c)
{
   if (!c || !c->config.decode || !record_gpu_queue_cancel(c->queue)) return false;
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

int record_pyrowave_metal_receive_drop(record_pyrowave_metal_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || !c->config.decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK && !record_gpu_queue_release(c->queue, frame->slot))
      return RECORD_GPU_QUEUE_ERROR;
   return result;
}

int record_pyrowave_metal_decode(record_pyrowave_metal_t *c, void *commands)
{
   record_gpu_frame_t frame;
   record_pyrowave_metal_slot_t *s;
   record_pyrowave_metal_api_t *a;
   void *pool;
   void *blit;
   int result;
   if (!c || !c->config.decode || c->failed) return RECORD_GPU_QUEUE_ERROR;
   a = &c->api;
   if (!a->commands_valid(a->user, commands, c->config.device, c->config.queue)) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, &frame);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   s = &c->slot[frame.slot];
   s->commands = a->retain(a->user, commands);
   if (!s->commands)
   {
      record_gpu_queue_release(c->queue, frame.slot);
      return RECORD_GPU_QUEUE_ERROR;
   }
   s->state = RPY_METAL_DECODING;
   s->invalid = false;
   pool = a->pool_push(a->user);
   a->buffer_flush(a->user, s->buffer, c->layout.wire_offset + s->parameters.maximum_bytes);
   c->encoder = a->compute_begin(a->user, commands);
   if (!c->encoder) s->invalid = true;
   else
   {
      record_pyrowave_metal_packet_bind(c, s->buffer, 1);
      rpyrowave_gpu_decode_record(&c->layout, &s->parameters, record_pyrowave_metal_packet_dispatch, c);
      a->compute_pipeline(a->user, c->encoder, c->pipeline[1]);
      c->cursor = 0;
      rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height,
            c->layout.chroma_444, 1, record_pyrowave_metal_dwt_dispatch, c);
      record_pyrowave_metal_color(c, c->dummy, s->texture, 1);
      a->end(a->user, c->encoder);
      c->encoder = NULL;
      blit = a->blit_begin(a->user, commands);
      if (!blit) s->invalid = true;
      else
      {
         a->synchronize(a->user, blit, s->buffer);
         a->end(a->user, blit);
      }
   }
   a->pool_pop(a->user, pool);
   return s->invalid ? RECORD_GPU_QUEUE_ERROR : RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_metal_image(record_pyrowave_metal_t *c, record_pyrowave_metal_image_t *image)
{
   unsigned i;
   unsigned selected = RECORD_GPU_QUEUE_SLOTS;
   if (!c || !c->config.decode || !image) return RECORD_GPU_QUEUE_ERROR;
   for (i = 0; i < c->config.slots; i++)
      if (c->slot[i].state == RPY_METAL_READY && (selected == RECORD_GPU_QUEUE_SLOTS
               || c->slot[i].frame.ready_value < c->slot[selected].frame.ready_value)) selected = i;
   if (selected == RECORD_GPU_QUEUE_SLOTS) return RECORD_GPU_QUEUE_BUSY;
   image->frame = c->slot[selected].frame;
   image->texture = c->slot[selected].texture;
   c->slot[selected].state = RPY_METAL_LENT;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_metal_present(record_pyrowave_metal_t *c, unsigned slot, void *commands)
{
   record_pyrowave_metal_slot_t *s;
   if (!c || !c->config.decode || slot >= c->config.slots) return false;
   s = &c->slot[slot];
   if (s->state != RPY_METAL_LENT) return false;
   if (commands)
   {
      if (!c->api.commands_valid(c->api.user, commands, c->config.device, c->config.queue)) return false;
      s->commands = c->api.retain(c->api.user, commands);
      if (!s->commands) return false;
      s->state = RPY_METAL_PRESENTING;
      return true;
   }
   s->state = RPY_METAL_IDLE;
   return record_gpu_queue_release(c->queue, slot);
}

static bool record_pyrowave_metal_complete(record_pyrowave_metal_t *c, unsigned slot, bool failed)
{
   record_pyrowave_metal_slot_t *s = &c->slot[slot];
   bool valid = !failed && !s->invalid;
   c->api.release(c->api.user, s->commands);
   s->commands = NULL;
   if (!c->config.decode)
   {
      s->state = RPY_METAL_IDLE;
      s->source_pending = false;
      if (c->reservation == slot)
      {
         record_gpu_queue_cancel(c->queue);
         c->reservation = RECORD_GPU_QUEUE_SLOTS;
      }
      else retro_atomic_store_release_int(&s->ready, valid ? 1 : -1);
   }
   else if (s->state == RPY_METAL_DECODING && valid && !record_pyrowave_metal_load32(s->mapped + 12))
      s->state = RPY_METAL_READY;
   else
   {
      if (s->state == RPY_METAL_DECODING) valid = false;
      s->state = RPY_METAL_IDLE;
      record_gpu_queue_release(c->queue, slot);
   }
   return valid;
}

bool record_pyrowave_metal_poll(record_pyrowave_metal_t *c)
{
   unsigned i;
   bool healthy;
   if (!c) return false;
   healthy = !c->failed;
   for (i = 0; i < c->config.slots; i++)
   {
      unsigned status;
      if (!c->slot[i].commands) continue;
      status = c->api.status(c->api.user, c->slot[i].commands);
      if ((status == 4 || status == 5) && !record_pyrowave_metal_complete(c, i, status != 4)) healthy = false;
   }
   if (!c->config.decode) record_pyrowave_metal_retire_sources(c);
   return healthy;
}

bool record_pyrowave_metal_discard(record_pyrowave_metal_t *c, void *commands)
{
   unsigned i;
   bool found = false;
   if (!c || !commands || c->api.status(c->api.user, commands) > 1) return false;
   for (i = 0; i < c->config.slots; i++)
      if (c->slot[i].commands == commands)
      {
         record_pyrowave_metal_complete(c, i, true);
         found = true;
      }
   if (!c->config.decode) record_pyrowave_metal_retire_sources(c);
   return found;
}

bool record_pyrowave_metal_finish(record_pyrowave_metal_t *c)
{
   return c && record_gpu_queue_finish(c->queue);
}

bool record_pyrowave_metal_free(record_pyrowave_metal_t *c)
{
   unsigned i;
   if (!c) return true;
   for (i = 0; i < c->config.slots; i++)
      if (c->slot[i].commands || c->slot[i].state != RPY_METAL_IDLE) return false;
   if (!record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_metal_destroy(c);
   return true;
}

static int record_pyrowave_metal_session_reserve(record_pyrowave_metal_t *c, void **data, size_t *capacity)
{
   unsigned slot;
   return record_pyrowave_metal_receive_acquire(c, &slot, data, capacity);
}

RECORD_PYROWAVE_SESSION_BINDING(record_pyrowave_metal, record_pyrowave_metal_t, record_pyrowave_metal_session_reserve)
