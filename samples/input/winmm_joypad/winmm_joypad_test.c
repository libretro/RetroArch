/* The winmm joypad driver (input/drivers_joypad/winmm_joypad.c), with
 * the multimedia library's joystick calls and the clock replaced by
 * ones the test controls - no controller is needed, and none could be
 * scripted. Held to:
 *
 * - a controller that is there at the start is announced once, by its
 *   name and maker's numbers, and one that only its driver answers for
 *   (capabilities, no position) is not;
 * - an axis runs from -0x7fff to 0x7fff across whatever range the
 *   controller gave, is read from the side asked for, and an axis the
 *   controller does not have reads nothing whatever is reported;
 * - all thirty-two buttons, the first and the last;
 * - the hat: the eight ways and centred;
 * - the RetroPad mask from binds: the port's own button, the
 *   profile's axis;
 * - a controller pulled out is let go at that poll and is not asked
 *   again until the next look, a second later by the clock, when one
 *   plugged in is found; the fifteen that are not there are not asked
 *   at every poll. */

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>

#include <boolean.h>

/* ---- the stand-ins ---- */
static bool      t_plugged[16];
static bool      t_caps_only[16];   /* its driver answers; it is not there */
static JOYINFOEX t_info[16];
static unsigned  t_pos_calls[16];
static DWORD     t_clock;

static UINT test_joyGetNumDevs(void) { return 16; }
static MMRESULT test_joyGetDevCapsA(UINT_PTR id, LPJOYCAPSA caps, UINT size)
{
   if (id >= 16 || !(t_plugged[id] || t_caps_only[id]))
      return JOYERR_PARMS;
   memset(caps, 0, size);
   caps->wMid  = 0x045e;
   caps->wPid  = 0x1234;
   strcpy(caps->szPname, "Test Pad");
   caps->wXmin = 0;   caps->wXmax = 65535;
   caps->wYmin = 0;   caps->wYmax = 65535;
   caps->wZmin = 0;   caps->wZmax = 255;
   caps->wRmin = 100; caps->wRmax = 1123;
   caps->wCaps = JOYCAPS_HASZ | JOYCAPS_HASR | JOYCAPS_HASPOV;
   return JOYERR_NOERROR;
}
static MMRESULT test_joyGetPosEx(UINT id, LPJOYINFOEX info)
{
   if (id < 16)
      t_pos_calls[id]++;
   if (id >= 16 || !t_plugged[id])
      return JOYERR_UNPLUGGED;
   *info = t_info[id];
   return JOYERR_NOERROR;
}
static DWORD test_GetTickCount(void) { return t_clock; }

#define joyGetNumDevs  test_joyGetNumDevs
#define joyGetDevCapsA test_joyGetDevCapsA
#define joyGetPosEx    test_joyGetPosEx
#define GetTickCount   test_GetTickCount
#include "../../../input/drivers_joypad/winmm_joypad.c"
#undef GetTickCount

/* ---- what the driver calls ---- */
static unsigned t_connects, t_disconnects, t_last_port, t_last_vid, t_last_pid;
static char     t_last_name[64], t_last_driver[16];
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   (void)display_name; (void)phys;
   t_connects++;
   t_last_port = port; t_last_vid = vid; t_last_pid = pid;
   strlcpy(t_last_name, name, sizeof(t_last_name));
   strlcpy(t_last_driver, driver, sizeof(t_last_driver));
   return true;
}
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{
   (void)name;
   t_disconnects++;
   t_last_port = port;
   return true;
}

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL: "); fprintf(stderr, __VA_ARGS__); \
   fprintf(stderr, "\n"); failures++; } } while (0)

static void pad0_centre(void)
{
   memset(&t_info[0], 0, sizeof(t_info[0]));
   t_info[0].dwXpos = 32767;
   t_info[0].dwYpos = 32768;
   t_info[0].dwZpos = 127;
   t_info[0].dwRpos = 611;
   t_info[0].dwPOV  = JOY_POVCENTERED;
}

int main(void)
{
   static const struct { DWORD pov; unsigned want; const char *what; } hats[] = {
      { 0,     HAT_UP_MASK,                    "up" },
      { 4500,  HAT_UP_MASK | HAT_RIGHT_MASK,   "up and right" },
      { 9000,  HAT_RIGHT_MASK,                 "right" },
      { 13500, HAT_DOWN_MASK | HAT_RIGHT_MASK, "down and right" },
      { 18000, HAT_DOWN_MASK,                  "down" },
      { 22500, HAT_DOWN_MASK | HAT_LEFT_MASK,  "down and left" },
      { 27000, HAT_LEFT_MASK,                  "left" },
      { 31500, HAT_UP_MASK | HAT_LEFT_MASK,    "up and left" },
      { 35900, HAT_UP_MASK,                    "almost round to up" },
      { JOY_POVCENTERED, 0,                    "centred" } };
   static const unsigned dirs[4] = {
      HAT_UP_MASK, HAT_DOWN_MASK, HAT_LEFT_MASK, HAT_RIGHT_MASK };
   struct retro_keybind binds[RARCH_BIND_LIST_END];
   struct retro_keybind autob[RARCH_BIND_LIST_END];
   rarch_joypad_info_t info;
   input_bits_t bits;
   unsigned i, d, before;

   /* one controller there, one whose driver alone answers */
   t_plugged[0]   = true;
   t_caps_only[3] = true;
   pad0_centre();
   t_clock        = 5000;
   CHECK(winmm_joypad.init(NULL) != NULL, "the driver did not start");
   CHECK(t_connects == 1 && t_last_port == 0, "%u controller(s) announced, the last at %u", t_connects, t_last_port);
   CHECK(!strcmp(t_last_name, "Test Pad") && !strcmp(t_last_driver, "winmm"),
         "announced as \"%s\" by \"%s\"", t_last_name, t_last_driver);
   CHECK(t_last_vid == 0x045e && t_last_pid == 0x1234, "announced with %04x:%04x", t_last_vid, t_last_pid);
   CHECK(winmm_joypad.query_pad(0) && !winmm_joypad.query_pad(1) && !winmm_joypad.query_pad(3),
         "which controllers are there: 0 %d, 1 %d, 3 %d", winmm_joypad.query_pad(0), winmm_joypad.query_pad(1), winmm_joypad.query_pad(3));
   CHECK(winmm_joypad.name(0) && !strcmp(winmm_joypad.name(0), "Test Pad") && !winmm_joypad.name(1), "the names");

   /* axes */
   winmm_joypad.poll();
   CHECK(winmm_joypad.axis(0, AXIS_NEG(0)) == 0 && winmm_joypad.axis(0, AXIS_POS(0)) == 0, "X at rest reads %d / %d",
         winmm_joypad.axis(0, AXIS_NEG(0)), winmm_joypad.axis(0, AXIS_POS(0)));
   t_info[0].dwXpos = 0;
   t_info[0].dwYpos = 65535;
   t_info[0].dwZpos = 255;
   t_info[0].dwRpos = 100;
   t_info[0].dwUpos = 60000;      /* an axis it does not have */
   winmm_joypad.poll();
   CHECK(winmm_joypad.axis(0, AXIS_NEG(0)) == -0x7fff && winmm_joypad.axis(0, AXIS_POS(0)) == 0,
         "X hard over reads %d from its negative side, %d from its positive", winmm_joypad.axis(0, AXIS_NEG(0)), winmm_joypad.axis(0, AXIS_POS(0)));
   CHECK(winmm_joypad.axis(0, AXIS_POS(1)) == 0x7fff && winmm_joypad.axis(0, AXIS_NEG(1)) == 0,
         "Y hard over reads %d / %d", winmm_joypad.axis(0, AXIS_POS(1)), winmm_joypad.axis(0, AXIS_NEG(1)));
   CHECK(winmm_joypad.axis(0, AXIS_POS(2)) == 0x7fff, "Z at the top of a range of 255 reads %d", winmm_joypad.axis(0, AXIS_POS(2)));
   CHECK(winmm_joypad.axis(0, AXIS_NEG(3)) == -0x7fff, "R at the bottom of 100..1123 reads %d", winmm_joypad.axis(0, AXIS_NEG(3)));
   CHECK(winmm_joypad.axis(0, AXIS_POS(4)) == 0 && winmm_joypad.axis(0, AXIS_NEG(4)) == 0, "an axis the controller does not have reads %d", winmm_joypad.axis(0, AXIS_POS(4)));
   CHECK(winmm_joypad.axis(1, AXIS_POS(0)) == 0 && winmm_joypad.axis(40, AXIS_POS(0)) == 0, "a controller that is not there has an axis");

   /* buttons */
   pad0_centre();
   t_info[0].dwButtons = 0x80000001u;
   winmm_joypad.poll();
   CHECK(winmm_joypad.button(0, 0) && winmm_joypad.button(0, 31) && !winmm_joypad.button(0, 1) && !winmm_joypad.button(0, 30),
         "the first and last of thirty-two buttons");
   CHECK(!winmm_joypad.button(0, 32) && !winmm_joypad.button(1, 0), "a button past the last, or on no controller, is down");
   memset(&bits, 0, sizeof(bits));
   winmm_joypad.get_buttons(0, &bits);
   CHECK(BIT256_GET(bits, 0) && BIT256_GET(bits, 31) && !BIT256_GET(bits, 5), "the buttons as a set");

   /* the hat */
   t_info[0].dwButtons = 0;
   for (i = 0; i < sizeof(hats) / sizeof(hats[0]); i++)
   {
      unsigned got = 0;
      t_info[0].dwPOV = hats[i].pov;
      winmm_joypad.poll();
      for (d = 0; d < 4; d++)
         if (winmm_joypad.button(0, HAT_MAP(0, dirs[d])))
            got |= dirs[d];
      CHECK(got == hats[i].want, "the hat %s reads %04x, wanted %04x", hats[i].what, got, hats[i].want);
   }
   CHECK(!winmm_joypad.button(0, HAT_MAP(1, HAT_UP_MASK)), "a second hat reads");

   /* the RetroPad mask: B on the port's own button 2, Left on the
    * profile's X axis, negative side */
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      binds[i].joykey  = NO_BTN;  binds[i].joyaxis = AXIS_NONE;  binds[i].attr = 0;
      autob[i]         = binds[i];
   }
   binds[RETRO_DEVICE_ID_JOYPAD_B].joykey     = 2;
   autob[RETRO_DEVICE_ID_JOYPAD_LEFT].joyaxis = AXIS_NEG(0);
   memset(&info, 0, sizeof(info));
   info.joy_idx        = 0;
   info.auto_binds     = autob;
   info.axis_threshold = 0.5f;
   pad0_centre();
   winmm_joypad.poll();
   CHECK(winmm_joypad.state(&info, binds, 0) == 0, "the mask at rest is %04x", (unsigned)winmm_joypad.state(&info, binds, 0));
   t_info[0].dwButtons = 1u << 2;
   t_info[0].dwXpos    = 0;
   winmm_joypad.poll();
   CHECK(winmm_joypad.state(&info, binds, 0) == ((1 << RETRO_DEVICE_ID_JOYPAD_B) | (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)),
         "the mask with B and Left held is %04x", (unsigned)winmm_joypad.state(&info, binds, 0));

   /* ten polls inside one second: the controllers that are not there
    * are not asked */
   pad0_centre();
   before = 0;
   for (i = 1; i < 16; i++)
      before += t_pos_calls[i];
   for (i = 0; i < 10; i++)
   {
      t_clock += 16;
      winmm_joypad.poll();
   }
   d = 0;
   for (i = 1; i < 16; i++)
      d += t_pos_calls[i];
   CHECK(d == before, "controllers that are not there were asked %u time(s) in ten polls within a second", d - before);

   /* pulled out: let go at that poll, not asked again until the look */
   t_plugged[0] = false;
   before       = t_pos_calls[0];
   t_clock     += 16;
   winmm_joypad.poll();
   CHECK(t_disconnects == 1 && !winmm_joypad.query_pad(0), "pulled out: %u let go, still there %d", t_disconnects, winmm_joypad.query_pad(0));
   CHECK(!winmm_joypad.button(0, 2) && winmm_joypad.axis(0, AXIS_NEG(0)) == 0, "a controller pulled out still holds something");
   for (i = 0; i < 5; i++)
   {
      t_clock += 16;
      winmm_joypad.poll();
   }
   CHECK(t_pos_calls[0] == before + 1, "a controller pulled out was asked %u time(s) after the poll that lost it", t_pos_calls[0] - before - 1);

   /* plugged in again, and another beside it: found at the next look */
   t_plugged[0] = true;
   t_plugged[2] = true;
   pad0_centre();
   t_info[2]    = t_info[0];
   t_connects   = 0;
   t_clock     += 16;
   winmm_joypad.poll();
   CHECK(t_connects == 0, "found before a second had passed");
   t_clock     += 1100;
   winmm_joypad.poll();
   CHECK(t_connects == 2 && winmm_joypad.query_pad(0) && winmm_joypad.query_pad(2), "after a second: %u found", t_connects);

   /* the clock's counter wrapping does not stop the looking */
   t_plugged[5] = true;
   t_info[5]    = t_info[0];
   t_clock      = 0xFFFFFF00u;
   winmm_joypad.poll();                 /* a look, at the new time */
   t_plugged[6] = true;
   t_info[6]    = t_info[0];
   t_clock      = 0x00000400u;          /* 1280 ms later, past the wrap */
   winmm_joypad.poll();
   CHECK(winmm_joypad.query_pad(6), "a controller plugged in across the wrap of the clock was not found");

   winmm_joypad.destroy();
   CHECK(!winmm_joypad.query_pad(0), "a controller is there after the driver was taken down");
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("PASS winmm_joypad_test\n");
   return 0;
}
