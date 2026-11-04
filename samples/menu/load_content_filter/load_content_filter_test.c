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
 * Content Scan's Content Location lists folders and files: every
 * file in Automatic, which scans every file, and the scan's
 * extensions in Custom. OK on any file, an image too, sets the
 * location to it. The Apple file pickers set it only while the browser
 * or a folder entered from it is open, not after Back. Import Content
 * offers Content Scan alone.
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
#include "../../../manual_content_scan.h"
#include "../../../menu/menu_setting.h"
#include "../../../menu/menu_cbs.h"

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

/* A list pushed the way the menu pushes it */
static bool open_list(const char *label, enum msg_hash_enums enum_idx,
      enum menu_displaylist_ctl_state dl_type)
{
   menu_displaylist_info_t info;
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = selection_buf();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);

   if (!buf || !menu_stack)
      return false;
   menu_entries_clear(buf);
   menu_entries_append(menu_stack, "", label, enum_idx,
         MENU_SETTING_ACTION, 0, 0, NULL);
   menu_displaylist_info_init(&info);
   info.list     = buf;
   info.path     = strdup("");
   info.label    = strdup(label);
   info.enum_idx = enum_idx;
   info.type     = MENU_SETTING_ACTION;
   menu_displaylist_ctl(dl_type, &info, config_get_ptr());
   menu_displaylist_process(&info);
   menu_displaylist_info_free(&info);
   return true;
}

static void act_on(size_t i, enum menu_action action)
{
   menu_entry_t entry;
   menu_state_get_ptr()->selection_ptr = i;
   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                | MENU_ENTRY_FLAG_LABEL_ENABLED
                | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                | MENU_ENTRY_FLAG_VALUE_ENABLED
                | MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
   menu_entry_get(&entry, 0, i, NULL, true);
   menu_entry_action(&entry, i, action);
}

/* OK on the first entry whose label (or path) is s */
static bool ok_on(const char *s, bool by_label)
{
   file_list_t *buf = selection_buf();
   size_t i;

   for (i = 0; buf && i < buf->size; i++)
   {
      const char *v = by_label ? buf->list[i].label : buf->list[i].path;
      if (v && string_is_equal(v, s))
      {
         act_on(i, MENU_ACTION_OK);
         return true;
      }
   }
   return false;
}

/* Back from the current list, once it lists entries */
static bool back(void)
{
   file_list_t *buf = selection_buf();
   if (!buf || !buf->size)
      return false;
   act_on(0, MENU_ACTION_CANCEL);
   pump(2);
   return true;
}

/* The current menu; a NULL path matches any */
static bool top_is(const char *path, const char *label)
{
   const char *top_path  = NULL;
   const char *top_label = NULL;
   menu_entries_get_last_stack(&top_path, &top_label, NULL, NULL, NULL);
   return (!path || string_is_equal(top_path, path))
       && string_is_equal(top_label, label);
}

/* Load Content, then its start directory, the way the menu opens them */
static bool open_start_directory(void)
{
   return open_list(MENU_ENUM_LABEL_LOAD_CONTENT_LIST_STR,
               MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
               DISPLAYLIST_LOAD_CONTENT_LIST)
       && ok_on(MENU_ENUM_LABEL_FAVORITES_STR, true);
}

static bool open_content_scan(void)
{
   return open_list(MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST_STR,
         MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST,
         DISPLAYLIST_MANUAL_CONTENT_SCAN_LIST);
}

/* The walk may finish in the background */
static void wait_listed(const char *name)
{
   unsigned frames;
   for (frames = 0; frames < 2000 && !listed(name); frames++)
      pump(1);
   pump(2);
}

/* Content Location's list of places, once listed */
static bool open_content_location_list(void)
{
   unsigned frames;
   if (   !open_content_scan()
       || !ok_on(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR, true))
      return false;
   for (frames = 0; frames < 200
         && !(   top_is(NULL,
                  MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_DIR_LIST_STR)
              && listed_label(MENU_ENUM_LABEL_FAVORITES_STR)); frames++)
      pump(1);
   return top_is(NULL,
            MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_DIR_LIST_STR)
       && listed_label(MENU_ENUM_LABEL_FAVORITES_STR);
}

/* The list's entry for the stored location */
static bool open_content_location(void)
{
   return open_content_location_list()
       && ok_on(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR, true);
}

/* The current list's selected entry is s */
static bool selected_is(const char *s)
{
   file_list_t *buf = selection_buf();
   size_t i         = menu_state_get_ptr()->selection_ptr;
   return buf && i < buf->size && string_is_equal(buf->list[i].path, s);
}

static void content_location_lanes(void)
{
   char sub[600];
   char sub_dir[700];
   char inner[700];
   char start[700];
   char want[700];
   char image[700];
   const char *use_dir =
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY);

   snprintf(sub,   sizeof(sub),   "%s/sub",           dir_content);
   snprintf(sub_dir, sizeof(sub_dir), "%s/",          sub);
   snprintf(inner, sizeof(inner), "%s/inner.zzz",     sub);
   snprintf(start, sizeof(start), "%s/f.h00q00",      dir_content);
   snprintf(want,  sizeof(want),  "%s/unclaimed.zzz", dir_content);
   snprintf(image, sizeof(image), "%s/picture.png",   dir_content);
   if (   !path_mkdir(sub) || !touch(dir_content, "unclaimed.zzz")
       || !touch(sub, "inner.zzz"))
   {
      CHECK(0, "could not write the Content Location fixture");
      return;
   }

   /* Load Content's places, less its settings shortcuts */
   manual_content_scan_set_menu_content_dir("");
   CHECK(   open_content_location_list()
         && listed_label(
            MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST_STR)
         && !listed_label(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR)
         && !listed_label(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS_STR),
         "Content Location does not list Load Content's places");
   CHECK(   back()
         && top_is(NULL,
            MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST_STR)
         && !action_ok_manual_content_scan_dir_browsing(),
         "Back from Content Location's list does not return to "
         "Content Scan");

   /* A stored location comes first; the other places open at their
    * own folder, and Back returns to the list */
   manual_content_scan_set_menu_content_dir(inner);
   CHECK(   open_content_location_list()
         && string_is_equal(selection_buf()->list[0].label,
            MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR),
         "Content Location's list does not start with the stored "
         "location");
   CHECK(ok_on(MENU_ENUM_LABEL_FAVORITES_STR, true),
         "Content Location's list has no Start Directory");
   wait_listed("unclaimed.zzz");
   CHECK(   top_is(dir_content, MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR)
         && action_ok_manual_content_scan_dir_browsing(),
         "Start Directory does not open Content Location's browser at "
         "the start directory");
   CHECK(   back()
         && top_is(NULL,
            MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_DIR_LIST_STR)
         && action_ok_manual_content_scan_dir_browsing(),
         "Back from a place does not return to Content Location's list");
   CHECK(ok_on(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST_STR, true),
         "Content Location's list has no Downloaded Content");
   wait_listed("content");
   CHECK(top_is(dir, MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR),
         "Downloaded Content does not open Content Location's browser "
         "at the downloads directory");
   CHECK(back(), "no Back from Downloaded Content");
   CHECK(ok_on(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR, true),
         "Content Location's list has no stored location entry");
   wait_listed("inner.zzz");
   CHECK(   top_is(sub_dir, MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR)
         && selected_is("inner.zzz"),
         "the stored location's entry does not reopen it");
   CHECK(back(), "no Back from the stored location");

   /* Start on the stored entry forgets it */
   act_on(0, MENU_ACTION_START);
   pump(2);
   CHECK(   !*manual_content_scan_get_content_dir_ptr()
         && !listed_label(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR),
         "Content Location's list keeps a location that was reset");
   pump(2);

#ifdef HAVE_SMBCLIENT
   /* A stored SMB root reopens at itself, not at the server above it */
   {
      settings_t *settings = config_get_ptr();
      bool smb_enable      = settings->bools.smb_client_enable;

      settings->bools.smb_client_enable = true;
      strlcpy(settings->arrays.smb_client_server_address, "127.0.0.1",
            sizeof(settings->arrays.smb_client_server_address));
      strlcpy(settings->arrays.smb_client_share, "share",
            sizeof(settings->arrays.smb_client_share));
      manual_content_scan_set_menu_content_dir("smb://127.0.0.1/share");
      CHECK(   open_content_location()
            && top_is("smb://127.0.0.1/share",
               MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR),
            "a stored SMB root does not reopen at itself");
      pump(2);
      settings->bools.smb_client_enable             = smb_enable;
      settings->arrays.smb_client_server_address[0] = '\0';
      settings->arrays.smb_client_share[0]          = '\0';
      manual_content_scan_set_menu_content_dir("");
   }
#endif

   /* A stored file opens its folder */
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   manual_content_scan_set_menu_content_dir(start);
   CHECK(open_content_location(),
         "Content Scan has no Content Location entry");
   wait_listed("unclaimed.zzz");
   CHECK(   listed("unclaimed.zzz") && listed("f.h00q00")
         && listed("picture.png"),
         "Content Location in Automatic does not list every file");
   CHECK(listed("sub"), "Content Location does not list folders");
   CHECK(listed(use_dir),
         "Content Location does not offer Use This Directory");

   /* Where the Apple file pickers set the location */
   CHECK(action_ok_manual_content_scan_dir_browsing(),
         "Content Location's browser is not taken as open");
   CHECK(   ok_on("sub", false)
         && top_is(sub, MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR),
         "Content Location cannot enter a folder");
   wait_listed(use_dir);
   CHECK(action_ok_manual_content_scan_dir_browsing(),
         "a folder entered from Content Location is not taken as its "
         "browser");
   CHECK(back(), "no Back from a folder in Content Location");
   wait_listed("unclaimed.zzz");

   CHECK(   ok_on("unclaimed.zzz", false)
         && string_is_equal(manual_content_scan_get_content_dir_ptr(),
            want),
         "OK on a file does not make it the Content Location");
   pump(2);

   /* Images, music and movies are content too */
   CHECK(open_content_location(),
         "Content Scan has no Content Location entry");
   wait_listed("picture.png");
   CHECK(   ok_on("picture.png", false)
         && string_is_equal(manual_content_scan_get_content_dir_ptr(),
            image)
         && top_is(NULL,
            MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST_STR),
         "OK on an image does not make it the Content Location");
   pump(2);

   CHECK(open_content_location(),
         "Content Scan has no Content Location entry");
   wait_listed(use_dir);
   CHECK(   ok_on(use_dir, false)
         && string_is_equal(manual_content_scan_get_content_dir_ptr(),
            dir_content)
         && top_is(NULL,
            MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST_STR),
         "OK on Use This Directory does not make the folder the "
         "Content Location");
   pump(2);
   /* The lanes below open in the image's folder */
   manual_content_scan_set_menu_content_dir(image);

   CHECK(open_content_location(),
         "Content Scan has no Content Location entry");
   wait_listed("picture.png");
   CHECK(   back()
         && top_is(NULL,
            MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_DIR_LIST_STR)
         && back()
         && top_is(NULL,
            MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST_STR)
         && !action_ok_manual_content_scan_dir_browsing(),
         "Content Location's browser is taken as open after Back");

   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   strlcpy(manual_content_scan_get_file_exts_custom_ptr(), "h00q00",
         manual_content_scan_get_file_exts_custom_size());
   CHECK(open_content_location(),
         "Content Scan has no Content Location entry in Custom");
   wait_listed("f.h00q00");
   CHECK(   listed("f.h00q00") && !listed("f.h01q00")
         && !listed("unclaimed.zzz"),
         "Content Location in Custom does not list the scan's "
         "extensions alone");
   pump(2);

   /* A scan of a file leaves Search Recursively as it was */
   {
      manual_content_scan_task_config_t *cfg =
         (manual_content_scan_task_config_t*)calloc(1, sizeof(*cfg));

      *manual_content_scan_get_search_recursively_ptr() = true;
      manual_content_scan_set_menu_content_dir(want);
      CHECK(   cfg
            && manual_content_scan_get_task_config(cfg, dir)
            && cfg->content_is_file
            && *manual_content_scan_get_search_recursively_ptr(),
            "a scan of a file turns Search Recursively off");
      free(cfg);
   }

   /* A file deeper than a folder-sized buffer is kept whole */
   {
      char   deep[PATH_MAX_LENGTH];
      char   file[PATH_MAX_LENGTH];
      char   seg[201];
      size_t len;
      int    depth = 0;
      FILE  *f     = NULL;

      memset(seg, 'd', sizeof(seg) - 1);
      seg[sizeof(seg) - 1] = '\0';
      len = strlcpy(deep, dir_content, sizeof(deep));
      while (   len < DIR_MAX_LENGTH + 64
             && len + sizeof(seg) + 16 < sizeof(deep))
      {
         len = fill_pathname_join_special(deep, deep, seg, sizeof(deep));
         if (!path_mkdir(deep))
            break;
         depth++;
      }
      fill_pathname_join_special(file, deep, "deep.zzz", sizeof(file));
      if (len >= DIR_MAX_LENGTH + 64 && (f = fopen(file, "wb")))
         fclose(f);
      CHECK(f != NULL, "could not write the deep Content Location fixture");
      CHECK(   manual_content_scan_set_menu_content_dir(file)
            && string_is_equal(manual_content_scan_get_content_dir_ptr(),
               file),
            "a file path longer than a folder buffer is cut short");
      remove(file);
      while (depth--)
      {
         path_rmdir(deep);
         path_parent_dir(deep, strlen(deep));
      }
   }

   /* No Scan Single File, even with Search Recursively off */
   *manual_content_scan_get_search_recursively_ptr() = false;
   CHECK(open_content_scan(), "Content Scan did not open");
   CHECK(   listed_label(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR_STR)
         && !listed_label("scan_single_file")
         && !menu_setting_find("scan_single_file"),
         "Content Scan still has Scan Single File");
   *manual_content_scan_get_search_recursively_ptr() = true;
   manual_content_scan_get_file_exts_custom_ptr()[0] = '\0';
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);

   CHECK(open_list(MENU_ENUM_LABEL_ADD_CONTENT_LIST_STR,
            MENU_ENUM_LABEL_ADD_CONTENT_LIST,
            DISPLAYLIST_ADD_CONTENT_LIST),
         "Import Content did not open");
#ifdef HAVE_LIBRETRODB
   CHECK(   listed_label(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST_STR)
         && listed_label(MENU_ENUM_LABEL_SCAN_FILE_STR)
         && listed_label(MENU_ENUM_LABEL_SCAN_DIRECTORY_STR),
         "Import Content does not offer Scan Directory, Scan File "
         "and Content Scan");

   /* Scan File: OK on a file scans that file */
   manual_content_scan_set_menu_content_dir(dir_content);
   CHECK(ok_on(MENU_ENUM_LABEL_SCAN_FILE_STR, true),
         "Import Content has no Scan File entry");
   wait_listed("unclaimed.zzz");
   CHECK(   ok_on("unclaimed.zzz", false)
         && string_is_equal(manual_content_scan_get_content_dir_ptr(),
            want),
         "OK on a file in Scan File does not scan it");
   task_queue_reset();
   pump(2);
#else
   CHECK(listed_label(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST_STR),
         "Import Content does not offer Content Scan");
#endif

   unlink_in(dir_content, "unclaimed.zzz");
   unlink_in(sub, "inner.zzz");
   path_rmdir(sub);
}

#ifdef HAVE_NFSCLIENT
/* The configured NFS server: its root, the levels above it that Parent
 * Directory skips, its titles with the export, and its entry in a file
 * browser's drive list. Pure string work over the settings; nothing
 * here reaches a server. */
static void nfs_root_set(const char *server, const char *export_p,
      const char *subdir)
{
   settings_t *settings = config_get_ptr();
   strlcpy(settings->arrays.nfs_server, server,
         sizeof(settings->arrays.nfs_server));
   strlcpy(settings->arrays.nfs_export, export_p,
         sizeof(settings->arrays.nfs_export));
   strlcpy(settings->arrays.nfs_subdir, subdir,
         sizeof(settings->arrays.nfs_subdir));
}

static bool nfs_title_is(const char *path, const char *want)
{
   char s[PATH_MAX_LENGTH];
   return menu_displaylist_nfs_title_path(s, sizeof(s), path)
       && string_is_equal(s, want);
}

static void nfs_root_lanes(void)
{
   char root[PATH_MAX_LENGTH];
   char small[16];

   nfs_root_set("", "", "");
   CHECK(!menu_displaylist_build_nfs_root(root, sizeof(root)),
         "an NFS root is built with no server set");
   CHECK(!menu_displaylist_is_network_root("nfs://127.0.0.1/"),
         "nfs:// is a configured root with no server set");

   /* the export given in the server address */
   nfs_root_set("127.0.0.1/mnt/user/roms", "", "");
   CHECK(   menu_displaylist_build_nfs_root(root, sizeof(root))
         && string_is_equal(root, "nfs://127.0.0.1/mnt/user/roms"),
         "the NFS root is not the server address");
   CHECK(menu_displaylist_is_network_root("nfs://127.0.0.1/mnt/user/roms"),
         "the server address is not the NFS root");
   CHECK(menu_displaylist_is_network_root("nfs://127.0.0.1/mnt/user/roms/"),
         "the server address with a slash is not the NFS root");
   CHECK(!menu_displaylist_is_network_root("nfs://127.0.0.1/mnt/user/roms/snes"),
         "a folder under the NFS root is taken for the root");
   CHECK(!menu_displaylist_is_network_root("nfs://127.0.0.1/mnt/user"),
         "a level above the NFS root is taken for the root");
   CHECK(!menu_displaylist_nfs_title_path(root, sizeof(root),
            "nfs://127.0.0.1/mnt/user/roms/snes"),
         "a title gains an export with none set");

   /* the export set apart, with a subdir */
   nfs_root_set("127.0.0.1", "/srv/roms/", "games");
   CHECK(   menu_displaylist_build_nfs_root(root, sizeof(root))
         && string_is_equal(root, "nfs://127.0.0.1/games"),
         "the NFS root is not the server and subdir");
   CHECK(menu_displaylist_is_network_root("nfs://127.0.0.1/"),
         "the server is not the NFS root with an export set");
   CHECK(!menu_displaylist_is_network_root("nfs://127.0.0.1/games"),
         "the subdir is taken for the NFS root");
   CHECK(!menu_displaylist_is_network_root("nfs://127.0.0.10/"),
         "another server sharing the name's start is taken for the root");
   CHECK(nfs_title_is("nfs://127.0.0.1/games/snes",
            "nfs://127.0.0.1/srv/roms/games/snes"),
         "a title does not show the export after the server");
   CHECK(nfs_title_is("nfs://127.0.0.1", "nfs://127.0.0.1/srv/roms"),
         "the server's title does not show the export");
   CHECK(!menu_displaylist_nfs_title_path(root, sizeof(root),
            "nfs://127.0.0.10/x"),
         "another server's title gains the export");
   CHECK(!menu_displaylist_nfs_title_path(root, sizeof(root),
            "smb://127.0.0.1/x"),
         "an SMB title gains the NFS export");
   CHECK(!menu_displaylist_nfs_title_path(small, sizeof(small),
            "nfs://127.0.0.1/games"),
         "a title too long for its buffer is written cut");

   /* the drive list offers the configured root */
   CHECK(   open_list("", MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY,
               DISPLAYLIST_DEFAULT)
         && listed("nfs://127.0.0.1/games"),
         "a file browser's drive list does not offer the NFS root");
   back();

   nfs_root_set("", "", "");
}
#endif

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char core_ext[32];
   char *rarch_argv[8] = {0};
   int rarch_argc      = 0;
   unsigned c, e, missing = 0;
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
   fprintf(f, "core_assets_directory = \"%s\"\n", dir);
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

   wait_listed("f.h00q00");

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
#if defined(HAVE_RPNG) && (defined(HAVE_MENU) || defined(HAVE_IMAGEVIEWER))
   CHECK(listed("picture.png"),
         "an image is not listed while the internal viewer is available");
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

   content_location_lanes();
#ifdef HAVE_NFSCLIENT
   nfs_root_lanes();
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
