/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_pyrowave_metal_internal.h"
#include <formats/rpyrowave_gpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "Metal contract failed line %u: %s\n", \
   (unsigned)__LINE__, #expr); exit(EXIT_FAILURE); } } while (0)

typedef struct test_metal_object
{
   uint8_t *cpu;
   uint8_t *gpu;
   size_t bytes;
   unsigned kind, refs, width, height, index;
} test_metal_object_t;

typedef struct test_metal_commands
{
   unsigned refs, status, direction, sequence, dispatches, barriers, copies, syncs, cursor, pipeline;
   bool active, unretained, foreign;
   test_metal_object_t *buffers[5];
   size_t offsets[5];
   test_metal_object_t *textures[4];
   test_metal_object_t *workspace;
} test_metal_commands_t;

typedef struct test_metal
{
   record_pyrowave_metal_api_t api;
   record_pyrowave_metal_config_t config;
   test_metal_object_t device, queue, source;
   test_metal_commands_t commands[8];
   unsigned objects, calls, fail_at, pools, flushes;
   bool hot, managed, shader_error, fail_compute, fail_blit;
} test_metal_t;

static void test_metal_store(uint8_t *p, uint32_t value)
{
   p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
   p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24);
}

static uint32_t test_metal_load(const uint8_t *p)
{
   return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void *test_metal_retain(void *user, void *object)
{
   test_metal_t *t = (test_metal_t*)user;
   if (!object) return NULL;
   if (object == &t->device || object == &t->queue) ((test_metal_object_t*)object)->refs++;
   else ((test_metal_commands_t*)object)->refs++;
   return object;
}

static void test_metal_release(void *user, void *object)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_object_t *o = (test_metal_object_t*)object;
   unsigned i;
   if (!object) return;
   for (i = 0; i < 8; i++)
      if (object == &t->commands[i])
      {
         CHECK(t->commands[i].refs > 1);
         t->commands[i].refs--;
         return;
      }
   CHECK(o->refs);
   if (--o->refs) return;
   CHECK(!t->hot && o != &t->device && o != &t->queue && t->objects);
   if (o->cpu != o->gpu) free(o->cpu);
   free(o->gpu);
   free(o);
   t->objects--;
}

static size_t test_metal_limit(void *user, void *device)
{
   CHECK(device == &((test_metal_t*)user)->device);
   return 128 * 1024 * 1024;
}

static test_metal_object_t *test_metal_object_new(test_metal_t *t, unsigned kind)
{
   test_metal_object_t *o;
   CHECK(!t->hot);
   if (++t->calls == t->fail_at) return NULL;
   o = (test_metal_object_t*)calloc(1, sizeof(*o));
   CHECK(o);
   o->kind = kind;
   o->refs = 1;
   t->objects++;
   return o;
}

static void *test_metal_buffer_new(void *user, void *device, size_t bytes, bool cpu)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_object_t *o = test_metal_object_new(t, 1);
   CHECK(device == &t->device);
   if (!o) return NULL;
   o->bytes = bytes;
   o->gpu = (uint8_t*)calloc(1, bytes);
   if (cpu) o->cpu = t->managed ? (uint8_t*)calloc(1, bytes) : o->gpu;
   CHECK(o->gpu && (!cpu || o->cpu));
   return o;
}

static void *test_metal_buffer_data(void *user, void *buffer)
{
   CHECK(!((test_metal_t*)user)->hot);
   return ((test_metal_object_t*)buffer)->cpu;
}

static void test_metal_flush(void *user, void *buffer, size_t bytes)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_object_t *o = (test_metal_object_t*)buffer;
   CHECK(o->cpu && bytes <= o->bytes);
   if (t->managed) memcpy(o->gpu, o->cpu, bytes);
   t->flushes++;
}

static void *test_metal_texture_new(void *user, void *device, unsigned width, unsigned height)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_object_t *o = test_metal_object_new(t, 2);
   CHECK(device == &t->device);
   if (o) { o->width = width; o->height = height; }
   return o;
}

static void *test_metal_pipeline_new(void *user, void *device, const char *source, unsigned threads)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_object_t *o = test_metal_object_new(t, 3);
   CHECK(device == &t->device && source && (threads == 64 || threads == 128));
   if (o) o->index = (unsigned)(source[0] - '0');
   return o;
}

static bool test_metal_source_valid(void *user, void *source, void *device, unsigned width, unsigned height)
{
   test_metal_t *t = (test_metal_t*)user;
   return source == &t->source && device == &t->device && width == 134 && height == 104;
}

static bool test_metal_commands_valid(void *user, void *commands, void *device, void *queue)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_commands_t *c = (test_metal_commands_t*)commands;
   return c && device == &t->device && queue == &t->queue && c->status <= 1
      && !c->active && !c->foreign && !c->unretained;
}

static unsigned test_metal_status(void *user, void *commands)
{
   (void)user;
   return ((test_metal_commands_t*)commands)->status;
}

static void *test_metal_compute(void *user, void *commands)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_commands_t *c = (test_metal_commands_t*)commands;
   if (t->fail_compute) return NULL;
   CHECK(!c->active && c->status <= 1);
   c->active = true;
   return c;
}

static void test_metal_pipeline(void *user, void *encoder, void *pipeline)
{
   test_metal_commands_t *c = (test_metal_commands_t*)encoder;
   test_metal_object_t *p = (test_metal_object_t*)pipeline;
   (void)user;
   CHECK(c->active && p->kind == 3 && p->index <= 2);
   c->pipeline = p->index;
   c->cursor = 0;
}

static void test_metal_buffer(void *user, void *encoder, void *buffer, size_t offset, unsigned index)
{
   test_metal_commands_t *c = (test_metal_commands_t*)encoder;
   test_metal_object_t *b = (test_metal_object_t*)buffer;
   (void)user;
   CHECK(c->active && index < 5 && b->kind == 1 && offset < b->bytes && !(offset & 255));
   c->buffers[index] = b;
   c->offsets[index] = offset;
}

static void test_metal_texture(void *user, void *encoder, void *texture, unsigned index)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_commands_t *c = (test_metal_commands_t*)encoder;
   CHECK(c->active && index < 4 && (texture == &t->source || ((test_metal_object_t*)texture)->kind == 2));
   c->textures[index] = (test_metal_object_t*)texture;
}

static void test_metal_dispatch(void *user, void *encoder,
      unsigned x, unsigned y, unsigned z, unsigned tx, unsigned ty)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_commands_t *c = (test_metal_commands_t*)encoder;
   unsigned inverse = c->direction;
   CHECK(c->active && c->dispatches == c->barriers && x && y && z && x <= 65535 && y <= 65535);
   if (c->pipeline == 0)
   {
      const rpyrowave_color_parameters_t *p = (const rpyrowave_color_parameters_t*)
         (c->buffers[4]->gpu + c->offsets[4]);
      CHECK(c->buffers[1] != c->buffers[2] && !c->buffers[1]->cpu && !c->buffers[2]->cpu);
      CHECK(p->inverse == inverse && p->width == 130 && p->height == 98 && p->image_width == 160);
      CHECK(p->texture_mode == t->config.texture_mode && !p->colorimetry);
      CHECK(c->textures[0] && c->textures[3] && tx == 8 && ty == 8);
      CHECK(x == (inverse ? 17U : 20U) && y == (inverse ? 13U : 16U) && z == (inverse ? 1U : 3U));
   }
   else if (c->pipeline == 1)
   {
      const rpyrowave_dwt_parameters_t *p = (const rpyrowave_dwt_parameters_t*)
         (c->buffers[2]->gpu + c->offsets[2]);
      unsigned axis = (c->cursor / 5) & 1;
      CHECK(c->buffers[0] != c->buffers[1] && !c->buffers[0]->cpu && !c->buffers[1]->cpu);
      CHECK(p->inverse == inverse && p->phase == c->cursor % 5);
      CHECK(p->axis == (inverse ? 1 - axis : axis));
      CHECK(p->level == (inverse ? 4 - c->cursor / 10 : c->cursor / 10));
      CHECK(tx == 8 && ty == 8 && z == 3);
      CHECK(x == ((160U >> p->level) + 7) / 8 && y == ((128U >> p->level) + 7) / 8);
   }
   else
   {
      const rpyrowave_packet_parameters_t *p = (const rpyrowave_packet_parameters_t*)
         (c->buffers[4]->gpu + c->offsets[4]);
      test_metal_object_t *workspace = c->buffers[2];
      unsigned operation = c->cursor == 0 ? 0 : c->cursor < 17 ? ((c->cursor & 1) ? 1 : 2)
         : c->cursor == 17 ? 1 : c->cursor - 15;
      size_t base = (64 + p->block_count * 16 + 255) & ~(size_t)255;
      CHECK(p->operation == (inverse ? 5U : operation) && p->sequence == c->sequence && p->maximum_bytes == 4096);
      CHECK(tx == 128 && ty == 1 && z == 1 && c->buffers[0] != c->buffers[3]);
      CHECK(!c->buffers[0]->cpu && !c->buffers[3]->cpu && c->buffers[1]->cpu);
      c->workspace = workspace;
      if (!inverse && !p->operation) memset(workspace->gpu, 0, workspace->bytes);
      if (!inverse && p->operation == 3)
      {
         test_metal_store(workspace->gpu + 4, 8);
         test_metal_store(workspace->gpu + base, 0x80000000U | (p->sequence << 28) | (97U << 14) | 129U);
         test_metal_store(workspace->gpu + base + 4, p->usability);
      }
      if (inverse)
      {
         CHECK(workspace->cpu && test_metal_load(workspace->gpu + 4) >= 8);
         CHECK(((test_metal_load(workspace->gpu + base) >> 28) & 7) == c->sequence);
      }
      if (t->shader_error) test_metal_store(workspace->gpu + 12, 1);
   }
   c->cursor++;
   c->dispatches++;
}

static void test_metal_barrier(void *user, void *encoder, bool textures)
{
   test_metal_commands_t *c = (test_metal_commands_t*)encoder;
   (void)user;
   CHECK(c->dispatches == c->barriers + 1 && textures == (c->pipeline == 0 && c->direction != 0));
   c->barriers++;
}

static void test_metal_end(void *user, void *encoder)
{
   test_metal_commands_t *c = (test_metal_commands_t*)encoder;
   (void)user;
   CHECK(c->active && c->dispatches == c->barriers);
   c->active = false;
}

static void *test_metal_blit(void *user, void *commands)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_commands_t *c = (test_metal_commands_t*)commands;
   if (t->fail_blit) return NULL;
   CHECK(!c->active);
   c->active = true;
   return c;
}

static void test_metal_copy(void *user, void *encoder,
      void *source, size_t source_offset, void *destination, size_t destination_offset, size_t bytes)
{
   test_metal_commands_t *c = (test_metal_commands_t*)encoder;
   test_metal_object_t *src = (test_metal_object_t*)source;
   test_metal_object_t *dst = (test_metal_object_t*)destination;
   (void)user;
   CHECK(c->active && src == c->workspace && dst->cpu && !src->cpu);
   CHECK(source_offset + bytes <= src->bytes && destination_offset + bytes <= dst->bytes);
   CHECK((bytes == 16 && !source_offset && !destination_offset)
         || (bytes == 4096 && source_offset >= 256 && destination_offset == 16));
   memcpy(dst->gpu + destination_offset, src->gpu + source_offset, bytes);
   c->copies++;
}

static void test_metal_sync(void *user, void *encoder, void *buffer)
{
   test_metal_t *t = (test_metal_t*)user;
   test_metal_commands_t *c = (test_metal_commands_t*)encoder;
   test_metal_object_t *b = (test_metal_object_t*)buffer;
   CHECK(c->active && b->cpu);
   if (t->managed) memcpy(b->cpu, b->gpu, b->bytes);
   c->syncs++;
}

static void *test_metal_pool_push(void *user)
{
   test_metal_t *t = (test_metal_t*)user;
   t->pools++;
   return t;
}

static void test_metal_pool_pop(void *user, void *pool)
{
   test_metal_t *t = (test_metal_t*)user;
   CHECK(pool == t && t->pools);
   t->pools--;
}

static test_metal_commands_t *test_metal_command(test_metal_t *t, unsigned index, unsigned inverse, unsigned sequence)
{
   test_metal_commands_t *c = &t->commands[index];
   CHECK(c->refs == 1 && !c->active);
   memset(c, 0, sizeof(*c));
   c->refs = 1;
   c->direction = inverse;
   c->sequence = sequence;
   return c;
}

static test_metal_t *test_metal_new(bool managed, bool decode)
{
   test_metal_t *t = (test_metal_t*)calloc(1, sizeof(*t));
   unsigned i;
   CHECK(t);
   t->managed = managed;
   t->device.refs = t->queue.refs = t->source.refs = 1;
   for (i = 0; i < 8; i++) t->commands[i].refs = 1;
   t->config.device = &t->device;
   t->config.queue = &t->queue;
   t->config.shaders[0] = "0"; t->config.shaders[1] = "1"; t->config.shaders[2] = "2";
   t->config.sequence.width = 130; t->config.sequence.height = 98;
   t->config.maximum_bytes = 4096; t->config.slots = 3; t->config.bit_depth = 8;
   t->config.texture_mode = managed ? RPYROWAVE_COLOR_SCRGB : RPYROWAVE_COLOR_CODE;
   t->config.texture_width = 134; t->config.texture_height = 104;
   t->config.x = 2; t->config.y = 3; t->config.decode = decode;
   t->api.user = t;
   t->api.retain = test_metal_retain; t->api.release = test_metal_release;
   t->api.buffer_limit = test_metal_limit; t->api.buffer_new = test_metal_buffer_new;
   t->api.buffer_data = test_metal_buffer_data; t->api.buffer_flush = test_metal_flush;
   t->api.texture_new = test_metal_texture_new; t->api.pipeline_new = test_metal_pipeline_new;
   t->api.source_valid = test_metal_source_valid; t->api.commands_valid = test_metal_commands_valid;
   t->api.status = test_metal_status; t->api.compute_begin = test_metal_compute;
   t->api.compute_pipeline = test_metal_pipeline; t->api.compute_buffer = test_metal_buffer;
   t->api.compute_texture = test_metal_texture; t->api.compute_dispatch = test_metal_dispatch;
   t->api.compute_barrier = test_metal_barrier; t->api.end = test_metal_end;
   t->api.blit_begin = test_metal_blit; t->api.blit_copy = test_metal_copy;
   t->api.synchronize = test_metal_sync; t->api.pool_push = test_metal_pool_push;
   t->api.pool_pop = test_metal_pool_pop;
   return t;
}

static void test_metal_free(test_metal_t *t)
{
   unsigned i;
   CHECK(!t->objects && !t->pools && t->device.refs == 1 && t->queue.refs == 1);
   for (i = 0; i < 8; i++) CHECK(t->commands[i].refs == 1);
   free(t);
}

static void test_metal_encoder(bool managed)
{
   test_metal_t *t = test_metal_new(managed, false);
   record_pyrowave_metal_t *codec = record_pyrowave_metal_new_with_api(&t->config, &t->api);
   record_gpu_frame_t frames[3];
   record_pyrowave_packet_t packets[3];
   uint64_t ticket = 0;
   unsigned round;
   unsigned i;
   CHECK(codec);
   for (round = 0; round < 9; round++)
   {
      uint8_t held[8];
      for (i = 0; i < 3; i++)
      {
         uint64_t next = ticket + 1;
         test_metal_commands_t *commands = test_metal_command(t, i, 0, (unsigned)(next & 7));
         t->hot = true;
         CHECK(record_pyrowave_metal_push(codec, commands, &t->source, next * 16667, 16667, &ticket)
               == RECORD_GPU_QUEUE_OK && ticket == next);
         CHECK(commands->dispatches == 71 && commands->copies == 2 && commands->syncs == 1);
         CHECK(record_pyrowave_metal_take(codec, &frames[i]) == RECORD_GPU_QUEUE_OK);
         CHECK(record_pyrowave_metal_packet(codec, frames[i].slot, &packets[i]) == RECORD_GPU_QUEUE_BUSY);
         CHECK(!record_pyrowave_metal_release(codec, frames[i].slot));
         t->hot = false;
      }
      CHECK(!record_pyrowave_metal_free(codec));
      {
         uint64_t unchanged = ticket;
         test_metal_commands_t *extra = test_metal_command(t, 3, 0, 0);
         CHECK(record_pyrowave_metal_push(codec, extra, &t->source, 0, 0, &unchanged) == RECORD_GPU_QUEUE_BUSY);
         CHECK(unchanged == ticket && !extra->dispatches);
      }
      t->hot = true;
      CHECK(record_pyrowave_metal_discard(codec, &t->commands[2]));
      CHECK(record_pyrowave_metal_source_retired(codec) == ticket - 3);
      CHECK(record_pyrowave_metal_packet(codec, frames[2].slot, &packets[2]) == RECORD_GPU_QUEUE_ERROR);
      CHECK(record_pyrowave_metal_release(codec, frames[2].slot));
      t->commands[1].status = 4;
      CHECK(record_pyrowave_metal_poll(codec));
      CHECK(record_pyrowave_metal_source_retired(codec) == ticket - 3);
      t->commands[0].status = 4;
      CHECK(record_pyrowave_metal_poll(codec));
      CHECK(record_pyrowave_metal_source_retired(codec) == ticket);
      for (i = 0; i < 2; i++)
      {
         rpyrowave_sequence_t sequence;
         CHECK(record_pyrowave_metal_packet(codec, frames[i].slot, &packets[i]) == RECORD_GPU_QUEUE_OK);
         CHECK(packets[i].bytes == 8 && !rpyrowave_read_sequence(packets[i].data, packets[i].bytes, &sequence));
         CHECK(sequence.sequence == (frames[i].ready_value & 7) && frames[i].duration == 16667);
      }
      memcpy(held, packets[0].data, 8);
      CHECK(record_pyrowave_metal_release(codec, frames[1].slot));
      {
         record_gpu_frame_t next_frame;
         record_pyrowave_packet_t next_packet;
         uint64_t next = ticket + 1;
         test_metal_commands_t *commands = test_metal_command(t, 3, 0, (unsigned)(next & 7));
         CHECK(record_pyrowave_metal_push(codec, commands, &t->source, 0, 0, &ticket) == RECORD_GPU_QUEUE_OK);
         CHECK(record_pyrowave_metal_take(codec, &next_frame) == RECORD_GPU_QUEUE_OK);
         commands->status = 5;
         CHECK(!record_pyrowave_metal_poll(codec));
         CHECK(record_pyrowave_metal_packet(codec, next_frame.slot, &next_packet) == RECORD_GPU_QUEUE_ERROR);
         CHECK(record_pyrowave_metal_release(codec, next_frame.slot));
      }
      CHECK(!memcmp(held, packets[0].data, 8));
      CHECK(record_pyrowave_metal_release(codec, frames[0].slot));
      CHECK(!record_pyrowave_metal_release(codec, frames[0].slot));
      t->hot = false;
   }
   CHECK(record_pyrowave_metal_finish(codec));
   CHECK(record_pyrowave_metal_take(codec, &frames[0]) == RECORD_GPU_QUEUE_END);
   CHECK(record_pyrowave_metal_free(codec));
   test_metal_free(t);
}

static unsigned test_metal_receive(record_pyrowave_metal_t *codec, unsigned sequence, uint64_t timestamp)
{
   void *data;
   size_t capacity;
   unsigned slot;
   CHECK(record_pyrowave_metal_receive_acquire(codec, &slot, &data, &capacity) == RECORD_GPU_QUEUE_OK);
   CHECK(capacity == 4096);
   test_metal_store((uint8_t*)data, 0x80000000U | (sequence << 28) | (97U << 14) | 129U);
   test_metal_store((uint8_t*)data + 4, 0);
   CHECK(record_pyrowave_metal_receive_submit(codec, 8, timestamp, 16667) == RECORD_GPU_QUEUE_OK);
   return slot;
}

static void test_metal_decoder(bool managed, bool sdr)
{
   test_metal_t *t = test_metal_new(managed, true);
   record_pyrowave_metal_t *codec;
   record_pyrowave_metal_image_t image;
   record_gpu_frame_t frame;
   test_metal_commands_t *commands;
   unsigned slot;
   unsigned round;
   if (sdr) t->config.texture_mode = RPYROWAVE_COLOR_SDR709;
   codec = record_pyrowave_metal_new_with_api(&t->config, &t->api);
   CHECK(codec);
   for (round = 0; round < 9; round++)
   {
      t->hot = true;
      slot = test_metal_receive(codec, 7, round * 16667);
      commands = test_metal_command(t, 0, 1, 7);
      CHECK(record_pyrowave_metal_decode(codec, commands) == RECORD_GPU_QUEUE_OK);
      CHECK(commands->dispatches == 52 && !commands->copies && commands->syncs == 1);
      CHECK(record_pyrowave_metal_image(codec, &image) == RECORD_GPU_QUEUE_BUSY);
      CHECK(!record_pyrowave_metal_present(codec, slot, NULL));
      commands->status = 4;
      CHECK(record_pyrowave_metal_poll(codec));
      CHECK(record_pyrowave_metal_image(codec, &image) == RECORD_GPU_QUEUE_OK);
      CHECK(image.frame.slot == slot && image.frame.timestamp == round * 16667 && image.texture);
      t->hot = false;
      CHECK(!record_pyrowave_metal_free(codec));
      t->hot = true;
      commands = test_metal_command(t, 1, 1, 0);
      CHECK(record_pyrowave_metal_present(codec, slot, commands));
      CHECK(!record_pyrowave_metal_present(codec, slot, NULL));
      commands->status = 4;
      CHECK(record_pyrowave_metal_poll(codec));
      t->hot = false;
   }
   t->hot = true;
   test_metal_receive(codec, 5, 100);
   commands = test_metal_command(t, 0, 1, 5);
   t->shader_error = true;
   CHECK(record_pyrowave_metal_decode(codec, commands) == RECORD_GPU_QUEUE_OK);
   t->shader_error = false;
   commands->status = 4;
   CHECK(!record_pyrowave_metal_poll(codec));
   CHECK(record_pyrowave_metal_image(codec, &image) == RECORD_GPU_QUEUE_BUSY);
   test_metal_receive(codec, 3, 100);
   CHECK(record_pyrowave_metal_receive_drop(codec, &frame) == RECORD_GPU_QUEUE_OK);
   CHECK(frame.timestamp == 100);
   test_metal_receive(codec, 1, 200);
   commands = test_metal_command(t, 0, 1, 1);
   CHECK(record_pyrowave_metal_decode(codec, commands) == RECORD_GPU_QUEUE_OK);
   CHECK(record_pyrowave_metal_discard(codec, commands));
   CHECK(record_pyrowave_metal_image(codec, &image) == RECORD_GPU_QUEUE_BUSY);
   {
      record_pyrowave_metal_image_t images[3];
      unsigned i;
      void *data;
      size_t capacity;
      for (i = 0; i < 3; i++)
      {
         test_metal_receive(codec, i, 1000 + i);
         commands = test_metal_command(t, i, 1, i);
         CHECK(record_pyrowave_metal_decode(codec, commands) == RECORD_GPU_QUEUE_OK);
      }
      CHECK(record_pyrowave_metal_receive_acquire(codec, &slot, &data, &capacity) == RECORD_GPU_QUEUE_BUSY);
      for (i = 0; i < 3; i++) t->commands[i].status = 4;
      CHECK(record_pyrowave_metal_poll(codec));
      for (i = 0; i < 3; i++)
      {
         CHECK(record_pyrowave_metal_image(codec, &images[i]) == RECORD_GPU_QUEUE_OK);
         CHECK(images[i].frame.timestamp == 1000 + i);
         if (i) CHECK(images[i].texture != images[i - 1].texture);
      }
      commands = test_metal_command(t, 4, 1, 0);
      CHECK(record_pyrowave_metal_present(codec, images[0].frame.slot, commands));
      CHECK(record_pyrowave_metal_present(codec, images[1].frame.slot, NULL));
      CHECK(record_pyrowave_metal_present(codec, images[2].frame.slot, NULL));
      for (i = 0; i < 2; i++) CHECK(test_metal_receive(codec, i, 2000) != images[0].frame.slot);
      CHECK(record_pyrowave_metal_receive_acquire(codec, &slot, &data, &capacity) == RECORD_GPU_QUEUE_BUSY);
      for (i = 0; i < 2; i++) CHECK(record_pyrowave_metal_receive_drop(codec, &frame) == RECORD_GPU_QUEUE_OK);
      commands->status = 4;
      CHECK(record_pyrowave_metal_poll(codec));
      for (i = 0; i < 3; i++) test_metal_receive(codec, i, 3000);
      CHECK(record_pyrowave_metal_receive_acquire(codec, &slot, &data, &capacity) == RECORD_GPU_QUEUE_BUSY);
      for (i = 0; i < 3; i++) CHECK(record_pyrowave_metal_receive_drop(codec, &frame) == RECORD_GPU_QUEUE_OK);
   }
   CHECK(record_pyrowave_metal_finish(codec));
   commands = test_metal_command(t, 0, 1, 0);
   CHECK(record_pyrowave_metal_decode(codec, commands) == RECORD_GPU_QUEUE_END);
   t->hot = false;
   CHECK(record_pyrowave_metal_free(codec));
   test_metal_free(t);
}

static void test_metal_failures(void)
{
   unsigned fail;
   for (fail = 1; fail <= 18; fail++)
   {
      test_metal_t *t = test_metal_new(true, (fail & 1) != 0);
      record_pyrowave_metal_t *codec;
      t->fail_at = fail;
      codec = record_pyrowave_metal_new_with_api(&t->config, &t->api);
      CHECK(!codec || record_pyrowave_metal_free(codec));
      test_metal_free(t);
   }
}

static void test_metal_invalid(void)
{
   test_metal_t *t = test_metal_new(true, true);
   record_pyrowave_metal_t *codec = record_pyrowave_metal_new_with_api(&t->config, &t->api);
   void *data;
   size_t capacity;
   unsigned slot;
   CHECK(codec);
   CHECK(record_pyrowave_metal_receive_acquire(codec, &slot, &data, &capacity) == RECORD_GPU_QUEUE_OK);
   memset(data, 0, 8);
   CHECK(record_pyrowave_metal_receive_submit(codec, 8, 0, 0) == RECORD_GPU_QUEUE_ERROR);
   CHECK(!record_pyrowave_metal_finish(codec));
   CHECK(record_pyrowave_metal_receive_cancel(codec));
   CHECK(!record_pyrowave_metal_receive_cancel(codec));
   CHECK(record_pyrowave_metal_receive_acquire(codec, &slot, &data, &capacity) == RECORD_GPU_QUEUE_OK);
   test_metal_store((uint8_t*)data, 0x80000000U | (97U << 14) | 129U);
   test_metal_store((uint8_t*)data + 4, 1U << 30);
   CHECK(record_pyrowave_metal_receive_submit(codec, 8, 0, 0) == RECORD_GPU_QUEUE_ERROR);
   CHECK(record_pyrowave_metal_receive_cancel(codec));
   CHECK(record_pyrowave_metal_free(codec));
   test_metal_free(t);
   t = test_metal_new(false, false);
   codec = record_pyrowave_metal_new_with_api(&t->config, &t->api);
   CHECK(codec);
   {
      record_gpu_frame_t frame;
      record_pyrowave_packet_t packet;
      uint64_t ticket = 0;
      test_metal_commands_t *commands = test_metal_command(t, 0, 0, 1);
      commands->unretained = true;
      CHECK(record_pyrowave_metal_push(codec, commands, &t->source, 0, 0, &ticket) == RECORD_GPU_QUEUE_ERROR);
      CHECK(!ticket);
      commands->unretained = false;
      commands->foreign = true;
      CHECK(record_pyrowave_metal_push(codec, commands, &t->source, 0, 0, &ticket) == RECORD_GPU_QUEUE_ERROR);
      commands->foreign = false;
      t->fail_compute = true;
      CHECK(record_pyrowave_metal_push(codec, commands, &t->source, 0, 0, &ticket) == RECORD_GPU_QUEUE_ERROR);
      CHECK(ticket == 1 && record_pyrowave_metal_take(codec, &frame) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_metal_packet(codec, frame.slot, &packet) == RECORD_GPU_QUEUE_BUSY);
      commands->status = 4;
      CHECK(!record_pyrowave_metal_poll(codec));
      CHECK(record_pyrowave_metal_packet(codec, frame.slot, &packet) == RECORD_GPU_QUEUE_ERROR);
      CHECK(record_pyrowave_metal_release(codec, frame.slot));
      t->fail_compute = false;
      t->fail_blit = true;
      commands = test_metal_command(t, 0, 0, 2);
      CHECK(record_pyrowave_metal_push(codec, commands, &t->source, 0, 0, &ticket) == RECORD_GPU_QUEUE_ERROR);
      CHECK(record_pyrowave_metal_take(codec, &frame) == RECORD_GPU_QUEUE_OK);
      commands->status = 4;
      CHECK(!record_pyrowave_metal_poll(codec));
      CHECK(record_pyrowave_metal_packet(codec, frame.slot, &packet) == RECORD_GPU_QUEUE_ERROR);
      CHECK(record_pyrowave_metal_release(codec, frame.slot));
   }
   CHECK(record_pyrowave_metal_free(codec));
   test_metal_free(t);
}

int main(void)
{
   test_metal_encoder(false);
   test_metal_encoder(true);
   test_metal_decoder(false, false);
   test_metal_decoder(true, false);
   test_metal_decoder(false, true);
   test_metal_decoder(true, true);
   test_metal_failures();
   test_metal_invalid();
   puts("Metal C89 encode/decode bindings and asynchronous ownership contracts PASS (portable stand-in, not native Metal)");
   return EXIT_SUCCESS;
}
