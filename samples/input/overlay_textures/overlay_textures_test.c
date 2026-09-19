/* overlay_textures_test.c -- what the driver is shown is what the
 * pack loaded, on every route a page can take to it.
 *
 * input/input_overlay_textures.c is compiled as it is; the driver is
 * the stub in stubs_retroarch.c, synchronous as a driver without the
 * threaded wrapper is, which is the configuration that regressed:
 * 86fbf34e01 freed the pack's pixels as soon as its textures existed
 * and then matched each page's images to those textures by the very
 * pointers it had just freed. No page ever matched, the upload was
 * declined, and the fallback handed the freed pixels to load(). With
 * threaded video the handles were not there at the poll, the free was
 * never reached, and nothing showed.
 *
 * The pack is two pages over three unique images, one of them on both
 * pages, which is the deduplication the page lists exist for. Every
 * lane is a route to the driver:
 *
 *   textures        a driver with load_textures gets, per page, the
 *                   textures made from that page's own images, and
 *                   load() is never reached;
 *   page switch     the second page uploads nothing;
 *   pixels go       once the driver has the textures the pixels are
 *                   released, and the sizes stay;
 *   declined        a driver that answers load_textures with false
 *                   (the wrapper over a driver with no such path)
 *                   gets live pixels through load(), on every page;
 *   no such path    the same for a driver with no load_textures;
 *   upload fails    a texture that cannot be made leaves the pack
 *                   whole and the page goes through load();
 *   spent pack      a pack whose textures went with the driver and
 *                   whose pixels went before them shows nothing
 *                   rather than something freed, and says it has
 *                   nothing to come back from;
 *   no images       a pack of hitboxes alone is always reusable.
 *
 * Built with THREADS=1 the threaded wrapper's stub is in, and the race
 * between the posts and the poll is the test's to decide:
 *
 *   prompt handles  the video thread uploads before the poll: the
 *                   unthreaded sequence by another road, and one
 *                   86fbf34e01 broke as well, whenever the thread
 *                   was quick;
 *   late handles    it has not: the page goes through load() with
 *                   live pixels, the pack keeps them, and when the
 *                   thread does run it reads pixels still there.
 *
 * load() and the texture upload read every pixel they are given, so
 * under ASan a stale pointer is a report, not a picture nobody sees.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../input/input_overlay.h"

#include "stubs_retroarch.h"

#ifdef HAVE_THREADS
#include "../../../gfx/video_thread_wrapper.h"
#endif

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("[FAIL] "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

#define NUM_IMAGES 3
#define NUM_PAGES  2

/* Which unique image each page entry is. Image 1 is on both pages. */
static const unsigned page_map[NUM_PAGES][2] = { { 0, 1 }, { 1, 2 } };
static const unsigned img_w[NUM_IMAGES]      = { 16, 8, 32 };
static const unsigned img_h[NUM_IMAGES]      = { 16, 24, 4 };
static uint32_t       img_sum[NUM_IMAGES];

/* --- the driver's overlay interface ------------------------------ */

static unsigned drv_loads;         /* load() calls                  */
static unsigned drv_load_textures; /* load_textures() calls         */
static unsigned drv_null_pixels;   /* load() entries with no pixels */
static unsigned drv_shown;         /* entries of the page shown     */
static uint32_t drv_sum[2];        /* what each entry looked like   */
static bool     drv_decline;       /* load_textures answers false   */

static bool drv_load(void *data, const void *images, unsigned num)
{
   unsigned i;
   const struct texture_image *img = (const struct texture_image*)images;
   (void)data;
   drv_loads++;
   drv_shown = num;
   for (i = 0; i < num && i < 2; i++)
   {
      if (!img[i].pixels)
      {
         drv_null_pixels++;
         drv_sum[i] = 0;
         continue;
      }
      drv_sum[i] = stub_checksum(&img[i]);
   }
   return true;
}

static bool drv_load_tex(void *data, const uintptr_t *tex, unsigned num)
{
   unsigned i;
   (void)data;
   drv_load_textures++;
   if (drv_decline)
      return false;
   drv_shown = num;
   for (i = 0; i < num && i < 2; i++)
   {
      const struct stub_texture *t = stub_texture_get(tex[i]);
      drv_sum[i] = t ? t->checksum : 0;
   }
   return true;
}

static const video_overlay_interface_t iface_textures = {
   NULL, drv_load, drv_load_tex, NULL, NULL, NULL, NULL
};
static const video_overlay_interface_t iface_pixels = {
   NULL, drv_load, NULL, NULL, NULL, NULL, NULL
};

static void drv_reset(void)
{
   drv_loads = drv_load_textures = drv_null_pixels = drv_shown = 0;
   drv_sum[0] = drv_sum[1] = 0;
   drv_decline = false;
   stub_reset();
}

/* --- the pack, as input_overlay_loaded() leaves it --------------- */

static input_overlay_t *pack_new(const video_overlay_interface_t *iface,
      unsigned num_images)
{
   unsigned i, j;
   input_overlay_t *ol = (input_overlay_t*)calloc(1, sizeof(*ol));

   ol->iface      = iface;
   ol->size       = NUM_PAGES;
   ol->overlays   = (struct overlay*)calloc(NUM_PAGES, sizeof(*ol->overlays));
   ol->num_images = num_images;
   if (num_images)
      ol->images  = (struct texture_image**)
         calloc(num_images, sizeof(*ol->images));

   for (i = 0; i < num_images; i++)
   {
      size_t p, n   = (size_t)img_w[i] * img_h[i];
      ol->images[i] = (struct texture_image*)calloc(1, sizeof(**ol->images));
      ol->images[i]->width  = img_w[i];
      ol->images[i]->height = img_h[i];
      ol->images[i]->pixels = (uint32_t*)malloc(n * sizeof(uint32_t));
      for (p = 0; p < n; p++)
         ol->images[i]->pixels[p] = 0xff000000u | (uint32_t)((i + 1) * 7919u * (p + 1));
      img_sum[i] = stub_checksum(ol->images[i]);
   }
   /* A page's entry is a copy of the unique image's struct: the same
    * pixels by another name, which is what the loader produces. */
   for (i = 0; i < NUM_PAGES && num_images; i++)
   {
      struct overlay *o   = &ol->overlays[i];
      o->load_images_size = 2;
      o->load_images      = (struct texture_image*)
         calloc(2, sizeof(*o->load_images));
      for (j = 0; j < 2; j++)
         o->load_images[j] = *ol->images[page_map[i][j]];
   }
   ol->active = &ol->overlays[0];
   ol->flags  = INPUT_OVERLAY_ALIVE;
   return ol;
}

static void pack_free(input_overlay_t *ol)
{
   size_t i;
   input_overlay_release_textures(ol);
   for (i = 0; i < ol->num_images; i++)
   {
      image_texture_free(ol->images[i]);
      free(ol->images[i]);
   }
   free(ol->images);
   for (i = 0; i < ol->size; i++)
      free(ol->overlays[i].load_images);
   free(ol->overlays);
   free(ol);
}

static void pack_show(input_overlay_t *ol, unsigned page)
{
   ol->index  = page;
   ol->active = &ol->overlays[page];
}

static void check_page(const char *lane, unsigned page)
{
   unsigned j;
   CHECK(drv_shown == 2, "%s: page %u showed %u entries", lane, page, drv_shown);
   for (j = 0; j < 2; j++)
      CHECK(drv_sum[j] == img_sum[page_map[page][j]],
            "%s: page %u entry %u is not image %u", lane, page, j,
            page_map[page][j]);
}

/* --- lanes -------------------------------------------------------- */

static void lane_textures(void)
{
   unsigned i;
   input_overlay_t *ol;
   drv_reset();
   ol = pack_new(&iface_textures, NUM_IMAGES);

   CHECK(input_overlay_load_page(ol), "textures: page 0 did not go as textures");
   CHECK(drv_loads == 0, "textures: load() was reached (%u)", drv_loads);
   CHECK(stub_tex_loads == NUM_IMAGES,
         "textures: %u uploads for %u unique images", stub_tex_loads, NUM_IMAGES);
   check_page("textures", 0);

   /* The pixels are on the GPU: they go, the sizes stay. */
   for (i = 0; i < NUM_IMAGES; i++)
   {
      CHECK(!ol->images[i]->pixels, "pixels go: image %u kept its pixels", i);
      CHECK(ol->images[i]->width == img_w[i] && ol->images[i]->height == img_h[i],
            "pixels go: image %u lost its size (%ux%u)", i,
            ol->images[i]->width, ol->images[i]->height);
   }
   CHECK(input_overlay_has_source(ol), "pixels go: an uploaded pack is not reusable");

   pack_show(ol, 1);
   CHECK(input_overlay_load_page(ol), "page switch: page 1 did not go as textures");
   CHECK(stub_tex_loads == NUM_IMAGES,
         "page switch: uploaded again (%u)", stub_tex_loads);
   CHECK(drv_loads == 0, "page switch: load() was reached");
   check_page("page switch", 1);

   /* The driver goes. Nothing is left to show the pack from, and it
    * must not show something freed instead. */
   input_overlay_release_textures(ol);
   CHECK(stub_tex_live == 0, "spent pack: %u textures outlived the release",
         stub_tex_live);
   CHECK(!input_overlay_has_source(ol), "spent pack: reported as reusable");
   ol->flags &= ~INPUT_OVERLAY_TEXTURES_DECLINED;
   CHECK(!input_overlay_load_page(ol), "spent pack: went as textures");
   CHECK(drv_loads == 0 && drv_null_pixels == 0,
         "spent pack: load() was handed a pack with no pixels");

   pack_free(ol);
   printf("[%s] textures / page switch / pixels go / spent pack lanes\n",
         failures ? "fail" : "pass");
}

static void lane_pixels(const char *lane,
      const video_overlay_interface_t *iface, bool decline, bool upload_fails)
{
   unsigned i, before = failures;
   input_overlay_t *ol;
   drv_reset();
   drv_decline         = decline;
   stub_tex_load_fails = upload_fails;
   ol                  = pack_new(iface, NUM_IMAGES);

   for (i = 0; i < NUM_PAGES; i++)
   {
      pack_show(ol, i);
      CHECK(!input_overlay_load_page(ol), "%s: page %u went as textures", lane, i);
      CHECK(drv_loads == i + 1, "%s: page %u did not reach load()", lane, i);
      CHECK(drv_null_pixels == 0, "%s: page %u had no pixels", lane, i);
      check_page(lane, i);
   }
   if (decline)
   {
      CHECK(drv_load_textures == 1,
            "%s: the driver was asked %u times", lane, drv_load_textures);
      CHECK(ol->flags & INPUT_OVERLAY_TEXTURES_DECLINED, "%s: not remembered", lane);
   }
   CHECK(stub_tex_live == 0, "%s: %u textures held for nothing", lane, stub_tex_live);
   for (i = 0; i < NUM_IMAGES; i++)
      CHECK(ol->images[i]->pixels != NULL, "%s: image %u lost its pixels", lane, i);
   CHECK(input_overlay_has_source(ol), "%s: pack is not reusable", lane);

   pack_free(ol);
   printf("[%s] %s lane\n", failures == before ? "pass" : "fail", lane);
}

static void lane_no_images(void)
{
   unsigned before = failures;
   input_overlay_t *ol;
   drv_reset();
   ol = pack_new(&iface_textures, 0);
   CHECK(input_overlay_has_source(ol), "no images: pack is not reusable");
   CHECK(!input_overlay_load_page(ol), "no images: went as textures");
   CHECK(stub_tex_loads == 0, "no images: uploaded something");
   pack_free(ol);
   printf("[%s] no images lane\n", failures == before ? "pass" : "fail");
}

#ifdef HAVE_THREADS
/* The wrapper is there and the video thread wins the race: the
 * handles are in at the poll, which is the unthreaded sequence by
 * another road - and the one 86fbf34e01 broke under threaded video
 * too, whenever the thread was quick. */
static void lane_threaded_prompt(void)
{
   unsigned i, before = failures;
   input_overlay_t *ol;
   drv_reset();
   stub_thread_active    = true;
   stub_thread_wins_race = true;
   ol = pack_new(&iface_textures, NUM_IMAGES);

   for (i = 0; i < NUM_PAGES; i++)
   {
      pack_show(ol, i);
      CHECK(input_overlay_load_page(ol),
            "threaded, prompt: page %u did not go as textures", i);
      check_page("threaded, prompt", i);
   }
   CHECK(drv_loads == 0, "threaded, prompt: load() was reached");
   CHECK(stub_tex_loads == NUM_IMAGES,
         "threaded, prompt: %u uploads", stub_tex_loads);
   for (i = 0; i < NUM_IMAGES; i++)
      CHECK(!ol->images[i]->pixels,
            "threaded, prompt: image %u kept its pixels", i);

   pack_free(ol);
   stub_thread_active = stub_thread_wins_race = false;
   printf("[%s] threaded, prompt handles lane\n",
         failures == before ? "pass" : "fail");
}

/* The video thread has not got round to the uploads by the poll,
 * which is the usual case: the poll follows the posts at once. The
 * page goes through load() with live pixels, the pack keeps them, and
 * when the thread does run it reads pixels that are still there. */
static void lane_threaded_late(void)
{
   unsigned i, before = failures;
   input_overlay_t *ol;
   drv_reset();
   stub_thread_active    = true;
   stub_thread_wins_race = false;
   ol = pack_new(&iface_textures, NUM_IMAGES);

   CHECK(!input_overlay_load_page(ol), "threaded, late: went as textures");
   CHECK(drv_loads == 1 && drv_null_pixels == 0,
         "threaded, late: page 0 did not reach load() with pixels");
   check_page("threaded, late", 0);
   for (i = 0; i < NUM_IMAGES; i++)
      CHECK(ol->images[i]->pixels != NULL,
            "threaded, late: image %u lost its pixels", i);

   /* Now the thread runs, and the completions come back. */
   stub_video_thread_run();
   video_thread_async_poll();

   pack_free(ol);
   stub_video_thread_run();
   video_thread_async_poll();
   CHECK(stub_tex_live == 0, "threaded, late: %u textures outlived the pack",
         stub_tex_live);
   stub_thread_active = false;
   printf("[%s] threaded, late handles lane\n",
         failures == before ? "pass" : "fail");
}
#endif

int main(void)
{
   lane_textures();
   lane_pixels("declined",     &iface_textures, true,  false);
   lane_pixels("no such path", &iface_pixels,   false, false);
   lane_pixels("upload fails", &iface_textures, false, true);
   lane_no_images();
#ifdef HAVE_THREADS
   lane_threaded_prompt();
   lane_threaded_late();
#endif

   if (failures)
   {
      printf("FAIL overlay_textures_test (%u)\n", failures);
      return 1;
   }
   printf("PASS overlay_textures_test\n");
   return 0;
}
