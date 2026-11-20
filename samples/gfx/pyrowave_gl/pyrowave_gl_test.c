/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../libretro-common/samples/formats/pyrowave/gpu_gl_context.h"
#include <glsym/glsym.h>
#include "../../../record/record_pyrowave_gl.h"

static unsigned rpy_capture_forbidden;
static bool rpy_capture_active;
static RGLSYMGLBUFFERDATAPROC rpy_capture_buffer_data;
static RGLSYMGLBUFFERSTORAGEPROC rpy_capture_buffer_storage;
static RGLSYMGLMAPBUFFERRANGEPROC rpy_capture_map;
static RGLSYMGLCLIENTWAITSYNCPROC rpy_capture_wait;
static RGLSYMGLFENCESYNCPROC rpy_capture_fence;

static void APIENTRY rpy_capture_buffer_data_spy(GLenum target, GLsizeiptr bytes, const void *data, GLenum usage)
{
   if (rpy_capture_active) rpy_capture_forbidden++;
   rpy_capture_buffer_data(target, bytes, data, usage);
}

static void APIENTRY rpy_capture_buffer_storage_spy(GLenum target, GLsizeiptr bytes, const void *data, GLbitfield flags)
{
   if (rpy_capture_active) rpy_capture_forbidden++;
   rpy_capture_buffer_storage(target, bytes, data, flags);
}

static void *APIENTRY rpy_capture_map_spy(GLenum target, GLintptr offset, GLsizeiptr bytes, GLbitfield access)
{
   if (rpy_capture_active) rpy_capture_forbidden++;
   return rpy_capture_map(target, offset, bytes, access);
}

static GLenum APIENTRY rpy_capture_wait_spy(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
   if (rpy_capture_active && (flags || timeout)) rpy_capture_forbidden++;
   return rpy_capture_wait(sync, flags, timeout);
}

static GLsync APIENTRY rpy_capture_fence_fail(GLenum condition, GLbitfield flags)
{
   (void)condition;
   (void)flags;
   return NULL;
}

static rglgen_func_t rpy_capture_get_proc(const char *name)
{
   rglgen_func_t result = NULL;
#if defined(_WIN32)
   PROC entry = wglGetProcAddress(name);
   if (!entry) entry = GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
#else
   __eglMustCastToProperFunctionPointerType entry = eglGetProcAddress(name);
#endif
   memcpy(&result, &entry, sizeof(entry));
   return result;
}

static uint32_t rpy_capture_hash(const record_pyrowave_packet_t *packet)
{
   const uint8_t *data = (const uint8_t*)packet->data;
   uint32_t hash = 2166136261U;
   size_t i;
   for (i = 0; i < packet->bytes; i++) hash = (hash ^ data[i]) * 16777619U;
   return hash;
}

static bool rpy_capture_scaled;
static const float *rpy_capture_pixel(const float *rgb, unsigned x, unsigned y)
{
   unsigned sx = rpy_capture_scaled ? ((2 * x + 1) * 134) / (2 * 130) : x + 2;
   unsigned sy = rpy_capture_scaled ? ((2 * y + 1) * 104) / (2 * 98) : y + 3;
   return rgb + (sy * 134 + sx) * 4;
}

static int rpy_capture_decode(rpyrowave_decoder_t *decoder, const record_pyrowave_packet_t *packet,
      const record_gpu_frame_t *frame, const float *rgb)
{
   rpyrowave_image_t image;
   unsigned component;
   if (frame->timestamp != frame->ready_value * 16667 || frame->duration != 16667
         || !frame->keyframe || packet->bytes > 262144
         || rpyrowave_decode_frame(decoder, packet->data, packet->bytes, 0, &image)
         || image.sequence.sequence != (frame->ready_value & 7)) return 0;
   for (component = 0; component < 3; component++)
   {
      unsigned x;
      unsigned y;
      unsigned shift = component ? 1 : 0;
      for (y = 0; y < 98U >> shift; y++)
         for (x = 0; x < 130U >> shift; x++)
         {
            double channels[3] = { 0.0, 0.0, 0.0 };
            double luma;
            double expected;
            double error;
            unsigned channel;
            unsigned dx;
            unsigned dy;
            for (dy = 0; dy <= shift; dy++)
               for (dx = 0; dx <= shift; dx++)
                  for (channel = 0; channel < 3; channel++)
                     channels[channel] += rpy_capture_pixel(rgb, (x << shift) + dx, (y << shift) + dy)[channel] / (shift ? 4.0 : 1.0);
            luma = channels[0] * 0.2126 + channels[1] * 0.7152 + channels[2] * 0.0722;
            expected = luma;
            if (component == 1) expected = 128.0 / 255.0 + (channels[2] - luma) / 1.8556;
            if (component == 2) expected = 128.0 / 255.0 + (channels[0] - luma) / 1.5748;
            if (expected < 0.0) expected = 0.0;
            if (expected > 1.0) expected = 1.0;
            error = fabs(image.planes[component][y * image.stride + x] - expected);
            if (!(error < 0.003))
            {
               fprintf(stderr, "Captured image differs: %u,%u,%u error %.9f\n", x, y, component, error);
               return 0;
            }
         }
   }
   return 1;
}

#define RPY_CAPTURE_CHECK(expr) do { if (!(expr)) { \
   fprintf(stderr, "GL capture failed line %u: %s\n", (unsigned)__LINE__, #expr); goto end; \
} } while (0)

#include "pyrowave_gl_receive_test.h"
#include "pyrowave_gl_player_test.h"
#include "pyrowave_gl_scrgb_test.h"

int main(int argc, char **argv)
{
   rpy_gl_test_t gl;
   record_pyrowave_gl_config_t config;
   record_pyrowave_gl_t *capture = NULL;
   record_pyrowave_gl_t *receive = NULL;
   rpyrowave_decoder_t *decoder = NULL;
   record_gpu_frame_t frames[5];
   record_pyrowave_packet_t packets[5];
   float *rgb = NULL;
   uint64_t ticket = 0;
   uint64_t rejected;
   uint32_t held_hash;
   GLuint texture = 0;
   unsigned programs[3] = { 0, 0, 0 };
   unsigned round;
   unsigned i;
   unsigned x;
   unsigned y;
   int success = 0;
   bool context_alive = true;
   (void)rpy_gl_setup;
   (void)rpy_gl_execute;
   (void)rpy_gpu_vector_load;
   (void)rpy_gpu_vector_compare;
   memset(&gl, 0, sizeof(gl));
   memset(&config, 0, sizeof(config));
   if (argc != 4 && argc != 5)
   {
      fprintf(stderr, "Usage: pyrowave_gl_test color.comp dwt.comp packet.comp [first-frame.pyro]\n");
      return EXIT_FAILURE;
   }
   RPY_CAPTURE_CHECK(rpy_gl_context(&gl) && rpy_gl_functions(&gl));
   rglgen_resolve_symbols(rpy_capture_get_proc);
   for (i = 0; i < 3; i++)
   {
      RPY_CAPTURE_CHECK(rpy_gl_program(&gl, argv[i + 1]));
      programs[i] = gl.program;
      gl.program = 0;
      config.programs[i] = programs[i];
   }
   rgb = (float*)malloc(134 * 104 * 4 * sizeof(float));
   decoder = rpyrowave_decoder_new(130, 98, 0, 1024 * 1024);
   RPY_CAPTURE_CHECK(rgb && decoder);
   for (y = 0; y < 104; y++)
      for (x = 0; x < 134; x++)
      {
         rgb[(y * 134 + x) * 4] = (float)x / 133.0f;
         rgb[(y * 134 + x) * 4 + 1] = (float)y / 103.0f;
         rgb[(y * 134 + x) * 4 + 2] = (float)((x * 13 + y * 7) & 255) / 255.0f;
         rgb[(y * 134 + x) * 4 + 3] = 0.25f;
      }
   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, 0x8814, 134, 104, 0, GL_RGBA, GL_FLOAT, rgb);
   config.sequence.width = 130;
   config.sequence.height = 98;
   config.get_proc = rpy_capture_get_proc;
   config.maximum_bytes = 262144;
   config.slots = 3;
   config.bit_depth = 8;
   config.texture_width = 134;
   config.texture_height = 104;
   config.x = 2;
   config.y = 3;
   rpy_capture_scaled = getenv("RPY_TEST_SCALE") != NULL;
   if (rpy_capture_scaled)
   {
      config.x = config.y = 0;
      config.scale_source = getenv("RPY_TEST_SCALE_BROKEN") == NULL;
   }
   capture = record_pyrowave_gl_new(&config);
   RPY_CAPTURE_CHECK(capture);
   config.decode = true;
   config.slots = 2;
   receive = record_pyrowave_gl_new(&config);
   RPY_CAPTURE_CHECK(receive);
   config.decode = false;
   config.slots = 3;
   rpy_capture_buffer_data = glBufferData;
   rpy_capture_buffer_storage = glBufferStorage;
   rpy_capture_map = glMapBufferRange;
   rpy_capture_wait = glClientWaitSync;
   glBufferData = rpy_capture_buffer_data_spy;
   glBufferStorage = rpy_capture_buffer_storage_spy;
   glMapBufferRange = rpy_capture_map_spy;
   glClientWaitSync = rpy_capture_wait_spy;
   for (round = 0; round < 8; round++)
   {
      rpy_capture_active = true;
      for (i = 0; i < 3; i++)
      {
         uint64_t next = ticket + 1;
         GLsync source_ready = glFenceSync(0x9117, 0);
         RPY_CAPTURE_CHECK(source_ready);
         RPY_CAPTURE_CHECK(record_pyrowave_gl_push(capture, texture, source_ready,
                  next * 16667, 16667, &ticket) == RECORD_GPU_QUEUE_OK && ticket == next);
         glDeleteSync(source_ready);
      }
      rejected = ticket;
      RPY_CAPTURE_CHECK(record_pyrowave_gl_push(capture, texture, NULL, 0, 0, &rejected)
            == RECORD_GPU_QUEUE_BUSY && rejected == ticket);
      for (i = 0; i < 3; i++)
      {
         RPY_CAPTURE_CHECK(record_pyrowave_gl_take(capture, &frames[i]) == RECORD_GPU_QUEUE_OK);
         RPY_CAPTURE_CHECK(record_pyrowave_gl_packet(capture, frames[i].slot, &packets[i]) == RECORD_GPU_QUEUE_BUSY);
         RPY_CAPTURE_CHECK(!record_pyrowave_gl_release(capture, frames[i].slot));
      }
      rpy_capture_active = false;
      RPY_CAPTURE_CHECK(!record_pyrowave_gl_free(capture));
      glFinish(); /* Test oracle only; the adapter never waits. */
      rpy_capture_active = true;
      RPY_CAPTURE_CHECK(record_pyrowave_gl_poll(capture));
      RPY_CAPTURE_CHECK(record_pyrowave_gl_source_retired(capture) == ticket);
      for (i = 0; i < 3; i++)
      {
         RPY_CAPTURE_CHECK(record_pyrowave_gl_packet(capture, frames[i].slot, &packets[i]) == RECORD_GPU_QUEUE_OK);
         RPY_CAPTURE_CHECK(rpy_capture_decode(decoder, &packets[i], &frames[i], rgb));
         if (!round && !i)
         {
            RPY_CAPTURE_CHECK(rpy_player_test(argv[1], &packets[i], decoder));
            RPY_CAPTURE_CHECK(rpy_receive_test(receive, &packets[i], decoder));
            RPY_CAPTURE_CHECK(record_pyrowave_gl_free(receive));
            receive = NULL;
         }
         if (!round && !i && argc == 5)
         {
            FILE *file = fopen(argv[4], "wb");
            size_t written;
            int closed;
            RPY_CAPTURE_CHECK(file);
            written = fwrite(packets[i].data, 1, packets[i].bytes, file);
            closed = fclose(file);
            RPY_CAPTURE_CHECK(written == packets[i].bytes && !closed);
         }
      }
      held_hash = rpy_capture_hash(&packets[0]);
      RPY_CAPTURE_CHECK(!record_pyrowave_gl_free(capture));
      RPY_CAPTURE_CHECK(record_pyrowave_gl_release(capture, frames[1].slot));
      RPY_CAPTURE_CHECK(record_pyrowave_gl_release(capture, frames[2].slot));
      RPY_CAPTURE_CHECK(!record_pyrowave_gl_release(capture, frames[2].slot));
      for (i = 3; i < 5; i++)
      {
         uint64_t next = ticket + 1;
         RPY_CAPTURE_CHECK(record_pyrowave_gl_push(capture, texture, NULL,
                  next * 16667, 16667, &ticket) == RECORD_GPU_QUEUE_OK && ticket == next);
         RPY_CAPTURE_CHECK(record_pyrowave_gl_take(capture, &frames[i]) == RECORD_GPU_QUEUE_OK);
      }
      if (round == 7)
      {
         record_gpu_frame_t end_frame;
         RPY_CAPTURE_CHECK(record_pyrowave_gl_finish(capture));
         RPY_CAPTURE_CHECK(record_pyrowave_gl_take(capture, &end_frame) == RECORD_GPU_QUEUE_END);
         RPY_CAPTURE_CHECK(!record_pyrowave_gl_free(capture));
      }
      rpy_capture_active = false;
      glFinish();
      rpy_capture_active = true;
      RPY_CAPTURE_CHECK(record_pyrowave_gl_poll(capture));
      RPY_CAPTURE_CHECK(rpy_capture_hash(&packets[0]) == held_hash);
      for (i = 3; i < 5; i++)
      {
         RPY_CAPTURE_CHECK(record_pyrowave_gl_packet(capture, frames[i].slot, &packets[i]) == RECORD_GPU_QUEUE_OK);
         RPY_CAPTURE_CHECK(rpy_capture_decode(decoder, &packets[i], &frames[i], rgb));
         RPY_CAPTURE_CHECK(record_pyrowave_gl_release(capture, frames[i].slot));
      }
      RPY_CAPTURE_CHECK(record_pyrowave_gl_release(capture, frames[0].slot));
      RPY_CAPTURE_CHECK(!rpy_capture_forbidden && glGetError() == GL_NO_ERROR);
   }
   rpy_capture_active = false;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_finish(capture));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_take(capture, &frames[0]) == RECORD_GPU_QUEUE_END);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_push(capture, texture, NULL, 0, 0, &ticket) == RECORD_GPU_QUEUE_END);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_free(capture));
   capture = NULL;
   RPY_CAPTURE_CHECK(rpy_scrgb_test(&config, texture, rgb, &capture, &receive));
   capture = record_pyrowave_gl_new(&config);
   RPY_CAPTURE_CHECK(capture);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_push(capture, texture, NULL, 0, 0, &ticket) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_take(capture, &frames[0]) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_push(capture, texture, NULL, 0, 0, &ticket) == RECORD_GPU_QUEUE_OK);
   config.decode = true;
   receive = record_pyrowave_gl_new(&config);
   config.decode = false;
   RPY_CAPTURE_CHECK(receive);
   {
      static const uint8_t neutral[8] = { 0x81, 0x40, 0x18, 0x80, 0, 0, 0, 0 };
      void *span;
      size_t capacity;
      for (i = 0; i < 2; i++)
      {
         RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
         memcpy(span, neutral, sizeof(neutral));
         RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_submit(receive, sizeof(neutral), i * 16667, 16667)
               == RECORD_GPU_QUEUE_OK);
      }
      RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
   }
   rpy_capture_fence = glFenceSync;
   glFenceSync = rpy_capture_fence_fail;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_decode(receive) == RECORD_GPU_QUEUE_ERROR);
   RPY_CAPTURE_CHECK(!record_pyrowave_gl_free(receive) && !record_pyrowave_gl_finish(receive));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_push(capture, texture, NULL, 0, 0, &ticket) == RECORD_GPU_QUEUE_ERROR);
   glFenceSync = rpy_capture_fence;
   RPY_CAPTURE_CHECK(ticket == 3 && !record_pyrowave_gl_free(capture));
   RPY_CAPTURE_CHECK(!record_pyrowave_gl_finish(capture));
   /* Failed marker creation leaves real GPU commands without their original
    * fence. A new ordered marker must retire them before healthy destruction. */
   {
      unsigned attempts;
      bool retired = false;
      for (attempts = 0; attempts < 1000000 && !retired; attempts++)
         retired = record_pyrowave_gl_retire(receive);
      RPY_CAPTURE_CHECK(retired);
      receive = NULL;
   }
   glDeleteTextures(1, &texture);
   texture = 0;
   for (i = 0; i < 3; i++) gl.api.DeleteProgram(programs[i]);
   memset(programs, 0, sizeof(programs));
   rpy_gl_free(&gl);
   context_alive = false;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_abandon(capture));
   capture = NULL;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_abandon(receive));
   receive = NULL;
   printf("OpenGL asynchronous PyroWave capture: 40 frames, bounded lends, backpressure, EOS and context loss PASS\n");
   success = 1;
end:
   rpy_capture_active = false;
   if (context_alive)
   {
      if (texture) glDeleteTextures(1, &texture);
      for (i = 0; i < 3; i++) if (programs[i]) gl.api.DeleteProgram(programs[i]);
      rpy_gl_free(&gl);
   }
   if (capture) record_pyrowave_gl_abandon(capture);
   if (receive) record_pyrowave_gl_abandon(receive);
   rpyrowave_decoder_free(decoder);
   free(rgb);
   return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
