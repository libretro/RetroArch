/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <string.h>
#include "../../../record/record_android_capture_ops.h"
static unsigned calls, missing;
static void resolved(void)
{
}
static function_t resolve(void *context, const char *name)
{
   assert(context == &calls);
   assert(name && (name[0] == 'e' || name[0] == 'g'));
   calls++;
   return calls == missing ? NULL : resolved;
}
int main(void)
{
   static record_gpu_mediacodec_config_t config, expected, zero;
   unsigned count, i;
   config.width = 1920;
   config.height = 1080;
   config.count = 3;
   config.mode = 2;
   config.flip_y = true;
   config.display = (EGLDisplay)&calls;
   config.config = (EGLConfig)&config;
   config.share = (EGLContext)&expected;
   config.buffers[0] = (struct AHardwareBuffer *)&zero;
   expected = config;
   assert(!record_android_capture_load_ops(NULL, resolve, &calls));
   assert(!calls);
   assert(record_android_capture_load_ops(&config, resolve, &calls));
   count = calls;
   assert(count > 0);
   assert(config.surface.swap == (PFNEGLSWAPBUFFERSPROC)resolved);
   assert(config.capture.image_target
         == (record_capture_gl_image_target_t)resolved);
   assert(config.draw.draw == (PFNGLDRAWARRAYSPROC)resolved);
   for (i = 1; i <= count; i++)
   {
      calls = 0;
      missing = i;
      assert(!record_android_capture_load_ops(&config, resolve, &calls));
      assert(calls == i);
      assert(!memcmp(&config, &expected, sizeof(config)));
      missing = 0;
      calls = 0;
      assert(record_android_capture_load_ops(&config, resolve, &calls));
      assert(calls == count);
   }
   assert(!record_android_capture_load_ops(&config, NULL, NULL));
   assert(!memcmp(&config, &expected, sizeof(config)));
   return 0;
}
