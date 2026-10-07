/* Cloud sync on content unload, in the shipping frontend.
 *
 * With Cloud Sync in automatic mode, closing content asks for a sync
 * so the save the content leaves behind reaches the server.  The save
 * RAM only reaches disk when the content's deinit flushes it, which a
 * close from the menu runs a frame or more later, and until that
 * deinit finishes the save file belongs to the running core (the sync
 * defers anything it would write there).  A sync started before then
 * uploads, and hashes, whatever the file held last.
 *
 * This links the real retroarch.c and loads sram_core.so, writes a
 * pattern into the core's save RAM and closes the content through
 * CMD_EVENT_UNLOAD_CORE, running frames until the close completes.
 * task_push_cloud_sync is wrapped (-Wl,--wrap): at each call the
 * harness records what the save file holds and whether a core still
 * owns it.  No server is contacted.
 *
 * Lanes:
 *  unload    one sync, pushed once the file on disk is the core's
 *            last save RAM and no core owns it.
 *  no content  unloading with no content running syncs at once, as
 *            before: nothing waits to be flushed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include <boolean.h>

#include "../../../runloop.h"
#include "../../../retroarch.h"
#include "../../../command.h"
#include "../../../content.h"
#include "../../../configuration.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"

#include <time/rtime.h>
#include <file/config_file.h>
#include <queues/task_queue.h>

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

/* ---- the wrapped push ---------------------------------------------- */

static char     srm_path[600];
static uint8_t  pattern[256];
static size_t   pattern_size;
static unsigned n_syncs;
static bool     last_on_disk;
static bool     last_live;

static bool file_holds_pattern(void)
{
   uint8_t buf[512];
   size_t  n;
   FILE   *f = fopen(srm_path, "rb");
   if (!f)
      return false;
   n = fread(buf, 1, sizeof(buf), f);
   fclose(f);
   return n == pattern_size && !memcmp(buf, pattern, pattern_size);
}

void __wrap_task_push_cloud_sync(void)
{
   n_syncs++;
   last_on_disk = file_holds_pattern();
   last_live    = content_savefile_is_live(srm_path);
}

/* ---- helpers ------------------------------------------------------- */

static bool content_running(void)
{
   return (content_get_flags() & CONTENT_ST_FLAG_IS_INITED)
      && runloop_state_get_ptr()->current_core_type != CORE_TYPE_DUMMY;
}

static void run_until_closed(void)
{
   unsigned i;
   for (i = 0; i < 600; i++)
   {
      if (!content_running() && !runloop_state_get_ptr()->content_switching)
         return;
      /* One pass of the frontend's main loop. */
      runloop_iterate();
      task_queue_check();
   }
}

/* ---- lanes --------------------------------------------------------- */

static void lane_unload(const char *core_path)
{
   void    *core = dlopen(core_path, RTLD_NOW | RTLD_NOLOAD);
   uint8_t *(*get_sram)(void);
   uint8_t *sram;
   size_t   i;

   CHECK(content_running(), "the content did not load");
   CHECK(core != NULL, "sram_core.so is not loaded");
   if (!core || !content_running())
      return;
   get_sram = (uint8_t *(*)(void))dlsym(core, "harness_core_sram");
   CHECK(get_sram != NULL, "sram_core.so has no harness_core_sram");
   if (!get_sram)
      return;
   sram         = get_sram();
   pattern_size = 256;
   for (i = 0; i < pattern_size; i++)
      pattern[i] = (uint8_t)(0xA5 ^ i);
   memcpy(sram, pattern, pattern_size);
   dlclose(core);

   CHECK(content_savefile_is_live(srm_path),
         "the save file is not the running core's: %s", srm_path);

   n_syncs = 0;
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);
   run_until_closed();

   CHECK(!content_running(), "the content did not close");
   CHECK(n_syncs == 1, "%u syncs on unload, expected 1", n_syncs);
   CHECK(last_on_disk,
         "the sync was pushed before the save RAM reached disk");
   CHECK(!last_live,
         "the sync was pushed while a core still owned the save file");
   printf("[%s] unload\n", failures ? "FAIL" : "ok");
}

static void lane_no_content(void)
{
   unsigned before = failures;
   n_syncs = 0;
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);
   CHECK(n_syncs == 1, "%u syncs on an unload with no content, expected 1 at once",
         n_syncs);
   run_until_closed();
   CHECK(n_syncs == 1, "%u syncs after the frames that follow, expected 1",
         n_syncs);
   printf("[%s] no content\n", failures == before ? "ok" : "FAIL");
}

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char content_path[512];
   char core_path[1024];
   char cmd[700];
   char dir[400];
   char *rarch_argv[8];
   int rarch_argc = 0;
   FILE *f;
   const char *self = argc > 0 ? argv[0] : "cloudsync_unload_test";
   const char *slash;

   /* sram_core.so sits next to the harness. */
   {
      char cwd[400];
      int  dir_len = (slash = strrchr(self, '/')) ? (int)(slash - self) : 1;
      const char *self_dir = slash ? self : ".";
      if (self_dir[0] == '/')
         snprintf(core_path, sizeof(core_path), "%.*s/sram_core.so",
               dir_len, self_dir);
      else if (getcwd(cwd, sizeof(cwd)))
         snprintf(core_path, sizeof(core_path), "%s/%.*s/sram_core.so",
               cwd, dir_len, self_dir);
      else
         return 1;
   }

   snprintf(dir, sizeof(dir), "/tmp/cloudsync_unload_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", dir);
   if (system(cmd) != 0)
      return 1;

   snprintf(content_path, sizeof(content_path), "%s/game.bin", dir);
   snprintf(srm_path, sizeof(srm_path), "%s/game.srm", dir);
   if ((f = fopen(content_path, "wb")))
   {
      fputs("game", f);
      fclose(f);
   }

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if ((f = fopen(cfg_path, "wb")))
   {
      fprintf(f, "video_driver = \"null\"\n");
      fprintf(f, "audio_driver = \"null\"\n");
      fprintf(f, "input_driver = \"null\"\n");
      fprintf(f, "input_joypad_driver = \"null\"\n");
      fprintf(f, "menu_driver = \"rgui\"\n");
      fprintf(f, "video_threaded = \"false\"\n");
      fprintf(f, "config_save_on_exit = \"false\"\n");
      fprintf(f, "savefile_directory = \"%s\"\n", dir);
      fprintf(f, "savestate_directory = \"%s\"\n", dir);
      fprintf(f, "sort_savefiles_enable = \"false\"\n");
      fprintf(f, "sort_savefiles_by_content_enable = \"false\"\n");
      fprintf(f, "savestate_auto_save = \"false\"\n");
      fprintf(f, "save_file_compression = \"false\"\n");
      fprintf(f, "autosave_interval = \"0\"\n");
      fprintf(f, "cloud_sync_enable = \"true\"\n");
      fprintf(f, "cloud_sync_sync_mode = \"0\"\n");
      fclose(f);
   }

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;
   rarch_argv[rarch_argc++] = (char*)"-L";
   rarch_argv[rarch_argc++] = core_path;
   rarch_argv[rarch_argc++] = content_path;
   if (getenv("HARNESS_VERBOSE"))
      rarch_argv[rarch_argc++] = (char*)"-v";

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   lane_unload(core_path);
   lane_no_content();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL cloudsync_unload_test: %u failures\n", failures);
      return 1;
   }
   printf("cloudsync_unload_test: all lanes pass\n");
   return 0;
}
