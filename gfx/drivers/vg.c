/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <math.h>
#include <string.h>

#include <VG/openvg.h>
#include <VG/vgext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <retro_inline.h>
#include <encodings/utf.h>
#include <gfx/math/matrix_3x3.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../retroarch.h"
#include "../../driver.h"
#include "../../content.h"
#include "../../verbosity.h"
#include "../../configuration.h"

/* Child images of the font atlas, one per glyph rectangle drawn,
 * looked up by rectangle: a power of two, and enough for the cells an
 * on-screen message keeps in use. */
#define VG_FONT_CHILDREN 128

typedef struct
{
   VGImage image;
   unsigned short x, y, w, h;
} vg_glyph_image_t;

typedef struct
{
   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;
   const font_renderer_driver_t *font_driver;
   void *font_data;

   VGint scissor[4];
   VGImageFormat mTexType;
   VGImage mImage;
   EGLImageKHR last_egl_image;

   /* The shared glyph cache's atlas, as one A8 image, and the paint
    * the glyphs are multiplied by */
   VGImage font_atlas;
   VGPaint font_paint;
   unsigned font_atlas_w;
   unsigned font_atlas_h;
   vg_glyph_image_t font_glyphs[VG_FONT_CHILDREN];

   unsigned mTextureWidth;
   unsigned mTextureHeight;
   unsigned mRenderWidth;
   unsigned mRenderHeight;
   unsigned x1, y1, x2, y2;
   float mScreenAspect;
   math_matrix_3x3 mTransformMatrix; /* float alignment */

   bool should_resize;
   bool keep_aspect;
   bool mEglImageBuf;
} vg_t;

static PFNVGCREATEEGLIMAGETARGETKHRPROC pvgCreateEGLImageTargetKHR;

static void vg_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   vg_t *vg     = (vg_t*)data;
   int interval = state ? 0 : 1;

   if (vg->ctx_driver && vg->ctx_driver->swap_interval)
   {
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      vg->ctx_driver->swap_interval(vg->ctx_data, interval);
   }
}

static INLINE bool vg_query_extension(const char *ext)
{
   const char *str = (const char*)vgGetString(VG_EXTENSIONS);
   bool ret = str && strstr(str, ext);
   RARCH_LOG("[VG] Querying VG extension: %s => %s.\n",
         ext, ret ? "exists" : "doesn't exist");

   return ret;
}

/* The on-screen message font: a face from the shared glyph cache, in
 * an A8 atlas allowed to grow to the largest image OpenVG makes. */
static bool vg_font_init(vg_t *vg, const char *path, float size)
{
   struct font_atlas *atlas;

   if (!font_renderer_create_default(&vg->font_driver, &vg->font_data,
            path, size, FONT_ATLAS_FORMAT_A8))
      return false;
   if ((atlas = vg->font_driver->get_atlas(vg->font_data)))
   {
      atlas->max_width  = (unsigned)vgGeti(VG_MAX_IMAGE_WIDTH);
      atlas->max_height = (unsigned)vgGeti(VG_MAX_IMAGE_HEIGHT);
   }
   if ((vg->font_paint = vgCreatePaint()) == VG_INVALID_HANDLE)
   {
      vg->font_driver->free(vg->font_data);
      vg->font_data = NULL;
      return false;
   }
   vgSetParameteri(vg->font_paint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
   return true;
}

static void *vg_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned win_dims;
   VGfloat clearColor[4]           = {0, 0, 0, 1};
   int interval                    = 0;
   unsigned mode_dims             = 0;
   unsigned temp_dims             = 0;
   void *ctx_data                  = NULL;
   settings_t        *settings     = config_get_ptr();
   const char *path_font           = settings->paths.path_font;
   float video_font_size           = settings->floats.video_font_size;
   vg_t                    *vg     = (vg_t*)calloc(1, sizeof(vg_t));
   const gfx_ctx_driver_t *ctx     = video_context_driver_init_first(
         vg, settings->arrays.video_context_driver,
         GFX_CTX_OPENVG_API, 0, 0, false, &ctx_data);
   bool adaptive_vsync_enabled     = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;

   if (!vg || !ctx)
      goto error;

   if (ctx_data)
      vg->ctx_data = ctx_data;

   vg->ctx_driver = ctx;
   video_context_driver_set((void*)ctx);

   if (vg->ctx_driver->get_video_size)
      vg->ctx_driver->get_video_size(vg->ctx_data,
               &mode_dims);

   temp_dims  = mode_dims;

   RARCH_LOG("[VG] Detecting screen resolution: %ux%u.\n",
         VIDEO_SCALE_W(temp_dims), VIDEO_SCALE_H(temp_dims));

   if (VIDEO_SCALE_W(temp_dims) != 0 && VIDEO_SCALE_H(temp_dims) != 0)
      video_driver_set_output_dims(temp_dims);

   interval = video->vsync ? 1 : 0;

   if (ctx->swap_interval)
   {
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      ctx->swap_interval(vg->ctx_data, interval);
   }

   vg->mTexType    = video->rgb32 ? VG_sXRGB_8888 : VG_sRGB_565;
   vg->keep_aspect = video->force_aspect;

   win_dims   = video->dims;

   /* Neither axis set is the whole word clear */
   if (video->fullscreen && (win_dims == 0))
      win_dims = video_driver_get_output_dims();

   if (     !vg->ctx_driver->set_video_mode
         || !vg->ctx_driver->set_video_mode(vg->ctx_data, win_dims,
            video->fullscreen))
      goto error;

   temp_dims        = 0;
   mode_dims        = 0;

   if (vg->ctx_driver->get_video_size)
      vg->ctx_driver->get_video_size(vg->ctx_data,
               &mode_dims);

   temp_dims        = mode_dims;

   vg->should_resize = true;

   if (VIDEO_SCALE_W(temp_dims) != 0 && VIDEO_SCALE_H(temp_dims) != 0)
   {
      RARCH_LOG("[VG] Verified window resolution %ux%u.\n",
            VIDEO_SCALE_W(temp_dims), VIDEO_SCALE_H(temp_dims));
      video_driver_set_output_dims(temp_dims);
   }
   else
      temp_dims = video_driver_get_output_dims();

   vg->mScreenAspect = (float)VIDEO_SCALE_W(temp_dims)
      / VIDEO_SCALE_H(temp_dims);

   if (vg->ctx_driver->translate_aspect)
      vg->mScreenAspect = vg->ctx_driver->translate_aspect(
            vg->ctx_data, VIDEO_SCALE_W(temp_dims), VIDEO_SCALE_H(temp_dims));

   vgSetfv(VG_CLEAR_COLOR, 4, clearColor);

   vg->mTextureWidth = vg->mTextureHeight = video->input_scale * RARCH_SCALE_BASE;
   vg->mImage        = vgCreateImage(
         vg->mTexType,
         vg->mTextureWidth,
         vg->mTextureHeight,
         video->smooth
         ? VG_IMAGE_QUALITY_BETTER
         : VG_IMAGE_QUALITY_NONANTIALIASED);
   vg_set_nonblock_state(vg, !video->vsync, adaptive_vsync_enabled, interval);

   if (vg->ctx_driver->input_driver)
   {
      const char *joypad_name = settings->arrays.input_joypad_driver;
      vg->ctx_driver->input_driver(
            vg->ctx_data, joypad_name,
            input, input_data);
   }

   if (video->font_enable)
      vg_font_init(vg, *path_font ? path_font : NULL, video_font_size);

   if (vg_query_extension("KHR_EGL_image")
         && vg->ctx_driver->image_buffer_init
         && vg->ctx_driver->image_buffer_init(vg->ctx_data, (void*)video))
   {
      if (vg->ctx_driver->get_proc_address)
         pvgCreateEGLImageTargetKHR = (PFNVGCREATEEGLIMAGETARGETKHRPROC)vg->ctx_driver->get_proc_address("vgCreateEGLImageTargetKHR");

      if (pvgCreateEGLImageTargetKHR)
      {
         RARCH_LOG("[VG] Using EGLImage buffer.\n");
         vg->mEglImageBuf = true;
      }
   }

   return vg;

error:
   video_context_driver_free();
   if (vg)
      free(vg);
   return NULL;
}

/* Drops the atlas image and every child image cut from it */
static void vg_font_release_images(vg_t *vg)
{
   unsigned i;
   for (i = 0; i < VG_FONT_CHILDREN; i++)
   {
      if (vg->font_glyphs[i].image != VG_INVALID_HANDLE)
         vgDestroyImage(vg->font_glyphs[i].image);
      vg->font_glyphs[i].image = VG_INVALID_HANDLE;
   }
   if (vg->font_atlas != VG_INVALID_HANDLE)
      vgDestroyImage(vg->font_atlas);
   vg->font_atlas   = VG_INVALID_HANDLE;
   vg->font_atlas_w = 0;
   vg->font_atlas_h = 0;
}

/* Brings the atlas image up to the glyph cache: made again when the
 * atlas has grown, otherwise only the region drawn into since. Rows go
 * in as they are, top row first; the draw flips them. */
static bool vg_font_sync_atlas(vg_t *vg, struct font_atlas *atlas)
{
   if (     vg->font_atlas == VG_INVALID_HANDLE
         || atlas->width   != vg->font_atlas_w
         || atlas->height  != vg->font_atlas_h)
   {
      vg_font_release_images(vg);
      if ((vg->font_atlas = vgCreateImage(VG_A_8,
                  (VGint)atlas->width, (VGint)atlas->height,
                    VG_IMAGE_QUALITY_NONANTIALIASED
                  | VG_IMAGE_QUALITY_FASTER
                  | VG_IMAGE_QUALITY_BETTER)) == VG_INVALID_HANDLE)
         return false;
      vg->font_atlas_w = atlas->width;
      vg->font_atlas_h = atlas->height;
      vgImageSubData(vg->font_atlas, atlas->buffer, (VGint)atlas->width,
            VG_A_8, 0, 0, (VGint)atlas->width, (VGint)atlas->height);
   }
   else if (atlas->dirty)
   {
      unsigned x0 = atlas->dirty_x0;
      unsigned y0 = atlas->dirty_y0;
      unsigned x1 = (atlas->dirty_x1 < atlas->width)
         ? atlas->dirty_x1 : atlas->width;
      unsigned y1 = (atlas->dirty_y1 < atlas->height)
         ? atlas->dirty_y1 : atlas->height;
      if (x1 > x0 && y1 > y0)
         vgImageSubData(vg->font_atlas,
               atlas->buffer + (size_t)y0 * atlas->width + x0,
               (VGint)atlas->width, VG_A_8,
               (VGint)x0, (VGint)y0, (VGint)(x1 - x0), (VGint)(y1 - y0));
   }
   atlas->dirty = false;
   return true;
}

/* The child image covering @glyph's rectangle of the atlas. Children
 * share the atlas's pixels, so one stays right for whatever glyph its
 * cell holds; one is made only when a rectangle is new to its slot. */
static VGImage vg_font_glyph_image(vg_t *vg, const struct font_glyph *glyph)
{
   vg_glyph_image_t *e;
   uint32_t hash;

   if (!glyph->width || !glyph->height)
      return VG_INVALID_HANDLE;

   hash = ((uint32_t)glyph->atlas_offset_x * 0x9E3779B1u)
        ^ ((uint32_t)glyph->atlas_offset_y * 0x85EBCA77u);
   e    = &vg->font_glyphs[(hash >> 16) & (VG_FONT_CHILDREN - 1)];

   if (     e->image != VG_INVALID_HANDLE
         && e->x == glyph->atlas_offset_x && e->y == glyph->atlas_offset_y
         && e->w == glyph->width          && e->h == glyph->height)
      return e->image;

   if (e->image != VG_INVALID_HANDLE)
      vgDestroyImage(e->image);
   e->x     = (unsigned short)glyph->atlas_offset_x;
   e->y     = (unsigned short)glyph->atlas_offset_y;
   e->w     = (unsigned short)glyph->width;
   e->h     = (unsigned short)glyph->height;
   e->image = vgChildImage(vg->font_atlas,
         (VGint)glyph->atlas_offset_x, (VGint)glyph->atlas_offset_y,
         (VGint)glyph->width, (VGint)glyph->height);
   return e->image;
}

/* Draws @msg from the shared glyph cache through gfx/font_layout.h:
 * each glyph is its atlas rectangle multiplied by the text colour, the
 * drop shadow first. OpenVG's y axis points up, as the message
 * position does. */
static void vg_render_msg(vg_t *vg, const char *msg, size_t msg_len,
      const struct font_params *params, unsigned width, unsigned height)
{
   font_params_resolved_t rp;
   VGfloat color[4];
   struct font_line_metrics *metrics = NULL;
   const struct font_glyph *(*get_glyph)(void*, uint32_t)
                                     = vg->font_driver->get_glyph;
   void *font_data                   = vg->font_data;
   struct font_atlas *atlas          = vg->font_driver->get_atlas(font_data);
   const struct font_glyph *glyph_q;
   float line_height                 = 0.0f;
   float off_x                       = 0.0f;
   float off_y                       = 0.0f;
   float line_x                      = 0.0f;
   float line_y                      = 0.0f;
   float x0, y0, scale;
   bool atlas_ok                     = true;
   int pass;

   if (!atlas || atlas->format != FONT_ATLAS_FORMAT_A8)
      return;

   font_driver_resolve_params(params, &rp);
   scale   = rp.scale;
   x0      = rp.x * (float)width;
   y0      = rp.y * (float)height;
   glyph_q = get_glyph(font_data, '?');
   if (vg->font_driver->get_line_metrics)
   {
      vg->font_driver->get_line_metrics(font_data, &metrics);
      if (metrics)
         line_height = metrics->height;
   }

   vgSeti(VG_SCISSORING, VG_FALSE);
   vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
   vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_MULTIPLY);
   vgSeti(VG_BLEND_MODE, VG_BLEND_SRC_OVER);
   vgSetPaint(vg->font_paint, VG_FILL_PATH);

   for (pass = (rp.drop_x || rp.drop_y) ? 0 : 1; pass < 2; pass++)
   {
      if (pass == 0)
      {
         color[0] = rp.color[0] * rp.drop_mod;
         color[1] = rp.color[1] * rp.drop_mod;
         color[2] = rp.color[2] * rp.drop_mod;
         color[3] = rp.color[3] * rp.drop_alpha;
         off_x    = (float)rp.drop_x * scale;
         off_y    = (float)rp.drop_y * scale;
      }
      else
      {
         color[0] = rp.color[0];
         color[1] = rp.color[1];
         color[2] = rp.color[2];
         color[3] = rp.color[3];
         off_x    = 0.0f;
         off_y    = 0.0f;
      }
      vgSetParameterfv(vg->font_paint, VG_PAINT_COLOR, 4, color);

      /* Every line is looked up before it is drawn, so its new glyphs
       * are in the atlas image by the time they are */
#define FONT_LAYOUT_ALIGNED 1
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
      line_x = x0 + off_x; \
      if (rp.text_align == TEXT_ALIGN_RIGHT) \
         line_x -= (float)(line_width) * scale; \
      else if (rp.text_align == TEXT_ALIGN_CENTER) \
         line_x -= (float)(line_width) * scale / 2.0f; \
      line_y = y0 + off_y - (float)(line) * line_height * scale; \
      if (     atlas->dirty \
            || atlas->width  != vg->font_atlas_w \
            || atlas->height != vg->font_atlas_h) \
         atlas_ok = vg_font_sync_atlas(vg, atlas);
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
      if (atlas_ok) \
      { \
         VGImage vg_glyph_img = vg_font_glyph_image(vg, (glyph)); \
         if (vg_glyph_img != VG_INVALID_HANDLE) \
         { \
            vgLoadIdentity(); \
            vgTranslate( \
                  line_x + (float)((pen_x) + (glyph)->draw_offset_x) * scale, \
                  line_y - (float)((pen_y) + (glyph)->draw_offset_y) * scale); \
            vgScale(scale, -scale); \
            vgDrawImage(vg_glyph_img); \
         } \
      }
#include "../font_layout.h"
   }

   vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_NORMAL);
   vgLoadMatrix(vg->mTransformMatrix.data);
   vgSeti(VG_SCISSORING, VG_TRUE);
}

static void vg_free(void *data)
{
   vg_t                    *vg = (vg_t*)data;

   if (!vg)
      return;

   vgDestroyImage(vg->mImage);

   if (vg->font_data)
   {
      vg_font_release_images(vg);
      vgDestroyPaint(vg->font_paint);
      vg->font_driver->free(vg->font_data);
   }

   if (vg->ctx_driver && vg->ctx_driver->destroy)
      vg->ctx_driver->destroy(vg->ctx_data);
   video_context_driver_free();

   free(vg);
}

static void vg_calculate_quad(vg_t *vg,
      unsigned vp_width, unsigned vp_height)
{
   video_viewport_t vp;

   vp.full_dims    = VIDEO_SCALE_PACK(vp_width, vp_height);

   /* Calculate device_aspect for mScreenAspect (used elsewhere) */
   vg->mScreenAspect = (float)vp_width / vp_height;
   if (vg->ctx_driver->translate_aspect)
      vg->mScreenAspect = vg->ctx_driver->translate_aspect(vg->ctx_data, vp_width, vp_height);

   /* OpenVG uses a bottom-left origin coordinate system */
   video_driver_update_viewport(&vp, false, vg->keep_aspect, false);

   vg->x1 = VIDEO_POS_X(vp.pos);
   vg->y1 = VIDEO_POS_Y(vp.pos);
   vg->x2 = VIDEO_SCALE_W(vp.dims);
   vg->y2 = VIDEO_SCALE_H(vp.dims);

   vg->scissor[0] = vg->x1;
   vg->scissor[1] = vg->y1;
   vg->scissor[2] = vg->x2 - vg->x1;
   vg->scissor[3] = vg->y2 - vg->y1;

   vgSetiv(VG_SCISSOR_RECTS, 4, vg->scissor);
}

static void vg_copy_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   vg_t *vg = (vg_t*)data;

   if (vg->mEglImageBuf)
   {
      EGLImageKHR img = 0;
      bool new_egl    = false;

      if (vg->ctx_driver->image_buffer_write)
         new_egl      = vg->ctx_driver->image_buffer_write(
               vg->ctx_data,
               frame, width, height, pitch,
               (vg->mTexType == VG_sXRGB_8888),
               0,
               &img);

      if (new_egl)
      {
         vgDestroyImage(vg->mImage);
         vg->mImage = pvgCreateEGLImageTargetKHR((VGeglImageKHR) img);
         if (!vg->mImage)
         {
            RARCH_ERR(
                  "[VG] Error creating image: %08x.\n",
                  vgGetError());
            exit(2);
         }
         vg->last_egl_image = img;
      }
   }
   else
      vgImageSubData(vg->mImage, frame, pitch, vg->mTexType, 0, 0, width, height);
}

static bool vg_frame(void *data, const void *frame,
      unsigned dims,
      uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   unsigned frame_width = VIDEO_SCALE_W(dims);
   unsigned frame_height = VIDEO_SCALE_H(dims);
   vg_t                           *vg = (vg_t*)data;
   unsigned width                     = VIDEO_SCALE_W(video_info->dims);
   unsigned height                    = VIDEO_SCALE_H(video_info->dims);
#ifdef HAVE_MENU
   bool menu_is_alive                 = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (     frame_width  != vg->mRenderWidth
         || frame_height != vg->mRenderHeight
         || vg->should_resize)
   {
      vg->mRenderWidth  = frame_width;
      vg->mRenderHeight = frame_height;
      vg_calculate_quad(vg, width, height);
      matrix_3x3_quad_to_quad(
         vg->x1, vg->y1, vg->x2, vg->y1, vg->x2, vg->y2, vg->x1, vg->y2,
         /* needs to be flipped, Khronos loves their bottom-left origin */
         0, frame_height, frame_width, frame_height, frame_width, 0, 0, 0,
         &vg->mTransformMatrix);
      vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
      vgLoadMatrix(vg->mTransformMatrix.data);

      vg->should_resize = false;
   }

   vgSeti(VG_SCISSORING, VG_FALSE);
   vgClear(0, 0, width, height);
   vgSeti(VG_SCISSORING, VG_TRUE);

   vg_copy_frame(vg, frame, frame_width, frame_height, pitch);

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   vgDrawImage(vg->mImage);

   if (msg && *msg && vg->font_data)
      vg_render_msg(vg, msg, strlen(msg), NULL, width, height);

   if (vg->ctx_driver->update_window_title)
      vg->ctx_driver->update_window_title(vg->ctx_data);

   if (vg->ctx_driver->swap_buffers)
      vg->ctx_driver->swap_buffers(vg->ctx_data);

   return true;
}

static bool vg_alive(void *data)
{
   bool quit            = false;
   bool resize          = false;
   unsigned temp_dims  = VIDEO_SCALE_PACK(0,
         0);
   vg_t            *vg  = (vg_t*)data;

   vg->ctx_driver->check_window(vg->ctx_data,
            &quit, &resize, &temp_dims);

   if (VIDEO_SCALE_W(temp_dims) != 0 && VIDEO_SCALE_H(temp_dims) != 0)
      video_driver_set_output_dims(temp_dims);

   return !quit;
}

static bool vg_suppress_screensaver(void *data, bool enable)
{
   bool enabled         = enable;
   vg_t            *vg  = (vg_t*)data;
   if (vg->ctx_data && vg->ctx_driver->suppress_screensaver)
      return vg->ctx_driver->suppress_screensaver(vg->ctx_data, enabled);
   return false;
}

static bool vg_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }
static void vg_get_poke_interface(void *data,
      const video_poke_interface_t **iface) { }

static bool vg_has_windowed(void *data)
{
   vg_t            *vg  = (vg_t*)data;
   if (vg && vg->ctx_driver)
      return vg->ctx_driver->has_windowed;
   return false;
}

static bool vg_focus(void *data)
{
   vg_t            *vg  = (vg_t*)data;
   if (vg && vg->ctx_driver && vg->ctx_driver->has_focus)
      return vg->ctx_driver->has_focus(vg->ctx_data);
   return true;
}

video_driver_t video_vg = {
   vg_init,
   vg_frame,
   vg_set_nonblock_state,
   vg_alive,
   vg_focus,
   vg_suppress_screensaver,
   vg_has_windowed,
   vg_set_shader,
   vg_free,
   "vg",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   vg_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
