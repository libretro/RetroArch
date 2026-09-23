/* Contracts gfx/drivers/psp1_gfx.c has to hold, run against the real
 * driver on a stand-in for the hardware (fake_psp.h).
 *
 * The PSP video driver is compiled by no desktop job and runs on a
 * machine with an uncached alias, a GE that reads display lists and
 * writes memory on its own, and a vblank handler in interrupt context.
 * Each of those is a contract the file can break without any build
 * noticing, so each gets a test here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "fake_psp.h"

static int failures;
static int at_entry;
static const char *current;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) { \
         printf("  FAIL %s: ", current); \
         printf(__VA_ARGS__); \
         printf("\n        (%s:%d)\n", __FILE__, __LINE__); \
         failures++; \
      } \
   } while (0)

static void begin(const char *name)
{
   current  = name;
   at_entry = failures;
   psp1_fake_reset();
}

static void done(void)
{
   if (failures == at_entry)
      printf("  ok   %s\n", current);
}

/* ------------------------------------------------------------ utilities */

static video_info_t   vinfo;
static video_frame_info_t finfo;

static void *driver_init(void)
{
   memset(&vinfo, 0, sizeof(vinfo));
   vinfo.vsync = true;
   vinfo.rgb32 = false;
   vinfo.smooth = false;
   return video_psp1.init(&vinfo, NULL, NULL);
}

static void driver_frame(void *psp, const void *frame, unsigned w,
      unsigned h, unsigned pitch, const char *msg)
{
   memset(&finfo, 0, sizeof(finfo));
   video_psp1.frame(psp, frame, VIDEO_SCALE_PACK(w, h), 0, pitch, msg,
         &finfo);
}

static const video_poke_interface_t *poke(void *psp)
{
   const video_poke_interface_t *iface = NULL;
   video_psp1.poke_interface(psp, &iface);
   return iface;
}

/* --------------------------------------------------------------- tests */

/* video_driver.h: read_viewport "Reads out in BGR byte order (24bpp)".
 * Every display format the PSP can be in has to answer in that order;
 * the 16-bit ones are what RetroArch actually boots into.
 *
 * Every display format carries red in its low bits - 565 is
 * B[15:11] G[10:5] R[4:0], 5551 is A B[14:10] G[9:5] R[4:0], 4444 is
 * A B[11:8] G[7:4] R[3:0], 8888 is A B[23:16] G[15:8] R[7:0] - so the
 * expected bytes below all take blue from the top of the pixel. That
 * layout is what the CLUT passes in psp_init() convert the core's
 * frame into, and the four cases here have to agree on it. */
static void test_readback_byte_order(void)
{
   struct { int fmt; const char *name; unsigned pixel; int bpp;
            unsigned char want[3]; } cases[] = {
      { PSP_DISPLAY_PIXEL_FORMAT_565,  "565",  0xFC05,     2,
        { 0xF8, 0x80, 0x28 } },
      { PSP_DISPLAY_PIXEL_FORMAT_5551, "5551", 0x7E85,     2,
        { 0xF8, 0xA0, 0x28 } },
      { PSP_DISPLAY_PIXEL_FORMAT_4444, "4444", 0x028F,     2,
        { 0x20, 0x80, 0xF0 } },
      { PSP_DISPLAY_PIXEL_FORMAT_8888, "8888", 0x002880F8, 4,
        { 0x28, 0x80, 0xF8 } }
   };
   unsigned c;

   for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
   {
      unsigned char out[4 * 2 * 3];
      void  *psp;
      int    i;

      begin("read_viewport answers in BGR byte order");
      psp = driver_init();
      CHECK(psp != NULL, "init failed");
      if (!psp)
         return;

      psp1_fake_set_viewport(0, 0, 4, 2);
      psp1_fake_set_display_format(cases[c].fmt);
      driver_frame(psp, NULL, 4, 2, 8, NULL);

      /* Paint the window read_viewport will walk. */
      for (i = 0; i < 2 * PSP1_FAKE_VRAM_WIDTH; i++)
      {
         if (cases[c].bpp == 2)
            ((unsigned short*)psp1_fake_display_buffer())[i] =
               (unsigned short)cases[c].pixel;
         else
            ((unsigned*)psp1_fake_display_buffer())[i] = cases[c].pixel;
      }

      memset(out, 0, sizeof(out));
      CHECK(video_psp1.read_viewport(psp, out, false),
            "%s: read_viewport refused the format", cases[c].name);

      for (i = 0; i < 4 * 2; i++)
      {
         CHECK(out[i * 3 + 0] == cases[c].want[0]
            && out[i * 3 + 1] == cases[c].want[1]
            && out[i * 3 + 2] == cases[c].want[2],
            "%s: pixel %d is %02x %02x %02x, want B=%02x G=%02x R=%02x",
            cases[c].name, i, out[i * 3], out[i * 3 + 1], out[i * 3 + 2],
            cases[c].want[0], cases[c].want[1], cases[c].want[2]);
         if (failures)
            break;
      }

      video_psp1.free(psp);
      done();
   }
}

/* The screenshot task hands over an uninitialised width*height*3 buffer
 * and encodes all of it, so a clipped viewport must not leave part of
 * it as it was. */
static void test_readback_fills_the_buffer(void)
{
   unsigned char out[600 * 300 * 3];
   void *psp;
   size_t i;
   size_t want;
   int    untouched = 0;

   begin("read_viewport fills every byte it is given");
   psp = driver_init();
   CHECK(psp != NULL, "init failed");
   if (!psp)
      return;

   /* Wider than the framebuffer and taller than the screen, which is
    * what an integer-scaled or custom viewport can ask for. */
   psp1_fake_set_viewport(0, 0, 600, 300);
   driver_frame(psp, NULL, 320, 240, 640, NULL);

   memset(out, 0xC7, sizeof(out));
   CHECK(video_psp1.read_viewport(psp, out, false), "read_viewport failed");

   want = (size_t)600 * 300 * 3;
   for (i = 0; i < want; i++)
      if (out[i] == 0xC7)
         untouched++;

   CHECK(untouched == 0,
         "%d of %zu bytes left uninitialised for the encoder",
         untouched, want);

   video_psp1.free(psp);
   done();
}

/* The vblank handler writes through the driver pointer it was given, so
 * it has to be gone before that pointer is. */
static void test_teardown_releases_the_handler_first(void)
{
   void *psp;
   int   release, freed;

   begin("teardown releases the vblank handler before freeing");
   psp = driver_init();
   CHECK(psp != NULL, "init failed");
   if (!psp)
      return;

   psp1_fake_fire_vblank_on_free(1);
   video_psp1.free(psp);

   CHECK(psp1_fake.vblank_after_release == 0,
         "the vblank handler wrote into the freed driver");

   release = psp1_fake_find_event(PSP1_EV_INTR_RELEASE, 0);
   freed   = psp1_fake_find_event(PSP1_EV_FREE, 0);
   CHECK(release >= 0, "the sub-interrupt handler was never released");
   CHECK(freed >= 0,   "nothing was freed");
   CHECK(release >= 0 && freed >= 0 && release < freed,
         "handler released at event %d, first free at %d", release, freed);
   CHECK(psp1_fake.live_allocs == 0,
         "%d allocation(s) still live after free", psp1_fake.live_allocs);
   done();
}

/* An init that runs out of memory still has to hand back something the
 * frontend can free without leaking the allocations that did succeed. */
static void test_teardown_after_failed_alloc(void)
{
   int n;

   for (n = 0; n < 9; n++)
   {
      void *psp;

      begin("a failed allocation leaves nothing behind");
      psp1_fake.fail_alloc_after = n + 1;   /* struct, then n buffers */
      psp = driver_init();

      if (psp)
         video_psp1.free(psp);

      CHECK(psp1_fake.live_allocs == 0,
            "allocation %d failed: %d block(s) still live",
            n, psp1_fake.live_allocs);
      CHECK(psp1_fake.vblank_registered == 0,
            "allocation %d failed: sub-interrupt handler left registered",
            n);
   }
   done();
}

/* sceGuFinish() hands the list to the GE and returns; the buffer stays
 * the GE's until it is waited on. Anything that refills it before then
 * is rewriting a list mid-execution. */
static void test_menu_texture_waits_for_the_ge(void)
{
   void                          *psp;
   const video_poke_interface_t  *p;
   unsigned short                *menu_tex;

   begin("a display list is not refilled while the GE holds it");
   menu_tex = (unsigned short*)psp1_fake_ram_alloc(64 * 64 * 2);
   psp = driver_init();
   CHECK(psp != NULL, "init failed");
   if (!psp)
      return;
   p = poke(psp);
   CHECK(p && p->set_texture_frame, "no set_texture_frame");
   if (!p || !p->set_texture_frame)
      return;

   driver_frame(psp, NULL, 320, 240, 640, NULL);
   /* The menu pushes a texture between frames, which is when RGUI does
    * it, so the frame's list is still outstanding. */
   p->set_texture_frame(psp, menu_tex, false, VIDEO_SCALE_PACK(64, 64), 1.0f);

   CHECK(psp1_fake.refilled_busy_list == 0,
         "a list the GE was executing was refilled %d time(s)",
         psp1_fake.refilled_busy_list);

   video_psp1.free(psp);
   done();
}

/* Anything the GE writes has to own its cache lines: a partial line
 * shared with data the CPU writes is lost to the next writeback. */
static void test_ge_written_buffers_are_reserved(void)
{
   void                         *psp;
   const video_poke_interface_t *p;
   unsigned short               *menu_tex;

   begin("buffers the GE writes reserve whole cache lines");
   menu_tex = (unsigned short*)psp1_fake_ram_alloc(64 * 64 * 2);
   psp = driver_init();
   CHECK(psp != NULL, "init failed");
   if (!psp)
      return;
   p = poke(psp);

   p->set_texture_frame(psp, menu_tex, false, VIDEO_SCALE_PACK(64, 64), 1.0f);
   CHECK(psp1_fake.copy_dest != NULL, "the menu texture was never written");
   CHECK(((uintptr_t)psp1_fake.copy_dest & 63) == 0,
         "menu texture at %p is not 64-byte aligned", psp1_fake.copy_dest);

   p->set_texture_enable(psp, true, false);
   driver_frame(psp, NULL, 320, 240, 640, NULL);

   CHECK(psp1_fake.ge_context != NULL, "the menu list was never sent");
   CHECK(((uintptr_t)psp1_fake.ge_context & 63) == 0,
         "GE context storage at %p is not 64-byte aligned",
         psp1_fake.ge_context);
   /* It must also not sit inside the driver struct, whose neighbouring
    * fields the CPU writes every frame. */
   CHECK((uintptr_t)psp1_fake.ge_context <  (uintptr_t)psp
      || (uintptr_t)psp1_fake.ge_context >= (uintptr_t)psp + 4096
      || psp1_fake.ge_context == psp,
         "GE context storage is embedded in the driver struct");

   video_psp1.free(psp);
   done();
}

/* VRAM is 2 MB with the display buffers and the colour tables already
 * carved out of it; what is left bounds the core frame the driver will
 * blit there. */
static void test_frame_texture_stays_in_vram(void)
{
   unsigned short *big;
   void           *psp;

   begin("a large core frame stays inside VRAM");
   big = (unsigned short*)psp1_fake_ram_alloc(1024 * 1024 * 2);
   psp = driver_init();
   CHECK(psp != NULL, "init failed");
   if (!psp)
      return;

   driver_frame(psp, big, 1024, 1024, 2048, NULL);
   CHECK(psp1_fake.copy_past_vram == 0,
         "the frame blit ran past the end of VRAM");

   video_psp1.free(psp);
   done();
}

/* The menu texture buffer is one screen of 4444; a menu framebuffer
 * larger than that must not run off the end of it. */
static void test_menu_texture_stays_in_its_buffer(void)
{
   unsigned short               *big;
   void                         *psp;
   const video_poke_interface_t *p;

   begin("an oversized menu texture stays inside its buffer");
   big = (unsigned short*)psp1_fake_ram_alloc(1024 * 1024 * 2);
   psp = driver_init();
   CHECK(psp != NULL, "init failed");
   if (!psp)
      return;
   p = poke(psp);

   p->set_texture_frame(psp, big, false, VIDEO_SCALE_PACK(1024, 1024), 1.0f);
   CHECK(psp1_fake.copy_past_dest == 0,
         "the menu blit ran past the end of its buffer");

   video_psp1.free(psp);
   done();
}

/* The vertex array is written through the uncached alias, where every
 * store goes to memory. Rebuilding it for geometry that has not moved
 * is 256 of those per frame. */
static void test_tex_coords_follow_the_geometry(void)
{
   void               *psp;
   psp1_fake_sprite_t *co;
   int                 i, rewritten;

   begin("texture coordinates are rebuilt only when geometry changes");
   psp = driver_init();
   CHECK(psp != NULL, "init failed");
   if (!psp)
      return;

   driver_frame(psp, NULL, 320, 240, 640, NULL);
   co = (psp1_fake_sprite_t*)psp1_fake.init_vertices;
   CHECK(co != NULL, "the vertex array was never handed to the GE");
   if (!co)
      return;

   /* Mark the texture coordinates; a frame of the same size must leave
    * the marks alone. */
   for (i = 0; i < PSP1_FAKE_SLICES; i++)
      co[i].v0.u = -1.0f;

   driver_frame(psp, NULL, 320, 240, 640, NULL);

   rewritten = 0;
   for (i = 0; i < PSP1_FAKE_SLICES; i++)
      if (co[i].v0.u != -1.0f)
         rewritten++;
   CHECK(rewritten == 0,
         "%d slice(s) rewritten for unchanged 320x240 geometry", rewritten);

   /* A size change must rebuild them, and correctly: the grid spans
    * the source frame exactly. */
   driver_frame(psp, NULL, 256, 224, 512, NULL);
   CHECK(fabsf(co[0].v0.u) < 0.001f, "u does not start at 0 (%f)",
         co[0].v0.u);
   CHECK(fabsf(co[0].v0.v) < 0.001f, "v does not start at 0 (%f)",
         co[0].v0.v);
   CHECK(fabsf(co[PSP1_FAKE_COLUMNS - 1].v1.u - 256.0f) < 0.01f,
         "the last column ends at u=%f, want 256",
         co[PSP1_FAKE_COLUMNS - 1].v1.u);
   CHECK(fabsf(co[PSP1_FAKE_SLICES - 1].v1.v - 224.0f) < 0.01f,
         "the last row ends at v=%f, want 224",
         co[PSP1_FAKE_SLICES - 1].v1.v);

   video_psp1.free(psp);
   done();
}

/* Every rotation has to cover the same viewport: the grid is what the
 * frame is drawn through, so a gap in it is a gap on screen. */
static void test_screen_coords_tile_the_viewport(void)
{
   unsigned rot;

   for (rot = 0; rot < 4; rot++)
   {
      void               *psp;
      psp1_fake_sprite_t *co;
      float               lo_x, hi_x, lo_y, hi_y, area;
      int                 i;

      begin("the sprite grid covers the viewport in every rotation");
      psp = driver_init();
      CHECK(psp != NULL, "init failed");
      if (!psp)
         return;

      psp1_fake_set_viewport(60, 16, 360, 240);
      video_psp1.set_rotation(psp, rot);
      driver_frame(psp, NULL, 320, 240, 640, NULL);

      co   = (psp1_fake_sprite_t*)psp1_fake.init_vertices;
      lo_x = hi_x = co[0].v0.x;
      lo_y = hi_y = co[0].v0.y;
      area = 0.0f;

      for (i = 0; i < PSP1_FAKE_SLICES; i++)
      {
         float xs[2], ys[2];
         int   k;
         xs[0] = co[i].v0.x; xs[1] = co[i].v1.x;
         ys[0] = co[i].v0.y; ys[1] = co[i].v1.y;
         for (k = 0; k < 2; k++)
         {
            if (xs[k] < lo_x) lo_x = xs[k];
            if (xs[k] > hi_x) hi_x = xs[k];
            if (ys[k] < lo_y) lo_y = ys[k];
            if (ys[k] > hi_y) hi_y = ys[k];
         }
         area += fabsf(xs[1] - xs[0]) * fabsf(ys[1] - ys[0]);
      }

      CHECK(fabsf((hi_x - lo_x) * (hi_y - lo_y) - area) < 1.0f,
            "rotation %u: slices cover %f of a %f bounding box",
            rot, area, (hi_x - lo_x) * (hi_y - lo_y));
      CHECK(fabsf(lo_x - 60.0f) < 0.01f && fabsf(lo_y - 16.0f) < 0.01f,
            "rotation %u: grid starts at %f,%f, want 60,16",
            rot, lo_x, lo_y);
      CHECK(fabsf((hi_x - lo_x) - 360.0f) < 0.01f
         && fabsf((hi_y - lo_y) - 240.0f) < 0.01f,
            "rotation %u: grid spans %fx%f, want 360x240",
            rot, hi_x - lo_x, hi_y - lo_y);

      video_psp1.free(psp);
      done();
   }
}

/* The GE's block transfer has hard limits - each side at most 1023, both
 * strides a multiple of 8 and no wider than 1024 - and shipping cores
 * sit outside them: vecx hands over 330-pixel rows, o2em 340-wide ones.
 * The frame has to arrive in the texture whatever the core's stride. */
static void test_blit_geometry_and_content(void)
{
   struct { unsigned w, h, pitch_px; const char *who; } cases[] = {
      { 256, 224, 256, "snes9x2010 256x224" },
      { 320, 240, 320, "prboom 320x240" },
      { 340, 250, 400, "o2em 340x250, stride 400" },
      { 330, 410, 330, "vecx 330x410" },
      { 660, 410, 660, "vecx 2x, 660x410" },
      { 1320, 410, 1320, "vecx 4x, 1320x410" }
   };
   unsigned c;

   for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
   {
      void           *psp;
      unsigned short *src;
      unsigned        x, y, mismatches = 0, checked, kept_w;

      begin("every blit is a legal GE transfer that lands the frame");
      psp = driver_init();
      CHECK(psp != NULL, "init failed");
      if (!psp)
         return;

      src = (unsigned short*)psp1_fake_ram_alloc(
            (size_t)cases[c].pitch_px * cases[c].h * 2);
      CHECK(src != NULL, "%s: no source buffer", cases[c].who);
      if (!src)
         return;

      for (y = 0; y < cases[c].h; y++)
         for (x = 0; x < cases[c].pitch_px; x++)
            src[y * cases[c].pitch_px + x] =
               (unsigned short)((y * 7919u + x * 31u) | 1u);

      driver_frame(psp, src, cases[c].w, cases[c].h,
            cases[c].pitch_px * 2, NULL);

      CHECK(psp1_fake.bad_blit_geometry == 0,
            "%s: %d malformed transfer(s)",
            cases[c].who, psp1_fake.bad_blit_geometry);
      CHECK(psp1_fake.tex_image != NULL,
            "%s: no texture was bound", cases[c].who);
      if (!psp1_fake.tex_image)
         return;

      /* Whatever the driver kept of the frame has to be the frame:
       * the GE's limits crop it at 1023 a side, but must not shear or
       * shift what is kept. */
      kept_w  = (cases[c].w < 1023) ? cases[c].w : 1023;
      checked = 0;
      for (y = 0; y < (unsigned)psp1_fake.tex_image_h
            && y < cases[c].h; y++)
      {
         const unsigned short *drow = (const unsigned short*)
            psp1_fake.tex_image + (size_t)y * psp1_fake.tex_image_bw;
         const unsigned short *srow = src + (size_t)y * cases[c].pitch_px;

         for (x = 0; x < kept_w && x < cases[c].pitch_px; x++)
         {
            checked++;
            if (drow[x] != srow[x])
               mismatches++;
         }
      }
      CHECK(checked > 0, "%s: nothing was copied", cases[c].who);
      CHECK(mismatches == 0,
            "%s: %u of %u texels differ from the source",
            cases[c].who, mismatches, checked);

      video_psp1.free(psp);
      done();
   }
}

/* Pacing: a vsync frame waits for a vblank unless one already went by
 * while it was being drawn. psp_on_vblank() reports that from interrupt
 * context, which is the only writer the main thread races. */
static void test_vsync_waits_for_one_vblank(void)
{
   void *psp;
   int   before;

   begin("vsync waits for a vblank, and skips it when one has passed");
   psp = driver_init();
   CHECK(psp != NULL, "init failed");
   if (!psp)
      return;

   driver_frame(psp, NULL, 320, 240, 640, NULL);

   before = psp1_fake.vblank_waits;
   driver_frame(psp, NULL, 320, 240, 640, NULL);
   CHECK(psp1_fake.vblank_waits == before + 1,
         "a vsync frame waited %d time(s), want 1",
         psp1_fake.vblank_waits - before);

   /* A vblank that lands while the frame is drawn means the next one
    * is already late and must not wait again. */
   psp1_fake_fire_vblank();
   before = psp1_fake.vblank_waits;
   driver_frame(psp, NULL, 320, 240, 640, NULL);
   CHECK(psp1_fake.vblank_waits == before,
         "waited although a vblank had already passed");

   video_psp1.free(psp);
   done();
}

/* With a hardware-rendering core the driver does not wait for the GE,
 * so the CPU must not go and write the framebuffer underneath it. */
static void test_osd_does_not_race_the_ge(void)
{
   void *psp;

   begin("the OSD does not write a framebuffer the GE is using");
   psp = driver_init();
   CHECK(psp != NULL, "init failed");
   if (!psp)
      return;

   /* A frame in VRAM puts the driver in its hardware-render path. */
   driver_frame(psp, (void*)(uintptr_t)(PSP1_FAKE_VRAM_TOP + 0x100000),
         320, 240, 640, "status");
   driver_frame(psp, (void*)(uintptr_t)(PSP1_FAKE_VRAM_TOP + 0x100000),
         320, 240, 640, "status");

   CHECK(psp1_fake.wrote_busy_framebuffer == 0,
         "the OSD wrote the framebuffer %d time(s) with the GE still on it",
         psp1_fake.wrote_busy_framebuffer);

   video_psp1.free(psp);
   done();
}

int main(void)
{
   if (psp1_fake_mem_init() != 0)
   {
      fprintf(stderr,
            "psp1_gfx_contracts: cannot map the PSP address windows "
            "(0x%08lx / 0x%08lx); skipping\n",
            PSP1_FAKE_RAM_BASE, PSP1_FAKE_VRAM_BASE);
      return 77;
   }

   setvbuf(stdout, NULL, _IONBF, 0);
   printf("psp1_gfx contracts\n");

   test_readback_byte_order();
   test_readback_fills_the_buffer();
   test_teardown_releases_the_handler_first();
   test_menu_texture_waits_for_the_ge();
   test_ge_written_buffers_are_reserved();
   test_frame_texture_stays_in_vram();
   test_menu_texture_stays_in_its_buffer();
   test_tex_coords_follow_the_geometry();
   test_screen_coords_tile_the_viewport();
   test_blit_geometry_and_content();
   test_vsync_waits_for_one_vblank();
   test_osd_does_not_race_the_ge();
   test_teardown_after_failed_alloc();

   if (failures)
   {
      printf("\n%d contract(s) broken\n", failures);
      return 1;
   }
   printf("\nall contracts hold\n");
   return 0;
}
