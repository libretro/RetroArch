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
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <retro_atomic.h>
#include <string/stdstring.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../common/drm_common.h"
#include "../video_views.h"
#include "../video_views_cpu.h"
#include "../video_turn.h"
#include "../font_driver.h"
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

/* One buffer each; whether it is on screen, on its way there or free
 * is told by the surface's current_page and drm.pending */
struct drm_page
{
   struct modeset_buf buf;

   struct drm_video *drmvars;

   /* This field will allow us to access the
    * surface the page belongs to. */
   struct drm_surface *surface;
};

/* The threaded wrapper's frame buffers: dumb buffers it writes frames
 * into, scanned out where they lie through a framebuffer made for the
 * frame's size, pitch and place in the buffer */
#define DRM_FRAME_MEM 8
struct drm_frame_mem
{
   struct drm_page page;
   uint8_t *base;
   size_t size;
   uint32_t handle;
   unsigned dims;
   unsigned pitch;
   size_t offset;
   uint32_t fmt;
   /* Freed while the plane still shows it: goes once it does not */
   bool doomed;
   /* The kernel took no framebuffer for this size and pitch */
   bool refused;
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
   /* Quarter turns counter-clockwise: rot written into the pages by the
    * CPU, hw_rot made by the plane */
   unsigned rot;
   unsigned hw_rot;
   /* The page the next frame is written into */
   unsigned flip_page;
   /* A page went to the core to render into */
   bool lent;
};

/* What the next commit of @surface may write into: not the page on
 * screen, nor one a flip is still taking there */
static unsigned drm_next_page(struct drm_surface *surface);

struct drm_struct
{
   /* DRM connection, mode and plane management stuff */
   int fd;
   drmModeModeInfo *current_mode;
   uint32_t crtc_id;
   uint32_t connector_id;

   /* What the CRTC shows below the planes, at the mode it runs */
   struct modeset_buf console;
   drmModeModeInfo mode;

   uint32_t plane_id;
   uint32_t plane_fb_prop_id;
   /* Reused by every flip */
   drmModeAtomicReqPtr flip_req;
   /* Where the plane puts the surface it shows, in screen pixels */
   int vp_x;
   int vp_y;
   unsigned vp_w;
   unsigned vp_h;

   drmModeRes *resources;
   /* The frame plane's rotation property, the plane it was asked of and
    * the quarter turns it takes, one bit each */
   uint32_t rot_prop;
   uint32_t rot_plane;
   unsigned rot_turns;
   /* A page a commit that did not wait is taking to the screen */
   struct drm_page *pending;
   /* That commit also takes the overlay's back page up */
   bool pending_ov;
   drmEventContext evctx;
   /* Flips are atomic commits; legacy plane updates without them */
   bool atomic;
   /* Without vsync: a flip does not wait for the one before, and a
    * frame that comes before it is done is not shown */
   bool nonblock;
} drm = { -1 };

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

   /* The plane last asked whether it takes RGUI's format, and its answer */
   uint32_t menu_probed_plane;

   /* Quarter turns counter-clockwise the frame is shown at */
   unsigned rotation;
   bool rotation_dirty;
   /* The frame's place on the screen changed: the next frame is put
    * there rather than flipped to */
   bool plane_dirty;

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
   struct drm_frame_mem fmem[DRM_FRAME_MEM];
   /* On-screen messages, drawn into the frame at a size that comes out
    * at the font size set once the plane has scaled the frame */
   const font_renderer_driver_t *font_driver;
   void *font;
   unsigned font_px;
   bool font_enable;
   /* The font file asked for at init, empty for the renderer's own */
   char font_path[PATH_MAX_LENGTH];
   /* This frame's message, while it is drawn */
   const char *msg;
   const video_frame_info_t *msg_info;
   /* The row pitch multiple the kernel gives dumb buffers, 0 unasked */
   unsigned fmem_align;
   /* Both main and menu surfaces are going to have the same aspect,
    * so we keep it here for future reference. */
   float current_aspect;

};

#ifdef HAVE_OVERLAY
static void drm_overlay_flipped(struct drm_video *v);
#endif

static void drm_flip_handler(int fd, unsigned frame, unsigned sec,
      unsigned usec, void *data)
{
   struct drm_page *page = drm.pending;
   (void)fd; (void)frame; (void)sec; (void)usec; (void)data;
   if (!page)
      return;
   drm.pending                 = NULL;
   page->surface->current_page = page;
#ifdef HAVE_OVERLAY
   if (drm.pending_ov)
      drm_overlay_flipped(page->drmvars);
#endif
   drm.pending_ov              = false;
}

/* Takes the event of a flip done, waiting up to @timeout ms (-1: until
 * it comes); true when no flip is left on its way */
static bool drm_flip_drain(int timeout)
{
   struct pollfd pfd;

   if (!drm.pending)
      return true;
   pfd.fd      = drm.fd;
   pfd.events  = POLLIN;
   pfd.revents = 0;
   if (poll(&pfd, 1, timeout) > 0 && (pfd.revents & POLLIN))
   {
      drm.evctx.version           = 2;
      drm.evctx.page_flip_handler = drm_flip_handler;
      drmHandleEvent(drm.fd, &drm.evctx);
   }
   return !drm.pending;
}

/* Before anything that would change what a flip on its way reads: a
 * vblank or two at most, so a flip that never reports is let go */
static void drm_flip_wait(void)
{
   int i;
   for (i = 0; i < 10 && !drm_flip_drain(100); i++) { }
   if (drm.pending)
   {
      RARCH_WARN("[DRM] A flip never reported done.\n");
      drm_flip_handler(drm.fd, 0, 0, 0, NULL);
   }
}

static int drm_set_plane(uint32_t plane, uint32_t crtc, uint32_t fb,
      uint32_t flags, int32_t cx, int32_t cy, uint32_t cw, uint32_t ch,
      uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh)
{
   drm_flip_wait();
   return drmModeSetPlane(drm.fd, plane, crtc, fb, flags, cx, cy, cw, ch,
         sx, sy, sw, sh);
}

/* Some prototypes for later use */

static int modeset_create_dumbfb(int fd,
      struct modeset_buf *buf, int bpp, uint32_t pixformat);

static void modeset_destroy_dumbfb(struct modeset_buf *buf);

/* The planes off and the CRTC as it was found, then the device let go */
static void deinit_drm(void)
{
   if (drm.fd < 0)
      return;
   drm_flip_wait();
   if (drm.plane_id)
      drm_set_plane(drm.plane_id, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0);
   /* The next to use the plane finds it unturned */
   if (drm.rot_prop)
      drmModeObjectSetProperty(drm.fd, drm.rot_plane, DRM_MODE_OBJECT_PLANE,
            drm.rot_prop, DRM_MODE_ROTATE_0);
   drm_restore_crtc();
   modeset_destroy_dumbfb(&drm.console);
   if (drm.flip_req)
      drmModeAtomicFree(drm.flip_req);
   if (drm.resources)
      drmModeFreeResources(drm.resources);
   drm_free();
   close(drm.fd);
   memset(&drm, 0, sizeof(drm));
   drm.fd     = -1;
   g_drm_fd   = -1;
   g_drm_mode = NULL;
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
   unsigned i;
   struct drm_surface *surface;

   if (!sp || !(surface = *sp))
      return;
   if (drm.pending && drm.pending->surface == surface)
      drm_flip_wait();

   for (i = 0; i < surface->numpages; i++)
      modeset_destroy_dumbfb(&surface->pages[i].buf);

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
   unsigned i;
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
      surface->pages[i].surface = surface;
      surface->pages[i].drmvars = _drmvars;
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

/* @page to the screen in place of @surface's: one of its own, the
 * next to be written moving on past it, or frame memory */
static void drm_page_flip_to(struct drm_surface *surface,
      struct drm_page *page)
{
#ifdef HAVE_OVERLAY
   struct drm_video *v = surface->pages[0].drmvars;
#endif
   bool own                = page == &surface->pages[surface->flip_page];
   /* We already have the id of the FB_ID property of
    * the plane on which we are going to do a pageflip:
    * we got it back in drm_plane_setup()  */
   drmModeAtomicReqPtr req = drm.flip_req;

   page->surface = surface;
   if (!drm.atomic)
   {
      /* The plane's update waits for the vblank, as the commit does */
      if (drmModeSetPlane(drm.fd, drm.plane_id, drm.crtc_id,
               page->buf.fb_id, 0,
               drm.vp_x, drm.vp_y, drm.vp_w, drm.vp_h, 0, 0,
               VIDEO_SCALE_W(surface->src_dims) << 16,
               VIDEO_SCALE_H(surface->src_dims) << 16))
         RARCH_ERR("[DRM] Failed to flip the plane.\n");
      surface->current_page = page;
      if (own)
         surface->flip_page = drm_next_page(surface);
      return;
   }

   /* Without vsync, a frame that comes while the last is still on its
    * way is not shown: the next is written over it. With vsync, the
    * commit waits for the vblank, and for a flip on its way first. */
   if (drm.nonblock)
   {
      if (!drm_flip_drain(0))
         return;
   }
   else
      drm_flip_wait();

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
         page->buf.fb_id) < 0)
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
   if (drm.nonblock)
   {
      if (drmModeAtomicCommit(drm.fd, req,
               DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT,
               NULL) < 0)
      {
         /* Not taken: the page is written over by the next frame */
         RARCH_ERR("[DRM] Failed to commit for pageflip.\n");
         return;
      }
      drm.pending        = page;
#ifdef HAVE_OVERLAY
      drm.pending_ov     = v && v->ov.pending;
#endif
      if (own)
         surface->flip_page = drm_next_page(surface);
      return;
   }

   if (drmModeAtomicCommit(drm.fd, req, 0, NULL) < 0)
   {
      RARCH_ERR("[DRM] Failed to commit for pageflip.\n");
   }

#ifdef HAVE_OVERLAY
   drm_overlay_flipped(v);
#endif
   surface->current_page = page;
   if (own)
      surface->flip_page = drm_next_page(surface);
}

static void drm_page_flip(struct drm_surface *surface)
{
   drm_page_flip_to(surface, &surface->pages[surface->flip_page]);
}

static unsigned drm_next_page(struct drm_surface *surface)
{
   unsigned i;
   for (;;)
   {
      for (i = 0; i < surface->numpages; i++)
         if (     &surface->pages[i] != surface->current_page
               && &surface->pages[i] != drm.pending)
            return i;
      /* Two pages, one shown and one on its way */
      drm_flip_wait();
   }
}

static bool drm_fmem_busy(const struct drm_video *v,
      const struct drm_frame_mem *m)
{
   return &m->page == drm.pending || (v->main_surface
         && &m->page == v->main_surface->current_page);
}

static void drm_fmem_destroy(struct drm_frame_mem *m)
{
   m->page.buf.map    = m->base;
   m->page.buf.size   = m->size;
   m->page.buf.handle = m->handle;
   modeset_destroy_dumbfb(&m->page.buf);
   memset(m, 0, sizeof(*m));
}

/* What was freed while it was shown goes once it is not */
static void drm_fmem_sweep(struct drm_video *v)
{
   unsigned i;
   for (i = 0; i < DRM_FRAME_MEM; i++)
      if (v->fmem[i].doomed && !drm_fmem_busy(v, &v->fmem[i]))
         drm_fmem_destroy(&v->fmem[i]);
}

/* The frame memory @frame lies in, with a framebuffer showing @dims
 * from there at @pitch; NULL to copy it instead */
static struct drm_page *drm_fmem_page(struct drm_video *v,
      const void *frame, unsigned dims, unsigned pitch, uint32_t fmt,
      unsigned bpp)
{
   unsigned i;
   const uint8_t *f = (const uint8_t*)frame;

   for (i = 0; i < DRM_FRAME_MEM; i++)
   {
      struct drm_frame_mem *m = &v->fmem[i];
      struct drm_mode_fb_cmd2 cmd;
      size_t off;
      if (     !m->base || m->doomed || f < m->base
            || f >= m->base + m->size)
         continue;
      off = (size_t)(f - m->base);
      if (     (size_t)pitch * (VIDEO_SCALE_H(dims) - 1)
               + (size_t)VIDEO_SCALE_W(dims) * bpp > m->size - off
            || pitch < VIDEO_SCALE_W(dims) * bpp)
         return NULL;
      if (     m->page.buf.fb_id && m->dims == dims && m->pitch == pitch
            && m->offset == off && m->fmt == fmt)
         return &m->page;
      if (     m->refused && m->dims == dims && m->pitch == pitch
            && m->offset == off && m->fmt == fmt)
         return NULL;
      /* Its framebuffer can go only off the screen: the kernel turns a
       * plane off when the one it shows is removed. A frame placed
       * otherwise in a buffer still shown is copied. */
      if (m->page.buf.fb_id && drm_fmem_busy(v, m))
         return NULL;
      if (m->page.buf.fb_id)
      {
         drmModeRmFB(drm.fd, m->page.buf.fb_id);
         m->page.buf.fb_id = 0;
      }
      m->dims    = dims;
      m->pitch   = pitch;
      m->offset  = off;
      m->fmt     = fmt;
      m->refused = false;
      memset(&cmd, 0, sizeof(cmd));
      cmd.width        = VIDEO_SCALE_W(dims);
      cmd.height       = VIDEO_SCALE_H(dims);
      cmd.pixel_format = fmt;
      cmd.handles[0]   = m->handle;
      cmd.pitches[0]   = pitch;
      cmd.offsets[0]   = (uint32_t)off;
      if (drmIoctl(drm.fd, DRM_IOCTL_MODE_ADDFB2, &cmd))
      {
         RARCH_WARN("[DRM] Frames at a pitch of %u are copied.\n", pitch);
         m->refused = true;
         return NULL;
      }
      m->page.buf.fb_id        = cmd.fb_id;
      m->page.buf.map          = m->base + off;
      m->page.buf.stride       = pitch;
      m->page.buf.dims         = dims;
      m->page.buf.pixel_format = fmt;
      m->page.drmvars          = v;
      return &m->page;
   }
   return NULL;
}

/* A frame buffer for the threaded wrapper: a dumb buffer of @size
 * bytes, the frames written into it flipped to without a copy */
static void *drm_frame_mem_alloc(void *data, size_t size, uintptr_t *id,
      bool *cached, unsigned *pitch_align)
{
   unsigned i;
   struct drm_video *v                     = (struct drm_video*)data;
   struct drm_mode_create_dumb create_dumb = {0};
   struct drm_mode_map_dumb map_dumb       = {0};
   struct drm_frame_mem *m                 = NULL;
   void *map;

   if (!v || !size || drm.fd < 0)
      return NULL;
   for (i = 0; i < DRM_FRAME_MEM && !m; i++)
      if (!v->fmem[i].base)
         m = &v->fmem[i];
   if (!m)
      return NULL;

   /* The pitch the kernel gives the narrowest buffer is the multiple
    * the display reads rows at */
   if (!v->fmem_align)
   {
      struct drm_mode_destroy_dumb destroy_dumb = {0};
      create_dumb.width  = 1;
      create_dumb.height = 1;
      create_dumb.bpp    = 32;
      if (drmIoctl(drm.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb))
         return NULL;
      destroy_dumb.handle = create_dumb.handle;
      drmIoctl(drm.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
      v->fmem_align = create_dumb.pitch < 64 ? 64 : create_dumb.pitch;
      memset(&create_dumb, 0, sizeof(create_dumb));
   }

   create_dumb.width  = 1024;
   create_dumb.height = (uint32_t)((size + 4095) / 4096);
   create_dumb.bpp    = 32;
   if (drmIoctl(drm.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb))
      return NULL;
   map_dumb.handle = create_dumb.handle;
   if (     drmIoctl(drm.fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb)
         || (map = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE,
               MAP_SHARED, drm.fd, map_dumb.offset)) == MAP_FAILED)
   {
      struct drm_mode_destroy_dumb destroy_dumb = {0};
      destroy_dumb.handle = create_dumb.handle;
      drmIoctl(drm.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
      return NULL;
   }
   memset(m, 0, sizeof(*m));
   m->base   = (uint8_t*)map;
   m->size   = create_dumb.size;
   m->handle = create_dumb.handle;
   *id       = (uintptr_t)(m - v->fmem) + 1;
   if (cached)
      *cached = false;
   if (pitch_align)
      *pitch_align = v->fmem_align;
   return map;
}

static bool drm_frame_mem_ready(void *data, uintptr_t id)
{
   struct drm_video *v = (struct drm_video*)data;
   if (!v || !id || id > DRM_FRAME_MEM)
      return false;
   drm_flip_drain(0);
   return !drm_fmem_busy(v, &v->fmem[id - 1]);
}

static void drm_frame_mem_free(void *data, uintptr_t id)
{
   struct drm_video *v = (struct drm_video*)data;
   struct drm_frame_mem *m;
   if (!v || !id || id > DRM_FRAME_MEM || !(m = &v->fmem[id - 1])->base)
      return;
   drm_flip_drain(0);
   if (drm_fmem_busy(v, m))
      m->doomed = true;
   else
      drm_fmem_destroy(m);
}

/* @msg drawn into @buf, @w x @h pixels the plane shows turned @rot
 * quarter turns counter-clockwise, laid out as it is seen */
static void drm_draw_msg(struct drm_video *v, struct modeset_buf *buf,
      unsigned w, unsigned h, unsigned bpp, unsigned rot, const char *msg,
      const video_frame_info_t *info)
{
   int x, y;
   unsigned px, fr, fg, fb;
   const struct font_atlas *atlas;
   const struct font_glyph *(*get_glyph)(void*, uint32_t);
   const struct font_glyph *glyph_q;
   struct font_line_metrics *line_metrics = NULL;
   void *font_data;
   size_t msg_len;
   int dw = (int)((rot & 1) ? h : w);
   int dh = (int)((rot & 1) ? w : h);
   int line_x, line_y, msg_base_x, msg_base_y, line_h = 0;
   unsigned screen_h = VIDEO_SCALE_H(v->kms_dims);

   if (!v->font_enable || !msg || !*msg || !info || !dw || !dh)
      return;
   /* The font's size on the screen, in the frame's pixels */
   px = (unsigned)(info->font_size * (float)dh
         / (float)(screen_h ? screen_h : (unsigned)dh) + 0.5f);
   if (px < 8)
      px = 8;
   if (!v->font || v->font_px != px)
   {
      if (v->font)
         v->font_driver->free(v->font);
      v->font = NULL;
      if (!font_renderer_create_default(&v->font_driver, &v->font,
               *v->font_path ? v->font_path : NULL,
               px, FONT_ATLAS_FORMAT_A8))
      {
         v->font_enable = false;
         return;
      }
      v->font_px = px;
      /* Glyphs are read from the atlas in memory: it may grow */
      v->font_driver->get_atlas(v->font)->max_dims
         = VIDEO_SCALE_PACK(2048, 2048);
   }

   font_data  = v->font;
   get_glyph  = v->font_driver->get_glyph;
   glyph_q    = get_glyph(font_data, '?');
   msg_len    = strlen(msg);
   atlas      = v->font_driver->get_atlas(font_data);
   fr         = (unsigned)(MAX(0.0f, MIN(info->font_msg_color_r, 1.0f)) * 255.0f);
   fg         = (unsigned)(MAX(0.0f, MIN(info->font_msg_color_g, 1.0f)) * 255.0f);
   fb         = (unsigned)(MAX(0.0f, MIN(info->font_msg_color_b, 1.0f)) * 255.0f);
   msg_base_x = (int)(info->font_msg_pos_x * dw);
   msg_base_y = (int)((1.0f - info->font_msg_pos_y) * dh);
   line_x     = msg_base_x;
   line_y     = msg_base_y;
   if (v->font_driver->get_line_metrics)
      v->font_driver->get_line_metrics(font_data, &line_metrics);
   if (line_metrics)
      line_h = (int)line_metrics->height;

#define FONT_LAYOUT_ALIGNED 0
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
   do \
   { \
      (void)(line_width); (void)(count); (void)(bytes); \
      line_x = msg_base_x; \
      line_y = msg_base_y + (line) * line_h; \
   } while (0)
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
   do \
   { \
      int gw = (int)VIDEO_SCALE_W((glyph)->dims); \
      int gh = (int)VIDEO_SCALE_H((glyph)->dims); \
      int bx = line_x + (pen_x) + (glyph)->draw_offset_x; \
      int by = line_y + (pen_y) + (glyph)->draw_offset_y; \
      const uint8_t *gsrc = atlas->buffer \
         + VIDEO_SCALE_W((glyph)->atlas_pos) \
         + (size_t)VIDEO_SCALE_H((glyph)->atlas_pos) * atlas->width; \
      for (y = 0; y < gh; y++) \
      { \
         int oy = by + y; \
         if (oy < 0 || oy >= dh) \
            continue; \
         for (x = 0; x < gw; x++) \
         { \
            unsigned a = gsrc[(size_t)y * atlas->width + x]; \
            int ox = bx + x, sx, sy; \
            uint8_t *d; \
            if (!a || ox < 0 || ox >= dw) \
               continue; \
            switch (rot & 3) \
            { \
               case 1:  sx = (int)w - 1 - oy; sy = ox;              break; \
               case 2:  sx = (int)w - 1 - ox; sy = (int)h - 1 - oy; break; \
               case 3:  sx = oy;              sy = (int)h - 1 - ox; break; \
               default: sx = ox;              sy = oy;              break; \
            } \
            d = buf->map + (size_t)sy * buf->stride + (size_t)sx * bpp; \
            if (bpp == 4) \
            { \
               uint32_t q = *(uint32_t*)d; \
               *(uint32_t*)d = \
                    ((((q >> 16) & 0xff) * (255 - a) + fr * a) / 255) << 16 \
                  | ((((q >> 8) & 0xff) * (255 - a) + fg * a) / 255) << 8 \
                  |  (((q & 0xff) * (255 - a) + fb * a) / 255); \
            } \
            else \
            { \
               uint32_t q = *(uint16_t*)d; \
               uint32_t r = ((q >> 11) & 0x1f) * 255 / 31; \
               uint32_t g = ((q >> 5) & 0x3f) * 255 / 63; \
               uint32_t b = (q & 0x1f) * 255 / 31; \
               r = (r * (255 - a) + fr * a) / 255; \
               g = (g * (255 - a) + fg * a) / 255; \
               b = (b * (255 - a) + fb * a) / 255; \
               *(uint16_t*)d = (uint16_t)(((r >> 3) << 11) \
                     | ((g >> 2) << 5) | (b >> 3)); \
            } \
         } \
      } \
   } while (0)
#include "../font_layout.h"
}

static void drm_surface_update(void *data, const void *frame,
      unsigned pitch, struct drm_surface *surface)
{
   struct modeset_buf *buf = &surface->pages[surface->flip_page].buf;

   if (surface->rot)
   {
      unsigned w = VIDEO_SCALE_W(surface->src_dims);
      unsigned h = VIDEO_SCALE_H(surface->src_dims);
      if (surface->rot & 1)
      {
         unsigned t = w;
         w          = h;
         h          = t;
      }
      video_turn_copy(buf->map, buf->stride, (const uint8_t*)frame, pitch,
            w, h, surface->bpp, surface->rot);
   }
   /* A lent page holds the frame already */
   else if ((const uint8_t*)frame != buf->map)
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
   {
      struct drm_video *v = surface->pages[0].drmvars;
      if (v && v->msg)
         drm_draw_msg(v, buf, VIDEO_SCALE_W(surface->src_dims),
               VIDEO_SCALE_H(surface->src_dims), surface->bpp,
               surface->hw_rot, v->msg, v->msg_info);
   }
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

/* The plane in use when it scans out @fmt, or with none in use the
 * first overlay on the CRTC that does; 0 when there is none */
static uint32_t drm_plane_takes(uint32_t fmt)
{
   int i;
   unsigned crtc_index = 0;
   uint32_t takes      = 0;
   drmModePlaneRes *res;

   if (drm.plane_id)
   {
      drmModePlane *plane = drmModeGetPlane(drm.fd, drm.plane_id);
      if (plane)
      {
         takes = format_support(plane, fmt) ? drm.plane_id : 0;
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
      if (     (plane->possible_crtcs & (1 << crtc_index))
            && drm_plane_type(plane) == DRM_PLANE_TYPE_OVERLAY
            && format_support(plane, fmt))
         takes = plane->plane_id;
      drmModeFreePlane(plane);
   }
   drmModeFreePlaneResources(res);
   return takes;
}

/* The quarter turns plane @id can make, bit n for n turns */
static unsigned drm_plane_turns(uint32_t id)
{
   drmModeObjectProperties *props;
   int i, j;

   if (!id)
      return 1;
   if (drm.rot_plane == id)
      return drm.rot_turns;
   drm.rot_plane = id;
   drm.rot_prop  = 0;
   drm.rot_turns = 1;
   if (!(props = drmModeObjectGetProperties(drm.fd, id, DRM_MODE_OBJECT_PLANE)))
      return 1;
   for (i = 0; i < (int)props->count_props && !drm.rot_prop; i++)
   {
      drmModePropertyRes *prop = drmModeGetProperty(drm.fd, props->props[i]);
      if (!prop)
         continue;
      if (string_is_equal(prop->name, "rotation"))
      {
         drm.rot_prop = prop->prop_id;
         for (j = 0; j < prop->count_enums; j++)
         {
            /* Bit numbers: 0 to 3 are rotate-0 to rotate-270 */
            if (prop->enums[j].value < 4)
               drm.rot_turns |= 1u << prop->enums[j].value;
         }
      }
      drmModeFreeProperty(prop);
   }
   drmModeFreeObjectProperties(props);
   return drm.rot_turns;
}

/* This configures our only overlay plane to render the given surface. */
static bool drm_plane_setup(struct drm_surface *surface)
{
   unsigned i;
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
   uint64_t rotation;

   /* Get plane resources */
   drmModePlane *plane;
   drmModePlaneRes *plane_resources;
   uint32_t in_use = drm.plane_id;
   uint32_t found  = 0;
   plane_resources = drmModeGetPlaneResources(drm.fd);
   if (!plane_resources)
   {
      RARCH_ERR("[DRM] No scaling planes available.\n");
      return false;
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
      RARCH_ERR("[DRM] Couldn't find an usable overlay plane for current CRTC and framebuffer pixel format.\n");
      return false;
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

   /* The plane turns this surface, or shows it as it is: in the same
    * commit as its buffer and place, as a turn the old buffer's place
    * does not fit is refused by a kernel that checks it on its own */
   rotation = (drm_plane_turns(drm.plane_id) & (1u << surface->hw_rot))
      && drm.rot_prop ? DRM_MODE_ROTATE_0 << surface->hw_rot : 0;
   if (drm.atomic)
   {
      static const char *const names[11] = { "FB_ID", "CRTC_ID", "CRTC_X",
         "CRTC_Y", "CRTC_W", "CRTC_H", "SRC_X", "SRC_Y", "SRC_W", "SRC_H",
         "rotation" };
      uint64_t vals[11];
      drmModeAtomicReqPtr req = drmModeAtomicAlloc();
      int ok                  = req != NULL;
      vals[0]  = surface->pages[surface->flip_page].buf.fb_id;
      vals[1]  = drm.crtc_id;
      vals[2]  = plane_x;
      vals[3]  = plane_y;
      vals[4]  = plane_w;
      vals[5]  = plane_h;
      vals[6]  = (uint64_t)src_x << 16;
      vals[7]  = (uint64_t)src_y << 16;
      vals[8]  = (uint64_t)src_w << 16;
      vals[9]  = (uint64_t)src_h << 16;
      vals[10] = rotation;
      for (i = 0; ok && i < (rotation ? 11u : 10u); i++)
      {
         uint32_t prop = i == 10 ? drm.rot_prop
            : get_plane_prop_id(drm.plane_id, names[i]);
         ok = prop && drmModeAtomicAddProperty(req, drm.plane_id, prop,
               vals[i]) >= 0;
      }
      drm_flip_wait();
      if (!ok || drmModeAtomicCommit(drm.fd, req, 0, NULL))
      {
         if (req)
            drmModeAtomicFree(req);
         RARCH_ERR("[DRM] Failed to enable plane.\n");
         return false;
      }
      drmModeAtomicFree(req);
   }
   else
   {
      if (     rotation && drmModeObjectSetProperty(drm.fd, drm.plane_id,
                  DRM_MODE_OBJECT_PLANE, drm.rot_prop, rotation)
            && surface->hw_rot)
         RARCH_WARN("[DRM] The plane would not turn the frame.\n");
      /* We have to set a buffer for the plane, whatever buffer we want,
       * but we must set a buffer so the plane starts reading from it now. */
      if (drm_set_plane(drm.plane_id, drm.crtc_id,
               surface->pages[surface->flip_page].buf.fb_id,
               plane_flags, plane_x, plane_y, plane_w, plane_h,
               src_x<<16, src_y<<16, src_w<<16, src_h<<16))
      {
         RARCH_ERR("[DRM] Failed to enable plane.\n");
         return false;
      }
   }
   surface->current_page = &surface->pages[surface->flip_page];
   surface->flip_page    = drm_next_page(surface);

   RARCH_DBG("[DRM] src_w %d, src_h %d, plane_w %d, plane_h %d\n",
         src_w, src_h, plane_w, plane_h);

   /* Report what plane (of overlay type) we're using. */
   drm_format_name(surface->pixformat, fmt_name);
   RARCH_LOG("[DRM] Using plane with ID %d on CRTC ID %d format %s.\n",
         drm.plane_id, drm.crtc_id, fmt_name);
   return true;
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

static uint32_t drm_plane_takes(uint32_t fmt);

/* @path, if it drives a connected display the monitor index names that a
 * plane can show frames on */
static bool init_drm_card(const char *path, unsigned monitor_index)
{
#ifdef O_CLOEXEC
   int fd = open(path, O_RDWR | O_CLOEXEC);
#else
   int fd = open(path, O_RDWR);
#endif

   if (fd < 0)
      return false;
   /* Universal planes, or the overlays are not listed */
   drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
   drm.atomic = !drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
   drm.fd     = fd;
   g_drm_fd   = fd;

   if (     !drm_get_resources(fd)
         || !drm_get_connector(fd, monitor_index)
         || !drm_get_encoder(fd))
      goto fail;
   drm_setup(fd);
   if (     !g_orig_crtc || !g_crtc_id
         || !(drm.resources = drmModeGetResources(fd)))
      goto fail;
   drm.crtc_id      = g_crtc_id;
   drm.connector_id = g_connector_id;
   /* Picked now, so what it can do is known before the first frame */
   if (!(drm.plane_id = drm_plane_takes(DRM_FORMAT_XRGB8888)))
   {
      RARCH_WARN("[DRM] %s has no overlay plane for frames.\n", path);
      goto fail;
   }
   return true;

fail:
   if (drm.resources)
      drmModeFreeResources(drm.resources);
   drm.resources = NULL;
   if (g_orig_crtc)
      drmModeFreeCrtc(g_orig_crtc);
   g_orig_crtc = NULL;
   drm_free();
   close(fd);
   drm.fd   = -1;
   g_drm_fd = -1;
   return false;
}

/* The CRTC at @mode, showing a blank frame below the planes */
static bool drm_set_mode(const drmModeModeInfo *mode)
{
   struct modeset_buf old = drm.console;

   memset(&drm.console, 0, sizeof(drm.console));
   drm.console.dims = VIDEO_SCALE_PACK(mode->hdisplay, mode->vdisplay);
   if (     modeset_create_dumbfb(drm.fd, &drm.console, 4,
               DRM_FORMAT_XRGB8888)
         || drmModeSetCrtc(drm.fd, drm.crtc_id, drm.console.fb_id, 0, 0,
               &drm.connector_id, 1, (drmModeModeInfo*)mode))
   {
      modeset_destroy_dumbfb(&drm.console);
      drm.console = old;
      return false;
   }
   modeset_destroy_dumbfb(&old);
   drm.mode         = *mode;
   drm.current_mode = &drm.mode;
   g_drm_mode       = &drm.mode;
   return true;
}

/* The first card with a display for the monitor index, at the size a
 * fullscreen video asks for, else the mode it runs, else its first */
static bool init_drm(const video_info_t *video)
{
   size_t i;
   drmModeModeInfo custom;
   settings_t *settings     = config_get_ptr();
   unsigned monitor         = settings ? settings->uints.video_monitor_index : 0;
   const drmModeModeInfo *m = NULL;
   struct string_list *cards = dir_list_new("/dev/dri", NULL, false, true,
         false, false);

   for (i = 0; cards && i < cards->size; i++)
      if (     strstr(path_basename_nocompression(cards->elems[i].data), "card")
            && init_drm_card(cards->elems[i].data, monitor))
         break;
   if (!cards || i == cards->size)
   {
      dir_list_free(cards);
      RARCH_ERR("[DRM] No card drives a display with a plane for frames.\n");
      return false;
   }
   RARCH_LOG("[DRM] Using %s%s.\n", cards->elems[i].data,
         drm.atomic ? "" : ", without atomic commits");
   dir_list_free(cards);

   /* The CRT switch's mode, then HDMI timings, then a fullscreen size,
    * then the console's */
   if (     drm_mode_from_crt_switch(&custom)
         || (settings && drm_mode_from_timings(
               settings->arrays.crt_switch_timings, &custom)))
      m = &custom;
   if (     !m && video && video->fullscreen
         && VIDEO_SCALE_W(video->dims) && VIDEO_SCALE_H(video->dims))
      m = drm_find_mode(g_drm_connector, VIDEO_SCALE_W(video->dims),
            VIDEO_SCALE_H(video->dims), 1.0f,
            settings ? settings->floats.video_refresh_rate : 60.0f);
   if (!m && g_orig_crtc->mode_valid)
      m = &g_orig_crtc->mode;
   if (!m && g_drm_connector->count_modes > 0)
      m = &g_drm_connector->modes[0];
   if (!m || !drm_set_mode(m))
   {
      RARCH_ERR("[DRM] Failed to set mode.\n");
      deinit_drm();
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
      drm_set_plane(ov->plane_id, 0, 0, 0, 0, 0, 0, 0,
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
   if (drm.pending_ov)
      drm_flip_wait();
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

static void drm_overlay_tex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   struct drm_video *v = (struct drm_video*)data;
   float *t;
   if (!v || index >= v->ov.count)
      return;
   t = v->ov.img[index].tex;
   if (t[0] != x || t[1] != y || t[2] != w || t[3] != h)
   {
      t[0] = x; t[1] = y; t[2] = w; t[3] = h;
      v->ov.geom_dirty = true;
   }
}

static void drm_overlay_vertex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   struct drm_video *v = (struct drm_video*)data;
   float *t;
   if (!v || index >= v->ov.count)
      return;
   t = v->ov.img[index].vert;
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

static void drm_overlay_set_alpha(void *data, unsigned index, float mod)
{
   struct drm_video *v = (struct drm_video*)data;
   if (v && index < v->ov.count && v->ov.img[index].alpha != mod)
   {
      v->ov.img[index].alpha  = mod;
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
         struct modeset_buf *pg;
         /* The back page may be on its way to the screen */
         if (drm.pending_ov)
            drm_flip_wait();
         pg = &ov->page[ov->back];
         for (y = 0; y < ov->lh; y++)
            memcpy(pg->map + (size_t)y * pg->stride,
                  ov->layer + (size_t)y * ov->lw, (size_t)ov->lw * 4);
         ov->pending = true;
      }
   }

   /* Placed once; after that its pages change with the frame's flips,
    * or on their own without atomic commits */
   if (     ov->mode == DRM_OVERLAY_PLANE && ov->pending
         && (!ov->shown || !drm.atomic)
         && !drm_set_plane(ov->plane_id, drm.crtc_id,
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
   if (     drm.atomic && ov->mode == DRM_OVERLAY_PLANE && ov->shown
         && ov->pending
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
   if (drm.atomic && ov->mode == DRM_OVERLAY_PLANE && ov->shown && ov->pending)
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

   if (!v || (s != v->main_surface && s != v->views_surface))
      return;
   ov = &v->ov;
   if (     ov->mode != DRM_OVERLAY_BLEND || !ov->enabled || !ov->layer
         || !ov->lw || !ov->lh)
      return;
   /* The plane would turn the layer with the frame: the next frame is
    * turned before it is blended instead */
   if (s->hw_rot)
   {
      v->rotation_dirty = true;
      return;
   }
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
   _drmvars->font_enable    = video->font_enable;
   if (video->path_font)
      strlcpy(_drmvars->font_path, video->path_font,
            sizeof(_drmvars->font_path));

   _drmvars->core_dims        = 0;

   _drmvars->main_surface     = NULL;
   _drmvars->menu_surface     = NULL;


   /* DRM Init */
   if (!init_drm(video))
   {
      RARCH_ERR("[DRM] Failed to initialize DRM.\n");
      free(_drmvars);
      return NULL;
   }
   /* Refresh rate, resolutions and switching through the KMS display server */
   video_driver_display_type_set(RARCH_DISPLAY_KMS);

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
      if (!v->menu_active && !drm_plane_setup(v->views_surface))
      {
         drm_surface_free(v, &v->views_surface);
         v->views_surface = old;
         return false;
      }
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
   /* Each view turned in its own place, as the layout laid them out:
    * the views surface itself is never turned */
   v->views_cpu.rot = v->rotation & 3;
   video_views_cpu_compose(&v->views_cpu, &info->views, layout, frame,
         pitch, v->rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565,
         info->video_smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT,
         scale, cdims, (uint32_t*)page->buf.map, page->buf.stride / 4,
         layout->offscreen ? v->views_canvas : NULL);
#ifdef HAVE_OVERLAY
   /* Laid out over the views as over a frame, the plane showing them */
   drm_overlay_prepare(v, v->views_surface);
   drm_overlay_blend(v, v->views_surface);
#endif
   if (v->msg)
      drm_draw_msg(v, &page->buf, VIDEO_SCALE_W(cdims), VIDEO_SCALE_H(cdims),
            4, 0, v->msg, v->msg_info);
   /* Under the menu the page is not shown: the next is written over it,
    * and it goes up when the menu closes */
   if (!v->menu_active)
      drm_page_flip(v->views_surface);
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
   if (frame && (_drmvars->core_dims != dims || _drmvars->rotation_dirty))
   {
      /* The plane reads from the old surface until it is moved to the
       * new one, so the old one goes only after that. */
      struct drm_surface *old_surface = NULL;
      uint32_t fmt = _drmvars->rgb32 ? DRM_FORMAT_XRGB8888
         : DRM_FORMAT_RGB565;
      unsigned rot = _drmvars->rotation & 3;
      unsigned hw  = 0;
      unsigned sdims = dims;

      /* Sanity check. */
      if (width == 0 || height == 0)
         return true;

      _drmvars->core_dims      = dims;
      _drmvars->core_pitch     = pitch;
      _drmvars->rotation_dirty = false;

      /* The plane turns the frame when it can; the copy does otherwise,
       * and where the overlay is blended into the frame it is turned */
      if (     rot
            && (drm_plane_turns(drm_plane_takes(fmt)) & (1u << rot))
#ifdef HAVE_OVERLAY
            && !(_drmvars->ov.enabled && _drmvars->ov.count
               && _drmvars->ov.mode == DRM_OVERLAY_BLEND)
#endif
         )
      {
         hw  = rot;
         rot = 0;
      }
      if (rot & 1)
         sdims = VIDEO_SCALE_PACK(height, width);

      old_surface            = _drmvars->main_surface;
      _drmvars->main_surface = NULL;

      /* We need to recreate the main surface and it's pages (buffers). */
      if (!drm_surface_setup(_drmvars,
            sdims,
            pitch,
            _drmvars->rgb32 ? 4 : 2,
            fmt,
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
      _drmvars->main_surface->rot    = rot;
      _drmvars->main_surface->hw_rot = hw;
      if (rot || hw)
         RARCH_LOG("[DRM] Frame turned %u degrees by the %s.\n",
               90 * (rot | hw), hw ? "plane" : "CPU");

      /* We need to change the plane to read from the main surface; under
       * the menu, when it closes */
      if (     !_drmvars->views_surface && !_drmvars->menu_active
            && !drm_plane_setup(_drmvars->main_surface))
      {
         drm_surface_free(_drmvars, &_drmvars->main_surface);
         _drmvars->main_surface = old_surface;
         /* Not the old surface's size: it may be turned, and the next
          * frame tries again */
         _drmvars->core_dims    = 0;
         return false;
      }
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

   _drmvars->msg      = (msg && *msg && !_drmvars->menu_active) ? msg : NULL;
   _drmvars->msg_info = video_info;

   if (drm_views_frame(_drmvars, frame, dims, pitch, video_info))
   {
      _drmvars->msg = NULL;
      return true;
   }

#ifdef HAVE_OVERLAY
   drm_overlay_prepare(_drmvars, _drmvars->main_surface);
#endif

   /* A frame in the threaded wrapper's frame memory goes up where it
    * lies, unless the copy turns it or blends the overlay into it, or it
    * waits under the menu or for the plane to be placed */
   if (     !_drmvars->main_surface->rot && !_drmvars->menu_active
         && !_drmvars->plane_dirty && !_drmvars->msg
#ifdef HAVE_OVERLAY
         && !(_drmvars->ov.enabled && _drmvars->ov.count
            && _drmvars->ov.mode == DRM_OVERLAY_BLEND)
#endif
      )
   {
      struct drm_page *page = drm_fmem_page(_drmvars, frame, dims, pitch,
            _drmvars->main_surface->pixformat, _drmvars->main_surface->bpp);
      if (page)
      {
         drm_page_flip_to(_drmvars->main_surface, page);
         drm_fmem_sweep(_drmvars);
         return true;
      }
   }

   /* Update main surface: locate free page, blit and flip. Under the
    * menu it is not shown: the next is written over it, and it goes up
    * when the menu closes. */
   drm_surface_update(_drmvars, frame, pitch, _drmvars->main_surface);
   if (!_drmvars->menu_active)
   {
      if (_drmvars->plane_dirty)
      {
         _drmvars->plane_dirty = false;
         drm_plane_setup(_drmvars->main_surface);
      }
      else
         drm_page_flip(_drmvars->main_surface);
   }
   _drmvars->msg = NULL;
   drm_fmem_sweep(_drmvars);
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
      struct drm_surface *s = _drmvars->views_surface
         ? _drmvars->views_surface : _drmvars->main_surface;
      /* With nothing to show, the plane goes off before the menu's
       * buffers do */
      if (!s || !drm_plane_setup(s))
         drm_set_plane(drm.plane_id, 0, 0, 0, 0, 0, 0, 0,
               0, 0, 0, 0);
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
   if (!drm_plane_setup(v->menu_surface))
   {
      drm_surface_free(v, &v->menu_surface);
      v->menu_surface = old;
      return NULL;
   }
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

   /* Nor while the copy turns the frame */
   if (     !v || !fb || !(s = v->main_surface) || v->views_surface || s->rot
         || v->rotation_dirty
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

/* The connector's mode closest to @dims at the refresh rate set: the
 * planes come off for the switch and go back on at the new size */
static void drm_set_video_mode(void *data, unsigned dims, bool fullscreen)
{
   struct drm_video *v        = (struct drm_video*)data;
   settings_t *settings       = config_get_ptr();
   const drmModeModeInfo *m;
   drmModeModeInfo custom;
   struct drm_surface *s;

   if (!v || !g_drm_connector || drm.fd < 0)
      return;
   /* A CRT modeline as it was asked for, else the connector's closest */
   if (drm_mode_from_crt_switch(&custom))
      m = &custom;
   else if (!(m = drm_find_mode(g_drm_connector, VIDEO_SCALE_W(dims),
               VIDEO_SCALE_H(dims), 1.0f,
               settings ? settings->floats.video_refresh_rate : 60.0f)))
      return;
   if (     m->hdisplay == drm.mode.hdisplay && m->vdisplay == drm.mode.vdisplay
         && m->clock == drm.mode.clock && m->vrefresh == drm.mode.vrefresh
         && m->htotal == drm.mode.htotal && m->vtotal == drm.mode.vtotal
         && m->hsync_start == drm.mode.hsync_start
         && m->vsync_start == drm.mode.vsync_start
         && m->flags == drm.mode.flags)
      return;

   drm_set_plane(drm.plane_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
#ifdef HAVE_OVERLAY
   drm_overlay_hide(&v->ov);
   v->ov.geom_dirty = true;
#endif
   if (!drm_set_mode(m))
      RARCH_ERR("[DRM] Failed to set %ux%u.\n", m->hdisplay, m->vdisplay);
   v->kms_dims = VIDEO_SCALE_PACK(drm.mode.hdisplay, drm.mode.vdisplay);
   video_driver_set_output_dims(v->kms_dims);
   s = (v->menu_active && v->menu_surface) ? v->menu_surface
      : (v->views_surface ? v->views_surface : v->main_surface);
   if (s)
      drm_plane_setup(s);
}

/* Atomic commits only: a legacy plane update always waits */
static void drm_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   (void)data; (void)adaptive_vsync_enabled; (void)swap_interval;
   if (drm.nonblock == (toggle && drm.atomic))
      return;
   drm.nonblock = toggle && drm.atomic;
   RARCH_LOG("[DRM] Vsync %s.\n", drm.nonblock ? "off" : "on");
}
static bool drm_alive(void *data) { return true; }
static bool drm_focus(void *data) { return true; }

static void drm_viewport_info(void *data, struct video_viewport *vp)
{
   struct drm_video *vid = data;

   if (!vid)
      return;

   vp->pos = VIDEO_POS_PACK(0, 0);

   /* The whole screen while views draw, else the frame as it is shown */
   if (vid->views_surface)
      vp->dims = vid->kms_dims;
   else if (vid->main_surface)
      vp->dims = (vid->main_surface->hw_rot & 1)
         ? VIDEO_SCALE_PACK(VIDEO_SCALE_H(vid->main_surface->src_dims),
               VIDEO_SCALE_W(vid->main_surface->src_dims))
         : vid->main_surface->src_dims;
   else
      vp->dims = (vid->rotation & 1)
         ? VIDEO_SCALE_PACK(VIDEO_SCALE_H(vid->core_dims),
               VIDEO_SCALE_W(vid->core_dims))
         : vid->core_dims;
   vp->full_dims = vp->dims;
}

/* Taken up when the next frame comes, where the surfaces are made */
static void drm_set_rotation(void *data, unsigned rotation)
{
   struct drm_video *v = (struct drm_video*)data;
   if (v && (rotation & 3) != v->rotation)
   {
      v->rotation       = rotation & 3;
      v->rotation_dirty = true;
   }
}

/* The frame on screen, as viewport_info gives its size: BGR24, bottom
 * row first. Not the views, which the plane enlarges past it. */
static bool drm_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   unsigned ox, oy, w, h, ow, oh;
   const struct modeset_buf *buf;
   struct drm_video *v = (struct drm_video*)data;
   struct drm_surface *s;

   if (     !v || !buffer || v->views_surface || !(s = v->main_surface)
         || !s->current_page)
      return false;
   buf = &s->current_page->buf;
   w   = VIDEO_SCALE_W(s->src_dims);
   h   = VIDEO_SCALE_H(s->src_dims);
   ow  = (s->hw_rot & 1) ? h : w;
   oh  = (s->hw_rot & 1) ? w : h;
   for (oy = 0; oy < oh; oy++)
   {
      ptrdiff_t step;
      uint8_t *out      = buffer + (size_t)(oh - 1 - oy) * ow * 3;
      const uint8_t *in = video_turned_row(buf->map, buf->stride, w, h,
            s->bpp, s->hw_rot, oy, &step);
      for (ox = 0; ox < ow; ox++, in += step, out += 3)
      {
         if (s->bpp == 4)
         {
            uint32_t p = *(const uint32_t*)in;
            out[0]     = (uint8_t)p;
            out[1]     = (uint8_t)(p >> 8);
            out[2]     = (uint8_t)(p >> 16);
         }
         else
         {
            uint32_t p = *(const uint16_t*)in;
            uint32_t r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, b = p & 0x1f;
            out[0]     = (uint8_t)((b << 3) | (b >> 2));
            out[1]     = (uint8_t)((g << 2) | (g >> 4));
            out[2]     = (uint8_t)((r << 3) | (r >> 2));
         }
      }
   }
   return true;
}

static bool drm_suppress_screensaver(void *a, bool b) { return false; }
static bool drm_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

/* The menu is placed again now, the frame with the next one */
static void drm_take_aspect(struct drm_video *v, float aspect)
{
   if (!v || v->current_aspect == aspect)
      return;
   v->current_aspect = aspect;
   drm_surface_set_aspect(v->main_surface, aspect);
   drm_surface_set_aspect(v->menu_surface, aspect);
   if (v->menu_active && v->menu_surface)
      drm_plane_setup(v->menu_surface);
   else if (v->main_surface)
      v->plane_dirty = true;
}

static void drm_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   drm_take_aspect((struct drm_video*)data,
         aspectratio_lut[aspect_ratio_idx].value);
}

/* The CRT switch puts its aspect and asks for it to be taken up */
static void drm_apply_state_changes(void *data)
{
   drm_take_aspect((struct drm_video*)data, video_driver_get_aspect_ratio());
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
   drm_set_video_mode,
   NULL, /* refresh_rate - handled by display server */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   drm_set_aspect_ratio,
   drm_apply_state_changes,
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
   drm_frame_mem_alloc,
   drm_frame_mem_ready,
   drm_frame_mem_free,
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
   /* Off before the buffers it scans out go */
   if (drm.fd >= 0 && drm.plane_id)
      drm_set_plane(drm.plane_id, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0);
   if (_drmvars->main_surface && _drmvars->main_surface->lent)
      video_driver_cached_frame_retire();
   drm_surface_free(_drmvars, &_drmvars->main_surface);
   {
      unsigned i;
      for (i = 0; i < DRM_FRAME_MEM; i++)
         if (_drmvars->fmem[i].base)
            drm_fmem_destroy(&_drmvars->fmem[i]);
   }

   if (_drmvars->menu_surface)
      drm_surface_free(_drmvars, &_drmvars->menu_surface);
   drm_surface_free(_drmvars, &_drmvars->views_surface);
   video_views_cpu_free(&_drmvars->views_cpu);
   free(_drmvars->views_canvas);
   if (_drmvars->font)
      _drmvars->font_driver->free(_drmvars->font);

   deinit_drm();

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
   drm_set_rotation,
   drm_viewport_info,
   drm_read_viewport,
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
