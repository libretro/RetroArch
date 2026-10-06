/* Oracle for input/input_pointer_frame.h: the frontend's answers for the
 * mouse, the pointer and the lightgun's aim, from what an input driver
 * published at its poll.
 *
 * The header is included as input_driver.c includes it, with a viewport
 * and a translation of this test's own that count how often they are
 * asked - which is the point of the store: a device is placed in the
 * viewport once a poll, however many values are read.
 *
 * 1. the mouse: motion, place in the window, buttons, the second port's
 *    device, a port with none;
 * 2. a wheel notch reads the same every time in its frame;
 * 3. the pointer from a mouse standing for three touches, and for one;
 * 4. real touches: a place with a touch reads the touch, one that is
 *    down is pressed, a lifted one keeps its place and is pressed only
 *    as the mouse says; a place with none reads the mouse;
 * 5. the lightgun's aim reports off-screen where the pointer holds to
 *    the edge;
 * 6. one viewport fetch a poll, one translation a device and kind;
 * 7. a poll in which nothing is published: the driver is to be asked. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include <libretro.h>

#include "input/input_driver.h"
#include "gfx/video_driver.h"

static unsigned failures, vp_asked, translations;
#define CHECK(cond, msg) do { if (!(cond)) { failures++; \
   printf("FAIL %s\n", msg); } } while (0)

/* A 200x100 viewport at 0,0 of a 200x100 screen. In it a position is
 * itself; outside, held to the edge or -0x8000 as asked. The screen
 * position is the position plus 1000, to tell the two apart. */
bool video_driver_get_viewport_info(struct video_viewport *vp)
{
   vp_asked++;
   vp->pos       = VIDEO_POS_PACK(0, 0);
   vp->dims      = VIDEO_SCALE_PACK(200, 100);
   vp->full_dims = VIDEO_SCALE_PACK(200, 100);
   return true;
}

bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int x, int y, uint32_t *res_pos, uint32_t *res_screen_pos,
      bool report_oob)
{
   int vx = x, vy = y;
   translations++;
   (void)vp;
   if (x < 0 || x >= 200)
      vx = report_oob ? -0x8000 : (x < 0 ? 0 : 199);
   if (y < 0 || y >= 100)
      vy = report_oob ? -0x8000 : (y < 0 ? 0 : 99);
   *res_pos        = VIDEO_POS_PACK(vx, vy);
   *res_screen_pos = VIDEO_POS_PACK(x + 1000, y + 1000);
   return true;
}

bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{ return x == -0x8000 || y == -0x8000; }

#include "input/input_pointer_frame.h"

static int16_t rd(unsigned port, unsigned device, unsigned idx, unsigned id)
{
   int16_t v = -1;
   if (!input_pointer_frame_read(port, device, idx, id, &v))
      return -2;   /* the driver is to be asked */
   return v;
}

int main(void)
{
   input_pointer_frame_t f[2];
   uint32_t touch[3];
   uint32_t pos = 0;

   memset(f, 0, sizeof(f));
   f[0].pos     = VIDEO_POS_PACK(40, 30);
   f[0].rel     = VIDEO_POS_PACK(-3, 7);
   f[0].buttons = INPUT_POINTER_RIGHT | INPUT_POINTER_WHEEL_DOWN;
   f[1].pos     = VIDEO_POS_PACK(5, 6);
   f[1].buttons = INPUT_POINTER_LEFT;

   /* 7 (before anything is published) */
   input_pointer_frames_clear();
   CHECK(rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == -2
      && rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == -2
      && !input_pointer_frame_pos(0, RETRO_DEVICE_MOUSE, 0, &pos),
         "nothing published: the driver is to be asked");

   /* 1 */
   input_pointer_frames_set(f, 2, 3);
   CHECK(rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == -3
      && rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y) == 7,
         "the mouse's motion");
   CHECK(rd(0, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == 40
      && rd(0, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_Y) == 30,
         "the mouse's place in the window");
   CHECK(input_pointer_frame_pos(0, RARCH_DEVICE_MOUSE_SCREEN, 0, &pos)
      && pos == VIDEO_POS_PACK(40, 30)
      && input_pointer_frame_pos(0, RETRO_DEVICE_MOUSE, 0, &pos)
      && pos == VIDEO_POS_PACK(-3, 7),
         "the packed position is the published word");
   CHECK( rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) == 1
      &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 0
      &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE) == 0
      &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_4) == 0,
         "the mouse's buttons");
   CHECK( rd(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 1
      &&  rd(1, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == 5,
         "the second port reads the second device");
   CHECK( rd(2, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 0
      &&  rd(2, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) == 0,
         "a port with no device reads nothing, and not the driver");
   CHECK( rd(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) == -2
      &&  rd(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_a) == -2
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) == -2,
         "pads, keys and the lightgun's buttons are still the driver's");

   /* 2 */
   CHECK( rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN) == 1
      &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN) == 1
      &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN) == 1
      &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP) == 0,
         "a wheel notch reads the same every time in its frame");

   /* 3: a mouse for three touches - any button; right or middle; middle */
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 40
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) == 30
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_X) == 1040,
         "the pointer is where the translation puts the mouse");
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 3, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(1, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0,
         "a mouse standing for three touches");
   /* ...and for one: the left button */
   input_pointer_frames_set(f, 2, 1);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(1, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(1, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_X) == 0,
         "a mouse standing for one touch");

   /* 6 */
   input_pointer_frames_set(f, 2, 3);
   vp_asked = translations = 0;
   rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
   rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
   rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
   rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED);
   rd(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_PRESSED);
   rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN);
   rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_X);
   input_pointer_frame_pos(0, RETRO_DEVICE_POINTER, 0, &pos);
   CHECK(vp_asked == 1 && translations == 1,
         "eight pointer reads: one viewport fetch, one translation");
   rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
   rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);
   rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN);
   CHECK(vp_asked == 1 && translations == 2,
         "the lightgun's aim after them: no fetch, one translation more");
   rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
   CHECK(vp_asked == 1 && translations == 3,
         "a second device: no fetch, one translation more");
   rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
   rd(0, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X);
   CHECK(vp_asked == 1 && translations == 3, "the mouse needs neither");
   input_pointer_frames_set(f, 2, 3);
   rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
   CHECK(vp_asked == 2 && translations == 4, "the next poll places it afresh");

   /* 5: out of the viewport the pointer holds to the edge, the gun says so */
   f[0].pos = VIDEO_POS_PACK(250, 30);
   input_pointer_frames_set(f, 2, 3);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 199
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 0
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == -0x8000
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y) == 30
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 1
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_X) == -3,
         "out of the viewport: the pointer holds to the edge, the lightgun is off-screen");
   f[0].pos = VIDEO_POS_PACK(40, 30);

   /* 4: touch 0 down at 60,70; touch 1 lifted, last at 80,90; no touch 2 */
   touch[0] = VIDEO_POS_PACK(60, 70);
   touch[1] = VIDEO_POS_PACK(80, 90);
   touch[2] = 0;
   f[0].buttons = INPUT_POINTER_MIDDLE;
   input_pointer_frames_set(f, 1, 3);
   input_pointer_touches_set(touch, 3, 1 << 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 60
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) == 70
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_Y) == 1070
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1,
         "a touch that is down: its place, pressed");
   CHECK( rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_X) == 80
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 1,
         "a lifted touch keeps its place, and is pressed as the mouse says");
   CHECK( rd(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_X) == 40
      &&  rd(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_PRESSED) == 1,
         "a place with no touch reads the mouse");
   CHECK(input_pointer_frame_pos(3, RETRO_DEVICE_POINTER, 0, &pos)
      && pos == VIDEO_POS_PACK(60, 70)
      && input_pointer_frame_pos(0, RARCH_DEVICE_POINTER_SCREEN, 1, &pos)
      && pos == VIDEO_POS_PACK(1080, 1090),
         "the packed pointer position, and one mouse is every port's");
   f[0].buttons = 0;
   input_pointer_frames_set(f, 1, 3);
   input_pointer_touches_set(touch, 3, 1 << 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1,
         "with no button held only the touch that is down is pressed");

   /* 7 */
   input_pointer_frames_clear();
   CHECK(rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) == -2
      && rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == -2,
         "a poll in which nothing is published: the driver is to be asked");

   if (failures)
   {
      printf("FAIL pointer_frame_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS pointer_frame_test\n");
   return 0;
}
