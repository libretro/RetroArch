/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PYROWAVE_GL_PLAYER_TEST_H
#define PYROWAVE_GL_PLAYER_TEST_H
#include "../../../record/record_pyrowave_player_gl.h"

static int rpy_player_test(const char *shader, const record_pyrowave_packet_t *packet,
      rpyrowave_decoder_t *cpu)
{
   record_pyrowave_source_config_t config;
   record_pyrowave_player_gl_t *player = NULL;
   record_pyrowave_video_t video;
   record_pyrowave_gl_image_t image;
   rpyrowave_image_t expected;
   char directory[1024];
   const char *slash = strrchr(shader, '/');
   const char *backslash = strrchr(shader, '\\');
   float *pixels = NULL;
   void *span;
   size_t capacity, length;
   unsigned x, y;
   GLint program, restored;
   bool active = rpy_capture_active;
   bool lent = false;
   int success = 0;
   memset(&config, 0, sizeof(config)); memset(&video, 0, sizeof(video));
   memset(&image, 0, sizeof(image));
   if (backslash && (!slash || backslash > slash)) slash = backslash;
   length = slash ? (size_t)(slash - shader) : 1;
   if (!length) length = 1;
   RPY_CAPTURE_CHECK(length < sizeof(directory));
   memcpy(directory, slash ? shader : ".", length); directory[length] = '\0';
   config.directory = directory; config.gl_proc = rpy_capture_get_proc;
   config.stream.sequence.width = 130; config.stream.sequence.height = 98;
   config.stream.bit_depth = 8; config.stream.maximum_frame_bytes = 1048576;
   config.slots = 2;
   rpy_capture_active = false;
   pixels = (float*)malloc(130 * 98 * 4 * sizeof(float));
   player = record_pyrowave_player_gl_new(&config);
   RPY_CAPTURE_CHECK(player && pixels);
   record_pyrowave_player_gl_video(player, &video);
   RPY_CAPTURE_CHECK(!rpyrowave_decode_frame(cpu, packet->data, packet->bytes, 0, &expected));
   rpy_capture_active = true;
   RPY_CAPTURE_CHECK(video.acquire(video.user, &span, &capacity) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(capacity >= packet->bytes);
   memcpy(span, packet->data, packet->bytes);
   RPY_CAPTURE_CHECK(video.submit(video.user, packet->bytes, 123000, 16667));
   glGetIntegerv(GL_CURRENT_PROGRAM, &program);
   RPY_CAPTURE_CHECK(record_pyrowave_player_gl_decode(player) == RECORD_GPU_QUEUE_OK);
   glGetIntegerv(GL_CURRENT_PROGRAM, &restored);
   RPY_CAPTURE_CHECK(restored == program);
   rpy_capture_active = false; glFinish(); rpy_capture_active = true;
   RPY_CAPTURE_CHECK(record_pyrowave_player_gl_poll(player));
   RPY_CAPTURE_CHECK(record_pyrowave_player_gl_image(player, &image) == RECORD_GPU_QUEUE_OK);
   lent = true;
   RPY_CAPTURE_CHECK(image.frame.timestamp == 123000 && image.frame.duration == 16667);
   RPY_CAPTURE_CHECK(video.finish(video.user));
   RPY_CAPTURE_CHECK(!record_pyrowave_player_gl_free(player));
   glBindTexture(GL_TEXTURE_2D, image.texture);
   glMemoryBarrier(0x100);
   glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels);
   for (y = 0; y < 98; y++)
      for (x = 0; x < 130; x++)
      {
         double luma = rpy_receive_plane(&expected, (int)x, (int)y, 0);
         double cb = rpy_receive_chroma(&expected, x, y, 1) - 128.0 / 255.0;
         double cr = rpy_receive_chroma(&expected, x, y, 2) - 128.0 / 255.0;
         double rgb[3];
         unsigned k;
         rgb[0] = luma + cr * 1.5748; rgb[2] = luma + cb * 1.8556;
         rgb[1] = (luma - 0.2126 * rgb[0] - 0.0722 * rgb[2]) / 0.7152;
         for (k = 0; k < 3; k++)
         {
            double value = rgb[k] < 0.0 ? 0.0 : (rgb[k] > 1.0 ? 1.0 : rgb[k]);
            RPY_CAPTURE_CHECK(fabs(value - pixels[(y * 130 + x) * 4 + k]) < 0.004);
         }
      }
   RPY_CAPTURE_CHECK(record_pyrowave_player_gl_present(player, image.frame.slot));
   lent = false;
   rpy_capture_active = false; glFinish(); rpy_capture_active = true;
   RPY_CAPTURE_CHECK(record_pyrowave_player_gl_poll(player) && record_pyrowave_player_gl_idle(player));
   RPY_CAPTURE_CHECK(record_pyrowave_player_gl_free(player)); player = NULL;
   printf("OpenGL player factory: negotiated decoder, direct spans, RGB and retirement PASS\n");
   success = 1;
end:
   rpy_capture_active = false;
   if (player)
   {
      record_gpu_frame_t dropped;
      unsigned i;
      glFinish(); record_pyrowave_player_gl_poll(player);
      if (lent) record_pyrowave_player_gl_drop(player, image.frame.slot);
      if (video.user) { video.cancel(video.user); video.finish(video.user); }
      for (i = 0; i < config.slots; i++)
         if (record_pyrowave_player_gl_discard(player, &dropped) != RECORD_GPU_QUEUE_OK) break;
      record_pyrowave_player_gl_free(player);
   }
   free(pixels); rpy_capture_active = active;
   return success;
}
#endif
