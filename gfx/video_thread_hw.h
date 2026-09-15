/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - libretroadmin
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Hardware-rendered cores under the threaded video wrapper.
 *
 * A hardware core renders with the video driver's own device. On the
 * unthreaded path the swapchain is the core's clock: the core asks
 * which swapchain slot the driver is on, renders into its image for
 * that slot, and the driver's swap has already waited that slot's fence
 * before retro_run was ever called. Under the wrapper the video thread
 * advances the swapchain slot during acquire and present, so a core on
 * the main thread reading it would render into an image the GPU may
 * still be presenting.
 *
 * So the wrapper gives the core a sync index space of its own: a ring
 * of VIDEO_THREAD_HW_RING slots, each holding a copy of what the core
 * hands over for one frame - image, semaphores, command buffers - and a
 * fence the video thread signals once the driver has been driven with
 * that slot. The core's get_sync_index is the ring index; its
 * wait_sync_index waits that slot's fence; the frame push publishes the
 * slot the core just filled and waits the one it will fill next, so a
 * core that never calls wait_sync_index (it was a no-op before) is
 * still safe. The video thread installs a slot's state into the driver
 * through a poke before the frame call and fence-signals after it.
 *
 * The interface handed to the core is the driver's own with the
 * bookkeeping calls redirected here; queue locking and the instance
 * proc address forward to the driver, which already serialises queue
 * use because cores may submit from other threads. Vulkan only, and
 * only when the driver provides the hw_ring pokes. */

#ifndef RARCH_VIDEO_THREAD_HW_H__
#define RARCH_VIDEO_THREAD_HW_H__

#include <boolean.h>
#include <retro_common_api.h>

#include "video_driver.h"

RETRO_BEGIN_DECLS

#ifdef HAVE_THREADS

#define VIDEO_THREAD_HW_RING 3

struct thread_video;

/* thread_poke.get_hw_render_interface: hands the core the wrapper's
 * interposed interface when the driver's is Vulkan and the ring can be
 * set up, else declines. Main thread. */
bool video_thread_get_hw_render_interface(void *data,
      const struct retro_hw_render_interface **iface);

/* Main thread, at a push with RETRO_HW_FRAME_BUFFER_VALID: returns the
 * HW slot to publish, advancing the core's index and waiting the slot
 * the core will fill next; -1 if there is no ring. */
int video_thread_hw_publish(struct thread_video *thr);

/* Video thread, around the driver's frame call for a slot the push
 * published: install the slot's state before, fence-signal after. */
void video_thread_hw_before_frame(struct thread_video *thr, int hw_slot);
void video_thread_hw_after_frame(struct thread_video *thr, int hw_slot);

/* OpenGL: takes the core's context on the calling (main) thread and
 * sets the ring up. Must run before the core's context_reset. */
bool video_thread_hw_bind_core_context(void *data);

/* thread_poke.get_current_framebuffer for OpenGL cores. Main thread. */
uintptr_t video_thread_hw_get_current_framebuffer(void *data);

/* Video thread, before the driver is freed: releases the ring and its
 * fences while the device is still up. */
void video_thread_hw_free(struct thread_video *thr);

/* Whether a hardware core may run under the wrapper at all in this
 * session: the setting, a Vulkan hw context, and the vulkan driver. Read
 * where the threaded decision is made. */
bool video_thread_hw_allowed(void);

#endif /* HAVE_THREADS */

RETRO_END_DECLS

#endif
