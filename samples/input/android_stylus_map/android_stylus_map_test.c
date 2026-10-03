/* android_stylus_map_test.c -- an Android pen, through the mapping
 * that turns its events into pointer and mouse state.
 *
 * The Android input driver reports a pen through the devices it
 * already has: its position is pointer 0, a press is pointer 0
 * pressed, and the barrel button is the right mouse button.
 *
 * The bug this pins: the pen support first gave pointer indices a
 * meaning of its own - index 1 for the tip pressing, index 2 for the
 * barrel button - and raised the pointer count to 2 or 3 to match.
 * Everything that counts pointers read a pressing pen as two fingers
 * and a press with the barrel button as three; the driver's own
 * lightgun mapping made those turbo and reload.
 *
 * The claims:
 *
 *   1. A pen is one touch: no event, under any setting, asks for a
 *      pointer count above 1.
 *   2. The tip pressing is pointer 0 pressed, and lifting releases it.
 *   3. A tip resting more lightly than the pressure threshold moves
 *      the pointer without pressing it.
 *   4. A hovering pen moves the pointer only if the user allowed it,
 *      and never presses while contact is required.
 *   5. A pen that is not pressing leaves the pointer count alone, so a
 *      hover does not release a finger; a pressing pen claims it on
 *      every event, so a finger lifting in between does not release
 *      the pen.
 *   6. The barrel button is the right mouse button, hovering or
 *      touching, and is let go when the pen leaves range or its
 *      gesture is cancelled.
 *   7. With contact not required, the barrel button presses while
 *      hovering, at the pen's position, and releasing it releases.
 *   8. Before the tip has touched, a move that still reports distance
 *      leaves the position alone.
 *   9. Sensitivity maps to a threshold that only falls as it rises,
 *      and out-of-range values are clamped.
 *
 * The decision is the driver's own, from android_stylus_map.h. The
 * driver's side - the pointer count and the right mouse button it
 * applies the result to - is reproduced here, because the driver only
 * builds against the NDK. A sabotage mode restores the old pointer
 * counts and is asserted to be caught. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "android_stylus_map.h"

static unsigned failures = 0;
static bool     quiet    = false;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         if (!quiet) \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* What the old mapping did with the pointer count: 1 for contact, 2
 * with the tip pressing, 3 with the barrel button as well. */
static bool sabotage_old_counts = false;

/* The driver's state, as far as the mapping touches it. */
struct driver
{
   android_stylus_state_t stylus;
   int  pointer_count;
   bool mouse_r;
   bool moved;           /* the last event wrote a position */
   int  max_count_asked; /* the highest count any event asked for */
};

static void driver_reset(struct driver *d)
{
   memset(d, 0, sizeof(*d));
}

/* android_stylus_event() in the driver. */
static void pen(struct driver *d, const android_stylus_cfg_t *cfg,
      enum android_stylus_event event, bool side_pressed,
      float pressure, float distance)
{
   android_stylus_result_t res;

   android_stylus_map(&d->stylus, cfg, event, side_pressed,
         pressure, distance, &res);

   if (sabotage_old_counts && event == ANDROID_STYLUS_CONTACT
         && res.pointer_count == 1)
   {
      res.pointer_count = 2;
      if (side_pressed)
         res.pointer_count = 3;
   }

   d->moved   = res.set_position;
   d->mouse_r = res.mouse_r;

   if (res.pointer_count != ANDROID_STYLUS_COUNT_KEEP)
   {
      d->pointer_count = res.pointer_count;
      if (res.pointer_count > d->max_count_asked)
         d->max_count_asked = res.pointer_count;
   }
}

static const android_stylus_cfg_t cfg_default  = { true,  true,  70 };
static const android_stylus_cfg_t cfg_no_hover = { true,  false, 70 };
static const android_stylus_cfg_t cfg_barrel   = { false, true,  70 };

#define FIRM  0.5f   /* well past any threshold */
#define LIGHT 0.001f /* below the default threshold (0.0075) */

/* 2, 3, 6: press with the tip, with the barrel button along. */
static void lane_press_and_lift(void)
{
   struct driver d;
   driver_reset(&d);

   pen(&d, &cfg_default, ANDROID_STYLUS_HOVER, false, 0.0f, 1.0f);
   CHECK(d.pointer_count == 0, "a hovering pen pressed the pointer");
   CHECK(d.moved, "a hovering pen did not move the pointer");

   pen(&d, &cfg_default, ANDROID_STYLUS_CONTACT, false, FIRM, 0.0f);
   CHECK(d.pointer_count == 1, "the tip pressing is not one pressed pointer");
   CHECK(d.moved, "the tip pressing did not move the pointer");
   CHECK(!d.mouse_r, "the right button is down with no barrel button");

   pen(&d, &cfg_default, ANDROID_STYLUS_CONTACT, true, FIRM, 0.0f);
   CHECK(d.pointer_count == 1,
         "the barrel button changed the pointer count of a pressing pen");
   CHECK(d.mouse_r, "the barrel button is not the right mouse button");

   pen(&d, &cfg_default, ANDROID_STYLUS_CONTACT, false, LIGHT, 0.0f);
   CHECK(d.pointer_count == 0,
         "a tip below the pressure threshold still presses");
   CHECK(d.moved, "a tip resting lightly did not move the pointer");

   pen(&d, &cfg_default, ANDROID_STYLUS_CONTACT, false, FIRM, 0.0f);
   pen(&d, &cfg_default, ANDROID_STYLUS_UP, false, 0.0f, 0.0f);
   CHECK(d.pointer_count == 0, "lifting the pen did not release the pointer");
   CHECK(!d.stylus.contact_active, "the pen is still in contact after a lift");
}

/* 4, 8: hover, and a move before the tip has touched. */
static void lane_hover(void)
{
   struct driver d;
   driver_reset(&d);

   pen(&d, &cfg_no_hover, ANDROID_STYLUS_HOVER, false, 0.0f, 1.0f);
   CHECK(!d.moved, "a hovering pen moved the pointer with the option off");

   pen(&d, &cfg_default, ANDROID_STYLUS_HOVER, true, 0.0f, 1.0f);
   CHECK(d.pointer_count == 0,
         "the barrel button pressed while hovering, with contact required");
   CHECK(d.mouse_r, "the barrel button is not the right button while hovering");

   pen(&d, &cfg_no_hover, ANDROID_STYLUS_CONTACT, false, 0.0f, 0.4f);
   CHECK(!d.moved,
         "a move that still reports distance moved the pointer before"
         " the tip had touched");
   CHECK(d.pointer_count == 0, "a move above the screen pressed the pointer");
}

/* 5: the pointer count is shared with the touchscreen. */
static void lane_shared_pointer_count(void)
{
   struct driver d;
   driver_reset(&d);

   /* Two fingers down, then the pen hovers past and leaves. */
   d.pointer_count = 2;
   pen(&d, &cfg_default, ANDROID_STYLUS_HOVER, false, 0.0f, 1.0f);
   pen(&d, &cfg_default, ANDROID_STYLUS_HOVER, true,  0.0f, 1.0f);
   pen(&d, &cfg_default, ANDROID_STYLUS_HOVER_EXIT, false, 0.0f, 1.0f);
   CHECK(d.pointer_count == 2, "a hovering pen released a finger");

   /* The pen presses; a finger event in between zeroes the count; the
    * pen's next move says it is still pressing. */
   pen(&d, &cfg_default, ANDROID_STYLUS_CONTACT, false, FIRM, 0.0f);
   d.pointer_count = 0;
   pen(&d, &cfg_default, ANDROID_STYLUS_CONTACT, false, FIRM, 0.0f);
   CHECK(d.pointer_count == 1,
         "a finger lifting left a pressing pen released");

   pen(&d, &cfg_default, ANDROID_STYLUS_UP, false, 0.0f, 0.0f);
   CHECK(d.pointer_count == 0, "the pen's own press was not let go");

   /* Nothing pressed: a lift or an exit has nothing to release. */
   d.pointer_count = 1;
   pen(&d, &cfg_default, ANDROID_STYLUS_UP, false, 0.0f, 0.0f);
   pen(&d, &cfg_default, ANDROID_STYLUS_HOVER_EXIT, false, 0.0f, 1.0f);
   CHECK(d.pointer_count == 1,
         "a pen that was not pressing released someone else's pointer");
}

/* 6: the barrel button is let go with the pen. */
static void lane_barrel_released(void)
{
   struct driver d;
   driver_reset(&d);

   pen(&d, &cfg_default, ANDROID_STYLUS_HOVER, true, 0.0f, 1.0f);
   pen(&d, &cfg_default, ANDROID_STYLUS_HOVER_EXIT, true, 0.0f, 1.0f);
   CHECK(!d.mouse_r, "the right button stayed down after the pen left range");

   pen(&d, &cfg_default, ANDROID_STYLUS_CONTACT, true, FIRM, 0.0f);
   pen(&d, &cfg_default, ANDROID_STYLUS_CANCEL, true, 0.0f, 0.0f);
   CHECK(!d.mouse_r, "the right button stayed down after a cancel");
   CHECK(d.pointer_count == 0, "a cancel did not release the pointer");
   CHECK(!d.stylus.contact_active, "a cancel left the pen in contact");

   /* A lift keeps it: the hover events that follow still carry it. */
   pen(&d, &cfg_default, ANDROID_STYLUS_CONTACT, true, FIRM, 0.0f);
   pen(&d, &cfg_default, ANDROID_STYLUS_UP, true, 0.0f, 0.0f);
   CHECK(d.mouse_r, "a lift let go of a barrel button still held");
}

/* 7: a click without contact. */
static void lane_click_without_contact(void)
{
   struct driver d;
   driver_reset(&d);

   pen(&d, &cfg_barrel, ANDROID_STYLUS_HOVER, false, 0.0f, 1.0f);
   CHECK(d.pointer_count == 0, "hovering pressed with no barrel button");

   pen(&d, &cfg_barrel, ANDROID_STYLUS_HOVER, true, 0.0f, 1.0f);
   CHECK(d.pointer_count == 1,
         "the barrel button did not press while hovering, with contact"
         " not required");
   CHECK(d.moved, "a hover click was not placed at the pen");

   pen(&d, &cfg_barrel, ANDROID_STYLUS_HOVER, false, 0.0f, 1.0f);
   CHECK(d.pointer_count == 0, "releasing the barrel button did not release");

   /* Touching lightly with the barrel button held is a press too. */
   pen(&d, &cfg_barrel, ANDROID_STYLUS_CONTACT, true, LIGHT, 0.0f);
   CHECK(d.pointer_count == 1,
         "the barrel button did not press on a light touch");
}

/* 9: the threshold. */
static void lane_threshold(void)
{
   unsigned s;
   float prev = android_stylus_pressure_threshold(0);

   for (s = 1; s <= 100; s++)
   {
      float t = android_stylus_pressure_threshold(s);
      CHECK(t <= prev, "a higher sensitivity raised the threshold");
      CHECK(t >= 0.0f, "the threshold went below zero");
      prev = t;
   }
   CHECK(android_stylus_pressure_threshold(100) == 0.0f,
         "full sensitivity is not a threshold of zero");
   CHECK(android_stylus_pressure_threshold(5000)
         == android_stylus_pressure_threshold(100),
         "an out-of-range sensitivity was not clamped");
}

/* 1: no sequence of events, under any setting, asks for more than one
 * pointer. A fixed-seed walk, so a failure reproduces. */
static void lane_one_touch(void)
{
   struct driver d;
   unsigned long seed = 0x5eed1234UL;
   unsigned i;

   driver_reset(&d);

   for (i = 0; i < 200000; i++)
   {
      android_stylus_cfg_t cfg;
      unsigned r;

      seed = seed * 1103515245UL + 12345UL;
      r    = (unsigned)((seed >> 8) & 0xffffff);

      cfg.require_contact      = (r & 1) != 0;
      cfg.hover_moves_pointer  = (r & 2) != 0;
      cfg.pressure_sensitivity = (r >> 2) % 120;

      pen(&d, &cfg, (enum android_stylus_event)((r >> 9) % 5),
            (r & 0x4000) != 0,
            (float)((r >> 15) % 100) / 100.0f,
            ((r >> 22) & 1) ? 0.0f : 0.5f);

      if (d.pointer_count < 0 || d.pointer_count > 1)
         break;
   }

   CHECK(d.max_count_asked <= 1,
         "a pen asked for more than one pointer");
   CHECK(d.pointer_count >= 0 && d.pointer_count <= 1,
         "the pointer count left the range of one touch");
}

static void all_lanes(void)
{
   lane_press_and_lift();
   lane_hover();
   lane_shared_pointer_count();
   lane_barrel_released();
   lane_click_without_contact();
   lane_threshold();
   lane_one_touch();
}

int main(void)
{
   all_lanes();
   if (failures)
   {
      fprintf(stderr, "FAIL android_stylus_map_test: %u failures\n",
            failures);
      return 1;
   }

   /* The old pointer counts have to be caught. */
   quiet               = true;
   sabotage_old_counts = true;
   all_lanes();
   quiet               = false;
   sabotage_old_counts = false;

   if (!failures)
   {
      fprintf(stderr, "FAIL android_stylus_map_test: the old pointer"
            " counts (2 for the tip, 3 with the barrel button) went"
            " unnoticed\n");
      return 1;
   }

   fprintf(stderr, "PASS android_stylus_map_test (sabotage caught by"
         " %u checks)\n", failures);
   return 0;
}
