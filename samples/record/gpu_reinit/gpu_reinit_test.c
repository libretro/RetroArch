/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Assertions are required
#endif
#include <assert.h>
#include "../../../record/record_gpu_reinit.c"
static video_driver_state_t video;
static runloop_state_t loop;
static bool native_pending, io_pending, switching;
static unsigned stops, reinits;
static int applied;
video_driver_state_t *video_state_get_ptr(void)
{
   return &video;
}
runloop_state_t *runloop_state_get_ptr(void)
{
   return &loop;
}
bool runloop_is_content_switching(void)
{
   return switching;
}
void recording_gpu_stop(void)
{
   stops++;
}
bool recording_gpu_pending(void)
{
   return native_pending || io_pending;
}
bool recording_gpu_reset_pending(void)
{
   return native_pending;
}
bool command_event_reinit(const int flags)
{
   assert(!native_pending && video.data);
   assert(!loop.quit_pending && !loop.content_closing && !switching);
   assert(!recording_gpu_defer_reinit(flags));
   applied = flags;
   reinits++;
   return true;
}
int main(void)
{
   unsigned i, before;
   video.data = &video;
   io_pending = true;
   assert(!recording_gpu_defer_reinit(DRIVER_VIDEO_MASK));
   assert(recording_gpu_pending());
   io_pending = false;
   assert(!recording_gpu_defer_reinit(DRIVER_VIDEO_MASK));
   assert(!stops);
   native_pending = true;
   assert(!recording_gpu_defer_reinit(DRIVER_AUDIO_MASK));
   assert(!stops);
   assert(recording_gpu_defer_reinit(DRIVER_VIDEO_MASK));
   assert(stops == 1);
   assert(recording_gpu_defer_reinit(DRIVER_INPUT_MASK));
   assert(recording_gpu_defer_reinit(DRIVER_AUDIO_MASK));
   for (i = 0; i < 100; i++)
      recording_gpu_reinit_poll();
   assert(!reinits && stops == 103);
   native_pending = false;
   recording_gpu_reinit_poll();
   assert(reinits == 1);
   assert(applied
         == (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK | DRIVER_INPUT_MASK));
   recording_gpu_reinit_poll();
   assert(reinits == 1);
   for (i = 0; i < 4; i++)
   {
      native_pending = true;
      assert(recording_gpu_defer_reinit(DRIVER_VIDEO_MASK));
      before = reinits;
      loop.quit_pending = i == 0;
      loop.content_closing = i == 1;
      switching = i == 2;
      video.data = i == 3 ? NULL : &video;
      recording_gpu_reinit_poll();
      loop.quit_pending = loop.content_closing = switching = false;
      video.data = &video;
      native_pending = false;
      recording_gpu_reinit_poll();
      assert(reinits == before);
   }
   assert(!recording_gpu_defer_reinit(DRIVER_VIDEO_MASK));
   return 0;
}
