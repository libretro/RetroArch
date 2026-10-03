/* A mouse in the menu: left click, right click, wheel and movement.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * drives the real frame loop, runloop_iterate(), with a scripted input
 * driver in place of the null one.  The scripted driver behaves like a
 * real one: input_state() reports what the last poll() latched.
 *
 * The bug this pins: with the pen support, the menu gave every pointer
 * that is not a touchscreen no gesture at all on release.  A mouse is
 * such a pointer, and its left click reaches the menu drivers as a tap
 * gesture, so the click was thrown away - in every menu driver, on
 * every platform.  Moving the pointer, the wheel and the right button
 * take other paths and kept working, which is how it went unnoticed.
 *
 * The claims:
 *
 *   1. Moving the mouse moves the menu's pointer, and the entry under
 *      it is the one the menu reports.
 *   2. A left click on an entry selects it: clicking Settings opens
 *      Settings.
 *   3. A right click goes back.
 *   4. The wheel scrolls a list longer than the screen, down and up.
 *   5. A press held past the long-press time is not a click.
 *
 * The null video driver has no viewport and never draws a menu frame,
 * and RGUI reads the pointer while drawing.  So the harness's video
 * driver supplies both, the way a real one does: a viewport the size
 * of RGUI's framebuffer, which makes mouse coordinates menu
 * coordinates, and the menu's per-frame call.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <libretro.h>
#include <lists/file_list.h>
#include <file/config_file.h>
#include <time/rtime.h>
#include <string/stdstring.h>

#include "../../../menu/menu_driver.h"
#include "../../../input/input_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/gfx_display.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../frontend/frontend_driver.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

/* ------------------------------------------------------------------ */
/* Scripted input driver: a mouse                                     */
/* ------------------------------------------------------------------ */

struct mouse
{
   int16_t x, y;
   bool    left, right, wheel_up, wheel_down;
};

static struct mouse held;     /* what the "user" is doing */
static struct mouse latched;  /* what the last poll saw */
static input_driver_t scripted;

static void scripted_poll(void *data)
{
   (void)data;
   latched         = held;
   /* A wheel notch is one poll long. */
   held.wheel_up   = false;
   held.wheel_down = false;
}

static int16_t scripted_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   (void)data; (void)joypad; (void)sec_joypad; (void)joypad_info;
   (void)binds; (void)keyboard_mapping_blocked; (void)idx;

   if (port != 0)
      return 0;

   if (device == RARCH_DEVICE_MOUSE_SCREEN)
   {
      if (id == RETRO_DEVICE_ID_MOUSE_X)
         return latched.x;
      if (id == RETRO_DEVICE_ID_MOUSE_Y)
         return latched.y;
   }
   else if (device == RETRO_DEVICE_MOUSE)
   {
      switch (id)
      {
         case RETRO_DEVICE_ID_MOUSE_LEFT:
            return latched.left;
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
            return latched.right;
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
            return latched.wheel_up;
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
            return latched.wheel_down;
         default:
            break;
      }
   }
   return 0;
}

/* ------------------------------------------------------------------ */
/* Scripted video driver: a viewport and the menu's frame call        */
/* ------------------------------------------------------------------ */

static video_driver_t scripted_video;
static bool (*null_frame)(void *, const void *, unsigned, uint64_t,
      unsigned, const char *, video_frame_info_t *);

static void scripted_viewport_info(void *data, struct video_viewport *vp)
{
   (void)data;
   memset(vp, 0, sizeof(*vp));
   vp->dims = disp_get_ptr()->framebuf_dims;
}

static bool scripted_frame(void *data, const void *frame, unsigned dims,
      uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   if (video_info)
      menu_driver_frame(
            (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0,
            video_info);
   return null_frame(data, frame, dims, frame_count, pitch, msg,
         video_info);
}

static void scripted_install(void)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();

   scripted                       = *input_st->current_driver;
   scripted.poll                  = scripted_poll;
   scripted.input_state           = scripted_input_state;
   input_st->current_driver       = &scripted;

   scripted_video                 = *video_st->current_video;
   null_frame                     = scripted_video.frame;
   scripted_video.frame           = scripted_frame;
   scripted_video.viewport_info   = scripted_viewport_info;
   video_st->current_video        = &scripted_video;
}

/* ------------------------------------------------------------------ */
/* Frame helpers                                                      */
/* ------------------------------------------------------------------ */

/* One frame through the real loop, paced at roughly 60 Hz. */
#define FRAME_US 16667

static void frames(unsigned n)
{
   while (n--)
   {
      runloop_iterate();
      usleep(FRAME_US);
   }
}

static file_list_t *entries(void)
{
   menu_list_t *menu_list = menu_state_get_ptr()->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

static size_t stack_depth(void)
{
   menu_list_t *menu_list = menu_state_get_ptr()->entries.list;
   return menu_list ? MENU_LIST_GET(menu_list, 0)->size : 0;
}

/* The entry the menu says is under the pointer. */
static unsigned pointed_entry(void)
{
   return menu_state_get_ptr()->input_state.ptr;
}

static int entry_index(const char *label)
{
   file_list_t *buf = entries();
   size_t i;

   for (i = 0; buf && i < buf->size; i++)
      if (string_is_equal(buf->list[i].label, label))
         return (int)i;
   return -1;
}

/* Move the mouse down the screen until the menu reports @entry under
 * it. The layout is the menu driver's business, so the row is found by
 * asking rather than computed. Returns false if no row is. */
static bool point_at(unsigned entry)
{
   unsigned fb_height = VIDEO_SCALE_H(disp_get_ptr()->framebuf_dims);
   unsigned y;

   held.x = (int16_t)(VIDEO_SCALE_W(disp_get_ptr()->framebuf_dims) / 2);

   for (y = 0; y < fb_height; y += 2)
   {
      held.y = (int16_t)y;
      frames(2);
      if (     pointed_entry() == entry
            && menu_state_get_ptr()->input_state.pointer.y == (int16_t)y)
      {
         /* A row is several pixels tall: stop a little inside it. */
         held.y = (int16_t)(y + 2);
         frames(2);
         return pointed_entry() == entry;
      }
   }
   return false;
}

static void click(bool right, unsigned held_frames)
{
   if (right)
      held.right = true;
   else
      held.left  = true;
   frames(held_frames);
   held.left  = false;
   held.right = false;
   frames(10);
}

/* ------------------------------------------------------------------ */
/* Lanes                                                              */
/* ------------------------------------------------------------------ */

/* 1: the pointer reaches the menu. */
static void lane_pointer_moves(void)
{
   unsigned had     = failures;
   file_list_t *buf = entries();
   unsigned last    = buf ? (unsigned)buf->size - 1 : 0;

   CHECK(buf && buf->size >= 3, "the main menu has fewer than 3 entries");
   CHECK(point_at(0), "moving the mouse never pointed at the first entry");
   CHECK(point_at(last), "moving the mouse never pointed at the last entry");
   CHECK(menu_state_get_ptr()->input_state.pointer.type == MENU_POINTER_MOUSE,
         "the menu did not take the pointer for a mouse");

   if (failures == had)
      fprintf(stderr, "[pass] moving the mouse points at entries\n");
}

/* 2, 4 and 3, in the order a user would: open Settings with a left
 * click, scroll its list with the wheel, go back with a right click. */
static void lane_click_wheel_and_back(void)
{
   unsigned had   = failures;
   int settings   = entry_index("settings");
   size_t depth   = stack_depth();
   size_t top;

   CHECK(settings >= 0, "no Settings entry in the main menu");
   if (settings < 0)
      return;

   CHECK(point_at((unsigned)settings),
         "could not point the mouse at Settings");

   click(false, 3);

   CHECK(stack_depth() == depth + 1,
         "a left click on Settings did not open it (menu depth %u,"
         " expected %u): the click was not taken as a tap",
         (unsigned)stack_depth(), (unsigned)(depth + 1));

   if (failures == had)
      fprintf(stderr, "[pass] a left click opens the entry under it\n");

   if (stack_depth() == depth + 1)
   {
      unsigned had_wheel = failures;

      top = menu_state_get_ptr()->entries.begin;
      held.wheel_down = true;
      frames(5);
      CHECK(menu_state_get_ptr()->entries.begin > top,
            "the wheel did not scroll the Settings list down"
            " (%u entries, still starting at %u)",
            (unsigned)(entries() ? entries()->size : 0),
            (unsigned)menu_state_get_ptr()->entries.begin);

      top = menu_state_get_ptr()->entries.begin;
      held.wheel_up = true;
      frames(5);
      CHECK(menu_state_get_ptr()->entries.begin < top,
            "the wheel did not scroll the Settings list back up"
            " (still starting at %u)",
            (unsigned)menu_state_get_ptr()->entries.begin);

      if (failures == had_wheel)
         fprintf(stderr, "[pass] the wheel scrolls the list\n");
   }

   {
      unsigned had_back = failures;

      click(true, 3);

      CHECK(stack_depth() == depth,
            "a right click did not go back (menu depth %u, expected %u)",
            (unsigned)stack_depth(), (unsigned)depth);

      if (failures == had_back)
         fprintf(stderr, "[pass] a right click goes back\n");
   }
}

/* 5: a long press is not a click. */
static void lane_long_press_is_not_a_click(void)
{
   unsigned had = failures;
   int settings = entry_index("settings");
   size_t depth = stack_depth();

   if (settings < 0 || !point_at((unsigned)settings))
   {
      CHECK(false, "could not point the mouse at Settings");
      return;
   }

   /* Well past MENU_INPUT_PRESS_TIME_LONG. */
   click(false, 120);

   CHECK(stack_depth() == depth,
         "a press held for two seconds opened Settings like a click");

   if (failures == had)
      fprintf(stderr, "[pass] a long press is not a click\n");
}

int main(int argc, char *argv[])
{
   char cmd[700];
   static char fixture_dir[512];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;
   FILE *cfg;

   (void)argc; (void)argv;

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/menu_mouse_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   /* The prelude rarch_main() runs before main_init */
   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", fixture_dir);
   if (!(cfg = fopen(cfg_path, "wb")))
      return 1;
   fprintf(cfg, "video_driver = \"null\"\n");
   fprintf(cfg, "audio_driver = \"null\"\n");
   fprintf(cfg, "input_driver = \"null\"\n");
   fprintf(cfg, "input_joypad_driver = \"null\"\n");
   fprintf(cfg, "menu_driver = \"rgui\"\n");
   fprintf(cfg, "video_threaded = \"false\"\n");
   fprintf(cfg, "menu_mouse_enable = \"true\"\n");
   fprintf(cfg, "menu_pointer_enable = \"false\"\n");
   fclose(cfg);

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   /* --menu brings the menu up on the first frames */
   {
      unsigned i;
      for (i = 0; i < 10; i++)
         runloop_iterate();
   }

   if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
   {
      fprintf(stderr, "FAIL: the menu is not up\n");
      return 1;
   }

   scripted_install();

   lane_pointer_moves();
   lane_click_wheel_and_back();
   lane_long_press_is_not_a_click();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL menu_mouse_click_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS menu_mouse_click_test\n");
   return 0;
}
