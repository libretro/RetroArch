/* Graphics checks, run in Dolphin's software renderer by
 * os/gekko/tests/run-dolphin.sh <elf> 90 sw: each case draws into the
 * EFB, copies to an XFB and reads the result back as YUV. */

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <gekko/gx.h>
#include <gekko/power.h>
#include <gekko/thread.h>
#include <gekko/video.h>

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s", what); \
         failures++; \
      } \
   } while (0)

static gk_vi_mode_t mode;
static uint32_t    *xfb;

/* ---- reading the XFB ---- */

static void rgb_to_yuv(uint32_t rgba, int *y, int *u, int *v)
{
   int r = (int)(rgba >> 24), g = (int)((rgba >> 16) & 0xff);
   int b = (int)((rgba >> 8) & 0xff);
   *y = 16  + (( 66 * r + 129 * g +  25 * b + 128) >> 8);
   *u = 128 + ((-38 * r -  74 * g + 112 * b + 128) >> 8);
   *v = 128 + ((112 * r -  94 * g -  18 * b + 128) >> 8);
}

static int pixel_is(unsigned x, unsigned y, uint32_t rgba, int tol)
{
   uint32_t w = xfb[(y * mode.fb_width + (x & ~1u)) / 2];
   int py = (int)((x & 1) ? (w >> 8) & 0xff : w >> 24);
   int pu = (int)((w >> 16) & 0xff), pv = (int)(w & 0xff);
   int ey, eu, ev;
   rgb_to_yuv(rgba, &ey, &eu, &ev);
   if (     py - ey > tol || ey - py > tol
         || pu - eu > tol || eu - pu > tol
         || pv - ev > tol || ev - pv > tol)
   {
      gk_debug_printf("     (%u,%u): yuv %d %d %d, want %d %d %d",
            x, y, py, pu, pv, ey, eu, ev);
      return 0;
   }
   return 1;
}

static void present(void)
{
   gk_gx_copy_xfb(xfb, mode.fb_width, 0, 0, 640, 480, 480, 1);
   gk_gx_draw_done();
   gk_dcache_invalidate(xfb, (size_t)mode.fb_width * mode.fb_lines * 2);
}

/* ---- drawing ---- */

static void setup_2d(void)
{
   gk_mtx44 p;
   gk_mtx34 m;
   gk_mtx_ortho(p, 0, 480, 0, 640, 0, 1);
   gk_gx_projection(p, 1);
   gk_mtx_identity(m);
   gk_gx_load_pos_mtx(m, 0);
   gk_gx_current_mtx(0);
   gk_gx_viewport(0, 0, 640, 480, 0, 1, 0);
   gk_gx_vtx_clear();
   gk_gx_vtx_desc(GK_GX_POS, GK_GX_DIRECT);
   gk_gx_vtx_desc(GK_GX_CLR0, GK_GX_DIRECT);
   gk_gx_vtx_fmt(0, GK_GX_POS, 3, GK_GX_F32, 0);
   gk_gx_vtx_fmt(0, GK_GX_CLR0, 4, GK_GX_RGBA8, 0);
   gk_gx_vtx_fmt(0, GK_GX_TEX0, 2, GK_GX_F32, 0);
   gk_gx_num_texgens(0);
   gk_gx_tev_order(0, 0, -1);
   gk_gx_tev(0, GK_GX_PASSCLR);
   gk_gx_blend(0, GK_GX_BL_ONE, GK_GX_BL_ZERO);
}

static void quad(float x, float y, float w, float h, uint32_t rgba)
{
   gk_gx_begin(GK_GX_QUADS, 0, 4);
   gk_gx_f32(x);     gk_gx_f32(y);     gk_gx_f32(0); gk_gx_u32(rgba);
   gk_gx_f32(x + w); gk_gx_f32(y);     gk_gx_f32(0); gk_gx_u32(rgba);
   gk_gx_f32(x + w); gk_gx_f32(y + h); gk_gx_f32(0); gk_gx_u32(rgba);
   gk_gx_f32(x);     gk_gx_f32(y + h); gk_gx_f32(0); gk_gx_u32(rgba);
}

static void test_clear_and_quads(void)
{
   gk_gx_copy_clear(0x202080ffu, 0xffffff);
   present();   /* clears the EFB behind the copy */
   quad(40, 40, 200, 100, 0xff0000ffu);
   quad(300, 40, 200, 100, 0x00ff00ffu);
   quad(40, 300, 200, 100, 0xffffffffu);
   present();
   CHECK(pixel_is(320, 240, 0x202080ffu, 4), "clear colour");
   CHECK(pixel_is(100, 80, 0xff0000ffu, 4)
         && pixel_is(400, 80, 0x00ff00ffu, 4)
         && pixel_is(100, 350, 0xffffffffu, 4),
         "flat quads in the right places and colours");
   CHECK(pixel_is(38, 80, 0x202080ffu, 4) && pixel_is(242, 80, 0x202080ffu, 4),
         "quad edges where the projection puts them");
}

static void test_blend(void)
{
   gk_gx_copy_clear(0x000000ffu, 0xffffff);
   present();
   gk_gx_blend(1, GK_GX_BL_SRCALPHA, GK_GX_BL_INVSRCALPHA);
   quad(100, 100, 200, 200, 0xffffff80u);
   gk_gx_blend(0, GK_GX_BL_ONE, GK_GX_BL_ZERO);
   present();
   CHECK(pixel_is(200, 200, 0x808080ffu, 6), "alpha blending over black");
}

/* 8x8 RGB565 in 4x4 tiles: left half red, right half blue. */
static void test_texture(void)
{
   static uint16_t tex[64] __attribute__((aligned(32)));
   gk_gx_tex_t t;
   unsigned tile, i;
   for (tile = 0; tile < 4; tile++)
      for (i = 0; i < 16; i++)
         tex[tile * 16 + i] = (tile & 1) ? 0x001f : 0xf800;
   gk_dcache_flush(tex, sizeof(tex));
   gk_gx_invalidate_tex();

   gk_gx_copy_clear(0x000000ffu, 0xffffff);
   present();
   gk_gx_tex_init(&t, tex, 8, 8, GK_GX_TF_RGB565, GK_GX_CLAMP, GK_GX_CLAMP);
   gk_gx_tex_filter(&t, GK_GX_NEAR, GK_GX_NEAR);
   gk_gx_tex_load(&t, 0, 0);
   gk_gx_vtx_desc(GK_GX_TEX0, GK_GX_DIRECT);
   gk_gx_num_texgens(1);
   gk_gx_tev_order(0, 0, 0);
   gk_gx_tev(0, GK_GX_MODULATE);
   gk_gx_begin(GK_GX_QUADS, 0, 4);
   gk_gx_f32(160); gk_gx_f32(120); gk_gx_f32(0); gk_gx_u32(0xffffffffu);
   gk_gx_f32(0);   gk_gx_f32(0);
   gk_gx_f32(480); gk_gx_f32(120); gk_gx_f32(0); gk_gx_u32(0xffffffffu);
   gk_gx_f32(1);   gk_gx_f32(0);
   gk_gx_f32(480); gk_gx_f32(360); gk_gx_f32(0); gk_gx_u32(0xffffffffu);
   gk_gx_f32(1);   gk_gx_f32(1);
   gk_gx_f32(160); gk_gx_f32(360); gk_gx_f32(0); gk_gx_u32(0xffffffffu);
   gk_gx_f32(0);   gk_gx_f32(1);
   present();
   CHECK(pixel_is(200, 240, 0xff0000ffu, 6) && pixel_is(440, 240, 0x0000ffffu, 6),
         "RGB565 texture, tiled, sampled through texcoord 0");
   setup_2d();
}

static void test_indexed(void)
{
   static float    pos[4 * 3] __attribute__((aligned(32))) = {
      300, 300, 0,  400, 300, 0,  400, 400, 0,  300, 400, 0 };
   static uint32_t clr[1] __attribute__((aligned(32))) = { 0xffff00ffu };
   unsigned i;
   gk_dcache_flush(pos, sizeof(pos));
   gk_dcache_flush(clr, sizeof(clr));
   gk_gx_copy_clear(0x000000ffu, 0xffffff);
   present();
   gk_gx_vtx_clear();
   gk_gx_vtx_desc(GK_GX_POS, GK_GX_INDEX8);
   gk_gx_vtx_desc(GK_GX_CLR0, GK_GX_INDEX8);
   gk_gx_array(GK_GX_POS, pos, 12);
   gk_gx_array(GK_GX_CLR0, clr, 4);
   gk_gx_invalidate_vtx_cache();
   gk_gx_begin(GK_GX_QUADS, 0, 4);
   for (i = 0; i < 4; i++)
   {
      gk_gx_u8((uint8_t)i);
      gk_gx_u8(0);
   }
   present();
   CHECK(pixel_is(350, 350, 0xffff00ffu, 6), "indexed vertex arrays");
   setup_2d();
}

static void test_list(void)
{
   static uint8_t list[256] __attribute__((aligned(32)));
   size_t size;
   gk_gx_copy_clear(0x000000ffu, 0xffffff);
   present();
   gk_gx_list_begin(list, sizeof(list));
   quad(500, 300, 100, 100, 0x00ffffffu);
   size = gk_gx_list_end();
   CHECK(size > 0 && size < sizeof(list), "display list recorded");
   gk_gx_list_call(list, size);
   gk_gx_list_call(list, size);
   present();
   CHECK(pixel_is(550, 350, 0x00ffffffu, 6), "display list replayed");
}

/* Far more commands than the ring holds: the writer has to wait for
 * the GPU on the high-water mark and resume on the low one. */
static void test_overflow(void)
{
   unsigned i;
   gk_gx_copy_clear(0x000000ffu, 0xffffff);
   present();
   for (i = 0; i < 40000; i++)
      quad((float)(i % 600), 10, 4, 4, (i == 39999) ? 0xff00ffffu
            : 0x404040ffu);
   quad(600, 200, 20, 20, 0xff00ffffu);
   present();
   CHECK(pixel_is(610, 210, 0xff00ffffu, 6),
         "3 MB of commands through a 64 KB ring");
}

static void test_scaled_copy(void)
{
   gk_gx_copy_clear(0x000000ffu, 0xffffff);
   present();
   quad(0, 200, 640, 40, 0xffffffffu);   /* EFB lines 200-239 */
   gk_gx_copy_xfb(xfb, mode.fb_width, 0, 0, 640, 240, 480, 1);
   gk_gx_draw_done();
   gk_dcache_invalidate(xfb, (size_t)mode.fb_width * mode.fb_lines * 2);
   CHECK(pixel_is(320, 440, 0xffffffffu, 10) && pixel_is(320, 380, 0x000000ffu, 10),
         "copy scaled 240 lines to 480");
}

int main(int argc, char **argv)
{
   (void)argc;
   (void)argv;
   gk_vi_preferred(&mode);
   mode.std  = GK_VI_NTSC;
   mode.scan = GK_VI_INTERLACED;
   mode.lines = mode.fb_lines = 480;
   gk_vi_configure(&mode);
   xfb = (uint32_t*)gk_vi_alloc_fb(&mode);
   gk_vi_clear_fb(xfb, &mode, GK_YUYV_BLACK);
   gk_vi_set_fb(xfb);
   gk_vi_set_black(0);

   CHECK(gk_gx_init(65536) == 0, "init with a 64 KB ring");
   setup_2d();
   test_clear_and_quads();
   test_blend();
   test_texture();
   test_indexed();
   test_list();
   test_overflow();
   test_scaled_copy();

   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   gk_sleep_us(300000);
#ifdef HW_RVL
   gk_power_off();
#endif
   return 0;
}
