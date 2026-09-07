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
#include <string/stdstring.h>
#include <vfs/vfs_implementation.h>

#include "../../../msg_hash_lbl_str.h"
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
static char path_based[640];
static char path_lvw[640];

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

/* As write_playlist, but recording a base_content_directory - the
 * shape of the #19427 attachment. */
static bool write_playlist_with_base(const char *path,
      const char *content_dir, unsigned entries)
{
   unsigned i;
   FILE *f = fopen(path, "wb");
   if (!f)
      return false;
   fprintf(f, "{\n  \"version\": \"1.5\",\n"
              "  \"base_content_directory\": \"%s\",\n"
              "  \"items\": [\n", content_dir);
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

/* A saved Explore view, the shape explore_action_saveview_complete()
 * writes.  The Playlists screen lists it from the same directory walk
 * that finds the .lpl files. */
static bool write_view(const char *path)
{
   FILE *f = fopen(path, "wb");
   if (!f)
      return false;
   fprintf(f, "{\n  \"filter_name\": \"metroid\",\n"
              "  \"filter_equal\": {\n    \"genre\": \"Platform\"\n  }\n}\n");
   fclose(f);
   return true;
}
/* ------------------------------------------------------------------ */

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

/* Builds the "Playlists" screen - the list of .lpl files - the way
 * opening that screen does. */
static size_t open_playlists_screen(void)
{
   menu_displaylist_info_t info;
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = selection_buf();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);
   settings_t *settings       = config_get_ptr();

   if (!buf || !menu_stack)
      return 0;

   menu_entries_clear(buf);
   menu_entries_append(menu_stack, fixture_dir,
         MENU_ENUM_LABEL_PLAYLISTS_TAB_STR,
         MENU_ENUM_LABEL_PLAYLISTS_TAB,
         MENU_SETTING_ACTION, 0, 0, NULL);

   menu_displaylist_info_init(&info);
   info.list          = buf;
   info.path          = strdup(fixture_dir);
   info.label         = strdup(MENU_ENUM_LABEL_PLAYLISTS_TAB_STR);
   info.enum_idx      = MENU_ENUM_LABEL_PLAYLISTS_TAB;
   info.type          = MENU_SETTING_ACTION;
   info.directory_ptr = 0;

   menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS, &info,
         settings);
   menu_displaylist_process(&info);
   menu_displaylist_info_free(&info);

   return buf->size;
}

/* Presses OK on the first entry whose path (or, for entries that
 * carry a file path in the label slot, whose label) contains
 * @needle, through the real dispatcher and whatever action_ok the
 * menu bound to that entry. */
static bool press_ok_on_match(const char *needle, bool match_label)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = selection_buf();
   size_t i;

   if (!buf)
      return false;

   for (i = 0; i < buf->size; i++)
   {
      const char *s = match_label ? buf->list[i].label : buf->list[i].path;
      if (s && strstr(s, needle))
      {
         menu_entry_t entry;
         menu_st->selection_ptr = i;
         MENU_ENTRY_INITIALIZE(entry);
         /* The macro zeroes flags, and menu_entry_get() fills only
          * what the flags enable - so without these the action is
          * handed an empty path and the rebuild that follows
          * dereferences it.  Same set the menu enables before it
          * dispatches an action. */
         entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                      | MENU_ENTRY_FLAG_LABEL_ENABLED
                      | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                      | MENU_ENTRY_FLAG_VALUE_ENABLED
                      | MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
         menu_entry_get(&entry, 0, i, NULL, true);
         menu_entry_action(&entry, i, MENU_ACTION_OK);
         return true;
      }
   }
   return false;
}

/* Selects the entry whose path ends in @leaf and presses OK on it.
 * This is what a tap on a playlist does. */
static bool select_and_press_ok(const char *leaf)
{
   return press_ok_on_match(leaf, false);
}

/* One frame of the real menu, which is what pumps a pending read. */
static void run_frame(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), MENU_ACTION_NOOP,
         cpu_features_get_time_usec());
}

/* Frames until a pending read completes.  No input of any kind. */
static unsigned run_until_loaded(void)
{
   unsigned frames = 0;
   while (playlist_init_cached_pending() && frames < 100000)
   {
      run_frame();
      frames++;
   }
   return frames;
}

/* The label of the first listed entry - which playlist is on screen.
 * The fixtures put the system in the label, the way a real playlist's
 * titles identify it. */
static const char *first_label(void)
{
   file_list_t *buf = selection_buf();
   if (!buf || !buf->size)
      return NULL;
   return buf->list[0].label ? buf->list[0].label : "";
}

/* ------------------------------------------------------------------ */
/* Lanes - driven the way a user drives the menu                      */
/* ------------------------------------------------------------------ */

/* Report 2: opening a playlist must never leave the list empty.  An
 * empty list is unrecoverable - the rebuild is gated on
 * selection_buf_size and the back action needs a selected entry - so
 * the screen becomes one the user cannot leave. */
static void lane_never_empty(void)
{
   unsigned had = failures;
   size_t n;

   CHECK(open_playlists_screen() > 0, "the Playlists screen was empty");
   CHECK(select_and_press_ok("n64.lpl"), "no n64 entry to press OK on");

   n = selection_buf() ? selection_buf()->size : 0;
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
   unsigned had = failures;
   unsigned frames;

   CHECK(open_playlists_screen() > 0, "the Playlists screen was empty");
   CHECK(select_and_press_ok("n64.lpl"), "no n64 entry to press OK on");

   frames = run_until_loaded();

   CHECK(frames > 0,
         "the read completed inside the OK press, so this lane is not "
         "exercising the pump it exists for - the VFS is not slow "
         "enough, or the playlist is not big enough");
   CHECK(!playlist_init_cached_pending(),
         "still pending after %u frames - a read that yields never "
         "completes without input", frames);
   CHECK(selection_buf() && selection_buf()->size > 1,
         "after the read completed the list still holds %u entries - "
         "the pump did not rebuild it",
         selection_buf() ? (unsigned)selection_buf()->size : 0);

   if (failures == had)
      fprintf(stderr, "[pass] frames-finish-the-read lane (%u frames)\n",
            frames);
}

/* Report 1: open one playlist, go back, open another - what is on
 * screen must be what was asked for, not the previous playlist's
 * entries.  The reported sequence, pressed rather than simulated. */
static void lane_switch_shows_requested(void)
{
   unsigned had = failures;
   const char *lbl;

   CHECK(open_playlists_screen() > 0, "the Playlists screen was empty");
   CHECK(select_and_press_ok("n64.lpl"), "no n64 entry");
   run_until_loaded();
   lbl = first_label();
   CHECK(lbl && strstr(lbl, "n64"),
         "the N64 playlist listed \"%s\"", lbl ? lbl : "(empty)");

   /* Back to Playlists, then open the other one. */
   CHECK(open_playlists_screen() > 0, "back to Playlists failed");
   CHECK(select_and_press_ok("nes.lpl"), "no nes entry");

   /* Checked HERE, before the read has finished: this is the window
    * the bug lived in.  For however many frames the load takes, the
    * screen must not still be showing the playlist the user just
    * navigated away from - that is what was reported, and waiting
    * for the load to finish before looking would miss it entirely. */
   lbl = first_label();
   CHECK(!lbl || !strstr(lbl, "n64"),
         "while the NES playlist was still loading the list showed "
         "\"%s\" - the previous playlist's entries, which is what "
         "the user sees for the whole of that load", lbl);

   run_until_loaded();

   lbl = first_label();
   CHECK(lbl && strstr(lbl, "nes"),
         "after switching to the NES playlist the list showed \"%s\" "
         "- the previous playlist's entries under the new heading, "
         "and selecting one would launch the wrong system's content",
         lbl ? lbl : "(empty)");

   /* And back again, the way a user cycles between two systems. */
   CHECK(open_playlists_screen() > 0, "back to Playlists failed");
   CHECK(select_and_press_ok("n64.lpl"), "no n64 entry");
   run_until_loaded();
   lbl = first_label();
   CHECK(lbl && strstr(lbl, "n64"),
         "switching back showed \"%s\"", lbl ? lbl : "(empty)");

   if (failures == had)
      fprintf(stderr, "[pass] switch-shows-requested lane\n");
}

/* Issue #19427: a playlist recording a base_content_directory,
 * opened with portable paths off, must load once and STAY loaded.
 * The reuse check compared the recorded base unconditionally, so
 * the rebuild after every completed read freed the playlist just
 * installed and began the read again - the placeholder flashing
 * once per completion.  In the broken state the parse is pending
 * again by the time each completing frame returns, so the read
 * never observably finishes, which the capped runner turns into a
 * failure. */
static void lane_based_playlist_loads_once(void)
{
   unsigned had     = failures;
   unsigned frames  = 0;
   unsigned repends = 0;
   unsigned i;

   CHECK(open_playlists_screen() > 0, "the Playlists screen was empty");
   CHECK(select_and_press_ok("based.lpl"), "no based entry to press OK on");

   CHECK(playlist_init_cached_pending(),
         "the read completed inside the OK press, so this lane is not "
         "exercising the pump it exists for");

   while (playlist_init_cached_pending() && frames < 600)
   {
      run_frame();
      frames++;
   }

   CHECK(!playlist_init_cached_pending(),
         "still pending after %u frames: the rebuild that follows every "
         "completed read is starting the read over - the #19427 restart "
         "cycle", frames);

   /* Stability: in the restart cycle every frame re-begins the read. */
   for (i = 0; i < 30; i++)
   {
      run_frame();
      if (playlist_init_cached_pending())
         repends++;
   }
   CHECK(repends == 0,
         "the read came back %u times after completing - one placeholder "
         "flash each", repends);
   CHECK(selection_buf() && selection_buf()->size > 1,
         "after the read completed the list holds %u entries",
         selection_buf() ? (unsigned)selection_buf()->size : 0);

   if (failures == had)
      fprintf(stderr,
            "[pass] based-playlist-loads-once lane (%u frames, 0 repends)\n",
            frames);
}

/* ------------------------------------------------------------------ */

/* Regression: a saved Explore view pressed on the Playlists screen
 * must push the deferred Explore list carrying the view's .lvw path.
 * These entries are appended under MENU_ENUM_LABEL_GOTO_EXPLORE with
 * the .lvw path in the LABEL slot (explore_get_view_path() reads it
 * back off the stack), so any dispatch that re-resolves the entry by
 * its label string misses every ok_dl_map row and falls through to
 * the archive/file browser - the menu came up showing root drives.
 * Broken for every driver without a sidebar; XMB/Ozone horizontal
 * lists never route through this callback, which is why only they
 * still worked. */
static void lane_saved_view_opens_explore(void)
{
#ifdef HAVE_LIBRETRODB
   unsigned had               = failures;
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);
   struct item_file *top      = NULL;

   CHECK(open_playlists_screen() > 0, "the Playlists screen was empty");
   CHECK(press_ok_on_match(".lvw", true),
         "no saved-view entry listed - the .lvw fixture was not "
         "picked up, or menu_content_show_explore is off");

   top = &menu_stack->list[menu_stack->size - 1];
   CHECK(top->label && string_is_equal(top->label,
            MENU_ENUM_LABEL_DEFERRED_EXPLORE_LIST_STR),
         "pressing a saved Explore view pushed \"%s\" instead of the "
         "deferred Explore list - the dispatcher resolved the entry "
         "by its label string, which for a saved view is the .lvw "
         "path, and fell through to the archive/file browser",
         top->label ? top->label : "(null)");
   CHECK(top->path && string_is_equal(top->path, path_lvw),
         "the pushed list carries \"%s\", not the view's .lvw path - "
         "explore_get_view_path() cannot load the view from it",
         top->path ? top->path : "(null)");
   CHECK(top->type == MENU_EXPLORE_TAB,
         "the pushed list has type %u, not MENU_EXPLORE_TAB - "
         "menu_displaylist_explore() will not treat it as a view",
         top->type);

   /* The plain Explore entry has the canonical label and no live
    * .lvw path; it must still resolve to the same list, and with
    * the label the view-path check downstream excludes. */
   CHECK(open_playlists_screen() > 0, "back to Playlists failed");
   CHECK(press_ok_on_match(MENU_ENUM_LABEL_GOTO_EXPLORE_STR, true),
         "no plain Explore entry to press OK on");
   top = &menu_stack->list[menu_stack->size - 1];
   CHECK(top->label && string_is_equal(top->label,
            MENU_ENUM_LABEL_DEFERRED_EXPLORE_LIST_STR),
         "the plain Explore entry pushed \"%s\"",
         top->label ? top->label : "(null)");
   CHECK(top->path && string_is_equal(top->path,
            MENU_ENUM_LABEL_GOTO_EXPLORE_STR),
         "the plain Explore entry pushed path \"%s\" - "
         "explore_get_view_path() would mistake it for a view",
         top->path ? top->path : "(null)");

   if (failures == had)
      fprintf(stderr, "[pass] saved-view-opens-explore lane\n");
#else
   fprintf(stderr,
         "[skip] saved-view-opens-explore lane (no HAVE_LIBRETRODB)\n");
#endif
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
   snprintf(path_based, sizeof(path_based), "%s/based.lpl", fixture_dir);
   snprintf(path_lvw, sizeof(path_lvw), "%s/Metroidvania.lvw", fixture_dir);
   if (   !write_playlist(path_n64, "/games/n64", 1500)
       || !write_playlist(path_nes, "/games/nes", 1500)
       || !write_playlist_with_base(path_based, "/games/based", 1500)
       || !write_view(path_lvw))
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
   lane_switch_shows_requested();
   lane_based_playlist_loads_once();
   lane_saved_view_opens_explore();

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
