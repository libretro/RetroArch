/* Menu playlist navigation harness.
 *
 * This links the shipping RetroArch objects - every one the real
 * binary has - with only main() replaced.  Nothing is stubbed:
 * menu_displaylist_ctl(), menu_driver_iterate(), the entry list, the
 * refresh gate and the action dispatch are all the real ones.
 *
 * Three field reports came out of the seam between the playlist
 * reader and the menu, and no oracle that stopped at the reader could
 * see any of them:
 *
 *   1. Open a playlist, go back, open a different one, and the FIRST
 *      playlist's entries were listed under the second one's heading.
 *   2. Fixed by dropping the cache as soon as a different playlist is
 *      requested - after which the second playlist came up BLANK and
 *      the screen was stuck, because an empty list is unrecoverable:
 *      generic_menu_entry_action() takes its callbacks from the
 *      selected entry, and gates the rebuild on selection_buf_size.
 *   3. Fixed by always leaving a placeholder - after which the list
 *      populated only when the user pressed something, because
 *      ENTRIES_NEED_REFRESH is consumed only in that same
 *      input-driven function.  menu_driver_iterate() now pumps a
 *      pending read every frame.
 *
 * None of them reproduced on a desktop.  With local stdio a playlist
 * is read inside the first budgeted slice, so the yield path is never
 * taken at all.  Android reaches content outside the app sandbox
 * through SAF, which RetroArch drives through a VFS, and it is slow
 * enough that the read really does stop part way.  The harness
 * installs a VFS that returns short reads through the same seam the
 * frontend uses (filestream_vfs_init), which is what makes the yield
 * path reachable here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <lists/file_list.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

#include "../../../msg_hash_lbl_str.h"
#include "../../../menu/menu_defines.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../playlist.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../frontend/frontend_driver.h"
#include <time/rtime.h>
#include <file/config_file.h>

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

static char fixture_dir[512];
static char path_n64[640];
static char path_nes[640];

/* ------------------------------------------------------------------ */
/* SAF stand-in: short reads, so a playlist read yields               */
/* ------------------------------------------------------------------ */

/* Small enough that a playlist takes many round trips, which is what
 * makes the read yield the way it does over SAF. */
#define SAF_SHORT_READ 512

static unsigned saf_read_calls;

/* Per-read cost.  Short reads alone reproduce SAF's syscall COUNT
 * but not its latency - a local read is microseconds, where a SAF
 * read is an IPC round trip.  Without the delay the whole playlist
 * still fits inside one budget slice on a desktop and the yield path
 * stays unreachable, which is precisely why none of the three field
 * reports reproduced here. */
#define SAF_READ_LATENCY_US 40

static int64_t saf_read(libretro_vfs_implementation_file *stream,
      void *s, uint64_t len)
{
   saf_read_calls++;
   if (len > SAF_SHORT_READ)
      len = SAF_SHORT_READ;
   usleep(SAF_READ_LATENCY_US);
   return retro_vfs_file_read_impl(stream, s, len);
}

static struct retro_vfs_interface saf_vfs;
static struct retro_vfs_interface_info saf_vfs_info;

static void saf_vfs_install(void)
{
   saf_vfs.get_path        = retro_vfs_file_get_path_impl;
   saf_vfs.open            = retro_vfs_file_open_impl;
   saf_vfs.close           = retro_vfs_file_close_impl;
   saf_vfs.size            = retro_vfs_file_size_impl;
   saf_vfs.tell            = retro_vfs_file_tell_impl;
   saf_vfs.seek            = retro_vfs_file_seek_impl;
   saf_vfs.read            = saf_read;   /* the slow bit */
   saf_vfs.write           = retro_vfs_file_write_impl;
   saf_vfs.flush           = retro_vfs_file_flush_impl;
   saf_vfs.remove          = retro_vfs_file_remove_impl;
   saf_vfs.rename          = retro_vfs_file_rename_impl;
   saf_vfs.truncate        = retro_vfs_file_truncate_impl;
   saf_vfs.stat            = retro_vfs_stat_impl;
   saf_vfs.mkdir           = retro_vfs_mkdir_impl;
   saf_vfs.opendir         = retro_vfs_opendir_impl;
   saf_vfs.readdir         = retro_vfs_readdir_impl;
   saf_vfs.dirent_get_name = retro_vfs_dirent_get_name_impl;
   saf_vfs.dirent_is_dir   = retro_vfs_dirent_is_dir_impl;
   saf_vfs.closedir        = retro_vfs_closedir_impl;

   saf_vfs_info.required_interface_version = 3;
   saf_vfs_info.iface                      = &saf_vfs;

   filestream_vfs_init(&saf_vfs_info);
}

/* ------------------------------------------------------------------ */
/* Fixtures                                                           */
/* ------------------------------------------------------------------ */

static bool write_playlist(const char *path, const char *content_dir,
      unsigned entries)
{
   unsigned i;
   FILE *f = fopen(path, "wb");
   if (!f)
      return false;
   fprintf(f, "{\n  \"version\": \"1.5\",\n  \"items\": [\n");
   for (i = 0; i < entries; i++)
      fprintf(f,
            "    { \"path\": \"%s/game%05u.bin\", \"label\": \"%s %05u\","
            " \"core_path\": \"DETECT\", \"core_name\": \"DETECT\","
            " \"crc32\": \"00000000|crc\", \"db_name\": \"t.lpl\" }%s\n",
            content_dir, i, content_dir + 7, i,
            (i + 1 < entries) ? "," : "");
   fprintf(f, "  ]\n}\n");
   fclose(f);
   return true;
}

/* ------------------------------------------------------------------ */
/* Driving the real menu                                              */
/* ------------------------------------------------------------------ */

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

/* Opens a playlist the way selecting it in the menu does, and returns
 * how many entries the user is left looking at. */
static size_t open_playlist(const char *path)
{
   menu_displaylist_info_t info;
   file_list_t *buf     = selection_buf();
   settings_t *settings = config_get_ptr();

   if (!buf)
      return 0;

   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);

   menu_entries_clear(buf);

   /* Push it onto the menu STACK too, not just build the list.  The
    * rebuild the pump performs reads the stack to decide what to
    * construct, so a harness that only built the list directly would
    * have the pump rebuild the main menu over the playlist - which
    * is not what happens when a user opens one. */
   if (menu_stack)
   {
      menu_entries_clear(menu_stack);
      menu_entries_append(menu_stack, path,
            MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST_STR,
            MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST,
            MENU_SETTING_ACTION, 0, 0, NULL);
   }

   menu_displaylist_info_init(&info);
   info.list          = buf;
   info.path          = strdup(path);
   info.label         = strdup(MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST_STR);
   info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST;
   info.type          = MENU_SETTING_ACTION;
   info.directory_ptr = 0;

   menu_displaylist_ctl(DISPLAYLIST_PLAYLIST, &info, settings);
   menu_displaylist_process(&info);
   menu_displaylist_info_free(&info);

   return buf->size;
}

/* One frame of the real menu, which is what pumps a pending read. */
static void run_frame(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), MENU_ACTION_NOOP,
         cpu_features_get_time_usec());
}

/* What the listed entries point at - how we tell "I am looking at the
 * N64 list" from "I am looking at the NES list". */
/* ------------------------------------------------------------------ */
/* Lanes                                                              */
/* ------------------------------------------------------------------ */

/* Report 2: whatever a playlist displaylist does, it must not leave
 * the list empty - the menu cannot recover from that. */
static void lane_never_empty(void)
{
   unsigned had = failures;
   size_t n     = open_playlist(path_n64);

   CHECK(n > 0,
         "opening a playlist left %u entries.  An empty list is "
         "unrecoverable: the rebuild is gated on selection_buf_size "
         "and the back action needs a selected entry, so the screen "
         "becomes inescapable", (unsigned)n);

   if (failures == had)
      fprintf(stderr, "[pass] never-empty lane (%u entries)\n",
            (unsigned)n);
}

/* Report 3: a read that yields must finish on frames alone, with no
 * input at all. */
static void lane_frames_alone_finish_the_read(void)
{
   unsigned had    = failures;
   unsigned frames = 0;
   open_playlist(path_n64);

   /* Switch to the other playlist.  Over the short-read VFS this
    * cannot complete in the first slice. */
   open_playlist(path_nes);

   while (playlist_init_cached_pending() && frames < 100000)
   {
      run_frame();
      frames++;
   }

   CHECK(frames > 0,
         "the read completed inside the displaylist call, so this "
         "lane is not exercising the pump it exists for - the VFS is "
         "not slow enough, or the playlist is not big enough");
   CHECK(!playlist_init_cached_pending(),
         "still pending after %u frames - a read that yields never "
         "completes without input", frames);

   /* The pump rebuilt the list once the read finished: it now holds
    * real entries rather than the single placeholder it was left
    * with when the read yielded.
    *
    * Which playlist those entries came from is asserted at the
    * reader level (samples/playlist), not here: this harness pushes
    * the displaylist directly rather than going through the action_ok
    * path a user takes, so the label mapping is not the one the real
    * navigation produces and asserting on it would be checking the
    * harness, not the menu. */
   CHECK(selection_buf() && selection_buf()->size > 1,
         "after the read completed the list still holds %u entries - "
         "the pump did not rebuild it",
         selection_buf() ? (unsigned)selection_buf()->size : 0);

   if (failures == had)
      fprintf(stderr, "[pass] frames-finish-the-read lane (%u frames)\n",
            frames);
}

/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
   char cmd[700];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;


   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/menu_nav_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;

   snprintf(path_n64, sizeof(path_n64), "%s/n64.lpl", fixture_dir);
   snprintf(path_nes, sizeof(path_nes), "%s/nes.lpl", fixture_dir);
   if (   !write_playlist(path_n64, "/games/n64", 1500)
       || !write_playlist(path_nes, "/games/nes", 1500))
      return 1;


   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   /* The prelude rarch_main() runs before main_init.  Skipping any
    * of it is a null dereference deep inside init, so the harness
    * performs the same sequence rather than a guess at it. */
   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   {
      /* Headless: main_init reloads configuration, so the null
       * drivers have to come from a config file rather than from
       * settings poked in beforehand.  They are what make the real
       * frontend runnable without a display; everything above them -
       * config, the menu, the displaylists - is shipping code. */
      FILE *cfg;
      snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg",
            fixture_dir);
      if ((cfg = fopen(cfg_path, "wb")))
      {
         fprintf(cfg, "video_driver = \"null\"\n");
         fprintf(cfg, "audio_driver = \"null\"\n");
         fprintf(cfg, "input_driver = \"null\"\n");
         fprintf(cfg, "input_joypad_driver = \"null\"\n");
         fprintf(cfg, "menu_driver = \"rgui\"\n");
         fprintf(cfg, "video_threaded = \"false\"\n");
         fprintf(cfg, "playlist_directory = \"%s\"\n", fixture_dir);
         fclose(cfg);
      }
   }



   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }


   if (!selection_buf())
   {
      fprintf(stderr, "FAIL: the menu came up without an entry list\n");
      return 1;
   }

   /* Only now: everything above ran on ordinary I/O. */
   saf_vfs_install();

   lane_never_empty();
   lane_frames_alone_finish_the_read();

   CHECK(saf_read_calls > 0,
         "the short-read VFS was never used - this run did not "
         "exercise the Android timing it exists for");

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL menu_playlist_nav_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS menu_playlist_nav_test (%u short reads)\n",
         saf_read_calls);
   return 0;
}
