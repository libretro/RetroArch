/* Regression test for the overlay visibility index bound in
 * input/input_driver.c::input_overlay_set_visibility().
 *
 * The index does not come from the overlay. The overlay LED driver
 * forwards settings->uints.led_map[], which configuration.c reads
 * straight out of the config file with CONFIG_GET_INT_BASE and no range
 * of its own, so "led1_map = 500" in a config reaches the setter as 500.
 * Unbounded, that wrote past overlay_visibility[MAX_VISIBILITY] and then
 * handed the same index to the video driver's set_alpha(), which indexes
 * its own per-image storage with it (gl2_overlay_set_alpha() reaches
 * overlay_color_coord[image * 16]).
 *
 * input_overlay_get_visibility() has always bounded its read; the write
 * side did not. This pins both halves:
 *
 *   1. the LED driver really does forward config values as they are, so
 *      the setter cannot assume a caller has checked - this half builds
 *      the shipping led/drivers/led_overlay.c and watches what comes out
 *      of it;
 *   2. the bound itself accepts every index the array has and rejects
 *      everything else.
 *
 * The predicate in part 2 is kept verbatim from the setter, since that
 * function needs the whole input driver behind it to link. If
 * input_driver.c changes the bound, the copy here must follow.
 */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "../../../input/input_overlay.h"
#include "../../../configuration.h"
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

/* --- what the LED overlay driver hands the setter -------------------- */

#define MAX_SEEN 64

static int      seen[MAX_SEEN];
static unsigned seen_count;

/* Stands in for input/input_driver.c's exported setter so the shipping
 * led/drivers/led_overlay.c can be built and driven here. */
void input_overlay_set_visibility(int overlay_idx,
      enum overlay_visibility vis)
{
   (void)vis;
   if (seen_count < MAX_SEEN)
      seen[seen_count++] = overlay_idx;
}

static settings_t stub_settings;

settings_t *config_get_ptr(void)
{
   return &stub_settings;
}


static void test_led_driver_forwards_config_values(void)
{
   unsigned i;
   bool out_of_range_seen = false;

   printf("what the overlay LED driver forwards\n");

   for (i = 0; i < MAX_LEDS; i++)
      stub_settings.uints.led_map[i] = (unsigned)-1;

   /* A config an ordinary user could write: valid, and far past the end. */
   stub_settings.uints.led_map[0] = 3;
   stub_settings.uints.led_map[1] = MAX_VISIBILITY;
   stub_settings.uints.led_map[2] = 500;

   seen_count = 0;
   overlay_led_driver.init();

   check(seen_count == 3,
         "only the mapped entries are forwarded, unmapped ones are skipped");

   for (i = 0; i < seen_count; i++)
      if (seen[i] < 0 || seen[i] >= MAX_VISIBILITY)
         out_of_range_seen = true;

   check(out_of_range_seen,
         "config values outside the array arrive at the setter unaltered");

   /* The per-LED entry point forwards just as directly. */
   seen_count = 0;
   overlay_led_driver.set_led(2, 1);
   check(seen_count == 1 && seen[0] == 500,
         "set_led forwards its mapped value without a range of its own");
}

/* --- the bound the setter applies ------------------------------------ */

/* Verbatim from input_overlay_set_visibility(). */
static bool index_is_rejected(int overlay_idx)
{
   return (overlay_idx < 0 || overlay_idx >= MAX_VISIBILITY);
}

static void test_bound_covers_the_array_exactly(void)
{
   printf("the bound applied to the index\n");

   check(!index_is_rejected(0), "the first slot is accepted");
   check(!index_is_rejected(MAX_VISIBILITY - 1), "the last slot is accepted");
   check(index_is_rejected(MAX_VISIBILITY),
         "one past the end is rejected");
   check(index_is_rejected(500),
         "a config value well past the end is rejected");
   check(index_is_rejected(-1),
         "the unmapped sentinel is rejected if it ever gets through");
}

int main(void)
{
   memset(&stub_settings, 0, sizeof(stub_settings));

   test_led_driver_forwards_config_values();
   test_bound_covers_the_array_exactly();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("overlay visibility index bounds hold\n");
   return 0;
}
