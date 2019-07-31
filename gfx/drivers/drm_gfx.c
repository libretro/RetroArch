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
#include <errno.h>

#include <compat/strl.h>
#include <rthreads/rthreads.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../common/drm_common.h"

#include "drm_pixformats.h"

struct modeset_buf
{
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t size;
	uint32_t handle;
	uint8_t *map;
	uint32_t fb_id;
	uint32_t pixel_format;
};

struct drm_rect
{
	int x;
	int y;
	int width;
	int height;
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

   /* The internal buffers size. */
   int src_width;
   int src_height;

   /* Surfaces with a higher layer will be on top of
    * the ones with lower. Default is 0. */
   int layer;

   /* We need to keep this value for the blitting on
    * the surface_update function. */
   int pitch;
   int total_pitch;

   float aspect;
   bool flip_page;
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

   drmModeEncoder *encoder;
   drmModeRes *resources;
} drm;

struct drm_video
{
   /* Abstract surface management stuff */
   struct drm_surface *main_surface;
   struct drm_surface *menu_surface;

   /* Total dispmanx video dimensions.
    * Not counting overscan settings. */
   unsigned int kms_width;
   unsigned int kms_height;

   /* For threading */
   scond_t *vsync_condition;
   slock_t *vsync_cond_mutex;
   slock_t *pending_mutex;

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

static void drm_surface_free(void *data, struct drm_surface **sp)
{
   int i;
   struct drm_video *_drmvars = data;
   struct drm_surface *surface = *sp;

   for (i = 0; i < surface->numpages; i++)
      surface->pages[i].used = false;

   free(surface->pages);

   free(surface);
   *sp = NULL;
}

/* Changes surface ratio only without recreating the buffers etc. */
static void drm_surface_set_aspect(struct drm_surface *surface, float aspect)
{
	surface->aspect = aspect;
}

static void drm_surface_setup(void *data,  int src_width, int src_height,
      int pitch, int bpp, uint32_t pixformat,
      int alpha, float aspect, int numpages, int layer,
      struct drm_surface **sp)
{
   struct drm_video *_drmvars = data;
   int i;
   struct drm_surface *surface = NULL;

   *sp = calloc (1, sizeof(struct drm_surface));

   surface = *sp;

   /* Setup surface parameters */
   surface->numpages = numpages;
   /* We receive the total pitch, including things that are
    * between scanlines and we calculate the visible pitch
    * from the visible width.
    *
    * These will be used to increase the offsets for blitting. */
   surface->total_pitch = pitch;
   surface->pitch       = src_width * bpp;
   surface->bpp         = bpp;
   surface->pixformat   = pixformat;
   surface->src_width   = src_width;
   surface->src_height  = src_height;
   surface->aspect      = aspect;

   /* Allocate memory for all the pages in each surface
    * and initialize variables inside each page's struct. */
   surface->pages = (struct drm_page*)
      calloc(surface->numpages, sizeof(struct drm_page));

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
      surface->pages[i].buf.width  = src_width;
      surface->pages[i].buf.height = src_height;
      int ret                      = modeset_create_dumbfb(
            drm.fd, &surface->pages[i].buf, bpp, pixformat);

      if (ret)
      {
         RARCH_ERR ("DRM: can't create fb\n");
      }
   }

   surface->flip_page = 0;
}

static void drm_page_flip(struct drm_surface *surface)
{
   /* We alredy have the id of the FB_ID property of
    * the plane on which we are going to do a pageflip:
    * we got it back in drm_plane_setup()  */
   int ret;
   static drmModeAtomicReqPtr req = NULL;

   req = drmModeAtomicAlloc();

   /* We add the buffer to the plane properties we want to
    * set on an atomically, in a single step.
    * We pass the plane id, the property id and the new fb id. */
   ret = drmModeAtomicAddProperty(req,
         drm.plane_id,
         drm.plane_fb_prop_id,
         surface->pages[surface->flip_page].buf.fb_id);

   if (ret < 0)
   {
      RARCH_ERR ("DRM: failed to add atomic property for pageflip\n");
   }
   /*... now we just need to do the commit */

   /* REMEMBER!!! The DRM_MODE_PAGE_FLIP_EVENT flag asks the kernel
    * to send you an event to the drm.fd once the
    * pageflip is complete. If you don't want -12 errors
    * (ENOMEM), namely "Cannot allocate memory", then
    * you must drain the event queue of that fd. */
   ret = drmModeAtomicCommit(drm.fd, req, 0, NULL);

   if (ret < 0)
   {
      RARCH_ERR ("DRM: failed to commit for pageflip: %s\n", strerror(errno));
   }

   surface->flip_page = !(surface->flip_page);

   drmModeAtomicFree(req);
}

static void drm_surface_update(void *data, const void *frame,
      struct drm_surface *surface)
{
   struct drm_video *_drmvars  = data;
   struct drm_page       *page = NULL;
   /* Frame blitting */
   int line                    = 0;
   int src_offset              = 0;
   int dst_offset              = 0;

   for (line = 0; line < surface->src_height; line++)
   {
      memcpy (
            surface->pages[surface->flip_page].buf.map + dst_offset,
            (uint8_t*)frame + src_offset,
            surface->pitch);
      src_offset += surface->total_pitch;
      dst_offset += surface->pitch;
   }

   /* Page flipping */
   drm_page_flip(surface);
}

static uint32_t get_plane_prop_id(uint32_t obj_id, const char *name)
{
   int i,j;
   drmModePlaneRes *plane_resources;
   drmModePlane *plane;
   drmModeObjectProperties *props;
   drmModePropertyRes **props_info;

   char format_str[5];

   plane_resources = drmModeGetPlaneResources(drm.fd);
   for (i = 0; i < plane_resources->count_planes; i++)
   {
      plane = drmModeGetPlane(drm.fd, plane_resources->planes[i]);
      if (plane->plane_id != obj_id)
         continue;

      /* TODO: Improvement. We get all the properties of the
       * plane and info about the properties.
       * We should have done this already...
       * This implementation must be improved. */
      props      = drmModeObjectGetProperties(drm.fd,
            plane->plane_id, DRM_MODE_OBJECT_PLANE);
      props_info = malloc(props->count_props * sizeof *props_info);

      for (j = 0; j < props->count_props; ++j)
         props_info[j] =	drmModeGetProperty(drm.fd, props->props[j]);

      /* We look for the prop_id we need */
      for (j = 0; j < props->count_props; j++)
      {
         if (string_is_equal(props_info[j]->name, name))
            return props_info[j]->prop_id;
      }
      RARCH_ERR ("DRM: plane %d fb property ID with name %s not found\n",
            plane->plane_id, name);
   }
   return (0);
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
   int i,j;

   /* The property values and their names are stored in different arrays,
    * so we access them simultaneously here.
    * We are interested in OVERLAY planes only, that's
    * type 0 or DRM_PLANE_TYPE_OVERLAY
    * (see /usr/xf86drmMode.h for definition). */
   drmModeObjectPropertiesPtr props =
      drmModeObjectGetProperties(drm.fd, plane->plane_id,
            DRM_MODE_OBJECT_PLANE);

   for (j = 0; j < props->count_props; j++)
   {
      /* found the type property */
      if (string_is_equal(
               drmModeGetProperty(drm.fd, props->props[j])->name, "type"))
         return (props->prop_values[j]);
   }

   return (0);
}

/* This configures our only overlay plane to render the given surface. */
static void drm_plane_setup(struct drm_surface *surface)
{
   int i,j;
   char fmt_name[5];

   /* Get plane resources */
   drmModePlane *plane;
   drmModePlaneRes *plane_resources;
   plane_resources = drmModeGetPlaneResources(drm.fd);
   if (!plane_resources)
   {
      RARCH_ERR ("DRM: No scaling planes available!\n");
   }

   RARCH_LOG ("DRM: Number of planes on FD %d is %d\n",
         drm.fd, plane_resources->count_planes);

   /* dump_planes(drm.fd); */

   /* Look for a plane/overlay we can use with the configured CRTC
    * Find a  plane which can be connected to our CRTC. Find the
    * CRTC index first, then iterate over available planes.
    * Yes, strangely we need the in-use CRTC index to mask possible_crtc
    * during the planes iteration... */
   unsigned int crtc_index = 0;
   for (i = 0; i < (unsigned int)drm.resources->count_crtcs; i++)
   {
      if (drm.crtc_id == drm.resources->crtcs[i])
      {
         crtc_index = i;
         RARCH_LOG ("DRM: CRTC index found %d with ID %d\n", crtc_index, drm.crtc_id);
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
      plane = drmModeGetPlane(drm.fd, plane_resources->planes[i]);

      if (!(plane->possible_crtcs & (1 << crtc_index))){
         RARCH_LOG ("DRM: plane with ID %d can't be used with current CRTC\n",
               plane->plane_id);
         continue;
      }

      /* We are only interested in overlay planes. No overlay, no fun.
       * (no scaling, must cover crtc..etc) so we skip primary planes */
      if (drm_plane_type(plane) != DRM_PLANE_TYPE_OVERLAY)
      {
         RARCH_LOG ("DRM: plane with ID %d is not an overlay. May be primary or cursor. Not usable.\n",
               plane->plane_id);
         continue;
      }

      if (!format_support(plane, surface->pixformat))
      {
         RARCH_LOG ("DRM: plane with ID %d does not support framebuffer format\n", plane->plane_id);
         continue;
      }

      drm.plane_id = plane->plane_id;
      drmModeFreePlane(plane);
   }

   if (!drm.plane_id)
   {
      RARCH_LOG ("DRM: couldn't find an usable overlay plane for current CRTC and framebuffer pixel formal.\n");
      deinit_drm();
      exit (0);
   }
   else
   {
      RARCH_LOG ("DRM: using plane/overlay ID %d\n", drm.plane_id);
   }

   /* We are going to be changing the framebuffer ID property of the chosen overlay every time
    * we do a pageflip, so we get the property ID here to have it handy on the PageFlip function. */
   drm.plane_fb_prop_id = get_plane_prop_id(drm.plane_id, "FB_ID");
   if (!drm.plane_fb_prop_id)
   {
      RARCH_LOG("[DRM]: Can't get the FB property ID for plane(%u)\n", drm.plane_id);
   }

   /* Note src coords (last 4 args) are in Q16 format
    * crtc_w and crtc_h are the final size with applied scale/ratio.
    * crtc_x and crtc_y are the position of the plane
    * pw and ph are the input size: the size of the area we read from the fb. */
   uint32_t plane_flags = 0;
   uint32_t plane_w = drm.current_mode->vdisplay * surface->aspect;
   uint32_t plane_h = drm.current_mode->vdisplay;
   /* If we obtain a scaled image width that is bigger than the physical screen width,
    * then we keep the physical screen width as our maximun width. */
   if (plane_w > drm.current_mode->hdisplay)
      plane_w = drm.current_mode->hdisplay;

   uint32_t plane_x = (drm.current_mode->hdisplay - plane_w) / 2;
   uint32_t plane_y = (drm.current_mode->vdisplay - plane_h) / 2;

   uint32_t src_w = surface->src_width;
   uint32_t src_h = surface->src_height;
   uint32_t src_x = 0;
   uint32_t src_y = 0;

   /* We have to set a buffer for the plane, whatever buffer we want,
    * but we must set a buffer so the plane starts reading from it now. */
   if (drmModeSetPlane(drm.fd, drm.plane_id, drm.crtc_id,
            surface->pages[surface->flip_page].buf.fb_id,
            plane_flags, plane_x, plane_y, plane_w, plane_h,
            src_x<<16, src_y<<16, src_w<<16, src_h<<16))
   {
      RARCH_ERR("[DRM]: failed to enable plane: %s\n", strerror(errno));
   }

   RARCH_LOG("[DRM]: src_w %d, src_h %d, plane_w %d, plane_h %d\n",
         src_w, src_h, plane_w, plane_h);

   /* Report what plane (of overlay type) we're using. */
   drm_format_name(surface->pixformat, fmt_name);
   RARCH_LOG("[DRM]: Using plane with ID %d on CRTC ID %d format %s\n",
         drm.plane_id, drm.crtc_id, fmt_name);
}

static int modeset_create_dumbfb(int fd, struct modeset_buf *buf,
      int bpp, uint32_t pixformat)
{
   struct drm_mode_create_dumb create_dumb = {0};
   struct drm_mode_map_dumb map_dumb       = {0};
   struct drm_mode_fb_cmd cmd_dumb         = {0};

   create_dumb.width  = buf->width;
   create_dumb.height = buf->height;
   create_dumb.bpp    = bpp * 8;
   create_dumb.flags  = 0;
   create_dumb.pitch  = 0;
   create_dumb.size   = 0;
   create_dumb.handle = 0;
   drmIoctl(drm.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);

   /* Create the buffer. We just copy values here... */
   cmd_dumb.width        = create_dumb.width;
   cmd_dumb.height       = create_dumb.height;
   cmd_dumb.bpp          = create_dumb.bpp;
   cmd_dumb.pitch        = create_dumb.pitch;
   cmd_dumb.handle       = create_dumb.handle;
   cmd_dumb.depth        = 24;

   /* Map the buffer */
   drmIoctl(drm.fd,DRM_IOCTL_MODE_ADDFB,&cmd_dumb);
   map_dumb.handle=create_dumb.handle;
   drmIoctl(drm.fd,DRM_IOCTL_MODE_MAP_DUMB,&map_dumb);

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
      RARCH_ERR ("DRM: cannot mmap dumb buffer\n");
      return 0;
   }

   return 0;
}

static bool init_drm(void)
{
   int ret;
   drmModeConnector *connector;
   uint i;

   drm.fd = open("/dev/dri/card0", O_RDWR);

   if (drm.fd < 0)
   {
      RARCH_LOG ("DRM: could not open drm device\n");
      return false;
   }

   /* Programmer!! Save your sanity!!
    * VERY important or we won't get all the available planes on drmGetPlaneResources()!
    * We also need to enable the ATOMIC cap to see the atomic properties in objects!! */
   ret = drmSetClientCap(drm.fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
   if (ret)
      RARCH_ERR ("DRM: can't set UNIVERSAL PLANES cap.\n");
   else
      RARCH_LOG ("DRM: UNIVERSAL PLANES cap set\n");

   ret = drmSetClientCap(drm.fd, DRM_CLIENT_CAP_ATOMIC, 1);
   if (ret)
   {
      /*If this happens, check kernel support and kernel parameters
       * (add i915.nuclear_pageflip=y to the kernel boot line for example) */
      RARCH_ERR ("DRM: can't set ATOMIC caps: %s\n", strerror(errno));
   }
   else
      RARCH_LOG ("DRM: ATOMIC caps set\n");

   drm.resources = drmModeGetResources(drm.fd);
   if (!drm.resources)
   {
      RARCH_ERR ("DRM: drmModeGetResources failed\n");
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
      RARCH_ERR ("DRM: no connected connector found\n");
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
      RARCH_ERR ("DRM: no encoder found.\n");
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
   struct modeset_buf buf;
   buf.width = drm.current_mode->hdisplay;
   buf.height = drm.current_mode->vdisplay;
   ret = modeset_create_dumbfb(drm.fd, &buf, 4, DRM_FORMAT_XRGB8888);
   if (ret)
   {
      RARCH_ERR ("DRM: can't create dumb fb\n");
   }

   if (drmModeSetCrtc(drm.fd, drm.crtc_id, buf.fb_id, 0, 0,
            &drm.connector_id, 1, drm.current_mode))
   {
      RARCH_ERR ("DRM: failed to set mode\n");
      return false;
   }

   return true;
}

static void *drm_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
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
   _drmvars->core_width       = 0;
   _drmvars->core_height      = 0;

   _drmvars->main_surface     = NULL;
   _drmvars->menu_surface     = NULL;

   if (input && input_data)
      *input = NULL;

   /* DRM Init */
   if (!init_drm())
   {
      RARCH_ERR ("DRM: Failed to initialize DRM\n");
      free(_drmvars);
      return NULL;
   }
   else
   {
      RARCH_LOG ("DRM: Init successful.\n");
   }

   _drmvars->kms_width  = drm.current_mode->hdisplay;
   _drmvars->kms_height = drm.current_mode->vdisplay;

   return _drmvars;
}

static bool drm_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   struct drm_video *_drmvars = data;

   if (  ( width != _drmvars->core_width) ||
         (height != _drmvars->core_height))
   {
      /* Sanity check. */
      if (width == 0 || height == 0)
         return true;

      _drmvars->core_width  = width;
      _drmvars->core_height = height;
      _drmvars->core_pitch  = pitch;

      if (_drmvars->main_surface != NULL)
         drm_surface_free(_drmvars, &_drmvars->main_surface);

      /* We need to recreate the main surface and it's pages (buffers). */
      drm_surface_setup(_drmvars,
            width,
            height,
            pitch,
            _drmvars->rgb32 ? 4 : 2,
            _drmvars->rgb32 ? DRM_FORMAT_XRGB8888 : DRM_FORMAT_RGB565,
	    255,
            _drmvars->current_aspect,
            3,
            0,
            &_drmvars->main_surface);

      /* We need to change the plane to read from the main surface */
      drm_plane_setup(_drmvars->main_surface);
   }

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   video_info->cb_update_window_title(
         video_info->context_data, video_info);

   /* Update main surface: locate free page, blit and flip. */
   drm_surface_update(_drmvars, frame, _drmvars->main_surface);
   return true;
}

static void drm_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct drm_video *_drmvars = data;

   /* If menu was active but it's not anymore... */
   if (!state && _drmvars->menu_active)
   {
      /* We tell ony the plane we have to read from the main surface again */
      drm_plane_setup(_drmvars->main_surface);
      /* We free the menu surface buffers */
      drm_surface_free(_drmvars, &_drmvars->menu_surface);
   }

   _drmvars->menu_active = state;
}

static void drm_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   unsigned int i, j;
   struct drm_video *_drmvars = data;

   if (!_drmvars->menu_active)
      return;

   /* If menu is active in this frame but the
    * menu surface is NULL, we allocate a new one.*/
   if (!_drmvars->menu_surface)
   {
      drm_surface_setup(_drmvars,
            width,
            height,
            width * 4,
            4,
            DRM_FORMAT_XRGB8888,
            210,
            _drmvars->current_aspect,
            2,
            0,
            &_drmvars->menu_surface);

      /* We need to re-setup the ONLY plane as the setup
       * depends on input buffers dimensions. */
      drm_plane_setup(_drmvars->menu_surface);
   }

   /* We have to go on a pixel format conversion adventure
    * for now, until we can convince RGUI to output
    * in an 8888 format. */
   unsigned int src_pitch        = width * 2;
   unsigned int dst_pitch        = width * 4;
   unsigned int dst_width        = width;
   uint32_t line[dst_width];

   /* The output pixel array with the converted pixels. */
   char *frame_output = (char *) malloc (dst_pitch * height);

   /* Remember, memcpy() works with 8bits pointers for increments. */
   char *dst_base_addr           = frame_output;

   for (i = 0; i < height; i++)
   {
      for (j = 0; j < src_pitch / 2; j++)
      {
         uint16_t src_pix = *((uint16_t*)frame + (src_pitch / 2 * i) + j);
         /* The hex AND is for keeping only the part we need for each component. */
         uint32_t R = (src_pix << 8) & 0x00FF0000;
         uint32_t G = (src_pix << 4) & 0x0000FF00;
         uint32_t B = (src_pix << 0) & 0x000000FF;
         line[j] = (0 | R | G | B);
      }
      memcpy(dst_base_addr + (dst_pitch * i), (char*)line, dst_pitch);
   }

   /* We update the menu surface if menu is active. */
   drm_surface_update(_drmvars, frame_output, _drmvars->menu_surface);
}

static void drm_gfx_set_nonblock_state(void *data, bool state)
{
   struct drm_video *vid = data;

   (void)data;
   (void)vid;

   /* TODO */
}

static bool drm_gfx_alive(void *data)
{
   (void)data;
   return true; /* always alive */
}

static bool drm_gfx_focus(void *data)
{
   (void)data;
   return true; /* fb device always has focus */
}

static void drm_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   struct drm_video *vid = data;

   if (!vid)
      return;

   vp->x = vp->y = 0;

   vp->width  = vp->full_width  = vid->core_width;
   vp->height = vp->full_height = vid->core_height;
}

static bool drm_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static bool drm_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void drm_set_aspect_ratio (void *data, unsigned aspect_ratio_idx)
{
   struct drm_video *_drmvars = data;
   /* Here we obtain the new aspect ratio. */
   float new_aspect = aspectratio_lut[aspect_ratio_idx].value;

   if (_drmvars->current_aspect != new_aspect)
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

static const video_poke_interface_t drm_poke_interface = {
   NULL, /* get_flags */
   NULL,
   NULL,
   NULL, /* set_video_mode */
   drm_get_refresh_rate,
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
   NULL,                         /* drm_set_osd_msg */
   NULL,                         /* drm_show_mouse */
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL                          /* get_hw_render_interface */
};

static void drm_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &drm_poke_interface;
}

static void drm_gfx_free(void *data)
{
   struct drm_video *_drmvars = data;

   if (!_drmvars)
      return;

   drm_surface_free(_drmvars, &_drmvars->main_surface);

   if (_drmvars->menu_surface)
      drm_surface_free(_drmvars, &_drmvars->menu_surface);

   /* Destroy mutexes and conditions. */
   slock_free(_drmvars->pending_mutex);
   slock_free(_drmvars->vsync_cond_mutex);
   scond_free(_drmvars->vsync_condition);

   g_drm_mode = NULL;

   free(_drmvars);
}

video_driver_t video_drm = {
   drm_gfx_init,
   drm_gfx_frame,
   drm_gfx_set_nonblock_state,
   drm_gfx_alive,
   drm_gfx_focus,
   drm_gfx_suppress_screensaver,
   NULL, /* has_windowed */
   drm_gfx_set_shader,
   drm_gfx_free,
   "drm",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   drm_gfx_viewport_info,
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   drm_gfx_get_poke_interface
};
