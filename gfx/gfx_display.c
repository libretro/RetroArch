/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <string/stdstring.h>

#include "gfx_display.h"

#ifdef HAVE_SDL2
/* SDL_version.h is needed for the SDL_VERSION_ATLEAST gate on the
 * gfx_display_ctx_sdl2 table entry. The driver itself requires
 * SDL_RenderGeometry (>= 2.0.18); on older SDL the symbol is not
 * defined in sdl2_gfx.c, so the table entry must be elided too. */
#include <SDL_version.h>
#endif

#include "../configuration.h"
#include "../tasks/tasks_internal.h"
#include "../verbosity.h"

#include "../input/input_osk.h"
#include "gfx_surface.h"
#ifdef HAVE_THREADS
#include <queues/task_queue.h>
#endif

/* Standard reference DPI value, used when determining
 * DPI-aware scaling factors */
#define REFERENCE_DPI 96.0f

/* 'OZONE_SIDEBAR_WIDTH' must be kept in sync
 * with Ozone driver metrics */
#define OZONE_SIDEBAR_WIDTH 408

/* Small 1x1 white texture used for blending purposes */
static uintptr_t gfx_white_texture;

/* ptr alignment */
static gfx_display_t dispgfx_st = {0};

gfx_display_t *disp_get_ptr(void)
{
   return &dispgfx_st;
}

/* Menu display drivers */
static gfx_display_ctx_driver_t *gfx_display_ctx_drivers[] = {
#ifdef HAVE_D3D8
   &gfx_display_ctx_d3d8,
#endif
#ifdef HAVE_D3D9
#ifdef HAVE_HLSL
   &gfx_display_ctx_d3d9_hlsl,
#endif
#ifdef HAVE_CG
   &gfx_display_ctx_d3d9_cg,
#endif
#endif
#ifdef HAVE_D3D10
   &gfx_display_ctx_d3d10,
#endif
#ifdef HAVE_D3D11
   &gfx_display_ctx_d3d11,
#endif
#ifdef HAVE_D3D12
   &gfx_display_ctx_d3d12,
#endif
#ifdef HAVE_OPENGL
   &gfx_display_ctx_gl,
#endif
#ifdef HAVE_OPENGL1
   &gfx_display_ctx_gl1,
#endif
#ifdef HAVE_OPENGL_CORE
   &gfx_display_ctx_gl3,
#endif
#ifdef HAVE_VULKAN
   &gfx_display_ctx_vulkan,
#endif
#ifdef HAVE_METAL
   &gfx_display_ctx_metal,
#endif
#ifdef HAVE_GXM
   &gfx_display_ctx_gxm,
#endif
#ifdef _3DS
   &gfx_display_ctx_ctr,
#endif
#ifdef WIIU
   &gfx_display_ctx_wiiu,
#endif
#ifdef HAVE_GCM
   &gfx_display_ctx_rsx,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
   &gfx_display_ctx_gdi,
#endif
#endif
#ifdef HAVE_SDL2
#if SDL_VERSION_ATLEAST(2, 0, 18)
   &gfx_display_ctx_sdl2,
#endif
#endif
#ifdef HAVE_SDL3
   &gfx_display_ctx_sdl3,
#endif
   NULL,
};

static float gfx_display_get_dpi_scale_internal(unsigned dims)
{
   float dpi;
   float diagonal_pixels;
   float pixel_scale;
   unsigned width              = VIDEO_SCALE_W(dims);
   unsigned height             = VIDEO_SCALE_H(dims);
   static unsigned last_dims   = 0;
   static float scale          = 0.0f;
   static bool scale_cached    = false;
   gfx_ctx_metrics_t metrics;

   if (scale_cached && dims == last_dims)
      return scale;

   /* Determine the diagonal 'size' of the display
    * (or window) in terms of pixels */
   diagonal_pixels = (float)sqrt(
         (double)((width * width) + (height * height)));

   /* Get pixel scale relative to baseline 1080p display */
   pixel_scale   = diagonal_pixels / (float)DIAGONAL_PIXELS_1080P;

   /* Attempt to get display DPI */
   metrics.type  = DISPLAY_METRIC_DPI;
   metrics.value = &dpi;

   if (video_context_driver_get_metrics(&metrics) && (dpi > 0.0f))
   {
      float display_size;
      float dpi_scale;

#if defined(ANDROID) || defined(HAVE_COCOATOUCH)
      /* Android/iOS devices tell complete lies when
       * reporting DPI values. From the Android devices
       * I've had access to, the DPI is generally
       * overestimated by 17%. All we can do is apply
       * a blind correction factor... */
      dpi *= 0.83f;
#endif

      /* Note: If we are running in windowed mode, this
       * 'display size' is actually the window size - which
       * kinda makes a mess of everything. Since we cannot
       * get fullscreen resolution when running in windowed
       * mode, there is nothing we can do about this. So just
       * treat the window as a display, and hope for the best... */
      display_size = diagonal_pixels / dpi;
      dpi_scale    = dpi / REFERENCE_DPI;

      /* Note: We have tried leveraging every possible metric
       * (and numerous studies on TV/monitor/mobile device
       * usage habits) to determine an appropriate auto scaling
       * factor. *None of these 'smart'/technical methods work
       * consistently in the real world* - there is simply too
       * much variance.
       * So instead we have implemented a very fuzzy/loose
       * method which is crude as can be, but actually has
       * some semblance of usability... */

      if (display_size > 24.0f)
      {
         /* DPI scaling fails miserably when using large
          * displays. Having a UI element that's 1 inch high
          * on all screens might seem like a good idea - until
          * you realise that a HTPC user is probably sitting
          * several metres from their TV, which makes something
          * 1 inch high virtually invisible.
          * So we make some assumptions:
          * - Normal size displays <= 24 inches are probably
          *   PC monitors, with an eye-to-screen distance of
          *   1 arm length. Under these conditions, fixed size
          *   (DPI scaled) UI elements should be visible for most
          *   users
          * - Large displays > 24 inches start to encroach on
          *   TV territory. Once we start working with TVs, we
          *   have to consider users sitting on a couch - and
          *   in this situation, we fall back to the age-old
          *   standard of UI elements occupying a fixed fraction
          *   of the display size (i.e. just look at the menu of
          *   any console system for the past decade)
          * - 24 -> 32 inches is a grey area, where the display
          *   might be a monitor or a TV. Above 32 inches, a TV
          *   is almost a certainty. So we simply lerp between
          *   dpi scaling and pixel scaling as the display size
          *   increases from 24 to 32 */
         float fraction  = (display_size > 32.0f) ? 32.0f : display_size;
         fraction       -= 24.0f;
         fraction       /= (32.0f - 24.0f);

         scale           =   ((1.0f - fraction) * dpi_scale)
                           + (fraction * pixel_scale);
      }
      else if (display_size < 12.0f)
      {
         /* DPI scaling also fails when using very small
          * displays - i.e. mobile devices (tablets/phones).
          * That 1 inch UI element is going to look pretty
          * dumb on a 5 inch screen in landscape orientation...
          * We're essentially in the opposite situation to the
          * TV case above, and it turns out that a similar
          * solution provides relief: as screen size reduces
          * from 12 inches to zero, we lerp from dpi scaling
          * to pixel scaling */
         float fraction = display_size / 12.0f;

         scale          =   ((1.0f - fraction) * pixel_scale)
                          + (fraction * dpi_scale);
      }
      else
         scale          = dpi_scale;
   }
   /* If DPI retrieval is unsupported, all we can do
    * is use the raw pixel scale */
   else
      scale             = pixel_scale;

   scale_cached         = true;
   last_dims            = dims;

   return scale;
}

float gfx_display_get_dpi_scale(
      gfx_display_t *p_disp,
      void *settings_data,
      unsigned dims,
      bool fullscreen,
      bool is_widget
)
{
   static unsigned last_dims                           = 0;
   static float scale                                  = 0.0f;
   static bool scale_cached                            = false;
   bool scale_updated                                  = false;
   static float last_menu_scale_factor                 = 0.0f;
   static enum menu_driver_id_type last_menu_driver_id = MENU_DRIVER_ID_UNKNOWN;
   static float adjusted_scale                         = 1.0f;
   settings_t *settings                                = (settings_t*)settings_data;
#ifdef HAVE_GFX_WIDGETS
   bool gfx_widget_scale_auto                          = settings->bools.menu_widget_scale_auto;
#if (defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   float menu_widget_scale_factor                      = settings->floats.menu_widget_scale_factor;
#else /* !RARCH_CONSOLE && !RARCH_MOBILE */
   float menu_widget_scale_factor_fullscreen           = settings->floats.menu_widget_scale_factor;
   float menu_widget_scale_factor_windowed             = settings->floats.menu_widget_scale_factor_windowed;
   float menu_widget_scale_factor                      = fullscreen ?
         menu_widget_scale_factor_fullscreen : menu_widget_scale_factor_windowed;
#endif /* RARCH_CONSOLE || RARCH_MOBILE */
   float menu_scale_factor                             = is_widget
      ? menu_widget_scale_factor
      : settings->floats.menu_scale_factor;
#else /* !HAVE_GFX_WIDGETS */
   float menu_scale_factor                             = settings->floats.menu_scale_factor;
#endif /* HAVE_GFX_WIDGETS */

#ifdef HAVE_GFX_WIDGETS
   if (is_widget)
   {
      if (gfx_widget_scale_auto)
      {
#ifdef HAVE_RGUI
         /* When using RGUI, _menu_scale_factor
          * is ignored
          * > If we are not using a widget scale factor override,
          *   just set menu_scale_factor to 1.0 */
         if (p_disp->menu_driver_id == MENU_DRIVER_ID_RGUI)
            menu_scale_factor        = 1.0f;
         else
#endif /* HAVE_RGUI */
            menu_scale_factor        = settings->floats.menu_scale_factor;
      }
   }
#endif /* HAVE_GFX_WIDGETS */

   /* Scale is based on display metrics - these are a fixed
    * hardware property. To minimise performance overheads
    * we therefore only call video_context_driver_get_metrics()
    * on first run, or when the current video resolution changes */
   if (!scale_cached || dims != last_dims)
   {
      scale         = gfx_display_get_dpi_scale_internal(dims);
      scale_cached  = true;
      scale_updated = true;
      last_dims     = dims;
   }

   /* Adjusted scale calculation may also be slow, so
    * only update if something changes */
   if (    scale_updated
       || (menu_scale_factor      != last_menu_scale_factor)
       || (p_disp->menu_driver_id != last_menu_driver_id))
   {
      adjusted_scale            = scale * menu_scale_factor;
#ifdef HAVE_OZONE
      if (p_disp->menu_driver_id == MENU_DRIVER_ID_OZONE)
      {
         /* Ozone's sidebar may take a third of the screen and no
          * more, so the scale is capped at whatever puts it there. */
         float sidebar_max      = (float)VIDEO_SCALE_W(dims) / 3.0f;
         if (((float)OZONE_SIDEBAR_WIDTH * adjusted_scale)
               > sidebar_max)
            adjusted_scale      = (sidebar_max / (float)OZONE_SIDEBAR_WIDTH);
      }
#endif
      adjusted_scale            = (adjusted_scale > 0.0001f) ? adjusted_scale : 1.0f;
      last_menu_scale_factor    = menu_scale_factor;
      last_menu_driver_id       = p_disp->menu_driver_id;
   }

   return adjusted_scale;
}

static void gfx_display_flush_impl(gfx_display_t *p_disp);

/* Sends what is gathered and records why it had to go */
static void gfx_display_flush_as(gfx_display_t *p_disp,
      enum gfx_display_flush_reason reason)
{
   if (p_disp && p_disp->batch_quads)
      p_disp->stats.v[GFX_DISPLAY_STAT_FLUSH + reason]++;
   gfx_display_flush_impl(p_disp);
}

/* Begin scissoring operation */
void gfx_display_scissor_begin(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_dims,
      int x, int y, unsigned dims)
{
   unsigned video_width              = VIDEO_SCALE_W(video_dims);
   unsigned video_height             = VIDEO_SCALE_H(video_dims);
   unsigned width                    = VIDEO_SCALE_W(dims);
   unsigned height                   = VIDEO_SCALE_H(dims);
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   /* What is gathered goes out before this draws */
   gfx_display_flush_as(disp_get_ptr(), GFX_DISPLAY_FLUSH_SCISSOR);
   if (dispctx && dispctx->scissor_begin)
   {
      if (y < 0)
      {
         if (height < (unsigned)(-y))
            height  = 0;
         else
            height += y;
         y          = 0;
      }
      if (x < 0)
      {
         if (width < (unsigned)(-x))
            width   = 0;
         else
            width  += x;
         x          = 0;
      }
      if (y >= (int)video_height)
      {
         height     = 0;
         y          = 0;
      }
      if (x >= (int)video_width)
      {
         width      = 0;
         x          = 0;
      }
      if ((y + height) > video_height)
         height     = video_height - y;
      if ((x + width) > video_width)
         width      = video_width - x;

      dispctx->scissor_begin(userdata, video_dims,
            x, y, dims);
   }
}

font_data_t *gfx_display_font_file(
      gfx_display_t *p_disp,
      char* fontpath, float menu_font_size, bool is_threaded)
{
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   if (dispctx)
   {
      font_data_t        *font_data  = NULL;
      float               font_size  = menu_font_size;
      /* Font size must be at least 2, or font_init_first()
       * will generate a heap-buffer-overflow when using
       * many font drivers */
      if (font_size < 2.0f)
         font_size = 2.0f;
      if ((font_data = font_driver_init_first(video_driver_get_ptr(),
                  fontpath, font_size, true, is_threaded,
                  dispctx->font_backend)))
         return font_data;
   }
   return NULL;
}

/* Draw text on top of the screen */
static void gfx_display_draw_text_internal(
      const font_data_t *font, const char *text,
      float x, float y, unsigned dims,
      uint32_t color, const float *color_hp,
      enum text_alignment text_align,
      float scale, bool shadows_enable, float shadow_offset,
      bool draw_outside)
{
   size_t _len;
   struct font_params params;
   int width                      = (int)VIDEO_SCALE_W(dims);
   int height                     = (int)VIDEO_SCALE_H(dims);
   gfx_display_t *p_disp          = disp_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   /* What is gathered goes out before this draws */
   gfx_display_flush_as(p_disp, GFX_DISPLAY_FLUSH_TEXT);

   /* NULL text is a no-op: ozone_draw_footer and similar menu code can
    * legitimately reach here with text==NULL for unset/optional fields,
    * and the original code path passed it straight through to
    * font_driver_render_msg whose (msg && *msg) check turned it into
    * a no-op.  Now that we strlen() at this boundary, NULL has to be
    * caught before the strlen. */
   if (!text)
      return;

   if ((color & 0x000000FF) == 0)
      return;

   /* Don't draw outside of the screen */
   if ( !draw_outside
         && ((x < -64 || x > width  + 64)
         ||  (y < -64 || y > height + 64))
      )
      return;

   params.x           = x / width;
   params.y           = 1.0f - y / height;
   params.scale       = scale;
   params.drop_mod    = 0.0f;
   params.drop_x      = 0.0f;
   params.drop_y      = 0.0f;
   params.color       = color;
   params.color_hp    = color_hp;
   params.full_screen = true;
   params.text_align  = text_align;

   if (shadows_enable)
   {
      params.drop_x      = shadow_offset;
      params.drop_y      = -shadow_offset;
      params.drop_alpha  = GFX_SHADOW_ALPHA;
   }

   _len = strlen(text);
   p_disp->stats.v[GFX_DISPLAY_STAT_TEXT_CALLS]++;
   p_disp->stats.v[GFX_DISPLAY_STAT_TEXT_BYTES] += (unsigned)_len;

   if (video_st->poke && video_st->poke->set_osd_msg)
      video_st->poke->set_osd_msg(video_st->data,
            text, _len, &params, (void*)font);
}

void gfx_display_draw_text(
      const font_data_t *font, const char *text,
      float x, float y, unsigned dims,
      uint32_t color, enum text_alignment text_align,
      float scale, bool shadows_enable, float shadow_offset,
      bool draw_outside)
{
   gfx_display_draw_text_internal(font, text, x, y, dims,
         color, NULL, text_align, scale, shadows_enable, shadow_offset,
         draw_outside);
}

/* As gfx_display_draw_text, but drives the glyph colour at full float
 * precision (color_rgba points to 4 floats R,G,B,A in 0..1) so text can
 * exceed 8 bits per channel on a deep-colour framebuffer. The 8-bit 'color'
 * is still supplied for backends that ignore the high-precision path (they
 * fall back to it), so pass an equivalent packed value. Font backends that
 * do not opt in behave exactly as the 8-bit entry point. */
void gfx_display_draw_text_hp(
      const font_data_t *font, const char *text,
      float x, float y, unsigned dims,
      uint32_t color, const float *color_rgba,
      enum text_alignment text_align,
      float scale, bool shadows_enable, float shadow_offset,
      bool draw_outside)
{
   gfx_display_draw_text_internal(font, text, x, y, dims,
         color, color_rgba, text_align, scale, shadows_enable,
         shadow_offset, draw_outside);
}

void gfx_display_draw_bg(
      gfx_display_t *p_disp,
      gfx_display_ctx_draw_t *draw,
      struct video_coords *coords,
      void *userdata, bool add_opacity_to_wallpaper,
      float override_opacity)
{
   const float           *new_vertex = NULL;
   const float        *new_tex_coord = NULL;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   if (!dispctx || !draw || !coords)
      return;

   if (draw->vertex)
      new_vertex                     = draw->vertex;
   else if (dispctx->get_default_vertices)
      new_vertex                     = dispctx->get_default_vertices();

   if (draw->tex_coord)
      new_tex_coord                  = draw->tex_coord;
   else if (dispctx->get_default_tex_coords)
      new_tex_coord                  = dispctx->get_default_tex_coords();

   coords->vertices                  = (unsigned)draw->vertex_count;
   coords->vertex                    = new_vertex;
   coords->tex_coord                 = new_tex_coord;
   coords->lut_tex_coord             = new_tex_coord;
   coords->color                     = (const float*)draw->color;

   draw->coords                      = coords;
   draw->scale_factor                = 1.0f;
   draw->rotation                    = 0.0f;

   if (draw->texture)
      add_opacity_to_wallpaper       = true;
   else
      draw->texture                  = gfx_white_texture;

   if (add_opacity_to_wallpaper)
      gfx_display_set_alpha(draw->color, override_opacity);

   if (dispctx->get_default_mvp)
      draw->matrix_data = (math_matrix_4x4*)dispctx->get_default_mvp(
            userdata);
}

/* How many quads may wait before the batch has to go out. One strip of
 * them is six vertices a quad less the two the first does not need to
 * be joined by, which is what the block below is sized for: 32 quads
 * is 190 vertices, six kilobytes for the lot. A run of quads between
 * two things that are not quads measured three or four, so this is
 * room to spare; going over it costs a draw, not a correction. */
#define GFX_DISPLAY_BATCH_QUADS 32
#define GFX_DISPLAY_BATCH_VERTS (GFX_DISPLAY_BATCH_QUADS * 6 - 2)

/* Adds one quad to the batch, in the strip order the drivers draw in -
 * bottom left, bottom right, top left, top right - joined to the quad
 * before it by a vertex repeated at each end of the seam, which the
 * rasteriser drops as zero-area. Returns false when the quad cannot
 * join, and the caller draws it itself. */
static bool gfx_display_batch_add(gfx_display_t *p_disp,
      uintptr_t texture, const float *color, void *userdata,
      unsigned video_dims,
      float x0, float x1, float y0, float y1,
      int px, int py, unsigned dims)
{
   unsigned video_width  = VIDEO_SCALE_W(video_dims);
   unsigned video_height = VIDEO_SCALE_H(video_dims);
   unsigned v, i;
   float *vert, *tex, *col;

   if (!p_disp)
      return false;
   /* A batch belongs to one texture and one frame's worth of state */
   if (     p_disp->batch_quads
         && (  p_disp->batch_texture     != texture
            || p_disp->batch_userdata    != userdata
            || VIDEO_SCALE_W(p_disp->batch_video_dims) != video_width
            || VIDEO_SCALE_H(p_disp->batch_video_dims) != video_height))
      gfx_display_flush_as(p_disp, GFX_DISPLAY_FLUSH_TEXTURE);
   if (p_disp->batch_quads >= GFX_DISPLAY_BATCH_QUADS)
      gfx_display_flush_as(p_disp, GFX_DISPLAY_FLUSH_CAPACITY);

   if (!p_disp->batch_mem)
   {
      /* 2 + 2 + 4 floats a vertex, in one block: they are filled
       * together and read together, so they are kept together. */
      if (!(p_disp->batch_mem = (float*)malloc(
                  sizeof(float) * 8 * GFX_DISPLAY_BATCH_VERTS)))
         return false;
      p_disp->batch_vertex = p_disp->batch_mem;
      p_disp->batch_tex    = p_disp->batch_mem + 2 * GFX_DISPLAY_BATCH_VERTS;
      p_disp->batch_color  = p_disp->batch_mem + 4 * GFX_DISPLAY_BATCH_VERTS;
   }

   vert = p_disp->batch_vertex;
   tex  = p_disp->batch_tex;
   col  = p_disp->batch_color;
   v    = p_disp->batch_quads ? (p_disp->batch_quads * 6 - 2) : 0;

   if (p_disp->batch_quads)
   {
      /* Seam: the quad before ends where this one starts */
      vert[v * 2]     = vert[(v - 1) * 2];
      vert[v * 2 + 1] = vert[(v - 1) * 2 + 1];
      tex [v * 2]     = tex [(v - 1) * 2];
      tex [v * 2 + 1] = tex [(v - 1) * 2 + 1];
      for (i = 0; i < 4; i++)
         col[v * 4 + i] = col[(v - 1) * 4 + i];
      v++;
      vert[v * 2]     = x0;
      vert[v * 2 + 1] = y0;
      tex [v * 2]     = 0.0f;
      tex [v * 2 + 1] = 1.0f;
      for (i = 0; i < 4; i++)
         col[v * 4 + i] = color[i];
      v++;
   }

   for (i = 0; i < 4; i++)
   {
      unsigned c;
      /* bottom left, bottom right, top left, top right */
      vert[v * 2]     = (i & 1) ? x1   : x0;
      vert[v * 2 + 1] = (i & 2) ? y1   : y0;
      tex [v * 2]     = (i & 1) ? 1.0f : 0.0f;
      tex [v * 2 + 1] = (i & 2) ? 0.0f : 1.0f;
      for (c = 0; c < 4; c++)
         col[v * 4 + c] = color[i * 4 + c];
      v++;
   }

   if (p_disp->batch_quads == 0)
   {
      p_disp->batch_first_x   = px;
      p_disp->batch_first_y   = py;
      p_disp->batch_first_dims = dims;
   }
   p_disp->batch_quads++;
   p_disp->stats.v[GFX_DISPLAY_STAT_QUADS]++;
   p_disp->batch_texture      = texture;
   p_disp->batch_userdata     = userdata;
   p_disp->batch_video_dims   = video_dims;
   return true;
}

/* Sends the quads that are waiting, as one strip, and empties the
 * batch. Called before anything else draws, so that what was gathered
 * lands under what comes after it, and at the end of a frame so that
 * nothing is still waiting when the frame is over. */
static void gfx_display_flush_impl(gfx_display_t *p_disp)
{
   gfx_display_ctx_driver_t *dispctx;
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;

   if (!p_disp || !p_disp->batch_quads)
      return;
   dispctx                 = p_disp->dispctx;
   p_disp->stats.v[GFX_DISPLAY_STAT_BATCHES]++;
   if (p_disp->batch_quads > p_disp->stats.v[GFX_DISPLAY_STAT_BATCH_MAX])
      p_disp->stats.v[GFX_DISPLAY_STAT_BATCH_MAX] = p_disp->batch_quads;
   coords.lut_tex_coord    = NULL;
   if (p_disp->batch_quads == 1)
   {
      /* One quad: hand it over as a quad */
      coords.vertices      = 4;
      coords.vertex        = NULL;
      coords.tex_coord     = NULL;
      coords.color         = p_disp->batch_color;
      draw.pos             = VIDEO_POS_PACK(p_disp->batch_first_x,
            p_disp->batch_first_y);
      draw.dims            = p_disp->batch_first_dims;
   }
   else
   {
      coords.vertices      = p_disp->batch_quads * 6 - 2;
      coords.vertex        = p_disp->batch_vertex;
      coords.tex_coord     = p_disp->batch_tex;
      coords.color         = p_disp->batch_color;
      draw.pos             = VIDEO_POS_PACK(0, 0);
      draw.dims            = p_disp->batch_video_dims;
   }
   draw.coords             = &coords;
   draw.matrix_data        = NULL;
   draw.texture            = p_disp->batch_texture;
   draw.pipeline_id        = 0;
   draw.scale_factor       = 1.0f;
   draw.rotation           = 0.0f;
   p_disp->batch_quads     = 0;
   if (dispctx)
   {
      /* Inside a caller's group blending is already on and stays on:
       * turning it off here would end the group early. */
      bool own_blend = !p_disp->blend_on;
      if (own_blend && dispctx->blend_begin)
         dispctx->blend_begin(p_disp->batch_userdata);
      if (dispctx->draw)
         dispctx->draw(&draw, p_disp->batch_userdata,
               p_disp->batch_video_dims);
      if (own_blend && dispctx->blend_end)
         dispctx->blend_end(p_disp->batch_userdata);
   }
}

/* The quad over the whole screen, in clip space, as a strip */
static gfx_display_mesh_vertex_t gfx_display_mesh_fs_vertices[4] = {
   { -1.0f, -1.0f, 0.0f,     0, 65535, { 255, 255, 255, 255 } },
   {  1.0f, -1.0f, 0.0f, 65535, 65535, { 255, 255, 255, 255 } },
   { -1.0f,  1.0f, 0.0f,     0,     0, { 255, 255, 255, 255 } },
   {  1.0f,  1.0f, 0.0f, 65535,     0, { 255, 255, 255, 255 } }
};
static float gfx_display_mesh_fs_positions[8] = {
   -1.0f, -1.0f,  1.0f, -1.0f,  -1.0f, 1.0f,  1.0f, 1.0f
};
static gfx_display_mesh_t gfx_display_mesh_fs = {
   gfx_display_mesh_fs_vertices, NULL, 4, 0, GFX_MESH_TRIANGLE_STRIP, 1,
   gfx_display_mesh_fs_positions
};
/* Mesh ids: 1 is the full-screen quad's, and none is ever reused */
static uint32_t gfx_display_mesh_next_id = 2;

/* Where the plain programs are transformed into on drivers that do not
 * draw meshes themselves: positions, texture coordinates and colours,
 * grown when a mesh needs more, never per frame */
static float  *gfx_display_mesh_scratch     = NULL;
static size_t  gfx_display_mesh_scratch_cap = 0;

gfx_display_mesh_t *gfx_display_mesh_create(const gfx_display_mesh_desc_t *desc)
{
   gfx_display_mesh_t *mesh;
   size_t vbytes, ibytes, pbytes;
   unsigned i;

   if (     !desc || !desc->vertices || !desc->vertex_count
         || desc->vertex_count > 65536
         || (desc->indices && !desc->index_count))
      return NULL;
   if (desc->indices)
      for (i = 0; i < desc->index_count; i++)
         if (desc->indices[i] >= desc->vertex_count)
            return NULL;
   if (     desc->topology == GFX_MESH_TRIANGLES
         && ((desc->indices ? desc->index_count : desc->vertex_count) % 3))
      return NULL;

   vbytes = (size_t)desc->vertex_count * sizeof(gfx_display_mesh_vertex_t);
   ibytes = desc->indices ? (size_t)desc->index_count * sizeof(uint16_t) : 0;
   pbytes = (desc->flags & GFX_MESH_FLAG_POSITIONS)
      ? 2 * sizeof(float) * (size_t)desc->vertex_count : 0;
   /* One allocation: the header, the vertices, the positions, then the
    * indices - in that order, which keeps each aligned for its type */
   if (!(mesh = (gfx_display_mesh_t*)malloc(sizeof(*mesh)
               + vbytes + pbytes + ibytes)))
      return NULL;
   mesh->vertices     = (gfx_display_mesh_vertex_t*)(mesh + 1);
   mesh->positions    = pbytes ? (float*)((uint8_t*)mesh->vertices + vbytes) : NULL;
   mesh->indices      = ibytes
      ? (uint16_t*)((uint8_t*)mesh->vertices + vbytes + pbytes) : NULL;
   mesh->vertex_count = desc->vertex_count;
   mesh->index_count  = desc->indices ? desc->index_count : 0;
   mesh->topology     = desc->topology;
   mesh->id           = gfx_display_mesh_next_id++;
   memcpy(mesh->vertices, desc->vertices, vbytes);
   if (ibytes)
      memcpy(mesh->indices, desc->indices, ibytes);

   if (mesh->positions)
      for (i = 0; i < desc->vertex_count; i++)
      {
         mesh->positions[2 * i]     = desc->vertices[i].x;
         mesh->positions[2 * i + 1] = desc->vertices[i].y;
      }
   return mesh;
}

void gfx_display_mesh_free(gfx_display_mesh_t *mesh)
{
   free(mesh);
}

const gfx_display_mesh_t *gfx_display_mesh_fullscreen(void)
{
   return &gfx_display_mesh_fs;
}

struct video_coords *gfx_display_effect_coords(gfx_display_t *p_disp)
{
   static struct video_coords coords;
   const gfx_display_mesh_t *mesh = p_disp ? p_disp->effect_mesh : NULL;
   if (!mesh || !mesh->positions)
      return NULL;
   coords.vertex        = mesh->positions;
   coords.tex_coord     = NULL;
   coords.color         = NULL;
   coords.lut_tex_coord = NULL;
   coords.vertices      = mesh->vertex_count;
   return &coords;
}

/* An effect, drawn the way the driver draws its menu pipelines: over
 * the background's draw state, with the pipeline's own geometry */
static void gfx_display_mesh_draw_effect(gfx_display_t *p_disp,
      gfx_display_ctx_driver_t *dispctx, void *userdata,
      unsigned video_dims, const gfx_display_mesh_t *mesh,
      const gfx_display_mesh_draw_t *md)
{
#ifdef HAVE_SHADERPIPELINE
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;

   draw.pos          = VIDEO_POS_PACK(0, 0);
   draw.texture      = md->texture;
   draw.dims         = video_dims;
   draw.color        = md->color;
   draw.vertex       = NULL;
   draw.tex_coord    = NULL;
   draw.vertex_count = 4;
   draw.pipeline_id  = 0;
   /* The drivers that stream an effect's geometry read its positions */
   if (!md->color || !mesh->positions)
      return;
   /* The colour keeps the alpha it carries */
   gfx_display_draw_bg(p_disp, &draw, &coords, userdata, true,
         md->color[3]);

   switch (md->program)
   {
      case GFX_MESH_PROGRAM_RIBBON:
         draw.pipeline_id = VIDEO_SHADER_MENU;
         break;
      case GFX_MESH_PROGRAM_RIBBON_SIMPLE:
         draw.pipeline_id = VIDEO_SHADER_MENU_2;
         break;
      case GFX_MESH_PROGRAM_SNOW_SIMPLE:
         draw.pipeline_id = VIDEO_SHADER_MENU_3;
         break;
      case GFX_MESH_PROGRAM_SNOW:
         draw.pipeline_id = VIDEO_SHADER_MENU_4;
         break;
      case GFX_MESH_PROGRAM_BOKEH:
         draw.pipeline_id = VIDEO_SHADER_MENU_5;
         break;
      case GFX_MESH_PROGRAM_SNOWFLAKE:
         draw.pipeline_id = VIDEO_SHADER_MENU_6;
         break;
      default:
         draw.pipeline_id = VIDEO_SHADER_STOCK_BLEND;
         break;
   }

   p_disp->effect_mesh = mesh;
   if (dispctx->draw_pipeline)
      dispctx->draw_pipeline(&draw, p_disp, userdata, video_dims);
   gfx_display_draw(dispctx, &draw, userdata, video_dims);
   p_disp->effect_mesh = NULL;

   /* Wrapped where the step is still exact: 0.01 stays representable
    * to about 167772, and a wrap every 30 hours of menu is not seen */
   p_disp->effect_time += 0.01f;
   if (p_disp->effect_time > 65536.0f)
      p_disp->effect_time -= 65536.0f;
#endif
}

/* A plain program on a driver that does not draw meshes itself: every
 * vertex transformed here into the display's 0..1 space and the whole
 * mesh sent as one strip, triangles joined by repeated vertices */
static void gfx_display_mesh_draw_cpu(gfx_display_t *p_disp,
      gfx_display_ctx_driver_t *dispctx, void *userdata,
      unsigned video_dims, const gfx_display_mesh_t *mesh,
      const gfx_display_mesh_draw_t *md)
{
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;
   const float *m   = md->mvp;
   unsigned count   = mesh->indices ? mesh->index_count : mesh->vertex_count;
   unsigned n_out   = (mesh->topology == GFX_MESH_TRIANGLES)
      ? (count / 3) * 5 : count;
   unsigned i, out  = 0;
   float tint[4];
   float *xy, *uv, *col;

   if (!dispctx->handles_vertex_strip || !n_out)
      return;
   if ((size_t)n_out * 8 > gfx_display_mesh_scratch_cap)
   {
      float *grown = (float*)realloc(gfx_display_mesh_scratch,
            (size_t)n_out * 8 * sizeof(float));
      if (!grown)
         return;
      gfx_display_mesh_scratch     = grown;
      gfx_display_mesh_scratch_cap = (size_t)n_out * 8;
   }
   xy  = gfx_display_mesh_scratch;
   uv  = xy + 2 * (size_t)n_out;
   col = uv + 2 * (size_t)n_out;

   for (i = 0; i < 4; i++)
      tint[i] = md->color ? md->color[i] : 1.0f;

   for (i = 0; i < count; )
   {
      /* A triangle is five strip vertices, a b c with a and c twice;
       * a strip is its vertices as they are */
      unsigned take = (mesh->topology == GFX_MESH_TRIANGLES) ? 3 : 1;
      unsigned k, j;
      float cx[3], cy[3], cw[3];
      const gfx_display_mesh_vertex_t *v[3];
      bool behind = false;

      for (k = 0; k < take; k++)
      {
         const gfx_display_mesh_vertex_t *p = &mesh->vertices[
            mesh->indices ? mesh->indices[i + k] : i + k];
         v[k] = p;
         if (m)
         {
            cx[k] = m[0] * p->x + m[4] * p->y + m[8]  * p->z + m[12];
            cy[k] = m[1] * p->x + m[5] * p->y + m[9]  * p->z + m[13];
            cw[k] = m[3] * p->x + m[7] * p->y + m[11] * p->z + m[15];
         }
         else
         {
            cx[k] = p->x;
            cy[k] = p->y;
            cw[k] = 1.0f;
         }
         if (cw[k] <= 1e-6f)
            behind = true;
      }
      i += take;

      /* No near clipping: a triangle reaching behind the eye is not
       * drawn rather than drawn inside out */
      if (behind)
      {
         if (take == 1)
            return;
         continue;
      }

      for (j = 0; j < ((take == 3) ? 5u : 1u); j++)
      {
         static const unsigned tri_order[5] = { 0, 0, 1, 2, 2 };
         unsigned c = (take == 3) ? tri_order[j] : 0;
         const gfx_display_mesh_vertex_t *p = v[c];
         xy [2 * out]     = (cx[c] / cw[c] + 1.0f) * 0.5f;
         xy [2 * out + 1] = (cy[c] / cw[c] + 1.0f) * 0.5f;
         uv [2 * out]     = p->u * (1.0f / 65535.0f);
         uv [2 * out + 1] = p->v * (1.0f / 65535.0f);
         col[4 * out]     = p->rgba[0] * (1.0f / 255.0f) * tint[0];
         col[4 * out + 1] = p->rgba[1] * (1.0f / 255.0f) * tint[1];
         col[4 * out + 2] = p->rgba[2] * (1.0f / 255.0f) * tint[2];
         col[4 * out + 3] = p->rgba[3] * (1.0f / 255.0f) * tint[3];
         out++;
      }
   }
   if (out < 3)
      return;

   coords.vertices      = out;
   coords.vertex        = xy;
   coords.tex_coord     = uv;
   coords.color         = col;
   coords.lut_tex_coord = NULL;
   draw.coords          = &coords;
   draw.pos             = VIDEO_POS_PACK(0, 0);
   draw.dims            = video_dims;
   draw.matrix_data     = NULL;
   draw.texture         = (md->program == GFX_MESH_PROGRAM_TEXTURED && md->texture)
      ? md->texture : gfx_white_texture;
   draw.pipeline_id     = 0;
   draw.scale_factor    = 1.0f;
   draw.rotation        = 0.0f;
   draw.color           = col;
   draw.vertex          = NULL;
   draw.tex_coord       = NULL;
   draw.vertex_count    = out;
   draw.backend_data    = NULL;
   draw.backend_data_size = 0;
   gfx_display_draw(dispctx, &draw, userdata, video_dims);
}

void gfx_display_mesh_draw(gfx_display_t *p_disp, void *userdata,
      unsigned video_dims, const gfx_display_mesh_t *mesh,
      const gfx_display_mesh_draw_t *md)
{
   gfx_display_ctx_driver_t *dispctx;

   if (!p_disp || !mesh || !md || !(dispctx = p_disp->dispctx))
      return;
   if (md->program >= GFX_MESH_PROGRAM_BLEND)
   {
      gfx_display_mesh_draw_effect(p_disp, dispctx, userdata, video_dims,
            mesh, md);
      return;
   }
   if (dispctx->mesh_draw)
   {
      /* Clip space to the display's 0..1, before the perspective
       * divide: x' = (x + w) / 2, so x'/w = (x/w + 1) / 2. Depth is
       * flattened, as the streamed path flattens it. Column-major. */
      static const float to_display[16] = {
         0.5f, 0.0f, 0.0f, 0.0f,
         0.0f, 0.5f, 0.0f, 0.0f,
         0.0f, 0.0f, 0.0f, 0.0f,
         0.5f, 0.5f, 0.0f, 1.0f };
      float mvp[16];
      float tint[4];
      uintptr_t texture = (md->program == GFX_MESH_PROGRAM_TEXTURED
            && md->texture) ? md->texture : gfx_white_texture;
      unsigned r, c, k;
      for (k = 0; k < 4; k++)
         tint[k] = md->color ? md->color[k] : 1.0f;
      if (md->mvp)
      {
         for (c = 0; c < 4; c++)
            for (r = 0; r < 4; r++)
            {
               float sum = 0.0f;
               for (k = 0; k < 4; k++)
                  sum += to_display[k * 4 + r] * md->mvp[c * 4 + k];
               mvp[c * 4 + r] = sum;
            }
      }
      else
         memcpy(mvp, to_display, sizeof(mvp));
      gfx_display_flush_as(p_disp, GFX_DISPLAY_FLUSH_DRAW);
      if (dispctx->mesh_draw(userdata, video_dims, mesh, mvp, texture, tint))
         return;
   }
   gfx_display_mesh_draw_cpu(p_disp, dispctx, userdata, video_dims,
         mesh, md);
}

void gfx_display_flush_batch(gfx_display_t *p_disp)
{
   if (p_disp && p_disp->batch_quads)
      p_disp->stats.v[GFX_DISPLAY_STAT_FLUSH + GFX_DISPLAY_FLUSH_EXPLICIT]++;
   gfx_display_flush_impl(p_disp);
}

void gfx_display_stats_latch(gfx_display_t *p_disp)
{
   unsigned i;
   if (!p_disp)
      return;
   for (i = 0; i < GFX_DISPLAY_STAT_LAST; i++)
   {
      retro_atomic_store_relaxed_int(&p_disp->stats_pub[i],
            (int)p_disp->stats.v[i]);
      p_disp->stats.v[i] = 0;
   }
}

void gfx_display_stats_get(gfx_display_stats_t *out)
{
   unsigned i;
   gfx_display_t *p_disp = disp_get_ptr();
   for (i = 0; i < GFX_DISPLAY_STAT_LAST; i++)
      out->v[i] = (unsigned)retro_atomic_load_relaxed_int(
            &p_disp->stats_pub[i]);
}

void gfx_display_blend_begin(gfx_display_ctx_driver_t *dispctx,
      void *userdata)
{
   gfx_display_t *p_disp = disp_get_ptr();
   /* What was gathered outside this group goes out under the state it
    * was gathered under */
   gfx_display_flush_as(p_disp, GFX_DISPLAY_FLUSH_BLEND);
   if (dispctx && dispctx->blend_begin)
   {
      dispctx->blend_begin(userdata);
      p_disp->blend_on = true;
   }
}

void gfx_display_blend_end(gfx_display_ctx_driver_t *dispctx,
      void *userdata)
{
   gfx_display_t *p_disp = disp_get_ptr();
   /* And what was gathered inside it goes out while it is still on */
   gfx_display_flush_as(p_disp, GFX_DISPLAY_FLUSH_BLEND);
   if (dispctx && dispctx->blend_end)
   {
      dispctx->blend_end(userdata);
      p_disp->blend_on = false;
   }
}

/* The one way a caller outside this file reaches the display driver.
 * Everything drawn while the menu is up passes through here or through
 * the helpers above it, which is what lets this file know the order
 * things are drawn in - and, when quads start being gathered rather
 * than drawn one at a time, where the gathered ones have to go out. */
void gfx_display_draw(gfx_display_ctx_driver_t *dispctx,
      gfx_display_ctx_draw_t *draw, void *userdata,
      unsigned video_dims)
{
   gfx_display_flush_as(disp_get_ptr(), GFX_DISPLAY_FLUSH_DRAW);
   if (dispctx && dispctx->draw && draw)
      dispctx->draw(draw, userdata, video_dims);
}

void gfx_display_draw_quad(
      gfx_display_t *p_disp,
      void *data,
      unsigned video_dims,
      int x, int y, unsigned dims,
      unsigned ref_dims,
      float *color,
      uintptr_t *texture)
{
   unsigned w            = VIDEO_SCALE_W(dims);
   unsigned h            = VIDEO_SCALE_H(dims);
   unsigned width        = VIDEO_SCALE_W(ref_dims);
   unsigned height       = VIDEO_SCALE_H(ref_dims);
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;
   gfx_display_ctx_driver_t
      *dispctx             = p_disp->dispctx;

   if (w == 0 || h == 0)
      return;
   if (!dispctx || !color)
      return;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   draw.pos             = VIDEO_POS_PACK(x, (int)height - y - (int)h);
   draw.dims            = dims;
   draw.coords          = &coords;
   draw.matrix_data     = NULL;
   draw.texture         = (texture && *texture)
      ? *texture
      : gfx_white_texture;
   draw.pipeline_id     = 0;
   draw.scale_factor    = 1.0f;
   draw.rotation        = 0.0f;

   /* Gathered rather than drawn, where the driver can be handed a
    * strip of quads instead of one at a time. What is gathered goes
    * out before anything else draws, so the order is unchanged. */
   if (     dispctx->handles_vertex_strip
         && gfx_display_batch_add(p_disp, draw.texture, color, data,
            video_dims,
            (float)x / (float)width,
            (float)(x + (int)w) / (float)width,
            (float)VIDEO_POS_Y(draw.pos) / (float)height,
            (float)(VIDEO_POS_Y(draw.pos) + (int)h) / (float)height,
            VIDEO_POS_X(draw.pos), VIDEO_POS_Y(draw.pos),
            draw.dims))
      return;

   gfx_display_flush_as(p_disp, GFX_DISPLAY_FLUSH_DRAW);
   if (dispctx->blend_begin)
      dispctx->blend_begin(data);
   if (dispctx->draw)
      dispctx->draw(&draw, data, video_dims);
   if (dispctx->blend_end)
      dispctx->blend_end(data);
}

/* Draw the texture split into 9 sections, without scaling the corners.
 * The middle sections will only scale in the X axis, and the side
 * sections will only scale in the Y axis. */
void gfx_display_draw_texture_slice(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_dims,
      int x, int y, unsigned src_dims,
      unsigned dst_dims,
      unsigned dims,
      float *color, unsigned offset, float scale_factor, uintptr_t texture,
      math_matrix_4x4 *mymat
)
{
   unsigned w                        = VIDEO_SCALE_W(src_dims);
   unsigned h                        = VIDEO_SCALE_H(src_dims);
   unsigned new_w                    = VIDEO_SCALE_W(dst_dims);
   unsigned new_h                    = VIDEO_SCALE_H(dst_dims);
   unsigned width                    = VIDEO_SCALE_W(dims);
   unsigned height                   = VIDEO_SCALE_H(dims);
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   /* The top left piece; the grid below walks out from it */
   float V_BL[2], V_TL[2];
   /* Nine pieces of four vertices, with two more at each of the eight
    * seams that join them into one strip */
   float tex_coord[52 * 2];
   float vert_coord[52 * 2];
   /* One colour per vertex: the source carries four, for a single
    * quad's corners, and every piece repeats them */
   float vert_color[52 * 4];
   float max_scale_w, max_scale_h, slice_scale;
   float vert_woff, vert_hoff, tex_woff, tex_hoff;
   float vert_scaled_mid_width, vert_scaled_mid_height;
   float tex_mid_width, tex_mid_height;
   float norm_x, norm_y;
   static const float colors[16] = {
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f
   };

   /* What is gathered goes out before this draws */
   gfx_display_flush_as(disp_get_ptr(), GFX_DISPLAY_FLUSH_DRAW);

   /* Early-out: guard against division by zero from
    * zero display dimensions or zero texture dimensions */
   if (width == 0 || height == 0 || w == 0 || h == 0)
      return;
   if (!dispctx || !dispctx->draw)
      return;

   /* To prevent visible seams between the corners and
    * middle segments of the sliced texture, the texture
    * must be scaled such that its effective size (before
    * expansion of the middle segments) is no greater than
    * the requested display size.
    * > Whenever an image is scaled, the colours at the
    *   transparent edges get interpolated, which means
    *   the colours of the transparent pixels bleed into
    *   the visible area.
    * > This is a problem because (by design) the corners
    *   of the sliced texture are drawn at native resolution,
    *   whereas the middle segments are stretched to fit.
    * > When *downscaling*, the interpolation effects dominate
    *   the output - creating an ugly corner/middle transition.
    * > Workaround: force downscaling of the entire texture
    *   (including corners) whenever the requested display size
    *   is less than the texture dimensions. */
   max_scale_w = (float)new_w / (float)w;
   max_scale_h = (float)new_h / (float)h;

   /* Find the minimum of scale_factor, max_scale_w, max_scale_h */
   slice_scale = (scale_factor < max_scale_w)
      ? ((scale_factor < max_scale_h) ? scale_factor : max_scale_h)
      : ((max_scale_w  < max_scale_h) ? max_scale_w  : max_scale_h);

   /* Normalized width/height of the amount to offset from the corners,
    * for both the vertex and texture coordinates */
   vert_woff   = (offset * slice_scale) / (float)width;
   vert_hoff   = (offset * slice_scale) / (float)height;
   tex_woff    = offset / (float)w;
   tex_hoff    = offset / (float)h;

   /* The width/height of the middle sections of both
    * the scaled and original image */
   vert_scaled_mid_width  = (new_w - (offset * slice_scale * 2))
      / (float)width;
   vert_scaled_mid_height = (new_h - (offset * slice_scale * 2))
      / (float)height;
   tex_mid_width          = (w - (offset * 2)) / (float)w;
   tex_mid_height         = (h - (offset * 2)) / (float)h;

   /* Normalized coordinates for the start position of the image */
   norm_x = x / (float)width;
   norm_y = (height - y) / (float)height;

   /* The four vertices of the top-left corner of the image,
    * used as a starting point for all the other sections
    * BL - Bottom Left
    * BR - Bottom Right
    * TL - Top Left
    * TR - Top Right
    */
   V_BL[0] = norm_x;
   V_BL[1] = norm_y;
   V_TL[0] = norm_x;
   V_TL[1] = norm_y + vert_hoff;

   coords.vertices          = 4;
   coords.vertex            = vert_coord;
   coords.tex_coord         = tex_coord;
   coords.lut_tex_coord     = NULL;
   draw.dims                = dims;
   draw.coords              = &coords;
   draw.matrix_data         = mymat;
   draw.pipeline_id         = 0;
   coords.color             = (const float*)(color == NULL ? colors : color);

   draw.texture             = texture;
   draw.pos                 = VIDEO_POS_PACK(0, 0);
   draw.scale_factor        = 1.0f;
   draw.rotation            = 0.0f;

   /* vertex coords are specified bottom-up in this order: BL BR TL TR */
   /* texture coords are specified top-down in this order: BL BR TL TR */

   /* If someone wants to change this to not draw several times, the
    * coordinates will need to be modified because of the triangle strip usage. */

   /* One strip for the nine pieces rather than nine draws of four
    * vertices each. The pieces are a three by three grid: four vertical
    * lines and four horizontal ones in both vertex and texture space,
    * and every piece is the rectangle between two of each. Consecutive
    * pieces are joined by repeating a vertex at each end of the seam,
    * which the rasteriser drops as zero-area - the price of a strip,
    * and cheaper than nine viewport sets and nine blend pairs. */
   {
      unsigned row, col, v  = 0;
      const float *src_col  = (const float*)(color == NULL ? colors : color);
      /* Vertex space: V_* describe the top left piece, so the lines are
       * its edges walked across and down by the middle's size. */
      float vx[4];
      float vy[4];
      float tx[4];
      float ty[4];

      vx[0] = V_BL[0];
      vx[1] = vx[0] + vert_woff;
      vx[2] = vx[1] + vert_scaled_mid_width;
      vx[3] = vx[2] + vert_woff;
      vy[0] = V_TL[1];
      vy[1] = vy[0] - vert_hoff;
      vy[2] = vy[1] - vert_scaled_mid_height;
      vy[3] = vy[2] - vert_hoff;
      tx[0] = 0.0f;
      tx[1] = tex_woff;
      tx[2] = tx[1] + tex_mid_width;
      tx[3] = 1.0f;
      ty[0] = 0.0f;
      ty[1] = tex_hoff;
      ty[2] = ty[1] + tex_mid_height;
      ty[3] = 1.0f;

      for (row = 0; row < 3; row++)
      {
         for (col = 0; col < 3; col++)
         {
            /* BL BR TL TR, the order the strip wants */
            float qx[4];
            float qy[4];
            float qu[4];
            float qv[4];
            unsigned i;

            qx[0] = vx[col];     qx[1] = vx[col + 1];
            qx[2] = vx[col];     qx[3] = vx[col + 1];
            qy[0] = vy[row + 1]; qy[1] = vy[row + 1];
            qy[2] = vy[row];     qy[3] = vy[row];
            qu[0] = tx[col];     qu[1] = tx[col + 1];
            qu[2] = tx[col];     qu[3] = tx[col + 1];
            qv[0] = ty[row + 1]; qv[1] = ty[row + 1];
            qv[2] = ty[row];     qv[3] = ty[row];

            if (v && !dispctx->handles_vertex_strip)
            {
               /* A driver that reads a fixed four vertices gets one
                * piece at a time, as it did before the pieces were
                * joined: it ends in a blit, and a blit has no use for
                * geometry it cannot walk. */
               coords.vertices = v;
               coords.color    = vert_color;
               dispctx->draw(&draw, userdata,
                     video_dims);
               v = 0;
            }
            if (v)
            {
               /* Seam: the piece before ends where this one starts */
               unsigned c;
               vert_coord[v * 2]     = vert_coord[(v - 1) * 2];
               vert_coord[v * 2 + 1] = vert_coord[(v - 1) * 2 + 1];
               tex_coord [v * 2]     = tex_coord [(v - 1) * 2];
               tex_coord [v * 2 + 1] = tex_coord [(v - 1) * 2 + 1];
               for (c = 0; c < 4; c++)
                  vert_color[v * 4 + c] = vert_color[(v - 1) * 4 + c];
               v++;
               vert_coord[v * 2]     = qx[0];
               vert_coord[v * 2 + 1] = qy[0];
               tex_coord [v * 2]     = qu[0];
               tex_coord [v * 2 + 1] = qv[0];
               for (c = 0; c < 4; c++)
                  vert_color[v * 4 + c] = src_col[c];
               v++;
            }

            for (i = 0; i < 4; i++)
            {
               unsigned c;
               vert_coord[v * 2]     = qx[i];
               vert_coord[v * 2 + 1] = qy[i];
               tex_coord [v * 2]     = qu[i];
               tex_coord [v * 2 + 1] = qv[i];
               for (c = 0; c < 4; c++)
                  vert_color[v * 4 + c] = src_col[i * 4 + c];
               v++;
            }
         }
      }

      coords.vertices = v;
      coords.color    = vert_color;
      dispctx->draw(&draw, userdata,
            video_dims);
   }
}

void gfx_display_rotate_z(gfx_display_t *p_disp,
      math_matrix_4x4 *matrix, float cosine, float sine, void *data)
{
   gfx_display_ctx_driver_t *dispctx  = p_disp->dispctx;
   math_matrix_4x4 *b                 = (dispctx->get_default_mvp)
      ? (math_matrix_4x4*)dispctx->get_default_mvp(data)
      : NULL;
   if (b)
   {
      math_matrix_4x4 rot             = {
         {  0.0f,          0.0f,          0.0f,          0.0f ,
            0.0f,          0.0f,          0.0f,          0.0f ,
            0.0f,          0.0f,          1.0f,          0.0f ,
            0.0f,          0.0f,          0.0f,          1.0f }
      };
      MAT_ELEM_4X4(rot, 0, 0)            = cosine;
      MAT_ELEM_4X4(rot, 0, 1)            = -sine;
      MAT_ELEM_4X4(rot, 1, 0)            = sine;
      MAT_ELEM_4X4(rot, 1, 1)            = cosine;
      matrix_4x4_multiply(*matrix, rot, *b);
   }
}

/*
 * Draw a hardware cursor on top of the screen for the mouse.
 */
void gfx_display_draw_cursor(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_dims,
      bool cursor_visible,
      float *color, float cursor_size, uintptr_t texture,
      float x, float y)
{
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   /* What is gathered goes out before this draws */
   gfx_display_flush_as(disp_get_ptr(), GFX_DISPLAY_FLUSH_DRAW);

   if (!dispctx)
      return;

   /* Bail out early if cursor should not be drawn */
   if (!cursor_visible)
      return;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   draw.pos             = VIDEO_POS_PACK(VIDEO_PX(x - (cursor_size / 2)),
         VIDEO_PX((int)VIDEO_SCALE_H(video_dims) - y - (cursor_size / 2)));
   draw.dims            = VIDEO_SCALE_PACK((unsigned)cursor_size,
         (unsigned)cursor_size);
   draw.coords          = &coords;
   draw.matrix_data     = NULL;
   draw.texture         = texture;
   draw.pipeline_id     = 0;
   draw.scale_factor    = 1.0f;
   draw.rotation        = 0.0f;

   if (dispctx->blend_begin)
      dispctx->blend_begin(userdata);
   if (dispctx->draw)
      dispctx->draw(&draw, userdata,
            video_dims);
   if (dispctx->blend_end)
      dispctx->blend_end(userdata);
}

/* Returns the OSK key at a given position */
int gfx_display_osk_ptr_at_pos(void *data, int x, int y,
      unsigned dims)
{
   unsigned i;
   int ptr_width  = VIDEO_SCALE_W(dims) / 11;
   int ptr_height = VIDEO_SCALE_H(dims) / 10;

   if (ptr_width > ptr_height)
      ptr_width = ptr_height;

   for (i = 0; i < 44; i++)
   {
      int line_y    = (int)((i / 11) * VIDEO_SCALE_H(dims) / 10);
      int ptr_x     = (int)(VIDEO_SCALE_W(dims) / 2 - (11 * ptr_width) / 2 + (i % 11) * ptr_width);
      int ptr_y     = (int)(VIDEO_SCALE_H(dims) / 2 + ptr_height * 3 / 2 + line_y - ptr_height);

      if (x > ptr_x && x < ptr_x + ptr_width
       && y > ptr_y && y < ptr_y + ptr_height)
         return i;
   }

   return -1;
}

void gfx_display_draw_keyboard(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_dims,
      uintptr_t hover_texture,
      const font_data_t *font,
      char *grid[], unsigned id,
      unsigned text_color)
{
   unsigned video_width  = VIDEO_SCALE_W(video_dims);
   unsigned video_height = VIDEO_SCALE_H(video_dims);
   unsigned i;
   int ptr_width, ptr_height;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   static const float white[16]    =  {
      1.00f, 1.00f, 1.00f, 1.00f,
      1.00f, 1.00f, 1.00f, 1.00f,
      1.00f, 1.00f, 1.00f, 1.00f,
      1.00f, 1.00f, 1.00f, 1.00f,
   };
   static const float osk_dark[16] =  {
      0.00f, 0.00f, 0.00f, 0.85f,
      0.00f, 0.00f, 0.00f, 0.85f,
      0.00f, 0.00f, 0.00f, 0.85f,
      0.00f, 0.00f, 0.00f, 0.85f,
   };

   /* A native keyboard panel is already covering the screen; drawing
    * the built-in one on top of it gives two keyboards at once. */
   if (input_osk_native_active())
      return;

   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_dims,
         0,
         (int)(video_height / 2),
         VIDEO_SCALE_PACK(video_width, video_height / 2),
         video_dims,
         (float*)osk_dark,
         NULL);

   ptr_width  = video_width  / 11;
   ptr_height = video_height / 10;

   if (ptr_width > ptr_height)
      ptr_width = ptr_height;

   for (i = 0; i < 44; i++)
   {
      int line_y     = (int)((i / 11) * video_height / 10);
      unsigned color = 0xffffffff;

      if (i == id)
      {
         if (dispctx && dispctx->blend_begin)
            dispctx->blend_begin(userdata);

         gfx_display_draw_quad(
           p_disp,
           userdata,
           video_dims,
           (int)(video_width / 2 - (11 * ptr_width) / 2 + (i % 11) * ptr_width),
           (int)(video_height / 2 + ptr_height * 3 / 2 + line_y - ptr_height),
           VIDEO_SCALE_PACK(ptr_width, ptr_height),
           video_dims,
           (float*)white,
           &hover_texture);

         if (dispctx && dispctx->blend_end)
            dispctx->blend_end(userdata);

         color = text_color;
      }

      gfx_display_draw_text(font, grid[i],
            (float)(video_width / 2 - (11 * ptr_width) / 2
               + (i % 11) * ptr_width + ptr_width / 2),
            (float)(video_height / 2 + ptr_height + line_y)
               + font->size / 3.0f,
            video_dims,
            color,
            TEXT_ALIGN_CENTER,
            1.0f,
            false, 0, false);
   }
}

/* ---- Still images through a surface, for callers that own the handle
 *
 * Under threaded video a plain texture load is a round trip: the main
 * thread posts it and waits for the video thread's reply, and a theme's
 * icons are dozens of them. On the main thread with the wrapper running
 * the load is a surface submit instead, queued, and the handle is
 * written into the caller's slot when the upload completes - the caller
 * owns the texture as before. A slot reset or freed while its load is
 * in flight is cancelled first (gfx_display_texture_loads_cancel), and
 * the completion then unloads the texture rather than write it. Without
 * the wrapper, off the main thread, and for an image the plain load
 * treats specially (compressed, 10-bit), nothing changes. */
#ifdef HAVE_THREADS
typedef struct gfx_display_tex_load
{
   struct gfx_display_tex_load *next;
   uintptr_t                   *item;
   /* The pixels, until the upload has read them */
   struct texture_image         img;
   /* img.pixels are freed at the completion; false for a borrowed
    * buffer that outlives the load */
   bool                         owned;
   bool                         cancelled;
} gfx_display_tex_load_t;

/* Main thread only, as surface submits and their completions are */
static gfx_display_tex_load_t *gfx_display_tex_loads;

static void gfx_display_tex_load_release(void *user, gfx_surface_t *s,
      unsigned slot)
{
   gfx_display_tex_load_t  *e = (gfx_display_tex_load_t*)user;
   gfx_display_tex_load_t **p = &gfx_display_tex_loads;
   (void)slot;

   while (*p && *p != e)
      p = &(*p)->next;
   if (*p)
      *p = e->next;

   /* A handle of 0 is a load the driver refused or a teardown */
   if (!e->cancelled && s->handle)
   {
      *e->item  = s->handle;
      s->handle = 0;
   }
   gfx_surface_free(s);
   if (e->owned)
      image_texture_free(&e->img);
   free(e);
}
#endif

void gfx_display_texture_loads_cancel(const void *base, size_t len)
{
#ifdef HAVE_THREADS
   const char             *lo = (const char*)base;
   const char             *hi = lo + len;
   gfx_display_tex_load_t *e;
   for (e = gfx_display_tex_loads; e; e = e->next)
      if ((const char*)e->item >= lo && (const char*)e->item < hi)
         e->cancelled = true;
#else
   (void)base;
   (void)len;
#endif
}

static bool gfx_display_texture_load_ex(struct texture_image *ti,
      enum texture_filter_type filter, uintptr_t *item, bool owned)
{
#ifdef HAVE_THREADS
   if (     ti && item && ti->pixels && !ti->compressed && !ti->pix10
         && VIDEO_SCALE_FITS(ti->width, ti->height)
         && video_driver_thread_wrapper_active()
         && task_is_on_main_thread())
   {
      gfx_surface_t          *s = gfx_surface_new_static(
            VIDEO_SCALE_PACK(ti->width, ti->height), filter);
      gfx_display_tex_load_t *e = s
         ? (gfx_display_tex_load_t*)calloc(1, sizeof(*e)) : NULL;
      if (e)
      {
         enum gfx_surface_submit_result r;
         gfx_surface_src_t src;
         /* A newer load for the same slot replaces one in flight */
         gfx_display_texture_loads_cancel(item, sizeof(*item));
         e->item          = item;
         e->img           = *ti;
         e->owned         = owned;
         /* The entry frees the pixels from its release, which always
          * runs: a display load's surface is only freed there. */
         src.pixels       = ti->pixels;
         src.payload      = NULL;
         src.payload_free = NULL;
         src.pixfmt       = GFX_SURFACE_PIXFMT_8888;
         src.rgba         = ti->supports_rgba;
         r                = gfx_surface_submit_external(s, &src,
               gfx_display_tex_load_release, e);
         if (r == GFX_SURFACE_SUBMIT_QUEUED)
         {
            e->next               = gfx_display_tex_loads;
            gfx_display_tex_loads = e;
            /* The load frees them now; the caller's free skips them */
            if (owned)
               ti->pixels         = NULL;
            return true;
         }
         if (r == GFX_SURFACE_SUBMIT_DONE)
         {
            *item     = s->handle;
            s->handle = 0;
            gfx_surface_free(s);
            free(e);
            return true;
         }
      }
      free(e);
      gfx_surface_free(s);
   }
#endif
   return video_driver_texture_load(ti, filter, item);
}

bool gfx_display_texture_load(struct texture_image *ti,
      enum texture_filter_type filter, uintptr_t *item)
{
   return gfx_display_texture_load_ex(ti, filter, item, true);
}

/* NOTE: Reads image from memory buffer */
bool gfx_display_reset_textures_list_buffer(
        uintptr_t *item, enum texture_filter_type filter_type,
        void* buffer, unsigned buffer_len, enum image_type_enum image_type,
        unsigned *dims)
{
   struct texture_image ti;

   ti.width         = 0;
   ti.height        = 0;
   ti.pixels        = NULL;
   ti.supports_rgba = gfx_surface_wants_rgba();
   ti.pix10         = false;

   if (image_texture_load_buffer(&ti, image_type, buffer, buffer_len))
   {
      if (dims)
         *dims      = VIDEO_SCALE_PACK(ti.width, ti.height);

      /* If the poke interface doesn't support 
         texture load then free and return false */
      if (!gfx_display_texture_load(&ti, filter_type, item))
      {
         image_texture_free(&ti);
         return false;
      }

      image_texture_free(&ti);
      return true;
   }
   return false;
}

/* NOTE: Reads image from file */
bool gfx_display_reset_textures_list(
      const char *texture_path, const char *iconpath,
      uintptr_t *item, enum texture_filter_type filter_type,
      unsigned *dims)
{
   char texpath[PATH_MAX_LENGTH];
   struct texture_image ti;

   ti.width                      = 0;
   ti.height                     = 0;
   ti.pixels                     = NULL;
   ti.supports_rgba              = gfx_surface_wants_rgba();
   ti.pix10                      = false;

   if (!texture_path || !*texture_path)
      return false;

   fill_pathname_join_special(texpath,
         iconpath, texture_path, sizeof(texpath));

   if (!image_texture_load(&ti, texpath))
      return false;

   if (dims)
      *dims = VIDEO_SCALE_PACK(ti.width, ti.height);

   if (!gfx_display_texture_load(&ti,
         filter_type, item))
   {
      image_texture_free(&ti);
      return false;
   }

   image_texture_free(&ti);

   return true;
}

bool gfx_display_reset_icon_texture(
      const char *texture_path,
      uintptr_t *item, enum texture_filter_type filter_type)
{
   struct texture_image ti;

   ti.width                      = 0;
   ti.height                     = 0;
   ti.pixels                     = NULL;
   ti.supports_rgba              = gfx_surface_wants_rgba();
   ti.pix10                      = false;

   if (!texture_path || !*texture_path)
      return false;
   if (!image_texture_load(&ti, texture_path))
      return false;

   if (!gfx_display_texture_load(&ti, filter_type, item))
   {
      image_texture_free(&ti);
      return false;
   }

   image_texture_free(&ti);

   return true;
}

/* -----------------------------------------------------------------------
 * Platform-adaptive icon/texture loading
 *
 * Dispatches to either the synchronous (blocking) or asynchronous
 * (task-queue) icon loading path depending on the platform.
 *
 * Platforms that define GFX_DISPLAY_ICON_LOAD_SYNCHRONOUS get the
 * pre-async behavior: image_texture_load -> video_driver_texture_load
 * in one call, no task queue involvement.  This avoids frame-spread
 * I/O and GL context contention on platforms where the async path is
 * actually slower (e.g. Android behind SAF / fuse storage).
 *
 * To opt a new platform into the synchronous path, add it to the
 * ifdef below.
 * ----------------------------------------------------------------------- */

#if 0 
#define GFX_DISPLAY_ICON_LOAD_SYNCHRONOUS
#endif

/* The mipmap choice, published for the draw-thread texture loads:
 * every main-thread caller of the live read below refreshes it, and
 * main callers run at least per menu rebuild, so the latch tracks
 * the setting to within one texture's filter mode. */
static retro_atomic_int_t gfx_display_mipmap_latch;

enum texture_filter_type gfx_display_texture_filter(void)
{
   settings_t *settings = config_get_ptr();
   int mip              = settings
         && settings->bools.menu_texture_mipmapping;
   retro_atomic_store_relaxed_int(&gfx_display_mipmap_latch, mip);
   return mip ? TEXTURE_FILTER_MIPMAP_LINEAR : TEXTURE_FILTER_LINEAR;
}

/* For texture loads issued off the main thread - badge fetches from
 * the widget appliers, the screenshot widget's iterate - where the
 * live settings must not be read. */
enum texture_filter_type gfx_display_texture_filter_latched(void)
{
   return retro_atomic_load_relaxed_int(&gfx_display_mipmap_latch)
         ? TEXTURE_FILTER_MIPMAP_LINEAR : TEXTURE_FILTER_LINEAR;
}

bool gfx_display_load_icon(
      const char *fullpath,
      bool supports_rgba,
      uintptr_t *target_texture,
      uint64_t generation,
      uint64_t *generation_ptr)
{
#ifdef GFX_DISPLAY_ICON_LOAD_SYNCHRONOUS
   /* Synchronous path - identical to pre-async behavior.
    * Generation counter is irrelevant: the load completes
    * before this function returns, so there is no in-flight
    * callback that could write to a freed pointer. */
   (void)supports_rgba;
   (void)generation;
   (void)generation_ptr;
   return gfx_display_reset_icon_texture(
         fullpath, target_texture,
         gfx_display_texture_filter());
#else
   return task_push_icon_load(
         fullpath, supports_rgba,
         target_texture, generation, generation_ptr);
#endif
}

void gfx_display_deinit_white_texture(void)
{
   gfx_display_texture_loads_cancel(&gfx_white_texture,
         sizeof(gfx_white_texture));
   if (gfx_white_texture)
      video_driver_texture_unload(&gfx_white_texture);
   gfx_white_texture = 0;
}

void gfx_display_init_white_texture(void)
{
   struct texture_image ti;
   static const uint8_t white_data[] = { 0xff, 0xff, 0xff, 0xff };

   ti.width         = 1;
   ti.height        = 1;
   ti.pixels        = (uint32_t*)&white_data;
   ti.compressed    = NULL; /* raw pixels, not a loaded compressed texture */
   ti.pix10         = false; /* 8-bit white; must not be read as 10-bit */
   /* Four 0xff bytes read either way, but the drivers read this field
    * and it is the caller's to set: nothing here fills the struct
    * beforehand, so an unset one is whatever the stack held. */
   ti.supports_rgba = gfx_surface_wants_rgba();

   /* The pixel is static: lent to the load, never freed by it */
   gfx_display_texture_load_ex(&ti,
         TEXTURE_FILTER_NEAREST, &gfx_white_texture, false);
}

void gfx_display_free(void)
{
   gfx_display_t *p_disp       = &dispgfx_st;
   free(gfx_display_mesh_scratch);
   gfx_display_mesh_scratch     = NULL;
   gfx_display_mesh_scratch_cap = 0;

   free(p_disp->batch_mem);
   p_disp->batch_mem           = NULL;
   p_disp->batch_vertex        = NULL;
   p_disp->batch_tex           = NULL;
   p_disp->batch_color         = NULL;
   p_disp->batch_quads         = 0;
   p_disp->blend_on            = false;

   p_disp->flags               = 0;
   p_disp->header_height       = 0;
   p_disp->framebuf_dims       = 0;
   p_disp->framebuf_pitch      = 0;
   p_disp->menu_driver_id      = MENU_DRIVER_ID_UNKNOWN;
   p_disp->dispctx             = NULL;
}

void gfx_display_init(void)
{
   gfx_display_t *p_disp         = &dispgfx_st;

   if (video_driver_has_windowed())
      p_disp->flags             |=  GFX_DISP_FLAG_HAS_WINDOWED;
   else
      p_disp->flags             &= ~GFX_DISP_FLAG_HAS_WINDOWED;
   {
      unsigned i;
      for (i = 0; i < GFX_DISPLAY_STAT_LAST; i++)
         retro_atomic_int_init(&p_disp->stats_pub[i], 0);
   }
}

bool gfx_display_init_first_driver(gfx_display_t *p_disp,
      bool video_is_threaded)
{
   unsigned i;
   const char *video_driver = video_driver_get_ident();

   for (i = 0; gfx_display_ctx_drivers[i]; i++)
   {
      gfx_display_ctx_driver_t *dispctx = gfx_display_ctx_drivers[i];
      enum gfx_display_driver_type type = dispctx->type;
      const char *ident                 = dispctx->ident;
      if (     (type != GFX_VIDEO_DRIVER_GENERIC)
            && (!string_is_equal(video_driver, ident)))
         continue;
      RARCH_LOG("[Display] Found display driver: \"%s\".\n", ident);
      p_disp->dispctx = dispctx;
      return true;
   }
   return false;
}
