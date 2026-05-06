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

#include <stdlib.h>

#include <encodings/utf.h>
#include <lists/string_list.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <string/stdstring.h>

#include <windows.h>
#include <wingdi.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"
#include "../gfx_display.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include "../common/gdi_defines.h"

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

/* AlphaBlend / GradientFill / TransparentBlt are msimg32 imports
 * that became available with Windows 98 / 2000.  Most of the menu
 * draw path requires real per-pixel alpha (Ozone shadows, XMB
 * ribbon transparencies, widget panels): without these calls we
 * fall back to opaque blits, which is the documented Win95 path. */
#define GDI_HAS_ALPHABLEND (_WIN32_WINNT >= 0x0410)

/* GDI font/menu draw operations all happen on memDC with bmp_menu
 * selected.  We fold the SelectObject ping-pong into a pair of
 * inline helpers so the hot path can be read without scanning past
 * setup boilerplate. */

struct bitmap_info
{
   BITMAPINFOHEADER header;
   union
   {
      RGBQUAD          colors;
      DWORD            masks[3];
   } u;
};

/* GDI texture cache.
 *
 * We hold three things per texture:
 *   - data:       BGRA32 pixels in *premultiplied* alpha form.  PNGs
 *                 arrive as straight alpha (BGRA, byte order B,G,R,A
 *                 since supports_rgba is false on this driver), and
 *                 AlphaBlend with AC_SRC_ALPHA requires premultiplied
 *                 source pixels.  We do the multiply once at load.
 *   - bmp:        a DIB section backed by `data` (top-down, 32 bpp).
 *                 Created lazily on first draw so we don't pay the
 *                 cost for textures that never reach the screen.
 *   - has_alpha:  cleared if the source had no transparent pixels;
 *                 lets the draw path use plain StretchBlt (faster
 *                 than AlphaBlend) for icons that are fully opaque. */
typedef struct gdi_texture
{
   HBITMAP bmp;
   HBITMAP bmp_old;
   void *data;            /* Owned BGRA premultiplied pixel buffer. */

   int width;
   int height;
   int active_width;
   int active_height;

   bool has_alpha;
   bool premultiplied;

   enum texture_filter_type type;
} gdi_texture_t;


HDC          win32_gdi_hdc;
static void *dinput_gdi;

/* Forward declarations for static helpers used across the display
 * driver / video driver / font driver sections. */
static void gdi_ensure_brush(gdi_t *gdi, COLORREF color);
static void gdi_release_brush(gdi_t *gdi);
static bool gdi_ensure_menu_surface(gdi_t *gdi,
      unsigned width, unsigned height);
static void gdi_release_menu_surface(gdi_t *gdi);
/* Defined alongside the vtable near the bottom of the file, but
 * gdi_frame calls it through the should_resize path. */
static void gdi_set_viewport(void *data, unsigned vp_width, unsigned vp_height,
      bool force_full, bool allow_rotate);
#ifdef HAVE_OVERLAY
/* Overlay impl lives near the bottom alongside the vtable; gdi_frame
 * composites overlays during Step 10b and gdi_free has to release
 * the DIB sections before the texDC goes away. */
static void gdi_overlay_free(gdi_t *gdi);
static void gdi_overlays_render(gdi_t *gdi,
      unsigned surface_width, unsigned surface_height);
#endif

/* Convert a [0..1] float colour component to a [0..255] byte,
 * clamping defensively (animation interpolators occasionally
 * produce slightly out-of-range values). */
static INLINE uint8_t gdi_float_to_byte(float f)
{
   int v;
   if (f <= 0.0f) return 0;
   if (f >= 1.0f) return 255;
   v = (int)(f * 255.0f + 0.5f);
   if (v < 0)   return 0;
   if (v > 255) return 255;
   return (uint8_t)v;
}

/* Fast divide-by-255 for 8-bit-times-8-bit products.
 *
 * The hot pixel paths (gradient bilinear, RGUI alpha composite,
 * texture-modulated tint, font tinted-glyph composite, load-time
 * texture / overlay premultiply) all need to divide a value of
 * the form (a * b) where both a and b are 0..255 by 255 to get
 * back into 0..255 range.  An integer divide is 20-30 cycles on
 * x86; the shift+add form below is bit-exact equivalent to
 * (uint32_t)x / 255u for x in [0, 255*255 = 65025], i.e. for
 * any product of two 8-bit values.
 *
 * Brute-force verified against integer division for every input
 * in that range; produces the same truncating (round-toward-
 * zero) result the original `/ 255u` produced, so the
 * substitution is byte-identical at every call site rather than
 * being a "close enough" rounding approximation.
 *
 * x is referenced multiple times, so callers should pass an
 * already-evaluated lvalue or temporary; passing a side-effecting
 * expression would evaluate it twice. */
#define GDI_DIV255(x) ((((x) + 1) + ((x) >> 8)) >> 8)

/* Pull the four corners of the per-vertex colour array off a
 * gfx_display_ctx_draw_t.  Caller passes pointers to four uint32_t
 * BGRA values plus a single averaged tint colour (used when we
 * can't actually express a per-vertex gradient on this surface).
 *
 * The colour array layout is the standard 16-float `gfx_white` /
 * `backdrop_orig` form: 4 vertices × {R,G,B,A}.  GFX widgets and
 * the menu drivers use the GL/D3D triangle-strip vertex order,
 * which translates to: 0=BL, 1=BR, 2=TL, 3=TR.  We don't actually
 * rely on positional ordering for the average; we only care about
 * detecting whether the four corners differ enough to bother with
 * a gradient. */
static void gdi_extract_corner_colors(const float *color,
      uint32_t *bl, uint32_t *br, uint32_t *tl, uint32_t *tr,
      uint8_t *avg_r, uint8_t *avg_g, uint8_t *avg_b, uint8_t *avg_a)
{
   uint8_t r0, g0, b0, a0;
   uint8_t r1, g1, b1, a1;
   uint8_t r2, g2, b2, a2;
   uint8_t r3, g3, b3, a3;
   unsigned r_sum, g_sum, b_sum, a_sum;

   if (!color)
   {
      *bl = *br = *tl = *tr = 0xFFFFFFFF;
      *avg_r = *avg_g = *avg_b = *avg_a = 0xFF;
      return;
   }

   r0 = gdi_float_to_byte(color[ 0]); g0 = gdi_float_to_byte(color[ 1]);
   b0 = gdi_float_to_byte(color[ 2]); a0 = gdi_float_to_byte(color[ 3]);
   r1 = gdi_float_to_byte(color[ 4]); g1 = gdi_float_to_byte(color[ 5]);
   b1 = gdi_float_to_byte(color[ 6]); a1 = gdi_float_to_byte(color[ 7]);
   r2 = gdi_float_to_byte(color[ 8]); g2 = gdi_float_to_byte(color[ 9]);
   b2 = gdi_float_to_byte(color[10]); a2 = gdi_float_to_byte(color[11]);
   r3 = gdi_float_to_byte(color[12]); g3 = gdi_float_to_byte(color[13]);
   b3 = gdi_float_to_byte(color[14]); a3 = gdi_float_to_byte(color[15]);

   *bl = ((uint32_t)a0 << 24) | ((uint32_t)r0 << 16) | ((uint32_t)g0 << 8) | b0;
   *br = ((uint32_t)a1 << 24) | ((uint32_t)r1 << 16) | ((uint32_t)g1 << 8) | b1;
   *tl = ((uint32_t)a2 << 24) | ((uint32_t)r2 << 16) | ((uint32_t)g2 << 8) | b2;
   *tr = ((uint32_t)a3 << 24) | ((uint32_t)r3 << 16) | ((uint32_t)g3 << 8) | b3;

   r_sum = r0 + r1 + r2 + r3;
   g_sum = g0 + g1 + g2 + g3;
   b_sum = b0 + b1 + b2 + b3;
   a_sum = a0 + a1 + a2 + a3;

   *avg_r = (uint8_t)(r_sum >> 2);
   *avg_g = (uint8_t)(g_sum >> 2);
   *avg_b = (uint8_t)(b_sum >> 2);
   *avg_a = (uint8_t)(a_sum >> 2);
}

/* Returns true when the four corner colours differ enough that we
 * should attempt a real gradient instead of a single FillRect.
 *
 * Compares the full 32-bit value, not just RGB.  Widget shadows
 * (e.g. the load-content-animation panel's drop shadow) hold the
 * RGB constant at black and animate alpha between top and bottom
 * corners; treating those as solid produces a flat band instead
 * of the intended fade-out.  We need to take the gradient path
 * whenever ANY channel differs across corners. */
static INLINE bool gdi_color_is_gradient(uint32_t bl, uint32_t br,
      uint32_t tl, uint32_t tr)
{
   if (bl != tl || br != tr || bl != br || tl != tr)
      return true;
   return false;
}

/* Cached brush management.  CreateSolidBrush + DeleteObject every
 * draw call would dwarf the actual blit cost, so we keep a single
 * brush and recreate it only when the colour changes. */
static void gdi_ensure_brush(gdi_t *gdi, COLORREF color)
{
   if (gdi->brush_color_cached_valid && gdi->brush_color_cached == color
         && gdi->brush_cached)
      return;
   if (gdi->brush_cached)
      DeleteObject(gdi->brush_cached);
   gdi->brush_cached              = CreateSolidBrush(color);
   gdi->brush_color_cached        = color;
   gdi->brush_color_cached_valid  = true;
}

static void gdi_release_brush(gdi_t *gdi)
{
   if (gdi->brush_cached)
      DeleteObject(gdi->brush_cached);
   gdi->brush_cached             = NULL;
   gdi->brush_color_cached_valid = false;
}

/* (Re)create the menu compositing surface (bmp_menu) at the given
 * size.  We use a top-down 32-bit BGRA DIB section: GDI gives us a
 * raw uint32_t* into the pixels (gdi->menu_pixels), which lets us
 * do scanline operations directly when needed (e.g. clearing) and
 * still works as an HBITMAP for AlphaBlend / BitBlt.  Returns false
 * on allocation failure (caller should fall back gracefully). */
static bool gdi_ensure_menu_surface(gdi_t *gdi,
      unsigned width, unsigned height)
{
   BITMAPINFO bmi;
   void *pixels = NULL;

   if (!gdi || !gdi->memDC || width == 0 || height == 0)
      return false;

   if (     gdi->bmp_menu
         && gdi->menu_surface_width  == width
         && gdi->menu_surface_height == height)
      return true;

   /* Tear down any prior surface before creating the new one.  The
    * menu DIB is selected into memDC during draws, so we must
    * deselect first if it's currently the selected object. */
   gdi_release_menu_surface(gdi);

   memset(&bmi, 0, sizeof(bmi));
   bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth       = (LONG)width;
   bmi.bmiHeader.biHeight      = -(LONG)height; /* top-down */
   bmi.bmiHeader.biPlanes      = 1;
   bmi.bmiHeader.biBitCount    = 32;
   bmi.bmiHeader.biCompression = BI_RGB;

   gdi->bmp_menu = CreateDIBSection(gdi->memDC, &bmi, DIB_RGB_COLORS,
         &pixels, NULL, 0);

   if (!gdi->bmp_menu || !pixels)
   {
      gdi->bmp_menu          = NULL;
      gdi->menu_pixels       = NULL;
      gdi->menu_surface_width  = 0;
      gdi->menu_surface_height = 0;
      return false;
   }

   gdi->menu_pixels         = (uint32_t*)pixels;
   gdi->menu_surface_width  = width;
   gdi->menu_surface_height = height;
   return true;
}

static void gdi_release_menu_surface(gdi_t *gdi)
{
   if (!gdi)
      return;
   if (gdi->bmp_menu_old && gdi->memDC)
   {
      SelectObject(gdi->memDC, gdi->bmp_menu_old);
      gdi->bmp_menu_old = NULL;
   }
   if (gdi->bmp_menu)
      DeleteObject(gdi->bmp_menu);
   gdi->bmp_menu          = NULL;
   gdi->menu_pixels       = NULL;
   gdi->menu_surface_width  = 0;
   gdi->menu_surface_height = 0;
}

/* Allocate (or reuse) a BGRA32 scratch DIB section of at least
 * w x h pixels.  Used by the gradient + texture-modulated paths.
 * Grows the DIB when the request exceeds the current capacity;
 * smaller requests reuse the existing buffer at its current size
 * (the caller writes into the top-left w x h sub-rect and the
 * downstream AlphaBlend uses that sub-rect as the source extent,
 * so leftover stale pixels in the unused tail don't matter).
 *
 * Returns false if allocation failed, in which case the caller
 * should bail out of the draw entirely.
 *
 * The DIB is BI_RGB top-down so the pixel pointer addresses row 0
 * first, matching the draw paths' iteration order. */
static bool gdi_ensure_scratch_quad(gdi_t *gdi, unsigned w, unsigned h)
{
   BITMAPINFO bmi;
   void      *pixels = NULL;

   if (!gdi || !gdi->memDC || !w || !h)
      return false;

   /* Existing allocation big enough? */
   if (     gdi->scratch_quad_bmp
         && gdi->scratch_quad_pixels
         && gdi->scratch_quad_w >= w
         && gdi->scratch_quad_h >= h)
      return true;

   /* Grow.  Take the max of the current cap and the new request so
    * we don't shrink any axis (one big draw shouldn't force the next
    * smaller draw to reallocate). */
   if (gdi->scratch_quad_w > w)
      w = gdi->scratch_quad_w;
   if (gdi->scratch_quad_h > h)
      h = gdi->scratch_quad_h;

   if (gdi->scratch_quad_bmp)
      DeleteObject(gdi->scratch_quad_bmp);
   gdi->scratch_quad_bmp    = NULL;
   gdi->scratch_quad_pixels = NULL;
   gdi->scratch_quad_w      = 0;
   gdi->scratch_quad_h      = 0;

   memset(&bmi, 0, sizeof(bmi));
   bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth       = (LONG)w;
   bmi.bmiHeader.biHeight      = -(LONG)h;
   bmi.bmiHeader.biPlanes      = 1;
   bmi.bmiHeader.biBitCount    = 32;
   bmi.bmiHeader.biCompression = BI_RGB;

   gdi->scratch_quad_bmp = CreateDIBSection(gdi->memDC, &bmi,
         DIB_RGB_COLORS, &pixels, NULL, 0);
   if (!gdi->scratch_quad_bmp || !pixels)
   {
      if (gdi->scratch_quad_bmp)
         DeleteObject(gdi->scratch_quad_bmp);
      gdi->scratch_quad_bmp    = NULL;
      gdi->scratch_quad_pixels = NULL;
      return false;
   }

   gdi->scratch_quad_pixels = (uint32_t*)pixels;
   gdi->scratch_quad_w      = w;
   gdi->scratch_quad_h      = h;
   return true;
}

/* Same idea as gdi_ensure_scratch_quad, but for the RGUI
 * alpha-composite path.  Kept as a separate slot so a frame that
 * draws gradient quads AND composites RGUI doesn't thrash one
 * shared DIB back and forth between sizes. */
static bool gdi_ensure_scratch_rgui(gdi_t *gdi, unsigned w, unsigned h)
{
   BITMAPINFO bmi;
   void      *pixels = NULL;

   if (!gdi || !gdi->memDC || !w || !h)
      return false;

   if (     gdi->scratch_rgui_bmp
         && gdi->scratch_rgui_pixels
         && gdi->scratch_rgui_w >= w
         && gdi->scratch_rgui_h >= h)
      return true;

   if (gdi->scratch_rgui_w > w)
      w = gdi->scratch_rgui_w;
   if (gdi->scratch_rgui_h > h)
      h = gdi->scratch_rgui_h;

   if (gdi->scratch_rgui_bmp)
      DeleteObject(gdi->scratch_rgui_bmp);
   gdi->scratch_rgui_bmp    = NULL;
   gdi->scratch_rgui_pixels = NULL;
   gdi->scratch_rgui_w      = 0;
   gdi->scratch_rgui_h      = 0;

   memset(&bmi, 0, sizeof(bmi));
   bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth       = (LONG)w;
   bmi.bmiHeader.biHeight      = -(LONG)h;
   bmi.bmiHeader.biPlanes      = 1;
   bmi.bmiHeader.biBitCount    = 32;
   bmi.bmiHeader.biCompression = BI_RGB;

   gdi->scratch_rgui_bmp = CreateDIBSection(gdi->memDC, &bmi,
         DIB_RGB_COLORS, &pixels, NULL, 0);
   if (!gdi->scratch_rgui_bmp || !pixels)
   {
      if (gdi->scratch_rgui_bmp)
         DeleteObject(gdi->scratch_rgui_bmp);
      gdi->scratch_rgui_bmp    = NULL;
      gdi->scratch_rgui_pixels = NULL;
      return false;
   }

   gdi->scratch_rgui_pixels = (uint32_t*)pixels;
   gdi->scratch_rgui_w      = w;
   gdi->scratch_rgui_h      = h;
   return true;
}

/* Allocate the fixed 1x1 BGRA scratch DIB.  Used by the
 * translucent-solid-quad path: one premultiplied pixel,
 * AlphaBlend stretches it across the destination rect.  Lazy: if
 * the caller hits the path before init has set this up, fall
 * back to a CreateDIBSection here.  We could move this to
 * gdi_init unconditionally but lazy initialisation costs nothing
 * and is robust to any future callers that arise before init
 * has run. */
static bool gdi_ensure_scratch_1x1(gdi_t *gdi)
{
   BITMAPINFO bmi;
   void      *pixels = NULL;

   if (!gdi || !gdi->memDC)
      return false;
   if (gdi->scratch_1x1_bmp && gdi->scratch_1x1_pixels)
      return true;

   memset(&bmi, 0, sizeof(bmi));
   bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth       = 1;
   bmi.bmiHeader.biHeight      = -1;
   bmi.bmiHeader.biPlanes      = 1;
   bmi.bmiHeader.biBitCount    = 32;
   bmi.bmiHeader.biCompression = BI_RGB;

   gdi->scratch_1x1_bmp = CreateDIBSection(gdi->memDC, &bmi,
         DIB_RGB_COLORS, &pixels, NULL, 0);
   if (!gdi->scratch_1x1_bmp || !pixels)
   {
      if (gdi->scratch_1x1_bmp)
         DeleteObject(gdi->scratch_1x1_bmp);
      gdi->scratch_1x1_bmp    = NULL;
      gdi->scratch_1x1_pixels = NULL;
      return false;
   }

   gdi->scratch_1x1_pixels = (uint32_t*)pixels;
   return true;
}

/* Free all cached scratch DIBs.  Called from gdi_free. */
static void gdi_release_scratch(gdi_t *gdi)
{
   if (!gdi)
      return;
   if (gdi->scratch_1x1_bmp)
      DeleteObject(gdi->scratch_1x1_bmp);
   if (gdi->scratch_quad_bmp)
      DeleteObject(gdi->scratch_quad_bmp);
   if (gdi->scratch_rgui_bmp)
      DeleteObject(gdi->scratch_rgui_bmp);
   gdi->scratch_1x1_bmp     = NULL;
   gdi->scratch_1x1_pixels  = NULL;
   gdi->scratch_quad_bmp    = NULL;
   gdi->scratch_quad_pixels = NULL;
   gdi->scratch_quad_w      = 0;
   gdi->scratch_quad_h      = 0;
   gdi->scratch_rgui_bmp    = NULL;
   gdi->scratch_rgui_pixels = NULL;
   gdi->scratch_rgui_w      = 0;
   gdi->scratch_rgui_h      = 0;
}

/* Clear the menu surface to a solid (premultiplied) BGRA value.
 *
 * Implemented via FillRect (rather than direct pixel write) so the
 * clear participates in GDI's DC pipeline like every other draw on
 * bmp_menu.  Direct writes to a DIB section that's selected into
 * a DC require GdiFlush boundaries to avoid races with GDI's
 * internal batch queue, and getting that wrong shows up as random
 * draws "missing" between frames — exactly the symptom of a
 * pre-Win98-era trick we don't actually need.  The performance
 * argument for direct writes (~4x for big clears) doesn't matter
 * once the clear is one operation per frame on a window-sized
 * surface.  bmp_menu must already be selected into gdi->memDC. */
static void gdi_menu_surface_clear(gdi_t *gdi, uint8_t r, uint8_t g, uint8_t b)
{
   RECT rect;
   if (!gdi || !gdi->memDC || !gdi->bmp_menu)
      return;
   rect.left   = 0;
   rect.top    = 0;
   rect.right  = (LONG)gdi->menu_surface_width;
   rect.bottom = (LONG)gdi->menu_surface_height;
   gdi_ensure_brush(gdi, RGB(r, g, b));
   if (gdi->brush_cached)
      FillRect(gdi->memDC, &rect, gdi->brush_cached);
}

/* StretchDIBits-upscale a core frame into bmp_menu, scaling up to
 * the menu compositing surface size.  Used as a "background
 * underlay" pass for textured menus over a running game so the menu
 * (drawn afterwards by menu_driver_frame, with its own alpha)
 * composites against the actual game pixels rather than solid black.
 *
 * bmp_menu must already be selected into gdi->memDC.  Caller must
 * have validated that frame_data is non-NULL and frame_w/frame_h
 * are reasonable (> 4, i.e. not the placeholder).  The 16-bit
 * format detection here mirrors Step 9's logic for the bmp path —
 * RGB565 for the core, RGB555 fallback for Win98 (no
 * BI_BITFIELDS-with-RGB444 support).  This helper is only ever
 * called with the *core* frame, not RGUI's menu_frame — RGUI
 * pixels are RGBA4444 and need an alpha-blend composite, handled
 * separately in Step 9. */
static void gdi_upload_core_frame_to_menu(gdi_t *gdi,
      const void *frame_data, unsigned frame_w, unsigned frame_h,
      unsigned frame_pitch, unsigned frame_bits)
{
   struct bitmap_info info;
   const void *src = frame_data;

   if (!gdi || !gdi->memDC || !gdi->bmp_menu || !frame_data)
      return;
   if (frame_w == 0 || frame_h == 0 || frame_bits == 0)
      return;

   memset(&info, 0, sizeof(info));
   info.header.biSize         = sizeof(BITMAPINFOHEADER);
   info.header.biWidth        = frame_pitch / (frame_bits / 8);
   info.header.biHeight       = -(int)frame_h;
   info.header.biPlanes       = 1;
   info.header.biBitCount     = frame_bits;
   info.header.biCompression  = 0;

   if (frame_bits == 16)
   {
      if (gdi->lte_win98 && gdi->temp_buf)
      {
         /* Win98 and below: legacy fallback that mirrors what the
          * non-textured path has always done — the bit pattern
          * matches the original gdi_gfx code in Step 9.  This
          * isn't a clean RGB565→RGB555 conversion and can produce
          * subtly wrong colours on cores that emit RGB565, but
          * preserving the existing behaviour avoids divergence
          * between the two upload paths. */
         unsigned x, y;
         for (y = 0; y < frame_h; y++)
         {
            for (x = 0; x < frame_w; x++)
            {
               unsigned short pixel =
                  ((const unsigned short*)frame_data)[frame_w * y + x];
               gdi->temp_buf[frame_w * y + x] =
                    (pixel & 0xF000) >> 1
                  | (pixel & 0x0F00) >> 2
                  | (pixel & 0x00F0) >> 3;
            }
         }
         src = gdi->temp_buf;
         info.header.biCompression = BI_RGB;
      }
      else
      {
         info.header.biCompression = BI_BITFIELDS;
         /* Core always emits RGB565 here (RGUI's RGB444 frames go
          * through the non-textured legacy path). */
         info.u.masks[0] = 0xF800;
         info.u.masks[1] = 0x07E0;
         info.u.masks[2] = 0x001F;
      }
   }
   else
      info.header.biCompression = BI_RGB;

   /* Destination rect is the viewport (game area), not the full
    * bmp_menu surface.  The surrounding pillarbox/letterbox area
    * was already cleared to black in Step 4, so we leave it
    * untouched and the bars appear automatically. */
   StretchDIBits(gdi->memDC,
         gdi->vp.x, gdi->vp.y, gdi->vp.width, gdi->vp.height,
         0, 0, frame_w, frame_h,
         src, (BITMAPINFO*)&info, DIB_RGB_COLORS, SRCCOPY);
}

#ifdef GDI_HAS_ALPHABLEND
/* Composite RGUI's RGBA4444 menu_frame onto bmp_menu using
 * source-over alpha blending.
 *
 * RGUI's frame buffer mixes opaque pixels (icons, text, thumbnail
 * panels) with semi-transparent pixels (the chequer background
 * when menu_rgui_transparency is enabled — the platform pixel
 * format function in rgui.c routes gdi to argb32_to_rgba4444,
 * which returns transparency_supported = true).  StretchDIBits
 * with SRCCOPY would discard the alpha and overwrite whatever
 * Step 4b put underneath; this helper preserves it.
 *
 * Strategy: convert the 16-bit RGBA4444 source into a 32-bit BGRA
 * premultiplied scratch DIB section, then AlphaBlend-stretch onto
 * bmp_menu over the existing pixels.  RGUI's frame is small
 * (typically 256x192 to 512x480), so the per-pixel conversion is
 * cheap.  The scratch DIB is allocated per call — caching it on
 * gdi_t is a possible future optimisation but not necessary for
 * correctness, and RGUI doesn't repaint every frame at high
 * resolutions where the cost would matter.
 *
 * Falls back to opaque copy on Win95 (no AlphaBlend) — see the
 * non-GDI_HAS_ALPHABLEND branch below.  Caller must have
 * bmp_menu selected into gdi->memDC. */
static void gdi_blit_rgui_alpha(gdi_t *gdi,
      const void *frame_data, unsigned frame_w, unsigned frame_h,
      unsigned dst_x, unsigned dst_y,
      unsigned dst_w, unsigned dst_h)
{
   HBITMAP          scratch_old;
   BLENDFUNCTION    blend;
   const uint16_t  *src;
   uint32_t        *dst;
   unsigned         x, y;
   unsigned         stride;

   if (!gdi || !gdi->memDC || !frame_data || !frame_w || !frame_h)
      return;

   /* Ensure the cached scratch DIB is big enough.  Stride is the
    * DIB's allocated row width in pixels, which may be larger than
    * frame_w if a previous frame requested a wider RGUI surface
    * (we never shrink).  We write into the top-left frame_w x
    * frame_h sub-rect; the AlphaBlend source extent below is also
    * frame_w x frame_h, so leftover pixels in the unused tail of
    * the DIB are ignored. */
   if (!gdi_ensure_scratch_rgui(gdi, frame_w, frame_h))
      return;
   stride = gdi->scratch_rgui_w;

   /* RGBA4444 → BGRA32 premultiplied.  Layout from
    * argb32_to_rgba4444:
    *   bits 12-15: R   bits 8-11: G   bits 4-7: B   bits 0-3: A
    *
    * Premultiply means each colour channel times alpha / 255.  In
    * 4-bit-alpha space that's roughly (c4 * a4 * 17) / 255 where
    * c4 / a4 are 0..15 and 17 expands 4-bit to 8-bit.  We compute
    * everything at 8-bit precision after the expand. */
   src = (const uint16_t *)frame_data;
   dst = gdi->scratch_rgui_pixels;
   for (y = 0; y < frame_h; y++)
   {
      for (x = 0; x < frame_w; x++)
      {
         uint16_t p = src[y * frame_w + x];
         unsigned r = ((p >> 12) & 0x0F) * 17; /* 0..255 */
         unsigned g = ((p >>  8) & 0x0F) * 17;
         unsigned b = ((p >>  4) & 0x0F) * 17;
         unsigned a = ( p        & 0x0F) * 17;

         if (a == 0)
         {
            dst[y * stride + x] = 0;
            continue;
         }
         if (a < 255)
         {
            r = (r * a + 127) / 255;
            g = (g * a + 127) / 255;
            b = (b * a + 127) / 255;
         }
         dst[y * stride + x] = (a << 24) | (r << 16) | (g << 8) | b;
      }
   }

   if (!gdi->texDC)
      gdi->texDC = CreateCompatibleDC(gdi->winDC);
   scratch_old = (HBITMAP)SelectObject(gdi->texDC, gdi->scratch_rgui_bmp);

   blend.BlendOp             = AC_SRC_OVER;
   blend.BlendFlags          = 0;
   blend.SourceConstantAlpha = 255;
   blend.AlphaFormat         = AC_SRC_ALPHA;

   AlphaBlend(gdi->memDC,
         dst_x, dst_y, dst_w, dst_h,
         gdi->texDC, 0, 0, frame_w, frame_h,
         blend);

   SelectObject(gdi->texDC, scratch_old);
}
#endif /* GDI_HAS_ALPHABLEND */

/*
 * DISPLAY DRIVER
 *
 * gfx_display_ctx_gdi is the bridge between the menu/widget code's
 * abstract "draw a quad" world and GDI's HDC + HBITMAP world.
 *
 * The menu drivers (XMB, Ozone, MaterialUI) and gfx_widgets issue
 * draw calls in three flavours:
 *
 *   1. Solid-colour rectangle (texture is gfx_white_texture, a 1x1
 *      white pixel).  By far the most common: backgrounds, panels,
 *      separator lines.  Per-vertex colour from coords->color.
 *      We render this with FillRect + cached SolidBrush.
 *
 *   2. Per-vertex colour gradient on the white texture (XMB ribbon
 *      band, Ozone sidebar fade).  We detect this and route to
 *      GradientFill (Win98+) for 2-stop horizontal/vertical
 *      gradients, or fall back to the average colour fill.
 *
 *   3. Real texture (icon, thumbnail, header art).  AlphaBlend
 *      from a per-texture DIB onto the menu surface, with
 *      SourceConstantAlpha modulated by the average vertex alpha.
 *
 * All output goes to gdi->bmp_menu (a DIB section) which is the
 * window-sized back buffer.  gdi_frame BitBlt's it to the window
 * at end-of-frame, after all menu/widget draws have landed.
 */

/* Default vertex / tex_coord arrays for the full-screen quad in
 * the conventions every other backend uses:
 *
 *   - vertex:    normalised 0..1, BOTTOM-UP, vertex order BL/BR/TL/TR.
 *                BL=(0,0) BR=(1,0) TL=(0,1) TR=(1,1).
 *   - tex_coord: normalised 0..1, TOP-DOWN, vertex order BL/BR/TL/TR.
 *                BL=(0,1) BR=(1,1) TL=(0,0) TR=(1,0).
 *
 * These get pulled in by gfx_display_draw_bg() (used by MaterialUI
 * and others for the menu-background fill) when the caller hasn't
 * supplied its own arrays.  Without correct defaults, our
 * custom-geometry path in gfx_display_gdi_draw computes a degenerate
 * (0,0,0,0) bounding box and silently drops the draw — the symptom
 * is MaterialUI's background showing as the bmp_menu clear colour
 * (black) wherever no other quad covers it. */
static const float gdi_default_vertices[8] = {
   0.0f, 0.0f,   /* BL */
   1.0f, 0.0f,   /* BR */
   0.0f, 1.0f,   /* TL */
   1.0f, 1.0f    /* TR */
};

static const float gdi_default_tex_coords[8] = {
   0.0f, 1.0f,   /* BL */
   1.0f, 1.0f,   /* BR */
   0.0f, 0.0f,   /* TL */
   1.0f, 0.0f    /* TR */
};

static const float *gfx_display_gdi_get_default_vertices(void)
{
   return &gdi_default_vertices[0];
}

static const float *gfx_display_gdi_get_default_tex_coords(void)
{
   return &gdi_default_tex_coords[0];
}

/* AlphaBlend / GradientFill state setup is per-call (no persistent
 * blend-state object on GDI), so blend_begin / blend_end are no-ops.
 * We keep them in the vtable for symmetry with the other drivers
 * and so menu code that calls them unconditionally doesn't choke
 * on a NULL function pointer. */
static void gfx_display_gdi_blend_begin(void *data) { (void)data; }
static void gfx_display_gdi_blend_end  (void *data) { (void)data; }

/* GDI clip regions don't stack natively: SelectClipRgn replaces the
 * current region, it doesn't push.  We use SaveDC / RestoreDC, which
 * the SDK explicitly says preserves the clip region.  Coordinates
 * arrive in the same conventions as the rest of the menu draw path:
 * (x,y) is the top-left of the scissor rect, with y measured from
 * the top of the screen. */
static void gfx_display_gdi_scissor_begin(void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   gdi_t *gdi = (gdi_t*)data;
   HRGN rgn;

   if (!gdi || !gdi->memDC)
      return;

   /* Clamp degenerate scissors to a 1x1 rect.  An empty clip region
    * would suppress *all* subsequent draws, which is nearly always
    * a menu animation glitch (e.g. a pre-animation 0-width strip)
    * rather than a deliberate hide-everything intent. */
   if (width == 0)  width  = 1;
   if (height == 0) height = 1;

   gdi->scissor_saved = SaveDC(gdi->memDC);
   rgn = CreateRectRgn(x, y, x + (int)width, y + (int)height);
   if (rgn)
   {
      SelectClipRgn(gdi->memDC, rgn);
      DeleteObject(rgn);
   }
   gdi->scissor_active = true;
}

static void gfx_display_gdi_scissor_end(void *data,
      unsigned video_width, unsigned video_height)
{
   gdi_t *gdi = (gdi_t*)data;
   if (!gdi || !gdi->memDC)
      return;

   if (gdi->scissor_active && gdi->scissor_saved)
      RestoreDC(gdi->memDC, gdi->scissor_saved);
   else
      SelectClipRgn(gdi->memDC, NULL);

   gdi->scissor_active = false;
   gdi->scissor_saved  = 0;
}

/* Pre-multiply a per-call source alpha into a texture's pixel data.
 * Called for one-shot tinted icon draws; we copy texture->data into
 * a scratch buffer, multiply, blit, then discard.  Only used when
 * the caller wants RGB tinting; pure alpha modulation goes through
 * the cheaper SourceConstantAlpha path on AlphaBlend. */
#if GDI_HAS_ALPHABLEND
/* Source rect (src_x/src_y/src_w/src_h) selects a sub-region within
 * the texture to sample from.  Pass src_w=0 or src_h=0 to mean "use
 * the whole texture" — convenient default for plain-quad callers
 * that don't have tex_coord. */
static void gdi_blit_texture_modulated(
      gdi_t *gdi, gdi_texture_t *texture,
      int dst_x, int dst_y, unsigned dst_w, unsigned dst_h,
      int src_x, int src_y, unsigned src_w, unsigned src_h,
      uint8_t mod_r, uint8_t mod_g, uint8_t mod_b, uint8_t mod_a)
{
   HBITMAP scratch_old;
   const uint32_t *src;
   uint32_t *dst;
   size_t   y_idx, x_idx;
   BLENDFUNCTION blend;
   unsigned stride;

   if (src_w == 0 || src_h == 0)
   {
      src_x = 0;
      src_y = 0;
      src_w = (unsigned)texture->width;
      src_h = (unsigned)texture->height;
   }

   /* Fast path: no RGB tint, only an alpha multiplier.  The texture
    * is already premultiplied with its own alpha, so we just scale
    * the SourceConstantAlpha and call AlphaBlend directly off the
    * cached per-texture DIB. */
   if (mod_r == 255 && mod_g == 255 && mod_b == 255)
   {
      if (!gdi->texDC)
         gdi->texDC = CreateCompatibleDC(gdi->winDC);

      texture->bmp_old = (HBITMAP)SelectObject(gdi->texDC, texture->bmp);

      blend.BlendOp             = AC_SRC_OVER;
      blend.BlendFlags          = 0;
      blend.SourceConstantAlpha = mod_a;
      blend.AlphaFormat         = texture->has_alpha ? AC_SRC_ALPHA : 0;

      AlphaBlend(gdi->memDC,
            dst_x, dst_y, dst_w, dst_h,
            gdi->texDC, src_x, src_y, src_w, src_h,
            blend);

      SelectObject(gdi->texDC, texture->bmp_old);
      texture->bmp_old = NULL;
      return;
   }

   /* Slow path: premultiplied copy with RGB tint baked in.  We only
    * tint the sub-rect we actually intend to sample from, which
    * keeps this path's cost proportional to the slice size rather
    * than the full texture.  The scratch DIB is cached on gdi_t
    * (gdi->scratch_quad_*) and grow-only across calls; stride is
    * the cached DIB's row width which may exceed src_w. */
   if (!gdi_ensure_scratch_quad(gdi, src_w, src_h))
      return;
   stride = gdi->scratch_quad_w;

   src   = (const uint32_t*)texture->data;
   dst   = gdi->scratch_quad_pixels;

   /* Source pixels are already premultiplied with their own alpha
    * (gdi_load_texture did the multiply once at load).  Apply the
    * tint colour: each channel gets multiplied by mod_X * mod_a.
    * Result stays premultiplied because the alpha factor (a*mod_a)
    * is folded into every channel uniformly. */
   for (y_idx = 0; y_idx < src_h; y_idx++)
   {
      const uint32_t *src_row = src
         + ((size_t)src_y + y_idx) * (size_t)texture->width
         + (size_t)src_x;
      uint32_t       *dst_row = dst + y_idx * stride;
      for (x_idx = 0; x_idx < src_w; x_idx++)
      {
         uint32_t s  = src_row[x_idx];
         uint8_t  sa = (uint8_t)((s >> 24) & 0xFF);
         uint8_t  sr = (uint8_t)((s >> 16) & 0xFF);
         uint8_t  sg = (uint8_t)((s >>  8) & 0xFF);
         uint8_t  sb = (uint8_t)( s        & 0xFF);
         uint32_t out_a = GDI_DIV255((uint32_t)sa * mod_a);
         /* The /255²/ divides for out_r/g/b are deliberately left
          * unchanged — collapsing to two GDI_DIV255 calls would
          * introduce rounding error of up to 1 LSB compared to
          * the single 16-bit divide, and the cost of one divide
          * per channel here isn't worth a visible drift in
          * tinted-icon pixels. */
         uint32_t out_r = ((uint32_t)sr * mod_r * mod_a) / (255u * 255u);
         uint32_t out_g = ((uint32_t)sg * mod_g * mod_a) / (255u * 255u);
         uint32_t out_b = ((uint32_t)sb * mod_b * mod_a) / (255u * 255u);
         dst_row[x_idx] = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
      }
   }

   if (!gdi->texDC)
      gdi->texDC = CreateCompatibleDC(gdi->winDC);
   scratch_old = (HBITMAP)SelectObject(gdi->texDC, gdi->scratch_quad_bmp);

   blend.BlendOp             = AC_SRC_OVER;
   blend.BlendFlags          = 0;
   blend.SourceConstantAlpha = 255;
   blend.AlphaFormat         = AC_SRC_ALPHA;

   AlphaBlend(gdi->memDC,
         dst_x, dst_y, dst_w, dst_h,
         gdi->texDC, 0, 0, src_w, src_h,
         blend);

   SelectObject(gdi->texDC, scratch_old);
}
#endif /* GDI_HAS_ALPHABLEND */

/* Lazily build a per-texture DIB section from texture->data.
 * Called the first time a given texture is drawn; the DIB is
 * cached on the gdi_texture_t so subsequent frames just bind it.
 * Returns false if the texture has no usable pixel data. */
static bool gdi_texture_realize(gdi_t *gdi, gdi_texture_t *texture)
{
   BITMAPINFO bmi;
   void *pixels = NULL;
   HBITMAP bmp;

   if (!texture || !texture->data)
      return false;
   if (texture->bmp)
      return true;
   if (!gdi || !gdi->memDC)
      return false;

   memset(&bmi, 0, sizeof(bmi));
   bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth       = texture->width;
   bmi.bmiHeader.biHeight      = -texture->height;
   bmi.bmiHeader.biPlanes      = 1;
   bmi.bmiHeader.biBitCount    = 32;
   bmi.bmiHeader.biCompression = BI_RGB;

   bmp = CreateDIBSection(gdi->memDC, &bmi, DIB_RGB_COLORS,
         &pixels, NULL, 0);
   if (!bmp || !pixels)
   {
      if (bmp)
         DeleteObject(bmp);
      return false;
   }

   /* texture->data was already premultiplied at load time
    * (gdi_load_texture).  Just copy it into the DIB-backed memory. */
   memcpy(pixels, texture->data,
         (size_t)texture->width * (size_t)texture->height * sizeof(uint32_t));

   texture->bmp = bmp;
   return true;
}

static void gfx_display_gdi_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   gdi_t        *gdi     = (gdi_t*)data;
   gdi_texture_t *texture = NULL;
   uint32_t bl, br, tl, tr;
   uint8_t  avg_r, avg_g, avg_b, avg_a;
   int      dst_x, dst_y;
   unsigned dst_w, dst_h;
   unsigned target_w, target_h;
   bool     is_white_texture;
   bool     have_vertex_coords;
   bool     have_tex_coords;
   /* Sub-rect within the source texture, in source-texture pixel
    * coordinates.  Used only when have_tex_coords is true; otherwise
    * we sample from the full texture. */
   int      src_x = 0, src_y = 0;
   unsigned src_w = 0, src_h = 0;
   const float *color;

   if (!gdi || !draw || !draw->coords)
      return;
   if (!gdi->memDC)
      return;

   /* Pick the target dimensions for clip-rejection.  Caller (menu /
    * widgets) has already arranged for the right bitmap to be
    * selected into memDC; we just need to know the bounds.  Three
    * cases:
    *   - Textured menu active → bmp_menu is the target (window-sized)
    *   - Otherwise → bmp is the target (menu/core size from
    *     bmp_width/bmp_height)
    *   - If neither is set yet, fall back to the video_width /
    *     video_height the caller passed us. */
   if (gdi->menu_textured_active && gdi->bmp_menu)
   {
      target_w = gdi->menu_surface_width;
      target_h = gdi->menu_surface_height;
   }
   else if (gdi->bmp_width && gdi->bmp_height)
   {
      target_w = gdi->bmp_width;
      target_h = gdi->bmp_height;
   }
   else
   {
      target_w = video_width;
      target_h = video_height;
   }

   if (!target_w || !target_h)
      return;

   color = draw->coords->color;
   gdi_extract_corner_colors(color, &bl, &br, &tl, &tr,
         &avg_r, &avg_g, &avg_b, &avg_a);

   /* Two coordinate-input conventions in this vtable:
    *
    *   1. Plain quad:  coords->vertex is NULL, geometry comes from
    *      draw->x / draw->y / draw->width / draw->height.  draw->y
    *      is "Y from bottom" because gfx_display_draw_quad flips it
    *      to match GL's bottom-up convention.  This is what
    *      menu/widget code uses for the simple-rect path.
    *
    *   2. Custom geometry:  coords->vertex points to 4 (x,y) pairs
    *      in NORMALISED 0..1 space, bottom-up.  draw->x/y/width/h
    *      are unrelated junk in this case (typically 0/0/full
    *      target dim).  Used by gfx_display_draw_texture_slice
    *      (9-patch), among others.  Without recognising this we
    *      would draw a single full-target quad for every slice
    *      sub-section, painting Ozone's selection cursor as a
    *      giant rectangle covering the whole sidebar.
    *
    * Texture sampling has the same two conventions for tex_coord:
    * NULL → full texture, non-NULL → 4 normalised 0..1 (u,v)
    * pairs picking a sub-rect (top-down convention here per the
    * comment in gfx_display.c). */
   have_vertex_coords = (draw->coords->vertex    != NULL);
   have_tex_coords    = (draw->coords->tex_coord != NULL);

   if (have_vertex_coords)
   {
      const float *v = draw->coords->vertex;
      float min_nx, max_nx, min_ny, max_ny;
      float vx[4], vy[4];
      int   i;

      /* 4 vertices × 2 floats = 8 floats. */
      vx[0] = v[0]; vy[0] = v[1];
      vx[1] = v[2]; vy[1] = v[3];
      vx[2] = v[4]; vy[2] = v[5];
      vx[3] = v[6]; vy[3] = v[7];

      min_nx = max_nx = vx[0];
      min_ny = max_ny = vy[0];
      for (i = 1; i < 4; i++)
      {
         if (vx[i] < min_nx) min_nx = vx[i];
         if (vx[i] > max_nx) max_nx = vx[i];
         if (vy[i] < min_ny) min_ny = vy[i];
         if (vy[i] > max_ny) max_ny = vy[i];
      }

      /* Y is bottom-up in the vertex array; flip to top-down
       * pixels.  The bounding box is what we actually draw —
       * GDI doesn't have non-affine geometry support, but for
       * 9-patch slices the four vertices always form an
       * axis-aligned rect, so the bounding box is the geometry. */
      dst_x = (int)(min_nx * (float)target_w + 0.5f);
      dst_y = (int)((1.0f - max_ny) * (float)target_h + 0.5f);
      {
         int x_end = (int)(max_nx * (float)target_w + 0.5f);
         int y_end = (int)((1.0f - min_ny) * (float)target_h + 0.5f);
         dst_w = (x_end > dst_x) ? (unsigned)(x_end - dst_x) : 0;
         dst_h = (y_end > dst_y) ? (unsigned)(y_end - dst_y) : 0;
      }
   }
   else
   {
      if (draw->width == 0 || draw->height == 0)
         return;
      /* Plain quad path: draw->y is bottom-up in caller's
       * coordinate system (the video_height value we were
       * passed), so flip. */
      dst_x = (int)draw->x;
      dst_y = (int)video_height - (int)draw->height - (int)draw->y;
      dst_w = draw->width;
      dst_h = draw->height;

      /* Apply draw->scale_factor (centered scaling around the
       * quad's midpoint).  XMB sets this on icon draws (node->zoom)
       * to grow the active tab icon roughly 2x relative to passive
       * icons.  d3d9 / d3d10 / gl1 all apply this in the same way;
       * without it, every icon renders at its base unscaled size
       * and the active tab is visually indistinguishable from
       * inactive tabs.
       *
       * Only honoured on the plain-quad path: gfx_display_draw_quad,
       * gfx_display_draw_bg and gfx_display_draw_texture all
       * explicitly set draw->scale_factor = 1.0f.  The slice path
       * (gfx_display_draw_texture_slice — Ozone cursor highlight,
       * message-box panels) leaves draw->scale_factor as stack
       * garbage from the local gfx_display_ctx_draw_t variable, so
       * applying it to the bounding-box rect would warp the slices.
       * The slice path encodes its scaling directly in coords->vertex,
       * which we've already consumed above. */
      if (draw->scale_factor > 0.0f && draw->scale_factor != 1.0f)
      {
         float cx = (float)dst_x + (float)dst_w * 0.5f;
         float cy = (float)dst_y + (float)dst_h * 0.5f;
         float hw = (float)dst_w * 0.5f * draw->scale_factor;
         float hh = (float)dst_h * 0.5f * draw->scale_factor;
         int   x1 = (int)(cx - hw + 0.5f);
         int   y1 = (int)(cy - hh + 0.5f);
         int   x2 = (int)(cx + hw + 0.5f);
         int   y2 = (int)(cy + hh + 0.5f);
         dst_x   = x1;
         dst_y   = y1;
         dst_w   = (x2 > x1) ? (unsigned)(x2 - x1) : 0;
         dst_h   = (y2 > y1) ? (unsigned)(y2 - y1) : 0;
      }
   }

   if (dst_w == 0 || dst_h == 0)
      return;

   /* Trivial reject: quad fully outside the active target bounds. */
   if (     dst_x >= (int)target_w
         || dst_y >= (int)target_h)
      return;
   if (     (int)(dst_x + (int)dst_w) <= 0
         || (int)(dst_y + (int)dst_h) <= 0)
      return;
   /* Fully transparent quad - nothing to draw. */
   if (avg_a == 0)
      return;

   texture = (gdi_texture_t*)draw->texture;

   /* Compute the source sub-rect from tex_coord if present.
    * tex_coord is 4 (u,v) pairs in normalised 0..1 top-down space.
    * For 9-patch this picks the corresponding source slice within
    * the cursor texture.  Without this we'd sample the full source
    * for every slice, which would re-tile the entire cursor PNG
    * onto each section. */
   if (have_tex_coords && texture && texture->width > 0 && texture->height > 0)
   {
      const float *t = draw->coords->tex_coord;
      float min_u, max_u, min_v, max_v;
      float tu[4], tv[4];
      int   i;

      tu[0] = t[0]; tv[0] = t[1];
      tu[1] = t[2]; tv[1] = t[3];
      tu[2] = t[4]; tv[2] = t[5];
      tu[3] = t[6]; tv[3] = t[7];

      min_u = max_u = tu[0];
      min_v = max_v = tv[0];
      for (i = 1; i < 4; i++)
      {
         if (tu[i] < min_u) min_u = tu[i];
         if (tu[i] > max_u) max_u = tu[i];
         if (tv[i] < min_v) min_v = tv[i];
         if (tv[i] > max_v) max_v = tv[i];
      }

      src_x = (int)(min_u * (float)texture->width  + 0.5f);
      src_y = (int)(min_v * (float)texture->height + 0.5f);
      {
         int x_end = (int)(max_u * (float)texture->width  + 0.5f);
         int y_end = (int)(max_v * (float)texture->height + 0.5f);
         src_w = (x_end > src_x) ? (unsigned)(x_end - src_x) : 0;
         src_h = (y_end > src_y) ? (unsigned)(y_end - src_y) : 0;
      }

      /* Defensive clamp: edge slices can legitimately have
       * src_w / src_h = 0 due to floating point rounding when a
       * slice has zero source extent (e.g. middle section of a
       * 9-patch where the texture has no middle).  Skip those. */
      if (src_w == 0 || src_h == 0)
         return;
   }

   /* gfx_white_texture is a 1x1 white pixel; the menu code uses
    * "draw a textured quad with the white texture" as its idiom
    * for solid-colour rectangles.  Detect that case so we can
    * skip the (much more expensive) blit path entirely. */
   is_white_texture = (!texture || (texture->width <= 1 && texture->height <= 1));

   if (is_white_texture)
   {
      /* Solid quad with optional gradient.  Three sub-cases:
       *
       *   a) avg_a == 255 and uniform colour: FillRect + brush
       *      (fastest, opaque overwrite).
       *   b) avg_a < 255 and uniform colour: AlphaBlend a 1x1
       *      premultiplied source for true compositing.
       *   c) per-vertex gradient: GradientFill (Win98+) for
       *      2-stop ramps, falling back to the averaged colour
       *      for older platforms or anything the API can't
       *      directly express. */
      bool     gradient = gdi_color_is_gradient(bl, br, tl, tr);

      if (!gradient && avg_a == 255)
      {
         RECT rect;
         rect.left   = dst_x;
         rect.top    = dst_y;
         rect.right  = dst_x + (int)dst_w;
         rect.bottom = dst_y + (int)dst_h;
         gdi_ensure_brush(gdi, RGB(avg_r, avg_g, avg_b));
         if (gdi->brush_cached)
            FillRect(gdi->memDC, &rect, gdi->brush_cached);
         return;
      }

#if GDI_HAS_ALPHABLEND
      if (gradient)
      {
         /* Per-vertex gradient.  GDI's GradientFill turns out to
          * produce subtly wrong colour values in some configurations
          * (the produced band looks measurably darker than either
          * endpoint), so we render the gradient ourselves into a
          * scratch DIB section in software and BitBlt or AlphaBlend
          * the result.  This is more code than calling GradientFill
          * but gives pixel-exact, predictable output that matches
          * the d3d9/gl1 reference renderers.
          *
          * Layout: 4 vertices in the colour array, indexed BL/BR/TL/TR
          * (y is bottom-up in the source convention).  We bilinearly
          * interpolate per channel across the destination rectangle.
          * The scratch DIB is cached on gdi_t (gdi->scratch_quad_*)
          * and grow-only across calls; stride is the cached DIB's
          * row width which may exceed dst_w when an earlier draw
          * needed a wider gradient. */
         uint32_t *out;
         unsigned  ix, iy;
         unsigned  stride;
         /* Pull RGBA components from each corner.  Alpha is
          * interpolated per-pixel just like RGB, because the alpha
          * channel itself can carry the gradient — widget drop
          * shadows hold RGB constant at black and animate alpha
          * top→bottom from opaque to transparent. */
         uint8_t   bl_r = (bl >> 16) & 0xFF, bl_g = (bl >> 8) & 0xFF, bl_b = bl & 0xFF;
         uint8_t   br_r = (br >> 16) & 0xFF, br_g = (br >> 8) & 0xFF, br_b = br & 0xFF;
         uint8_t   tl_r = (tl >> 16) & 0xFF, tl_g = (tl >> 8) & 0xFF, tl_b = tl & 0xFF;
         uint8_t   tr_r = (tr >> 16) & 0xFF, tr_g = (tr >> 8) & 0xFF, tr_b = tr & 0xFF;
         uint8_t   bl_a = (bl >> 24) & 0xFF, br_a = (br >> 24) & 0xFF;
         uint8_t   tl_a = (tl >> 24) & 0xFF, tr_a = (tr >> 24) & 0xFF;
         /* If every corner is fully opaque we can skip the
          * AlphaBlend at present time and use a faster BitBlt. */
         bool all_opaque = (bl_a == 255 && br_a == 255
                         && tl_a == 255 && tr_a == 255);

         if (!gdi_ensure_scratch_quad(gdi, dst_w, dst_h))
            return;
         out    = gdi->scratch_quad_pixels;
         stride = gdi->scratch_quad_w;

         /* In practice the menu / widget code almost always draws 1D
          * gradients: vertical (header strips, drop shadows, sidebar
          * fades — TL == TR and BL == BR) or horizontal (rare but
          * exists — TL == BL and TR == BR).  The general 4-corner
          * bilinear case is the fallback for anything that doesn't
          * fit those patterns.
          *
          * Detection lets us collapse the doubly-nested per-pixel
          * loop into a single 1D computation plus a fill — for a
          * 600x80 vertical gradient, that's ~80 pixel computes
          * instead of ~48000.  At wider resolutions (full-window
          * widget shadows on a 4K display) the saving scales with
          * dst_w. */
         {
            bool vertical_only   = (tl == tr) && (bl == br);
            bool horizontal_only = (tl == bl) && (tr == br);

            /* Solid-colour case (all four corners equal) is caught
             * by gdi_color_is_gradient() upstream and routed to the
             * FillRect path before we get here, so we don't need to
             * special-case it.  But if it ever did slip through
             * (e.g. uniform alpha fade), both vertical_only and
             * horizontal_only would be true and we'd just use the
             * vertical path — still correct. */

            if (vertical_only)
            {
               /* TL == TR and BL == BR, so every row is a uniform
                * colour interpolated between top and bottom.
                * Compute the row colour once, fill the row.  We use
                * the BL corner for the bottom contribution and TL
                * for the top contribution since the L/R sides are
                * identical by precondition. */
               for (iy = 0; iy < dst_h; iy++)
               {
                  uint32_t *row = out + (size_t)iy * stride;
                  unsigned ty   = (dst_h <= 1) ? 0 : (iy * 255u) / (dst_h - 1);
                  unsigned t_top = 255u - ty;
                  unsigned t_bot = ty;
                  uint32_t r_   = GDI_DIV255(tl_r * t_top + bl_r * t_bot);
                  uint32_t g_   = GDI_DIV255(tl_g * t_top + bl_g * t_bot);
                  uint32_t b_   = GDI_DIV255(tl_b * t_top + bl_b * t_bot);
                  uint32_t a_   = GDI_DIV255(tl_a * t_top + bl_a * t_bot);
                  uint32_t pix;
                  if (all_opaque)
                     pix = (0xFFu << 24) | (r_ << 16) | (g_ << 8) | b_;
                  else
                  {
                     uint32_t pr = GDI_DIV255(r_ * a_);
                     uint32_t pg = GDI_DIV255(g_ * a_);
                     uint32_t pb = GDI_DIV255(b_ * a_);
                     pix = (a_ << 24) | (pr << 16) | (pg << 8) | pb;
                  }
                  /* Fill the row.  Inline 32-bit stores are what
                   * any remotely competent compiler turns this into
                   * (rep stosd or vectorised); explicit memset_pattern4
                   * isn't portable C. */
                  for (ix = 0; ix < dst_w; ix++)
                     row[ix] = pix;
               }
            }
            else if (horizontal_only)
            {
               /* TL == BL and TR == BR — every column is uniform.
                * Compute the first row pixel-by-pixel, then memcpy
                * it to every subsequent row. */
               uint32_t *first_row = out;
               for (ix = 0; ix < dst_w; ix++)
               {
                  unsigned tx      = (dst_w <= 1) ? 0 : (ix * 255u) / (dst_w - 1);
                  unsigned t_left  = 255u - tx;
                  unsigned t_right = tx;
                  uint32_t r_      = GDI_DIV255(tl_r * t_left + tr_r * t_right);
                  uint32_t g_      = GDI_DIV255(tl_g * t_left + tr_g * t_right);
                  uint32_t b_      = GDI_DIV255(tl_b * t_left + tr_b * t_right);
                  uint32_t a_      = GDI_DIV255(tl_a * t_left + tr_a * t_right);
                  if (all_opaque)
                     first_row[ix] = (0xFFu << 24) | (r_ << 16) | (g_ << 8) | b_;
                  else
                  {
                     uint32_t pr = GDI_DIV255(r_ * a_);
                     uint32_t pg = GDI_DIV255(g_ * a_);
                     uint32_t pb = GDI_DIV255(b_ * a_);
                     first_row[ix] = (a_ << 24) | (pr << 16) | (pg << 8) | pb;
                  }
               }
               for (iy = 1; iy < dst_h; iy++)
                  memcpy(out + (size_t)iy * stride, first_row,
                        (size_t)dst_w * sizeof(uint32_t));
            }
            else
            {
               /* General 4-corner bilinear.  Bilinear interpolation:
                * t_x in [0,1] across width, t_y in [0,1] across
                * height.  Source rows in the colour array are
                * bottom-up, so y=0 (top of dst) corresponds to
                * t_y=1 (TL/TR) and y=dst_h-1 (bottom) corresponds
                * to t_y=0 (BL/BR). */
               for (iy = 0; iy < dst_h; iy++)
               {
                  unsigned ty;
                  unsigned t_top, t_bot;
                  uint32_t *row = out + (size_t)iy * stride;

                  ty    = (dst_h <= 1) ? 0 : (iy * 255u) / (dst_h - 1);
                  t_bot = ty;
                  t_top = 255u - ty;

                  for (ix = 0; ix < dst_w; ix++)
                  {
                     unsigned tx, t_left, t_right;
                     uint32_t left_r, left_g, left_b, left_a;
                     uint32_t right_r, right_g, right_b, right_a;
                     uint32_t r_, g_, b_, a_;

                     tx      = (dst_w <= 1) ? 0 : (ix * 255u) / (dst_w - 1);
                     t_right = tx;
                     t_left  = 255u - tx;

                     /* Vertical interp: left edge (TL→BL) and right
                      * edge (TR→BR). */
                     left_r  = GDI_DIV255(tl_r * t_top + bl_r * t_bot);
                     left_g  = GDI_DIV255(tl_g * t_top + bl_g * t_bot);
                     left_b  = GDI_DIV255(tl_b * t_top + bl_b * t_bot);
                     left_a  = GDI_DIV255(tl_a * t_top + bl_a * t_bot);
                     right_r = GDI_DIV255(tr_r * t_top + br_r * t_bot);
                     right_g = GDI_DIV255(tr_g * t_top + br_g * t_bot);
                     right_b = GDI_DIV255(tr_b * t_top + br_b * t_bot);
                     right_a = GDI_DIV255(tr_a * t_top + br_a * t_bot);

                     /* Horizontal interp between the two vertical
                      * edges. */
                     r_ = GDI_DIV255(left_r * t_left + right_r * t_right);
                     g_ = GDI_DIV255(left_g * t_left + right_g * t_right);
                     b_ = GDI_DIV255(left_b * t_left + right_b * t_right);
                     a_ = GDI_DIV255(left_a * t_left + right_a * t_right);

                     if (all_opaque)
                        row[ix] = (0xFFu << 24) | (r_ << 16) | (g_ << 8) | b_;
                     else
                     {
                        uint32_t pr = GDI_DIV255(r_ * a_);
                        uint32_t pg = GDI_DIV255(g_ * a_);
                        uint32_t pb = GDI_DIV255(b_ * a_);
                        row[ix] = (a_ << 24) | (pr << 16) | (pg << 8) | pb;
                     }
                  }
               }
            }
         }

         /* Blit the gradient into bmp_menu / bmp.  Fully-opaque
          * gradient uses BitBlt (faster, ignores alpha); anything
          * with alpha variation uses AlphaBlend with the
          * premultiplied source we just wrote. */
         if (!gdi->texDC)
            gdi->texDC = CreateCompatibleDC(gdi->winDC);
         {
            HBITMAP scratch_old = (HBITMAP)SelectObject(gdi->texDC, gdi->scratch_quad_bmp);
            if (all_opaque)
            {
               BitBlt(gdi->memDC,
                     dst_x, dst_y, dst_w, dst_h,
                     gdi->texDC, 0, 0, SRCCOPY);
            }
            else
            {
               BLENDFUNCTION blend;
               blend.BlendOp             = AC_SRC_OVER;
               blend.BlendFlags          = 0;
               blend.SourceConstantAlpha = 255;
               blend.AlphaFormat         = AC_SRC_ALPHA;
               AlphaBlend(gdi->memDC,
                     dst_x, dst_y, dst_w, dst_h,
                     gdi->texDC, 0, 0, dst_w, dst_h, blend);
            }
            SelectObject(gdi->texDC, scratch_old);
         }
         return;
      }

      /* Translucent solid colour.  AlphaBlend a 1x1 premultiplied
       * pixel across the destination rect.  The 1x1 DIB is cached on
       * gdi_t (gdi->scratch_1x1_bmp); we just rewrite the pixel
       * each call. */
      {
         HBITMAP src_old;
         BLENDFUNCTION blend;
         uint32_t pre;

         if (!gdi_ensure_scratch_1x1(gdi))
            return;

         /* Premultiply the source colour by its alpha. */
         pre = ((uint32_t)avg_a << 24)
             | (GDI_DIV255((uint32_t)avg_r * avg_a) << 16)
             | (GDI_DIV255((uint32_t)avg_g * avg_a) <<  8)
             |  GDI_DIV255((uint32_t)avg_b * avg_a);
         *gdi->scratch_1x1_pixels = pre;

         if (!gdi->texDC)
            gdi->texDC = CreateCompatibleDC(gdi->winDC);
         src_old = (HBITMAP)SelectObject(gdi->texDC, gdi->scratch_1x1_bmp);

         blend.BlendOp             = AC_SRC_OVER;
         blend.BlendFlags          = 0;
         blend.SourceConstantAlpha = 255;
         blend.AlphaFormat         = AC_SRC_ALPHA;

         AlphaBlend(gdi->memDC,
               dst_x, dst_y, dst_w, dst_h,
               gdi->texDC, 0, 0, 1, 1, blend);

         SelectObject(gdi->texDC, src_old);
      }
#else
      /* Pre-Win98 fallback: no AlphaBlend, no GradientFill.  Just
       * draw the averaged opaque colour - we lose transparency
       * fidelity entirely, but the menu remains legible. */
      {
         RECT rect;
         rect.left   = dst_x;
         rect.top    = dst_y;
         rect.right  = dst_x + (int)dst_w;
         rect.bottom = dst_y + (int)dst_h;
         gdi_ensure_brush(gdi, RGB(avg_r, avg_g, avg_b));
         if (gdi->brush_cached)
            FillRect(gdi->memDC, &rect, gdi->brush_cached);
      }
#endif
      return;
   }

   /* Real texture path.  Make sure the per-texture DIB exists,
    * then composite it onto the menu surface. */
   if (!gdi_texture_realize(gdi, texture))
      return;

#if GDI_HAS_ALPHABLEND
   gdi_blit_texture_modulated(gdi, texture,
         dst_x, dst_y, dst_w, dst_h,
         src_x, src_y, src_w, src_h,
         avg_r, avg_g, avg_b, avg_a);
#else
   /* Pre-Win98 fallback: opaque scaled blit, no compositing.
    * Icons with transparent regions will show their full bounding
    * rectangle, but cores still run and the menu remains usable. */
   {
      int  bx_src = have_tex_coords ? src_x : 0;
      int  by_src = have_tex_coords ? src_y : 0;
      unsigned bw_src = (have_tex_coords && src_w) ? src_w
         : (unsigned)texture->width;
      unsigned bh_src = (have_tex_coords && src_h) ? src_h
         : (unsigned)texture->height;
      if (!gdi->texDC)
         gdi->texDC        = CreateCompatibleDC(gdi->winDC);
      texture->bmp_old     = (HBITMAP)SelectObject(gdi->texDC, texture->bmp);
      StretchBlt(gdi->memDC,
            dst_x, dst_y, dst_w, dst_h,
            gdi->texDC, bx_src, by_src, bw_src, bh_src,
            SRCCOPY);
      SelectObject(gdi->texDC, texture->bmp_old);
      texture->bmp_old     = NULL;
   }
#endif
}

gfx_display_ctx_driver_t gfx_display_ctx_gdi = {
   gfx_display_gdi_draw,
   NULL,                                     /* draw_pipeline   */
   gfx_display_gdi_blend_begin,
   gfx_display_gdi_blend_end,
   NULL,                                     /* get_default_mvp */
   gfx_display_gdi_get_default_vertices,
   gfx_display_gdi_get_default_tex_coords,
   FONT_DRIVER_RENDER_GDI,
   GFX_VIDEO_DRIVER_GDI,
   "gdi",
   false,
   gfx_display_gdi_scissor_begin,
   gfx_display_gdi_scissor_end
};

/*
 * FONT DRIVER
 *
 * Proper raster-font driver for GDI.  We do *not* use the platform
 * HFONT + TextOut approach (which the previous implementation did)
 * because:
 *
 *   - The menu drivers ship their own .ttf files (Open Sans, NotoSans,
 *     etc.) and expect their glyph metrics, not Tahoma's.
 *   - Layout code calls font_driver_get_message_width() and
 *     font_driver_get_line_metrics() per frame to position items;
 *     a GetTextExtentPoint32-based shim would round-trip the entire
 *     menu text content through GDI per call.  Painfully slow, and
 *     the metrics still wouldn't match the .ttf the menu was
 *     designed against.
 *
 * Instead we delegate to font_renderer_create_default which gives us
 * an A8 atlas plus per-glyph metrics from FreeType / STB / bitmap.
 * We mirror the atlas into a 32-bit BGRA *premultiplied* DIB (white
 * RGB, A = atlas sample) and AlphaBlend each glyph onto the menu
 * surface.  RGB tinting via per-glyph SourceConstantAlpha works for
 * the alpha component; for true RGB colour we render through a
 * per-line scratch DIB that bakes the tint into the glyph copy.
 *
 * For OSD messages on the core video output (not the menu), we
 * render to gdi->bmp - the DDB sized to the core frame - using the
 * same code path.
 */

typedef struct
{
   const font_renderer_driver_t *font_driver;
   void                         *font_data;
   struct font_atlas            *atlas;
   gdi_t                        *gdi;
   /* DIB section that mirrors the atlas in 32-bit BGRA premultiplied
    * form.  Re-uploaded whenever the underlying atlas grows or the
    * dirty flag is set.  Sized to atlas width × height, top-down. */
   HBITMAP                       atlas_bmp;
   uint32_t                     *atlas_pixels;
   unsigned                      atlas_width;
   unsigned                      atlas_height;
   /* Scratch DIB used to bake an RGB tint into a copy of the atlas
    * for one render_msg call.  Sized to the largest line we've
    * encountered; we reuse it across calls to keep allocations
    * bounded. */
   HBITMAP                       scratch_bmp;
   uint32_t                     *scratch_pixels;
   unsigned                      scratch_width;
   unsigned                      scratch_height;
} gdi_raster_t;

/* Build / refresh the BGRA premultiplied DIB that mirrors the A8
 * atlas.  The atlas grows as new glyphs are encountered (the font
 * backend tracks this via atlas->dirty); we resize the DIB on width
 * or height change. */
static bool gdi_font_upload_atlas(gdi_raster_t *font)
{
   BITMAPINFO bmi;
   void *pixels = NULL;
   unsigned i, j;

   if (!font || !font->atlas || !font->gdi || !font->gdi->memDC)
      return false;

   if (     !font->atlas_bmp
         || font->atlas_width  != font->atlas->width
         || font->atlas_height != font->atlas->height)
   {
      if (font->atlas_bmp)
      {
         DeleteObject(font->atlas_bmp);
         font->atlas_bmp    = NULL;
         font->atlas_pixels = NULL;
      }

      memset(&bmi, 0, sizeof(bmi));
      bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth       = (LONG)font->atlas->width;
      bmi.bmiHeader.biHeight      = -(LONG)font->atlas->height;
      bmi.bmiHeader.biPlanes      = 1;
      bmi.bmiHeader.biBitCount    = 32;
      bmi.bmiHeader.biCompression = BI_RGB;

      font->atlas_bmp = CreateDIBSection(font->gdi->memDC, &bmi,
            DIB_RGB_COLORS, &pixels, NULL, 0);
      if (!font->atlas_bmp || !pixels)
      {
         if (font->atlas_bmp)
            DeleteObject(font->atlas_bmp);
         font->atlas_bmp = NULL;
         return false;
      }

      font->atlas_pixels = (uint32_t*)pixels;
      font->atlas_width  = font->atlas->width;
      font->atlas_height = font->atlas->height;
   }

   /* Expand A8 -> BGRA premultiplied: A=atlas[i], R=G=B=A.  This
    * gives us a "white glyph with embedded alpha" source that
    * AlphaBlend can composite directly with AC_SRC_ALPHA. */
   for (j = 0; j < font->atlas->height; j++)
   {
      uint32_t      *dst = font->atlas_pixels
         + (size_t)j * font->atlas_width;
      const uint8_t *src = font->atlas->buffer
         + (size_t)j * font->atlas->width;
      for (i = 0; i < font->atlas->width; i++)
      {
         uint32_t a = src[i];
         dst[i] = (a << 24) | (a << 16) | (a << 8) | a;
      }
   }

   return true;
}

static bool gdi_font_ensure_scratch(gdi_raster_t *font,
      unsigned width, unsigned height)
{
   BITMAPINFO bmi;
   void *pixels = NULL;

   if (!font || !font->gdi || !font->gdi->memDC)
      return false;

   if (     font->scratch_bmp
         && font->scratch_width  >= width
         && font->scratch_height >= height)
      return true;

   /* Grow only - never shrink, so a long line followed by short
    * lines doesn't thrash the allocator.  Grow geometrically to
    * amortise repeated small expansions. */
   if (font->scratch_bmp)
   {
      DeleteObject(font->scratch_bmp);
      font->scratch_bmp    = NULL;
      font->scratch_pixels = NULL;
   }
   if (font->scratch_width  < width)
      font->scratch_width  = width  + width  / 2 + 1;
   if (font->scratch_height < height)
      font->scratch_height = height + height / 2 + 1;

   memset(&bmi, 0, sizeof(bmi));
   bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth       = (LONG)font->scratch_width;
   bmi.bmiHeader.biHeight      = -(LONG)font->scratch_height;
   bmi.bmiHeader.biPlanes      = 1;
   bmi.bmiHeader.biBitCount    = 32;
   bmi.bmiHeader.biCompression = BI_RGB;

   font->scratch_bmp = CreateDIBSection(font->gdi->memDC, &bmi,
         DIB_RGB_COLORS, &pixels, NULL, 0);
   if (!font->scratch_bmp || !pixels)
   {
      if (font->scratch_bmp)
         DeleteObject(font->scratch_bmp);
      font->scratch_bmp    = NULL;
      font->scratch_pixels = NULL;
      return false;
   }
   font->scratch_pixels = (uint32_t*)pixels;
   return true;
}

static void *gdi_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   gdi_raster_t *font = (gdi_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gdi = (gdi_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas = font->font_driver->get_atlas(font->font_data);

   /* The atlas DIB is created lazily on first render_msg, since
    * gdi->memDC may not exist yet at font init time (font_driver
    * is initialised before the first frame in some code paths). */
   return font;
}

static void gdi_font_free(void *data, bool is_threaded)
{
   gdi_raster_t *font = (gdi_raster_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (font->atlas_bmp)
      DeleteObject(font->atlas_bmp);
   if (font->scratch_bmp)
      DeleteObject(font->scratch_bmp);

   free(font);
}

static int gdi_font_get_message_width(void *data,
      const char *msg, size_t msg_len, float scale)
{
   const struct font_glyph *glyph_q = NULL;
   gdi_raster_t *font = (gdi_raster_t*)data;
   const char *msg_end;
   int delta_x = 0;

   if (!font || !font->font_driver || !font->font_data || !msg)
      return 0;

   msg_end = msg + msg_len;
   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      const struct font_glyph *glyph;
      unsigned code = utf8_walk(&msg);
      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;
      delta_x += glyph->advance_x;
   }

   return (int)(delta_x * scale);
}

static const struct font_glyph *gdi_font_get_glyph(
      void *data, uint32_t code)
{
   gdi_raster_t *font = (gdi_raster_t*)data;
   if (!font || !font->font_driver || !font->font_data)
      return NULL;
   return font->font_driver->get_glyph(font->font_data, code);
}

static bool gdi_font_get_line_metrics(void *data,
      struct font_line_metrics **metrics)
{
   gdi_raster_t *font = (gdi_raster_t*)data;
   if (!font || !font->font_driver || !font->font_data
         || !font->font_driver->get_line_metrics)
      return false;
   font->font_driver->get_line_metrics(font->font_data, metrics);
   return true;
}

/* Render one line of text at (line_x, line_y) (top-left, pixel
 * coordinates on the active draw surface) with colour (r,g,b,a).
 * The active draw surface is whichever bitmap the font's gdi->memDC
 * currently has selected: bmp_menu when a menu is alive, or bmp
 * when drawing OSD over the core frame.
 *
 * Implementation: walk glyphs left-to-right, AlphaBlend each glyph
 * region from the atlas DIB onto the destination.  For non-white
 * colours we composite via the scratch DIB (one allocation per
 * line) which holds the line pre-coloured. */
static void gdi_font_render_line(
      gdi_raster_t *font, HDC dst_dc,
      const char *msg, size_t msg_len,
      int line_x, int line_y, float scale,
      enum text_alignment text_align,
      uint8_t r, uint8_t g, uint8_t b, uint8_t a,
      int viewport_width, int viewport_height)
{
#if GDI_HAS_ALPHABLEND
   const struct font_glyph *glyph_q;
   const char *msg_ptr;
   const char *msg_end;
   HDC      atlas_dc;
   HBITMAP  atlas_old;
   int      x_offset = 0;
   bool     plain_white;
#endif

   if (!font || !msg || !msg_len || !dst_dc)
      return;
#if !GDI_HAS_ALPHABLEND
   /* Pre-Win98 fallback: punt to the system font via TextOut.  We
    * lose the .ttf's exact glyph metrics, but the menu still has
    * legible text on Windows 95.  Coordinates are top-left here;
    * SetTextAlign defaults are TA_LEFT|TA_TOP which is what we want. */
   {
      HDC hdc = dst_dc;
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, RGB(r, g, b));
      TextOut(hdc, line_x, line_y, msg, (int)msg_len);
   }
   return;
#else

   if (!font->atlas)
      return;
   if (font->atlas->dirty)
   {
      gdi_font_upload_atlas(font);
      font->atlas->dirty = false;
   }
   if (!font->atlas_bmp || !font->atlas_pixels)
      return;

   plain_white = (r == 255 && g == 255 && b == 255);
   glyph_q     = font->font_driver->get_glyph(font->font_data, '?');

   atlas_dc = CreateCompatibleDC(dst_dc);
   if (!atlas_dc)
      return;
   atlas_old = (HBITMAP)SelectObject(atlas_dc, font->atlas_bmp);

   /* Optimisation: handle alignment up front by pre-measuring the
    * line.  The caller has already pre-positioned line_x for left-
    * aligned text, but for centre/right we need the line width. */
   if (text_align == TEXT_ALIGN_RIGHT || text_align == TEXT_ALIGN_CENTER)
   {
      int line_w = gdi_font_get_message_width(font, msg, msg_len, scale);
      if (text_align == TEXT_ALIGN_RIGHT)
         line_x -= line_w;
      else
         line_x -= line_w / 2;
   }

   msg_ptr = msg;
   msg_end = msg + msg_len;

   if (plain_white)
   {
      /* Hot path: no tint, AlphaBlend straight from the atlas.
       * AC_SRC_ALPHA + SourceConstantAlpha=a gives us per-glyph
       * alpha modulated by the requested overall opacity. */
      BLENDFUNCTION blend;
      blend.BlendOp             = AC_SRC_OVER;
      blend.BlendFlags          = 0;
      blend.SourceConstantAlpha = a;
      blend.AlphaFormat         = AC_SRC_ALPHA;

      while (msg_ptr < msg_end)
      {
         const struct font_glyph *glyph;
         unsigned code = utf8_walk(&msg_ptr);
         int gx, gy, gw, gh;

         if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
            if (!(glyph = glyph_q))
               continue;

         gw = (int)((float)glyph->width  * scale);
         gh = (int)((float)glyph->height * scale);

         if (gw > 0 && gh > 0)
         {
            gx = line_x + x_offset + (int)((float)glyph->draw_offset_x * scale);
            gy = line_y           + (int)((float)glyph->draw_offset_y * scale);

            AlphaBlend(dst_dc, gx, gy, gw, gh,
                  atlas_dc,
                  glyph->atlas_offset_x, glyph->atlas_offset_y,
                  glyph->width, glyph->height,
                  blend);
         }
         x_offset += (int)((float)glyph->advance_x * scale);
      }
   }
   else
   {
      /* Tinted path: bake the line into the scratch DIB with the
       * requested RGB colour, then AlphaBlend the whole line in
       * one go.  We size the scratch buffer to fit the line's
       * pixel extent including descender height. */
      struct font_line_metrics *metrics = NULL;
      int line_w = gdi_font_get_message_width(font, msg, msg_len, scale);
      int line_h;
      uint32_t pre_a, pre_r, pre_g, pre_b;
      BLENDFUNCTION blend;

      if (line_w <= 0)
         goto done;

      font->font_driver->get_line_metrics(font->font_data, &metrics);
      line_h = metrics ? (int)(metrics->height * scale + 0.5f) : 32;
      if (line_h <= 0)
         line_h = 32;

      if (!gdi_font_ensure_scratch(font, (unsigned)line_w, (unsigned)line_h))
         goto done;

      /* Clear scratch pixels to fully transparent. */
      {
         size_t total = (size_t)font->scratch_width * (size_t)font->scratch_height;
         memset(font->scratch_pixels, 0, total * sizeof(uint32_t));
      }

      /* Premultiply the requested colour.  We'll multiply by the
       * atlas alpha per pixel below. */
      pre_a = a;
      pre_r = GDI_DIV255((uint32_t)r * a);
      pre_g = GDI_DIV255((uint32_t)g * a);
      pre_b = GDI_DIV255((uint32_t)b * a);

      /* Composite glyphs into the scratch DIB.  Scale-1.0 fast path
       * does direct A8 → premultiplied BGRA copy; scaled glyphs go
       * through nearest-neighbour for simplicity (the menu fonts
       * are pre-rasterised at the right size, so scale is almost
       * always 1.0 in practice). */
      while (msg_ptr < msg_end)
      {
         const struct font_glyph *glyph;
         unsigned code = utf8_walk(&msg_ptr);
         int gx_dst, gy_dst, gw, gh;
         int gx_src, gy_src;

         if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
            if (!(glyph = glyph_q))
               continue;

         gw     = (int)((float)glyph->width  * scale);
         gh     = (int)((float)glyph->height * scale);
         gx_dst = x_offset + (int)((float)glyph->draw_offset_x * scale);
         gy_dst = (metrics ? (int)(metrics->ascender * scale + 0.5f) : 0)
                + (int)((float)glyph->draw_offset_y * scale);
         gx_src = (int)glyph->atlas_offset_x;
         gy_src = (int)glyph->atlas_offset_y;

         if (gw > 0 && gh > 0)
         {
            int yy, xx;
            for (yy = 0; yy < gh; yy++)
            {
               int dst_y2 = gy_dst + yy;
               int src_y2;
               uint32_t *dst_row;
               const uint8_t *src_row;
               if (dst_y2 < 0 || dst_y2 >= (int)font->scratch_height)
                  continue;
               src_y2  = gy_src + (int)((float)yy * (float)glyph->height
                     / (float)gh);
               if (src_y2 < 0 || src_y2 >= (int)font->atlas_height)
                  continue;

               dst_row = font->scratch_pixels
                  + (size_t)dst_y2 * font->scratch_width;
               src_row = font->atlas->buffer
                  + (size_t)src_y2 * font->atlas->width;

               for (xx = 0; xx < gw; xx++)
               {
                  int dst_x2 = gx_dst + xx;
                  int src_x2;
                  uint8_t  alpha;
                  uint32_t out_a, out_r, out_g, out_b;
                  if (dst_x2 < 0 || dst_x2 >= (int)font->scratch_width)
                     continue;
                  src_x2 = gx_src + (int)((float)xx * (float)glyph->width
                        / (float)gw);
                  if (src_x2 < 0 || src_x2 >= (int)font->atlas_width)
                     continue;

                  alpha = src_row[src_x2];
                  if (alpha == 0)
                     continue;

                  /* Premultiplied glyph pixel at the requested tint.
                   * Output alpha = atlas_a * tint_a; output RGB =
                   * tint_RGB premultiplied by output alpha. */
                  out_a = GDI_DIV255((uint32_t)alpha * pre_a);
                  out_r = GDI_DIV255((uint32_t)alpha * pre_r);
                  out_g = GDI_DIV255((uint32_t)alpha * pre_g);
                  out_b = GDI_DIV255((uint32_t)alpha * pre_b);
                  /* Last-write wins where glyphs overlap: kerned
                   * fonts can produce overlapping bounding boxes,
                   * but the actual coverage rarely overlaps. */
                  dst_row[dst_x2] =
                       (out_a << 24)
                     | (out_r << 16)
                     | (out_g <<  8)
                     |  out_b;
               }
            }
         }
         x_offset += (int)((float)glyph->advance_x * scale);
      }

      /* Bind scratch and AlphaBlend the whole line. */
      {
         HDC scratch_dc = CreateCompatibleDC(dst_dc);
         HBITMAP scratch_old;
         if (!scratch_dc)
            goto done;
         scratch_old = (HBITMAP)SelectObject(scratch_dc, font->scratch_bmp);

         /* Compute Y offset: line_y was passed as the baseline
          * approximation; we drew with ascender baked in, so adjust
          * back so that line_y aligns with the glyph baseline. */
         {
            int draw_y = line_y;
            if (metrics)
               draw_y = line_y - (int)(metrics->ascender * scale + 0.5f);

            blend.BlendOp             = AC_SRC_OVER;
            blend.BlendFlags          = 0;
            blend.SourceConstantAlpha = 255;
            blend.AlphaFormat         = AC_SRC_ALPHA;

            AlphaBlend(dst_dc, line_x, draw_y, line_w, line_h,
                  scratch_dc, 0, 0, line_w, line_h, blend);
         }

         SelectObject(scratch_dc, scratch_old);
         DeleteDC(scratch_dc);
      }
   }

done:
   SelectObject(atlas_dc, atlas_old);
   DeleteDC(atlas_dc);
#endif /* GDI_HAS_ALPHABLEND */
}

static void gdi_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   float    x, y, scale, drop_mod, drop_alpha;
   int      drop_x, drop_y;
   unsigned r, g, b, a;
   enum text_alignment text_align;
   gdi_raster_t *font   = (gdi_raster_t*)data;
   gdi_t        *gdi    = (gdi_t*)userdata;
   HDC           dst_dc;
   HBITMAP       dst_old = NULL;
   HBITMAP       dst_bmp;
   unsigned      width, height;
   struct font_line_metrics *line_metrics = NULL;
   int           line_h;
   const char   *line_start;
   int           line_index = 0;

   if (!font || !msg || !*msg || !gdi)
      return;
   if (!font->font_driver || !font->font_data)
      return;
   if (!gdi->memDC)
      return;

   /* Choose the destination bitmap.  When a textured menu is being
    * composited, we draw onto bmp_menu (so the menu's own font
    * draws land on the same surface as its quads); otherwise we
    * draw onto bmp (RGUI image / core frame DDB) for OSD messages.
    * For bmp, use bmp_width/bmp_height which holds the actual size
    * (could be RGUI's resolution, not the core's). */
   if (gdi->menu_textured_active && gdi->bmp_menu)
   {
      dst_bmp = gdi->bmp_menu;
      width   = gdi->menu_surface_width;
      height  = gdi->menu_surface_height;
   }
   else
   {
      dst_bmp = gdi->bmp;
      width   = gdi->bmp_width  ? gdi->bmp_width  : gdi->frame_width;
      height  = gdi->bmp_height ? gdi->bmp_height : gdi->frame_height;
   }

   if (!dst_bmp || !width || !height)
      return;

   if (params)
   {
      x          = params->x;
      y          = params->y;
      drop_x     = params->drop_x;
      drop_y     = params->drop_y;
      drop_mod   = params->drop_mod;
      drop_alpha = params->drop_alpha;
      scale      = params->scale;
      text_align = params->text_align;
      r          = FONT_COLOR_GET_RED  (params->color);
      g          = FONT_COLOR_GET_GREEN(params->color);
      b          = FONT_COLOR_GET_BLUE (params->color);
      a          = FONT_COLOR_GET_ALPHA(params->color);
   }
   else
   {
      settings_t *settings    = config_get_ptr();
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;
      x          = video_msg_pos_x;
      y          = video_msg_pos_y;
      drop_x     = -2;
      drop_y     = -2;
      drop_mod   = 0.3f;
      drop_alpha = 1.0f;
      scale      = 1.0f;
      text_align = TEXT_ALIGN_LEFT;
      r          = (unsigned)(video_msg_color_r * 255.0f);
      g          = (unsigned)(video_msg_color_g * 255.0f);
      b          = (unsigned)(video_msg_color_b * 255.0f);
      a          = 255;
   }

   /* Refresh the atlas if the backend reports new glyphs.  We do
    * this once per render_msg, before any line rendering, so the
    * scratch path can rely on font->atlas being in sync with
    * font->atlas_bmp. */
   if (font->atlas && font->atlas->dirty)
   {
      gdi_font_upload_atlas(font);
      font->atlas->dirty = false;
   }

   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_h = line_metrics ? (int)(line_metrics->height * scale + 0.5f) : 16;
   if (line_h <= 0)
      line_h = 16;

   /* Select destination once for the whole render_msg.  The display
    * draw path (gdi_frame) is responsible for re-selecting bmp_menu
    * after we return. */
   dst_dc  = gdi->memDC;
   dst_old = (HBITMAP)SelectObject(dst_dc, dst_bmp);

   /* Iterate over '\n'-separated lines.  Coordinates are normalised
    * 0..1 with Y from the bottom (gfx_display_draw_text passes
    * params.y = 1 - y_pixels/height); convert to top-down pixels
    * and apply per-line offsets. */
   line_start = msg;
   for (;;)
   {
      const char *line_end = line_start;
      size_t      line_len;
      int         pixel_x, pixel_y;
      int         drop_pixel_x, drop_pixel_y;

      while (*line_end && *line_end != '\n')
         line_end++;
      line_len = (size_t)(line_end - line_start);

      if (line_len > 0)
      {
         /* Convert (x_norm, y_norm) -> pixel.  y_norm starts as
          * (1 - y_pixels_from_top / height); invert.  Successive
          * lines step downward by line_h. */
         pixel_x = (int)(x * (float)width);
         pixel_y = (int)((1.0f - y) * (float)height) + line_index * line_h;

         /* Drop shadow pass (rendered first so the main glyph
          * lands on top). */
         if (drop_x || drop_y)
         {
            unsigned dr = (unsigned)((float)r * drop_mod);
            unsigned dg = (unsigned)((float)g * drop_mod);
            unsigned db = (unsigned)((float)b * drop_mod);
            unsigned da = (unsigned)((float)a * drop_alpha);
            if (dr > 255) dr = 255;
            if (dg > 255) dg = 255;
            if (db > 255) db = 255;
            if (da > 255) da = 255;

            /* params->drop_y in the GL/D3D drivers is "up", we want
             * "down" on a top-down surface, so subtract. */
            drop_pixel_x = pixel_x + drop_x;
            drop_pixel_y = pixel_y - drop_y;

            gdi_font_render_line(font, dst_dc,
                  line_start, line_len,
                  drop_pixel_x, drop_pixel_y, scale, text_align,
                  (uint8_t)dr, (uint8_t)dg, (uint8_t)db, (uint8_t)da,
                  (int)width, (int)height);
         }

         gdi_font_render_line(font, dst_dc,
               line_start, line_len,
               pixel_x, pixel_y, scale, text_align,
               (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a,
               (int)width, (int)height);
      }

      if (*line_end != '\n')
         break;
      line_start = line_end + 1;
      line_index++;
   }

   SelectObject(dst_dc, dst_old);
}

font_renderer_t gdi_font = {
   gdi_font_init,
   gdi_font_free,
   gdi_font_render_msg,
   "gdi",
   gdi_font_get_glyph,        /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   gdi_font_get_message_width,
   gdi_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static void gfx_ctx_gdi_get_video_size(
      unsigned *width, unsigned *height)
{
   HWND window                  = win32_get_window();

   if (window)
   {
      *width                    = g_win32_resize_width;
      *height                   = g_win32_resize_height;
   }
   else
   {
      RECT mon_rect;
      MONITORINFOEX current_mon;
      unsigned mon_id           = 0;
      HMONITOR hm_to_use        = NULL;

      win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
      mon_rect = current_mon.rcMonitor;
      *width   = mon_rect.right - mon_rect.left;
      *height  = mon_rect.bottom - mon_rect.top;
   }
}

static bool gfx_ctx_gdi_init(void)
{
   WNDCLASSEX wndclass      = {0};
   settings_t *settings     = config_get_ptr();
   uint8_t win32_flags      = win32_get_flags();

   if (win32_flags & WIN32_CMN_FLAG_INITED)
      return true;

   win32_window_reset();
   win32_monitor_init();

   wndclass.lpfnWndProc   = wnd_proc_gdi_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      wndclass.lpfnWndProc   = wnd_proc_gdi_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      wndclass.lpfnWndProc   = wnd_proc_gdi_winraw;
#endif
   if (!win32_window_init(&wndclass, true, NULL))
      return false;
   return true;
}

static void gfx_ctx_gdi_destroy(void)
{
   HWND     window         = win32_get_window();

   if (window && win32_gdi_hdc)
   {
      ReleaseDC(window, win32_gdi_hdc);
      win32_gdi_hdc = NULL;
   }

   if (window)
   {
      win32_monitor_from_window();
      win32_destroy_window();
   }

   if (g_win32_flags & WIN32_CMN_FLAG_RESTORE_DESKTOP)
   {
      win32_monitor_get_info();
      g_win32_flags &= ~WIN32_CMN_FLAG_RESTORE_DESKTOP;
   }

   g_win32_flags &= ~WIN32_CMN_FLAG_INITED;
}

static bool gfx_ctx_gdi_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (!win32_set_video_mode(NULL, width, height, fullscreen))
   {
      gfx_ctx_gdi_destroy();
      return false;
   }

   return true;
}

static void gfx_ctx_gdi_input_driver(
      input_driver_t **input, void **input_data)
{
   settings_t *settings = config_get_ptr();
#if _WIN32_WINNT >= 0x0501
#ifdef HAVE_WINRAWINPUT
   /* winraw only available since XP */
   if (string_is_equal(settings->arrays.input_driver, "raw"))
   {
      *input_data = input_driver_init_wrap(&input_winraw, settings->arrays.input_driver);
      if (*input_data)
      {
         *input     = &input_winraw;
         dinput_gdi = NULL;
         return;
      }
   }
#endif
#endif

#ifdef HAVE_DINPUT
   dinput_gdi  = input_driver_init_wrap(&input_dinput, settings->arrays.input_driver);
   *input      = dinput_gdi ? &input_dinput : NULL;
#else
   dinput_gdi  = NULL;
   *input      = NULL;
#endif
   *input_data = dinput_gdi;
}

static void gdi_create(gdi_t *gdi)
{
   char os[64] = {0};

   frontend_ctx_driver_t *ctx = frontend_get_ptr();

   if (!ctx || !ctx->get_os)
   {
      RARCH_ERR("[GDI] No frontend driver found.\n");
      return;
   }

   ctx->get_os(os, sizeof(os), &gdi->win_major, &gdi->win_minor);

   /* Are we running on Windows 98 or below? */
   if (gdi->win_major < 4 || (gdi->win_major == 4 && gdi->win_minor <= 10))
   {
      RARCH_LOG("[GDI] Win98 or lower detected, using slow frame conversion method for RGB444.\n");
      gdi->lte_win98 = true;
   }
}

static void *gdi_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned full_x, full_y;
   unsigned mode_width = 0, mode_height = 0;
   unsigned win_width  = 0, win_height  = 0;
   unsigned temp_width = 0, temp_height = 0;
   settings_t *settings                 = config_get_ptr();
   bool video_font_enable               = settings->bools.video_font_enable;
   gdi_t *gdi                           = (gdi_t*)calloc(1, sizeof(*gdi));

   if (!gdi)
      return NULL;

   *input                               = NULL;
   *input_data                          = NULL;

   gdi->frame_width                     = video->width;
   gdi->frame_height                    = video->height;
   gdi->rgb32                           = video->rgb32;

   gdi->frame_bits                      = video->rgb32 ? 32 : 16;

   if (video->rgb32)
      gdi->frame_pitch                  = video->width * 4;
   else
      gdi->frame_pitch                  = video->width * 2;

   /* Aspect-ratio handling.  Pulled from video_info_t at init the
    * same way d3d8/d3d9 do it; the user can override via the
    * set_aspect_ratio poke (which forces keep_aspect = true and
    * dirties the viewport).  should_resize starts true so the
    * first frame computes a real viewport before any present. */
   gdi->keep_aspect                     = video->force_aspect;
   gdi->should_resize                   = true;

   gdi_create(gdi);
   if (!gfx_ctx_gdi_init())
      goto error;

   gfx_ctx_gdi_get_video_size(&mode_width, &mode_height);

   full_x      = mode_width;
   full_y      = mode_height;
   mode_width  = 0;
   mode_height = 0;

   RARCH_LOG("[GDI] Detecting screen resolution: %ux%u.\n", full_x, full_y);

   win_width   = video->width;
   win_height  = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }

   mode_width      = win_width;
   mode_height     = win_height;

   if (!gfx_ctx_gdi_set_video_mode(mode_width,
            mode_height, video->fullscreen))
      goto error;

   mode_width     = 0;
   mode_height    = 0;

   gfx_ctx_gdi_get_video_size(&mode_width, &mode_height);

   temp_width     = mode_width;
   temp_height    = mode_height;
   mode_width     = 0;
   mode_height    = 0;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_output_size(temp_width, temp_height);
   else
      video_driver_get_output_size(&temp_width, &temp_height);
   gdi->full_width  = temp_width;
   gdi->full_height = temp_height;

   RARCH_LOG("[GDI] Using resolution %ux%u.\n", temp_width, temp_height);

   gfx_ctx_gdi_input_driver(input, input_data);

   if (video_font_enable)
      font_driver_init_osd(gdi,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_GDI);

   RARCH_LOG("[GDI] Init complete.\n");

   return gdi;

error:
   gfx_ctx_gdi_destroy();
   if (gdi)
      free(gdi);
   return NULL;
}

static bool gdi_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   struct bitmap_info info;
   unsigned mode_width              = 0;
   unsigned mode_height             = 0;
   const void *frame_to_copy        = frame;
   unsigned width                   = 0;
   unsigned height                  = 0;
   bool draw                        = true;
   gdi_t *gdi                       = (gdi_t*)data;
   unsigned bits                    = gdi->frame_bits;
   HWND hwnd                        = win32_get_window();
#ifdef HAVE_MENU
   bool menu_is_alive               = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE)
      ? true : false;
   bool menu_textured               = false;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active              = video_info->widgets_active;
#else
   bool widgets_active              = false;
#endif
   /* "Display Statistics" overlay: a fixed top-left dump of core/
    * video/audio state, gated by Settings → User Interface →
    * Onscreen Notifications → Display Statistics.  Rendered
    * through font_driver_render_msg with explicit positioning
    * params (osd_stat_params), distinct from the per-frame OSD
    * msg which uses the driver's default placement.
    *
    * Suppressed while the menu is alive, matching d3d8/d3d9: those
    * drivers branch stat_text rendering as `else if (statistics_show)`
    * off the menu condition.  The menu drivers
    * (XMB/Ozone/MaterialUI/RGUI) own the screen and have their own
    * stats display path; the overlay would just bleed through
    * underneath the menu. */
   const char *stat_text            = video_info->stat_text;
   struct font_params *osd_params   = (struct font_params*)
                                      &video_info->osd_stat_params;
   bool show_stats                  = video_info->statistics_show
                                      && stat_text && stat_text[0] != '\0'
#ifdef HAVE_MENU
                                      && !menu_is_alive
#endif
                                      ;
   unsigned surface_width;
   unsigned surface_height;

   /* GDI has no programmable shader pipeline, so the animated XMB
    * backgrounds (Ribbon / Snow / Bokeh / etc.) can't run — force
    * that off so XMB falls back to the static gradient. */
   video_info->menu_shader_pipeline = 0;

   if (!frame || !frame_width || !frame_height)
      return true;

   /* --- Step 1: ensure DCs and bitmaps exist BEFORE we run any
    * menu/widget draws.
    *
    * The legacy driver created memDC and gdi->bmp inline halfway
    * through gdi_frame, *after* menu_driver_frame had already run.
    * That meant the very first frame after a menu became active
    * silently dropped its draw calls.  We do all surface creation
    * up front so menu_driver_frame and gfx_widgets_frame have a
    * valid target from the first call onward. */
   if (hwnd && !gdi->winDC)
   {
      gdi->winDC        = GetDC(hwnd);
      gdi->memDC        = CreateCompatibleDC(gdi->winDC);
      /* gdi->bmp is sized to the core frame; created on first frame
       * with whatever size the core has just announced. */
      gdi->bmp          = CreateCompatibleBitmap(
            gdi->winDC, frame_width, frame_height);
      gdi->bmp_width    = frame_width;
      gdi->bmp_height   = frame_height;
      gdi->frame_width  = frame_width;
      gdi->frame_height = frame_height;
   }

   /* --- Step 2: figure out the on-screen surface size. */
   gfx_ctx_gdi_get_video_size(&mode_width, &mode_height);
   surface_width  = mode_width  ? mode_width  : gdi->full_width;
   surface_height = mode_height ? mode_height : gdi->full_height;
   if (!surface_width)  surface_width  = frame_width;
   if (!surface_height) surface_height = frame_height;

   gdi->screen_width  = surface_width;
   gdi->screen_height = surface_height;

   /* --- Step 2b: recompute the aspect-ratio-aware viewport when
    * something has dirtied it.  Triggers: window resize (caught in
    * gdi_alive), aspect-ratio change (set_aspect_ratio poke), or
    * generic state change (apply_state_changes poke).  The
    * resulting gdi->vp.{x,y,width,height} is the destination rect
    * for the core frame inside the window; everything outside it
    * is filled black at present time to produce letterbox /
    * pillarbox bars.  Mirrors d3d8's should_resize pattern. */
   if (gdi->should_resize)
   {
      gdi_set_viewport(gdi, surface_width, surface_height, false, true);
      gdi->should_resize = false;
   }
   /* Defensive: if vp was never populated (e.g. should_resize
    * cleared without us recomputing), fall back to full-window
    * destination so we still draw something. */
   if (gdi->vp.width == 0 || gdi->vp.height == 0)
   {
      gdi->vp.x           = 0;
      gdi->vp.y           = 0;
      gdi->vp.width       = surface_width;
      gdi->vp.height      = surface_height;
      gdi->vp.full_width  = surface_width;
      gdi->vp.full_height = surface_height;
   }

   /* --- Step 3: detect whether any window-resolution content needs
    * to be composited this frame.  Three triggers:
    *
    *   - Textured menu (XMB/Ozone/MaterialUI) is alive.  Its draws
    *     all expect window-pixel coordinates.
    *   - gfx_widgets is active.  Notifications, FPS counter, the
    *     load-content animation, etc. — all rendered at window
    *     pixel scale through the font driver.
    *   - An OSD msg is being rendered this frame.  Same window-
    *     pixel-scale font path.
    *
    * In all three cases we need bmp_menu as the compositing target.
    * If we tried to render those onto gdi->bmp (which is sized to
    * the core's frame, e.g. 256x224 for SNES), the StretchBlt at
    * present time would smear glyphs into illegible pixel mush —
    * the font driver writes glyphs at window-pixel scale onto a
    * surface only 1/15th that size.
    *
    * If none of the three triggers fire (just a core running, just
    * RGUI without widgets), we keep the legacy path: render at
    * core size into gdi->bmp and let WM_PAINT's StretchBlt scale
    * the whole image up.  That's the steady-state gameplay path
    * and we don't want to pay the bmp_menu allocation/clear cost
    * for it. */
#ifdef HAVE_MENU
   menu_textured = menu_is_alive && gdi->menu_enable && !gdi->menu_frame;
#endif
   {
      bool need_bmp_menu = false;
#ifdef HAVE_MENU
      if (menu_textured)
         need_bmp_menu = true;
#endif
#ifdef HAVE_GFX_WIDGETS
      if (widgets_active)
         need_bmp_menu = true;
#endif
#ifdef HAVE_OVERLAY
      /* Active input overlay: same reasoning as widgets — the
       * overlay is window-resolution content (button images sized
       * to window), so it has to land on bmp_menu rather than
       * being drawn into the small core-frame bmp and smeared
       * during WM_PAINT scaling. */
      if (gdi->overlays_enabled
            && gdi->overlays && gdi->overlays_size > 0)
         need_bmp_menu = true;
#endif
      if (msg && msg[0] != '\0')
         need_bmp_menu = true;
      if (show_stats)
         need_bmp_menu = true;

      if (need_bmp_menu)
         gdi_ensure_menu_surface(gdi, surface_width, surface_height);

      gdi->menu_textured_active = need_bmp_menu && (gdi->bmp_menu != NULL);
   }

   /* --- Step 4: if we need bmp_menu, prepare it as the draw target.
    *
    * Order matters: we SelectObject first, then clear via FillRect.
    * The clear goes through GDI's pipeline (same as every other
    * draw on bmp_menu), avoiding the GDI-batch-vs-direct-write
    * race that produced sporadic missing quads. */
   if (gdi->menu_textured_active && gdi->bmp_menu)
   {
      gdi->bmp_menu_old = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp_menu);
      SetBkMode(gdi->memDC, TRANSPARENT);
      /* Opaque black background.  Premultiplication doesn't matter
       * here because the clear is a fully-opaque FillRect, not an
       * AlphaBlend source. */
      gdi_menu_surface_clear(gdi, 0, 0, 0);
   }

   /* --- Step 4b: menu-over-running-game underlay.
    *
    * When the menu is up and the user has content loaded, upload
    * the actual game frame into bmp_menu as a background BEFORE
    * menu_driver_frame paints the menu on top.  This applies to:
    *
    *   - Textured menus (XMB/Ozone/MaterialUI): without this,
    *     their semi-transparent panels composite against solid
    *     black instead of the running game.
    *   - RGUI with transparency: its chequer pattern is
    *     rendered with partial alpha (when
    *     menu_rgui_transparency = true and the platform supports
    *     it, which gdi does — falls through to argb32_to_rgba4444
    *     in rgui_set_pixel_format_function).  The Step 9 RGUI
    *     branch composites that against bmp_menu, so the game
    *     needs to be there as the underlay.
    *
    * d3d9 achieves the same effect implicitly because each draw
    * call targets the back buffer which already holds the game
    * image; we have to do it explicitly because bmp_menu was
    * just cleared.
    *
    * Skipped when:
    *   - the menu isn't alive (no underlay needed)
    *   - no content is running (frame is the 4x4 placeholder;
    *     we want solid black under the menu) */
#ifdef HAVE_MENU
   if (     gdi->menu_textured_active
         && gdi->bmp_menu
         && menu_is_alive
         && frame
         && frame_width  > 4
         && frame_height > 4)
   {
      gdi_upload_core_frame_to_menu(gdi, frame,
            frame_width, frame_height, pitch, gdi->frame_bits);
   }
#endif

   /* --- Step 5: invoke the menu driver.  This is what actually
    * issues all the gfx_display_ctx_gdi_draw calls (and font draws)
    * that build up the menu image on bmp_menu. */
#ifdef HAVE_MENU
   if (gdi->menu_enable)
      menu_driver_frame(menu_is_alive, video_info);
#endif

   /* --- Step 6: track core-frame size changes (needed for both the
    * RGUI-overlay path and the no-menu path).  We resize bmp here
    * if the core's announced dimensions changed. */
   if (     (gdi->frame_width  != frame_width)
         || (gdi->frame_height != frame_height)
         || (gdi->frame_pitch  != pitch))
   {
      if (frame_width > 4 && frame_height > 4)
      {
         gdi->frame_width  = frame_width;
         gdi->frame_height = frame_height;
         gdi->frame_pitch  = pitch;
      }
   }

   /* --- Step 7: pick the source pixels for the core-frame upload.
    * RGUI's menu_frame takes precedence (legacy path); otherwise
    * use the core frame.  When a textured menu is alive, set
    * draw=false to skip the upload entirely (the menu surface is
    * the final image). */
#ifdef HAVE_MENU
   if (gdi->menu_frame && menu_is_alive)
   {
      frame_to_copy = gdi->menu_frame;
      width         = gdi->menu_width;
      height        = gdi->menu_height;
      pitch         = gdi->menu_pitch;
      bits          = gdi->menu_bits;
   }
   else
#endif
   {
      width         = gdi->frame_width;
      height        = gdi->frame_height;
      pitch         = gdi->frame_pitch;

      if (  frame_width  == 4
         && frame_height == 4
         && (frame_width < width && frame_height < height))
         draw = false;

#ifdef HAVE_MENU
      if (menu_is_alive)
         draw = false;
#endif
   }

   /* --- Step 8: resize bmp if its current size doesn't match the
    * effective draw target size (width/height computed in Step 7).
    * We compare against bmp_width/bmp_height (the DDB's own size),
    * NOT frame_width — the latter holds the core's announced frame
    * size, which can legitimately differ from the menu's frame size
    * when RGUI is alive.  Conflating the two causes a destructive
    * recreate every frame as Steps 6 and 8 fight over the field. */
   if (gdi->bmp_width != width || gdi->bmp_height != height)
   {
      /* Deselect bmp_menu temporarily so we can reselect bmp for
       * the resize; restore bmp_menu afterwards if it was selected. */
      bool reselect_menu = gdi->menu_textured_active && gdi->bmp_menu;
      if (reselect_menu)
      {
         SelectObject(gdi->memDC, gdi->bmp_menu_old);
         gdi->bmp_menu_old = NULL;
      }
      if (gdi->bmp)
         DeleteObject(gdi->bmp);

      gdi->bmp_width    = width;
      gdi->bmp_height   = height;
      gdi->bmp          = CreateCompatibleBitmap(
            gdi->winDC, gdi->bmp_width, gdi->bmp_height);

      if (reselect_menu && gdi->bmp_menu)
         gdi->bmp_menu_old = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp_menu);

      if (gdi->lte_win98)
      {
         unsigned short *tmp = NULL;
         if (gdi->temp_buf)
            free(gdi->temp_buf);
         tmp = (unsigned short*)malloc(width * height
               * sizeof(unsigned short));
         if (tmp)
            gdi->temp_buf = tmp;
      }
   }

   /* --- Step 9: blit the frame data into the active target.
    *
    * Two routing cases:
    *   - menu_textured_active && bmp_menu (widgets and/or OSD msg
    *     present, possibly with RGUI/core underneath): StretchDIBits
    *     into bmp_menu, scaled up to the window's surface size.
    *     Widgets and OSD draws will then land at native window
    *     resolution on top of the upscaled core/RGUI image.
    *   - Otherwise (just core, just RGUI, no widgets/OSD): legacy
    *     path — copy at native size into gdi->bmp, let WM_PAINT's
    *     StretchBlt scale at present time.
    *
    * For a true textured menu (XMB/Ozone/MaterialUI), draw=false
    * was already set in Step 7 because menu_is_alive is true and
    * menu_frame is NULL — the menu itself owns the bmp_menu image
    * and we have no underlying core frame to upload. */
   if (draw)
   {
      memset(&info, 0, sizeof(info));
      info.header.biSize         = sizeof(BITMAPINFOHEADER);
      info.header.biWidth        = pitch / (bits / 8);
      info.header.biHeight       = -(int)height;
      info.header.biPlanes       = 1;
      info.header.biBitCount     = bits;
      info.header.biCompression  = 0;

      if (bits == 16)
      {
         if (gdi->lte_win98 && gdi->temp_buf)
         {
            /* Win98 and below cannot use BI_BITFIELDS with RGB444,
             * so convert it to RGB555 first. */
            unsigned x, y;
            for (y = 0; y < height; y++)
            {
               for (x = 0; x < width; x++)
               {
                  unsigned short pixel =
                     ((unsigned short*)frame_to_copy)[width * y + x];
                  gdi->temp_buf[width * y + x] =
                       (pixel & 0xF000) >> 1
                     | (pixel & 0x0F00) >> 2
                     | (pixel & 0x00F0) >> 3;
               }
            }
            frame_to_copy = gdi->temp_buf;
            info.header.biCompression = BI_RGB;
         }
         else
         {
            info.header.biCompression = BI_BITFIELDS;
            /* default 16-bit format on Windows is XRGB1555 */
            if (frame_to_copy == gdi->menu_frame)
            {
               /* RGB444 for RGUI */
               info.u.masks[0] = 0xF000;
               info.u.masks[1] = 0x0F00;
               info.u.masks[2] = 0x00F0;
            }
            else
            {
               /* RGB565 for core */
               info.u.masks[0] = 0xF800;
               info.u.masks[1] = 0x07E0;
               info.u.masks[2] = 0x001F;
            }
         }
      }
      else
         info.header.biCompression = BI_RGB;

      if (gdi->menu_textured_active && gdi->bmp_menu)
      {
         /* bmp_menu was cleared in Step 4 (so the surrounding
          * pillarbox / letterbox bars stay black) and Step 4b
          * underlayed the running game frame into the viewport
          * sub-rect when content is loaded.  StretchDIBits goes
          * into that same viewport sub-rect; widget / OSD draws
          * that follow then land at native resolution on top of
          * the upscaled image (and on top of the bars, which is
          * fine — widgets are positioned in window space).
          *
          * For RGUI specifically, the source format is RGBA4444
          * with alpha actually meaningful (the chequer pattern
          * uses partial alpha when menu_rgui_transparency is on,
          * which is the default for any platform RGUI considers
          * transparency-capable — gdi falls into the
          * transparency-supported branch in
          * rgui_set_pixel_format_function).  We need a real
          * alpha-blend composite over the underlayed game frame,
          * not an opaque overwrite.  gdi_blit_rgui_alpha handles
          * the format conversion + AlphaBlend.
          *
          * The opaque StretchDIBits path remains for:
          *   - non-RGUI menu_frame sources, if any (none today).
          *   - core game frames going through this branch (the
          *     Step 4b skip-condition gates: menu_textured_active
          *     is true, but the source here is the core frame,
          *     not menu_frame).
          *   - Win95 fallback where AlphaBlend isn't available;
          *     transparency degrades to opaque, RGUI's chequer
          *     becomes solid, which is the same behaviour
          *     non-transparency-capable backends give. */
#ifdef GDI_HAS_ALPHABLEND
         if (frame_to_copy == gdi->menu_frame && bits == 16)
         {
            gdi_blit_rgui_alpha(gdi, frame_to_copy, width, height,
                  gdi->vp.x, gdi->vp.y, gdi->vp.width, gdi->vp.height);
         }
         else
#endif
         {
            StretchDIBits(gdi->memDC,
                  gdi->vp.x, gdi->vp.y, gdi->vp.width, gdi->vp.height,
                  0, 0, width, height,
                  frame_to_copy, (BITMAPINFO*)&info, DIB_RGB_COLORS, SRCCOPY);
         }
      }
      else
      {
         /* Legacy path: native-size copy into gdi->bmp; WM_PAINT
          * does the scaling AND viewport letterboxing at present
          * time. */
         gdi->bmp_old = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);
         StretchDIBits(gdi->memDC, 0, 0, width, height, 0, 0, width, height,
               frame_to_copy, (BITMAPINFO*)&info, DIB_RGB_COLORS, SRCCOPY);
         SelectObject(gdi->memDC, gdi->bmp_old);
         gdi->bmp_old = NULL;
      }
   }

   /* --- Step 10: Display Statistics overlay (statistics_show +
    * stat_text).  Gated by Settings → User Interface → Onscreen
    * Notifications → Display Statistics.  Rendered through the
    * font driver with osd_stat_params positioning (top-left, small
    * grey text).
    *
    * This runs BEFORE widgets so widget panels paint on top of the
    * stats text.  d3d8/d3d9 do the same: a notification or
    * achievement panel with a semi-transparent background should
    * obscure the stats it overlaps, not vice-versa. */
   if (show_stats)
      font_driver_render_msg(gdi, stat_text, osd_params, NULL);

   /* --- Step 10b: input overlay (touch / virtual gamepad images).
    *
    * Rendered between stats and widgets, matching the d3d8 / d3d9
    * order so overlapping widget notifications can still paint
    * over the buttons (rare in practice — widgets land in their
    * own corner — but keeping the layering consistent across
    * backends means an overlay-on-d3d9 config carries over here
    * unchanged). */
#ifdef HAVE_OVERLAY
   if (gdi->overlays_enabled)
      gdi_overlays_render(gdi, surface_width, surface_height);
#endif

   /* --- Step 11: render widgets.  Widgets are drawn through the
    * same gfx_display_ctx_gdi_draw path that the menu uses, but they
    * always paint after the core/menu base.
    *
    * Target selection follows the menu_textured_active flag set in
    * Step 3:
    *   - menu_textured_active → bmp_menu is already selected
    *     (Step 4) and contains either the textured-menu image or
    *     the upscaled core/RGUI frame from Step 9.  Widgets land
    *     on top at native window resolution.
    *   - Otherwise (legacy bmp path) → fall back to selecting bmp.
    *     This branch is now only reached if widgets_active is true
    *     but bmp_menu allocation failed; without bmp_menu the
    *     widget glyphs WILL look smeared after WM_PAINT scales the
    *     small bmp up, but that beats not rendering them at all. */
#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
   {
      bool selected_bmp_for_widgets = false;
      if (!gdi->menu_textured_active && gdi->bmp)
      {
         gdi->bmp_old = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);
         SetBkMode(gdi->memDC, TRANSPARENT);
         selected_bmp_for_widgets = true;
      }
      gfx_widgets_frame(video_info);
      if (selected_bmp_for_widgets)
      {
         SelectObject(gdi->memDC, gdi->bmp_old);
         gdi->bmp_old = NULL;
      }
   }
#endif

   /* --- Step 12: per-frame OSD message (transient text passed in
    * via the msg parameter from the core/runtime).  Rendered with
    * default positioning (the font driver's bottom-left fallback
    * when params is NULL).
    *
    * This runs AFTER widgets so messages aren't hidden behind
    * notification panels — the OSD msg is a one-shot event and
    * needs to be visible. */
   if (msg)
      font_driver_render_msg(gdi, msg, NULL, NULL);

   /* --- Step 13: final deselection.  If bmp_menu is still selected,
    * pop it now so memDC is back to its baseline (no bitmap selected,
    * default 1x1 bitmap).  This keeps DC ownership tidy across frame
    * boundaries; the next frame will reselect as needed. */
   if (gdi->bmp_menu_old && gdi->bmp_menu)
   {
      SelectObject(gdi->memDC, gdi->bmp_menu_old);
      gdi->bmp_menu_old = NULL;
   }

   /* --- Step 14: present.
    *
    * Two cases:
    *   - menu_textured_active (textured menu OR widgets/OSD/stats
    *     present): bmp_menu holds the final image at window size.
    *     Hand the DIB pixel buffer straight to SetDIBitsToDevice
    *     and ValidateRect.
    *   - Otherwise (just core, just RGUI): bmp holds the final
    *     image at native size.  InvalidateRect → WM_PAINT does the
    *     StretchBlt to the window.  This single-target,
    *     single-present design is the legacy behaviour and it
    *     does not flicker. */
   if (gdi->winDC)
   {
      if (gdi->menu_textured_active && gdi->bmp_menu && gdi->menu_pixels)
      {
         /* SetDIBitsToDevice takes the raw DIB pixel buffer
          * (gdi->menu_pixels) directly, skipping the round-trip
          * through a temporary CreateCompatibleDC / SelectObject /
          * BitBlt / DeleteDC sequence.  No scaling is involved
          * here — bmp_menu was allocated at exactly
          * surface_width x surface_height — so the no-stretch
          * SetDIBitsToDevice form fits perfectly.
          *
          * The DIB is top-down (biHeight = -surface_height in
          * gdi_ensure_menu_surface), so we pass a top-down
          * BITMAPINFOHEADER here too and StartScan / cLines
          * count from row 0 down.  Available since Windows 95,
          * so no compatibility regression vs the previous BitBlt
          * path.
          *
          * GdiFlush before sampling: bmp_menu was the selected
          * draw target up to step 13.  Direct-access reads
          * (which is what SetDIBitsToDevice does — it reads
          * lpvBits straight) need the GDI command queue drained
          * so all prior FillRect / AlphaBlend / etc. calls have
          * actually committed to the underlying pixel buffer.
          * Without this, sporadic missing-draw artifacts. */
         BITMAPINFO bmi;
         memset(&bmi, 0, sizeof(bmi));
         bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
         bmi.bmiHeader.biWidth       = (LONG)surface_width;
         bmi.bmiHeader.biHeight      = -(LONG)surface_height;
         bmi.bmiHeader.biPlanes      = 1;
         bmi.bmiHeader.biBitCount    = 32;
         bmi.bmiHeader.biCompression = BI_RGB;

         GdiFlush();
         SetDIBitsToDevice(gdi->winDC,
               0, 0, surface_width, surface_height,
               0, 0, 0, surface_height,
               gdi->menu_pixels, &bmi, DIB_RGB_COLORS);

         /* We just painted everything; suppress the WM_PAINT that
          * the legacy code path would otherwise queue. */
         ValidateRect(hwnd, NULL);
      }
      else
      {
         InvalidateRect(hwnd, NULL, false);
      }
   }

   video_driver_update_title(NULL);

   return true;
}

static bool gdi_alive(void *data)
{
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool quit            = false;
   bool resize          = false;
   bool ret             = false;
   gdi_t *gdi           = (gdi_t*)data;

   /* Read from local bookkeeping rather than video_st (which would
    * acquire context_lock + display_lock).  gdi->full_{width,height}
    * is written at every set_size call site in this driver. */
   temp_width  = gdi->full_width;
   temp_height = gdi->full_height;

   win32_check_window(NULL,
            &quit, &resize, &temp_width, &temp_height);

   ret = !quit;

   if (resize)
      gdi->should_resize = true;

   if (temp_width != 0 && temp_height != 0)
   {
      video_driver_set_output_size(temp_width, temp_height);
      gdi->full_width  = temp_width;
      gdi->full_height = temp_height;
   }

   return ret;
}

static void gdi_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool gdi_focus(void *data) { return true; }
static bool gdi_suppress_screensaver(void *a, bool b) { return false; }
static bool gdi_has_windowed(void *data) { return true; }

static void gdi_free(void *data)
{
   gdi_t *gdi = (gdi_t*)data;
   HWND hwnd  = win32_get_window();

   if (!gdi)
      return;

   if (gdi->menu_frame)
      free(gdi->menu_frame);
   gdi->menu_frame = NULL;

   if (gdi->temp_buf)
      free(gdi->temp_buf);
   gdi->temp_buf = NULL;

   /* Tear down menu surface (DIB section + selection state) before
    * the DC it's selected into goes away. */
   gdi_release_menu_surface(gdi);
   gdi_release_brush(gdi);

   /* Cached scratch DIB sections from the gradient / 1x1 / RGUI
    * paths.  Like the menu surface, these need to go before texDC
    * is destroyed — they may currently be selected into it. */
   gdi_release_scratch(gdi);

#ifdef HAVE_OVERLAY
   /* Overlay DIB sections must be released before texDC goes away
    * — they may have been selected into it during the last
    * render. */
   gdi_overlay_free(gdi);
#endif

   if (gdi->bmp)
      DeleteObject(gdi->bmp);
   gdi->bmp = NULL;

   if (gdi->texDC)
   {
      DeleteDC(gdi->texDC);
      gdi->texDC = 0;
   }
   if (gdi->memDC)
   {
      DeleteDC(gdi->memDC);
      gdi->memDC = 0;
   }

   if (hwnd && gdi->winDC)
   {
      ReleaseDC(hwnd, gdi->winDC);
      gdi->winDC = 0;
   }

   font_driver_free_osd();
   gfx_ctx_gdi_destroy();
   free(gdi);
}

static bool gdi_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static void gdi_set_texture_enable(
      void *data, bool state, bool full_screen)
{
   gdi_t *gdi     = (gdi_t*)data;
   if (!gdi)
      return;

   gdi->menu_enable      = state;
   gdi->menu_full_screen = full_screen;
}

static void gdi_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   gdi_t   *gdi     = (gdi_t*)data;
   unsigned pitch   = width * (rgb32 ? 4 : 2);
   size_t   required;

   if (!frame || !width || !height || !pitch)
      return;

   required = (size_t)pitch * (size_t)height;

   if (required > gdi->menu_frame_cap)
   {
      uint8_t *tmp = (uint8_t*)realloc(gdi->menu_frame, required);
      if (!tmp)
         return;                        /* keep previous frame intact */
      gdi->menu_frame     = tmp;
      gdi->menu_frame_cap = required;
   }

   memcpy(gdi->menu_frame, frame, required);
   gdi->menu_width  = width;
   gdi->menu_height = height;
   gdi->menu_pitch  = pitch;
   gdi->menu_bits   = rgb32 ? 32 : 16;
}

static void gdi_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_gdi_set_video_mode(width, height, fullscreen);
}

static uintptr_t gdi_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   gdi_texture_t *texture      = NULL;
   struct texture_image *image = (struct texture_image*)data;
   const uint32_t *src;
   uint32_t       *dst;
   size_t          i, total;
   bool            has_alpha   = false;

   if (!image || image->width > 2048 || image->height > 2048)
      return 0;

   texture                     = (gdi_texture_t*)calloc(1, sizeof(*texture));

   if (!texture)
      return 0;

   texture->width              = image->width;
   texture->height             = image->height;
   texture->active_width       = image->width;
   texture->active_height      = image->height;
   texture->type               = filter_type;
   total                       = (size_t)texture->width * (size_t)texture->height;
   texture->data               = calloc(1, total * sizeof(uint32_t));

   if (!texture->data)
   {
      free(texture);
      return 0;
   }

   /* Source pixels arrive in BGRA byte order (PNG -> ARGB32 little-
    * endian when supports_rgba is false, which is the GDI default).
    * AlphaBlend with AC_SRC_ALPHA needs *premultiplied* alpha source
    * pixels: do that conversion once here so the per-frame draw path
    * is a straight blit.  Also detect "no transparency anywhere" so
    * the draw path can use a faster opaque blit when appropriate. */
   src   = (const uint32_t*)image->pixels;
   dst   = (uint32_t*)texture->data;

   for (i = 0; i < total; i++)
   {
      uint32_t s  = src[i];
      uint8_t  sa = (uint8_t)((s >> 24) & 0xFF);
      uint8_t  sr = (uint8_t)((s >> 16) & 0xFF);
      uint8_t  sg = (uint8_t)((s >>  8) & 0xFF);
      uint8_t  sb = (uint8_t)( s        & 0xFF);
      uint8_t  pr, pg, pb;

      if (sa < 255)
         has_alpha = true;

      if (sa == 255)
      {
         pr = sr; pg = sg; pb = sb;
      }
      else if (sa == 0)
      {
         pr = pg = pb = 0;
      }
      else
      {
         pr = (uint8_t)GDI_DIV255((unsigned)sr * sa);
         pg = (uint8_t)GDI_DIV255((unsigned)sg * sa);
         pb = (uint8_t)GDI_DIV255((unsigned)sb * sa);
      }
      dst[i] = ((uint32_t)sa << 24)
             | ((uint32_t)pr << 16)
             | ((uint32_t)pg <<  8)
             |  (uint32_t)pb;
   }

   texture->has_alpha     = has_alpha;
   texture->premultiplied = true;

   return (uintptr_t)texture;
}

static void gdi_unload_texture(void *data,
      bool threaded, uintptr_t handle)
{
   struct gdi_texture *texture = (struct gdi_texture*)handle;

   if (!texture)
      return;

   if (texture->data)
      free(texture->data);

   if (texture->bmp)
   {
      DeleteObject(texture->bmp);
      texture->bmp = NULL;
   }

   free(texture);
}

static uint32_t gdi_get_flags(void *data) { return 0; }

#ifdef HAVE_GFX_WIDGETS
/* gfx_widgets is enabled whenever the driver can route the widget
 * draw calls through gfx_display_ctx_gdi.  We always can, regardless
 * of menu state, so this is unconditionally true.  AlphaBlend is
 * required for proper widget compositing - on pre-Win98 systems we
 * fall back to opaque blits, which is degraded but functional. */
static bool gdi_gfx_widgets_enabled(void *data) { (void)data; return true; }
#endif



static void gdi_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gdi_t *gdi = (gdi_t*)data;
   if (!gdi)
      return;
   gdi->keep_aspect   = true;
   gdi->should_resize = true;
}

static void gdi_apply_state_changes(void *data)
{
   gdi_t *gdi = (gdi_t*)data;
   if (gdi)
      gdi->should_resize = true;
}

static const video_poke_interface_t gdi_poke_interface = {
   gdi_get_flags,
   gdi_load_texture,
   gdi_unload_texture,
   gdi_set_video_mode,
   NULL, /* refresh_rate - handled by display server */
   NULL, /* set_filtering */
   NULL, /* video_output_size - handled by display server */
   NULL, /* get_video_output_prev - handled by display server */
   NULL, /* get_video_output_next - handled by display server */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   gdi_set_aspect_ratio,
   gdi_apply_state_changes,
   gdi_set_texture_frame,
   gdi_set_texture_enable,
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void gdi_get_poke_interface(void *data,
      const video_poke_interface_t **iface) { *iface = &gdi_poke_interface; }
/* Recompute the destination rect (gdi->vp.x/y/width/height) for
 * the core frame inside the window based on the current aspect
 * ratio settings.  Called from gdi_frame when should_resize is
 * set, mirroring d3d8 / d3d9 timing.  vp.full_width/full_height
 * must already hold the current window size (the caller refreshes
 * those via gfx_ctx_gdi_get_video_size first). */
static void gdi_set_viewport(void *data, unsigned vp_width, unsigned vp_height,
      bool force_full, bool allow_rotate)
{
   gdi_t *gdi = (gdi_t*)data;
   if (!gdi)
      return;

   gdi->vp.full_width  = vp_width;
   gdi->vp.full_height = vp_height;

   video_driver_update_viewport(&gdi->vp, force_full, gdi->keep_aspect, true);
}

/* Hand a snapshot of the current viewport back to the caller.
 *
 * Many subsystems call into video_driver_get_viewport_info() to
 * read viewport / window dimensions — most importantly the menu's
 * "Custom Aspect Ratio" handlers, which read vp.full_width /
 * vp.full_height to compute width-from-x / height-from-y for the
 * custom viewport.
 *
 * Without this hookup video_driver_get_viewport_info silently
 * returns false; that contract is now also strengthened on the
 * API side (the function zero-fills the output struct so callers
 * that ignore the return value at least read zeros rather than
 * stack garbage), but implementing it properly here is still the
 * right thing to do.  Mirrors d3d8_viewport_info /
 * d3d9_hlsl_viewport_info. */
static void gdi_viewport_info(void *data, struct video_viewport *vp)
{
   gdi_t *gdi = (gdi_t*)data;

   if (!gdi || !vp)
      return;

   vp->x           = gdi->vp.x;
   vp->y           = gdi->vp.y;
   vp->width       = gdi->vp.width;
   vp->height      = gdi->vp.height;
   vp->full_width  = gdi->vp.full_width;
   vp->full_height = gdi->vp.full_height;
}

#ifdef HAVE_OVERLAY
/*
 * INPUT OVERLAY DRIVER
 *
 * Implements video_overlay_interface_t for GDI.  The overlay
 * subsystem (input/input_overlay.c) hands us BGRA32 RGBA images
 * via load(), tells us where they go in 0..1 normalised space via
 * vertex_geom() / tex_geom(), and we draw them on top of the game
 * frame each frame using AlphaBlend.
 *
 * Storage: each loaded overlay is converted to a premultiplied
 * BGRA DIB section (HBITMAP) at load time so the per-frame draw
 * is a straight AlphaBlend with no pixel rewriting.  This mirrors
 * what gdi_load_texture does for menu/widget textures.
 *
 * Coordinate conventions (matching d3d8/d3d9):
 *   - vert_coords[0..3] = (x, y, w, h) in 0..1 normalised space.
 *     The setter flips y to (1.0f - y) and negates h, the same
 *     adjustment d3d8 makes for D3D's y-up convention.  We undo
 *     that flip at render time so the overlay lands the right way
 *     up in GDI's y-down space.
 *   - tex_coords[0..3] = (x, y, w, h) in 0..1 texture space.
 *     This driver doesn't currently sub-rect overlay textures —
 *     core libretro never seems to use anything other than the
 *     full image — but the field is preserved so the contract
 *     matches the other backends in case something does.
 *   - fullscreen=true: vert_coords span the entire window, including
 *     the letterbox/pillarbox bars.  This is what regular touch
 *     overlays use so the buttons keep working when the game is
 *     letterboxed.
 *   - fullscreen=false: vert_coords span only the game viewport.
 */
static void gdi_overlay_free(gdi_t *gdi)
{
   unsigned i;
   if (!gdi || !gdi->overlays)
      return;
   for (i = 0; i < gdi->overlays_size; i++)
   {
      if (gdi->overlays[i].bmp)
         DeleteObject(gdi->overlays[i].bmp);
   }
   free(gdi->overlays);
   gdi->overlays      = NULL;
   gdi->overlays_size = 0;
}

static bool gdi_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i;
   gdi_t                       *gdi = (gdi_t*)data;
   const struct texture_image *imgs = (const struct texture_image*)image_data;

   if (!gdi)
      return false;

   /* Drop any prior overlay set first.  load() is the install
    * point — input_overlay.c calls it once per overlay activation
    * with the full image array, never incrementally. */
   gdi_overlay_free(gdi);

   if (num_images == 0 || !imgs)
      return true;

   gdi->overlays = (struct gdi_overlay*)calloc(num_images,
         sizeof(*gdi->overlays));
   if (!gdi->overlays)
      return false;
   gdi->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      BITMAPINFO bmi;
      void              *bits = NULL;
      HBITMAP            bmp;
      const uint32_t    *src;
      uint32_t          *dst;
      size_t             j, total;
      struct gdi_overlay *o = &gdi->overlays[i];
      unsigned           w  = imgs[i].width;
      unsigned           h  = imgs[i].height;

      if (w == 0 || h == 0 || !imgs[i].pixels)
         continue;

      memset(&bmi, 0, sizeof(bmi));
      bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth       = (LONG)w;
      bmi.bmiHeader.biHeight      = -(LONG)h; /* top-down */
      bmi.bmiHeader.biPlanes      = 1;
      bmi.bmiHeader.biBitCount    = 32;
      bmi.bmiHeader.biCompression = BI_RGB;

      bmp = CreateDIBSection(gdi->memDC ? gdi->memDC : NULL,
            &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
      if (!bmp || !bits)
      {
         if (bmp)
            DeleteObject(bmp);
         continue;
      }

      /* Premultiply alpha at load: AlphaBlend with AC_SRC_ALPHA
       * needs premultiplied RGB or transparent pixels bleed
       * background colour through their fringes.  Source pixels
       * are 0xAARRGGBB (BGRA in memory order, the GDI default
       * when supports_rgba is false). */
      src   = imgs[i].pixels;
      dst   = (uint32_t*)bits;
      total = (size_t)w * (size_t)h;
      for (j = 0; j < total; j++)
      {
         uint32_t s  = src[j];
         uint8_t  sa = (uint8_t)((s >> 24) & 0xFF);
         uint8_t  sr = (uint8_t)((s >> 16) & 0xFF);
         uint8_t  sg = (uint8_t)((s >>  8) & 0xFF);
         uint8_t  sb = (uint8_t)( s        & 0xFF);
         uint8_t  pr, pg, pb;

         if (sa == 255)      { pr = sr; pg = sg; pb = sb; }
         else if (sa == 0)   { pr = pg = pb = 0; }
         else
         {
            pr = (uint8_t)GDI_DIV255((unsigned)sr * sa);
            pg = (uint8_t)GDI_DIV255((unsigned)sg * sa);
            pb = (uint8_t)GDI_DIV255((unsigned)sb * sa);
         }
         dst[j] = ((uint32_t)sa << 24)
                | ((uint32_t)pr << 16)
                | ((uint32_t)pg <<  8)
                |  (uint32_t)pb;
      }

      o->bmp           = bmp;
      o->tex_w         = w;
      o->tex_h         = h;
      o->alpha_mod     = 1.0f;
      o->fullscreen    = false;
      /* Stretch to the full target rect by default.  The overlay
       * descriptor drives subsequent vertex_geom calls before
       * anything is actually drawn. */
      o->tex_coords[0]  = 0.0f;
      o->tex_coords[1]  = 0.0f;
      o->tex_coords[2]  = 1.0f;
      o->tex_coords[3]  = 1.0f;
      o->vert_coords[0] = 0.0f;
      o->vert_coords[1] = 0.0f;
      o->vert_coords[2] = 1.0f;
      o->vert_coords[3] = 1.0f;
   }

   return true;
}

static void gdi_overlay_tex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   gdi_t *gdi = (gdi_t*)data;
   if (!gdi || index >= gdi->overlays_size)
      return;
   gdi->overlays[index].tex_coords[0] = x;
   gdi->overlays[index].tex_coords[1] = y;
   gdi->overlays[index].tex_coords[2] = w;
   gdi->overlays[index].tex_coords[3] = h;
}

static void gdi_overlay_vertex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   gdi_t *gdi = (gdi_t*)data;
   if (!gdi || index >= gdi->overlays_size)
      return;
   /* Overlay descriptor coordinates from input_overlay are already
    * y-down (y=0 top of screen, y=1 bottom — same convention as
    * RETRO_DEVICE_POINTER which the hit-test path uses) and (x, y)
    * names the top-left corner of the rect.  GDI's screen space is
    * also y-down with the same origin, so we store the values
    * verbatim and the render path does a direct multiply.
    *
    * d3d8 / d3d9 / gl all flip y here (y = 1.0f - y; h = -h;)
    * because their pipelines emit vertices in y-up clip space and
    * rely on the viewport transform to flip back to screen.  GDI
    * has no such pipeline — every draw is a pixel-space blit
    * straight to a DC — so the flip is a bug for us, not a
    * compatibility shim. */
   gdi->overlays[index].vert_coords[0] = x;
   gdi->overlays[index].vert_coords[1] = y;
   gdi->overlays[index].vert_coords[2] = w;
   gdi->overlays[index].vert_coords[3] = h;
}

static void gdi_overlay_enable(void *data, bool state)
{
   gdi_t *gdi = (gdi_t*)data;
   if (!gdi)
      return;
   gdi->overlays_enabled = state;
}

static void gdi_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   gdi_t *gdi = (gdi_t*)data;
   if (!gdi || !gdi->overlays)
      return;
   for (i = 0; i < gdi->overlays_size; i++)
      gdi->overlays[i].fullscreen = enable;
}

static void gdi_overlay_set_alpha(void *data, unsigned index, float mod)
{
   gdi_t *gdi = (gdi_t*)data;
   if (!gdi || index >= gdi->overlays_size)
      return;
   gdi->overlays[index].alpha_mod = mod;
}

/* Composite all enabled overlays onto the currently selected DC.
 * Caller has already SelectObject'd bmp_menu (or whatever the
 * active target is) into gdi->memDC.  Each overlay is alpha-
 * blended on top of whatever's already there.
 *
 * Coordinate space:
 *   - fullscreen overlay → vert_coords span (0,0,W,H) where W,H is
 *     the window size, so buttons drawn outside the game viewport
 *     (e.g. on the pillar bars) still hit-test correctly.
 *   - non-fullscreen overlay → vert_coords span the game viewport
 *     rect (gdi->vp.x/y, gdi->vp.width/height).
 *
 * vert_coords[1] / [3] hold the d3d8-style flipped y / negative
 * h; we recompute the rect's top-left and absolute size locally
 * so the math stays straightforward. */
static void gdi_overlays_render(gdi_t *gdi,
      unsigned surface_width, unsigned surface_height)
{
#if GDI_HAS_ALPHABLEND
   unsigned i;

   if (!gdi || !gdi->overlays || !gdi->overlays_enabled
         || !gdi->memDC)
      return;

   if (!gdi->texDC)
      gdi->texDC = CreateCompatibleDC(gdi->winDC);
   if (!gdi->texDC)
      return;

   for (i = 0; i < gdi->overlays_size; i++)
   {
      BLENDFUNCTION blend;
      HBITMAP            tex_old;
      struct gdi_overlay *o = &gdi->overlays[i];
      float vx, vy, vw, vh;
      int    base_x, base_y;
      unsigned base_w, base_h;
      int    dst_x, dst_y;
      int    dst_w, dst_h;
      unsigned alpha_byte;

      if (!o->bmp || o->tex_w == 0 || o->tex_h == 0)
         continue;
      if (o->alpha_mod <= 0.0f)
         continue;

      /* Direct mapping from y-down 0..1 normalized space to
       * y-down screen pixels.  vert_coords[0..3] = (x, y, w, h)
       * with (x, y) = top-left corner of the rect. */
      vx = o->vert_coords[0];
      vy = o->vert_coords[1];
      vw = o->vert_coords[2];
      vh = o->vert_coords[3];

      if (o->fullscreen)
      {
         base_x = 0;
         base_y = 0;
         base_w = surface_width;
         base_h = surface_height;
      }
      else
      {
         base_x = gdi->vp.x;
         base_y = gdi->vp.y;
         base_w = gdi->vp.width  ? gdi->vp.width  : surface_width;
         base_h = gdi->vp.height ? gdi->vp.height : surface_height;
      }

      dst_x = base_x + (int)(vx * (float)base_w + 0.5f);
      dst_y = base_y + (int)(vy * (float)base_h + 0.5f);
      dst_w = (int)(vw * (float)base_w + 0.5f);
      dst_h = (int)(vh * (float)base_h + 0.5f);
      if (dst_w <= 0 || dst_h <= 0)
         continue;

      alpha_byte = (unsigned)(o->alpha_mod * 255.0f);
      if (alpha_byte > 255)
         alpha_byte = 255;

      /* The overlay's own per-pixel alpha was premultiplied at
       * load.  alpha_mod is applied as SourceConstantAlpha so the
       * driver multiplies through the whole image uniformly —
       * AC_SRC_ALPHA + SourceConstantAlpha together give the
       * "premultiplied source modulated by a constant" semantics
       * the input overlay subsystem expects from set_alpha. */
      blend.BlendOp             = AC_SRC_OVER;
      blend.BlendFlags          = 0;
      blend.SourceConstantAlpha = (BYTE)alpha_byte;
      blend.AlphaFormat         = AC_SRC_ALPHA;

      tex_old = (HBITMAP)SelectObject(gdi->texDC, o->bmp);
      AlphaBlend(gdi->memDC,
            dst_x, dst_y, dst_w, dst_h,
            gdi->texDC,
            0, 0, o->tex_w, o->tex_h, blend);
      SelectObject(gdi->texDC, tex_old);
   }
#else
   (void)gdi;
   (void)surface_width;
   (void)surface_height;
#endif
}

static const video_overlay_interface_t gdi_overlay_interface = {
   gdi_overlay_enable,
   gdi_overlay_load,
   gdi_overlay_tex_geom,
   gdi_overlay_vertex_geom,
   gdi_overlay_full_screen,
   gdi_overlay_set_alpha,
};

static void gdi_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gdi_overlay_interface;
}
#endif

video_driver_t video_gdi = {
   gdi_init,
   gdi_frame,
   gdi_set_nonblock_state,
   gdi_alive,
   gdi_focus,
   gdi_suppress_screensaver,
   gdi_has_windowed,
   gdi_set_shader,
   gdi_free,
   "gdi",
   gdi_set_viewport,
   NULL, /* set_rotation */
   gdi_viewport_info,
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   gdi_get_overlay_interface,
#endif
   gdi_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   gdi_gfx_widgets_enabled
#endif
};
