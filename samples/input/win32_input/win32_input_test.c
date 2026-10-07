/* The plain Win32 input driver (input/drivers/win32_input.c), run
 * under Wine against a real window.
 *
 * Its keys come from the window procedure, a scancode at a time, into
 * atomic bits; its mouse from where the cursor is at the poll. Held to:
 *
 * - its key map: a key's scancode is what Windows sends for it, and a
 *   scancode translates back to the key;
 * - a key told down reads down - to the frontend's question about a
 *   list of keys, and as the keyboard a core reads - and up when told
 *   up; an extended key (Left) is not the keypad key with the same
 *   low scancode; the top bit of a word of the bits is a bit like any
 *   other;
 * - with another window in front nothing is down, and a key that was
 *   down when the window lost the foreground is not down when it has
 *   it back;
 * - the mouse is published where the cursor is in the window, and its
 *   motion is how far the cursor went since the last poll. */

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../../input/drivers/win32_input.c"

/* ---- what the driver calls ---- */
static HWND test_window;
uintptr_t video_driver_window_get(void) { return (uintptr_t)test_window; }

static input_pointer_frame_t last_frame;
static unsigned              last_flags;
static unsigned              frames_published;
void input_driver_publish_pointers(const input_pointer_frame_t *frames,
      unsigned count, unsigned flags)
{
   if (count)
      last_frame = frames[0];
   last_flags = flags;
   frames_published++;
}

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL: "); fprintf(stderr, __VA_ARGS__); \
   fprintf(stderr, "\n"); failures++; } } while (0)

static void pump(void)
{
   MSG msg;
   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
}

static bool key_listed_down(void *w32, unsigned key)
{
   uint16_t keys[2];
   uint8_t  bind[2] = { 0, 1 };
   uint32_t down[1] = { 0 };
   keys[0] = (uint16_t)key;
   keys[1] = RETROK_F12;
   input_win32.keys_down(w32, 0, keys, bind, 2, down);
   return (down[0] & 1) != 0;
}

static bool key_read_down(void *w32, unsigned key)
{
   return input_win32.input_state(w32, NULL, NULL, NULL, 0,
         RETRO_DEVICE_KEYBOARD, 0, key) != 0;
}

int main(void)
{
   WNDCLASSA wc;
   void *w32;
   POINT origin;
   bool front;
   unsigned i;

   memset(&wc, 0, sizeof(wc));
   wc.lpfnWndProc   = DefWindowProcA;
   wc.hInstance     = GetModuleHandle(NULL);
   wc.lpszClassName = "win32_input_test";
   RegisterClassA(&wc);
   test_window = CreateWindowA("win32_input_test", "win32 input test",
         WS_OVERLAPPEDWINDOW | WS_VISIBLE, 40, 40, 400, 300,
         NULL, NULL, wc.hInstance, NULL);
   if (!test_window)
   {
      printf("[skip] win32 input: no window to be had\n");
      return 0;
   }
   ShowWindow(test_window, SW_SHOW);
   SetForegroundWindow(test_window);
   for (i = 0; i < 20 && GetForegroundWindow() != test_window; i++)
   {
      pump();
      Sleep(50);
   }
   front = GetForegroundWindow() == test_window;

   w32 = input_win32.init(NULL);
   CHECK(w32 != NULL, "the driver did not start");
   if (!w32)
      return 1;

   /* the key map */
   CHECK(rarch_keysym_lut[RETROK_a]      == 0x1E, "A's scancode is 0x%02X", (unsigned)rarch_keysym_lut[RETROK_a]);
   CHECK(rarch_keysym_lut[RETROK_RETURN] == 0x1C, "Return's scancode is 0x%02X", (unsigned)rarch_keysym_lut[RETROK_RETURN]);
   CHECK(rarch_keysym_lut[RETROK_LEFT]   == 0xCB, "Left's scancode is 0x%02X", (unsigned)rarch_keysym_lut[RETROK_LEFT]);
   CHECK(rarch_keysym_lut[RETROK_KP4]    == 0x4B, "keypad 4's scancode is 0x%02X", (unsigned)rarch_keysym_lut[RETROK_KP4]);
   CHECK(input_keymaps_translate_keysym_to_rk(0x1E) == RETROK_a, "scancode 0x1E does not translate to A");
   CHECK(input_keymaps_translate_keysym_to_rk(0xCB) == RETROK_LEFT, "scancode 0xCB does not translate to Left");

   /* a key down, and up */
   CHECK(!key_listed_down(w32, RETROK_a) && !key_read_down(w32, RETROK_a), "A is down before anything was pressed");
   win32_input_key_message(0x1E, true);
   CHECK(key_listed_down(w32, RETROK_a), "A told down is not in the list of keys down");
   CHECK(key_read_down(w32, RETROK_a), "A told down does not read down");
   CHECK(!key_listed_down(w32, RETROK_s) && !key_read_down(w32, RETROK_b), "another key reads down with A");
   win32_input_key_message(0x1E, false);
   CHECK(!key_listed_down(w32, RETROK_a) && !key_read_down(w32, RETROK_a), "A told up still reads down");

   /* an extended key and the keypad key under the same low scancode */
   win32_input_key_message(0xCB, true);
   CHECK(key_listed_down(w32, RETROK_LEFT), "Left told down is not down");
   CHECK(!key_listed_down(w32, RETROK_KP4), "keypad 4 reads down with Left");
   win32_input_key_message(0xCB, false);

   /* the top bit of a word: scancodes 0x1F (S) and 0xDF */
   win32_input_key_message(0x1E, true);
   win32_input_key_message(0x1F, true);
   CHECK(key_listed_down(w32, RETROK_s) && key_listed_down(w32, RETROK_a), "S and A told down are not both down");
   win32_input_key_message(0x1F, false);
   CHECK(!key_listed_down(w32, RETROK_s) && key_listed_down(w32, RETROK_a), "S told up took A with it, or stayed");
   win32_input_key_message(0x1E, false);

   if (!front)
      printf("[skip] win32 input: the window could not be brought to the front; focus and mouse not checked\n");
   else
   {
      /* in front: a poll leaves the keys alone and publishes the mouse */
      GetClientRect(test_window, (RECT*)&origin); /* only to touch the window */
      origin.x = 0;
      origin.y = 0;
      ClientToScreen(test_window, &origin);
      SetCursorPos(origin.x + 50, origin.y + 60);
      win32_input_key_message(0x1E, true);
      input_win32.poll(w32);
      CHECK(key_listed_down(w32, RETROK_a), "a poll with the window in front let A go");
      CHECK(frames_published == 1, "the poll published %u frames", frames_published);
      CHECK(VIDEO_POS_X(last_frame.pos) == 50 && VIDEO_POS_Y(last_frame.pos) == 60,
            "the cursor at 50,60 in the window was published at %d,%d",
            (int)VIDEO_POS_X(last_frame.pos), (int)VIDEO_POS_Y(last_frame.pos));
      CHECK(VIDEO_POS_X(last_frame.rel) == 0 && VIDEO_POS_Y(last_frame.rel) == 0,
            "the first poll has motion %d,%d", (int)VIDEO_POS_X(last_frame.rel), (int)VIDEO_POS_Y(last_frame.rel));
      SetCursorPos(origin.x + 60, origin.y + 55);
      input_win32.poll(w32);
      CHECK(VIDEO_POS_X(last_frame.rel) == 10 && VIDEO_POS_Y(last_frame.rel) == -5,
            "the cursor moved 10,-5 and the motion published is %d,%d",
            (int)VIDEO_POS_X(last_frame.rel), (int)VIDEO_POS_Y(last_frame.rel));
      CHECK(last_flags & INPUT_POINTERS_GUN_BUTTONS_BOUND, "the frame does not say the lightgun's buttons are bound");

      /* another window in front: nothing is down, and A does not come
       * back with the foreground */
      {
         HWND other = CreateWindowA("win32_input_test", "other",
               WS_OVERLAPPEDWINDOW | WS_VISIBLE, 500, 40, 200, 200,
               NULL, NULL, wc.hInstance, NULL);
         SetForegroundWindow(other);
         for (i = 0; i < 20 && GetForegroundWindow() != other; i++)
         {
            pump();
            Sleep(50);
         }
         if (GetForegroundWindow() != other)
            printf("[skip] win32 input: a second window could not take the front; focus loss not checked\n");
         else
         {
            input_win32.poll(w32);
            CHECK(!key_listed_down(w32, RETROK_a), "A reads down with another window in front");
            SetForegroundWindow(test_window);
            for (i = 0; i < 20 && GetForegroundWindow() != test_window; i++)
            {
               pump();
               Sleep(50);
            }
            input_win32.poll(w32);
            CHECK(!key_listed_down(w32, RETROK_a), "A came back down with the foreground");
         }
         DestroyWindow(other);
      }
   }

   input_win32.free(w32);
   DestroyWindow(test_window);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("PASS win32_input_test\n");
   return 0;
}
