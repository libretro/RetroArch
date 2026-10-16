/* Load Content lists every file some installed core or a built-in
 * player opens, however many extensions the installed cores claim
 * together.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * boots the frontend with no core and the null drivers. A scratch
 * core directory holds CORES cores whose info files claim EXTS_PER
 * extensions each, together far past 2 KiB. Load Content's start
 * directory is opened the way the menu opens it, and every file there
 * must be listed: one per core extension, an image with the image
 * viewer built in and the movies the menu previews with the in-tree
 * demuxers. One core loads directories, which does not make the
 * start directory loadable content while no core is running.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/load_content_filter/build.sh
 *   samples/menu/load_content_filter/load_content_filter_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <lists/file_list.h>
#include <time/rtime.h>
#include <queues/task_queue.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../command.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../verbosity.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../msg_hash.h"
#include "../../../msg_hash_lbl_str.h"

#define CORES    40
#define EXTS_PER 15

static unsigned failures = 0;

#define CHECK(cond, what) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what); \
         failures++; \
      } \
   } while (0)

static char dir[400];
static char dir_cores[500];
static char dir_info[500];
static char dir_content[500];

static void pump(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      runloop_iterate();
      task_queue_check();
   }
}

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

static bool touch(const char *base, const char *name)
{
   char path[700];
   FILE *f;
   snprintf(path, sizeof(path), "%s/%s", base, name);
   if (!(f = fopen(path, "wb")))
      return false;
   fclose(f);
   return true;
}

static void unlink_in(const char *base, const char *name)
{
   char path[700];
   snprintf(path, sizeof(path), "%s/%s", base, name);
   remove(path);
}

static bool listed(const char *name)
{
   size_t i;
   file_list_t *buf = selection_buf();
   for (i = 0; buf && i < buf->size; i++)
      if (buf->list[i].path && string_is_equal(buf->list[i].path, name))
         return true;
   return false;
}

static bool listed_label(const char *label)
{
   size_t i;
   file_list_t *buf = selection_buf();
   for (i = 0; buf && i < buf->size; i++)
      if (buf->list[i].label && string_is_equal(buf->list[i].label, label))
         return true;
   return false;
}

static void ext_name(char *s, size_t len, unsigned c, unsigned e)
{
   snprintf(s, len, "h%02uq%02u", c, e);
}

static bool write_fixture(const char *core_ext)
{
   unsigned c, e;
   char name[64];
   char ext[16];

   for (c = 0; c < CORES; c++)
   {
      char path[700];
      FILE *f;

      snprintf(name, sizeof(name), "h%02u_libretro.%s", c, core_ext);
      if (!touch(dir_cores, name))
         return false;

      snprintf(path, sizeof(path), "%s/h%02u_libretro.info", dir_info, c);
      if (!(f = fopen(path, "wb")))
         return false;
      fprintf(f, "display_name = \"Harness %02u\"\n", c);
      fprintf(f, "supported_extensions = \"");
      for (e = 0; e < EXTS_PER; e++)
      {
         ext_name(ext, sizeof(ext), c, e);
         fprintf(f, "%s%s", e ? "|" : "", ext);
      }
      fprintf(f, "\"\n");
      fprintf(f, "corename = \"Harness %02u\"\n", c);
      fclose(f);

      for (e = 0; e < EXTS_PER; e++)
      {
         ext_name(ext, sizeof(ext), c, e);
         snprintf(name, sizeof(name), "f.%s", ext);
         if (!touch(dir_content, name))
            return false;
      }
   }
   {
      char path[700];
      FILE *f;
      snprintf(name, sizeof(name), "hdir_libretro.%s", core_ext);
      if (!touch(dir_cores, name))
         return false;
      snprintf(path, sizeof(path), "%s/hdir_libretro.info", dir_info);
      if (!(f = fopen(path, "wb")))
         return false;
      fprintf(f, "display_name = \"Harness Directory\"\n");
      fprintf(f, "supported_extensions = \"/|hdirx\"\n");
      fprintf(f, "corename = \"Harness Directory\"\n");
      fclose(f);
   }
   return touch(dir_content, "picture.png")
       && touch(dir_content, "movie.webm")
       && touch(dir_content, "movie.mp4");
}

static void remove_fixture(const char *core_ext)
{
   unsigned c, e;
   char name[64];
   char ext[16];

   for (c = 0; c < CORES; c++)
   {
      snprintf(name, sizeof(name), "h%02u_libretro.%s", c, core_ext);
      unlink_in(dir_cores, name);
      snprintf(name, sizeof(name), "h%02u_libretro.info", c);
      unlink_in(dir_info, name);
      for (e = 0; e < EXTS_PER; e++)
      {
         ext_name(ext, sizeof(ext), c, e);
         snprintf(name, sizeof(name), "f.%s", ext);
         unlink_in(dir_content, name);
      }
   }
   snprintf(name, sizeof(name), "hdir_libretro.%s", core_ext);
   unlink_in(dir_cores, name);
   unlink_in(dir_info, "hdir_libretro.info");
   unlink_in(dir_content, "picture.png");
   unlink_in(dir_content, "movie.webm");
   unlink_in(dir_content, "movie.mp4");
   path_rmdir(dir_cores);
   path_rmdir(dir_info);
   path_rmdir(dir_content);
}

/* Load Content, then its start directory, the way the menu opens them */
static bool open_start_directory(void)
{
   menu_displaylist_info_t info;
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = selection_buf();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);
   size_t i;

   if (!buf || !menu_stack)
      return false;
   menu_entries_clear(buf);
   menu_entries_append(menu_stack, "",
         MENU_ENUM_LABEL_LOAD_CONTENT_LIST_STR,
         MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
         MENU_SETTING_ACTION, 0, 0, NULL);
   menu_displaylist_info_init(&info);
   info.list     = buf;
   info.path     = strdup("");
   info.label    = strdup(MENU_ENUM_LABEL_LOAD_CONTENT_LIST_STR);
   info.enum_idx = MENU_ENUM_LABEL_LOAD_CONTENT_LIST;
   info.type     = MENU_SETTING_ACTION;
   menu_displaylist_ctl(DISPLAYLIST_LOAD_CONTENT_LIST, &info,
         config_get_ptr());
   menu_displaylist_process(&info);
   menu_displaylist_info_free(&info);

   for (i = 0; i < buf->size; i++)
   {
      if (     buf->list[i].label
            && string_is_equal(buf->list[i].label,
               MENU_ENUM_LABEL_FAVORITES_STR))
      {
         menu_entry_t entry;
         menu_st->selection_ptr = i;
         MENU_ENTRY_INITIALIZE(entry);
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

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char core_ext[32];
   char *rarch_argv[8] = {0};
   int rarch_argc      = 0;
   unsigned c, e, frames, missing = 0;
   char name[64];
   char ext[16];
   FILE *f;
   (void)argc;
   (void)argv;

   {
      const char *tmp = getenv("TMPDIR");
      if (!tmp || !*tmp)
         tmp = getenv("TEMP");
      if (!tmp || !*tmp)
         tmp = "/tmp";
      snprintf(dir, sizeof(dir), "%s/load_content_filter_%ld",
            tmp, (long)getpid());
   }
   snprintf(dir_cores,   sizeof(dir_cores),   "%s/cores",   dir);
   snprintf(dir_info,    sizeof(dir_info),    "%s/info",    dir);
   snprintf(dir_content, sizeof(dir_content), "%s/content", dir);
   if (     !path_mkdir(dir_cores)
         || !path_mkdir(dir_info)
         || !path_mkdir(dir_content))
      return 1;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   runloop_msg_queue_init();

   if (!frontend_driver_get_core_extension(core_ext, sizeof(core_ext)))
      strlcpy(core_ext, "so", sizeof(core_ext));
   if (!write_fixture(core_ext))
   {
      fprintf(stderr, "FAIL: could not write the fixture\n");
      return 1;
   }

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if (!(f = fopen(cfg_path, "wb")))
      return 1;
   fprintf(f, "video_driver = \"null\"\n");
   fprintf(f, "audio_driver = \"null\"\n");
   fprintf(f, "input_driver = \"null\"\n");
   fprintf(f, "input_joypad_driver = \"null\"\n");
   fprintf(f, "menu_driver = \"rgui\"\n");
   fprintf(f, "video_threaded = \"false\"\n");
   fprintf(f, "config_save_on_exit = \"false\"\n");
   fprintf(f, "libretro_directory = \"%s\"\n", dir_cores);
   fprintf(f, "libretro_info_path = \"%s\"\n", dir_info);
   fprintf(f, "rgui_browser_directory = \"%s\"\n", dir_content);
   fprintf(f, "core_info_cache_enable = \"false\"\n");
   fprintf(f, "builtin_imageviewer_enable = \"true\"\n");
   fprintf(f, "menu_navigation_browser_filter_supported_extensions_enable = \"true\"\n");
   fclose(f);

   if (getenv("HARNESS_VERBOSE"))
      verbosity_enable();

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;
   rarch_argv[rarch_argc++] = (char*)"--menu";
   if (getenv("HARNESS_VERBOSE"))
      rarch_argv[rarch_argc++] = (char*)"-v";

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   pump(5);
   if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   pump(2);

   CHECK(open_start_directory(), "Load Content has no start directory entry");

   /* The walk may finish in the background */
   for (frames = 0; frames < 2000 && !listed("f.h00q00"); frames++)
      pump(1);
   pump(2);

   for (c = 0; c < CORES; c++)
      for (e = 0; e < EXTS_PER; e++)
      {
         ext_name(ext, sizeof(ext), c, e);
         snprintf(name, sizeof(name), "f.%s", ext);
         if (!listed(name))
         {
            if (!missing)
               fprintf(stderr, "FAIL: %s is not listed\n", name);
            missing++;
         }
      }
   if (missing)
   {
      fprintf(stderr, "FAIL: %u of %u core extensions' files are not "
            "listed\n", missing, CORES * EXTS_PER);
      failures++;
   }
#ifdef HAVE_IMAGEVIEWER
   CHECK(listed("picture.png"),
         "an image is not listed while the image viewer is built in");
#endif
   CHECK(!listed_label(MENU_ENUM_LABEL_USE_THIS_DIRECTORY_STR),
         "the start directory offers Use This Directory with no core "
         "running");
#ifdef HAVE_RWEBM
   CHECK(listed("movie.webm"), "a WebM movie is not listed");
#endif
#ifdef HAVE_RMP4
   CHECK(listed("movie.mp4"), "an MP4 movie is not listed");
#endif

   main_exit(NULL);

   remove(cfg_path);
   remove_fixture(core_ext);
   path_rmdir(dir);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "[pass] load_content_filter\n");
   return 0;
}
