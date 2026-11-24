/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PYROWAVE_GL_SCRGB_TEST_H
#define PYROWAVE_GL_SCRGB_TEST_H

static int rpy_scrgb_test(const record_pyrowave_gl_config_t *source, GLuint texture,
      float *pixels, record_pyrowave_gl_t **capture, record_pyrowave_gl_t **receive)
{
   /* BT.2020 linear (12.5,0,0), expressed in BT.709 scRGB. */
   static const float original[3] = { 20.756137525f, -1.556880938f, -0.226884538f };
   record_pyrowave_gl_config_t config = *source;
   record_gpu_frame_t frame;
   record_pyrowave_packet_t packet;
   record_pyrowave_gl_image_t image;
   void *span;
   size_t capacity;
   uint64_t ticket;
   unsigned i;
   double maximum = 0.0;
   int success = 0;
   rpy_capture_active = false;
   config.texture_mode = 2;
   RPY_CAPTURE_CHECK(!record_pyrowave_gl_new(&config));
   config.texture_mode = RPYROWAVE_COLOR_SCRGB;
   config.bit_depth = 10;
   config.sequence.primaries_bt2020 = config.sequence.transfer_pq = config.sequence.transform_bt2020 = 1;
   *capture = record_pyrowave_gl_new(&config);
   config.decode = true;
   *receive = record_pyrowave_gl_new(&config);
   RPY_CAPTURE_CHECK(*capture && *receive);
   for (i = 0; i < 134 * 104 * 4; i++) pixels[i] = i % 4 == 3 ? 1.0f : original[i % 4];
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 134, 104, GL_RGBA, GL_FLOAT, pixels);
   rpy_capture_active = true;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_push(*capture, texture, NULL, 0, 16667, &ticket) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_take(*capture, &frame) == RECORD_GPU_QUEUE_OK);
   rpy_capture_active = false;
   glFinish(); /* Offline end-to-end numerical oracle. */
   rpy_capture_active = true;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_poll(*capture));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_packet(*capture, frame.slot, &packet) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(*receive, &span, &capacity) == RECORD_GPU_QUEUE_OK
         && capacity >= packet.bytes);
   memcpy(span, packet.data, packet.bytes);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_submit(*receive, packet.bytes, 0, 16667) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_decode(*receive) == RECORD_GPU_QUEUE_OK);
   rpy_capture_active = false;
   glFinish();
   rpy_capture_active = true;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_poll(*receive));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_image(*receive, &image) == RECORD_GPU_QUEUE_OK);
   glBindTexture(GL_TEXTURE_2D, image.texture);
   glMemoryBarrier(0x100);
   glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels);
   for (i = 0; i < 130 * 98 * 4; i++)
   {
      double error;
      if (i % 4 == 3)
      {
         RPY_CAPTURE_CHECK(pixels[i] == 1.0f);
         continue;
      }
      error = fabs(pixels[i] - original[i % 4]);
      if (error > maximum) maximum = error;
      RPY_CAPTURE_CHECK(error < 0.02 + fabs(original[i % 4]) * 0.003);
   }
   RPY_CAPTURE_CHECK(record_pyrowave_gl_image_drop(*receive, image.frame.slot));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_release(*capture, frame.slot));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_finish(*capture) && record_pyrowave_gl_finish(*receive));
   rpy_capture_active = false;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_free(*capture));
   *capture = NULL;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_free(*receive));
   *receive = NULL;
   printf("OpenGL asynchronous scRGB/PQ/BT.2020 roundtrip, signed HDR max %.9f PASS\n", maximum);
   success = 1;
end:
   rpy_capture_active = false;
   return success;
}
#endif
