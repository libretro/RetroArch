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
 * 7. a poll in which nothing is published: the driver is to be asked;
 * 8. ports reading by Mouse Index, a port with no mouse still reading a
 *    real touch, and the kind of device each mouse was last read as;
 * 9. only the mouse by index with the pointer and the lightgun every
 *    port's, and the lightgun aiming at the first touch;
 * 10. how many pointers are down;
 * 11. the pointer placed off-screen as the lightgun is;
 * 12. a touch read alone: a pen in the air;
 * 13. the poll's viewport, asked for once a poll;
 * 14. a touch's place, its presence and whether it is down are three
 *     things: a touch at 0,0, one lifted there, a place with none,
 *     the viewport's edges, sparse places, the lightgun;
 * 15. devices and touches in a screen's own window: placed through
 *     that window, nowhere on the main window's screen, off-screen
 *     once the window is gone; motion, buttons and ports as on the
 *     main window;
 * 16. the flags that keep a driver's own answers (Android's), each off
 *     and on: pressed only inside, the second list, the mouse and
 *     the ids left to the driver, touch slots; and a touch read over
 *     its own window's client area;
 * 17. Android's publication against the driver's own answers before
 *     it published (989ba0b1df), finger by finger;
 * 18. the driver's own lightgun buttons: a tap on a main window that
 *     shows no screen counts no finger, one in the letterbox does. */
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
 * position is the position plus 1000, to tell the two apart, and
 * -0x8000 left of or above -100, off the screen. */
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
   *res_screen_pos = VIDEO_POS_PACK(x < -100 ? -0x8000 : x + 1000,
         y < -100 ? -0x8000 : y + 1000);
   return true;
}

/* Window 1, generation 5, has a 100x100 layout: a place in it is
 * itself plus 2000, held to its edges or -0x8000 outside as asked.
 * Any other window or generation has none. */
static unsigned window_translations;
bool video_driver_translate_window_point(unsigned window,
      unsigned generation, int x, int y, bool report_oob,
      uint32_t *result)
{
   window_translations++;
   if (window != 1 || generation != 5)
      return false;
   if (report_oob && (x < 0 || x >= 100 || y < 0 || y >= 100))
   {
      *result = VIDEO_POS_PACK(-0x8000, -0x8000);
      return true;
   }
   x       = x < 0 ? 0 : (x > 99 ? 99 : x);
   y       = y < 0 ? 0 : (y > 99 ? 99 : y);
   *result = VIDEO_POS_PACK(x + 2000, y + 2000);
   return true;
}

/* Window shown_window has drawn, shown_dims big, on display 4 */
static unsigned shown_window, shown_dims;
unsigned video_driver_window_shown(unsigned window, unsigned *dims)
{
   if (!window || window != shown_window)
      return 0;
   if (dims)
      *dims = shown_dims;
   return 4;
}

bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{ return x == -0x8000 || y == -0x8000; }

/* Port 0's Mouse Index is 1, port 1's is 0, port 2's is a mouse that is
 * not there. */
unsigned input_mouse_port_index(unsigned port)
{ return port == 0 ? 1 : (port == 1 ? 0 : 7); }

#include "input/input_pointer_frame.h"

/* @count places, each with a contact; @down says which are pressed. */
static void touches_set(const uint32_t *pos, unsigned count, unsigned down)
{
   input_pointer_touches_set(pos, NULL, count, (1u << count) - 1, down);
}

static int16_t rd(unsigned port, unsigned device, unsigned idx, unsigned id)
{
   int16_t v = -1;
   if (!input_pointer_frame_read(port, device, idx, id, &v))
      return -2;   /* the driver is to be asked */
   return v;
}

/* 16 */
static void test_flags(void)
{
   input_pointer_frame_t f;
   input_video_window_t tw[3];
   uint32_t touch[3];
   uint32_t pos = 0;
   bool down    = false;

   input_pointer_frames_clear();
   memset(&f, 0, sizeof(f));
   memset(tw, 0, sizeof(tw));
   f.pos     = VIDEO_POS_PACK(10, 10);
   f.buttons = INPUT_POINTER_LEFT;

   /* PRESSED_INSIDE: right of the viewport, left of the screen, in */
   touch[0] = VIDEO_POS_PACK(250, 40);
   touch[1] = VIDEO_POS_PACK(-150, 40);
   touch[2] = VIDEO_POS_PACK(30, 20);
   input_pointer_frames_set(&f, NULL, 1, 0);
   touches_set(touch, 3, 7);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 1,
         "without PRESSED_INSIDE a touch that is down is pressed anywhere");
   input_pointer_frames_set(&f, NULL, 1, INPUT_POINTERS_PRESSED_INSIDE);
   touches_set(touch, 3, 7);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 199
      &&  rd(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 3,
         "with it, pressed only inside the viewport, or the screen read as SCREEN, and still counted");

   /* POINTER_IDS_BY_DRIVER */
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RARCH_DEVICE_ID_POINTER_BACK) == 0
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RARCH_DEVICE_ID_POINTER_BACK) == 0,
         "without POINTER_IDS_BY_DRIVER an id the frontend does not hold reads 0");
   input_pointer_frames_set(&f, NULL, 1, INPUT_POINTERS_POINTER_IDS_BY_DRIVER);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RARCH_DEVICE_ID_POINTER_BACK) == -2
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RARCH_DEVICE_ID_POINTER_BACK) == -2
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 199,
         "with it the driver answers that id, and the frontend the rest");

   /* WINDOWS_SECOND: touches 0 and 2 in window 1, 1 in the main one */
   tw[0].window     = 1;
   tw[0].generation = 5;
   tw[2]            = tw[0];
   touch[0] = VIDEO_POS_PACK(10, 20);
   touch[1] = VIDEO_POS_PACK(50, 60);
   touch[2] = VIDEO_POS_PACK(30, 40);
   input_pointer_frames_set(&f, NULL, 1, 0);
   input_pointer_touches_set(touch, tw, 3, 7, 7);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 2010
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 3
      &&  rd(0, RARCH_DEVICE_POINTER_SECOND, 0, RETRO_DEVICE_ID_POINTER_X) == -2,
         "without WINDOWS_SECOND every touch is the pointer's, and there is no second list");
   input_pointer_frames_set(&f, NULL, 1,
         INPUT_POINTERS_WINDOWS_SECOND | INPUT_POINTERS_GUN_AT_TOUCH);
   input_pointer_touches_set(touch, tw, 3, 7, 7);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 50
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_X) == 1050
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 50
      &&  input_pointer_frame_pos(0, RETRO_DEVICE_POINTER, 0, &pos)
      &&  pos == VIDEO_POS_PACK(50, 60),
         "with it the pointer and the lightgun have the main window's touches");
   CHECK( rd(0, RARCH_DEVICE_POINTER_SECOND, 0, RETRO_DEVICE_ID_POINTER_X) == 2010
      &&  rd(0, RARCH_DEVICE_POINTER_SECOND, 1, RETRO_DEVICE_ID_POINTER_X) == 2030
      &&  rd(0, RARCH_DEVICE_POINTER_SECOND, 1, RETRO_DEVICE_ID_POINTER_Y) == 2040
      &&  rd(0, RARCH_DEVICE_POINTER_SECOND, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RARCH_DEVICE_POINTER_SECOND, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 2
      &&  rd(0, RARCH_DEVICE_POINTER_SECOND, 2, RETRO_DEVICE_ID_POINTER_X) == 0
      &&  rd(0, RARCH_DEVICE_POINTER_SECOND, 2, RETRO_DEVICE_ID_POINTER_PRESSED) == 0,
         "and the others are a second list, in the order published");
   input_pointer_touches_set(touch, tw, 0, 0, 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RARCH_DEVICE_POINTER_SECOND, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RARCH_DEVICE_POINTER_SECOND, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 0,
         "no mouse stands for a touch of the second list");

   /* MOUSE_BY_DRIVER */
   f.rel     = VIDEO_POS_PACK(3, 4);
   f.buttons = INPUT_POINTER_RIGHT | INPUT_POINTER_WHEEL_DOWN;
   input_pointer_frames_set(&f, NULL, 1, 0);
   CHECK( rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == 3
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_X) == 3,
         "without MOUSE_BY_DRIVER the frontend answers the mouse");
   input_pointer_frames_set(&f, NULL, 1, INPUT_POINTERS_MOUSE_BY_DRIVER);
   CHECK( rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == -2
      &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) == -2
      &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN) == -2
      &&  rd(0, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == -2
      &&  rd(0, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP) == -2
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_X) == -2
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_Y) == -2
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == -2
      &&  !input_pointer_frame_pos(0, RETRO_DEVICE_MOUSE, 0, &pos)
      &&  !input_pointer_frame_pos(0, RARCH_DEVICE_MOUSE_SCREEN, 0, &pos),
         "with it the mouse, the lightgun's motion and its mouse aim are the driver's");
   touch[0] = VIDEO_POS_PACK(60, 70);
   input_pointer_frames_set(&f, NULL, 1,
         INPUT_POINTERS_MOUSE_BY_DRIVER | INPUT_POINTERS_GUN_AT_TOUCH);
   touches_set(touch, 1, 1);
   CHECK( rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 60
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 0
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_X) == -2
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 60,
         "a lightgun aimed at a touch is the frontend's, its motion still the driver's");

   /* TOUCH_SLOTS: slot 0 never placed, 1 lifted off the viewport, 2 down */
   touch[0]  = VIDEO_POS_PACK(60, 70);
   touch[1]  = VIDEO_POS_PACK(250, 40);
   touch[2]  = VIDEO_POS_PACK(30, 40);
   f.buttons = INPUT_POINTER_LEFT;
   input_pointer_frames_set(&f, NULL, 1, INPUT_POINTERS_GUN_AT_TOUCH);
   input_pointer_touches_set(touch, NULL, 3, 6, 4);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 10
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 1, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 0
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 2, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 10,
         "without TOUCH_SLOTS the mouse stands for an index with no touch");
   input_pointer_frames_set(&f, NULL, 1,
         INPUT_POINTERS_GUN_AT_TOUCH | INPUT_POINTERS_TOUCH_SLOTS);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_X) == 199
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 1, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_X) == 30
      &&  rd(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_PRESSED) == 1,
         "with it an index is a slot: none reads 0, a lifted one keeps its place");
   CHECK( rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 0
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 1, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == -0x8000
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 1, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 1
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 2, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y) == 40
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 3, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 0,
         "and the lightgun aims at the touch at its index, never the mouse");
   input_pointer_frames_set(&f, NULL, 1, INPUT_POINTERS_TOUCH_SLOTS);
   CHECK( rd(0, RETRO_DEVICE_LIGHTGUN, 1, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 1
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 2, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 0
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 0,
         "its IS_OFFSCREEN is that touch's while the mouse aims it");

   /* A touch over its own window's whole client area */
   memset(tw, 0, sizeof(tw));
   tw[0].window     = 1;
   tw[0].generation = 5;
   tw[2]            = tw[0];
   touch[0]         = VIDEO_POS_PACK(0, 50);
   touch[1]         = VIDEO_POS_PACK(5, 5);
   touch[2]         = VIDEO_POS_PACK(100, 25);
   shown_window     = 1;
   shown_dims       = VIDEO_SCALE_PACK(101, 51);
   input_pointer_touches_set(touch, tw, 3, 7, 5);
   CHECK( input_pointer_window_touch(1, 0, &pos, &down)
      &&  pos == VIDEO_POS_PACK(-0x7fff, 0x7fff) && down
      &&  input_pointer_window_touch(1, 1, &pos, &down)
      &&  pos == VIDEO_POS_PACK(0x7fff, -1) && down
      &&  !input_pointer_window_touch(1, 2, &pos, &down)
      &&  !input_pointer_window_touch(2, 0, &pos, &down),
         "a window's touches over its whole client area, in order; none past them or elsewhere");
   touch[2] = VIDEO_POS_PACK(150, -3);
   input_pointer_touches_set(touch, tw, 3, 7, 1);
   CHECK( input_pointer_window_touch(1, 1, &pos, &down)
      &&  pos == VIDEO_POS_PACK(-0x8000, -0x8000) && !down,
         "outside its window: -0x8000; a lifted touch is not down");
   shown_window = 0;
   CHECK(!input_pointer_window_touch(1, 0, &pos, &down),
         "a window that has not shown has no touches");
   input_pointer_frames_clear();
}

/* 17. Android's publication against the driver as it was. The publish
 * calls are input_driver.c's. */
#include "input/drivers/android_pointers.h"

void input_driver_publish_window_pointers(
      const input_pointer_frame_t *frames,
      const input_video_window_t *windows,
      unsigned count, unsigned flags)
{
   input_pointer_frames_set(frames, windows, count, flags);
}

void input_driver_publish_window_touches(const uint32_t *pos,
      const input_video_window_t *windows, unsigned count,
      unsigned present, unsigned down)
{
   input_pointer_touches_set(pos, windows, count, present, down);
}

/* Before the publication (989ba0b1df) the driver kept each touch placed
 * three ways, as its event came. The same fingers go to both. */
struct today_pointer
{
   uint32_t pos;
   uint32_t confined_pos;
   uint32_t full_pos;
};

static struct today_pointer today_main[16], today_second[16];
static unsigned today_count, today_second_count;
static struct input_pointer now_main[16], now_second[16];
static unsigned now_count, now_second_count;
static input_video_window_t now_window;
static bool mouse_used;

/* android_pointer_set(), before the publication and with it */
static void finger_place(unsigned i, int x, int y)
{
   struct video_viewport vp;
   memset(&vp, 0, sizeof(vp));
   if (video_driver_get_viewport_info(&vp))
   {
      video_driver_translate_coord_viewport(&vp, x, y,
            &today_main[i].confined_pos, &today_main[i].full_pos, false);
      video_driver_translate_coord_viewport(&vp, x, y,
            &today_main[i].pos, &today_main[i].full_pos, true);
   }
   now_main[i].pos    = VIDEO_POS_PACK(x, y);
   now_main[i].placed = true;
}

/* A touch event with @n fingers at @xy: each placed, as the driver's
 * motion handler does */
static void fingers_move(unsigned n, const int *xy)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      finger_place(i, xy[2 * i], xy[2 * i + 1]);
      if (today_count < i + 1)
         today_count = i + 1;
      if (now_count < i + 1)
         now_count   = i + 1;
   }
}

/* Finger @i up: the slots above shift down */
static void finger_up(unsigned i)
{
   memmove(&today_main[i], &today_main[i + 1],
         (16 - i - 1) * sizeof(today_main[0]));
   memmove(&now_main[i], &now_main[i + 1],
         (16 - i - 1) * sizeof(now_main[0]));
   if (today_count)
      today_count--;
   if (now_count)
      now_count--;
}

/* The second display's finger @i, in down order, at @x, @y of window
 * 1, generation 5; a new one when @i is the count.
 * android_input_poll_second_touches(), then and now. */
static void second_place(unsigned i, int x, int y)
{
   if (!video_driver_translate_window_point(1, 5, x, y, false,
            &today_second[i].confined_pos))
      today_second[i].confined_pos = VIDEO_POS_PACK(-0x8000, -0x8000);
   if (!video_driver_translate_window_point(1, 5, x, y, true,
            &today_second[i].pos))
      today_second[i].pos = VIDEO_POS_PACK(-0x8000, -0x8000);
   today_second[i].full_pos = VIDEO_POS_PACK(-0x8000, -0x8000);
   now_second[i].pos        = VIDEO_POS_PACK(x, y);
   now_second[i].placed     = true;
   now_window.window        = 1;
   now_window.generation    = 5;
   if (i == today_second_count)
   {
      today_second_count++;
      now_second_count++;
   }
}

static void second_up(unsigned i)
{
   memmove(&today_second[i], &today_second[i + 1],
         (today_second_count - i - 1) * sizeof(today_second[0]));
   memmove(&now_second[i], &now_second[i + 1],
         (now_second_count - i - 1) * sizeof(now_second[0]));
   today_second_count--;
   now_second_count--;
}

/* android_input_state() before the publication (989ba0b1df), for what
 * the publication answers: the pointer, the screen pointer, the second
 * display's and a touch's lightgun aim */
static int16_t today_state(unsigned device, unsigned idx, unsigned id)
{
   const struct today_pointer *p = (idx < 16) ? &today_main[idx] : NULL;
   switch (device)
   {
      case RETRO_DEVICE_LIGHTGUN:
         if (!p)
            return 0;
         if (id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X)
            return VIDEO_POS_X(p->pos);
         if (id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y)
            return VIDEO_POS_Y(p->pos);
         return input_driver_pointer_is_offscreen(
               VIDEO_POS_X(p->pos), VIDEO_POS_Y(p->pos));
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         if (id == RETRO_DEVICE_ID_POINTER_COUNT)
            return (int16_t)today_count;
         if (!p)
            return 0;
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return VIDEO_POS_X((device == RARCH_DEVICE_POINTER_SCREEN)
                     ? p->full_pos : p->confined_pos);
            case RETRO_DEVICE_ID_POINTER_Y:
               return VIDEO_POS_Y((device == RARCH_DEVICE_POINTER_SCREEN)
                     ? p->full_pos : p->confined_pos);
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               {
                  uint32_t at = (device == RARCH_DEVICE_POINTER_SCREEN)
                     ? p->full_pos : p->pos;
                  return idx < today_count
                     && VIDEO_POS_X(at) != -0x8000
                     && VIDEO_POS_Y(at) != -0x8000;
               }
            case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
               return input_driver_pointer_is_offscreen(
                     VIDEO_POS_X(p->pos), VIDEO_POS_Y(p->pos));
            default:
               break;
         }
         return 0;
      case RARCH_DEVICE_POINTER_SECOND:
         if (id == RETRO_DEVICE_ID_POINTER_COUNT)
            return (int16_t)today_second_count;
         if (idx >= today_second_count)
            return 0;
         p = &today_second[idx];
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return VIDEO_POS_X(p->confined_pos);
            case RETRO_DEVICE_ID_POINTER_Y:
               return VIDEO_POS_Y(p->confined_pos);
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return VIDEO_POS_X(p->pos) != -0x8000
                  && VIDEO_POS_Y(p->pos) != -0x8000;
            case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
               return input_driver_pointer_is_offscreen(
                     VIDEO_POS_X(p->pos), VIDEO_POS_Y(p->pos));
            default:
               break;
         }
         return 0;
      default:
         break;
   }
   return 0;
}

/* Publishes the fingers as android_input_publish() does, then reads,
 * at two ports, every answer the publication gives Android and
 * compares it with the driver's; the number that differ. */
static unsigned android_compare(const char *what)
{
   static const unsigned devices[3] = { RETRO_DEVICE_POINTER,
      RARCH_DEVICE_POINTER_SCREEN, RARCH_DEVICE_POINTER_SECOND };
   static const unsigned ids[5]     = { RETRO_DEVICE_ID_POINTER_X,
      RETRO_DEVICE_ID_POINTER_Y, RETRO_DEVICE_ID_POINTER_PRESSED,
      RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN, RETRO_DEVICE_ID_POINTER_COUNT };
   static const unsigned gun[3]     = { RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X,
      RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y,
      RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN };
   android_pointers_t p;
   unsigned port, d, idx, i;
   unsigned bad = 0;

   memset(&p, 0, sizeof(p));
   p.main_slots    = now_main;
   p.slots         = 16;
   p.main_count    = now_count;
   p.second        = now_second;
   p.second_count  = now_second_count;
   p.second_window = now_window;
   p.mouse_pos     = VIDEO_POS_PACK(5, 5);
   p.mouse_buttons = INPUT_POINTER_LEFT;
   p.mouse_used    = mouse_used;
   input_pointer_frames_clear();
   android_pointers_publish(&p);

   for (port = 0; port < 2; port++)
   {
      for (d = 0; d < 3; d++)
         for (idx = 0; idx < 18; idx++)
            for (i = 0; i < 5; i++)
            {
               int16_t got  = rd(port, devices[d], idx, ids[i]);
               int16_t want = today_state(devices[d], idx, ids[i]);
               if (got != want && !bad++)
                  printf("  %s: device %x idx %u id %u reads %d, was %d\n",
                        what, devices[d], idx, ids[i], got, want);
            }
      for (idx = 0; idx < 18; idx++)
         for (i = 0; i < 3; i++)
         {
            int16_t got  = rd(port, RETRO_DEVICE_LIGHTGUN, idx, gun[i]);
            int16_t want = (mouse_used
                  && gun[i] != RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN)
               ? -2 : today_state(RETRO_DEVICE_LIGHTGUN, idx, gun[i]);
            if (got != want && !bad++)
               printf("  %s: lightgun idx %u id %u reads %d, was %d\n",
                     what, idx, gun[i], got, want);
         }
      /* the menu's pointer, read as a place */
      for (idx = 0; idx < 18; idx++)
      {
         uint32_t pos  = 1;
         uint32_t want = VIDEO_POS_PACK(
               today_state(RETRO_DEVICE_POINTER, idx,
                  RETRO_DEVICE_ID_POINTER_X),
               today_state(RETRO_DEVICE_POINTER, idx,
                  RETRO_DEVICE_ID_POINTER_Y));
         if (     (  !input_pointer_frame_pos(port, RETRO_DEVICE_POINTER,
                        idx, &pos)
                  || pos != want)
               && !bad++)
            printf("  %s: pointer %u placed at %08x, was %08x\n",
                  what, idx, (unsigned)pos, (unsigned)want);
      }
      /* the overlay's fingers, each read at one look */
      for (d = 0; d < 2; d++)
         for (idx = 0; idx < 18; idx++)
         {
            uint32_t pos  = 1;
            int down      = input_pointer_frame_finger(port, devices[d],
                  idx, &pos);
            int16_t want  = today_state(devices[d], idx,
                  RETRO_DEVICE_ID_POINTER_PRESSED);
            uint32_t at   = VIDEO_POS_PACK(
                  today_state(devices[d], idx, RETRO_DEVICE_ID_POINTER_X),
                  today_state(devices[d], idx, RETRO_DEVICE_ID_POINTER_Y));
            if (     (down != want || (want && pos != at))
                  && !bad++)
               printf("  %s: device %x finger %u reads %d at %08x, was %d"
                     " at %08x\n", what, devices[d], idx, down,
                     (unsigned)pos, want, (unsigned)at);
         }
   }
   return bad;
}

/* 17 */
static void test_android_answers(void)
{
   static const int one[2]   = { 40, 30 };
   static const int three[6] = { 10, 10, 250, 40, 60, 90 };
   int many[32];
   unsigned i;
   uint32_t pos = 0;

   memset(today_main, 0, sizeof(today_main));
   memset(today_second, 0, sizeof(today_second));
   memset(now_main, 0, sizeof(now_main));
   memset(now_second, 0, sizeof(now_second));
   memset(&now_window, 0, sizeof(now_window));
   today_count = today_second_count = 0;
   now_count   = now_second_count   = 0;
   mouse_used  = false;

   CHECK(!android_compare("no touch yet"),
         "Android, no touch yet: every answer as before");
   fingers_move(1, one);
   CHECK(!android_compare("one finger"),
         "Android, one finger: every answer as before");
   finger_up(0);
   CHECK(!android_compare("lifted"),
         "Android, the finger up: its index reads what its slot holds, as before");
   finger_place(0, 30, 40);
   CHECK(!android_compare("pen hovering"),
         "Android, a pen hovering: placed, not down, as before");
   fingers_move(3, three);
   CHECK(!android_compare("three fingers"),
         "Android, three fingers, one off the viewport: every answer as before");
   finger_up(1);
   CHECK(!android_compare("the middle one up"),
         "Android, the middle finger up: the slots above shift down, as before");
   finger_place(0, -150, 120);
   CHECK(!android_compare("the pen off the screen"),
         "Android, the pen off the screen: every answer as before");
   second_place(0, 20, 30);
   second_place(1, 150, 40);
   CHECK(!android_compare("two on the second display"),
         "Android, two fingers on the second display, one off its screen: every answer as before");
   second_up(0);
   CHECK(!android_compare("one up on the second display"),
         "Android, one up on the second display: every answer as before");
   for (i = 0; i < 16; i++)
   {
      many[2 * i]     = 12 * i;
      many[2 * i + 1] = 6 * i;
   }
   fingers_move(16, many);
   for (i = 0; i < 16; i++)
      second_place(i, 6 * i, 6 * i);
   CHECK(!android_compare("sixteen on each display"),
         "Android, sixteen fingers on each display: none dropped, every answer as before");
   finger_up(3);
   CHECK(!android_compare("one of sixteen up"),
         "Android, one of sixteen up: the top slot keeps its place, as before");
   mouse_used = true;
   CHECK(!android_compare("a mouse in use"),
         "Android, a mouse in use: its lightgun aim is the driver's, the rest as before");

   /* What the driver keeps, with today's code: every read reaches it */
   {
      bool driver = true;
      for (i = RETRO_DEVICE_ID_MOUSE_X; i <= RETRO_DEVICE_ID_MOUSE_BUTTON_5; i++)
         if (     rd(0, RETRO_DEVICE_MOUSE, 0, i) != -2
               || rd(1, RARCH_DEVICE_MOUSE_SCREEN, 0, i) != -2)
            driver = false;
      CHECK( driver
         &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_X) == -2
         &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_Y) == -2
         &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) == -2
         &&  rd(0, RETRO_DEVICE_POINTER, 0, RARCH_DEVICE_ID_POINTER_BACK) == -2
         &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RARCH_DEVICE_ID_POINTER_BACK) == -2
         &&  !input_pointer_frame_pos(0, RARCH_DEVICE_MOUSE_SCREEN, 0, &pos),
            "Android: the mouse, the lightgun's motion and buttons, and BACK are the driver's");
   }
   CHECK( input_pointers.count == 1
      &&  input_pointers.frame[0].buttons == INPUT_POINTER_LEFT
      &&  input_pointers.frame[0].rel == 0,
         "Android's mouse is published, for binds to its buttons");
   input_pointer_frames_clear();
}

/* 18 */
static void test_android_gun_fingers(void)
{
   video_views_layout_t layout;
   memset(&layout, 0, sizeof(layout));
   CHECK( android_pointers_gun_fingers(1, NULL) == 1
      &&  android_pointers_gun_fingers(3, NULL) == 3,
         "Android, no views: every finger counts for the lightgun's buttons");
   layout.num_placements = 1;
   CHECK( android_pointers_gun_fingers(1, &layout) == 1
      &&  android_pointers_gun_fingers(2, &layout) == 2,
         "Android, a main window that shows a screen: a tap counts, in its letterbox too");
   layout.num_placements = 0;
   CHECK( android_pointers_gun_fingers(1, &layout) == 0
      &&  android_pointers_gun_fingers(3, &layout) == 0,
         "Android, a main window that shows no screen: a tap there is on nothing");
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
   input_pointer_frames_set(f, NULL, 2, INPUT_POINTERS_MOUSE_3_TOUCHES);
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
   input_pointer_frames_set(f, NULL, 2, 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(1, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(1, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_X) == 0,
         "a mouse standing for one touch");

   /* 6 */
   input_pointer_frames_set(f, NULL, 2, INPUT_POINTERS_MOUSE_3_TOUCHES);
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
   input_pointer_frames_set(f, NULL, 2, INPUT_POINTERS_MOUSE_3_TOUCHES);
   rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
   CHECK(vp_asked == 2 && translations == 4, "the next poll places it afresh");

   /* 5: out of the viewport the pointer holds to the edge, the gun says so */
   f[0].pos = VIDEO_POS_PACK(250, 30);
   input_pointer_frames_set(f, NULL, 2, INPUT_POINTERS_MOUSE_3_TOUCHES);
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
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_MOUSE_3_TOUCHES);
   /* places 0 and 1 hold a contact; place 2 holds none */
   input_pointer_touches_set(touch, NULL, 3, 3, 1 << 0);
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
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_MOUSE_3_TOUCHES);
   /* places 0 and 1 hold a contact; place 2 holds none */
   input_pointer_touches_set(touch, NULL, 3, 3, 1 << 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1,
         "with no button held only the touch that is down is pressed");

   /* 8: by Mouse Index - port 0 reads device 1, port 1 device 0 */
   f[0].buttons = INPUT_POINTER_RIGHT;
   f[1].buttons = INPUT_POINTER_LEFT;
   input_pointer_frames_set(f, NULL, 2, INPUT_POINTERS_BY_MOUSE_INDEX);
   touches_set(touch, 1, 1 << 0);
   CHECK( rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 1
      &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) == 0
      &&  rd(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) == 1
      &&  rd(1, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == 40,
         "by Mouse Index: a port reads the device at its index");
   CHECK( rd(2, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 0
      &&  rd(2, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == 0
      &&  rd(2, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 0,
         "by Mouse Index: a port whose mouse is not there reads nothing");
   CHECK( rd(2, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 60
      &&  rd(2, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(2, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  input_pointer_frame_pos(2, RETRO_DEVICE_POINTER, 0, &pos)
      &&  pos == VIDEO_POS_PACK(60, 70),
         "a port with no mouse still reads a real touch, and only that");
   touches_set(touch, 0, 0);
   CHECK( rd(2, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(2, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 0,
         "with no mouse and no touch a pointer reads nothing");
   /* the kind each device was last read as: port 0 is device 1 */
   rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
   rd(1, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_X);
   CHECK(input_pointers.read_as[1] == RETRO_DEVICE_LIGHTGUN
      && input_pointers.read_as[0] == RARCH_DEVICE_POINTER_SCREEN,
         "each device records the kind it was last read as");
   input_pointer_frame_pos(0, RETRO_DEVICE_MOUSE, 0, &pos);
   input_pointer_frames_set(f, NULL, 2, INPUT_POINTERS_BY_MOUSE_INDEX);
   CHECK(input_pointers.read_as[1] == RETRO_DEVICE_MOUSE
      && input_pointers.read_as[0] == RARCH_DEVICE_POINTER_SCREEN,
         "and keeps it over the next poll");

   /* 9: only the mouse by index; the pointer and the gun every port's.
    * Port 2's mouse is not there, port 1's index is 0. */
   f[0].pos     = VIDEO_POS_PACK(40, 30);
   f[0].buttons = INPUT_POINTER_LEFT;
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_BY_MOUSE_INDEX
         | INPUT_POINTERS_AIM_EVERY_PORT | INPUT_POINTERS_GUN_AT_TOUCH);
   touches_set(touch, 0, 0);
   CHECK( rd(2, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 0
      &&  rd(2, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == 0
      &&  rd(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 1,
         "aim every port's: the mouse still goes by the port's index");
   CHECK( rd(2, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 40
      &&  rd(2, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(2, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 40
      &&  rd(2, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y) == 30,
         "aim every port's: the pointer and the lightgun read the mouse on any port");
   /* the gun at the first touch while there is one; out of the viewport
    * it is off-screen */
   touch[0] = VIDEO_POS_PACK(60, 70);
   touches_set(touch, 1, 1 << 0);
   vp_asked = translations = 0;
   CHECK( rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 60
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y) == 70
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 0
      &&  input_pointer_frame_pos(0, RETRO_DEVICE_LIGHTGUN, 0, &pos)
      &&  pos == VIDEO_POS_PACK(60, 70)
      &&  vp_asked == 0 && translations == 1,
         "the lightgun aims at the first touch: one translation, the poll's viewport");
   touch[0] = VIDEO_POS_PACK(260, 70);
   touches_set(touch, 1, 1 << 0);
   CHECK( rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 199,
         "a touch out of the viewport: the gun is off-screen, the pointer at the edge");
   touches_set(touch, 0, 0);
   CHECK( rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 40,
         "with no touch the lightgun aims at the mouse");
   /* without the flag a touch does not move the gun */
   touch[0] = VIDEO_POS_PACK(60, 70);
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_MOUSE_3_TOUCHES);
   touches_set(touch, 1, 1 << 0);
   CHECK( rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 40,
         "a driver that does not say so: the lightgun stays on the mouse");

   /* 10: how many pointers are down - the touches that are, else the
    * mouse standing for one */
   f[0].pos     = VIDEO_POS_PACK(40, 30);
   f[0].buttons = INPUT_POINTER_LEFT;
   touch[0]     = VIDEO_POS_PACK(60, 70);
   touch[1]     = VIDEO_POS_PACK(80, 90);
   input_pointer_frames_set(f, NULL, 1, 0);
   touches_set(touch, 2, 3);
   CHECK(rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 2,
         "the count is the touches that are down");
   touches_set(touch, 0, 0);
   CHECK(rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 1,
         "with no touch the count is the mouse's, its button held");
   f[0].buttons = 0;
   input_pointer_frames_set(f, NULL, 1, 0);
   vp_asked = translations = 0;
   CHECK(rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 0
      && vp_asked == 0 && translations == 0,
         "nothing down counts none, and the count places nothing");

   /* 11: the pointer placed as the lightgun is - off-screen outside */
   f[0].pos = VIDEO_POS_PACK(250, 30);
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_POINTER_OFFSCREEN);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == -0x8000
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) == 30
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 1,
         "an off-screen pointer says so where the driver has it so");
   touch[0] = VIDEO_POS_PACK(60, 170);
   touches_set(touch, 1, 1);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 60
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) == -0x8000
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 1
      &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_Y) == 1170,
         "and so does a touch");
   f[0].pos = VIDEO_POS_PACK(40, 30);

   /* 12: a touch read alone - a pen in the air is not pressed by the
    * mouse's button, and counts none */
   f[0].buttons = INPUT_POINTER_LEFT;
   touch[0]     = VIDEO_POS_PACK(60, 70);
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_TOUCH_ALONE);
   touches_set(touch, 1, 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 60
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 0,
         "a touch read alone: lifted, the mouse's button does not press it");
   touches_set(touch, 1, 1);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 1,
         "a touch read alone: down, it is pressed");
   touches_set(touch, 0, 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 40
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 0,
         "a touch read alone: with none, the mouse stands for the first");
   /* without the flag a lifted touch is pressed as the mouse says */
   input_pointer_frames_set(f, NULL, 1, 0);
   touches_set(touch, 1, 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1,
         "a driver that does not say so: a lifted touch is pressed as the mouse says");

   /* 7 */
   input_pointer_frames_clear();
   CHECK(rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) == -2
      && rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == -2,
         "a poll in which nothing is published: the driver is to be asked");

   /* 14: where a touch is, whether it is there, and whether it is down
    * are three things. The mouse is at 40,30 with no button held. */
   f[0].pos     = VIDEO_POS_PACK(40, 30);
   f[0].buttons = 0;
   /* a touch down at the window's origin is a touch */
   touch[0] = VIDEO_POS_PACK(0, 0);
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_GUN_AT_TOUCH);
   input_pointer_touches_set(touch, NULL, 1, 1, 1);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 1
      &&  input_pointer_frame_pos(0, RETRO_DEVICE_POINTER, 0, &pos)
      &&  pos == VIDEO_POS_PACK(0, 0),
         "a touch down at 0,0 is pressed and at 0,0, not the mouse's");
   CHECK( rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 0
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y) == 0,
         "the lightgun aims at a touch at 0,0, not at the mouse");
   /* lifted at the origin, its place kept: there, not pressed */
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_GUN_AT_TOUCH);
   input_pointer_touches_set(touch, NULL, 1, 1, 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 0
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 0,
         "a touch lifted at 0,0 keeps its place and is not pressed");
   /* not there at all: the mouse answers, whatever the place holds */
   touch[0] = VIDEO_POS_PACK(60, 70);
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_GUN_AT_TOUCH);
   input_pointer_touches_set(touch, NULL, 1, 0, 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 40
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 40,
         "a place with no contact reads the mouse, whatever position it holds");
   /* down said without there: not a touch */
   input_pointer_frames_set(f, NULL, 1, 0);
   input_pointer_touches_set(touch, NULL, 1, 0, 1);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 0,
         "a place that is down but not there is not a touch");
   /* at the viewport's edges: its first and last pixel, both down */
   touch[0] = VIDEO_POS_PACK(0, 99);
   touch[1] = VIDEO_POS_PACK(199, 0);
   input_pointer_frames_set(f, NULL, 1, 0);
   input_pointer_touches_set(touch, NULL, 2, 3, 3);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) == 99
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_X) == 199
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_Y) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 2,
         "touches on the viewport's edges, an axis at 0 each");
   /* sparse places: 0 empty, 1 down at the origin, 2 empty. The mouse
    * stands for three touches and its middle button is held. */
   touch[0]     = VIDEO_POS_PACK(0, 0);
   touch[1]     = VIDEO_POS_PACK(0, 0);
   touch[2]     = VIDEO_POS_PACK(0, 0);
   f[0].buttons = INPUT_POINTER_MIDDLE;
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_MOUSE_3_TOUCHES);
   input_pointer_touches_set(touch, NULL, 3, 1 << 1, 1 << 1);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 40
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_X) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_X) == 40
      &&  rd(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 1,
         "sparse places: the one with a contact reads it, the others the mouse");
   /* the lightgun goes by the first place only */
   f[0].buttons = 0;
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_GUN_AT_TOUCH);
   input_pointer_touches_set(touch, NULL, 3, 1 << 1, 1 << 1);
   CHECK( rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 40,
         "the lightgun aims at the first place's touch, and with none there at the mouse");
   /* a pen in the air at the origin, read alone */
   f[0].buttons = INPUT_POINTER_LEFT;
   input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_TOUCH_ALONE);
   input_pointer_touches_set(touch, NULL, 1, 1, 0);
   CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 0
      &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_COUNT) == 0,
         "a pen in the air at 0,0 is there, at 0,0, and not pressed by the mouse");
   f[0].buttons = 0;

   /* 15: device 1 and touch 0 in window 1; device 0 and touch 1 in
    * the main window */
   {
      input_video_window_t w[2];
      memset(w, 0, sizeof(w));
      w[1].window     = 1;
      w[1].generation = 5;
      f[0].pos     = VIDEO_POS_PACK(40, 30);
      f[0].rel     = VIDEO_POS_PACK(-3, 7);
      f[0].buttons = 0;
      f[1].pos     = VIDEO_POS_PACK(10, 20);
      f[1].rel     = VIDEO_POS_PACK(4, -2);
      f[1].buttons = INPUT_POINTER_LEFT;
      input_pointer_frames_set(f, w, 2, INPUT_POINTERS_MOUSE_3_TOUCHES);
      touches_set(touch, 0, 0);
      vp_asked = translations = window_translations = 0;
      CHECK( rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 2010
         &&  rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) == 2020
         &&  rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
         &&  rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 0
         &&  rd(1, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 2010
         &&  rd(1, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 0,
            "a device in a screen's window is placed through that window");
      CHECK(vp_asked == 0 && translations == 0 && window_translations == 2,
            "placed once a kind, with no main viewport asked for");
      CHECK( rd(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == 4
         &&  rd(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y) == -2
         &&  rd(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 1
         &&  rd(1, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_X) == 4,
            "its motion and buttons are its own, untranslated");
      CHECK( rd(1, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == -0x8000
         &&  rd(1, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_X) == -0x8000
         &&  input_pointer_frame_pos(1, RARCH_DEVICE_MOUSE_SCREEN, 0, &pos)
         &&  pos == VIDEO_POS_PACK(-0x8000, -0x8000),
            "and no place on the main window's screen");
      CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 40
         &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_X) == 1040
         &&  rd(0, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == 40
         &&  vp_asked == 1 && translations == 1,
            "the main window's device as before");
      /* held to the window's edges, or off-screen as the lightgun is */
      f[1].pos = VIDEO_POS_PACK(150, 20);
      input_pointer_frames_set(f, w, 2, 0);
      CHECK( rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 2099
         &&  rd(1, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == -0x8000
         &&  rd(1, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 1,
            "out of the window: the pointer at its edge, the lightgun off-screen");
      input_pointer_frames_set(f, w, 2, INPUT_POINTERS_POINTER_OFFSCREEN);
      CHECK( rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == -0x8000,
            "and the pointer off-screen where the driver has it so");
      f[1].pos = VIDEO_POS_PACK(10, 20);
      /* by Mouse Index: port 0 reads device 1, in the window */
      input_pointer_frames_set(f, w, 2, INPUT_POINTERS_BY_MOUSE_INDEX);
      CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 2010
         &&  rd(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == 4
         &&  rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 40
         &&  rd(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == -3,
            "by Mouse Index: a port reads its device, in that device's window");
      /* a window that is gone: off-screen, not the main window's place */
      w[1].generation = 4;
      input_pointer_frames_set(f, w, 2, INPUT_POINTERS_MOUSE_3_TOUCHES);
      CHECK( rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == -0x8000
         &&  rd(1, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 1
         &&  rd(1, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 1
         &&  rd(1, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == -0x8000
         &&  rd(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == 4,
            "a gone window's device is off-screen, its motion still its own");
      w[1].generation = 5;
      /* touches: 0 in window 1, 1 in the main window */
      touch[0] = VIDEO_POS_PACK(60, 70);
      touch[1] = VIDEO_POS_PACK(80, 90);
      input_pointer_frames_set(f, NULL, 1, INPUT_POINTERS_GUN_AT_TOUCH);
      {
         input_video_window_t tw[2];
         tw[0] = w[1];
         tw[1] = w[0];
         input_pointer_touches_set(touch, tw, 2, 3, 3);
         CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 2060
            &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == 1
            &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_X) == -0x8000
            &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_X) == 80
            &&  rd(0, RARCH_DEVICE_POINTER_SCREEN, 1, RETRO_DEVICE_ID_POINTER_X) == 1080
            &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == 2060,
               "a touch in a screen's window is placed through it, one in the main window as before");
         tw[0].generation = 9;
         input_pointer_touches_set(touch, tw, 2, 3, 3);
         CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == -0x8000
            &&  rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 1
            &&  rd(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 1
            &&  rd(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_X) == 80,
               "a touch in a gone window is off-screen");
         input_pointer_touches_set(touch, NULL, 2, 3, 3);
         CHECK( rd(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == 60,
               "touches published with no windows are the main window's");
      }
      touches_set(touch, 0, 0);
      f[1].buttons = 0;
   }

   /* 13: the poll's viewport, for a driver that places its own touches:
    * asked for once however often it is wanted, and afresh each poll */
   input_pointer_frames_clear();
   vp_asked = 0;
   CHECK(input_pointer_viewport() && input_pointer_viewport()
         && input_pointer_viewport() && vp_asked == 1,
         "the poll's viewport is asked for once");
   input_pointer_frames_clear();
   CHECK(input_pointer_viewport() && vp_asked == 2,
         "and afresh after the next poll");

   test_flags();
   test_android_answers();
   test_android_gun_fingers();
   if (failures)
   {
      printf("FAIL pointer_frame_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS pointer_frame_test\n");
   return 0;
}
