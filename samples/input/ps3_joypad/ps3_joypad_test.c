/* The PS3 joypad driver (input/drivers_joypad/ps3_joypad.c, the
 * PSL1GHT build), against the tree's PS3 stubs and ps3_defines.h, with
 * the pad library's calls defined by the test. Held to:
 *
 * - a connected pad gives its buttons and both sticks, both directions;
 * - a report of length 0 (nothing new) keeps the last state;
 * - a pad that disconnects holds nothing and reads centred, whatever it
 *   held at the time, and still does on the polls after;
 * - a connected pad is there with nothing held, and not once it is gone;
 * - rumble sets both motors at once: the one asked for (the small one on
 *   or off, the large one scaled to its 0-255), the other at its last
 *   value, the reserved bytes zero; nothing is sent for a pad out of
 *   range, and a pad that disconnects starts again with both off. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>

#include <defines/ps3_defines.h>

/* ---- the stand-ins ---- */
static padInfo2 t_info;
static padData  t_data[7];

s32 ioPadInit(u32 max) { (void)max; return 0; }
s32 ioPadEnd(void)      { return 0; }
s32 ioPadGetInfo2(padInfo2 *info)
{
   *info = t_info;
   return 0;
}
static padActParam t_act;
static u32         t_act_port;
static unsigned    t_act_sends;
u32 ioPadSetActDirect(u32 port, padActParam *param)
{
   t_act      = *param;
   t_act_port = port;
   t_act_sends++;
   return 0;
}

/* What the rumble call's parameter block holds before it is filled is
 * whatever was on the stack; each call runs behind one that leaves junk
 * there. */
static void __attribute__((noinline)) dirty_stack(void)
{
   volatile unsigned char junk[512];
   unsigned i;
   for (i = 0; i < sizeof(junk); i++)
      junk[i] = 0xA5;
}
s32 ioPadGetData(u32 port, padData *data)
{
   *data = t_data[port];
   /* A report is read once; the next read has nothing new. */
   t_data[port].len = 0;
   return 0;
}

/* Griffin includes the frontend's input header ahead of the driver. */
#include "input/input_driver.h"
#include "input/drivers_joypad/ps3_joypad.c"

bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   (void)name; (void)display_name; (void)phys; (void)driver;
   (void)port; (void)vid; (void)pid;
   return true;
}

bool input_autoconfigure_disconnect(unsigned port, const char *ident)
{
   (void)port; (void)ident;
   return true;
}

/* ---- the checks ---- */
static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   if (!cond)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

static void status(uint32_t s) { t_info.port_status[0] = s; }

static void report(uint16_t d1, uint16_t d2, uint8_t lx, uint8_t ly,
      uint8_t rx, uint8_t ry)
{
   memset(&t_data[0], 0, sizeof(t_data[0]));
   t_data[0].len = 24;
   t_data[0].button[CELL_PAD_BTN_OFFSET_DIGITAL1]       = d1;
   t_data[0].button[CELL_PAD_BTN_OFFSET_DIGITAL2]       = d2;
   t_data[0].button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X]  = lx;
   t_data[0].button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y]  = ly;
   t_data[0].button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X] = rx;
   t_data[0].button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y] = ry;
}

static int16_t axis(uint32_t a) { return ps3_joypad.axis(0, a); }

static bool rumble(unsigned pad, enum retro_rumble_effect e, uint16_t v)
{
   dirty_stack();
   return ps3_joypad.set_rumble(pad, e, v);
}

static bool reserved_zero(void)
{
   unsigned i;
   for (i = 0; i < sizeof(t_act.reserved); i++)
      if (t_act.reserved[i])
         return false;
   return true;
}

static bool all_centred(void)
{
   unsigned a;
   for (a = 0; a < 4; a++)
      if (axis(AXIS_POS(a)) || axis(AXIS_NEG(a)))
         return false;
   return true;
}

int main(void)
{
   check(ps3_joypad.init(NULL) != NULL, "init");

   /* Connected: left stick down and right, right stick up and left. */
   status(CELL_PAD_STATUS_CONNECTED | CELL_PAD_STATUS_ASSIGN_CHANGES);
   report(CELL_PAD_CTRL_DOWN, CELL_PAD_CTRL_CROSS, 0xFF, 0xFF, 0x00, 0x00);
   ps3_joypad.poll();
   check(ps3_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0, "down held");
   check(ps3_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) != 0,    "cross is B");
   check(axis(AXIS_POS(0)) == 0x7fff,  "left x right");
   check(axis(AXIS_POS(1)) == 0x7fff,  "left y down");
   check(axis(AXIS_NEG(2)) == -0x7fff, "right x left");
   check(axis(AXIS_NEG(3)) == -0x7fff, "right y up");

   /* Connected with nothing held: still there. */
   check(ps3_joypad.query_pad(0), "connected: there");
   check(!ps3_joypad.query_pad(1), "no pad: not there");
   check(!ps3_joypad.query_pad(DEFAULT_MAX_PADS + 3), "past the pads: not there");

   /* Nothing new: the state stands. */
   status(CELL_PAD_STATUS_CONNECTED);
   ps3_joypad.poll();
   check(axis(AXIS_POS(1)) == 0x7fff, "no new report keeps the stick");
   check(ps3_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0,
         "no new report keeps the buttons");

   /* Nothing held: still there. */
   report(0, 0, 0x80, 0x80, 0x80, 0x80);
   ps3_joypad.poll();
   check(ps3_joypad.query_pad(0), "idle pad: there");

   /* Pulled with the stick held down. */
   status(CELL_PAD_STATUS_ASSIGN_CHANGES);
   ps3_joypad.poll();
   check(ps3_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_DOWN) == 0, "disconnect: down let go");
   check(all_centred(), "disconnect: sticks centred");
   check(!ps3_joypad.query_pad(0), "disconnect: not there");

   status(0);
   ps3_joypad.poll();
   check(all_centred(), "after disconnect: sticks stay centred");

   /* Rumble. */
   status(CELL_PAD_STATUS_CONNECTED | CELL_PAD_STATUS_ASSIGN_CHANGES);
   ps3_joypad.poll();
   check(rumble(0, RETRO_RUMBLE_WEAK, 0x4000), "rumble: weak sent");
   check(t_act.motor[0] == 1 && t_act.motor[1] == 0 && reserved_zero(),
         "rumble: weak on, large off, reserved zero");
   check(rumble(0, RETRO_RUMBLE_STRONG, 0x8000), "rumble: strong sent");
   check(t_act.motor[0] == 1 && t_act.motor[1] == 0x80 && reserved_zero(),
         "rumble: large at half, small still on");
   check(rumble(0, RETRO_RUMBLE_WEAK, 0), "rumble: weak off sent");
   check(t_act.motor[0] == 0 && t_act.motor[1] == 0x80,
         "rumble: small off, large still at half");
   check(rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF) && t_act.motor[1] == 0xFF,
         "rumble: large at full");
   {
      unsigned sends = t_act_sends;
      check(!rumble(DEFAULT_MAX_PADS, RETRO_RUMBLE_STRONG, 0xFFFF)
            && t_act_sends == sends, "rumble: pad out of range sends nothing");
   }
   status(CELL_PAD_STATUS_ASSIGN_CHANGES);
   ps3_joypad.poll();
   status(CELL_PAD_STATUS_CONNECTED | CELL_PAD_STATUS_ASSIGN_CHANGES);
   ps3_joypad.poll();
   check(rumble(0, RETRO_RUMBLE_WEAK, 0x4000)
         && t_act.motor[0] == 1 && t_act.motor[1] == 0,
         "rumble: after a disconnect the large motor starts off");

   ps3_joypad.destroy();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("ps3_joypad: all checks passed\n");
   return 0;
}
