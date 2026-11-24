/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PYROWAVE_GL_RECEIVE_TEST_H
#define PYROWAVE_GL_RECEIVE_TEST_H

static double rpy_receive_plane(const rpyrowave_image_t *image, int x, int y, unsigned component)
{
   unsigned shift = component && !image->sequence.chroma_444 ? 1 : 0;
   double value;
   if (x < 0) x = 0;
   if (y < 0) y = 0;
   if ((unsigned)x >= image->sequence.width >> shift) x = (int)(image->sequence.width >> shift) - 1;
   if ((unsigned)y >= image->sequence.height >> shift) y = (int)(image->sequence.height >> shift) - 1;
   value = image->planes[component][y * image->stride + x];
   return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

static double rpy_receive_chroma(const rpyrowave_image_t *image, unsigned x, unsigned y, unsigned component)
{
   double cx = x * 0.5 - 0.25;
   double cy = y * 0.5 - 0.25;
   int ix = (int)floor(cx);
   int iy = (int)floor(cy);
   double wx = cx - ix;
   double wy = cy - iy;
   return (1.0 - wy) * ((1.0 - wx) * rpy_receive_plane(image, ix, iy, component)
         + wx * rpy_receive_plane(image, ix + 1, iy, component))
      + wy * ((1.0 - wx) * rpy_receive_plane(image, ix, iy + 1, component)
         + wx * rpy_receive_plane(image, ix + 1, iy + 1, component));
}

static int rpy_receive_copy(void *span, const record_pyrowave_packet_t *packet, unsigned sequence)
{
   uint8_t *bytes = (uint8_t*)span;
   size_t offset = 8;
   memcpy(bytes, packet->data, packet->bytes);
   bytes[3] = (uint8_t)((bytes[3] & 0x8f) | (sequence << 4));
   while (offset < packet->bytes)
   {
      rpyrowave_block_t block;
      if (rpyrowave_read_block(bytes + offset, packet->bytes - offset, &block)) return 0;
      bytes[offset + 3] = (uint8_t)((bytes[offset + 3] & 0x8f) | (sequence << 4));
      offset += block.bytes;
   }
   return offset == packet->bytes;
}

static int rpy_receive_test(record_pyrowave_gl_t *receive, const record_pyrowave_packet_t *packet,
      rpyrowave_decoder_t *cpu)
{
   static const uint8_t missing_sign[12] = { 1, 0, 3, 0, 128, 0, 0, 0, 1, 0, 0, 255 };
   record_pyrowave_gl_image_t images[2];
   record_pyrowave_gl_image_t extra;
   record_gpu_frame_t dropped;
   rpyrowave_image_t expected;
   float *pixels = (float*)malloc(130 * 98 * 4 * sizeof(float));
   void *span;
   size_t capacity;
   unsigned i, x, y, sequence;
   double maximum = 0.0;
   int success = 0;
   RPY_CAPTURE_CHECK(pixels && !rpyrowave_decode_frame(cpu, packet->data, packet->bytes, 0, &expected));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(capacity >= packet->bytes && !record_pyrowave_gl_finish(receive));
   memcpy(span, packet->data, packet->bytes);
   ((uint8_t*)span)[7] ^= 0x10;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_submit(receive, packet->bytes, 0, 16667) == RECORD_GPU_QUEUE_ERROR);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_cancel(receive));
   for (i = 0; i < 2; i++)
   {
      RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
      RPY_CAPTURE_CHECK(rpy_receive_copy(span, packet, i));
      RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_submit(receive, packet->bytes,
               ((uint64_t)1 << 40) + i * 16667, 16667) == RECORD_GPU_QUEUE_OK);
   }
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_BUSY);
   for (i = 0; i < 2; i++) RPY_CAPTURE_CHECK(record_pyrowave_gl_decode(receive) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_image(receive, &extra) == RECORD_GPU_QUEUE_BUSY);
   RPY_CAPTURE_CHECK(!record_pyrowave_gl_free(receive));
   rpy_capture_active = false;
   glFinish(); /* Offline numerical oracle only. */
   rpy_capture_active = true;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_poll(receive));
   for (i = 0; i < 2; i++)
   {
      RPY_CAPTURE_CHECK(record_pyrowave_gl_image(receive, &images[i]) == RECORD_GPU_QUEUE_OK);
      RPY_CAPTURE_CHECK(images[i].ready_sync);
      glWaitSync((GLsync)images[i].ready_sync, 0, (GLuint64)-1);
      RPY_CAPTURE_CHECK(images[i].frame.timestamp == ((uint64_t)1 << 40) + i * 16667
            && images[i].frame.duration == 16667 && images[i].frame.ready_value == i + 1);
      glBindTexture(GL_TEXTURE_2D, images[i].texture);
      glMemoryBarrier(0x100); /* Offline texture readback visibility. */
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels);
      for (y = 0; y < 98; y++)
         for (x = 0; x < 130; x++)
         {
            double luma = rpy_receive_plane(&expected, (int)x, (int)y, 0);
            double cb = rpy_receive_chroma(&expected, x, y, 1) - 128.0 / 255.0;
            double cr = rpy_receive_chroma(&expected, x, y, 2) - 128.0 / 255.0;
            double rgb[3];
            unsigned component;
            rgb[0] = luma + cr * 1.5748;
            rgb[2] = luma + cb * 1.8556;
            rgb[1] = (luma - 0.2126 * rgb[0] - 0.0722 * rgb[2]) / 0.7152;
            for (component = 0; component < 3; component++)
            {
               double value = rgb[component] < 0.0 ? 0.0 : (rgb[component] > 1.0 ? 1.0 : rgb[component]);
               double error = fabs(value - pixels[(y * 130 + x) * 4 + component]);
               if (error > maximum) maximum = error;
               RPY_CAPTURE_CHECK(error < 0.004);
            }
            RPY_CAPTURE_CHECK(pixels[(y * 130 + x) * 4 + 3] == 1.0f);
         }
   }
   RPY_CAPTURE_CHECK(record_pyrowave_gl_poll(receive));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_BUSY);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_present(receive, images[0].frame.slot, NULL));
   RPY_CAPTURE_CHECK(!record_pyrowave_gl_image_drop(receive, images[0].frame.slot));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_BUSY);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_image_drop(receive, images[1].frame.slot));
   RPY_CAPTURE_CHECK(!record_pyrowave_gl_image_drop(receive, images[1].frame.slot));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
   memcpy(span, packet->data, 8);
   ((uint8_t*)span)[3] &= 0x8f;
   ((uint8_t*)span)[4] = 1;
   ((uint8_t*)span)[5] = ((uint8_t*)span)[6] = 0;
   memcpy((uint8_t*)span + 8, missing_sign, sizeof(missing_sign));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_submit(receive, 20, 0, 16667) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_decode(receive) == RECORD_GPU_QUEUE_OK);
   rpy_capture_active = false;
   glFinish();
   rpy_capture_active = true;
   RPY_CAPTURE_CHECK(!record_pyrowave_gl_poll(receive));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_image(receive, &extra) == RECORD_GPU_QUEUE_BUSY);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
   memcpy(span, packet->data, packet->bytes);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_submit(receive, packet->bytes, 0, 16667) == RECORD_GPU_QUEUE_OK);
   RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_drop(receive, &dropped) == RECORD_GPU_QUEUE_OK);
   for (sequence = 2; sequence < 10; sequence++)
   {
      RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_acquire(receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
      RPY_CAPTURE_CHECK(rpy_receive_copy(span, packet, sequence & 7));
      RPY_CAPTURE_CHECK(record_pyrowave_gl_receive_submit(receive, packet->bytes, sequence * 16667, 16667)
            == RECORD_GPU_QUEUE_OK);
      RPY_CAPTURE_CHECK(record_pyrowave_gl_decode(receive) == RECORD_GPU_QUEUE_OK);
      rpy_capture_active = false;
      glFinish();
      rpy_capture_active = true;
      RPY_CAPTURE_CHECK(record_pyrowave_gl_poll(receive));
      RPY_CAPTURE_CHECK(record_pyrowave_gl_image(receive, &extra) == RECORD_GPU_QUEUE_OK);
      if (sequence < 9) RPY_CAPTURE_CHECK(record_pyrowave_gl_image_drop(receive, extra.frame.slot));
   }
   RPY_CAPTURE_CHECK(record_pyrowave_gl_finish(receive));
   RPY_CAPTURE_CHECK(record_pyrowave_gl_decode(receive) == RECORD_GPU_QUEUE_END);
   RPY_CAPTURE_CHECK(!record_pyrowave_gl_free(receive));
   {
      GLsync display = glFenceSync(0x9117, 0);
      RPY_CAPTURE_CHECK(display);
      glFlush();
      RPY_CAPTURE_CHECK(record_pyrowave_gl_present(receive, extra.frame.slot, display));
      glDeleteSync(display);
   }
   RPY_CAPTURE_CHECK(!record_pyrowave_gl_free(receive));
   rpy_capture_active = false;
   glFinish();
   rpy_capture_active = true;
   RPY_CAPTURE_CHECK(record_pyrowave_gl_poll(receive));
   printf("OpenGL asynchronous receive: direct spans, display retirement, malformed payload; RGB max %.9f PASS\n", maximum);
   success = 1;
end:
   free(pixels);
   return success;
}
#endif
