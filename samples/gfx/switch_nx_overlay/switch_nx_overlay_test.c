/* The software Switch video driver's input overlay, as
 * gfx/drivers/switch_nx_gfx.c ships: the driver is included below,
 * against the libnx stand-in headers the compile matrix uses, and
 * draws into a host framebuffer of the console's 1280x720.
 *
 * Lanes: a page copied at load (the pack's pixels freed after it), the
 * frame or the whole screen as the base, per-image alpha blended
 * exactly, per-texel alpha and sub-rectangles of the image, the
 * memory-order RGBA images, the menu with the overlay over or behind
 * it, the bilinear mode's scaled-up window, and the page freed. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../../gfx/drivers/switch_nx_gfx.c"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; \
   printf("[FAIL] " __VA_ARGS__); printf("\n"); } } while (0)

#define FB_W 1280
#define FB_H 720

/* --- the console ------------------------------------------------- */

static uint32_t fb_mem[FB_W * FB_H];
static unsigned win_w, win_h;

NWindow *nwindowGetDefault(void) { return (NWindow*)&win_w; }
Result nwindowSetDimensions(NWindow *nw, u32 width, u32 height)
{
   (void)nw;
   win_w = width;
   win_h = height;
   return 0;
}
Result framebufferCreate(Framebuffer *fb, NWindow *win,
      u32 width, u32 height, u32 format, u32 num_fbs)
{
   (void)fb; (void)win; (void)width; (void)height; (void)format;
   (void)num_fbs;
   return 0;
}
Result framebufferMakeLinear(Framebuffer *fb) { (void)fb; return 0; }
void *framebufferBegin(Framebuffer *fb, u32 *out_stride)
{
   (void)fb;
   *out_stride = FB_W * sizeof(uint32_t);
   return fb_mem;
}
void framebufferEnd(Framebuffer *fb) { (void)fb; }
void framebufferClose(Framebuffer *fb) { (void)fb; }

/* --- the frontend ------------------------------------------------ */

uint64_t cpu_features_get(void) { return 0; }

void video_driver_update_viewport(struct video_viewport *vp,
      bool force_full, bool keep_aspect, bool y_down)
{
   (void)vp; (void)force_full; (void)keep_aspect; (void)y_down;
}
void video_driver_set_viewport_core(void) { }
void input_driver_video_window(enum input_window_kind window,
      void *window_data)
{
   (void)window; (void)window_data;
}
void menu_driver_frame(bool menu_is_alive, video_frame_info_t *video_info)
{
   (void)menu_is_alive; (void)video_info;
}
void font_driver_render_msg(void *data, const char *msg, size_t msg_len,
      const struct font_params *params, void *font_data)
{
   (void)data; (void)msg; (void)msg_len; (void)params; (void)font_data;
}
void font_driver_resolve_params(const struct font_params *params,
      font_params_resolved_t *out)
{
   (void)params;
   memset(out, 0, sizeof(*out));
}
int font_renderer_create_default(const font_renderer_driver_t **drv,
      void **handle, const char *font_path, unsigned font_size,
      enum font_atlas_format fmt)
{
   (void)drv; (void)handle; (void)font_path; (void)font_size; (void)fmt;
   return 0;
}
int font_renderer_get_message_width(const font_renderer_driver_t *renderer,
      void *renderer_data, const char *msg, size_t msg_len, float scale)
{
   (void)renderer; (void)renderer_data; (void)msg; (void)msg_len;
   (void)scale;
   return 0;
}
uint32_t utf8_walk(const char **string)
{
   return (uint32_t)(unsigned char)*(*string)++;
}

/* --- helpers ----------------------------------------------------- */

#define OV_RED   0x0000ffu   /* R in the low byte, as the framebuffer */
#define OV_WHITE 0xffffffu

static const video_overlay_interface_t *iface;
static const video_poke_interface_t   *sw_poke;
static uint32_t core_frame[640 * 360];
static video_frame_info_t vfi;

static switch_video_t *driver_new(unsigned w, unsigned h, bool smooth)
{
   video_info_t info;
   switch_video_t *sw;
   memset(&info, 0, sizeof(info));
   info.dims   = VIDEO_SCALE_PACK(w, h);
   info.rgb32  = true;
   info.smooth = smooth;
   sw          = (switch_video_t*)video_switch.init(&info);
   video_switch.overlay_interface(sw, &iface);
   return sw;
}

static void frame(switch_video_t *sw, unsigned w, unsigned h)
{
   size_t i;
   for (i = 0; i < (size_t)w * h; i++)
      core_frame[i] = 0xff102030u;
   memset(fb_mem, 0, sizeof(fb_mem));
   video_switch.frame(sw, core_frame, VIDEO_SCALE_PACK(w, h), 1,
         w * sizeof(uint32_t), NULL, &vfi);
}

static uint32_t at(int x, int y)
{
   return fb_mem[(size_t)y * FB_W + x] & 0xffffffu;
}

/* One image of @w x @h of ARGB words, the pack's to free */
static struct texture_image image(unsigned w, unsigned h, uint32_t argb)
{
   struct texture_image img;
   size_t i;
   memset(&img, 0, sizeof(img));
   img.width  = w;
   img.height = h;
   img.pixels = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
   for (i = 0; i < (size_t)w * h; i++)
      img.pixels[i] = argb;
   return img;
}

static void load_one(switch_video_t *sw, struct texture_image *img)
{
   CHECK(iface->load(sw, img, 1), "load refused");
   /* The pack's pixels are not the driver's: gone after the load */
   free(img->pixels);
   img->pixels = NULL;
}

/* x / 255 rounded, per channel, as the reference the blend must hit */
static uint32_t blend_ref(uint32_t s, uint32_t d, unsigned a)
{
   uint32_t out = 0;
   unsigned c;
   for (c = 0; c < 24; c += 8)
   {
      unsigned sv = (s >> c) & 0xffu;
      unsigned dv = (d >> c) & 0xffu;
      out |= (uint32_t)((sv * a + dv * (255u - a) + 127u) / 255u) << c;
   }
   return out;
}

/* --- lanes ------------------------------------------------------- */

/* The frame's viewport is the base: a 640x360 core at its own size
 * sits at (320, 180), so (0.25, 0.5, 0.25, 0.25) is (480, 360) for
 * 160x90 */
static void lane_viewport(void)
{
   int before = failures;
   uint32_t bg;
   struct texture_image img = image(4, 4, 0xffff0000u);
   switch_video_t *sw       = driver_new(640, 360, false);

   load_one(sw, &img);
   iface->vertex_geom(sw, 0, 0.25f, 0.5f, 0.25f, 0.25f);
   frame(sw, 640, 360);
   CHECK(at(480 + 80, 360 + 45) != OV_RED, "drawn while not enabled");

   iface->enable(sw, true);
   frame(sw, 640, 360);
   bg = at(400, 300);
   CHECK(bg != OV_RED, "the core frame is red");
   CHECK(at(480, 360) == OV_RED && at(639, 449) == OV_RED
         && at(560, 405) == OV_RED, "the image does not fill its rectangle");
   CHECK(at(479, 360) == bg && at(640, 449) == bg
         && at(480, 359) == bg && at(480, 450) == bg,
         "drawn past its rectangle");

   /* the whole screen, not the frame */
   iface->full_screen(sw, true);
   frame(sw, 640, 360);
   CHECK(at(320, 360) == OV_RED && at(639, 539) == OV_RED,
         "full screen: the rectangle is not the screen's");
   CHECK(at(319, 360) != OV_RED && at(640, 539) != OV_RED && at(320, 540) != OV_RED,
         "full screen: drawn past its rectangle");

   video_switch.free(sw);
   printf("[%s] viewport and full-screen bases\n",
         failures == before ? "pass" : "fail");
}

/* Per-image alpha is blended exactly; a texel's own alpha too, and a
 * sub-rectangle of the image samples only its texels */
static void lane_alpha(void)
{
   int before = failures;
   uint32_t bg;
   unsigned a;
   struct texture_image img = image(4, 4, 0xffff0000u);
   struct texture_image two;
   switch_video_t *sw       = driver_new(640, 360, false);

   load_one(sw, &img);
   iface->enable(sw, true);
   iface->vertex_geom(sw, 0, 0.0f, 0.0f, 0.5f, 0.5f);
   frame(sw, 640, 360);
   bg = at(321 + 400, 181 + 200);
   iface->set_alpha(sw, 0, 0.5f);
   frame(sw, 640, 360);
   a  = (unsigned)(0.5f * 255.0f + 0.5f);
   CHECK(at(330, 190) == blend_ref(OV_RED, bg, a),
         "alpha 0.5: %06x, not %06x", (unsigned)at(330, 190),
         (unsigned)blend_ref(OV_RED, bg, a));
   iface->set_alpha(sw, 0, 0.0f);
   frame(sw, 640, 360);
   CHECK(at(330, 190) == bg, "alpha 0: drawn");

   /* left texel half white, right one clear */
   two           = image(2, 1, 0);
   two.pixels[0] = 0x80ffffffu;
   load_one(sw, &two);
   iface->vertex_geom(sw, 0, 0.0f, 0.0f, 0.5f, 0.5f);
   iface->tex_geom(sw, 0, 0.0f, 0.0f, 0.5f, 1.0f);
   frame(sw, 640, 360);
   CHECK(at(330, 190) == blend_ref(OV_WHITE, bg, 0x80)
         && at(639, 359) == blend_ref(OV_WHITE, bg, 0x80),
         "left half: not the half-white texel everywhere");
   iface->tex_geom(sw, 0, 0.5f, 0.0f, 0.5f, 1.0f);
   frame(sw, 640, 360);
   CHECK(at(330, 190) == bg && at(639, 359) == bg,
         "right half: the clear texel drew");

   video_switch.free(sw);
   printf("[%s] per-image and per-texel alpha, sub-rectangles\n",
         failures == before ? "pass" : "fail");
}

/* An image decoded in memory order R, G, B, A is not swizzled */
static void lane_rgba(void)
{
   int before = failures;
   struct texture_image img = image(2, 2, 0xff0000ffu);
   switch_video_t *sw       = driver_new(640, 360, false);
   img.supports_rgba        = true;
   load_one(sw, &img);
   iface->enable(sw, true);
   frame(sw, 640, 360);
   CHECK(at(330, 190) == OV_RED, "RGBA image drawn as %06x",
         (unsigned)at(330, 190));
   video_switch.free(sw);
   printf("[%s] memory-order RGBA images\n",
         failures == before ? "pass" : "fail");
}

/* Over the menu, unless the overlay goes behind it */
static void lane_menu(void)
{
   int before = failures;
   static uint16_t menu[320 * 240];
   struct texture_image img = image(4, 4, 0xffff0000u);
   switch_video_t *sw       = driver_new(640, 360, false);
   uint32_t menu_px;

   load_one(sw, &img);
   iface->enable(sw, true);
   iface->full_screen(sw, true);
   memset(menu, 0x55, sizeof(menu));
   video_switch.poke_interface(sw, &sw_poke);
   sw_poke->set_texture_frame(sw, menu, false,
         VIDEO_SCALE_PACK(320, 240), 1.0f);
   sw_poke->set_texture_enable(sw, true, false);
   vfi.menu_st_flags = MENU_ST_FLAG_ALIVE;

   vfi.overlay_behind_menu = true;
   frame(sw, 640, 360);
   menu_px = at(640, 360);
   CHECK(menu_px != OV_RED, "behind the menu: drawn over it");
   vfi.overlay_behind_menu = false;
   frame(sw, 640, 360);
   CHECK(at(640, 360) == OV_RED, "over the menu: not drawn");

   vfi.menu_st_flags = 0;
   sw_poke->set_texture_enable(sw, false, false);
   video_switch.free(sw);
   printf("[%s] over and behind the menu\n",
         failures == before ? "pass" : "fail");
}

/* Bilinear mode: the frame is scaled up by the compositor from the
 * framebuffer's top left, so the overlay is drawn there at the frame's
 * size, beside it at x_offset */
static void lane_smooth(void)
{
   int before = failures;
   int x0, w, h;
   struct texture_image img = image(4, 4, 0xffff0000u);
   switch_video_t *sw       = driver_new(320, 240, true);

   load_one(sw, &img);
   iface->enable(sw, true);
   frame(sw, 320, 240);
   x0 = (int)sw->hw_scale.x_offset;
   w  = (int)sw->scaler.out_width;
   h  = (int)sw->scaler.out_height;
   CHECK(x0 > 0 && w == 320 && h == 240, "smooth: the window is %d+%dx%d",
         x0, w, h);
   CHECK(at(x0, 0) == OV_RED && at(x0 + w - 1, h - 1) == OV_RED,
         "smooth: the frame's rectangle is not covered");
   CHECK(at(x0 - 1, 0) != OV_RED && at(x0 + w, 0) != OV_RED
         && at(x0, h) != OV_RED, "smooth: drawn outside the frame");

   iface->full_screen(sw, true);
   frame(sw, 320, 240);
   CHECK(at(0, 0) == OV_RED
         && at((int)VIDEO_SCALE_W(sw->hw_scale.dims) - 1, h - 1) == OV_RED
         && at((int)VIDEO_SCALE_W(sw->hw_scale.dims), 0) != OV_RED,
         "smooth, full screen: not the window");

   video_switch.free(sw);
   printf("[%s] bilinear mode's window\n",
         failures == before ? "pass" : "fail");
}

/* A page with no images, setters past the page, a page freed */
static void lane_pages(void)
{
   int before = failures;
   struct texture_image imgs[2];
   switch_video_t *sw = driver_new(640, 360, false);

   imgs[0] = image(4, 4, 0xffff0000u);
   imgs[1] = image(8, 2, 0x80ffffffu);
   CHECK(iface->load(sw, imgs, 2) && sw->overlays_size == 2,
         "a page of two not loaded");
   free(imgs[0].pixels);
   free(imgs[1].pixels);
   iface->vertex_geom(sw, 2, 0, 0, 1, 1);
   iface->tex_geom(sw, 7, 0, 0, 1, 1);
   iface->set_alpha(sw, 9, 0.5f);
   CHECK(!sw->overlay_enabled, "a load enabled the overlay");
   CHECK(iface->load(sw, NULL, 0) && !sw->overlays
         && !sw->overlays_size, "an empty page left images behind");
   iface->enable(sw, true);
   frame(sw, 640, 360);
   video_switch.free(sw);
   printf("[%s] pages\n", failures == before ? "pass" : "fail");
}

/* What a frame's blend costs: a full-screen image of half-transparent
 * texels, the worst case, over the 1280x720 framebuffer */
static void lane_cost(void)
{
   int i;
   clock_t t0;
   double us;
   struct texture_image img = image(256, 144, 0x80336699u);
   switch_video_t *sw       = driver_new(640, 360, false);
   load_one(sw, &img);
   iface->enable(sw, true);
   iface->full_screen(sw, true);
   t0 = clock();
   for (i = 0; i < 50; i++)
      switch_overlay_render(sw, fb_mem, FB_W, sw->vp.full_dims,
            sw->vp.pos, sw->vp.dims);
   us = (double)(clock() - t0) * 1e6 / CLOCKS_PER_SEC / 50.0;
   printf("[info] full-screen blend: %.0f us a frame on this host\n", us);
   video_switch.free(sw);
}

int main(void)
{
   lane_viewport();
   lane_alpha();
   lane_rgba();
   lane_menu();
   lane_smooth();
   lane_pages();
   lane_cost();
   printf("%s switch_nx_overlay_test\n", failures ? "FAIL" : "PASS");
   return failures ? 1 : 0;
}
