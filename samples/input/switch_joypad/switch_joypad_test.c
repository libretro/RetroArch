/* The Switch joypad driver (input/drivers_joypad/switch_joypad.c, the
 * libnx build), against the tree's libnx stubs, with the libnx calls
 * defined by the test. Held to:
 *
 * - Joy-Con joined: both sticks;
 * - split, a lone left Joy-Con held sideways: its stick turned onto the
 *   left stick, and the right stick centred whatever it held joined;
 * - the same for a lone right Joy-Con;
 * - a pad is there while connected, with nothing held, and not once it
 *   is not. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include <switch.h>

#include "input/input_driver.h"
#include "input/drivers_joypad/switch_joypad.c"

/* ---- the stand-ins: pad 0 is the one the test moves ---- */
static HidAnalogStickState t_stick[2];
static u32                 t_device_type;
static bool                t_connected;
static unsigned            t_split;

void padConfigureInput(u32 m, u32 s)          { (void)m; (void)s; }
void padInitialize(PadState *pad, int id)     { (void)id; memset(pad, 0, sizeof(*pad)); }
void padInitializeDefault(PadState *pad)      { memset(pad, 0, sizeof(*pad)); }
void padUpdate(PadState *pad)                 { (void)pad; }
bool padIsHandheld(const PadState *pad)       { (void)pad; return false; }
bool padIsNpadActive(const PadState *p, HidNpadIdType id) { (void)p; (void)id; return true; }
bool padIsConnected(const PadState *pad) { return pad == &pad_states[0] && t_connected; }
u64  padGetButtons(const PadState *pad)       { (void)pad; return 0; }
HidAnalogStickState padGetStickPos(const PadState *pad, unsigned i)
{
   HidAnalogStickState none = { 0, 0 };
   return pad == &pad_states[0] ? t_stick[i] : none;
}
void   hidSetNpadJoyHoldType(int type) { (void)type; }
u32    hidGetNpadDeviceType(HidNpadIdType id) { return id == 0 ? t_device_type : 0; }
Result hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType id) { (void)id; return 0; }
Result hidSetNpadJoyAssignmentModeDual(HidNpadIdType id) { (void)id; return 0; }
Result hidMergeSingleJoyAsDualJoy(HidNpadIdType a, HidNpadIdType b) { (void)a; (void)b; return 0; }
Result hidInitializeVibrationDevices(HidVibrationDeviceHandle *h, s32 t,
      HidNpadIdType id, u32 s) { (void)h; (void)t; (void)id; (void)s; return 0; }
Result hidSendVibrationValues(const HidVibrationDeviceHandle *h,
      const HidVibrationValue *v, s32 c) { (void)h; (void)v; (void)c; return 0; }

unsigned input_config_get_split_joycon(unsigned port) { (void)port; return t_split; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   (void)name; (void)display_name; (void)phys; (void)driver;
   (void)port; (void)vid; (void)pid;
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

static int16_t axis(uint32_t a) { return switch_joypad.axis(0, a); }

static void sticks(s32 lx, s32 ly, s32 rx, s32 ry)
{
   t_stick[0].x = lx; t_stick[0].y = ly;
   t_stick[1].x = rx; t_stick[1].y = ry;
}

int main(void)
{
   check(switch_joypad.init(NULL) != NULL, "init");

   /* Joined, right stick held down (libnx y is up-positive). */
   t_split = 0;
   sticks(0, 0, 0, -0x7fff);
   switch_joypad.poll();
   check(axis(AXIS_POS(3)) == 0x7fff, "joined: right stick down");

   /* Split, a lone left Joy-Con: its stick pushed toward the sideways
    * "down", the right stick bytes still holding the joined value. */
   t_split       = 1;
   t_device_type = HidDeviceTypeBits_JoyLeft;
   sticks(0x7fff, 0, 0, -0x7fff);
   switch_joypad.poll();
   check(axis(AXIS_NEG(1)) == -0x7fff, "split left: its stick on the left stick");
   check(axis(AXIS_POS(3)) == 0 && axis(AXIS_NEG(3)) == 0
      && axis(AXIS_POS(2)) == 0 && axis(AXIS_NEG(2)) == 0,
      "split left: right stick centred");

   /* Back to joined with the right stick held, then a lone right. */
   t_split = 0;
   sticks(0, 0, 0, -0x7fff);
   switch_joypad.poll();
   t_split       = 1;
   t_device_type = HidDeviceTypeBits_JoyRight;
   switch_joypad.poll();
   check(axis(AXIS_POS(3)) == 0 && axis(AXIS_NEG(3)) == 0,
      "split right: right stick centred");

   /* Connected and idle: there. Not connected: not. */
   t_split     = 0;
   t_connected = true;
   sticks(0, 0, 0, 0);
   switch_joypad.poll();
   check(switch_joypad.query_pad(0), "idle pad: there");
   check(!switch_joypad.query_pad(1), "no pad: not there");
   t_connected = false;
   switch_joypad.poll();
   check(!switch_joypad.query_pad(0), "disconnected: not there");

   switch_joypad.destroy();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("switch_joypad: all checks passed\n");
   return 0;
}
