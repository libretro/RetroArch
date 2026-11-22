/* The hold armed when a text entry ends (input_driver_hold_held_input_late()),
 * run through the shipping input_driver_hold_bits(): the key that ended
 * the entry may reach the poll on the frame after, alone or beside a
 * control already down, and is held back until it is let go either way.
 * The functions are taken from input/input_driver.c by extract.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input/input_driver.h"

#define CORE_HOLD_MOUSE_IDS 0xffffu
#define CORE_HOLD_GUN_IDS   0xffffffffu

static input_driver_state_t input_driver_st;
static uint8_t input_held_focused;
static retro_time_t input_held_deadline;

static void input_core_read_gate_set(input_driver_state_t *st) { (void)st; }
bool video_driver_has_focus(void) { return true; }
retro_time_t cpu_features_get_time_usec(void) { return 0; }

#include "driver_code.inc"

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

/* One frame: the bits down, then whether each survives the hold. */
static input_bits_t frame(unsigned a, unsigned b)
{
   input_bits_t bits;
   BIT256_CLEAR_ALL(bits);
   if (a)
      BIT256_SET(bits, a);
   if (b)
      BIT256_SET(bits, b);
   input_driver_hold_bits(&bits);
   return bits;
}

#define OK    RETRO_DEVICE_ID_JOYPAD_A
#define LEFT  RETRO_DEVICE_ID_JOYPAD_LEFT
#define NONE  0

static void begin(void)
{
   memset(&input_driver_st, 0, sizeof(input_driver_st));
   input_driver_hold_held_input_late();
}

int main(void)
{
   input_bits_t out;

   /* Return seen on the frame the entry ends: held until let go. */
   begin();
   out = frame(OK, NONE);
   CHECK(!BIT256_GET(out, OK));
   out = frame(OK, NONE);
   CHECK(!BIT256_GET(out, OK));
   out = frame(NONE, NONE);
   out = frame(OK, NONE);
   CHECK(BIT256_GET(out, OK));

   /* Return a frame late, nothing else down. */
   begin();
   out = frame(NONE, NONE);
   out = frame(OK, NONE);
   CHECK(!BIT256_GET(out, OK));
   out = frame(OK, NONE);
   CHECK(!BIT256_GET(out, OK));
   out = frame(NONE, NONE);
   CHECK(!(input_driver_st.held_bits_phase & INPUT_HELD_PHASE));
   out = frame(OK, NONE);
   CHECK(BIT256_GET(out, OK));

   /* Return a frame late beside a direction already down: the
    * direction is held back, and so is Return. */
   begin();
   out = frame(LEFT, NONE);
   CHECK(!BIT256_GET(out, LEFT));
   out = frame(LEFT, OK);
   CHECK(!BIT256_GET(out, OK));
   CHECK(!BIT256_GET(out, LEFT));
   out = frame(NONE, OK);
   CHECK(!BIT256_GET(out, OK));
   out = frame(NONE, NONE);
   out = frame(LEFT, OK);
   CHECK(BIT256_GET(out, OK));
   CHECK(BIT256_GET(out, LEFT));

   /* The hotkey enabler is never held back. */
   begin();
   out = frame(RARCH_ENABLE_HOTKEY, NONE);
   CHECK(BIT256_GET(out, RARCH_ENABLE_HOTKEY));
   out = frame(RARCH_ENABLE_HOTKEY, NONE);
   CHECK(BIT256_GET(out, RARCH_ENABLE_HOTKEY));

   /* Nothing down at all: the hold is over after its two frames. */
   begin();
   out = frame(NONE, NONE);
   out = frame(NONE, NONE);
   CHECK(!(input_driver_st.held_bits_phase & INPUT_HELD_PHASE));
   out = frame(OK, NONE);
   CHECK(BIT256_GET(out, OK));

   /* The plain hold takes the first frame only. */
   memset(&input_driver_st, 0, sizeof(input_driver_st));
   input_driver_hold_held_input();
   out = frame(LEFT, NONE);
   CHECK(!BIT256_GET(out, LEFT));
   out = frame(LEFT, OK);
   CHECK(BIT256_GET(out, OK));

   puts("hold_late_test: ok");
   return 0;
}
