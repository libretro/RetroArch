/* Menu navigation auto-repeat across a stalled frame.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * drives the real frame loop, runloop_iterate(), with a scripted
 * input driver in place of the null one.  The scripted driver
 * behaves like a real one: input_state() reports what the last
 * poll() latched, not what the harness is holding right now.
 *
 * The menu collects its input before it polls, so the state it acts
 * on is one poll old.  A frame that stalls after its poll - Ozone
 * and XMB build a whole playlist synchronously when the selection
 * lands on it - must not count the stall as time the button was
 * held: a single press then fires an auto-repeat on the next frame
 * and the selection skips an entry (issue #16350).
 *
 * The stall is injected inside poll(), after the latch.  From the
 * menu's point of view that is the same as a stall later in the
 * frame: it lies after the timestamp of the poll that saw the
 * button down, and before the poll that sees it released.
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
#include <features/features_cpu.h>

#include "../../../menu/menu_driver.h"
#include "../../../input/input_driver.h"
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

/* Well past the default 256 ms initial scroll delay */
#define STALL_US 1000000

/* ------------------------------------------------------------------ */
/* Scripted input driver                                              */
/* ------------------------------------------------------------------ */

static bool held_down;        /* what the "user" is doing */
static bool latched_down;     /* what the last poll saw */
static bool stall_next_poll;
static input_driver_t scripted;

static void scripted_poll(void *data)
{
   (void)data;
   latched_down = held_down;
   if (stall_next_poll)
   {
      stall_next_poll = false;
      usleep(STALL_US);
   }
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
   if (port != 0 || device != RETRO_DEVICE_JOYPAD || !latched_down)
      return 0;
   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      return (int16_t)(1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
   return id == RETRO_DEVICE_ID_JOYPAD_DOWN;
}

static void scripted_install(void)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   scripted                       = *input_st->current_driver;
   scripted.poll                  = scripted_poll;
   scripted.input_state           = scripted_input_state;
   input_st->current_driver       = &scripted;
}

/* ------------------------------------------------------------------ */
/* Frame helpers                                                      */
/* ------------------------------------------------------------------ */

static size_t selection(void)
{
   return menu_state_get_ptr()->selection_ptr;
}

/* One frame through the real loop, paced at roughly 60 Hz the way a
 * display would pace it; returns 1 if the selection moved */
#define FRAME_US 16667

static unsigned frame(void)
{
   size_t before = selection();
   runloop_iterate();
   usleep(FRAME_US);
   return selection() != before;
}

static void settle(void)
{
   unsigned i;
   held_down = false;
   for (i = 0; i < 30; i++)
      frame();
}

/* ------------------------------------------------------------------ */
/* Lanes                                                              */
/* ------------------------------------------------------------------ */

/* A short press whose frame stalls moves exactly once. */
static void lane_press_across_stall_moves_once(void)
{
   unsigned had   = failures;
   unsigned moves = 0;
   unsigned i;

   settle();

   held_down       = true;
   moves          += frame();   /* poll latches the press */
   stall_next_poll = true;
   moves          += frame();   /* press acted on, then the stall */
   held_down       = false;     /* released during the stall */
   for (i = 0; i < 30; i++)
      moves += frame();

   CHECK(moves == 1,
         "one press across a %u ms stall moved the selection %u times; "
         "the stall was counted as hold time and fired an auto-repeat",
         STALL_US / 1000, moves);

   if (failures == had)
      fprintf(stderr, "[pass] press across stall moves once\n");
}

/* A button that really is held through the stall still repeats as
 * soon as a poll after the stall sees it down. */
static void lane_hold_across_stall_repeats(void)
{
   unsigned had   = failures;
   unsigned moves = 0;
   unsigned i;

   settle();

   held_down       = true;
   moves          += frame();
   stall_next_poll = true;
   moves          += frame();   /* first move, then the stall */
   for (i = 0; i < 3; i++)
      moves += frame();         /* still held */

   CHECK(moves >= 2,
         "a button held through a %u ms stall did not auto-repeat "
         "within three frames after it (%u moves)",
         STALL_US / 1000, moves);

   settle();
   if (failures == had)
      fprintf(stderr, "[pass] hold across stall repeats (%u moves)\n",
            moves);
}

/* Without a stall, the first repeat still waits out the initial
 * scroll delay and then arrives. */
static void lane_steady_hold_initial_delay(void)
{
   unsigned had          = failures;
   unsigned delay_ms     = config_get_ptr()->uints.menu_scroll_delay;
   retro_time_t pressed  = 0;
   retro_time_t repeated = 0;
   unsigned moves        = 0;
   unsigned i;

   settle();

   held_down = true;
   for (i = 0; i < 600 && moves < 2; i++)
   {
      unsigned moved = frame();
      if (moved)
      {
         moves++;
         if (moves == 1)
            pressed  = cpu_features_get_time_usec();
         else
            repeated = cpu_features_get_time_usec();
      }
      if (moves == 1 && cpu_features_get_time_usec() - pressed
            > (retro_time_t)delay_ms * 1000 * 4)
         break;
   }
   held_down = false;

   CHECK(moves == 2, "a steady hold never auto-repeated");
   if (moves == 2)
      CHECK(repeated - pressed >= (retro_time_t)delay_ms * 1000 * 9 / 10,
            "the first auto-repeat came %lld ms after the press, "
            "before the %u ms initial scroll delay",
            (long long)((repeated - pressed) / 1000), delay_ms);

   settle();
   if (failures == had)
      fprintf(stderr, "[pass] steady hold waits out the initial delay "
            "(%lld ms, delay %u ms)\n",
            (long long)((repeated - pressed) / 1000), delay_ms);
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
         "/tmp/menu_repeat_%ld", (long)getpid());
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
   fprintf(cfg, "menu_navigation_wraparound_enable = \"true\"\n");
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

   lane_press_across_stall_moves_once();
   lane_hold_across_stall_repeats();
   lane_steady_hold_initial_delay();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL menu_nav_repeat_stall_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS menu_nav_repeat_stall_test\n");
   return 0;
}
