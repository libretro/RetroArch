/*  RetroArch - A frontend for libretro.
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

/* The camera's live picture in a corner over running content
 * (Settings > Camera > Picture-in-Picture). The camera preview feeds a
 * live thumbnail on the main thread (gfx_widget_camera_pip_update):
 * the core's own camera as the core polls it, or a camera of the
 * preview's own while the core holds none. Shown while content runs
 * and the menu is closed; the menu's own preview has the camera while
 * it is open. The widget's frame only draws, in the widgets' pass, or
 * in that pass drawn for it alone while the widgets are off
 * (video_frame_info_t::camera_pip_only). */

#include <retro_miscellaneous.h>
#include <retro_atomic.h>
#include <formats/image.h>
#include <formats/image_yuv_blit.h>

#include "../gfx_widgets.h"
#include "../gfx_display.h"
#include "../video_driver.h"
#include "../gfx_thumbnail.h"
#include "../video_defines.h"
#include "../../camera/camera_driver.h"
#include "../../configuration.h"
#include "../../runloop.h"
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

/* What the frame draws, published by the main thread: 0 for nothing,
 * else CAMERA_PIP_SHOW | corner << 1 | size percent << 3 */
#define CAMERA_PIP_SHOW 1

typedef struct gfx_widget_camera_pip_state
{
   gfx_thumbnail_t thumbnail;
   retro_atomic_int_t layout;
   bool live; /* the preview is open, feeding the thumbnail; main thread */
} gfx_widget_camera_pip_state_t;

static gfx_widget_camera_pip_state_t p_w_camera_pip_st;

static void gfx_widget_camera_pip_planar(void *user,
      const struct retro_camera_planar_frame *f)
{
   unsigned yuv = 0;
   if (f->flags & RETRO_CAMERA_PLANAR_FULL_RANGE)
      yuv |= IMAGE_YUV_FLAG_FULL_RANGE;
   if (f->flags & RETRO_CAMERA_PLANAR_BT709)
      yuv |= IMAGE_YUV_FLAG_BT709;
   if (VIDEO_SCALE_FITS(f->width, f->height))
      gfx_thumbnail_live_planes((gfx_thumbnail_t*)user, f->planes,
            f->strides, (unsigned)f->chroma_step,
            VIDEO_SCALE_PACK(f->width, f->height), yuv);
}

static void gfx_widget_camera_pip_raw(void *user, const uint32_t *buf,
      unsigned width, unsigned height, size_t pitch)
{
   if (VIDEO_SCALE_FITS(width, height))
      gfx_thumbnail_live_pixels((gfx_thumbnail_t*)user, buf,
            VIDEO_SCALE_PACK(width, height), pitch);
}

/* The thumbnail reset, by us or by another live thumbnail beginning:
 * the camera goes with it */
static void gfx_widget_camera_pip_end(void *user)
{
   gfx_widget_camera_pip_state_t *state = (gfx_widget_camera_pip_state_t*)user;
   retro_atomic_store_release_int(&state->layout, 0);
   if (!state->live)
      return;
   state->live = false;
   camera_driver_preview_close();
}

void gfx_widget_camera_pip_stop(void)
{
   gfx_widget_camera_pip_state_t *state = &p_w_camera_pip_st;
   retro_atomic_store_release_int(&state->layout, 0);
   if (state->live)
      gfx_thumbnail_reset(&state->thumbnail);
}

void gfx_widget_camera_pip_update(bool widgets_active)
{
   gfx_widget_camera_pip_state_t *state = &p_w_camera_pip_st;
   settings_t *settings                 = config_get_ptr();
   /* The widgets off, the driver still draws their pass for it */
   bool wanted                          = settings
         && settings->bools.camera_pip && settings->bools.camera_allow
         && (runloop_state_get_ptr()->flags & RUNLOOP_FLAG_CORE_RUNNING)
         && (widgets_active || video_driver_widgets_supported());
   enum camera_preview_state cam;

#ifdef HAVE_MENU
   /* The menu shows the camera its own way */
   if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
      wanted = false;
#endif
   if (!wanted)
   {
      gfx_widget_camera_pip_stop();
      return;
   }

   if (!state->live)
   {
      camera_preview_sink_t sink;
      /* Ends any other live thumbnail, and the preview with it */
      gfx_thumbnail_live_begin(&state->thumbnail,
            gfx_widget_camera_pip_end, state);
      sink.planar = gfx_widget_camera_pip_planar;
      sink.raw    = gfx_widget_camera_pip_raw;
      sink.user   = &state->thumbnail;
      camera_driver_preview_open(&sink);
      state->live = true;
   }

   switch ((cam = camera_driver_preview_poll()))
   {
      case CAMERA_PREVIEW_OWN:
      case CAMERA_PREVIEW_SHARED:
         retro_atomic_store_release_int(&state->layout, (int)(CAMERA_PIP_SHOW
               | ((settings->uints.camera_pip_corner & 3) << 1)
               | (MIN(settings->uints.camera_pip_size, 100) << 3)));
         break;
      case CAMERA_PREVIEW_OFF:
         /* Closed under us (the menu went): opened again next frame */
         gfx_widget_camera_pip_stop();
         break;
      default:
         /* The core's camera stopped, or none would open */
         retro_atomic_store_release_int(&state->layout, 0);
         break;
   }
}

static void gfx_widget_camera_pip_frame(void *data, void *user_data)
{
   gfx_widget_camera_pip_state_t *state = &p_w_camera_pip_st;
   video_frame_info_t *video_info       = (video_frame_info_t*)data;
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)user_data;
   int layout       = retro_atomic_load_acquire_int(&state->layout);
   unsigned thumb   = state->thumbnail.dims;
   unsigned vw      = VIDEO_SCALE_W(video_info->dims);
   unsigned vh      = VIDEO_SCALE_H(video_info->dims);
   unsigned tw      = VIDEO_SCALE_W(thumb);
   unsigned th      = VIDEO_SCALE_H(thumb);
   /* The widgets' padding, or about as much without them */
   unsigned margin  = p_dispwidget ? p_dispwidget->simple_widget_padding
      : vh / 54;
   unsigned w, h;
   int x, y;

   if (     !(layout & CAMERA_PIP_SHOW) || !tw || !th
         || retro_atomic_load_acquire_int(&state->thumbnail.status)
            != GFX_THUMBNAIL_STATUS_AVAILABLE)
      return;
   /* The picture's height a share of the screen's, at the camera's
    * aspect and no wider than half the screen, in the corner asked for */
   h = vh * ((unsigned)layout >> 3) / 100;
   w = (unsigned)(((uint64_t)h * tw) / th);
   if (w > vw / 2)
   {
      w = vw / 2;
      h = (unsigned)(((uint64_t)w * th) / tw);
   }
   if (!w || !h || w + 2 * margin > vw || h + 2 * margin > vh)
      return;
   x = (layout & (1 << 1)) ? (int)(vw - w - margin) : (int)margin;
   y = (layout & (2 << 1)) ? (int)(vh - h - margin) : (int)margin;
   gfx_thumbnail_draw(video_info->userdata, video_info->dims,
         &state->thumbnail, (float)x, (float)y, VIDEO_SCALE_PACK(w, h),
         GFX_THUMBNAIL_ALIGN_CENTRE, 1.0f, 1.0f, NULL);
}

bool gfx_widget_camera_pip_showing(void)
{
   return (retro_atomic_load_acquire_int(&p_w_camera_pip_st.layout)
         & CAMERA_PIP_SHOW) != 0;
}

static bool gfx_widget_camera_pip_visible(void)
{
   return gfx_widget_camera_pip_showing();
}

const gfx_widget_t gfx_widget_camera_pip = {
   NULL, /* init */
   NULL, /* free: the main thread stops the camera */
   NULL, /* context_reset */
   NULL, /* context_destroy */
   NULL, /* layout */
   NULL, /* iterate: on the video thread under the threaded wrapper */
   gfx_widget_camera_pip_frame,
   gfx_widget_camera_pip_visible
};
