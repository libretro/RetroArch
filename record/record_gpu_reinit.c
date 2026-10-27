/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_driver.h"
#include "../command.h"
#include "../driver.h"
#include "../gfx/video_driver.h"
#include "../runloop.h"

static int recording_reinit_flags;

bool recording_gpu_defer_reinit(int flags)
{
   if (!recording_reinit_flags
         && (!(flags & DRIVER_VIDEO_MASK) || !recording_gpu_reset_pending()))
      return false;
   recording_reinit_flags |= flags;
   recording_gpu_stop();
   return true;
}

void recording_gpu_reinit_poll(void)
{
   int flags;
   if (!recording_reinit_flags)
      return;
   if (runloop_state_get_ptr()->quit_pending
         || runloop_state_get_ptr()->content_closing
         || runloop_is_content_switching() || !video_state_get_ptr()->data)
   {
      recording_reinit_flags = 0;
      return;
   }
   recording_gpu_stop();
   if (recording_gpu_reset_pending())
      return;
   flags = recording_reinit_flags;
   recording_reinit_flags = 0;
   command_event_reinit(flags);
}
