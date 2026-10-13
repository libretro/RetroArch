/* The PS4 joypad driver (input/drivers_joypad/ps4_joypad.c, the ORBIS
 * build), against the tree's ORBIS stubs, with the pad library and
 * user service calls defined by the test. Held to:
 *
 * - a pad that reports gives its buttons and both sticks;
 * - a pad that stops reporting (connected 0: asleep or out of range)
 *   holds nothing and reads centred, whatever its report's stick bytes
 *   hold and whatever it held last;
 * - a read that fails does the same;
 * - a pad that reports again is read again on the same handle;
 * - a pad is there while it reports, with nothing held, and not while
 *   it does not. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include <orbis/libScePad.h>
#include <orbis/orbisPad.h>
#include <defines/ps4_defines.h>

/* ---- the stand-ins ---- */
static ScePadData     t_report;
static int            t_ret;
static OrbisPadConfig t_conf;

int scePadInit(void) { return 0; }
int scePadOpen(SceUserServiceUserId user, int type, int index,
      uint8_t *param)
{
   (void)user; (void)type; (void)index; (void)param;
   return 7;
}
int scePadReadState(int handle, ScePadData *data)
{
   if (handle != 7)
      return -1;
   *data = t_report;
   return t_ret;
}
OrbisPadConfig *orbisPadGetConf(void) { return &t_conf; }

/* Declared by the driver itself under ORBIS. */
struct SceUserServiceLoginUserIdList;
int sceUserServiceGetLoginUserIdList(struct SceUserServiceLoginUserIdList *l);

#include "input/input_driver.h"
#include "input/drivers_joypad/ps4_joypad.c"

int sceUserServiceGetLoginUserIdList(SceUserServiceLoginUserIdList *l)
{
   unsigned i;
   for (i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++)
      l->userId[i] = (int32_t)SCE_USER_SERVICE_USER_ID_INVALID;
   l->userId[0] = 1;
   return 0;
}

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

static void poll_with(int ret, uint8_t connected, unsigned buttons,
      uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry)
{
   memset(&t_report, 0, sizeof(t_report));
   t_report.buttons   = buttons;
   t_report.lx        = lx;
   t_report.ly        = ly;
   t_report.rx        = rx;
   t_report.ry        = ry;
   t_report.connected = connected;
   t_ret              = ret;
   ps4_joypad.poll();
}

static int16_t axis(uint32_t a) { return ps4_joypad.axis(0, a); }

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
   check(ps4_joypad.init(NULL) != NULL, "init");

   poll_with(0, 1, ORBISPAD_CROSS | ORBISPAD_DOWN, 0xFF, 0xFF, 0x00, 0x00);
   check(ps4_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) != 0,    "cross is B");
   check(ps4_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0, "down held");
   check(axis(AXIS_POS(1)) == 0x7fff,  "left y down");
   check(axis(AXIS_NEG(3)) == -0x7fff, "right y up");

   /* Reporting with nothing held: there. */
   poll_with(0, 1, 0, 0x80, 0x80, 0x80, 0x80);
   check(ps4_joypad.query_pad(0), "idle pad: there");
   check(!ps4_joypad.query_pad(1), "no pad: not there");

   /* Stops reporting: the report still carries the last bytes. */
   poll_with(0, 0, ORBISPAD_CROSS, 0xFF, 0xFF, 0x00, 0x00);
   check(ps4_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) == 0, "not reporting: nothing held");
   check(all_centred(), "not reporting: sticks centred");
   check(!ps4_joypad.query_pad(0), "not reporting: not there");

   /* Reports again. */
   poll_with(0, 1, 0, 0x80, 0xFF, 0x80, 0x80);
   check(axis(AXIS_POS(1)) == 0x7fff, "reporting again: read again");
   check(ps4_joypad.query_pad(0), "reporting again: there");

   /* A failed read. */
   poll_with(-1, 1, ORBISPAD_CROSS, 0xFF, 0xFF, 0x00, 0x00);
   check(ps4_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) == 0, "failed read: nothing held");
   check(all_centred(), "failed read: sticks centred");

   ps4_joypad.destroy();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("ps4_joypad: all checks passed\n");
   return 0;
}
