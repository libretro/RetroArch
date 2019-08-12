/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Manuel Alfayate
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <bcm_host.h>

#include <rthreads/rthreads.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../driver.h"
#include "../../retroarch.h"
#include "../font_driver.h"

struct dispmanx_page
{
   /* Each page contains it's own resource handler
    * instead of pointing to in by page number */
   DISPMANX_RESOURCE_HANDLE_T resource;
   bool used;
   /* Each page has it's own mutex for
    * isolating it's used flag access. */
   slock_t *page_used_mutex;

   /* This field will allow us to access the
    * main _dispvars struct from the vsync CB function */
   struct dispmanx_video *dispvars;

   /* This field will allow us to access the
    * surface the page belongs to. */
   struct dispmanx_surface *surface;
};

struct dispmanx_surface
{
   /* main surface has 3 pages, menu surface has 1 */
   unsigned int numpages;
   struct dispmanx_page *pages;
   /* the page that's currently on screen */
   struct dispmanx_page *current_page;
   /*The page to wich we will dump the render. We need to know this
    * already when we enter the surface update function. No time to wait
    * for free pages before blitting and showing the just rendered frame! */
   struct dispmanx_page *next_page;
   unsigned int bpp;

   VC_RECT_T src_rect;
   VC_RECT_T dst_rect;
   VC_RECT_T bmp_rect;

   /* Each surface has it's own element, and the
    * resources are contained one in each page */
   DISPMANX_ELEMENT_HANDLE_T element;
   VC_DISPMANX_ALPHA_T alpha;
   VC_IMAGE_TYPE_T pixformat;

   /* Surfaces with a higher layer will be on top of
    * the ones with lower. Default is 0. */
   int layer;

   /* We need to keep this value for the blitting on
    * the surface_update function. */
   int pitch;
};

struct dispmanx_video
{
   DISPMANX_DISPLAY_HANDLE_T display;
   DISPMANX_UPDATE_HANDLE_T update;
   uint32_t vc_image_ptr;

   struct dispmanx_surface *main_surface;
   struct dispmanx_surface *menu_surface;
   struct dispmanx_surface *back_surface;

   /* For console blanking */
   int fb_fd;
   uint8_t *fb_addr;
   unsigned int screensize;
   uint8_t *screen_bck;

   /* Total dispmanx video dimensions. Not counting overscan settings. */
   unsigned int dispmanx_width;
   unsigned int dispmanx_height;

   /* For threading */
   scond_t *vsync_condition;
   slock_t *pending_mutex;
   unsigned int pageflip_pending;

   /* Menu */
   bool menu_active;

   bool rgb32;

   /* We use this to keep track of internal resolution changes
    * done by cores in the main surface or in the menu.
    * We need these outside the surface because we free surfaces
    * and then we want to test if these values have changed before
    * recreating them. */
   int core_width;
   int core_height;
   int core_pitch;
   int menu_width;
   int menu_height;
   int menu_pitch;
   /* Both main and menu surfaces are going to have the same aspect,
    * so we keep it here for future reference. */
   float aspect_ratio;
};

/* If no free page is available when called, wait for a page flip. */
static struct dispmanx_page *dispmanx_get_free_page(struct dispmanx_video *_dispvars,
      struct dispmanx_surface *surface)
{
   unsigned i;
   struct dispmanx_page *page = NULL;

   while (!page)
   {
      /* Try to find a free page */
      for (i = 0; i < surface->numpages; ++i)
      {
         if (!surface->pages[i].used)
         {
            page = (surface->pages) + i;
            break;
         }
      }

      /* If no page is free at the moment,
       * wait until a free page is freed by vsync CB. */
      if (!page)
      {
         slock_lock(_dispvars->pending_mutex);
          if (_dispvars->pageflip_pending > 0)
             scond_wait(_dispvars->vsync_condition, _dispvars->pending_mutex);
         slock_unlock(_dispvars->pending_mutex);
      }
   }

   /* We mark the choosen page as used */
   slock_lock(page->page_used_mutex);
   page->used = true;
   slock_unlock(page->page_used_mutex);

   return page;
}

static void dispmanx_vsync_callback(DISPMANX_UPDATE_HANDLE_T u, void *data)
{
   struct dispmanx_page *page = data;
   struct dispmanx_surface *surface = page->surface;

   /* Marking the page as free must be done before the signaling
    * so when update_main continues (it won't continue until we signal)
    * we can chose this page as free */
   if (surface->current_page)
   {
      slock_lock(surface->current_page->page_used_mutex);

      /* We mark as free the page that was visible until now */
      surface->current_page->used = false;
      slock_unlock(surface->current_page->page_used_mutex);
   }

   /* The page on which we issued the flip that
    * caused this callback becomes the visible one */
   surface->current_page = page;

   /* These two things must be isolated "atomically" to avoid getting
    * a false positive in the pending_mutex test in update_main. */
   slock_lock(page->dispvars->pending_mutex);

   page->dispvars->pageflip_pending--;
   scond_signal(page->dispvars->vsync_condition);

   slock_unlock(page->dispvars->pending_mutex);
}

static void dispmanx_surface_free(struct dispmanx_video *_dispvars,
      struct dispmanx_surface **sp)
{
   int i;
   struct dispmanx_surface *surface = *sp;

   /* What if we run into the vsync cb code after freeing the surface?
    * We could be trying to get non-existant lock, signal non-existant condition..
    * So we wait for any pending flips to complete before freeing any surface. */
   slock_lock(_dispvars->pending_mutex);
   if (_dispvars->pageflip_pending > 0)
      scond_wait(_dispvars->vsync_condition, _dispvars->pending_mutex);
   slock_unlock(_dispvars->pending_mutex);

   for (i = 0; i < surface->numpages; i++)
   {
      vc_dispmanx_resource_delete(surface->pages[i].resource);
      surface->pages[i].used = false;
      slock_free(surface->pages[i].page_used_mutex);
   }

   free(surface->pages);

   _dispvars->update = vc_dispmanx_update_start(0);
   vc_dispmanx_element_remove(_dispvars->update, surface->element);
   vc_dispmanx_update_submit_sync(_dispvars->update);

   free(surface);
   *sp = NULL;
}

static void dispmanx_surface_setup(struct dispmanx_video *_dispvars,
      int src_width, int src_height,
      int visible_pitch, int bpp, VC_IMAGE_TYPE_T pixformat,
      int alpha, float aspect, int numpages, int layer,
      struct dispmanx_surface **sp)
{
   int i, dst_width, dst_height, dst_xpos, dst_ypos, visible_width;
   struct dispmanx_surface *surface = NULL;

   *sp = calloc(1, sizeof(struct dispmanx_surface));

   surface = *sp;

   /* Setup surface parameters */
   surface->numpages = numpages;
   /* We receive the pitch for what we consider "useful info",
    * excluding things that are between scanlines.
    * Then we align it to 16 pixels (not bytes) for performance reasons. */
   surface->pitch = ALIGN_UP(visible_pitch, (pixformat == VC_IMAGE_XRGB8888 ? 64 : 32));

   /* Transparency disabled */
   surface->alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
   surface->alpha.opacity = alpha;
   surface->alpha.mask = 0;

   /* Allocate memory for all the pages in each surface
    * and initialize variables inside each page's struct. */
   surface->pages = calloc(surface->numpages, sizeof(struct dispmanx_page));

   for (i = 0; i < surface->numpages; i++)
   {
      surface->pages[i].used = false;
      surface->pages[i].surface = surface;
      surface->pages[i].dispvars = _dispvars;
      surface->pages[i].page_used_mutex = slock_new();
   }

   /* No need to mutex this access to the "used" member because
    * the flipping/callbacks are not still running */
   surface->next_page = &(surface->pages[0]);
   surface->next_page->used = true;

   /* The "visible" width obtained from the core pitch. We blit based on
    * the "visible" width, for cores with things between scanlines. */
   visible_width = visible_pitch / (bpp / 8);

   dst_width  = _dispvars->dispmanx_height * aspect;
   dst_height = _dispvars->dispmanx_height;

   /* If we obtain a scaled image width that is bigger than the physical screen width,
    * then we keep the physical screen width as our maximun width. */
   if (dst_width > _dispvars->dispmanx_width)
      dst_width = _dispvars->dispmanx_width;

   dst_xpos = (_dispvars->dispmanx_width - dst_width) / 2;
   dst_ypos = (_dispvars->dispmanx_height - dst_height) / 2;

   /* We configure the rects now. */
   vc_dispmanx_rect_set(&surface->dst_rect, dst_xpos, dst_ypos, dst_width, dst_height);
   vc_dispmanx_rect_set(&surface->bmp_rect, 0, 0, src_width, src_height);
   vc_dispmanx_rect_set(&surface->src_rect, 0, 0, src_width << 16, src_height << 16);

   for (i = 0; i < surface->numpages; i++)
   {
      surface->pages[i].resource = vc_dispmanx_resource_create(pixformat,
            visible_width, src_height, &(_dispvars->vc_image_ptr));
   }
   /* Add element. */
   _dispvars->update = vc_dispmanx_update_start(0);

   surface->element = vc_dispmanx_element_add(
         _dispvars->update,_dispvars->display, layer,
         &surface->dst_rect, surface->pages[0].resource,
         &surface->src_rect, DISPMANX_PROTECTION_NONE,
         &surface->alpha, 0, (DISPMANX_TRANSFORM_T)0);

   vc_dispmanx_update_submit_sync(_dispvars->update);
}

static void dispmanx_surface_update_async(const void *frame, struct dispmanx_surface *surface)
{
   struct dispmanx_page       *page = NULL;

   /* Since it's an async update, there's no need for multiple pages */
   page = &(surface->pages[0]);

   /* Frame blitting. Nothing else is needed if we only have a page. */
   vc_dispmanx_resource_write_data(page->resource, surface->pixformat,
         surface->pitch, (void*)frame, &(surface->bmp_rect));
}

static void dispmanx_surface_update(struct dispmanx_video *_dispvars, const void *frame,
      struct dispmanx_surface *surface)
{
   /* Frame blitting */
   vc_dispmanx_resource_write_data(surface->next_page->resource, surface->pixformat,
         surface->pitch, (void*)frame, &(surface->bmp_rect));

   /* Dispmanx doesn't support more than one pending pageflip. Doing so would overwrite
    * the page in the callback function, so we would be always freeing the same page. */
   slock_lock(_dispvars->pending_mutex);
   if (_dispvars->pageflip_pending > 0)
      scond_wait(_dispvars->vsync_condition, _dispvars->pending_mutex);
   slock_unlock(_dispvars->pending_mutex);

   /* Issue a page flip that will be done at the next vsync. */
   _dispvars->update = vc_dispmanx_update_start(0);

   vc_dispmanx_element_change_source(_dispvars->update, surface->element,
         surface->next_page->resource);

   slock_lock(_dispvars->pending_mutex);
   _dispvars->pageflip_pending++;
   slock_unlock(_dispvars->pending_mutex);

   vc_dispmanx_update_submit(_dispvars->update,
      dispmanx_vsync_callback, (void*)(surface->next_page));

   /* This may block waiting on a new page when max_swapchain_images <= 2 */
   surface->next_page = dispmanx_get_free_page(_dispvars, surface);
}

/* Enable/disable bilinear filtering. */
static void dispmanx_set_scaling (bool bilinear_filter)
{
   if (bilinear_filter)
      vc_gencmd_send("%s", "scaling_kernel 0 -2 -6 -8 -10 -8 -3 2 18 50 82 119 155 187 213 227 227 213 187 155 119 82 50 18 2 -3 -8 -10 -8 -6 -2 0 0");
   else
      vc_gencmd_send("%s", "scaling_kernel 0 0 0 0 0 0 0 0 1 1 1 1 255 255 255 255 255 255 255 255 1 1 1 1 0 0 0 0 0 0 0 0 1");
}

static void dispmanx_blank_console (struct dispmanx_video *_dispvars)
{
   /* Since pitch will be aligned to 16 pixels (not bytes) we use a
    * 16 pixels image to save the alignment */
   uint16_t image[16] = {0x0000};
   float aspect = (float)_dispvars->dispmanx_width / (float)_dispvars->dispmanx_height;

   dispmanx_surface_setup(_dispvars,
         16,
         1,
         32,
         16,
         VC_IMAGE_RGB565,
         255,
         aspect,
         1,
         -1,
         &_dispvars->back_surface);

   /* Updating 1-page surface synchronously asks for truble, since the 1st CB will
    * signal but not free because the only page is on screen, so get_free will wait forever. */
   dispmanx_surface_update_async(image, _dispvars->back_surface);
}

static void *dispmanx_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   struct dispmanx_video *_dispvars = calloc(1, sizeof(struct dispmanx_video));

   if (!_dispvars)
      return NULL;

   bcm_host_init();
   _dispvars->display = vc_dispmanx_display_open(0 /* LCD */);

   /* If the console framebuffer has active overscan settings,
    * the user must have overscan_scale=1 in config.txt to have
    * the same size for both fb console and dispmanx. */
   graphics_get_display_size(_dispvars->display,
         &_dispvars->dispmanx_width, &_dispvars->dispmanx_height);

   /* Setup surface parameters */
   _dispvars->vc_image_ptr     = 0;
   _dispvars->pageflip_pending = 0;
   _dispvars->menu_active      = false;
   _dispvars->rgb32            = video->rgb32;

   /* It's very important that we set aspect here because the
    * call seq when a core is loaded is gfx_init()->set_aspect()->gfx_frame()
    * and we don't want the main surface to be setup in set_aspect()
    * before we get to gfx_frame(). */
   _dispvars->aspect_ratio = video_driver_get_aspect_ratio();

   /* Initialize the rest of the mutexes and conditions. */
   _dispvars->vsync_condition  = scond_new();
   _dispvars->pending_mutex    = slock_new();
   _dispvars->core_width       = 0;
   _dispvars->core_height      = 0;
   _dispvars->menu_width       = 0;
   _dispvars->menu_height      = 0;

   _dispvars->main_surface     = NULL;
   _dispvars->menu_surface     = NULL;

   if (input && input_data)
      *input = NULL;

   /* Enable/disable dispmanx bilinear filtering. */
   dispmanx_set_scaling(video->smooth);

   dispmanx_blank_console(_dispvars);
   return _dispvars;
}

static bool dispmanx_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   struct dispmanx_video *_dispvars = data;
   float aspect = video_driver_get_aspect_ratio();

   if (!frame)
      return true;

   if (  (width != _dispvars->core_width)   ||
         (height != _dispvars->core_height) ||
         (_dispvars->aspect_ratio != aspect))
   {
      /* Sanity check. */
      if (width == 0 || height == 0)
         return true;

      _dispvars->core_width    = width;
      _dispvars->core_height   = height;
      _dispvars->core_pitch    = pitch;
      _dispvars->aspect_ratio  = aspect;

      if (_dispvars->main_surface != NULL)
         dispmanx_surface_free(_dispvars, &_dispvars->main_surface);

      /* Internal resolution or ratio has changed, so we need
       * to recreate the main surface. */
      dispmanx_surface_setup(_dispvars,
            width,
            height,
            pitch,
            _dispvars->rgb32 ? 32 : 16,
            _dispvars->rgb32 ? VC_IMAGE_XRGB8888 : VC_IMAGE_RGB565,
            255,
            _dispvars->aspect_ratio,
            video_info->max_swapchain_images,
            0,
            &_dispvars->main_surface);

      /* We need to recreate the menu surface too, if it exists already, so we
       * free it and let dispmanx_set_texture_frame() recreate it as it detects it's NULL.*/
      if (_dispvars->menu_active && _dispvars->menu_surface)
         dispmanx_surface_free(_dispvars, &_dispvars->menu_surface);
   }

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   /* Update main surface: locate free page, blit and flip. */
   dispmanx_surface_update(_dispvars, frame, _dispvars->main_surface);
   return true;
}

static void dispmanx_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct dispmanx_video *_dispvars = data;

   /* If it was active but it's not anymore... */
   if (!state && _dispvars->menu_active)
      dispmanx_surface_free(_dispvars, &_dispvars->menu_surface);

   _dispvars->menu_active = state;
}

static void dispmanx_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   struct dispmanx_video *_dispvars = data;

   if (!_dispvars->menu_active)
      return;

   /* If menu is active in this frame but our menu surface is NULL, we allocate a new one.*/
   if (_dispvars->menu_surface == NULL)
   {
      _dispvars->menu_width  = width;
      _dispvars->menu_height = height;
      _dispvars->menu_pitch  = width * (rgb32 ? 4 : 2);

      /* Menu surface only needs a page as it will be updated asynchronously. */
      dispmanx_surface_setup(_dispvars,
            width,
            height,
            _dispvars->menu_pitch,
            16,
            VC_IMAGE_RGBA16,
            210,
            _dispvars->aspect_ratio,
            1,
            0,
            &_dispvars->menu_surface);
   }

   /* We update the menu surface if menu is active.
    * This update is asynchronous, yet menu screen update
    * will be synced because main surface updating is synchronous */
   dispmanx_surface_update_async(frame, _dispvars->menu_surface);
}

static void dispmanx_gfx_set_nonblock_state(void *data, bool state)
{
   struct dispmanx_video *vid = data;

   (void)data;
   (void)vid;

   /* TODO */
}

static bool dispmanx_gfx_alive(void *data)
{
   (void)data;
   return true; /* always alive */
}

static bool dispmanx_gfx_focus(void *data)
{
   (void)data;
   return true; /* fb device always has focus */
}

static void dispmanx_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   struct dispmanx_video *vid = data;

   if (!vid)
      return;

   vp->x = vp->y = 0;

   vp->width  = vp->full_width  = vid->core_width;
   vp->height = vp->full_height = vid->core_height;
}

static bool dispmanx_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static bool dispmanx_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static uint32_t dispmanx_get_flags(void *data)
{
   uint32_t             flags = 0;

   return flags;
}

static const video_poke_interface_t dispmanx_poke_interface = {
   dispmanx_get_flags,
   NULL,
   NULL,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* dispmanx_apply_state_changes */
   dispmanx_set_texture_frame,
   dispmanx_set_texture_enable,
   NULL,                         /* set_osd_msg */
   NULL,                         /* show_mouse */
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL                          /* get_hw_render_interface */
};

static void dispmanx_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &dispmanx_poke_interface;
}

static void dispmanx_gfx_free(void *data)
{
   struct dispmanx_video *_dispvars = data;

   if (!_dispvars)
      return;

   dispmanx_surface_free(_dispvars, &_dispvars->main_surface);
   dispmanx_surface_free(_dispvars, &_dispvars->back_surface);

   if (_dispvars->menu_surface)
      dispmanx_surface_free(_dispvars, &_dispvars->menu_surface);

   /* Close display and deinitialize. */
   vc_dispmanx_display_close(_dispvars->display);
   bcm_host_deinit();

   /* Destroy mutexes and conditions. */
   slock_free(_dispvars->pending_mutex);
   scond_free(_dispvars->vsync_condition);

   free(_dispvars);
}

video_driver_t video_dispmanx = {
   dispmanx_gfx_init,
   dispmanx_gfx_frame,
   dispmanx_gfx_set_nonblock_state,
   dispmanx_gfx_alive,
   dispmanx_gfx_focus,
   dispmanx_gfx_suppress_screensaver,
   NULL, /* has_windowed */
   dispmanx_gfx_set_shader,
   dispmanx_gfx_free,
   "dispmanx",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   dispmanx_gfx_viewport_info,
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   dispmanx_gfx_get_poke_interface
};
