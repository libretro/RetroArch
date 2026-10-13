/* The PSP joypad driver (input/drivers_joypad/psp_joypad.c, the PSP
 * build), with the controller calls replaced by the test's. A read
 * that fails leaves the caller's SceCtrlData as it was, which the test
 * fills with junk first. Held to:
 *
 * - a read that succeeds gives the buttons and the analog nub, both
 *   directions of both axes;
 * - a read that fails holds nothing and reads the nub centred, whatever
 *   the unfilled report holds and whatever the last good read held. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "pspctrl.h"

/* ---- the stand-ins ---- */
static SceCtrlData t_report;
static int         t_ret;

int sceCtrlSetSamplingCycle(int cycle) { (void)cycle; return 0; }
int sceCtrlSetSamplingMode(int mode)   { (void)mode;  return 0; }
int sceCtrlPeekBufferPositive(SceCtrlData *pad_data, int count)
{
   (void)count;
   if (t_ret >= 0)
      *pad_data = t_report;
   return t_ret;
}

/* The poll's report lives on the stack; what a failed read leaves in
 * it is whatever was there. Each poll runs behind a call that leaves
 * junk where the report will be. */
static void __attribute__((noinline)) dirty_stack(void)
{
   volatile unsigned char junk[512];
   unsigned i;
   for (i = 0; i < sizeof(junk); i++)
      junk[i] = 0xFF;
}

#include "input/drivers_joypad/psp_joypad.c"

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

static void poll_with(int ret, unsigned buttons, unsigned char lx,
      unsigned char ly)
{
   memset(&t_report, 0, sizeof(t_report));
   t_report.Buttons = buttons;
   t_report.Lx      = lx;
   t_report.Ly      = ly;
   t_ret            = ret;
   dirty_stack();
   psp_joypad.poll();
}

static int16_t axis(uint32_t a) { return psp_joypad.axis(0, a); }

int main(void)
{
   check(psp_joypad.init(NULL) != NULL, "init");

   poll_with(1, PSP_CTRL_CROSS | PSP_CTRL_DOWN, 0xFF, 0x00);
   check(psp_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) != 0,    "cross is B");
   check(psp_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0, "down held");
   check(axis(AXIS_POS(0)) > 0x7e00,  "nub right");
   check(axis(AXIS_NEG(1)) == -0x7fff, "nub up");

   poll_with(1, 0, 0x00, 0xFF);
   check(axis(AXIS_NEG(0)) == -0x7fff, "nub left");
   check(axis(AXIS_POS(1)) > 0x7e00,  "nub down");

   /* A failed read: nothing held, nub centred. */
   poll_with(-1, PSP_CTRL_CROSS, 0xFF, 0xFF);
   check(psp_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) == 0,    "failed read holds nothing");
   check(psp_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_DOWN) == 0, "failed read: no down");
   check(axis(AXIS_POS(0)) == 0 && axis(AXIS_NEG(0)) == 0
      && axis(AXIS_POS(1)) == 0 && axis(AXIS_NEG(1)) == 0,
      "failed read: nub centred");

   psp_joypad.destroy();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("psp_pad_read: all checks passed\n");
   return 0;
}
