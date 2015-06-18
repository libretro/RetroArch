/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015 - Manuel Alfayate
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

#include "../../driver.h"
#include "../../general.h"
#include "../../retroarch.h"
#include "../video_viewport.h"
#include "../video_monitor.h"
#include "../video_context_driver.h"
#include "../font_renderer_driver.h"

#include <bcm_host.h>
#include <rthreads/rthreads.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* We have two "drawing surfaces" (abstract), one for running 
 * core and other for menu, each one backed by a dispmanx 
 * element and a set of buffers (resources in dispmanx terms). */
enum dispmanx_surface_type {
   MAIN_SURFACE,   /* Always first surface */
   MENU_SURFACE,
   BACK_SURFACE    /* Always last surface */
}; 

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
};

struct dispmanx_surface
{
   /* main surface has 3 pages, menu surface has 1 */
   unsigned int numpages;
   struct dispmanx_page *pages;
   unsigned int bpp;   

   VC_RECT_T src_rect;
   VC_RECT_T dst_rect;
   VC_RECT_T bmp_rect;
   
   /* Each surface has it's own element, and the resources are contained one in each page */
   DISPMANX_ELEMENT_HANDLE_T element;
   VC_DISPMANX_ALPHA_T alpha;    
   VC_IMAGE_TYPE_T pixformat;

   /* Surfaces with a higher layer will be on top of the ones with lower. Default is 0. */
   int layer;

   /* Internal frame dimensions */
   int width;
   int height;
   int pitch;

   /* External aspect for scaling */
   float aspect;

   /* Has the surface been setup already? */
   bool setup;
};

struct dispmanx_video
{
   uint64_t frame_count;
   DISPMANX_DISPLAY_HANDLE_T display;
   DISPMANX_UPDATE_HANDLE_T update;
   uint32_t vc_image_ptr;
  
   struct dispmanx_page *current_page;

   /* We abstract three "surfaces": main surface, menu surface and black back surface. */
   struct dispmanx_surface surfaces[3];

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
   slock_t *vsync_cond_mutex;
   slock_t *pending_mutex;
   unsigned int pageflip_pending;

   /* Menu */
   bool menu_active;
};

/* If no free page is available when called, wait for a page flip. */
static struct dispmanx_page *dispmanx_get_free_page(void *data, struct dispmanx_surface *surface) {
   unsigned i;
   struct dispmanx_video *_dispvars = data;
   struct dispmanx_page *page = NULL;

   while (!page)
   {
      /* Try to find a free page */
      for (i = 0; i < surface->numpages; ++i) {
         if (!surface->pages[i].used)
         {
            page = (surface->pages) + i;
            break;
         }
      }

      /* If no page is free at the moment,
       * wait until a free page is freed by vsync CB. */
      if (!page) {
	 slock_lock(_dispvars->vsync_cond_mutex);
	 scond_wait(_dispvars->vsync_condition, _dispvars->vsync_cond_mutex);
	 slock_unlock(_dispvars->vsync_cond_mutex);
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

   /* Marking the page as free must be done before the signaling
    * so when update_main continues (it won't continue until we signal) 
    * we can chose this page as free */
   if (page->dispvars->current_page) {
      slock_lock(page->dispvars->current_page->page_used_mutex);

      /* We mark as free the page that was visible until now */
      page->dispvars->current_page->used = false;

      slock_unlock(page->dispvars->current_page->page_used_mutex);
   }

  /* The page on which we issued the flip that
   * caused this callback becomes the visible one */
   page->dispvars->current_page = page;

   /* These two things must be isolated "atomically" to avoid getting 
    * a false positive in the pending_mutex test in update_main. */ 
   slock_lock(page->dispvars->pending_mutex);
   
   page->dispvars->pageflip_pending--;	
   scond_signal(page->dispvars->vsync_condition);
  
   slock_unlock(page->dispvars->pending_mutex);
}

static void dispmanx_surface_free(void *data, struct dispmanx_surface *surface)
{
   int i;	
   struct dispmanx_video *_dispvars = data;
   
   for (i = 0; i < surface->numpages; i++) { 
      vc_dispmanx_resource_delete(surface->pages[i].resource);
      surface->pages[i].used = false;   
      slock_free(surface->pages[i].page_used_mutex); 
   }
   
   free(surface->pages);
   
   _dispvars->update = vc_dispmanx_update_start(0);
   vc_dispmanx_element_remove(_dispvars->update, surface->element);
   vc_dispmanx_update_submit_sync(_dispvars->update);		

   surface->setup = false;
}

static void dispmanx_surface_setup(void *data, int width, int height, int pitch, float aspect,
   struct dispmanx_surface *surface)
{
   struct dispmanx_video *_dispvars = data;
   int i, dst_width, dst_height, dst_xpos, dst_ypos;

   /* Allocate memory for all the pages in each surface
    * and initialize variables inside each page's struct. */
   surface->pages = calloc(surface->numpages, sizeof(struct dispmanx_page));
   for (i = 0; i < surface->numpages; i++) {
      surface->pages[i].used = false;   
      surface->pages[i].dispvars = _dispvars;   
      surface->pages[i].page_used_mutex = slock_new(); 
   }

   /* Internal frame dimensions. Pitch is total pitch including info 
    * between scanlines */
   surface->width = width;
   surface->height = height;
   surface->pitch = pitch;
   surface->aspect = aspect;  
 
   /* The "visible" width obtained from the core pitch. We blit based on 
    * the "visible" width, for cores with things between scanlines. */
   int visible_width = pitch / surface->bpp;
 
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
   vc_dispmanx_rect_set(&surface->bmp_rect, 0, 0, width, height);	
   vc_dispmanx_rect_set(&surface->src_rect, 0, 0, width << 16, height << 16);	

   for (i = 0; i < surface->numpages; i++) {
      surface->pages[i].resource = 
         vc_dispmanx_resource_create(surface->pixformat, 
	 visible_width, height, &(_dispvars->vc_image_ptr));
   }
   /* Add element. */
   _dispvars->update = vc_dispmanx_update_start(0);

   surface->element = vc_dispmanx_element_add(
      _dispvars->update,_dispvars->display, surface->layer, 
      &surface->dst_rect, surface->pages[0].resource, 
      &surface->src_rect, DISPMANX_PROTECTION_NONE,
      &surface->alpha, 0, (DISPMANX_TRANSFORM_T)0);

   vc_dispmanx_update_submit_sync(_dispvars->update);		

   surface->setup = true;
}

static void dispmanx_surface_init(void *data, 
   int bpp,
   int pixformat,
   int layer,
   unsigned alpha,
   int numpages,
   struct dispmanx_surface *surface)
{
   struct dispmanx_video *_dispvars = data;
   
   /* Setup surface parameters */
   surface->numpages = numpages;
   surface->bpp = bpp;
   surface->layer = layer;
   surface->pixformat = pixformat;

   /* Internal frame dimensions. We leave them as 0 until we get 
    * to the gfx_frame function where we get image width and height. */
   surface->width  = 0;
   surface->height = 0;
   surface->pitch  = 0;

   /* Transparency disabled */
   surface->alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
   surface->alpha.opacity = alpha;
   surface->alpha.mask = 0;

   /* This will be true when we have allocated mem for the pages and 
    * created their element, resources, etc.. */
   surface->setup = false;
}

static void dispmanx_surface_update(void *data, const void *frame, struct dispmanx_surface *surface)
{
   struct dispmanx_video *_dispvars = data;
   struct dispmanx_page *page = NULL;
   
   /* Wait until last issued flip completes to get a free page. Also, 
      dispmanx doesn't support issuing more than one pageflip.*/
   slock_lock(_dispvars->pending_mutex);
   if (_dispvars->pageflip_pending > 0)
   {
      scond_wait(_dispvars->vsync_condition, _dispvars->pending_mutex);
   }
   slock_unlock(_dispvars->pending_mutex);
  
   page = dispmanx_get_free_page(_dispvars, surface);
 
   /* Frame blitting */
   vc_dispmanx_resource_write_data(page->resource, surface->pixformat,
      surface->pitch, (void*)frame, &(surface->bmp_rect));
   
   /* Issue a page flip that will be done at the next vsync. */
   _dispvars->update = vc_dispmanx_update_start(0);

   vc_dispmanx_element_change_source(_dispvars->update, surface->element,
         page->resource);

   vc_dispmanx_update_submit(_dispvars->update, dispmanx_vsync_callback, (void*)page);

   slock_lock(_dispvars->pending_mutex);
   _dispvars->pageflip_pending++;	
   slock_unlock(_dispvars->pending_mutex);
}

static void dispmanx_blank_console (void *data)
{
   /* Note that a 2-pixels array is needed to accomplish console blanking because with 1-pixel
    * only the write data function doesn't work well, so when we do the only resource 
    * change in the surface update function, we will be seeing a distorted console. */
   struct dispmanx_video *_dispvars = data;
   uint16_t image[2] = {0x0000, 0x0000};
   float aspect = (float)_dispvars->dispmanx_width / (float)_dispvars->dispmanx_height;   

   dispmanx_surface_init(_dispvars,
      1, 
      VC_IMAGE_RGB565, 
      -1, 
      255, 
      1,
      &_dispvars->surfaces[BACK_SURFACE]);
   dispmanx_surface_setup(_dispvars, 2, 2, 4, aspect, &_dispvars->surfaces[BACK_SURFACE]);
   dispmanx_surface_update(_dispvars, &image, &_dispvars->surfaces[BACK_SURFACE]);
}

static void *dispmanx_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   struct dispmanx_video *_dispvars = calloc(1, sizeof(struct dispmanx_video));

   bcm_host_init();
   _dispvars->display = vc_dispmanx_display_open(0 /* LCD */);
   /* If the console framebuffer has active overscan settings, 
    * the user must have overscan_scale=1 in config.txt to have 
    * the same size for both fb console and dispmanx. */
   graphics_get_display_size(_dispvars->display, &_dispvars->dispmanx_width, &_dispvars->dispmanx_height);

   /* Setup surface parameters */
   _dispvars->vc_image_ptr     = 0;
   _dispvars->pageflip_pending = 0;	
   _dispvars->current_page     = NULL;
   _dispvars->menu_active      = false;
  
   /* Initialize the rest of the mutexes and conditions. */
   _dispvars->vsync_condition  = scond_new();
   _dispvars->vsync_cond_mutex = slock_new();
   _dispvars->pending_mutex    = slock_new();

   dispmanx_surface_init(_dispvars,
      video->rgb32 ? 4 : 2, 
      video->rgb32 ? VC_IMAGE_XRGB8888 : VC_IMAGE_RGB565, 
      0   /* layer */, 
      255 /* alpha */, 
      3,  /* numpages */
      &_dispvars->surfaces[MAIN_SURFACE]);

   if (input && input_data)
      *input = NULL;
  
   dispmanx_blank_console(_dispvars);
 
   return _dispvars;
}

static bool dispmanx_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, unsigned pitch, const char *msg)
{
   struct dispmanx_video *_dispvars = data;

   if (width != _dispvars->surfaces[MAIN_SURFACE].width || height != _dispvars->surfaces[MAIN_SURFACE].height)
   {
      /* Sanity check. */
      if (width == 0 || height == 0)
         return true;
      if (_dispvars->surfaces[MAIN_SURFACE].setup) 
         dispmanx_surface_free(_dispvars, &_dispvars->surfaces[MAIN_SURFACE]);
      
      float aspect = video_driver_get_aspect_ratio();
      /* Reconfiguring internal dimensions of the main surface is needed. */
      dispmanx_surface_setup(_dispvars, width, height, pitch, aspect, &_dispvars->surfaces[MAIN_SURFACE]);
   }
   
   if (_dispvars->menu_active)
   {
      char buf[128];
      video_monitor_get_fps(buf, sizeof(buf), NULL, 0);
   }

   /* Update main surface: locate free page, blit and flip. */
   dispmanx_surface_update(_dispvars, frame, &_dispvars->surfaces[MAIN_SURFACE]);
   _dispvars->frame_count++;
   return true;
}

static void dispmanx_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct dispmanx_video *_dispvars = data;
   
   /* If it was active but it's not anymore... */
   if (!state && _dispvars->menu_active) {
      dispmanx_surface_free(_dispvars, &_dispvars->surfaces[MENU_SURFACE]);
      /* This is needed so we enter thru surface_setup on 
       * set_texture_frame() next time menu is active. */
      _dispvars->surfaces[MENU_SURFACE].width = 0;
      _dispvars->surfaces[MENU_SURFACE].height = 0;
   }
   _dispvars->menu_active = state;
}

static void dispmanx_set_texture_frame(void *data, const void *frame, bool rgb32,
   unsigned width, unsigned height, float alpha)
{
   struct dispmanx_video *_dispvars = data;
 
   /* If we're entering the menu in this frame, 
    * we must setup rects, resources and menu element. */
   if (width != _dispvars->surfaces[MENU_SURFACE].width || height != _dispvars->surfaces[MENU_SURFACE].height)
   {
      /* Sanity check */
      if (width == 0 || height == 0)
         return;
      int pitch = width * (rgb32 ? 4 : 2);
      
      dispmanx_surface_init(_dispvars,
	 rgb32 ? 4 : 2, 
	 VC_IMAGE_RGBA16, 
	 0     /* layer */, 
	 210 /* alpha, hardcoded */, 
	 3,    /* numpages */
	 &_dispvars->surfaces[MENU_SURFACE]);

      dispmanx_surface_setup(_dispvars, width, height, pitch, 
         _dispvars->surfaces[MAIN_SURFACE].aspect,
         &_dispvars->surfaces[MENU_SURFACE]);
   }
   dispmanx_surface_update(_dispvars, frame, &_dispvars->surfaces[MENU_SURFACE]);
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

    vp->width  = vp->full_width  = vid->surfaces[MAIN_SURFACE].width;
    vp->height = vp->full_height = vid->surfaces[MAIN_SURFACE].height;
}

static bool dispmanx_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static bool dispmanx_gfx_has_windowed(void *data)
{
   (void)data;

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

static void dispmanx_gfx_set_rotation(void *data, unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static bool dispmanx_gfx_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

static uint64_t dispmanx_gfx_get_frame_count(void *data)
{
   struct dispmanx_video *_dispvars = data;
   return _dispvars->frame_count;
}

static void dispmanx_set_aspect_ratio (void *data, unsigned aspect_ratio_idx) 
{
   struct dispmanx_video *_dispvars = data;
   struct dispmanx_surface *surface; 
   /* Here we obtain the new aspect ratio. */

   float aspect = aspectratio_lut[aspect_ratio_idx].value;

   surface = &_dispvars->surfaces[MAIN_SURFACE];
   dispmanx_surface_free(_dispvars, surface);
   dispmanx_surface_setup(_dispvars, surface->width, surface->height, surface->pitch, aspect, surface);
  
   surface = &_dispvars->surfaces[MENU_SURFACE];
   dispmanx_surface_free(_dispvars, surface);
   dispmanx_surface_setup(_dispvars, surface->width, surface->height, surface->pitch, aspect, surface);
}

static const video_poke_interface_t dispmanx_poke_interface = {
   dispmanx_gfx_get_frame_count,
   NULL, /* set_video_mode */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   dispmanx_set_aspect_ratio,
   NULL, /* dispmanx_apply_state_changes */
#ifdef HAVE_MENU
   dispmanx_set_texture_frame,
   dispmanx_set_texture_enable,
#endif
   NULL, /* dispmanx_set_osd_msg */
   NULL  /* dispmanx_show_mouse */
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
   int i;

   for (i = MAIN_SURFACE; i <= BACK_SURFACE; i++) {
      if (_dispvars->surfaces[i].setup) {
         dispmanx_surface_free(_dispvars, &_dispvars->surfaces[i]);
      }
   }

   /* Close display and deinitialize. */
   vc_dispmanx_display_close(_dispvars->display);
   bcm_host_deinit();

   /* Destroy mutexes and conditions. */
   slock_free(_dispvars->pending_mutex);
   slock_free(_dispvars->vsync_cond_mutex);
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
   dispmanx_gfx_has_windowed,
   dispmanx_gfx_set_shader,
   dispmanx_gfx_free,
   "dispmanx",
   NULL, /* set_viewport */
   dispmanx_gfx_set_rotation,
   dispmanx_gfx_viewport_info,
   dispmanx_gfx_read_viewport,
   NULL, /* read_frame_raw */

   #ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
   #endif
   dispmanx_gfx_get_poke_interface
};
