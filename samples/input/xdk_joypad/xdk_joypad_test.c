/* The original Xbox joypad driver (input/drivers_joypad/xdk_joypad.c),
 * with the XDK's device and input calls replaced by the test's. Held
 * to:
 *
 * - a pad inserted is opened once and read on every poll after, on
 *   the same handle: its buttons and both sticks;
 * - a pad whose poll or read fails holds nothing and reads centred,
 *   and is read again once it answers;
 * - a pad removed is closed, holds nothing and reads centred;
 * - destroy closes what is open;
 * - a port past the four the console has: no pad, no name, no axis. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include <Xtl.h>

/* ---- the stand-ins: one pad, on port 0 ---- */
#define T_HANDLE ((HANDLE)0x1234)
static DWORD        t_insert, t_remove;
static DWORD        t_poll_ret, t_get_ret;
static XINPUT_STATE t_state;
static unsigned     t_opens, t_closes, t_bad_handle;

void   XInitDevices(DWORD c, PXDEVICE_PREALLOC_TYPE t) { (void)c; (void)t; }
BOOL   XGetDeviceChanges(PXPP_DEVICE_TYPE type, PDWORD ins, PDWORD rem)
{
   (void)type;
   *ins = t_insert; *rem = t_remove;
   t_insert = t_remove = 0;
   return TRUE;
}
HANDLE XInputOpen(PXPP_DEVICE_TYPE type, DWORD port, DWORD slot,
      PXINPUT_POLLING_PARAMETERS params)
{
   (void)type; (void)slot; (void)params;
   t_opens++;
   return port == 0 ? T_HANDLE : NULL;
}
void   XInputClose(HANDLE d) { if (d == T_HANDLE) t_closes++; }
DWORD  XInputPoll(HANDLE d)
{
   if (d != T_HANDLE)
   {
      t_bad_handle++;
      return ERROR_DEVICE_NOT_CONNECTED;
   }
   return t_poll_ret;
}
DWORD  XInputGetState(HANDLE d, PXINPUT_STATE s)
{
   if (d != T_HANDLE)
   {
      t_bad_handle++;
      return ERROR_DEVICE_NOT_CONNECTED;
   }
   if (t_get_ret == ERROR_SUCCESS)
      *s = t_state;
   return t_get_ret;
}

#include "input/input_driver.h"
#include "input/drivers_joypad/xdk_joypad.c"

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

static int16_t axis(uint32_t a) { return xdk_joypad.axis(0, a); }

static bool all_centred(void)
{
   unsigned a;
   for (a = 0; a < 4; a++)
      if (axis(AXIS_POS(a)) || axis(AXIS_NEG(a)))
         return false;
   return true;
}

static void held_down(void)
{
   memset(&t_state, 0, sizeof(t_state));
   t_state.Gamepad.wButtons                         = XINPUT_GAMEPAD_START;
   t_state.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_A] = 255;
   t_state.Gamepad.sThumbLY                         = -32768;
   t_state.Gamepad.sThumbRX                         = 32767;
}

int main(void)
{
   unsigned i;

   check(xdk_joypad.init(NULL) != NULL, "init");

   held_down();
   t_insert = 1;
   xdk_joypad.poll();
   check(t_opens == 1, "inserted: opened once");
   check(xdk_joypad.query_pad(0), "inserted: there");
   check(xdk_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_START) != 0, "start held");
   check(xdk_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) != 0,     "A is B");
   check(axis(AXIS_NEG(1)) == -32767, "left stick y negative");
   check(axis(AXIS_POS(2)) == 32767,  "right stick x positive");

   /* The next polls read on the same handle. */
   for (i = 0; i < 3; i++)
      xdk_joypad.poll();
   check(t_bad_handle == 0, "later polls: same handle");
   check(axis(AXIS_NEG(1)) == -32767, "later polls: still read");

   /* A poll that fails, then a read that fails. */
   t_poll_ret = ERROR_DEVICE_NOT_CONNECTED;
   xdk_joypad.poll();
   check(all_centred() && !xdk_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_START),
         "failed poll: nothing held, centred");
   t_poll_ret = ERROR_SUCCESS;
   t_get_ret  = ERROR_DEVICE_NOT_CONNECTED;
   xdk_joypad.poll();
   check(all_centred(), "failed read: centred");
   t_get_ret  = ERROR_SUCCESS;
   xdk_joypad.poll();
   check(axis(AXIS_NEG(1)) == -32767, "answers again: read again");

   /* Removed with everything held. */
   t_remove = 1;
   xdk_joypad.poll();
   check(t_closes == 1, "removed: closed");
   check(!xdk_joypad.query_pad(0), "removed: gone");
   check(all_centred() && !xdk_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_START),
         "removed: nothing held, centred");

   /* Past the four ports. */
   for (i = DEFAULT_MAX_PADS; i < MAX_USERS; i++)
      if (     xdk_joypad.query_pad(i) || xdk_joypad.name(i)
            || xdk_joypad.axis(i, AXIS_NEG(1)))
         break;
   check(i == MAX_USERS, "past the ports: no pad, no name, no axis");

   /* Destroy closes what is open. */
   t_insert = 1;
   xdk_joypad.poll();
   xdk_joypad.destroy();
   check(t_closes == 2, "destroy: closed");

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("xdk_joypad: all checks passed\n");
   return 0;
}
