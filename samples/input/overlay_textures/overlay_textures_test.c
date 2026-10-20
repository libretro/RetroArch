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
 *   has image       and every desc still says it has an image, which
 *                   is what gets its geometry set. The pack is laid
 *                   out as the loader lays it out - images[] pointing
 *                   INTO the descs - because a pack with private
 *                   image structs cannot see this go wrong;
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
 *   load fails      a page the driver could not load is reported
 *                   as not there, so nobody sets its alpha and
 *                   geometry on whatever the driver held before;
 *   no images       a pack of hitboxes alone is always reusable.
 *
 * Built with THREADS=1 the threaded wrapper's stub is in, and the race
 * between the posts and the poll is the test's to decide:
 *
 *   prompt handles  the video thread uploads before the poll: the
 *                   unthreaded sequence by another road, and one
 *                   86fbf34e01 broke as well, whenever the thread
 *                   was quick;
 *   late handles    it has not, which is the usual case: the page
 *                   goes through load() with live pixels, the uploads
 *                   are kept rather than posted again at every page,
 *                   and once the handles are in the page on screen
 *                   moves over to them and the pixels go;
 *   late, refused   the same, and then the driver declines them or
 *                   the thread could not make them: load() from then
 *                   on, pixels kept, no texture held for nothing;
 *   freed in flight the pack is freed, or the driver goes, with the
 *                   uploads still queued: the video thread reads
 *                   pixels that went with the surfaces, not pixels
 *                   freed under it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../input/input_overlay.h"

#include "../../../gfx/gfx_surface.h"
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

/* The image pack_new() makes a BC1 DDS of, the way the loader leaves
 * one (a payload and no pixels); NUM_IMAGES for none. Image 1, the one
 * on both pages. */
#define BCN_IMAGE 1
static unsigned pack_bcn_image = NUM_IMAGES;
static uint32_t bcn_sum_bytes;  /* its blocks, as a driver gets them  */
static uint32_t bcn_sum_rgba;   /* its pixels, decoded on their own   */

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

/* A DXT1 file of @w x @h, multiples of 4, into @buf; its length */
static size_t dds_bc1_make(uint8_t *buf, unsigned w, unsigned h)
{
   unsigned b, blocks = (w / 4) * (h / 4);
   uint8_t *p         = buf + 128;
   memset(buf, 0, 128);
   put32(buf +   0, 0x20534444u);           /* "DDS " */
   put32(buf +   4, 124);
   put32(buf +   8, 0x1007u | 0x80000u);    /* caps|h|w|pf|linearsize */
   put32(buf +  12, h);
   put32(buf +  16, w);
   put32(buf +  20, blocks * 8);
   put32(buf +  76, 32);
   put32(buf +  80, 0x4u);                  /* DDPF_FOURCC */
   memcpy(buf + 84, "DXT1", 4);
   put32(buf + 108, 0x1000u);               /* DDSCAPS_TEXTURE */
   for (b = 0; b < blocks; b++, p += 8)
   {
      /* color0 > color1: four colours, no transparency */
      put32(p,     0xf800u | ((b * 0x841u) << 16));
      put32(p + 4, 0x1b2c3d4eu * (b + 1));
   }
   return 128 + (size_t)blocks * 8;
}

static bool dds_bc1_load(struct texture_image *img, unsigned w,
      unsigned h, bool compressed)
{
   uint8_t buf[128 + 32 * 8];
   image_texture_request_t req;
   size_t len = dds_bc1_make(buf, w, h);
   memset(&req, 0, sizeof(req));
   memset(img, 0, sizeof(*img));
   req.want_compressed = compressed;
   img->supports_rgba  = false;
   return image_texture_load_buffer_request(img, IMAGE_TYPE_DDS,
         buf, len, &req, NULL, NULL);
}

/* --- the driver's overlay interface ------------------------------ */

static unsigned drv_loads;         /* load() calls                  */
static unsigned drv_load_textures; /* load_textures() calls         */
static unsigned drv_null_pixels;   /* load() entries with no pixels */
static unsigned drv_shown;         /* entries of the page shown     */
static uint32_t drv_sum[2];        /* what each entry looked like   */
static bool     drv_decline;       /* load_textures answers false   */
static bool     drv_load_fails;    /* load() answers false          */

static bool drv_load(void *data, const void *images, unsigned num)
{
   unsigned i;
   const struct texture_image *img = (const struct texture_image*)images;
   (void)data;
   drv_loads++;
   if (drv_load_fails)
      return false;
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
   drv_decline = drv_load_fails = false;
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

   /* As the loader leaves it (task_overlay_load_image_texture): every
    * desc with an image has a texture_image of its own, and the pack's
    * unique image is not a struct apart but the ADDRESS of the first
    * desc's that named the file - so images[] points into the pages,
    * and whatever is done to images[u] is done to that desc. A pack
    * built any other way hides exactly the bug the "has image" lane
    * below is for. */
   for (i = 0; i < NUM_PAGES && num_images; i++)
   {
      struct overlay *o = &ol->overlays[i];
      o->size           = 2;
      o->descs          = (struct overlay_desc*)calloc(2, sizeof(*o->descs));
   }
   for (i = 0; i < num_images; i++)
   {
      size_t p, n = (size_t)img_w[i] * img_h[i];
      struct texture_image *img = NULL;
      /* the first page entry that uses image i owns it */
      for (j = 0; j < NUM_PAGES * 2 && !img; j++)
         if (page_map[j / 2][j % 2] == i)
            img = &ol->overlays[j / 2].descs[j % 2].image;
      if (i == pack_bcn_image)
      {
         struct texture_image ref;
         CHECK(dds_bc1_load(img, img_w[i], img_h[i], true)
               && img->compressed && !img->pixels,
               "bcn: the DDS did not load as a payload");
         CHECK(dds_bc1_load(&ref, img_w[i], img_h[i], false) && ref.pixels,
               "bcn: the DDS did not decode");
         bcn_sum_bytes = img->compressed
            ? stub_checksum_bytes(img->compressed->mips[0].data,
                  img->compressed->mips[0].size) : 0;
         bcn_sum_rgba  = ref.pixels ? stub_checksum(&ref) : 0;
         image_texture_free(&ref);
         ol->images[i] = img;
         continue;
      }
      img->width  = img_w[i];
      img->height = img_h[i];
      img->pixels = (uint32_t*)malloc(n * sizeof(uint32_t));
      for (p = 0; p < n; p++)
         img->pixels[p] = 0xff000000u | (uint32_t)((i + 1) * 7919u * (p + 1));
      ol->images[i] = img;
      img_sum[i]    = stub_checksum(img);
   }
   /* A page's entry, and the image of every later desc that shares a
    * file, is a copy of the unique image's struct: the same pixels by
    * another name. */
   for (i = 0; i < NUM_PAGES && num_images; i++)
   {
      struct overlay *o   = &ol->overlays[i];
      o->load_images_size = 2;
      o->load_images      = (struct texture_image*)
         calloc(2, sizeof(*o->load_images));
      for (j = 0; j < 2; j++)
      {
         o->descs[j].image       = *ol->images[page_map[i][j]];
         o->descs[j].image_index = j;
         o->load_images[j]       = *ol->images[page_map[i][j]];
      }
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
      image_texture_free(ol->images[i]);   /* a desc's: not freed itself */
   free(ol->images);
   for (i = 0; i < ol->size; i++)
   {
      free(ol->overlays[i].load_images);
      free(ol->overlays[i].descs);
   }
   free(ol->overlays);
   free(ol->anim_stream);
   free(ol->anim_player);
   free(ol->anim_resend);
   if (ol->anim_2frame_pix)
      for (i = 0; i < ol->num_images; i++)
         free(ol->anim_2frame_pix[i]);
   free(ol->anim_2frame_pix);
   free(ol->anim_2frame);
   free(ol->anim_2frame_cur);
   free(ol->anim_2frame_pressed);
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

   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_TEXTURES, "textures: page 0 did not go as textures");
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

   /* has image: the pixels went, and the struct they went from is the
    * desc's own. Every desc must still say it has an image - that is
    * what decides whether its geometry is ever set; when it said no,
    * every image of the page was drawn over the whole screen - and
    * none may be left pointing at what was freed. */
   {
      unsigned pg, d;
      for (pg = 0; pg < NUM_PAGES; pg++)
         for (d = 0; d < 2; d++)
         {
            struct overlay_desc *desc = &ol->overlays[pg].descs[d];
            CHECK(OVERLAY_HAS_IMAGE(&desc->image),
                  "has image: page %u desc %u no longer has one", pg, d);
            CHECK(!desc->image.pixels,
                  "has image: page %u desc %u points at freed pixels", pg, d);
         }
   }

   pack_show(ol, 1);
   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_TEXTURES, "page switch: page 1 did not go as textures");
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
   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_NONE, "spent pack: went as textures");
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
      CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_PIXELS, "%s: page %u went as textures", lane, i);
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

/* The driver could not load the page. It is reported as not there, so
 * that the caller does not go on to set this page's alpha and geometry
 * on whatever the driver held before - fewer images, or none. */
static void lane_load_fails(void)
{
   unsigned before = failures;
   input_overlay_t *ol;
   drv_reset();
   drv_load_fails = true;
   ol = pack_new(&iface_pixels, NUM_IMAGES);
   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_NONE,
         "load fails: the page was reported as loaded");
   CHECK(drv_loads == 1, "load fails: load() was not tried");
   pack_free(ol);
   printf("[%s] load fails lane\n", failures == before ? "pass" : "fail");
}

static void lane_no_images(void)
{
   unsigned before = failures;
   input_overlay_t *ol;
   drv_reset();
   ol = pack_new(&iface_textures, 0);
   CHECK(input_overlay_has_source(ol), "no images: pack is not reusable");
   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_PIXELS, "no images: went as textures");
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
      CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_TEXTURES,
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
 * page goes through load() with live pixels and the uploads are kept,
 * not thrown away and posted again at the next page; when the handles
 * are in, the page on screen moves over to them and the pixels go. */
static void lane_threaded_late(void)
{
   unsigned i, before = failures;
   input_overlay_t *ol;
   drv_reset();
   stub_thread_active    = true;
   stub_thread_wins_race = false;
   ol = pack_new(&iface_textures, NUM_IMAGES);

   for (i = 0; i < NUM_PAGES; i++)
   {
      pack_show(ol, i);
      CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_PIXELS,
            "threaded, late: page %u went as textures", i);
      CHECK(drv_loads == i + 1 && drv_null_pixels == 0,
            "threaded, late: page %u did not reach load() with pixels", i);
      check_page("threaded, late", i);
      CHECK(!input_overlay_promote_textures(ol),
            "threaded, late: promoted with nothing uploaded");
   }
   for (i = 0; i < NUM_IMAGES; i++)
      CHECK(ol->images[i]->pixels != NULL,
            "threaded, late: image %u lost its pixels", i);

   /* The thread runs, the frame's poll delivers, the input poll
    * promotes: page 1, the one on screen. */
   CHECK(stub_video_thread_run() == NUM_IMAGES,
         "threaded, late: the uploads were posted more than once");
   video_thread_async_poll();
   drv_shown = 0;
   CHECK(input_overlay_promote_textures(ol), "threaded, late: not promoted");
   check_page("threaded, late (promoted)", 1);
   CHECK(!input_overlay_promote_textures(ol), "threaded, late: promoted twice");
   for (i = 0; i < NUM_IMAGES; i++)
      CHECK(!ol->images[i]->pixels,
            "threaded, late: image %u kept its pixels", i);

   pack_show(ol, 0);
   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_TEXTURES, "threaded, late: page 0 not as textures");
   check_page("threaded, late (switch)", 0);
   CHECK(stub_tex_loads == NUM_IMAGES && drv_loads == NUM_PAGES,
         "threaded, late: %u uploads, %u load() calls",
         stub_tex_loads, drv_loads);

   pack_free(ol);
   CHECK(stub_tex_live == 0, "threaded, late: %u textures outlived the pack",
         stub_tex_live);
   stub_thread_active = false;
   printf("[%s] threaded, late handles lane\n",
         failures == before ? "pass" : "fail");
}

/* The handles come in late and then the driver beneath the wrapper
 * declines them, or the thread could not make them: the pack stays
 * on load() with its pixels and holds no texture for nothing. */
static void lane_threaded_late_refused(const char *lane,
      bool decline, bool upload_fails)
{
   unsigned i, before = failures;
   input_overlay_t *ol;
   drv_reset();
   stub_thread_active    = true;
   stub_thread_wins_race = false;
   drv_decline           = decline;
   ol = pack_new(&iface_textures, NUM_IMAGES);

   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_PIXELS, "%s: went as textures", lane);
   stub_tex_load_fails = upload_fails;
   stub_video_thread_run();
   video_thread_async_poll();
   CHECK(!input_overlay_promote_textures(ol), "%s: promoted", lane);
   CHECK(ol->flags & INPUT_OVERLAY_TEXTURES_DECLINED, "%s: not remembered", lane);
   CHECK(stub_tex_live == 0, "%s: %u textures held for nothing", lane,
         stub_tex_live);
   for (i = 0; i < NUM_IMAGES; i++)
      CHECK(ol->images[i]->pixels != NULL, "%s: image %u lost its pixels",
            lane, i);
   pack_show(ol, 1);
   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_PIXELS, "%s: page 1 went as textures", lane);
   CHECK(drv_null_pixels == 0, "%s: page 1 had no pixels", lane);
   check_page(lane, 1);

   pack_free(ol);
   stub_thread_active = false;
   printf("[%s] %s lane\n", failures == before ? "pass" : "fail", lane);
}

/* The pack is freed, or the driver goes, with the uploads still
 * queued, and the video thread gets to them afterwards: it reads
 * pixels that went with the surfaces, not pixels that were freed
 * under it, and nothing is left behind. A pack that lost its pixels
 * this way and lives on says it has nothing to come back from. */
static void lane_threaded_freed_in_flight(void)
{
   unsigned before = failures;
   input_overlay_t *ol;
   drv_reset();
   stub_thread_active    = true;
   stub_thread_wins_race = false;

   ol = pack_new(&iface_textures, NUM_IMAGES);
   input_overlay_load_page(ol);
   pack_free(ol);
   CHECK(stub_video_thread_run() == NUM_IMAGES,
         "freed in flight: the uploads were not in flight");
   video_thread_async_poll();
   CHECK(stub_tex_live == 0, "freed in flight: %u textures outlived the pack",
         stub_tex_live);

   ol = pack_new(&iface_textures, NUM_IMAGES);
   input_overlay_load_page(ol);
   input_overlay_release_textures(ol);      /* video teardown */
   stub_video_thread_run();
   video_thread_async_poll();
   CHECK(!input_overlay_has_source(ol),
         "released in flight: reported as reusable");
   drv_loads = 0;
   ol->flags &= ~INPUT_OVERLAY_TEXTURES_DECLINED;
   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_NONE, "released in flight: went as textures");
   CHECK(drv_loads == 0 && drv_null_pixels == 0,
         "released in flight: load() was handed a pack with no pixels");
   pack_free(ol);
   CHECK(stub_tex_live == 0, "released in flight: %u textures left",
         stub_tex_live);

   stub_thread_active = false;
   printf("[%s] threaded, freed in flight lane\n",
         failures == before ? "pass" : "fail");
}
#endif

/* A looping animated image whose frame the driver drops: the frame is
 * marked and sent again from its slot, and only then reaches the
 * texture. Direct, and under the wrapper, where the drop comes back
 * through the surface's release. */
static void lane_anim_dropped(bool threaded)
{
   static int dummy_stream;
   unsigned before = failures;
   const char *lane = threaded ? "threaded, dropped animation frame"
                               : "dropped animation frame";
   input_overlay_t *ol;
   gfx_surface_t *s;
   drv_reset();
#ifdef HAVE_THREADS
   stub_thread_active    = threaded;
   stub_thread_wins_race = threaded;
#endif
   ol = pack_new(&iface_textures, NUM_IMAGES);
   ol->anim_stream    = (void**)calloc(NUM_IMAGES, sizeof(void*));
   ol->anim_player    = (void**)calloc(NUM_IMAGES, sizeof(void*));
   ol->anim_resend    = (uint8_t*)calloc(NUM_IMAGES, sizeof(uint8_t));
   ol->anim_stream[0] = &dummy_stream; /* image 0 animates */
   pack_show(ol, 0);
   input_overlay_load_page(ol);
#ifdef HAVE_THREADS
   if (threaded)
      video_thread_async_poll();
#endif
   s = ol->surfaces ? (gfx_surface_t*)ol->surfaces[0] : NULL;
   CHECK(s && s->num_slots == 1 && s->handle, "%s: no animated surface", lane);
   if (s && s->num_slots == 1 && s->handle)
   {
      struct texture_image want;
      uint32_t sum;
      size_t p, n = VIDEO_SCALE_AREA(s->dims);
      for (p = 0; p < n; p++)
         s->slots[0][p] = 0xff00ff00u ^ (uint32_t)p;
      memset(&want, 0, sizeof(want));
      want.width  = VIDEO_SCALE_W(s->dims);
      want.height = VIDEO_SCALE_H(s->dims);
      want.pixels = s->slots[0];
      sum         = stub_checksum(&want);

      stub_drop_updates = 1;
      CHECK(input_overlay_anim_submit(ol, 0), "%s: not taken", lane);
#ifdef HAVE_THREADS
      if (threaded)
         video_thread_async_poll();
#endif
      CHECK(ol->anim_resend[0], "%s: not marked to go again", lane);
      CHECK(stub_texture_get(s->handle)->checksum != sum,
            "%s: the dropped frame reached the texture", lane);
      CHECK(input_overlay_anim_submit(ol, 0), "%s: resend not taken", lane);
#ifdef HAVE_THREADS
      if (threaded)
         video_thread_async_poll();
#endif
      CHECK(!ol->anim_resend[0] && stub_texture_get(s->handle)->checksum == sum,
            "%s: the frame sent again did not land", lane);
   }
   stub_drop_updates = 0;
   pack_free(ol);
#ifdef HAVE_THREADS
   stub_thread_active = stub_thread_wins_race = false;
#endif
   printf("[%s] %s lane\n", failures == before ? "pass" : "fail", lane);
}

/* A two-frame image is one texture, both frames stacked with a row of
 * each repeated between them: a page that shows it is pointed at the
 * frame it shows, and a press moves the rectangle and uploads nothing.
 * The stacked frames go once the texture is in. Direct, and under the
 * wrapper. */
static unsigned atlas_geoms;
static int      atlas_entry;
static float    atlas_v, atlas_h;

static void drv_tex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   (void)data; (void)x; (void)w;
   atlas_geoms++;
   atlas_entry = (int)image;
   atlas_v     = y;
   atlas_h     = h;
}

static const video_overlay_interface_t iface_atlas = {
   NULL, drv_load, drv_load_tex, drv_tex_geom, NULL, NULL, NULL
};

static void lane_two_frame_atlas(bool threaded)
{
   static int dummy_stream;
   unsigned before = failures;
   const char *lane = threaded ? "threaded, two-frame atlas"
                               : "two-frame atlas";
   unsigned w = img_w[0], h = img_h[0], rows = 2 * h + 2;
   uint32_t *stack, sum;
   struct texture_image want;
   input_overlay_t *ol;
   gfx_surface_t *s;
   size_t p;

   drv_reset();
   atlas_geoms = 0;
   atlas_entry = -1;
#ifdef HAVE_THREADS
   stub_thread_active    = threaded;
   stub_thread_wins_race = threaded;
#endif
   ol = pack_new(&iface_atlas, NUM_IMAGES);
   ol->anim_stream         = (void**)calloc(NUM_IMAGES, sizeof(void*));
   ol->anim_player         = (void**)calloc(NUM_IMAGES, sizeof(void*));
   ol->anim_resend         = (uint8_t*)calloc(NUM_IMAGES, sizeof(uint8_t));
   ol->anim_2frame         = (uint8_t*)calloc(NUM_IMAGES, sizeof(uint8_t));
   ol->anim_2frame_cur     = (uint8_t*)calloc(NUM_IMAGES, sizeof(uint8_t));
   ol->anim_2frame_pressed = (uint8_t*)calloc(NUM_IMAGES, sizeof(uint8_t));
   ol->anim_2frame_pix     = (uint32_t**)calloc(NUM_IMAGES, sizeof(uint32_t*));
   ol->anim_stream[0]      = &dummy_stream; /* image 0 is a pair */
   ol->anim_2frame[0]      = INPUT_OVERLAY_2FRAME_PAIR;
   stack = (uint32_t*)malloc((size_t)w * rows * sizeof(uint32_t));
   for (p = 0; p < (size_t)w * rows; p++)
      stack[p] = 0xff000000u | (uint32_t)(p * 2654435761u);
   ol->anim_2frame_pix[0] = stack;
   memset(&want, 0, sizeof(want));
   want.width  = w;
   want.height = rows;
   want.pixels = stack;
   sum         = stub_checksum(&want);

   pack_show(ol, 0);
   input_overlay_load_page(ol);
#ifdef HAVE_THREADS
   if (threaded)
   {
      video_thread_async_poll();
      input_overlay_promote_textures(ol);
   }
#endif
   s = ol->surfaces ? (gfx_surface_t*)ol->surfaces[0] : NULL;
   CHECK(s && s->handle && !s->num_slots
         && VIDEO_SCALE_W(s->dims) == w && VIDEO_SCALE_H(s->dims) == rows,
         "%s: not one texture of both frames", lane);
   CHECK(ol->anim_2frame[0] == INPUT_OVERLAY_2FRAME_ATLAS,
         "%s: not marked as stacked", lane);
   if (s && s->handle)
      CHECK(stub_texture_get(s->handle)->checksum == sum,
            "%s: the texture is not the stack", lane);
   CHECK(!ol->anim_2frame_pix[0], "%s: the stack was kept after its upload", lane);
   CHECK(atlas_geoms == 1 && atlas_entry == 0 && atlas_v == 0.0f
         && atlas_h == (float)h / (float)rows,
         "%s: the page was not pointed at the first frame (%u calls, entry %d, %f+%f)",
         lane, atlas_geoms, atlas_entry, atlas_v, atlas_h);

   /* pressed: the rectangle moves past the first frame and the two
    * repeated rows; nothing is uploaded */
   {
      unsigned loads = stub_tex_loads;
      ol->anim_2frame_cur[0] = 1;
      input_overlay_atlas_frames(ol, 0);
      CHECK(atlas_geoms == 2 && atlas_entry == 0
            && atlas_v == (float)(h + 2) / (float)rows
            && atlas_h == (float)h / (float)rows,
            "%s: a press did not move the rectangle to the second frame", lane);
      CHECK(stub_tex_loads == loads
            && (!s || !s->handle || stub_texture_get(s->handle)->checksum == sum),
            "%s: a press uploaded something", lane);
      /* the other page does not show it: nothing to point */
      pack_show(ol, 1);
      input_overlay_atlas_frames(ol, 0);
      CHECK(atlas_geoms == 2, "%s: a page without the image was pointed", lane);
   }
   pack_free(ol);
#ifdef HAVE_THREADS
   stub_thread_active = stub_thread_wins_race = false;
#endif
   printf("[%s] %s lane\n", failures == before ? "pass" : "fail", lane);
}

/* A DDS image comes from the loader as its BC1 blocks and no pixels.
 * A driver that samples BC1 gets the blocks as one texture; one that
 * does not, or a page that goes through load(), gets them decoded, on
 * both pages that show the image. */
static void lane_bcn(const char *lane, const video_overlay_interface_t *iface,
      bool samples, bool decline)
{
   unsigned pg, before = failures;
   bool textures       = iface->load_textures && !decline;
   input_overlay_t *ol;

   drv_reset();
   stub_samples_bc1   = samples;
   drv_decline        = decline;
   pack_bcn_image     = BCN_IMAGE;
   ol                 = pack_new(iface, NUM_IMAGES);
   pack_bcn_image     = NUM_IMAGES;
   img_sum[BCN_IMAGE] = (textures && samples) ? bcn_sum_bytes : bcn_sum_rgba;

   for (pg = 0; pg < NUM_PAGES; pg++)
   {
      enum input_overlay_page r;
      pack_show(ol, pg);
      r = input_overlay_load_page(ol);
      CHECK(r == (textures ? INPUT_OVERLAY_PAGE_TEXTURES
               : INPUT_OVERLAY_PAGE_PIXELS),
            "%s: page %u went as %d", lane, pg, (int)r);
      CHECK(drv_null_pixels == 0, "%s: page %u had no pixels", lane, pg);
      check_page(lane, pg);
   }

   if (textures)
   {
      const struct stub_texture *t = ol->page_textures
         ? stub_texture_get(ol->page_textures[BCN_IMAGE]) : NULL;
      CHECK(stub_tex_loads == NUM_IMAGES,
            "%s: %u uploads for %u unique images", lane, stub_tex_loads,
            NUM_IMAGES);
      CHECK(t && t->bcn == samples, "%s: the texture is %s", lane,
            !t ? "missing" : t->bcn ? "the blocks" : "decoded");
      CHECK(!ol->images[BCN_IMAGE]->compressed
            && !ol->images[BCN_IMAGE]->pixels,
            "%s: the image outlived its upload", lane);
      CHECK(ol->images[BCN_IMAGE]->width == img_w[BCN_IMAGE],
            "%s: the image lost its size", lane);
      CHECK(input_overlay_has_source(ol), "%s: pack is not reusable", lane);
      input_overlay_release_textures(ol);
      CHECK(stub_tex_live == 0, "%s: %u textures outlived the release",
            lane, stub_tex_live);
   }
   else
   {
      CHECK(drv_loads == NUM_PAGES, "%s: load() reached %u times", lane,
            drv_loads);
      CHECK(stub_tex_live == 0, "%s: %u textures held for nothing", lane,
            stub_tex_live);
      CHECK(input_overlay_has_source(ol), "%s: pack is not reusable", lane);
   }

   pack_free(ol);
   printf("[%s] %s lane\n", failures == before ? "pass" : "fail", lane);
}

#ifdef HAVE_THREADS
/* Under the wrapper the blocks are posted, not uploaded on the caller.
 * Until the handle lands the page goes through load(), decoded; then
 * it moves over to the blocks and both the payload and the decode go.
 * With @free_early the pack is released and freed while the upload is
 * still queued: the video thread then reads a payload that went with
 * the surface, and nothing outlives the completion. */
static void lane_bcn_threaded(const char *lane, bool free_early)
{
   unsigned before = failures;
   input_overlay_t *ol;

   drv_reset();
   stub_thread_active    = true;
   stub_thread_wins_race = false;
   stub_samples_bc1      = true;
   pack_bcn_image        = BCN_IMAGE;
   ol                    = pack_new(&iface_textures, NUM_IMAGES);
   pack_bcn_image        = NUM_IMAGES;
   img_sum[BCN_IMAGE]    = bcn_sum_rgba;

   CHECK(input_overlay_load_page(ol) == INPUT_OVERLAY_PAGE_PIXELS,
         "%s: page 0 did not go through load() while pending", lane);
   CHECK(drv_null_pixels == 0, "%s: load() had no pixels", lane);
   check_page(lane, 0);
   CHECK(stub_tex_loads == 0, "%s: uploaded on the caller (%u)", lane,
         stub_tex_loads);

   if (free_early)
   {
      input_overlay_release_textures(ol);
      pack_free(ol);
      CHECK(stub_video_thread_run() == NUM_IMAGES,
            "%s: the uploads were not all queued", lane);
      video_thread_async_poll();
      CHECK(stub_tex_live == 0, "%s: %u textures outlived the pack", lane,
            stub_tex_live);
   }
   else
   {
      const struct stub_texture *t;
      CHECK(stub_video_thread_run() == NUM_IMAGES,
            "%s: the uploads were posted more than once", lane);
      video_thread_async_poll();
      img_sum[BCN_IMAGE] = bcn_sum_bytes;
      CHECK(input_overlay_promote_textures(ol), "%s: not promoted", lane);
      check_page(lane, 0);
      t = ol->page_textures
         ? stub_texture_get(ol->page_textures[BCN_IMAGE]) : NULL;
      CHECK(t && t->bcn, "%s: the texture is not the blocks", lane);
      CHECK(!ol->images[BCN_IMAGE]->compressed
            && !ol->images[BCN_IMAGE]->pixels,
            "%s: the image outlived its upload", lane);
      pack_free(ol);
      CHECK(stub_tex_live == 0, "%s: %u textures outlived the pack", lane,
            stub_tex_live);
   }

   stub_thread_active = false;
   printf("[%s] %s lane\n", failures == before ? "pass" : "fail", lane);
}
#endif

int main(void)
{
   lane_textures();
   lane_bcn("bcn, sampled",  &iface_textures, true,  false);
   lane_bcn("bcn, decoded",  &iface_textures, false, false);
   lane_bcn("bcn, declined", &iface_textures, true,  true);
   lane_bcn("bcn, load()",   &iface_pixels,   true,  false);
   lane_pixels("declined",     &iface_textures, true,  false);
   lane_pixels("no such path", &iface_pixels,   false, false);
   lane_pixels("upload fails", &iface_textures, false, true);
   lane_load_fails();
   lane_no_images();
   lane_anim_dropped(false);
   lane_two_frame_atlas(false);
#ifdef HAVE_THREADS
   lane_anim_dropped(true);
   lane_two_frame_atlas(true);
   lane_threaded_prompt();
   lane_threaded_late();
   lane_threaded_late_refused("threaded, late and declined", true,  false);
   lane_threaded_late_refused("threaded, late and failed",   false, true);
   lane_threaded_freed_in_flight();
   lane_bcn_threaded("bcn, threaded",                false);
   lane_bcn_threaded("bcn, threaded, freed in flight", true);
#endif

   if (failures)
   {
      printf("FAIL overlay_textures_test (%u)\n", failures);
      return 1;
   }
   printf("PASS overlay_textures_test\n");
   return 0;
}
