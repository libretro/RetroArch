/* Core info cache: a late info file is read, and a rebuild keeps every cached entry whole */

/* Run with: make core_info_cache_test && ./core_info_cache_test <writable dir> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <lists/dir_list.h>
#include <streams/interface_stream.h>

#include "../../../core_info.h"
#include "../../../list_special.h"
#include "../../../configuration.h"
#include "../../../msg_hash.h"
#include "../../../tasks/tasks_internal.h"

/* What the linked scanner objects reach for and this test never uses. */
int msg_hash_get_help_us_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   (void)msg;
   if (s && len)
      s[0] = '\0';
   return 0;
}

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
   (void)msg;
   return "";
}

void msg_hash_us_index_init(void) { }

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration,
      bool flush, char *title, unsigned icon, unsigned category)
{
   (void)msg; (void)len; (void)prio; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
}

bool retroarch_override_setting_is_set(unsigned enum_idx, void *data)
{
   (void)enum_idx; (void)data;
   return false;
}

struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter, bool show_hidden_files)
{
   (void)input_dir; (void)type; (void)filter; (void)show_hidden_files;
   return NULL;
}

void ui_companion_driver_notify_refresh(void) { }

void task_window_progress_cb(retro_task_t *task) { (void)task; }

static char dir[1024];

static bool write_file(const char *name, const char *text)
{
   char p[1100];
   FILE *f;
   snprintf(p, sizeof(p), "%s/%s", dir, name);
   if (!(f = fopen(p, "wb")))
      return false;
   fputs(text, f);
   fclose(f);
   return true;
}

/* Builds the list with the cache on, as RetroArch does; returns the core's extensions or NULL */
static const char *list_up(void)
{
   char core_path[1100];
   bool cache_supported = false;
   core_info_t *info    = NULL;

   core_info_deinit_list();
   core_info_init_list(dir, dir, "so", true, true, &cache_supported);
   snprintf(core_path, sizeof(core_path), "%s/late_libretro.so", dir);
   if (!core_info_find(core_path, &info) || !info)
      return NULL;
   return info->supported_extensions;
}

/* True when the written cache names every core id given */
static bool cache_names(const char *a, const char *b)
{
   char p[1100];
   static char buf[65536];
   int64_t n;
   intfstream_t *f;
   snprintf(p, sizeof(p), "%s/core_info.cache", dir);
   if (!(f = intfstream_open_rzip_file(p, RETRO_VFS_FILE_ACCESS_READ)))
      return false;
   n = intfstream_read(f, buf, sizeof(buf) - 1);
   intfstream_close(f);
   free(f);
   buf[n > 0 ? n : 0] = '\0';
   return strstr(buf, a) && strstr(buf, b);
}

int main(int argc, char **argv)
{
   int i;
   const char *exts;

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s <writable dir>\n", argv[0]);
      return 2;
   }
   snprintf(dir, sizeof(dir), "%s/late_info", argv[1]);
   path_mkdir(dir);

   /* A sideloaded core with no info file: cached as such */
   if (!write_file("late_libretro.so", "\177ELF"))
   {
      fprintf(stderr, "FAIL  core_info_cache: fixture\n");
      return 1;
   }
   if ((exts = list_up()) && *exts)
   {
      fprintf(stderr, "FAIL  core_info_cache: extensions without an info file\n");
      return 1;
   }

   /* The info file arrives later, by hand */
   if (!write_file("late_libretro.info",
            "display_name = \"Late Info\"\n"
            "supported_extensions = \"cue|chd\"\n"))
   {
      fprintf(stderr, "FAIL  core_info_cache: fixture\n");
      return 1;
   }
   exts = list_up();
   if (!exts || strcmp(exts, "cue|chd"))
   {
      fprintf(stderr, "FAIL  core_info_cache: info file added later is hidden by the cache (\"%s\")\n",
            exts ? exts : "(none)");
      return 1;
   }

   /* The rewritten cache serves it from now on */
   exts = list_up();
   if (!exts || strcmp(exts, "cue|chd"))
   {
      fprintf(stderr, "FAIL  core_info_cache: rewritten cache lost the info (\"%s\")\n",
            exts ? exts : "(none)");
      return 1;
   }

   /* A new core makes the cache be rewritten; the entries served from it must survive that */
   if (!write_file("other_libretro.so", "\177ELF"))
   {
      fprintf(stderr, "FAIL  core_info_cache: fixture\n");
      return 1;
   }
   for (i = 0; i < 3; i++)
   {
      list_up();
      if (!cache_names("\"late_libretro\"", "\"other_libretro\""))
      {
         fprintf(stderr, "FAIL  core_info_cache: build %d wrote a cache that lost a core\n", i + 1);
         return 1;
      }
   }

   core_info_deinit_list();
   printf("ok    core_info_cache: a late info file is read, and rebuilds keep the cache whole\n");
   return 0;
}
