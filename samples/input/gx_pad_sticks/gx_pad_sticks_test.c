/* The libogc GameCube/Wii joypad driver
 * (input/drivers_joypad/gx_joypad_libogc.c, the Wii build), with libogc
 * replaced by the test's. Held to:
 *
 * - a GameCube pad gives both sticks; pulled out with them held, they
 *   read centred;
 * - a Nunchuk gives its stick; pulled out (a bare Wiimote left) with
 *   the stick held, it reads centred;
 * - a Classic gives both sticks; a Nunchuk in its place leaves the
 *   right stick centred;
 * - a Wiimote that goes away with a stick held leaves it centred;
 * - a joypad index past the driver's pads (any user's device index
 *   reaches query_pad and name) is no pad and has no name, and reads
 *   nothing outside the driver's tables. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include <gccore.h>
#include <ogc/pad.h>
#include <wiiuse/wpad.h>

/* ---- the stand-ins ---- */
static bool     t_gc;            /* a GameCube pad in port 0 */
static s8       t_gc_stick[4];   /* x, y, sub x, sub y */
static bool     t_wm;            /* a Wiimote on channel 0 */
static WPADData t_wpad;

resetcallback SYS_SetResetCallback(resetcallback cb) { (void)cb; return NULL; }
powercallback SYS_SetPowerCallback(powercallback cb) { (void)cb; return NULL; }
u32 PAD_Init(void)              { return 1; }
u32 PAD_ScanPads(void)          { return t_gc ? 1 : 0; }
u16 PAD_ButtonsHeld(int pad)    { (void)pad; return 0; }
s8  PAD_StickX(int pad)         { (void)pad; return t_gc_stick[0]; }
s8  PAD_StickY(int pad)         { (void)pad; return t_gc_stick[1]; }
s8  PAD_SubStickX(int pad)      { (void)pad; return t_gc_stick[2]; }
s8  PAD_SubStickY(int pad)      { (void)pad; return t_gc_stick[3]; }
u8  PAD_TriggerL(int pad)       { (void)pad; return 0; }
u8  PAD_TriggerR(int pad)       { (void)pad; return 0; }
s32 WPAD_Init(void)                              { return 0; }
s32 WPAD_ReadPending(s32 c, WPADDataCallback cb) { (void)c; (void)cb; return 0; }
s32 WPAD_SetDataFormat(s32 c, s32 f)             { (void)c; (void)f; return 0; }
s32 WPAD_SetVRes(s32 c, u32 x, u32 y)            { (void)c; (void)x; (void)y; return 0; }
s32 WPAD_Disconnect(s32 c)                       { (void)c; return 0; }
s32 WPAD_Flush(s32 c)                            { (void)c; return 0; }
s32 WPAD_Probe(s32 chan, u32 *type)
{
   if (chan != 0 || !t_wm)
      return -1;
   *type = (u32)t_wpad.exp.type;
   return WPAD_ERR_NONE;
}
void WPAD_IR(int chan, ir_t *ir) { (void)chan; memset(ir, 0, sizeof(*ir)); }
WPADData *WPAD_Data(int chan)    { (void)chan; return &t_wpad; }

#include "input/input_driver.h"
#include "input/drivers_joypad/gx_joypad_libogc.c"

bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   (void)name; (void)display_name; (void)phys; (void)driver;
   (void)port; (void)vid; (void)pid;
   return true;
}
unsigned input_config_get_joypad_index(unsigned port) { (void)port; return 0; }
void input_config_set_joypad_index(unsigned port, unsigned joypad_index)
{
   (void)port; (void)joypad_index;
}
void input_config_set_device_name(unsigned port, const char *name)
{
   (void)port; (void)name;
}
void input_driver_set_platform_menu_button(bool held) { (void)held; }
void input_driver_platform_request(enum input_platform_request req)
{
   (void)req;
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

static int16_t axis(uint32_t a) { return gx_joypad.axis(0, a); }

static bool all_centred(void)
{
   unsigned a;
   for (a = 0; a < 4; a++)
      if (axis(AXIS_POS(a)) || axis(AXIS_NEG(a)))
         return false;
   return true;
}

static void wiimote(int ext, float lmag, float lang, float rmag, float rang)
{
   memset(&t_wpad, 0, sizeof(t_wpad));
   t_wm             = true;
   t_wpad.exp.type  = ext;
   if (ext == WPAD_EXP_CLASSIC)
   {
      t_wpad.exp.classic.ljs.mag = lmag;
      t_wpad.exp.classic.ljs.ang = lang;
      t_wpad.exp.classic.rjs.mag = rmag;
      t_wpad.exp.classic.rjs.ang = rang;
   }
   else if (ext == WPAD_EXP_NUNCHUK)
   {
      t_wpad.exp.nunchuk.js.mag = lmag;
      t_wpad.exp.nunchuk.js.ang = lang;
   }
}

int main(void)
{
   check(gx_joypad.init(NULL) != NULL, "init");

   /* GameCube pad, both sticks pushed down. */
   t_gc = true;
   t_gc_stick[0] = 0;  t_gc_stick[1] = -100;
   t_gc_stick[2] = 0;  t_gc_stick[3] = -100;
   gx_joypad.poll();
   check(axis(AXIS_POS(1)) > 0x6000, "gc: main stick down");
   check(axis(AXIS_POS(3)) > 0x6000, "gc: c-stick down");
   t_gc = false;
   gx_joypad.poll();
   check(all_centred(), "gc pulled: sticks centred");

   /* Nunchuk held down (angle 180), then pulled out. */
   wiimote(WPAD_EXP_NUNCHUK, 1.0f, 180.0f, 0, 0);
   gx_joypad.poll();
   check(axis(AXIS_POS(1)) > 0x7000, "nunchuk: stick down");
   wiimote(WPAD_EXP_NONE, 0, 0, 0, 0);
   gx_joypad.poll();
   check(all_centred(), "nunchuk pulled: sticks centred");

   /* Classic with the right stick held down, then a Nunchuk. */
   wiimote(WPAD_EXP_CLASSIC, 0, 0, 1.0f, 180.0f);
   gx_joypad.poll();
   check(axis(AXIS_POS(3)) > 0x7000, "classic: right stick down");
   wiimote(WPAD_EXP_NUNCHUK, 0, 0, 0, 0);
   gx_joypad.poll();
   check(axis(AXIS_POS(3)) == 0, "nunchuk for classic: right stick centred");

   /* The Wiimote goes away with the stick held. */
   wiimote(WPAD_EXP_NUNCHUK, 1.0f, 180.0f, 0, 0);
   gx_joypad.poll();
   t_wm = false;
   gx_joypad.poll();
   check(all_centred(), "wiimote gone: sticks centred");

   /* Indices the frontend can pass that the driver does not track. */
   {
      unsigned pad;
      for (pad = DEFAULT_MAX_PADS; pad < MAX_USERS; pad++)
      {
         check(!gx_joypad.query_pad(pad), "past the pads: no pad");
         check(gx_joypad.name(pad) == NULL, "past the pads: no name");
      }
   }

   gx_joypad.destroy();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("gx_pad_sticks: all checks passed\n");
   return 0;
}
