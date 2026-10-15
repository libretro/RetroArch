/* Turbo and hold, driven directly.
 *
 * input_turbo_hold_apply() is extracted from input_driver.c between its
 * BEGIN/END markers. Each call is one core query of one button in one
 * frame; the frame's hotkey states and the turbo counter are set by the
 * test. Under UBSan a shift into the sign bit fails. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <libretro.h>

#include "../../../configuration.h"
#include "../../../input/input_defines.h"
#include "../../../input/input_types.h"

static unsigned failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("      FAIL: %s\n", msg); failures++; } } while (0)

#include "turbo_hold_fragment.c"

#define FLAG (UINT32_C(1) << 31)

static settings_t settings;
static turbo_buttons_t turbo;
static hold_buttons_t hold;

static void reset(unsigned mode)
{
   unsigned i;
   memset(&settings, 0, sizeof(settings));
   memset(&turbo, 0, sizeof(turbo));
   memset(&hold, 0, sizeof(hold));
   for (i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      settings.uints.input_remap_ids[0][i] = i;
   settings.uints.input_turbo_period     = 4;
   settings.uints.input_turbo_duty_cycle = 2;
   settings.uints.input_turbo_mode       = mode;
   settings.uints.input_turbo_button     = RETRO_DEVICE_ID_JOYPAD_B;
   settings.ints.input_turbo_bind        = -1;
}

static int16_t query(unsigned id, bool turbo_key, int16_t res)
{
   turbo.frame_enable[0] = turbo_key;
   return input_turbo_hold_apply(&turbo, &hold, &settings, 0, id, res);
}

/* The turbo hotkey's held flag sits in the top bit: one toggle per press
 * however long it is held, cleared on release. */
static void t_single_toggle(void)
{
   unsigned f;
   printf("   single button: the hotkey toggles once per press\n");
   reset(INPUT_TURBO_MODE_SINGLEBUTTON);

   for (f = 0; f < 4; f++)
   {
      query(RETRO_DEVICE_ID_JOYPAD_B, true, 0);
      CHECK(turbo.mode1_enable[0] == 1, "a held hotkey toggled more than once");
   }
   CHECK(turbo.turbo_pressed[0] & FLAG, "held flag not set while the hotkey is held");
   CHECK(turbo.enable[0] == (1 << RETRO_DEVICE_ID_JOYPAD_B), "turbo not on B");

   query(RETRO_DEVICE_ID_JOYPAD_B, false, 0);
   CHECK(!(turbo.turbo_pressed[0] & FLAG), "held flag kept after release");
   CHECK(turbo.mode1_enable[0] == 1, "release toggled");

   /* Released B pulses with the counter */
   for (f = 0; f < 8; f++)
   {
      turbo.count = f;
      CHECK(query(RETRO_DEVICE_ID_JOYPAD_B, false, 0) == ((f % 4) < 2),
            "B does not follow the duty cycle");
   }
   /* Held B passes through */
   turbo.count = 3;
   CHECK(query(RETRO_DEVICE_ID_JOYPAD_B, false, 1) == 1, "held B was modulated");

   query(RETRO_DEVICE_ID_JOYPAD_B, true, 0);
   query(RETRO_DEVICE_ID_JOYPAD_B, true, 0);
   CHECK(turbo.mode1_enable[0] == 0, "second press did not toggle off");
}

/* Hold mode: turbo stops when the hotkey is released */
static void t_single_hold(void)
{
   printf("   single button hold: release stops turbo\n");
   reset(INPUT_TURBO_MODE_SINGLEBUTTON_HOLD);
   query(RETRO_DEVICE_ID_JOYPAD_B, true, 0);
   CHECK(turbo.mode1_enable[0] == 1, "hotkey did not start turbo");
   query(RETRO_DEVICE_ID_JOYPAD_B, false, 0);
   CHECK(turbo.mode1_enable[0] == 0, "release did not stop turbo");
}

static void t_classic(void)
{
   unsigned f;
   printf("   classic: buttons pressed with the hotkey pulse while held\n");
   reset(INPUT_TURBO_MODE_CLASSIC);
   turbo.count = 2;
   query(RETRO_DEVICE_ID_JOYPAD_A, true, 1);
   CHECK(turbo.enable[0] & (1 << RETRO_DEVICE_ID_JOYPAD_A), "A not enabled");
   for (f = 0; f < 8; f++)
   {
      turbo.count = f;
      CHECK(query(RETRO_DEVICE_ID_JOYPAD_A, false, 1) == ((f % 4) < 2),
            "held A does not follow the duty cycle");
   }
   query(RETRO_DEVICE_ID_JOYPAD_A, false, 0);
   CHECK(!(turbo.enable[0] & (1 << RETRO_DEVICE_ID_JOYPAD_A)), "release kept A enabled");

   /* The D-pad stays plain unless allowed */
   turbo.count = 3;
   CHECK(query(RETRO_DEVICE_ID_JOYPAD_UP, true, 1) == 1, "D-pad was modulated");
   settings.bools.input_turbo_allow_dpad = true;
   query(RETRO_DEVICE_ID_JOYPAD_UP, true, 1);
   CHECK(query(RETRO_DEVICE_ID_JOYPAD_UP, false, 1) == 0, "allowed D-pad not modulated");
}

static void t_classic_toggle(void)
{
   unsigned f;
   printf("   classic toggle: one toggle per press of a button with the hotkey\n");
   reset(INPUT_TURBO_MODE_CLASSIC_TOGGLE);
   for (f = 0; f < 3; f++)
      query(RETRO_DEVICE_ID_JOYPAD_Y, true, 1);
   CHECK(turbo.enable[0] & (1 << RETRO_DEVICE_ID_JOYPAD_Y), "held press toggled off again");
   query(RETRO_DEVICE_ID_JOYPAD_Y, true, 0);
   query(RETRO_DEVICE_ID_JOYPAD_Y, true, 1);
   CHECK(!(turbo.enable[0] & (1 << RETRO_DEVICE_ID_JOYPAD_Y)), "second press did not toggle");
}

static void t_hold(void)
{
   printf("   hold: a tap with the modifier latches the button\n");
   reset(INPUT_TURBO_MODE_CLASSIC);
   hold.frame_enable[0] = true;
   query(RETRO_DEVICE_ID_JOYPAD_X, false, 1);
   query(RETRO_DEVICE_ID_JOYPAD_X, false, 1);
   query(RETRO_DEVICE_ID_JOYPAD_X, false, 0);
   hold.frame_enable[0] = false;
   CHECK(query(RETRO_DEVICE_ID_JOYPAD_X, false, 0) == 1, "X not latched");
   hold.frame_enable[0] = true;
   query(RETRO_DEVICE_ID_JOYPAD_X, false, 1);
   hold.frame_enable[0] = false;
   CHECK(query(RETRO_DEVICE_ID_JOYPAD_X, false, 0) == 0, "second tap did not release X");
}

int main(void)
{
   printf("turbo_hold:\n");
   t_single_toggle();
   t_single_hold();
   t_classic();
   t_classic_toggle();
   t_hold();
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("all passed\n");
   return 0;
}
