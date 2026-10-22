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
 *
 */

 /*  Plain DRM diver */

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_fourcc.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <compat/strl.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../common/drm_common.h"
#include "../video_views.h"
#include "../video_views_cpu.h"
#ifdef HAVE_OVERLAY
#include "../../input/input_overlay.h"
#endif

#include "drm_pixformats.h"

struct modeset_buf
{
	/* The buffer's pixel size, packed; stride is the row the
	   scanout walks, which is not always the width. */
	uint32_t dims;
	uint32_t stride;
	uint32_t size;
	uint32_t handle;
	uint8_t *map;
	uint32_t fb_id;
	uint32_t pixel_format;
};

/* Pages are abstractions of buffers, encapsulated together with more
   data for multiple buffering: to know if it's used, etc.
   Hence, each page will have ONE buffer. No more, no less.*/

struct drm_page
{
   struct modeset_buf buf;
   bool used;

   /* Each page has it's own mutex for
    * isolating it's used flag access. */
   slock_t *page_used_mutex;

   /* This field will allow us to access the
    * main _dispvars struct from the vsync CB function */
   struct drm_video *drmvars;

   /* This field will allow us to access the
    * surface the page belongs to. */
   struct drm_surface *surface;
};

/* One surface for main game, another for menu. */
struct drm_surface
{
   /* main surface has 3 pages, menu surface has 1 */
   unsigned int numpages;
   struct drm_page *pages;

   /* the page that's currently on screen */
   struct drm_page *current_page;
   unsigned int bpp;
   uint32_t pixformat;

   /* The internal buffers size, packed. */
   unsigned src_dims;

   /* Surfaces with a higher layer will be on top of
    * the ones with lower. Default is 0. */
   int layer;

   /* The visible row in bytes */
   int pitch;

   float aspect;
   bool flip_page;
   /* A page went to the core to render into */
   bool lent;
};

struct drm_struct
{
   /* DRM connection, mode and plane management stuff */
   int fd;
   drmModeModeInfo *current_mode;
   uint32_t crtc_id;
   uint32_t connector_id;

   drmModeCrtcPtr orig_crtc;

   uint32_t plane_id;
   uint32_t plane_fb_prop_id;
   /* Reused by every flip */
   drmModeAtomicReqPtr flip_req;
   /* Where the plane puts the surface it shows, in screen pixels */
   int vp_x;
   int vp_y;
   unsigned vp_w;
   unsigned vp_h;

   drmModeEncoder *encoder;
   drmModeRes *resources;
} drm;

#ifdef HAVE_OVERLAY
struct drm_overlay_image
{
   uint32_t *px;     /* premultiplied ARGB8888 */
   uint32_t *scaled; /* px at the size it is shown */
   float tex[4];
   float vert[4];
   float alpha;
   unsigned w, h;
   int x, y;         /* where it is shown, in the layer's space */
   unsigned sw, sh;
};

/* The input overlay: its images composed, when one of them changes,
 * into a layer that a plane of its own shows over the frame, or that
 * is blended into each frame where the CRTC has no plane to spare */
struct drm_overlay
{
   struct drm_overlay_image *img;
   uint32_t *layer;  /* premultiplied ARGB8888, lw x lh */
   struct modeset_buf page[2];
   unsigned count;
   int lx, ly;       /* the layer: screen pixels, or the frame's */
   unsigned lw, lh;
   uint32_t plane_id;
   uint32_t fb_prop;
   unsigned back;
   /* The frame's size the layer was laid out at, where it is blended */
   unsigned frame_dims;
   enum
   {
      DRM_OVERLAY_UNTRIED = 0,
      DRM_OVERLAY_PLANE,
      DRM_OVERLAY_BLEND
   } mode;
   bool enabled;
   bool fullscreen;
   bool geom_dirty;
   bool layer_dirty;
   /* The back page holds a layer the next flip shows */
   bool pending;
   /* The plane shows the layer at lx, ly */
   bool shown;
};
#endif

struct drm_video
{
   /* Abstract surface management stuff */
   struct drm_surface *main_surface;
   struct drm_surface *menu_surface;
   /* A core's views composed for the plane to scale to the screen */
   struct drm_surface *views_surface;
#ifdef HAVE_OVERLAY
   struct drm_overlay ov;
#endif
   video_views_cpu_t views_cpu;
   /* Both eyes side by side, for anaglyph and interlaced */
   uint32_t *views_canvas;
   size_t views_canvas_size;
   /* For get_flags: the last frame with a map was drawn whole */
   retro_atomic_int_t views_fallback;
   bool views_seen;

   /* Total dispmanx video dimensions.
    * Not counting overscan settings. */
   /* The mode the display is running, packed. */
   unsigned kms_dims;

   /* For threading */
   scond_t *vsync_condition;
   slock_t *vsync_cond_mutex;
   slock_t *pending_mutex;

   /* The plane last asked whether it takes RGUI's format, and its answer */
   uint32_t menu_probed_plane;

   /* Menu */
   bool menu_active;
   bool menu_probed;
   bool menu_rgbx4444;

   bool rgb32;

   /* We use this to keep track of internal resolution changes
    * done by cores in the main surface or in the menu.
    * We need these outside the surface because we free surfaces
    * and then we want to test if these values have changed before
    * recreating them. */
   /* The size the core last handed over, packed. */
   unsigned core_dims;
   int core_pitch;
   /* Both main and menu surfaces are going to have the same aspect,
    * so we keep it here for future reference. */
   float current_aspect;

};

/* Some prototypes for later use */

static int modeset_create_dumbfb(int fd,
      struct modeset_buf *buf, int bpp, uint32_t pixformat);

static void deinit_drm(void)
{
   /* Restore the original videomode/connector/scanoutbuffer(fb)
    * combination (the original CRTC, that is). */
   drmModeSetCrtc(drm.fd, drm.orig_crtc->crtc_id,
         drm.orig_crtc->buffer_id,
         drm.orig_crtc->x, drm.orig_crtc->y,
         &drm.connector_id, 1, &drm.orig_crtc->mode);

#if 0
   /* TODO: Free surfaces here along
    * with their pages (framebuffers)! */

   if (bufs[0].fb_id)
   {
      drmModeRmFB(drm.fd, bufs[0].fb_id);
      drmModeRmFB(drm.fd, bufs[1].fb_id);
   }
#endif
}

/* Takes down what modeset_create_dumbfb() made: the mapping, the
 * framebuffer and the dumb buffer. A page never made has none. */
static void modeset_destroy_dumbfb(struct modeset_buf *buf)
{
   if (buf->map)
   {
      munmap(buf->map, buf->size);
      buf->map = NULL;
   }
   if (buf->fb_id)
   {
      drmModeRmFB(drm.fd, buf->fb_id);
      buf->fb_id = 0;
   }
   if (buf->handle)
   {
      struct drm_mode_destroy_dumb destroy_dumb = {0};
      destroy_dumb.handle = buf->handle;
      drmIoctl(drm.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
      buf->handle = 0;
   }
}

/* The plane must not be reading from the surface: the kernel switches
 * a plane off when the framebuffer it scans out is removed. */
static void drm_surface_free(void *data, struct drm_surface **sp)
{
   int i;
   struct drm_surface *surface;

   if (!sp || !(surface = *sp))
      return;

   for (i = 0; i < surface->numpages; i++)
   {
      modeset_destroy_dumbfb(&surface->pages[i].buf);
      if (surface->pages[i].page_used_mutex)
         slock_free(surface->pages[i].page_used_mutex);
   }

   free(surface->pages);
   free(surface);
   *sp = NULL;
}

/* Changes surface ratio only without recreating the buffers etc. */
static void drm_surface_set_aspect(struct drm_surface *surface, float aspect)
{
   if (surface)
      surface->aspect = aspect;
}

static bool drm_surface_setup(void *data, unsigned src_dims,
      int pitch, int bpp, uint32_t pixformat,
      int alpha, float aspect, int numpages, int layer,
      struct drm_surface **sp)
{
   struct drm_video *_drmvars = data;
   int i;
   int ret;
   struct drm_surface *surface = NULL;

   *sp = calloc (1, sizeof(struct drm_surface));

   surface = *sp;

   /* NULL-check the outer calloc: the surface->... field writes
    * below would NULL-deref on OOM.  Sibling bug to the one in
    * dispmanx_surface_setup - the two routines share this
    * structure (output-pointer + void return) and both had the
    * same missing checks. */
   if (!surface)
      return false;

   /* Setup surface parameters */
   surface->numpages = numpages;
   surface->pitch       = VIDEO_SCALE_W(src_dims) * bpp;
   surface->bpp         = bpp;
   surface->pixformat   = pixformat;
   surface->src_dims    = src_dims;
   surface->aspect      = aspect;

   /* Allocate memory for all the pages in each surface
    * and initialize variables inside each page's struct. */
   surface->pages = (struct drm_page*)
      calloc(surface->numpages, sizeof(struct drm_page));

   /* Same NULL-check for the pages array.  Undo the outer
    * surface allocation on OOM to give callers a consistent
    * NULL. */
   if (!surface->pages)
   {
      free(surface);
      *sp = NULL;
      return false;
   }

   for (i = 0; i < surface->numpages; i++)
   {
      surface->pages[i].used            = false;
      surface->pages[i].surface         = surface;
      surface->pages[i].drmvars         = _drmvars;
      surface->pages[i].page_used_mutex = slock_new();
   }

   /* Create the framebuffer for each one of the pages of the surface. */
   for (i = 0; i < surface->numpages; i++)
   {
      surface->pages[i].buf.dims   = src_dims;
      ret                          = modeset_create_dumbfb(
            drm.fd, &surface->pages[i].buf, bpp, pixformat);

      if (ret)
      {
         RARCH_ERR("[DRM] Can't create fb.\n");
         drm_surface_free(_drmvars, sp);
         return false;
      }
   }

   surface->flip_page = 0;
   return true;
}

#ifdef HAVE_OVERLAY
static void drm_overlay_flip_add(struct drm_video *v, drmModeAtomicReqPtr req);
static void drm_overlay_flipped(struct drm_video *v);
static void drm_overlay_blend(struct drm_video *v, struct drm_surface *s);
#endif

static void drm_page_flip(struct drm_surface *surface)
{
#ifdef HAVE_OVERLAY
   struct drm_video *v = surface->pages[0].drmvars;
#endif
   /* We already have the id of the FB_ID property of
    * the plane on which we are going to do a pageflip:
    * we got it back in drm_plane_setup()  */
   drmModeAtomicReqPtr req = drm.flip_req;

   if (req)
      drmModeAtomicSetCursor(req, 0);
   else if (!(req = drm.flip_req = drmModeAtomicAlloc()))
      return;

   /* We add the buffer to the plane properties we want to
    * set on an atomically, in a single step.
    * We pass the plane id, the property id and the new fb id. */
   if (drmModeAtomicAddProperty(req,
         drm.plane_id,
         drm.plane_fb_prop_id,
         surface->pages[surface->flip_page].buf.fb_id) < 0)
   {
      RARCH_ERR("[DRM] Failed to add atomic property for pageflip.\n");
   }
#ifdef HAVE_OVERLAY
   /* A new overlay layer goes up in the same commit as the frame */
   drm_overlay_flip_add(v, req);
#endif
   /*... now we just need to do the commit */

   /* REMEMBER!!! The DRM_MODE_PAGE_FLIP_EVENT flag asks the kernel
    * to send you an event to the drm.fd once the
    * pageflip is complete. If you don't want -12 errors
    * (ENOMEM), namely "Cannot allocate memory", then
    * you must drain the event queue of that fd. */
   if (drmModeAtomicCommit(drm.fd, req, 0, NULL) < 0)
   {
      RARCH_ERR("[DRM] Failed to commit for pageflip.\n");
   }

#ifdef HAVE_OVERLAY
   drm_overlay_flipped(v);
#endif
   surface->flip_page = !(surface->flip_page);
}

static void drm_surface_update(void *data, const void *frame,
      unsigned pitch, struct drm_surface *surface)
{
   struct modeset_buf *buf = &surface->pages[surface->flip_page].buf;

   /* A lent page holds the frame already */
   if ((const uint8_t*)frame != buf->map)
   {
      int line;
      const uint8_t *src = (const uint8_t*)frame;
      uint8_t *dst       = buf->map;

      for (line = 0; line < (int)VIDEO_SCALE_H(surface->src_dims); line++)
      {
         memcpy(dst, src, surface->pitch);
         src += pitch;
         dst += buf->stride;
      }
   }

#ifdef HAVE_OVERLAY
   drm_overlay_blend(surface->pages[0].drmvars, surface);
#endif

   /* Page flipping */
   drm_page_flip(surface);
}

/* The ID of the plane's property called @name, 0 if it has none. */
static uint32_t get_plane_prop_id(uint32_t plane_id, const char *name)
{
   int j;
   uint32_t prop_id               = 0;
   drmModeObjectProperties *props = drmModeObjectGetProperties(drm.fd,
         plane_id, DRM_MODE_OBJECT_PLANE);

   if (!props)
      return 0;

   for (j = 0; j < (int)props->count_props && !prop_id; j++)
   {
      drmModePropertyRes *prop = drmModeGetProperty(drm.fd, props->props[j]);
      if (!prop)
         continue;
      if (string_is_equal(prop->name, name))
         prop_id = prop->prop_id;
      drmModeFreeProperty(prop);
   }
   drmModeFreeObjectProperties(props);

   if (!prop_id)
      RARCH_ERR("[DRM] Plane %d fb property ID with name %s not found.\n",
            plane_id, name);
   return prop_id;
}

/* gets fourcc, returns name string. */
static void drm_format_name(const unsigned int fourcc, char *format_str)
{
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(format_info); i++)
   {
		if (format_info[i].format == fourcc)
			strlcpy(format_str, format_info[i].name, sizeof(format_str));
	}
}

/* Will tell us if the supplied plane supports the supplied pix format. */
static bool format_support(const drmModePlanePtr ovr, uint32_t fmt)
{
   unsigned int i;

   for (i = 0; i < ovr->count_formats; ++i)
   {
      if (ovr->formats[i] == fmt)
         return true;
   }

   return false;
}

static uint64_t drm_plane_type(drmModePlane *plane)
{
   int j;
   /* Not found reads as a primary plane, never as an overlay (0) */
   uint64_t type = DRM_PLANE_TYPE_PRIMARY;

   /* The property values and their names are stored in different arrays,
    * so we access them simultaneously here.
    * We are interested in OVERLAY planes only, that's
    * type 0 or DRM_PLANE_TYPE_OVERLAY
    * (see /usr/xf86drmMode.h for definition). */
   drmModeObjectPropertiesPtr props =
      drmModeObjectGetProperties(drm.fd, plane->plane_id,
            DRM_MODE_OBJECT_PLANE);

   if (!props)
      return type;

   for (j = 0; j < (int)props->count_props; j++)
   {
      drmModePropertyRes *prop = drmModeGetProperty(drm.fd, props->props[j]);
      bool found               = prop && string_is_equal(prop->name, "type");
      if (found)
         type = props->prop_values[j];
      if (prop)
         drmModeFreeProperty(prop);
      if (found)
         break;
   }
   drmModeFreeObjectProperties(props);

   return type;
}

/* Whether the plane in use, or the one setup would pick when none is,
 * scans out @fmt */
static bool drm_plane_takes(uint32_t fmt)
{
   int i;
   unsigned crtc_index = 0;
   bool takes          = false;
   drmModePlaneRes *res;

   if (drm.plane_id)
   {
      drmModePlane *plane = drmModeGetPlane(drm.fd, drm.plane_id);
      if (plane)
      {
         takes = format_support(plane, fmt);
         drmModeFreePlane(plane);
      }
      return takes;
   }

   if (!(res = drmModeGetPlaneResources(drm.fd)))
      return false;
   for (i = 0; i < drm.resources->count_crtcs; i++)
      if (drm.crtc_id == drm.resources->crtcs[i])
         crtc_index = i;
   for (i = 0; i < (int)res->count_planes && !takes; i++)
   {
      drmModePlane *plane = drmModeGetPlane(drm.fd, res->planes[i]);
      if (!plane)
         continue;
      takes = (plane->possible_crtcs & (1 << crtc_index))
         && drm_plane_type(plane) == DRM_PLANE_TYPE_OVERLAY
         && format_support(plane, fmt);
      drmModeFreePlane(plane);
   }
   drmModeFreePlaneResources(res);
   return takes;
}

/* This configures our only overlay plane to render the given surface. */
static void drm_plane_setup(struct drm_surface *surface)
{
   int i;
   char fmt_name[5];
   unsigned int crtc_index = 0;
   uint32_t plane_flags = 0;
   uint32_t plane_w;
   uint32_t plane_h;
   uint32_t plane_x;
   uint32_t plane_y;
   uint32_t src_w;
   uint32_t src_h;
   uint32_t src_x = 0;
   uint32_t src_y = 0;

   /* Get plane resources */
   drmModePlane *plane;
   drmModePlaneRes *plane_resources;
   uint32_t in_use = drm.plane_id;
   uint32_t found  = 0;
   plane_resources = drmModeGetPlaneResources(drm.fd);
   if (!plane_resources)
   {
      RARCH_ERR("[DRM] No scaling planes available.\n");
      return;
   }

   RARCH_LOG("[DRM] Number of planes on FD %d is %d.\n",
         drm.fd, plane_resources->count_planes);

   /* dump_planes(drm.fd); */

   /* Look for a plane/overlay we can use with the configured CRTC
    * Find a  plane which can be connected to our CRTC. Find the
    * CRTC index first, then iterate over available planes.
    * Yes, strangely we need the in-use CRTC index to mask possible_crtc
    * during the planes iteration... */
   for (i = 0; i < (unsigned int)drm.resources->count_crtcs; i++)
   {
      if (drm.crtc_id == drm.resources->crtcs[i])
      {
         crtc_index = i;
         RARCH_LOG("[DRM] CRTC index found %d with ID %d.\n", crtc_index, drm.crtc_id);
         break;
      }
   }

   /* Programmer!! Save your sanity!! Primary planes have to
    * cover the entire CRTC, and if you don't do that, you
    * will get dmesg error "Plane must cover entire CRTC".
    *
    * Look at linux/source/drivers/gpu/drm/drm_plane_helper.c comments for more info.
    * Also, primary planes can't be scaled: we need overlays for that. */
   for (i = 0; i < plane_resources->count_planes; i++)
   {
      if (!(plane = drmModeGetPlane(drm.fd, plane_resources->planes[i])))
         continue;

      if (!(plane->possible_crtcs & (1 << crtc_index)))
         RARCH_LOG("[DRM] Plane with ID %d can't be used with current CRTC.\n",
               plane->plane_id);
      /* We are only interested in overlay planes. No overlay, no fun.
       * (no scaling, must cover crtc..etc) so we skip primary planes */
      else if (drm_plane_type(plane) != DRM_PLANE_TYPE_OVERLAY)
         RARCH_LOG("[DRM] Plane with ID %d is not an overlay. May be primary or cursor. Not usable.\n",
               plane->plane_id);
      else if (!format_support(plane, surface->pixformat))
         RARCH_LOG("[DRM] Plane with ID %d does not support framebuffer format.\n", plane->plane_id);
      /* The plane in use stays, or it would go on showing its frame */
      else if (!found || found != in_use)
         found = plane->plane_id;

      drmModeFreePlane(plane);
   }
   drmModeFreePlaneResources(plane_resources);

   if (found)
      drm.plane_id = found;

   if (!drm.plane_id)
   {
      RARCH_LOG("[DRM] Couldn't find an usable overlay plane for current CRTC and framebuffer pixel format.\n");
      deinit_drm();
      exit (0);
   }
   else
   {
      RARCH_LOG("[DRM] Using plane/overlay ID %d.\n", drm.plane_id);
   }

   /* We are going to be changing the framebuffer ID property of the chosen overlay every time
    * we do a pageflip, so we get the property ID here to have it handy on the PageFlip function. */
   drm.plane_fb_prop_id = get_plane_prop_id(drm.plane_id, "FB_ID");
   if (!drm.plane_fb_prop_id)
   {
      RARCH_LOG("[DRM] Can't get the FB property ID for plane(%u).\n", drm.plane_id);
   }

   /* Note src coords (last 4 args) are in Q16 format
    * crtc_w and crtc_h are the final size with applied scale/ratio.
    * crtc_x and crtc_y are the position of the plane
    * pw and ph are the input size: the size of the area we read from the fb. */
   plane_w = drm.current_mode->vdisplay * surface->aspect;
   plane_h = drm.current_mode->vdisplay;
   /* If we obtain a scaled image width that is bigger than the physical screen width,
    * then we keep the physical screen width as our maximum width. */
   if (plane_w > drm.current_mode->hdisplay)
      plane_w = drm.current_mode->hdisplay;

   plane_x = (drm.current_mode->hdisplay - plane_w) / 2;
   plane_y = (drm.current_mode->vdisplay - plane_h) / 2;

   src_w = VIDEO_SCALE_W(surface->src_dims);
   src_h = VIDEO_SCALE_H(surface->src_dims);

   if (     drm.vp_x != (int)plane_x || drm.vp_y != (int)plane_y
         || drm.vp_w != plane_w || drm.vp_h != plane_h)
   {
      drm.vp_x = (int)plane_x;
      drm.vp_y = (int)plane_y;
      drm.vp_w = plane_w;
      drm.vp_h = plane_h;
#ifdef HAVE_OVERLAY
      if (surface->pages[0].drmvars)
         surface->pages[0].drmvars->ov.geom_dirty = true;
#endif
   }

   /* We have to set a buffer for the plane, whatever buffer we want,
    * but we must set a buffer so the plane starts reading from it now. */
   if (drmModeSetPlane(drm.fd, drm.plane_id, drm.crtc_id,
            surface->pages[surface->flip_page].buf.fb_id,
            plane_flags, plane_x, plane_y, plane_w, plane_h,
            src_x<<16, src_y<<16, src_w<<16, src_h<<16))
   {
      RARCH_ERR("[DRM] Failed to enable plane.\n");
   }

   RARCH_DBG("[DRM] src_w %d, src_h %d, plane_w %d, plane_h %d\n",
         src_w, src_h, plane_w, plane_h);

   /* Report what plane (of overlay type) we're using. */
   drm_format_name(surface->pixformat, fmt_name);
   RARCH_LOG("[DRM] Using plane with ID %d on CRTC ID %d format %s.\n",
         drm.plane_id, drm.crtc_id, fmt_name);
}

static int modeset_create_dumbfb(int fd, struct modeset_buf *buf,
      int bpp, uint32_t pixformat)
{
   struct drm_mode_create_dumb create_dumb = {0};
   struct drm_mode_map_dumb map_dumb       = {0};
   struct drm_mode_fb_cmd2 cmd_dumb        = {0};
   struct drm_mode_destroy_dumb destroy_dumb = {0};

   create_dumb.width  = VIDEO_SCALE_W(buf->dims);
   create_dumb.height = VIDEO_SCALE_H(buf->dims);
   create_dumb.bpp    = bpp * 8;
   create_dumb.flags  = 0;
   create_dumb.pitch  = 0;
   create_dumb.size   = 0;
   create_dumb.handle = 0;
   if (drmIoctl(drm.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb))
   {
      RARCH_ERR("[DRM] Cannot create dumb buffer.\n");
      return -1;
   }

   /* The format named, as no legacy depth and bpp can name RGBX4444 */
   cmd_dumb.width        = create_dumb.width;
   cmd_dumb.height       = create_dumb.height;
   cmd_dumb.pixel_format = pixformat;
   cmd_dumb.handles[0]   = create_dumb.handle;
   cmd_dumb.pitches[0]   = create_dumb.pitch;

   if (drmIoctl(drm.fd, DRM_IOCTL_MODE_ADDFB2, &cmd_dumb))
   {
      RARCH_ERR("[DRM] Cannot add framebuffer.\n");
      goto error_destroy;
   }

   /* Map the buffer */
   map_dumb.handle = create_dumb.handle;
   if (drmIoctl(drm.fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb))
   {
      RARCH_ERR("[DRM] Cannot map dumb buffer.\n");
      goto error_rmfb;
   }

   buf->pixel_format = pixformat;
   buf->fb_id = cmd_dumb.fb_id;
   buf->stride = create_dumb.pitch;
   buf->size = create_dumb.size;
   buf->handle = create_dumb.handle;

   /* Get address */
   buf->map = mmap(0, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
         fd, map_dumb.offset);
   if (buf->map == MAP_FAILED)
   {
      RARCH_ERR("[DRM] Cannot mmap dumb buffer.\n");
      buf->map = NULL;
      goto error_rmfb;
   }

   return 0;

error_rmfb:
   drmModeRmFB(drm.fd, cmd_dumb.fb_id);
error_destroy:
   destroy_dumb.handle = create_dumb.handle;
   drmIoctl(drm.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
   buf->fb_id  = 0;
   buf->handle = 0;
   return -1;
}

static bool init_drm(void)
{
   uint i;
   drmModeConnector *connector = NULL;
   struct modeset_buf buf;

   drm.fd = open("/dev/dri/card0", O_RDWR);

   if (drm.fd < 0)
   {
      RARCH_ERR("[DRM] Could not open DRM device.\n");
      return false;
   }

   /* Programmer!! Save your sanity!!
    * VERY important or we won't get all the available planes on drmGetPlaneResources()!
    * We also need to enable the ATOMIC cap to see the atomic properties in objects!! */
   if (drmSetClientCap(drm.fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1))
      RARCH_ERR("[DRM] Can't set UNIVERSAL PLANES cap.\n");
   else
      RARCH_LOG("[DRM] UNIVERSAL PLANES cap set.\n");

   if (drmSetClientCap(drm.fd, DRM_CLIENT_CAP_ATOMIC, 1))
   {
      /*If this happens, check kernel support and kernel parameters
       * (add i915.nuclear_pageflip=y to the kernel boot line for example) */
      RARCH_ERR("[DRM] Can't set ATOMIC caps.\n");
   }
   else
      RARCH_LOG("[DRM] ATOMIC caps set.\n");

   drm.resources = drmModeGetResources(drm.fd);
   if (!drm.resources)
   {
      RARCH_ERR("[DRM] drmModeGetResources failed.\n");
      return false;
   }

   /* Find a connected connector. */
   for (i = 0; i < (uint)drm.resources->count_connectors; i++)
   {
      connector = drmModeGetConnector(drm.fd, drm.resources->connectors[i]);
      /* It's connected, let's use it. */
      if (connector->connection == DRM_MODE_CONNECTED)
         break;
      drmModeFreeConnector(connector);
      connector = NULL;
   }

   if (!connector)
   {
      RARCH_ERR("[DRM] No connected connector found.\n");
      return false;
   }

   /* Find encoder */
   for (i = 0; i < (uint)drm.resources->count_encoders; i++)
   {
      drm.encoder = drmModeGetEncoder(drm.fd, drm.resources->encoders[i]);
      if (drm.encoder->encoder_id == connector->encoder_id)
         break;
      drmModeFreeEncoder(drm.encoder);
      drm.encoder = NULL;
   }

   if (!drm.encoder)
   {
      RARCH_ERR("[DRM] No encoder found.\n");
      return false;
   }

   drm.crtc_id = drm.encoder->crtc_id;
   drm.connector_id = connector->connector_id;

   /* Backup original crtc and it's mode, so we can restore the original video mode
    * on exit in case we change it. */
   drm.orig_crtc = drmModeGetCrtc(drm.fd, drm.encoder->crtc_id);
   drm.current_mode = &(drm.orig_crtc->mode);
   g_drm_mode = drm.current_mode;

   /* Set mode physical video mode. Not really needed, but clears TTY console. */
   buf.dims = VIDEO_SCALE_PACK(drm.current_mode->hdisplay,
         drm.current_mode->vdisplay);
   if (modeset_create_dumbfb(drm.fd, &buf, 4, DRM_FORMAT_XRGB8888))
   {
      RARCH_ERR("[DRM] Can't create dumb fb.\n");
      return false;
   }

   if (drmModeSetCrtc(drm.fd, drm.crtc_id, buf.fb_id, 0, 0,
            &drm.connector_id, 1, drm.current_mode))
   {
      RARCH_ERR("[DRM] Failed to set mode.\n");
      return false;
   }

   return true;
}

#ifdef HAVE_OVERLAY
/* Plane @plane's property @name: its id, value, flags and the top of its
 * range, and the value of its enum entry @entry when asked for one; 0
 * when it has none */
static uint32_t drm_plane_prop(uint32_t plane, const char *name,
      uint64_t *value, uint32_t *flags, uint64_t *max,
      const char *entry, uint64_t *entry_value)
{
   int i, j;
   uint32_t id                    = 0;
   drmModeObjectProperties *props = drmModeObjectGetProperties(drm.fd,
         plane, DRM_MODE_OBJECT_PLANE);

   if (!props)
      return 0;
   for (i = 0; i < (int)props->count_props && !id; i++)
   {
      drmModePropertyRes *prop = drmModeGetProperty(drm.fd, props->props[i]);
      if (!prop)
         continue;
      if (string_is_equal(prop->name, name))
      {
         id = prop->prop_id;
         if (value)
            *value = props->prop_values[i];
         if (flags)
            *flags = prop->flags;
         if (max)
            *max = ((prop->flags & DRM_MODE_PROP_RANGE)
                  && prop->count_values >= 2) ? prop->values[1] : 0;
         if (entry && entry_value)
         {
            id = 0;
            for (j = 0; j < prop->count_enums; j++)
               if (string_is_equal(prop->enums[j].name, entry))
               {
                  *entry_value = prop->enums[j].value;
                  id           = prop->prop_id;
               }
         }
      }
      drmModeFreeProperty(prop);
   }
   drmModeFreeObjectProperties(props);
   return id;
}

/* An overlay plane besides the frame's, on the CRTC, that takes ARGB8888
 * and is above the frame's: by zpos, raised when it can be, else by
 * order. 0 when there is none. */
static uint32_t drm_overlay_find_plane(void)
{
   int i;
   int frame_idx   = -1;
   unsigned crtc   = 0;
   uint32_t found  = 0;
   uint64_t fz     = 0;
   uint32_t fz_id  = drm_plane_prop(drm.plane_id, "zpos", &fz, NULL, NULL,
         NULL, NULL);
   drmModePlaneRes *res = drmModeGetPlaneResources(drm.fd);

   if (!res)
      return 0;
   for (i = 0; i < drm.resources->count_crtcs; i++)
      if (drm.crtc_id == drm.resources->crtcs[i])
         crtc = i;
   for (i = 0; i < (int)res->count_planes; i++)
      if (res->planes[i] == drm.plane_id)
         frame_idx = i;

   for (i = 0; i < (int)res->count_planes && !found; i++)
   {
      bool fits;
      uint64_t cz = 0, cmax = 0;
      uint32_t cflags = 0, cz_id;
      drmModePlane *plane;

      if (res->planes[i] == drm.plane_id)
         continue;
      if (!(plane = drmModeGetPlane(drm.fd, res->planes[i])))
         continue;
      fits = (plane->possible_crtcs & (1u << crtc))
         && drm_plane_type(plane) == DRM_PLANE_TYPE_OVERLAY
         && format_support(plane, DRM_FORMAT_ARGB8888);
      drmModeFreePlane(plane);
      if (!fits)
         continue;

      cz_id = drm_plane_prop(res->planes[i], "zpos", &cz, &cflags, &cmax,
            NULL, NULL);
      if (fz_id && cz_id)
      {
         if (     cz <= fz
               && (     (cflags & DRM_MODE_PROP_IMMUTABLE) || fz + 1 > cmax
                     || drmModeObjectSetProperty(drm.fd, res->planes[i],
                           DRM_MODE_OBJECT_PLANE, cz_id, fz + 1)))
            continue;
      }
      /* Without zpos, later planes are drawn over earlier ones */
      else if (i < frame_idx)
         continue;
      found = res->planes[i];
   }
   drmModeFreePlaneResources(res);

   if (found)
   {
      uint64_t premul = 0;
      uint32_t flags  = 0;
      uint32_t mode   = drm_plane_prop(found, "pixel blend mode", NULL,
            &flags, NULL, "Pre-multiplied", &premul);
      if (mode && !(flags & DRM_MODE_PROP_IMMUTABLE))
         drmModeObjectSetProperty(drm.fd, found, DRM_MODE_OBJECT_PLANE,
               mode, premul);
   }
   return found;
}

static void drm_overlay_hide(struct drm_overlay *ov)
{
   if (ov->mode == DRM_OVERLAY_PLANE && ov->shown)
      drmModeSetPlane(drm.fd, ov->plane_id, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0);
   ov->shown   = false;
   ov->pending = false;
}

static void drm_overlay_free_images(struct drm_overlay *ov)
{
   unsigned i;
   for (i = 0; i < ov->count; i++)
   {
      free(ov->img[i].px);
      free(ov->img[i].scaled);
   }
   free(ov->img);
   ov->img   = NULL;
   ov->count = 0;
}

static void drm_overlay_free(struct drm_overlay *ov)
{
   drm_overlay_hide(ov);
   modeset_destroy_dumbfb(&ov->page[0]);
   modeset_destroy_dumbfb(&ov->page[1]);
   drm_overlay_free_images(ov);
   free(ov->layer);
   ov->layer = NULL;
}

static bool drm_overlay_load(void *data, const void *image_data,
      unsigned num_images)
{
   unsigned i;
   size_t k, n;
   struct drm_video *v              = (struct drm_video*)data;
   const struct texture_image *imgs = (const struct texture_image*)image_data;
   struct drm_overlay *ov;

   if (!v)
      return false;
   ov = &v->ov;
   drm_overlay_free_images(ov);
   ov->geom_dirty  = true;
   ov->layer_dirty = true;
   if (!num_images || !imgs)
      return true;
   if (!(ov->img = (struct drm_overlay_image*)calloc(num_images,
               sizeof(*ov->img))))
      return false;
   ov->count = num_images;

   for (i = 0; i < num_images; i++)
   {
      struct drm_overlay_image *o = &ov->img[i];
      o->tex[2]  = o->tex[3]  = 1.0f;
      o->vert[2] = o->vert[3] = 1.0f;
      o->alpha   = 1.0f;
      if (     !imgs[i].pixels || !imgs[i].width || !imgs[i].height
            || imgs[i].pix10 || imgs[i].fp16)
         continue;
      n = (size_t)imgs[i].width * imgs[i].height;
      if (!(o->px = (uint32_t*)malloc(n * sizeof(uint32_t))))
         continue;
      o->w = imgs[i].width;
      o->h = imgs[i].height;
      /* Premultiplied, as the plane blends and the layer is composed */
      for (k = 0; k < n; k++)
      {
         uint32_t p = imgs[i].pixels[k];
         uint32_t a = p >> 24;
         uint32_t r = imgs[i].supports_rgba ? (p & 0xff) : ((p >> 16) & 0xff);
         uint32_t g = (p >> 8) & 0xff;
         uint32_t b = imgs[i].supports_rgba ? ((p >> 16) & 0xff) : (p & 0xff);
         o->px[k]   = (a << 24) | (((r * a + 127) / 255) << 16)
            | (((g * a + 127) / 255) << 8) | ((b * a + 127) / 255);
      }
   }
   return true;
}

static void drm_overlay_tex_geom(void *data, unsigned i,
      float x, float y, float w, float h)
{
   struct drm_video *v = (struct drm_video*)data;
   float *t;
   if (!v || i >= v->ov.count)
      return;
   t = v->ov.img[i].tex;
   if (t[0] != x || t[1] != y || t[2] != w || t[3] != h)
   {
      t[0] = x; t[1] = y; t[2] = w; t[3] = h;
      v->ov.geom_dirty = true;
   }
}

static void drm_overlay_vertex_geom(void *data, unsigned i,
      float x, float y, float w, float h)
{
   struct drm_video *v = (struct drm_video*)data;
   float *t;
   if (!v || i >= v->ov.count)
      return;
   t = v->ov.img[i].vert;
   if (t[0] != x || t[1] != y || t[2] != w || t[3] != h)
   {
      t[0] = x; t[1] = y; t[2] = w; t[3] = h;
      v->ov.geom_dirty = true;
   }
}

static void drm_overlay_full_screen(void *data, bool enable)
{
   struct drm_video *v = (struct drm_video*)data;
   if (v && v->ov.fullscreen != enable)
   {
      v->ov.fullscreen = enable;
      v->ov.geom_dirty = true;
   }
}

static void drm_overlay_set_alpha(void *data, unsigned i, float mod)
{
   struct drm_video *v = (struct drm_video*)data;
   if (v && i < v->ov.count && v->ov.img[i].alpha != mod)
   {
      v->ov.img[i].alpha  = mod;
      v->ov.layer_dirty   = true;
   }
}

static void drm_overlay_enable(void *data, bool state)
{
   struct drm_video *v = (struct drm_video*)data;
   if (!v || v->ov.enabled == state)
      return;
   v->ov.enabled     = state;
   v->ov.layer_dirty = true;
   if (!state)
      drm_overlay_hide(&v->ov);
}

/* Image @o at @w x @h, from the part of it its texture coordinates name:
 * bilinear in 8.8 fixed point, which keeps a flat colour exact */
static bool drm_overlay_scale(struct drm_overlay_image *o,
      unsigned w, unsigned h)
{
   unsigned x, y;
   int x0 = (int)(o->tex[0] * o->w + 0.5f);
   int y0 = (int)(o->tex[1] * o->h + 0.5f);
   int tw = (int)(o->tex[2] * o->w + 0.5f);
   int th = (int)(o->tex[3] * o->h + 0.5f);
   uint32_t *out;

   x0 = MAX(0, MIN(x0, (int)o->w - 1));
   y0 = MAX(0, MIN(y0, (int)o->h - 1));
   tw = MAX(1, MIN(tw, (int)o->w - x0));
   th = MAX(1, MIN(th, (int)o->h - y0));
   free(o->scaled);
   o->scaled = NULL;
   if (!(out = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t))))
      return false;

   for (y = 0; y < h; y++)
   {
      /* Texel centres, in 1/256ths */
      int sy  = (int)(((int64_t)(2 * y + 1) * th * 256) / (2 * (int)h)) - 128;
      int ya  = MAX(0, MIN(sy >> 8, th - 1));
      int yb  = MIN(ya + 1, th - 1);
      unsigned fy = (sy < 0) ? 0 : (unsigned)(sy & 0xff);
      const uint32_t *ra = o->px + (size_t)(y0 + ya) * o->w + x0;
      const uint32_t *rb = o->px + (size_t)(y0 + yb) * o->w + x0;
      for (x = 0; x < w; x++)
      {
         int sx = (int)(((int64_t)(2 * x + 1) * tw * 256) / (2 * (int)w)) - 128;
         int xa = MAX(0, MIN(sx >> 8, tw - 1));
         int xb = MIN(xa + 1, tw - 1);
         unsigned fx = (sx < 0) ? 0 : (unsigned)(sx & 0xff);
         uint32_t p00 = ra[xa], p01 = ra[xb], p10 = rb[xa], p11 = rb[xb];
         uint32_t px = 0;
         unsigned sh;
         for (sh = 0; sh < 32; sh += 8)
         {
            uint32_t top = ((p00 >> sh) & 0xff) * (256 - fx)
               + ((p01 >> sh) & 0xff) * fx;
            uint32_t bot = ((p10 >> sh) & 0xff) * (256 - fx)
               + ((p11 >> sh) & 0xff) * fx;
            px |= ((top * (256 - fy) + bot * fy + 32768) >> 16) << sh;
         }
         out[(size_t)y * w + x] = px;
      }
   }
   o->scaled = out;
   return true;
}

/* Each image placed and scaled in @base (x, y, w, h), the layer the part
 * of them inside 0, 0, @bw x @bh */
static void drm_overlay_layout(struct drm_overlay *ov, const float *base,
      int bw, int bh)
{
   unsigned i;
   int x0 = bw, y0 = bh, x1 = 0, y1 = 0;

   for (i = 0; i < ov->count; i++)
   {
      struct drm_overlay_image *o = &ov->img[i];
      float fx = base[0] + o->vert[0] * base[2];
      float fy = base[1] + o->vert[1] * base[3];
      float fw = o->vert[2] * base[2];
      float fh = o->vert[3] * base[3];
      if (fw < 0.0f)
      {
         fx += fw;
         fw  = -fw;
      }
      if (fh < 0.0f)
      {
         fy += fh;
         fh  = -fh;
      }
      o->x  = (int)(fx + 0.5f);
      o->y  = (int)(fy + 0.5f);
      o->sw = (unsigned)(fw + 0.5f);
      o->sh = (unsigned)(fh + 0.5f);
      if (     !o->px || !o->sw || !o->sh || o->sw > 8192 || o->sh > 8192
            || !drm_overlay_scale(o, o->sw, o->sh))
      {
         free(o->scaled);
         o->scaled = NULL;
         continue;
      }
      x0 = MIN(x0, MAX(o->x, 0));
      y0 = MIN(y0, MAX(o->y, 0));
      x1 = MAX(x1, MIN(o->x + (int)o->sw, bw));
      y1 = MAX(y1, MIN(o->y + (int)o->sh, bh));
   }
   if (x1 <= x0 || y1 <= y0)
      x0 = y0 = x1 = y1 = 0;
   ov->lx = x0;
   ov->ly = y0;
   for (i = 0; i < ov->count; i++)
   {
      ov->img[i].x -= x0;
      ov->img[i].y -= y0;
   }
   if (     (unsigned)(x1 - x0) != ov->lw || (unsigned)(y1 - y0) != ov->lh
         || !ov->layer)
   {
      free(ov->layer);
      ov->lw    = (unsigned)(x1 - x0);
      ov->lh    = (unsigned)(y1 - y0);
      ov->layer = (ov->lw && ov->lh) ? (uint32_t*)malloc(
            (size_t)ov->lw * ov->lh * sizeof(uint32_t)) : NULL;
      if (!ov->layer)
         ov->lw = ov->lh = 0;
   }
}

#define DRM_DIV255(x) ((((x) + 1) + ((x) >> 8)) >> 8)

/* The images over one another, each at its alpha */
static void drm_overlay_compose(struct drm_overlay *ov)
{
   unsigned i;
   int x, y;
   memset(ov->layer, 0, (size_t)ov->lw * ov->lh * sizeof(uint32_t));
   for (i = 0; i < ov->count; i++)
   {
      const struct drm_overlay_image *o = &ov->img[i];
      unsigned m = (unsigned)(MAX(0.0f, MIN(o->alpha, 1.0f)) * 255.0f + 0.5f);
      int xa = MAX(0, o->x), ya = MAX(0, o->y);
      int xb = MIN((int)ov->lw, o->x + (int)o->sw);
      int yb = MIN((int)ov->lh, o->y + (int)o->sh);
      if (!o->scaled || !m)
         continue;
      for (y = ya; y < yb; y++)
      {
         const uint32_t *src = o->scaled + (size_t)(y - o->y) * o->sw
            + (xa - o->x);
         uint32_t *dst       = ov->layer + (size_t)y * ov->lw + xa;
         for (x = xa; x < xb; x++, src++, dst++)
         {
            uint32_t s = *src, d = *dst, a, inv, c, out = 0;
            unsigned sh;
            if (m < 255)
               s = (DRM_DIV255((s >> 24) * m) << 24)
                 | (DRM_DIV255(((s >> 16) & 0xff) * m) << 16)
                 | (DRM_DIV255(((s >> 8) & 0xff) * m) << 8)
                 | DRM_DIV255((s & 0xff) * m);
            if (!(a = s >> 24))
               continue;
            inv = 255 - a;
            for (sh = 0; sh < 32; sh += 8)
            {
               c    = ((s >> sh) & 0xff) + DRM_DIV255(((d >> sh) & 0xff) * inv);
               out |= MIN(c, 255u) << sh;
            }
            *dst = out;
         }
      }
   }
}

/* Composes what changed, before the flip that shows it: into a page of
 * the plane's own, or into the layer @frame's pages are blended with */
static void drm_overlay_prepare(struct drm_video *v, struct drm_surface *frame)
{
   unsigned y;
   float base[4];
   int bw, bh;
   struct drm_overlay *ov = &v->ov;

   if (!ov->enabled || !ov->count)
      return;
   if (ov->mode == DRM_OVERLAY_UNTRIED)
   {
      if (drm.plane_id && (ov->plane_id = drm_overlay_find_plane()))
      {
         ov->fb_prop = get_plane_prop_id(ov->plane_id, "FB_ID");
         ov->mode    = ov->fb_prop ? DRM_OVERLAY_PLANE : DRM_OVERLAY_BLEND;
      }
      else
         ov->mode    = DRM_OVERLAY_BLEND;
      RARCH_LOG("[DRM] Overlay %s.\n", ov->mode == DRM_OVERLAY_PLANE
            ? "on a plane of its own" : "blended into the frame");
      ov->geom_dirty = true;
   }

   if (ov->mode == DRM_OVERLAY_PLANE)
   {
      bw = (int)VIDEO_SCALE_W(v->kms_dims);
      bh = (int)VIDEO_SCALE_H(v->kms_dims);
      if (ov->fullscreen || !drm.vp_w || !drm.vp_h)
      {
         base[0] = base[1] = 0.0f;
         base[2] = (float)bw;
         base[3] = (float)bh;
      }
      else
      {
         base[0] = (float)drm.vp_x;
         base[1] = (float)drm.vp_y;
         base[2] = (float)drm.vp_w;
         base[3] = (float)drm.vp_h;
      }
   }
   else
   {
      /* In the frame's pixels: what lies off the frame is not shown */
      if (!frame || !drm.vp_w || !drm.vp_h)
         return;
      bw = (int)VIDEO_SCALE_W(frame->src_dims);
      bh = (int)VIDEO_SCALE_H(frame->src_dims);
      if (frame->src_dims != ov->frame_dims)
      {
         ov->frame_dims = frame->src_dims;
         ov->geom_dirty = true;
      }
      if (ov->fullscreen)
      {
         float sx = (float)bw / drm.vp_w, sy = (float)bh / drm.vp_h;
         base[0]  = -drm.vp_x * sx;
         base[1]  = -drm.vp_y * sy;
         base[2]  = VIDEO_SCALE_W(v->kms_dims) * sx;
         base[3]  = VIDEO_SCALE_H(v->kms_dims) * sy;
      }
      else
      {
         base[0] = base[1] = 0.0f;
         base[2] = (float)bw;
         base[3] = (float)bh;
      }
   }

   if (ov->geom_dirty)
   {
      unsigned lw = ov->lw, lh = ov->lh;
      drm_overlay_layout(ov, base, bw, bh);
      ov->geom_dirty  = false;
      ov->layer_dirty = true;
      if (ov->mode == DRM_OVERLAY_PLANE)
      {
         /* The plane moves, and is given pages of the new size */
         drm_overlay_hide(ov);
         if (lw != ov->lw || lh != ov->lh || !ov->page[0].map)
         {
            modeset_destroy_dumbfb(&ov->page[0]);
            modeset_destroy_dumbfb(&ov->page[1]);
            if (ov->lw && ov->lh)
            {
               ov->page[0].dims = ov->page[1].dims =
                  VIDEO_SCALE_PACK(ov->lw, ov->lh);
               if (     modeset_create_dumbfb(drm.fd, &ov->page[0], 4,
                           DRM_FORMAT_ARGB8888)
                     || modeset_create_dumbfb(drm.fd, &ov->page[1], 4,
                           DRM_FORMAT_ARGB8888))
               {
                  modeset_destroy_dumbfb(&ov->page[0]);
                  modeset_destroy_dumbfb(&ov->page[1]);
               }
            }
         }
      }
   }

   if (!ov->layer || !ov->lw || !ov->lh)
      return;
   if (ov->layer_dirty)
   {
      drm_overlay_compose(ov);
      ov->layer_dirty = false;
      if (ov->mode == DRM_OVERLAY_PLANE && ov->page[ov->back].map)
      {
         struct modeset_buf *pg = &ov->page[ov->back];
         for (y = 0; y < ov->lh; y++)
            memcpy(pg->map + (size_t)y * pg->stride,
                  ov->layer + (size_t)y * ov->lw, (size_t)ov->lw * 4);
         ov->pending = true;
      }
   }

   /* Placed once; after that its pages change with the frame's flips */
   if (     ov->mode == DRM_OVERLAY_PLANE && ov->pending && !ov->shown
         && !drmModeSetPlane(drm.fd, ov->plane_id, drm.crtc_id,
               ov->page[ov->back].fb_id, 0, ov->lx, ov->ly, ov->lw, ov->lh,
               0, 0, ov->lw << 16, ov->lh << 16))
   {
      ov->shown   = true;
      ov->pending = false;
      ov->back   ^= 1;
   }
}

static void drm_overlay_flip_add(struct drm_video *v, drmModeAtomicReqPtr req)
{
   struct drm_overlay *ov;
   if (!v)
      return;
   ov = &v->ov;
   if (     ov->mode == DRM_OVERLAY_PLANE && ov->shown && ov->pending
         && drmModeAtomicAddProperty(req, ov->plane_id, ov->fb_prop,
               ov->page[ov->back].fb_id) < 0)
      ov->pending = false;
}

static void drm_overlay_flipped(struct drm_video *v)
{
   struct drm_overlay *ov;
   if (!v)
      return;
   ov = &v->ov;
   if (ov->mode == DRM_OVERLAY_PLANE && ov->shown && ov->pending)
   {
      ov->pending = false;
      ov->back   ^= 1;
   }
}

/* Where no plane is spare: the layer over the frame, in its page */
static void drm_overlay_blend(struct drm_video *v, struct drm_surface *s)
{
   int x, y;
   struct modeset_buf *buf;
   struct drm_overlay *ov;

   if (!v || s != v->main_surface)
      return;
   ov = &v->ov;
   if (     ov->mode != DRM_OVERLAY_BLEND || !ov->enabled || !ov->layer
         || !ov->lw || !ov->lh)
      return;
   buf = &s->pages[s->flip_page].buf;
   for (y = 0; y < (int)ov->lh; y++)
   {
      const uint32_t *src = ov->layer + (size_t)y * ov->lw;
      uint8_t *row        = buf->map + (size_t)(ov->ly + y) * buf->stride;
      for (x = 0; x < (int)ov->lw; x++)
      {
         uint32_t p = src[x], a = p >> 24, inv;
         if (!a)
            continue;
         inv = 255 - a;
         if (s->bpp == 4)
         {
            uint32_t *d = (uint32_t*)row + ov->lx + x;
            uint32_t q  = *d;
            *d = (MIN(((p >> 16) & 0xff) + DRM_DIV255(((q >> 16) & 0xff) * inv), 255u) << 16)
               | (MIN(((p >> 8) & 0xff) + DRM_DIV255(((q >> 8) & 0xff) * inv), 255u) << 8)
               |  MIN((p & 0xff) + DRM_DIV255((q & 0xff) * inv), 255u);
         }
         else
         {
            uint16_t *d = (uint16_t*)row + ov->lx + x;
            uint32_t q  = *d;
            uint32_t r  = ((q >> 11) & 0x1f), g = ((q >> 5) & 0x3f), b = q & 0x1f;
            r = MIN(((p >> 16) & 0xff) + DRM_DIV255(((r << 3) | (r >> 2)) * inv), 255u);
            g = MIN(((p >> 8) & 0xff) + DRM_DIV255(((g << 2) | (g >> 4)) * inv), 255u);
            b = MIN((p & 0xff) + DRM_DIV255(((b << 3) | (b >> 2)) * inv), 255u);
            *d = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
         }
      }
   }
}

static const video_overlay_interface_t drm_overlay_interface = {
   drm_overlay_enable,
   drm_overlay_load,
   NULL, /* load_textures */
   drm_overlay_tex_geom,
   drm_overlay_vertex_geom,
   drm_overlay_full_screen,
   drm_overlay_set_alpha
};

static void drm_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   *iface = &drm_overlay_interface;
}
#endif

static void *drm_init(const video_info_t *video)
{
   struct drm_video *_drmvars = (struct drm_video*)
      calloc(1, sizeof(struct drm_video));
   if (!_drmvars)
      return NULL;

   /* Setup surface parameters */
   _drmvars->menu_active      = false;
   _drmvars->rgb32            = video->rgb32;

   /* It's very important that we set aspect here because the
    * call seq when a core is loaded is gfx_init()->set_aspect()->gfx_frame()
    * and we don't want the main surface to be setup in set_aspect()
    * before we get to gfx_frame(). */
   _drmvars->current_aspect = video_driver_get_aspect_ratio();

   /* Initialize the rest of the mutexes and conditions. */
   _drmvars->vsync_condition  = scond_new();
   _drmvars->vsync_cond_mutex = slock_new();
   _drmvars->pending_mutex    = slock_new();
   _drmvars->core_dims        = 0;

   _drmvars->main_surface     = NULL;
   _drmvars->menu_surface     = NULL;


   /* DRM Init */
   if (!init_drm())
   {
      RARCH_ERR("[DRM] Failed to initialize DRM.\n");
      free(_drmvars);
      return NULL;
   }

   RARCH_LOG ("[DRM] Init successful.\n");

   _drmvars->kms_dims   = VIDEO_SCALE_PACK(drm.current_mode->hdisplay,
         drm.current_mode->vdisplay);
   /* What views are laid out on */
   video_driver_set_output_dims(_drmvars->kms_dims);

   return _drmvars;
}

/* A core's views: laid out on the screen, composed at the scale that
 * draws no view larger than it is, into a surface the plane enlarges to
 * the whole screen. Each view straight from the frame. False when the
 * frame is to be drawn whole. */
static bool drm_views_frame(struct drm_video *v, const void *frame,
      unsigned dims, unsigned pitch, const video_frame_info_t *info)
{
   float scale;
   unsigned cdims;
   struct drm_page *page;
   const video_views_layout_t *layout;
   bool views = info && frame && info->views.num_views
      && video_views_fit_frame(&info->views, dims);

   retro_atomic_store_release_int(&v->views_fallback,
         (info && info->views.num_views && !views) ? 1 : 0);
   if (!views)
   {
      if (v->views_surface)
      {
         /* The plane goes back to the frame before the surface goes */
         if (v->main_surface)
            drm_plane_setup(v->main_surface);
         drm_surface_free(v, &v->views_surface);
      }
      return false;
   }

   layout = &info->views_layout;
   scale  = video_views_cpu_scale(&info->views, layout);
   cdims  = video_views_cpu_dims(info->dims, scale);
   if (!v->views_surface || v->views_surface->src_dims != cdims)
   {
      struct drm_surface *old = v->views_surface;
      v->views_surface        = NULL;
      if (!drm_surface_setup(v, cdims, VIDEO_SCALE_W(cdims) * 4, 4,
               DRM_FORMAT_XRGB8888, 255,
               (float)VIDEO_SCALE_W(v->kms_dims) / VIDEO_SCALE_H(v->kms_dims),
               3, 0, &v->views_surface))
      {
         v->views_surface = old;
         return false;
      }
      if (!v->menu_active)
         drm_plane_setup(v->views_surface);
      drm_surface_free(v, &old);
   }
   if (layout->offscreen)
   {
      size_t size = (size_t)2 * VIDEO_SCALE_AREA(cdims) * sizeof(uint32_t);
      if (size > v->views_canvas_size)
      {
         uint32_t *grown = (uint32_t*)realloc(v->views_canvas, size);
         if (!grown)
            return false;
         v->views_canvas      = grown;
         v->views_canvas_size = size;
      }
   }
   if (!v->views_seen)
   {
      v->views_seen = true;
      RARCH_LOG("[DRM] Views drawn straight from the frame.\n");
   }

   page = &v->views_surface->pages[v->views_surface->flip_page];
   video_views_cpu_compose(&v->views_cpu, &info->views, layout, frame,
         pitch, v->rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565,
         info->video_smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT,
         scale, cdims, (uint32_t*)page->buf.map, page->buf.stride / 4,
         layout->offscreen ? v->views_canvas : NULL);
   if (!v->menu_active)
      drm_page_flip(v->views_surface);
   else
      v->views_surface->flip_page = !v->views_surface->flip_page;
   return true;
}

static bool drm_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   unsigned width = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
   struct drm_video *_drmvars = data;
#ifdef HAVE_MENU
   bool menu_is_alive         = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   /* A dupe leaves the plane on the frame it shows */
   if (frame && _drmvars->core_dims != dims)
   {
      /* The plane reads from the old surface until it is moved to the
       * new one, so the old one goes only after that. */
      struct drm_surface *old_surface = NULL;

      /* Sanity check. */
      if (width == 0 || height == 0)
         return true;

      _drmvars->core_dims   = dims;
      _drmvars->core_pitch  = pitch;

      old_surface            = _drmvars->main_surface;
      _drmvars->main_surface = NULL;

      /* We need to recreate the main surface and it's pages (buffers). */
      if (!drm_surface_setup(_drmvars,
            dims,
            pitch,
            _drmvars->rgb32 ? 4 : 2,
            _drmvars->rgb32 ? DRM_FORMAT_XRGB8888 : DRM_FORMAT_RGB565,
            255,
            _drmvars->current_aspect,
            3,
            0,
            &_drmvars->main_surface))
      {
         drm_surface_free(_drmvars, &old_surface);
         _drmvars->core_dims = 0;
         return false;
      }

      /* We need to change the plane to read from the main surface */
      if (!_drmvars->views_surface)
         drm_plane_setup(_drmvars->main_surface);
      /* The cached frame can point into a page the core rendered to */
      if (old_surface && old_surface->lent)
         video_driver_cached_frame_retire();
      drm_surface_free(_drmvars, &old_surface);
   }

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   if (!frame || !_drmvars->main_surface)
      return true;

#ifdef HAVE_OVERLAY
   drm_overlay_prepare(_drmvars, _drmvars->main_surface);
#endif

   if (drm_views_frame(_drmvars, frame, dims, pitch, video_info))
      return true;

   /* Update main surface: locate free page, blit and flip. */
   drm_surface_update(_drmvars, frame, pitch, _drmvars->main_surface);
   return true;
}

static void drm_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct drm_video *_drmvars = data;

   /* If menu was active but it's not anymore... */
   if (!state && _drmvars->menu_active)
   {
      /* We tell only the plane we have to read from the main surface
       * again, or the views' */
      drm_plane_setup(_drmvars->views_surface
            ? _drmvars->views_surface : _drmvars->main_surface);
      /* We free the menu surface buffers */
      drm_surface_free(_drmvars, &_drmvars->menu_surface);
   }

   _drmvars->menu_active = state;
}

/* The menu's surface for frames of @dims: XRGB8888, or RGUI's own
 * RGBA4444 where the plane scans that out, alpha ignored as before */
static struct drm_surface *drm_menu_surface(struct drm_video *v,
      bool rgb32, unsigned dims)
{
   struct drm_surface *old = v->menu_surface;
   uint32_t fmt            = DRM_FORMAT_XRGB8888;
   unsigned bpp            = 4;

   if (!v->menu_probed || v->menu_probed_plane != drm.plane_id)
   {
      v->menu_rgbx4444     = drm_plane_takes(DRM_FORMAT_RGBX4444);
      v->menu_probed_plane = drm.plane_id;
      v->menu_probed       = true;
   }
   if (!rgb32 && v->menu_rgbx4444)
   {
      fmt = DRM_FORMAT_RGBX4444;
      bpp = 2;
   }
   if (old && old->src_dims == dims && old->pixformat == fmt)
      return old;

   v->menu_surface = NULL;
   if (!drm_surface_setup(v, dims, VIDEO_SCALE_W(dims) * bpp, bpp, fmt,
            210, v->current_aspect, 2, 0, &v->menu_surface))
   {
      v->menu_surface = old;
      return NULL;
   }
   /* The plane moves to the new surface before the old one goes */
   drm_plane_setup(v->menu_surface);
   drm_surface_free(v, &old);
   return v->menu_surface;
}

/* RGUI renders into the page the next flip shows */
static void *drm_get_menu_framebuffer(void *data, bool rgb32,
      unsigned dims, size_t *pitch)
{
   struct drm_video *v = (struct drm_video*)data;
   struct drm_surface *s;

   if (     !v || !pitch || !v->menu_active
         || !(s = drm_menu_surface(v, rgb32, dims))
         || s->bpp != (rgb32 ? 4u : 2u))
      return NULL;
   *pitch = s->pages[s->flip_page].buf.stride;
   return s->pages[s->flip_page].buf.map;
}

static void drm_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   unsigned width = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
   unsigned int i;
   struct drm_video    *_drmvars = data;
   struct drm_surface  *surface  = NULL;
   uint8_t             *dst_base = NULL;
   unsigned int         dst_pitch;

   if (!_drmvars->menu_active || !frame)
      return;
   if (!(surface = drm_menu_surface(_drmvars, rgb32, dims)))
      return;

   dst_base  = (uint8_t*)surface->pages[surface->flip_page].buf.map;
   dst_pitch = surface->pages[surface->flip_page].buf.stride;

   /* A loaned page holds the frame already */
   if ((const uint8_t*)frame != dst_base)
   {
      if (rgb32 || surface->bpp == 2)
      {
         /* The surface's own format: row by row, for the stride */
         const uint8_t *src      = (const uint8_t*)frame;
         unsigned int   row_bytes = width * surface->bpp;

         for (i = 0; i < height; i++)
            memcpy(dst_base + (dst_pitch * i), src + (row_bytes * i),
                  row_bytes);
      }
      else
      {
         /* RGUI's RGBA4444 (R = bits 15..12 down to A = 3..0) into
          * XRGB8888, each nibble replicated to 8 bits */
         for (i = 0; i < height; i++)
         {
            const uint16_t *src_row = (const uint16_t*)frame + (width * i);
            uint32_t       *dst_row = (uint32_t*)(dst_base + (dst_pitch * i));
            unsigned int    j;

            for (j = 0; j < width; j++)
            {
               uint16_t src_pix = src_row[j];
               uint32_t r4      = (src_pix >> 12) & 0xF;
               uint32_t g4      = (src_pix >>  8) & 0xF;
               uint32_t b4      = (src_pix >>  4) & 0xF;
               dst_row[j]       = (((r4 << 4) | r4) << 16)
                  | (((g4 << 4) | g4) << 8) | ((b4 << 4) | b4);
            }
         }
      }
   }

#ifdef HAVE_OVERLAY
   drm_overlay_prepare(_drmvars, NULL);
#endif
   /* The bytes are in place in the dumb buffer; commit the page flip. */
   drm_page_flip(surface);
}

/* The page the next flip shows, for the core to render into: the frame
 * then goes to the screen without a copy. Not while views compose from
 * the frame, which would read it back from uncached memory. */
static bool drm_get_current_software_framebuffer(void *data,
      struct retro_framebuffer *fb)
{
   struct drm_video *v = (struct drm_video*)data;
   struct drm_surface *s;
   struct modeset_buf *buf;

   if (     !v || !fb || !(s = v->main_surface) || v->views_surface
         || (fb->access_flags & RETRO_MEMORY_ACCESS_READ)
         || s->src_dims != VIDEO_SCALE_PACK(fb->width, fb->height))
      return false;

   buf              = &s->pages[s->flip_page].buf;
   s->lent          = true;
   fb->data         = buf->map;
   fb->pitch        = buf->stride;
   fb->format       = v->rgb32
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   fb->memory_flags = 0;
   return true;
}

static void drm_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool drm_alive(void *data) { return true; }
static bool drm_focus(void *data) { return true; }

static void drm_viewport_info(void *data, struct video_viewport *vp)
{
   struct drm_video *vid = data;

   if (!vid)
      return;

   vp->pos = VIDEO_POS_PACK(0, 0);

   /* The whole screen while views draw */
   vp->dims   = vp->full_dims   = vid->views_surface
      ? vid->kms_dims : vid->core_dims;
}

static bool drm_suppress_screensaver(void *a, bool b) { return false; }
static bool drm_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static void drm_set_aspect_ratio (void *data, unsigned aspect_ratio_idx)
{
   struct drm_video *_drmvars = data;
   /* Here we obtain the new aspect ratio. */
   float new_aspect = aspectratio_lut[aspect_ratio_idx].value;

   if (_drmvars && _drmvars->current_aspect != new_aspect)
   {
      _drmvars->current_aspect = new_aspect;
      drm_surface_set_aspect(_drmvars->main_surface, new_aspect);
      if (_drmvars->menu_active)
      {
         drm_surface_set_aspect(_drmvars->menu_surface, new_aspect);
         drm_plane_setup(_drmvars->menu_surface);
      }
   }
}

static uint32_t drm_get_flags(void *data)
{
   uint32_t flags             = 0;
   struct drm_video *_drmvars = data;
   if (_drmvars)
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      if (retro_atomic_load_acquire_int(&_drmvars->views_fallback))
         BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
   return flags;
}

static const video_poke_interface_t drm_poke_interface = {
   drm_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
   NULL, /* refresh_rate - handled by display server */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   drm_set_aspect_ratio,
   NULL, /* drm_apply_state_changes */
   drm_set_texture_frame,
   drm_set_texture_enable,
   NULL, /* drm_set_osd_msg */
   NULL, /* drm_show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   drm_get_current_software_framebuffer,
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL, /* set_hdr_subpixel_layout */
   NULL, /* supports_texture_format */
   NULL, /* load_texture_compressed */
   NULL, /* present_last */
   NULL, /* get_last_present_time */
   NULL, /* hw_ring_install */
   NULL, /* hw_ring_fence_new */
   NULL, /* hw_ring_fence_free */
   NULL, /* hw_ring_fence_signal */
   NULL, /* hw_ring_fence_wait */
   NULL, /* hw_ring_capture */
   NULL, /* hw_ring_present_slot */
   NULL, /* hw_ring_context_new */
   NULL, /* hw_ring_context_free */
   NULL, /* hw_ring_framebuffer */
   NULL, /* update_texture */
   NULL, /* get_swap_interval_cap */
   NULL, /* texture_lend */
   NULL, /* texture_lend_ready */
   NULL, /* get_last_present_wait */
   NULL, /* set_view_count */
   NULL, /* hw_context_destroying */
   NULL, /* get_headset_refresh */
   NULL, /* set_frame_format */
   NULL, /* frame_mem_alloc */
   NULL, /* frame_mem_ready */
   NULL, /* frame_mem_free */
   NULL, /* get_device */
   NULL, /* import_texture */
   NULL, /* get_last_present_sync */
   NULL, /* record_read */
   NULL, /* import_movie_frame */
   drm_get_menu_framebuffer
};

static void drm_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &drm_poke_interface;
}

static void drm_gfx_free(void *data)
{
   struct drm_video *_drmvars = data;

   if (!_drmvars)
      return;

#ifdef HAVE_OVERLAY
   drm_overlay_free(&_drmvars->ov);
#endif
   if (_drmvars->main_surface && _drmvars->main_surface->lent)
      video_driver_cached_frame_retire();
   drm_surface_free(_drmvars, &_drmvars->main_surface);

   if (_drmvars->menu_surface)
      drm_surface_free(_drmvars, &_drmvars->menu_surface);
   drm_surface_free(_drmvars, &_drmvars->views_surface);
   video_views_cpu_free(&_drmvars->views_cpu);
   free(_drmvars->views_canvas);

   /* Destroy mutexes and conditions. */
   slock_free(_drmvars->pending_mutex);
   slock_free(_drmvars->vsync_cond_mutex);
   scond_free(_drmvars->vsync_condition);

   g_drm_mode = NULL;

   if (drm.flip_req)
   {
      drmModeAtomicFree(drm.flip_req);
      drm.flip_req = NULL;
   }

   free(_drmvars);
}

video_driver_t video_drm = {
   drm_init,
   drm_frame,
   drm_set_nonblock_state,
   drm_alive,
   drm_focus,
   drm_suppress_screensaver,
   NULL, /* has_windowed */
   drm_set_shader,
   drm_gfx_free,
   "drm",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   drm_viewport_info,
   NULL, /* read_viewport */
#ifdef HAVE_OVERLAY
   drm_get_overlay_interface,
#endif
   drm_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
