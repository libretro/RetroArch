/* android_key_state_test.c -- key presses and releases, through the
 * code that writes them into the Android input driver's key state.
 *
 * The state is a row of bits per pad slot plus one for the keyboard.
 * Two keys are not written as themselves: DPAD_CENTER becomes ENTER on
 * the keyboard row, and BACK also presses X - on its own row and on
 * the keyboard's - unless a gamepad sent it.
 *
 * The bug this pins (issue #19436): for nine days a press of BACK set
 * X on the keyboard row and nothing cleared it. X is RetroPad A by
 * default, and the frontend drops all controller input after a menu
 * toggle until every button is up, so one press of Back left the menu
 * deaf to the controller - on handhelds, whose Back button is how the
 * menu gets opened - while touch kept working.
 *
 * The claims:
 *
 *   1. Whatever a press sets, the release clears: for every keycode,
 *      from every source, on every row, press then release leaves
 *      every row empty.
 *   2. That holds for any interleaving of keys, sources and rows once
 *      everything held has been released.
 *   3. A keycode outside the rows is dropped: nothing is written,
 *      inside the rows or past them.
 *   4. DPAD_CENTER is ENTER on the keyboard row and stays itself on a
 *      pad row, where a remote's OK button is configured as "23"
 *      (issue #18819).
 *   5. BACK presses X on its own row and the keyboard row while it is
 *      held, and not at all when a gamepad sent it.
 *   6. A pad that goes away while holding BACK leaves nothing behind,
 *      on its row or the keyboard's.
 *
 * The writes are the driver's own, from android_key_state.h. A
 * sabotage mode puts the missing clear back and is asserted to be
 * caught. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "android_key_state.h"

/* android/keycodes.h and android/input.h, as far as this cares. */
#define LAST_KEYCODE      219  /* AKEYCODE_ASSIST */
#define MAX_KEYS          ((LAST_KEYCODE + 7) / 8)
#define MAX_PADS          8
#define KEYBOARD_ROW      MAX_PADS

#define SOURCE_KEYBOARD   0x00000101
#define SOURCE_DPAD       0x00000201
#define SOURCE_GAMEPAD    0x00000401
#define SOURCE_JOYSTICK   0x01000010
#define SOURCE_HDMI       0x02000001

static const int sources[] = {
   0,
   SOURCE_KEYBOARD,
   SOURCE_DPAD,
   SOURCE_KEYBOARD | SOURCE_DPAD,
   SOURCE_GAMEPAD,
   SOURCE_JOYSTICK,
   SOURCE_GAMEPAD | SOURCE_KEYBOARD,
   SOURCE_GAMEPAD | SOURCE_JOYSTICK | SOURCE_KEYBOARD,
   SOURCE_HDMI
};
#define N_SOURCES (sizeof(sources) / sizeof(sources[0]))

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

/* What the driver did from 13 to 22 August: the X a BACK press put on
 * the keyboard row was not cleared by its release. */
static bool sabotage_no_clear = false;

/* The driver's key state, with a guard on either side of it. */
#define GUARD 0xa5
struct state
{
   uint8_t before[MAX_KEYS];
   uint8_t rows[MAX_PADS + 1][MAX_KEYS];
   uint8_t after[MAX_KEYS];
};

static void state_reset(struct state *st)
{
   memset(st->before, GUARD, sizeof(st->before));
   memset(st->rows,   0,     sizeof(st->rows));
   memset(st->after,  GUARD, sizeof(st->after));
}

static bool state_empty(const struct state *st)
{
   size_t i;
   const uint8_t *p = &st->rows[0][0];
   for (i = 0; i < sizeof(st->rows); i++)
      if (p[i])
         return false;
   return true;
}

static bool guards_intact(const struct state *st)
{
   size_t i;
   for (i = 0; i < MAX_KEYS; i++)
      if (st->before[i] != GUARD || st->after[i] != GUARD)
         return false;
   return true;
}

/* android_input_poll_event_type_key() in the driver. */
static void key(struct state *st, int row, int keycode, int source, bool down)
{
   android_key_state_write(st->rows[row], st->rows[KEYBOARD_ROW],
         LAST_KEYCODE, keycode, source, down);

   if (     sabotage_no_clear && !down
         && keycode == ANDROID_KEY_BACK
         && android_key_back_is_aliased(source))
      BIT_SET(st->rows[KEYBOARD_ROW], ANDROID_KEY_X);
}

/* 1: press, release, nothing left. */
static void lane_press_release(void)
{
   struct state st;
   int keycode, row;
   size_t s;
   unsigned left = 0;

   for (keycode = 0; keycode < LAST_KEYCODE; keycode++)
      for (s = 0; s < N_SOURCES; s++)
         for (row = 0; row <= KEYBOARD_ROW; row++)
         {
            state_reset(&st);
            key(&st, row, keycode, sources[s], true);
            key(&st, row, keycode, sources[s], false);
            if (!state_empty(&st))
            {
               if (!left && !quiet)
                  fprintf(stderr, "FAIL: keycode %d from source 0x%x on"
                        " row %d was pressed and released, and something"
                        " is still held\n", keycode, sources[s], row);
               left++;
            }
         }

   CHECK(left == 0, "a release did not clear everything its press set");
}

/* 2: any interleaving, then everything released. A fixed-seed walk, so
 * a failure reproduces. */
static void lane_interleaved(void)
{
   struct state st;
   static bool held[MAX_PADS + 1][LAST_KEYCODE];
   static int  held_source[MAX_PADS + 1][LAST_KEYCODE];
   unsigned long seed = 0x19436UL;
   unsigned i;
   int row, keycode;

   state_reset(&st);
   memset(held, 0, sizeof(held));

   for (i = 0; i < 200000; i++)
   {
      unsigned r;
      seed    = seed * 1103515245UL + 12345UL;
      r       = (unsigned)((seed >> 8) & 0xffffff);
      row     = (int)(r % (MAX_PADS + 1));
      /* Mostly the keys with special handling, so they collide. */
      switch ((r >> 4) % 6)
      {
         case 0:  keycode = ANDROID_KEY_BACK;        break;
         case 1:  keycode = ANDROID_KEY_X;           break;
         case 2:  keycode = ANDROID_KEY_DPAD_CENTER; break;
         case 3:  keycode = ANDROID_KEY_ENTER;       break;
         default: keycode = (int)((r >> 8) % LAST_KEYCODE); break;
      }

      if (held[row][keycode])
      {
         key(&st, row, keycode, held_source[row][keycode], false);
         held[row][keycode] = false;
      }
      else
      {
         int source = sources[(r >> 16) % N_SOURCES];
         key(&st, row, keycode, source, true);
         held[row][keycode]        = true;
         held_source[row][keycode] = source;
      }
   }

   for (row = 0; row <= KEYBOARD_ROW; row++)
      for (keycode = 0; keycode < LAST_KEYCODE; keycode++)
         if (held[row][keycode])
            key(&st, row, keycode, held_source[row][keycode], false);

   CHECK(state_empty(&st),
         "after every held key was released, something is still held");
   CHECK(guards_intact(&st), "a key write landed outside the rows");
}

/* 3: keycodes the rows have no room for. */
static void lane_out_of_range(void)
{
   static const int codes[] = {
      -1, -1000, LAST_KEYCODE, LAST_KEYCODE + 1, 224, 288, 1000, 0x7fffffff };
   struct state st;
   size_t i, s;
   int row;

   state_reset(&st);
   for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
      for (s = 0; s < N_SOURCES; s++)
         for (row = 0; row <= KEYBOARD_ROW; row++)
         {
            key(&st, row, codes[i], sources[s], true);
            key(&st, row, codes[i], sources[s], false);
            key(&st, row, codes[i], sources[s], true);
         }

   CHECK(state_empty(&st), "a keycode outside the rows was written");
   CHECK(guards_intact(&st),
         "a keycode outside the rows was written past them");
}

/* 4: DPAD_CENTER. */
static void lane_center(void)
{
   struct state st;
   state_reset(&st);

   key(&st, KEYBOARD_ROW, ANDROID_KEY_DPAD_CENTER, SOURCE_KEYBOARD, true);
   CHECK(BIT_GET(st.rows[KEYBOARD_ROW], ANDROID_KEY_ENTER),
         "Center on the keyboard row is not Enter");
   CHECK(!BIT_GET(st.rows[KEYBOARD_ROW], ANDROID_KEY_DPAD_CENTER),
         "Center on the keyboard row was also written as itself");
   key(&st, KEYBOARD_ROW, ANDROID_KEY_DPAD_CENTER, SOURCE_KEYBOARD, false);

   key(&st, 0, ANDROID_KEY_DPAD_CENTER, SOURCE_KEYBOARD | SOURCE_DPAD, true);
   CHECK(BIT_GET(st.rows[0], ANDROID_KEY_DPAD_CENTER),
         "Center on a pad row is not keycode 23: a remote's OK button"
         " is dead");
   CHECK(!BIT_GET(st.rows[0], ANDROID_KEY_ENTER),
         "Center on a pad row was rewritten to Enter");
   key(&st, 0, ANDROID_KEY_DPAD_CENTER, SOURCE_KEYBOARD | SOURCE_DPAD, false);

   CHECK(state_empty(&st), "Center left something held");
}

/* 5: BACK. */
static void lane_back(void)
{
   struct state st;
   state_reset(&st);

   /* A remote, on a pad row. */
   key(&st, 2, ANDROID_KEY_BACK, SOURCE_KEYBOARD | SOURCE_DPAD, true);
   CHECK(BIT_GET(st.rows[2], ANDROID_KEY_BACK), "Back was not written");
   CHECK(BIT_GET(st.rows[2], ANDROID_KEY_X),
         "a remote's Back did not press X on its own row");
   CHECK(BIT_GET(st.rows[KEYBOARD_ROW], ANDROID_KEY_X),
         "a remote's Back did not press X on the keyboard row");
   key(&st, 2, ANDROID_KEY_BACK, SOURCE_KEYBOARD | SOURCE_DPAD, false);
   CHECK(!BIT_GET(st.rows[KEYBOARD_ROW], ANDROID_KEY_X),
         "releasing Back left X held on the keyboard row: the frontend"
         " reads RetroPad A as held and drops all controller input"
         " after the next menu toggle");
   CHECK(state_empty(&st), "releasing a remote's Back left something held");

   /* A gamepad: its Back is its own button and nothing else. */
   key(&st, 0, ANDROID_KEY_BACK, SOURCE_GAMEPAD | SOURCE_KEYBOARD, true);
   CHECK(BIT_GET(st.rows[0], ANDROID_KEY_BACK), "a gamepad's Back was not written");
   CHECK(!BIT_GET(st.rows[0], ANDROID_KEY_X),
         "a gamepad's Back pressed X on its row");
   CHECK(!BIT_GET(st.rows[KEYBOARD_ROW], ANDROID_KEY_X),
         "a gamepad's Back pressed X on the keyboard row");
   key(&st, 0, ANDROID_KEY_BACK, SOURCE_GAMEPAD | SOURCE_KEYBOARD, false);
   CHECK(state_empty(&st), "releasing a gamepad's Back left something held");

   CHECK(!android_key_back_is_aliased(SOURCE_JOYSTICK),
         "a joystick's Back is aliased");
   CHECK(android_key_back_is_aliased(SOURCE_HDMI),
         "a CEC remote's Back is not aliased");
}

/* 6: the pad goes away with Back held. */
static void lane_removed_while_held(void)
{
   struct state st;
   state_reset(&st);

   key(&st, 1, ANDROID_KEY_BACK, SOURCE_KEYBOARD, true);
   key(&st, 1, 30, SOURCE_KEYBOARD, true);
   android_key_state_release_row(st.rows[1], st.rows[KEYBOARD_ROW],
         LAST_KEYCODE);
   CHECK(state_empty(&st),
         "a pad removed while holding Back left a key held");
   CHECK(guards_intact(&st), "releasing a row wrote outside the rows");

   /* Another pad's keys are not let go with it. */
   key(&st, 0, 40, SOURCE_GAMEPAD, true);
   android_key_state_release_row(st.rows[1], st.rows[KEYBOARD_ROW],
         LAST_KEYCODE);
   CHECK(BIT_GET(st.rows[0], 40),
         "removing one pad released another pad's key");
}

static void all_lanes(void)
{
   lane_press_release();
   lane_interleaved();
   lane_out_of_range();
   lane_center();
   lane_back();
   lane_removed_while_held();
}

int main(void)
{
   all_lanes();
   if (failures)
   {
      fprintf(stderr, "FAIL android_key_state_test: %u failures\n",
            failures);
      return 1;
   }

   /* The missing clear has to be caught. */
   quiet             = true;
   sabotage_no_clear = true;
   all_lanes();
   quiet             = false;
   sabotage_no_clear = false;

   if (!failures)
   {
      fprintf(stderr, "FAIL android_key_state_test: X left held on the"
            " keyboard row after Back went unnoticed\n");
      return 1;
   }

   fprintf(stderr, "PASS android_key_state_test (sabotage caught by"
         " %u checks)\n", failures);
   return 0;
}
