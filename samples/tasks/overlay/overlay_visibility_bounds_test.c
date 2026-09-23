/* Regression test for which overlay images the overlay LED driver
 * hides: input/input_overlay_leds.c, built from the tree, and the
 * shipping led/drivers/led_overlay.c in front of it.
 *
 * ledN_map names a slot of whatever page is loaded. Applied to every
 * pack, a config made for an LED overlay blanked the controls at those
 * slots of a gamepad pack - the d-pad arms at slots 1-3 of neo-retropad
 * and of the flat Dreamcast pack, showing only while pressed. Now:
 *
 *   1. the LED driver only reports the core's LEDs, as they come;
 *   2. a map entry reaches only the image of a "nul" desc - a control is
 *      never an LED - and an entry off the end of the page, as the
 *      config can say, is never handed to the video driver;
 *   3. a pack that names its LED images (_led) is not mapped at all:
 *      those images, and only those, show their LED's state;
 *   4. with the overlay LED driver off (no map), nothing is hidden.
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

static void stub_set_alpha(void *data, unsigned image, float mod)
{
   (void)data;
   if (image >= IMAGES)
      calls_off_page++;
   else
      alpha[image] = mod;
}

static video_overlay_interface_t stub_iface;

static struct overlay_desc descs[IMAGES];
static struct overlay      page;
static input_overlay_t     pack;
static unsigned            led_map[MAX_LEDS];

/* A page of IMAGES descs, desc i showing image i: "nul" where @nul has
 * bit i, a control otherwise; @leds[i] the _led of desc i. */
static void build(unsigned nul, const uint8_t *leds)
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
      if (nul & (1u << i))
         descs[i].flags |= OVERLAY_DESC_DISPLAY_ONLY;
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
}

static bool hidden(unsigned image, uint32_t lit, const unsigned *map)
{
   return input_overlay_image_hidden(&pack, image, lit, map);
}

static void test_map_reaches_nul_images_only(void)
{
   printf("ledN_map on a pack that names no LEDs\n");

   /* Images 0-3 are the controls of a gamepad pack; 4 is a "nul" LED. */
   build(1u << 4, NULL);
   led_map[0] = 1;
   led_map[1] = 2;
   led_map[2] = 3;
   led_map[3] = 4;

   check(!hidden(1, 0, led_map) && !hidden(2, 0, led_map)
         && !hidden(3, 0, led_map),
         "the controls at mapped slots 1-3 are not hidden");
   check(hidden(4, 0, led_map), "the nul image at a mapped slot is hidden "
         "while its LED is off");
   check(!hidden(4, 1u << 3, led_map), "and shown while it is lit");

   input_overlay_hide_leds(&pack, 0, led_map);
   check(alpha[1] == 1.0f && alpha[2] == 1.0f && alpha[3] == 1.0f,
         "hide_leds leaves the controls alone");
   check(alpha[4] == 0.0f, "hide_leds hides the nul image");
}

static void test_map_off_the_page(void)
{
   printf("ledN_map entries the page does not have\n");

   build(0x1f, NULL);
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

   /* Image 1 is a control that shows LED 1; image 2 is a nul button
    * ledN_map points at, which a pack naming its LEDs does not use. */
   build(1u << 2, leds);
   led_map[1] = 2;

   check(hidden(1, 0, led_map), "a desc's image is hidden while its LED "
         "is off, control or not");
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

   build(0x1f, leds);
   check(!hidden(1, 0, NULL), "nothing is hidden without a map");
   input_overlay_hide_leds(&pack, 0, NULL);
   check(alpha[1] == 1.0f, "hide_leds does nothing without a map");
}

int main(void)
{
   test_led_driver();
   test_map_reaches_nul_images_only();
   test_map_off_the_page();
   test_named_leds();
   test_driver_off();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("overlay LED images hold\n");
   return 0;
}
