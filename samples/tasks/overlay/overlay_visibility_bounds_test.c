/* Regression test for which overlay images the overlay LED driver
 * hides: input/input_overlay_alpha.c, built from the tree, and the
 * shipping led/drivers/led_overlay.c in front of it.
 *
 * ledN_map names a slot of whatever page is loaded:
 *
 *   1. the LED driver only reports the core's LEDs, as they come;
 *   2. a map entry reaches the image at its slot whatever that desc
 *      does when pressed - an LED pack may put its lights on keys or
 *      buttons - and an entry off the end of the page, as the config
 *      can say, is never handed to the video driver;
 *   3. a pack that names its LED images (_led) is not mapped at all:
 *      those images, and only those, show their LED's state;
 *   4. with the overlay LED driver off (no map), nothing is hidden.
 *
 * The same file hands each image its alpha at every input poll, and
 * sets only what changed: D3D10/11/12 map the sprite buffer for every
 * set, and the threaded wrapper replays the page's alphas whenever one
 * is set, so a pass that re-sent all of them cost a page of maps per
 * frame for nothing. Pinned here:
 *
 *   5. a pass that changes nothing sets nothing;
 *   6. a press sets its image once, and the release sets it back;
 *   7. after a page load (forget) every image is set again;
 *   8. an image the LED driver hid between passes is not left hidden
 *      when its LED comes back on before the next pass;
 *   9. without the cache block every image is set every pass.
 */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "../../../input/input_overlay.h"
#include "../../../led/led_defines.h"
#include "../../../led/led_driver.h"

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   if (cond)
      printf("  [pass] %s\n", what);
   else
   {
      printf("  [FAIL] %s\n", what);
      failures++;
   }
}

/* --- the LED driver's side ------------------------------------------- */

static int  enabled   = -1;
static int  last_led  = -2;
static bool last_lit  = false;

void input_overlay_leds_enable(bool enable)
{
   enabled = enable ? 1 : 0;
}

void input_overlay_set_led(int led, bool lit)
{
   last_led = led;
   last_lit = lit;
}

static void test_led_driver(void)
{
   printf("what the overlay LED driver reports\n");

   overlay_led_driver.init();
   check(enabled == 1, "init turns the overlay's LEDs on");

   overlay_led_driver.set_led(2, 1);
   check(last_led == 2 && last_lit, "an LED coming on arrives as it is");
   overlay_led_driver.set_led(500, 0);
   check(last_led == 500 && !last_lit,
         "an LED number off the end arrives unaltered, for the frontend "
         "to bound");

   overlay_led_driver.free();
   check(enabled == 0, "free turns them off");
}

/* --- which images are hidden ----------------------------------------- */

#define IMAGES 5

static float alpha[IMAGES];
static unsigned calls_off_page;
static unsigned calls;

static void stub_set_alpha(void *data, unsigned image, float mod)
{
   (void)data;
   calls++;
   if (image >= IMAGES)
      calls_off_page++;
   else
      alpha[image] = mod;
}

static video_overlay_interface_t stub_iface;

static struct overlay_desc descs[IMAGES];
static float               alpha_block[2 * IMAGES];
static struct overlay      page;
static input_overlay_t     pack;
static unsigned            led_map[MAX_LEDS];

/* A page of IMAGES descs, desc i showing image i; @leds[i] the _led of
 * desc i. */
static void build(const uint8_t *leds)
{
   unsigned i;

   memset(descs, 0, sizeof(descs));
   memset(&page, 0, sizeof(page));
   memset(&pack, 0, sizeof(pack));

   for (i = 0; i < IMAGES; i++)
   {
      descs[i].image_index  = i;
      descs[i].image.width  = 16;
      descs[i].image.height = 16;
      if (leds && leds[i])
      {
         descs[i].led = leds[i];
         pack.flags  |= INPUT_OVERLAY_HAS_LEDS;
      }
   }
   page.descs            = descs;
   page.size             = IMAGES;
   page.load_images_size = IMAGES;

   stub_iface.set_alpha  = stub_set_alpha;
   pack.iface            = &stub_iface;
   pack.active           = &page;

   for (i = 0; i < MAX_LEDS; i++)
      led_map[i] = (unsigned)-1;
   for (i = 0; i < IMAGES; i++)
      alpha[i] = 1.0f;
   calls_off_page = 0;
   calls          = 0;
}

/* The cache block, as input_overlay_loaded() makes it. */
static void with_cache(void)
{
   pack.alpha_sent = alpha_block;
   pack.alpha_want = alpha_block + IMAGES;
   pack.alpha_cap  = IMAGES;
   input_overlay_alpha_forget(&pack);
}

static unsigned pass(float mod, bool show_input, uint32_t lit,
      const unsigned *map)
{
   unsigned before = calls;
   input_overlay_alpha_pass(&pack, mod, show_input, mod, lit, map);
   return calls - before;
}

static bool hidden(unsigned image, uint32_t lit, const unsigned *map)
{
   return input_overlay_image_hidden(&pack, image, lit, map);
}

static void test_map_reaches_its_slots(void)
{
   printf("ledN_map on a pack that names no LEDs\n");

   /* Slots 1-4 are mapped, slot 0 is not. */
   build(NULL);
   led_map[0] = 1;
   led_map[1] = 2;
   led_map[2] = 3;
   led_map[3] = 4;

   check(!hidden(0, 0, led_map), "an unmapped slot is not hidden");
   check(hidden(1, 0, led_map) && hidden(2, 0, led_map)
         && hidden(3, 0, led_map) && hidden(4, 0, led_map),
         "every mapped slot is hidden while its LED is off");
   check(!hidden(4, 1u << 3, led_map) && hidden(1, 1u << 3, led_map),
         "and shown while its own LED is lit");

   input_overlay_hide_leds(&pack, 1u << 1, led_map);
   check(alpha[0] == 1.0f && alpha[2] == 1.0f,
         "hide_leds leaves the unmapped and the lit slots alone");
   check(alpha[1] == 0.0f && alpha[3] == 0.0f && alpha[4] == 0.0f,
         "hide_leds hides the unlit mapped slots");
}

static void test_map_off_the_page(void)
{
   printf("ledN_map entries the page does not have\n");

   build(NULL);
   led_map[0] = IMAGES;
   led_map[1] = 500;
   led_map[2] = (unsigned)-1;

   input_overlay_hide_leds(&pack, 0, led_map);
   check(calls_off_page == 0,
         "no index off the end of the page reaches set_alpha");
   check(!hidden(0, 0, led_map), "an unmapped image is not hidden");
}

static void test_named_leds(void)
{
   static const uint8_t leds[IMAGES] = { 0, 1, 0, 0, 32 };

   printf("a pack that names its LED images\n");

   /* Image 1 shows LED 1; image 2 is a slot ledN_map points at, which
    * a pack naming its LEDs does not use. */
   build(leds);
   led_map[1] = 2;

   check(hidden(1, 0, led_map), "a desc's image is hidden while its LED "
         "is off");
   check(!hidden(1, 1u << 0, led_map), "and shown while it is lit");
   check(!hidden(2, 0, led_map), "ledN_map is not applied");
   check(hidden(4, 0, led_map) && !hidden(4, 1u << 31, led_map),
         "LED 32 is the last bit");

   input_overlay_hide_leds(&pack, 1u << 31, led_map);
   check(alpha[1] == 0.0f && alpha[2] == 1.0f && alpha[4] == 1.0f,
         "hide_leds hides exactly the unlit LED images");
}

static void test_driver_off(void)
{
   static const uint8_t leds[IMAGES] = { 0, 1, 0, 0, 0 };

   printf("the overlay LED driver not in use\n");

   build(leds);
   check(!hidden(1, 0, NULL), "nothing is hidden without a map");
   input_overlay_hide_leds(&pack, 0, NULL);
   check(alpha[1] == 1.0f, "hide_leds does nothing without a map");
}

static void test_alpha_cache(void)
{
   unsigned n;

   printf("the alpha pass sets what changed\n");

   build(NULL);
   with_cache();
   descs[2].alpha_mod = 2.0f;

   check(pass(0.7f, true, 0, NULL) == IMAGES,
         "the first pass after a load sets every image");
   check(pass(0.7f, true, 0, NULL) == 0, "the same pass again sets none");

   descs[2].touch_mask = 1;
   n = pass(0.7f, true, 0, NULL);
   check(n == 1 && alpha[2] == 2.0f * 0.7f,
         "a press sets its image once, to alpha_mod * opacity");
   check(pass(0.7f, true, 0, NULL) == 0, "held, nothing more is set");
   descs[2].touch_mask = 0;
   n = pass(0.7f, true, 0, NULL);
   check(n == 1 && alpha[2] == 0.7f, "the release sets it back");

   check(pass(0.5f, false, 0, NULL) == IMAGES,
         "an opacity change sets every image");

   input_overlay_alpha_forget(&pack);
   check(pass(0.5f, false, 0, NULL) == IMAGES,
         "after a page load every image is set again");
}

static void test_alpha_cache_leds(void)
{
   printf("the alpha pass and the LED driver\n");

   /* Image 4 is the LED image at led1_map's slot. */
   build(NULL);
   with_cache();
   led_map[0] = 4;

   pass(1.0f, false, 1u << 0, led_map);
   check(alpha[4] == 1.0f, "lit, the LED image is shown");

   /* The LED goes off and on again between two passes: the hide is
    * immediate, and the pass after must show the image again. */
   input_overlay_hide_leds(&pack, 0, led_map);
   check(alpha[4] == 0.0f, "going out, it is hidden at once");
   pass(1.0f, false, 1u << 0, led_map);
   check(alpha[4] == 1.0f, "lit again before the next pass, it is shown");
}

static void test_alpha_no_cache(void)
{
   printf("the alpha pass without its cache block\n");

   build(NULL);
   check(pass(0.7f, false, 0, NULL) == IMAGES
         && pass(0.7f, false, 0, NULL) == IMAGES,
         "every image is set every pass");
   descs[1].touch_mask = 1;
   descs[1].alpha_mod  = 2.0f;
   pass(0.7f, true, 0, NULL);
   check(alpha[1] == 2.0f * 0.7f, "a press is still lit");
}

int main(void)
{
   test_led_driver();
   test_map_reaches_its_slots();
   test_map_off_the_page();
   test_named_leds();
   test_driver_off();
   test_alpha_cache();
   test_alpha_cache_leds();
   test_alpha_no_cache();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("overlay LED images hold\n");
   return 0;
}
