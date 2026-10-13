/* The PS2 joypad driver (input/drivers_joypad/ps2_joypad.c), against
 * the tree's PS2 stubs, with the libpad and libmtap calls defined by the
 * test. padRead() hands over the whole 32-byte report as padman does,
 * whatever mode the pad is in, so the bytes behind the sticks can be
 * anything the test puts there.
 * Held to:
 *
 * - a DualShock in analog mode (0x73) and in pressure mode (0x79)
 *   reads its sticks, both directions of every axis;
 * - a pad in digital mode (0x41) reads centred sticks whatever the
 *   stick bytes hold, and nothing on the RetroPad from the axis the
 *   d-pad down is bound to;
 * - a pad that leaves analog mode lets its sticks go at that poll;
 * - a pad of another type (0x23) reads centred sticks;
 * - the buttons map to the RetroPad, active low;
 * - a pad disconnected, executing a command, in error or unreadable
 *   holds nothing and reads centred;
 * - a pad is there with nothing held, and while it changes mode, and
 *   not once it is disconnected, in error or unreadable;
 * - a stable DualShock in digital mode is asked for analog mode once,
 *   unlocked, and asked again only after it is disconnected; one with a
 *   request in flight is asked once it completes; a pad with no
 *   DualShock mode, one already in analog mode and one not yet stable
 *   are not asked, and none of them is queried again. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "kernel.h"
#include "libpad.h"
#include "libmtap.h"

/* ---- the stand-ins ---- */
static unsigned char t_report[2][4][32];
static int           t_state[2][4];
static unsigned char t_read_len[2][4];
static int           t_modes[4];
static int           t_nmodes;
static unsigned char t_req = PAD_RSTAT_COMPLETE;
static unsigned      t_info_calls, t_set_calls;
static int           t_set_mode, t_set_lock;

int DIntr(void)             { return 0; }
int EIntr(void)             { return 0; }
int mtapPortOpen(int port)  { (void)port; return 1; }
int mtapPortClose(int port) { (void)port; return 1; }
int padPortOpen(int port, int slot, void *area)
{
   (void)port; (void)slot; (void)area;
   return 1;
}
int padPortClose(int port, int slot) { (void)port; (void)slot; return 1; }
int padGetState(int port, int slot)  { return t_state[port][slot]; }
unsigned char padGetReqState(int port, int slot)
{
   (void)port; (void)slot;
   return t_req;
}
int padInfoMode(int port, int slot, int info, int index)
{
   (void)port; (void)slot;
   t_info_calls++;
   if (info != PAD_MODETABLE)
      return 0;
   if (index == -1)
      return t_nmodes;
   return (index >= 0 && index < t_nmodes) ? t_modes[index] : 0;
}
int padSetMainMode(int port, int slot, int mode, int lock)
{
   (void)port; (void)slot;
   t_set_calls++;
   t_set_mode = mode;
   t_set_lock = lock;
   return 1;
}
unsigned char padRead(int port, int slot, struct padButtonStatus *data)
{
   unsigned char len = t_read_len[port][slot];
   memcpy(data, t_report[port][slot], len);
   return len;
}

#include "input/drivers_joypad/ps2_joypad.c"

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

/* Pad 0 is port 0, slot 0. Buttons are active low on the wire. */
static void report(int state, unsigned char mode, unsigned short held,
      unsigned char lh, unsigned char lv, unsigned char rh, unsigned char rv)
{
   unsigned short btns = (unsigned short)(0xffff ^ held);
   memset(t_report[0][0], 0xA5, sizeof(t_report[0][0]));
   t_report[0][0][0] = 0;
   t_report[0][0][1] = mode;
   memcpy(&t_report[0][0][2], &btns, sizeof(btns));
   t_report[0][0][4] = rh;
   t_report[0][0][5] = rv;
   t_report[0][0][6] = lh;
   t_report[0][0][7] = lv;
   t_state[0][0]     = state;
   t_read_len[0][0]  = 32;
   ps2_joypad.poll();
}

static int16_t axis(uint32_t a) { return ps2_joypad.axis(0, a); }

static bool all_centred(void)
{
   unsigned a;
   for (a = 0; a < 4; a++)
      if (axis(AXIS_POS(a)) || axis(AXIS_NEG(a)))
         return false;
   return true;
}

static int16_t down_mask(void)
{
   uint16_t joykeys[RARCH_FIRST_CUSTOM_BIND];
   uint32_t joyaxes[RARCH_FIRST_CUSTOM_BIND];
   rarch_joypad_info_t info;
   unsigned i;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      joykeys[i] = NO_BTN;
      joyaxes[i] = AXIS_NONE;
   }
   joyaxes[RETRO_DEVICE_ID_JOYPAD_DOWN] = AXIS_POS(1);
   info.auto_binds     = NULL;
   info.axis_threshold = 0.5f;
   info.joy_idx        = 0;
   return ps2_joypad.state(&info, joykeys, joyaxes, 0);
}

int main(void)
{
   int i;

   check(ps2_joypad.init(NULL) != NULL, "init");
   for (i = 0; i < 4; i++)
   {
      t_state[0][i] = PAD_STATE_DISCONN;
      t_state[1][i] = PAD_STATE_DISCONN;
   }

   /* DualShock, analog: left stick full down and right, right stick
    * full up and left. */
   report(PAD_STATE_STABLE, 0x73, 0, 0xFF, 0xFF, 0x00, 0x00);
   check(axis(AXIS_POS(0)) == 0x7fff,  "analog: left x right");
   check(axis(AXIS_POS(1)) == 0x7fff,  "analog: left y down");
   check(axis(AXIS_NEG(2)) == -0x7fff, "analog: right x left");
   check(axis(AXIS_NEG(3)) == -0x7fff, "analog: right y up");
   check(axis(AXIS_NEG(1)) == 0,       "analog: left y up reads nothing");
   check(down_mask() == (1 << RETRO_DEVICE_ID_JOYPAD_DOWN),
         "analog: stick down presses down");

   /* Pressure mode is a DualShock mode too. */
   report(PAD_STATE_STABLE, 0x79, 0, 0x80, 0xFF, 0x80, 0x80);
   check(axis(AXIS_POS(1)) == 0x7fff,  "pressure: left y down");

   /* Analog joystick (0x5x). */
   report(PAD_STATE_STABLE, 0x53, 0, 0x80, 0x00, 0x80, 0x80);
   check(axis(AXIS_NEG(1)) == -0x7fff, "analog joystick: left y up");

   /* Leaves analog mode: the stick bytes still hold the last analog
    * report, as padman leaves them. */
   report(PAD_STATE_STABLE, 0x41, 0, 0xFF, 0xFF, 0x00, 0x00);
   check(all_centred(),     "digital: sticks centred over stale bytes");
   check(down_mask() == 0,  "digital: nothing from the stick axis");

   /* Digital with bytes no pad ever sent. */
   report(PAD_STATE_STABLE, 0x41, 0, 0xA5, 0xFE, 0x13, 0xC0);
   check(all_centred(),     "digital: sticks centred over junk");

   /* Another type's bytes are not sticks. */
   report(PAD_STATE_STABLE, 0x23, 0, 0x80, 0x00, 0xFF, 0x00);
   check(all_centred(),     "negcon: sticks centred");

   /* Buttons, active low. */
   report(PAD_STATE_STABLE, 0x41,
         PAD_DOWN | PAD_CROSS | PAD_START | PAD_L3, 0x80, 0x80, 0x80, 0x80);
   check(ps2_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0,  "down held");
   check(ps2_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) != 0,     "cross is B");
   check(ps2_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_START) != 0, "start held");
   check(ps2_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_L3) != 0,    "L3 held");
   check(ps2_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_UP) == 0,    "up free");
   check(ps2_joypad.query_pad(0),                                 "pad there");

   /* Nothing held: still there; changing mode: still there. */
   report(PAD_STATE_STABLE, 0x73, 0, 0x80, 0x80, 0x80, 0x80);
   check(ps2_joypad.query_pad(0),  "idle pad: there");
   check(!ps2_joypad.query_pad(1), "no pad: not there");
   report(PAD_STATE_EXECCMD, 0x73, 0, 0x80, 0x80, 0x80, 0x80);
   check(ps2_joypad.query_pad(0),  "changing mode: there");

   /* Every state that is not read lets everything go. */
   {
      static const int gone[] = {
         PAD_STATE_DISCONN, PAD_STATE_EXECCMD, PAD_STATE_ERROR };
      for (i = 0; i < 3; i++)
      {
         report(PAD_STATE_STABLE, 0x73, PAD_CROSS, 0xFF, 0xFF, 0xFF, 0xFF);
         report(gone[i], 0x73, PAD_CROSS, 0xFF, 0xFF, 0xFF, 0xFF);
         check(ps2_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) == 0
               && all_centred(), "unread state: nothing held, centred");
         check(ps2_joypad.query_pad(0) == (gone[i] == PAD_STATE_EXECCMD),
               "unread state: there only while changing mode");
      }
   }

   /* A read that returns nothing lets everything go. */
   report(PAD_STATE_STABLE, 0x73, PAD_CROSS, 0xFF, 0xFF, 0xFF, 0xFF);
   t_read_len[0][0] = 0;
   ps2_joypad.poll();
   check(!ps2_joypad.query_pad(0) && all_centred(), "failed read clears");

   /* Analog mode: a DualShock that offers it, in digital mode. */
   report(PAD_STATE_DISCONN, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   t_modes[0] = PAD_TYPE_DIGITAL;
   t_modes[1] = PAD_TYPE_DUALSHOCK;
   t_nmodes   = 2;
   t_set_calls = t_info_calls = 0;
   report(PAD_STATE_STABLE, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   check(t_set_calls == 1, "dualshock: asked for analog");
   check(t_set_mode == PAD_MMODE_DUALSHOCK && t_set_lock == PAD_MMODE_UNLOCK,
         "dualshock: analog, unlocked");
   {
      unsigned info = t_info_calls;
      for (i = 0; i < 5; i++)
         report(PAD_STATE_STABLE, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
      check(t_set_calls == 1 && t_info_calls == info,
            "dualshock switched back by hand: not asked again");
   }

   /* Unplugged and back: asked again. */
   report(PAD_STATE_DISCONN, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   report(PAD_STATE_STABLE, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   check(t_set_calls == 2, "reconnected: asked again");

   /* A request in flight: asked once it completes. */
   report(PAD_STATE_DISCONN, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   t_req = PAD_RSTAT_BUSY;
   t_set_calls = t_info_calls = 0;
   report(PAD_STATE_STABLE, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   check(t_set_calls == 0 && t_info_calls == 0, "busy: not asked yet");
   t_req = PAD_RSTAT_COMPLETE;
   report(PAD_STATE_STABLE, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   check(t_set_calls == 1, "busy then done: asked");

   /* Not yet stable: not asked. */
   report(PAD_STATE_DISCONN, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   t_set_calls = t_info_calls = 0;
   report(PAD_STATE_FINDCTP1, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   check(t_set_calls == 0 && t_info_calls == 0, "not stable: not asked");

   /* Already in analog mode: nothing asked, nothing queried. */
   report(PAD_STATE_DISCONN, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   t_set_calls = t_info_calls = 0;
   report(PAD_STATE_STABLE, 0x73, 0, 0x80, 0x80, 0x80, 0x80);
   report(PAD_STATE_STABLE, 0x73, 0, 0x80, 0x80, 0x80, 0x80);
   check(t_set_calls == 0 && t_info_calls == 0, "already analog: not asked");

   /* A digital-only pad: queried once, never asked, never again. */
   report(PAD_STATE_DISCONN, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   t_nmodes    = 0;
   t_set_calls = t_info_calls = 0;
   report(PAD_STATE_STABLE, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   {
      unsigned info = t_info_calls;
      for (i = 0; i < 5; i++)
         report(PAD_STATE_STABLE, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
      check(t_set_calls == 0, "digital only: not asked");
      check(info == 1 && t_info_calls == info, "digital only: queried once");
   }

   /* A pad with modes but none of them DualShock (a neGcon). */
   report(PAD_STATE_DISCONN, 0x41, 0, 0x80, 0x80, 0x80, 0x80);
   t_modes[0] = PAD_TYPE_NEJICON;
   t_nmodes   = 1;
   t_set_calls = 0;
   report(PAD_STATE_STABLE, 0x23, 0, 0x80, 0x80, 0x80, 0x80);
   check(t_set_calls == 0, "no dualshock mode: not asked");

   ps2_joypad.destroy();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("ps2_joypad: all checks passed\n");
   return 0;
}
