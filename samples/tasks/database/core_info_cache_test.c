/* Core info cache: a late info file is read, and a rebuild keeps every cached entry whole */

/* Run with: make core_info_cache_test && ./core_info_cache_test <writable dir> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <stdint.h>

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

/* The cache is replaced by a rename, so a rewrite shows as a new inode */
static unsigned long cache_ino(void)
{
   char p[1100];
   struct stat st;
   snprintf(p, sizeof(p), "%s/core_info.cache", dir);
   if (stat(p, &st) != 0)
      return 0;
   return (unsigned long)st.st_ino;
}

static void use_dir(const char *root, const char *sub)
{
   snprintf(dir, sizeof(dir), "%s/%s", root, sub);
   path_mkdir(dir);
}

static void remove_file(const char *name)
{
   char p[1100];
   snprintf(p, sizeof(p), "%s/%s", dir, name);
   remove(p);
}

static void build_list(void)
{
   bool cache_supported = false;
   core_info_deinit_list();
   core_info_init_list(dir, dir, "so", true, true, &cache_supported);
}

#define MANY_CORES 20

/* More cores than the cache list starts with and long firmware lists:
 * the second start is served from the cache alone (the info files are
 * gone by then), and must give back what the info files said, each
 * core at its own path. */
static int check_many_cores(const char *root)
{
   int c, f, pass;
   char name[64], text[4096], core_path[1100];

   use_dir(root, "many");
   for (c = 0; c < MANY_CORES; c++)
   {
      size_t _len;
      snprintf(name, sizeof(name), "c%02d_libretro.so", c);
      if (!write_file(name, "\177ELF"))
         return 1;
      _len = (size_t)snprintf(text, sizeof(text),
            "display_name = \"Core %d\"\nsupported_extensions = \"a%d|b\"\n"
            "firmware_count = %d\n", c, c, c % 7);
      for (f = 0; f < c % 7; f++)
         _len += (size_t)snprintf(text + _len, sizeof(text) - _len,
               "firmware%d_path = \"bios/c%d_%d.bin\"\n"
               "firmware%d_desc = \"BIOS %d of core %d\"\n"
               "firmware%d_opt = \"%s\"\n",
               f, c, f, f, f, c, f, (f & 1) ? "true" : "false");
      snprintf(name, sizeof(name), "c%02d_libretro.info", c);
      if (!write_file(name, text))
         return 1;
   }

   for (pass = 0; pass < 2; pass++)
   {
      build_list();
      for (c = 0; c < MANY_CORES; c++)
      {
         char want[64];
         core_info_t *info = NULL;
         snprintf(core_path, sizeof(core_path), "%s/c%02d_libretro.so", dir, c);
         snprintf(want, sizeof(want), "Core %d", c);
         if (     !core_info_find(core_path, &info) || !info
               || !info->path || strcmp(info->path, core_path)
               || !info->display_name || strcmp(info->display_name, want)
               || info->firmware_count != (size_t)(c % 7))
         {
            fprintf(stderr, "FAIL  core_info_cache: core %d wrong on %s start\n",
                  c, pass ? "cached" : "first");
            return 1;
         }
         for (f = 0; f < c % 7; f++)
         {
            char fw_path[64], fw_desc[64];
            snprintf(fw_path, sizeof(fw_path), "bios/c%d_%d.bin", c, f);
            snprintf(fw_desc, sizeof(fw_desc), "BIOS %d of core %d", f, c);
            if (     !info->firmware[f].path || strcmp(info->firmware[f].path, fw_path)
                  || !info->firmware[f].desc || strcmp(info->firmware[f].desc, fw_desc)
                  || info->firmware[f].optional != ((f & 1) ? true : false))
            {
               fprintf(stderr, "FAIL  core_info_cache: core %d firmware %d wrong on %s start\n",
                     c, f, pass ? "cached" : "first");
               return 1;
            }
         }
      }
      if (!pass)
         for (c = 0; c < MANY_CORES; c++)
         {
            snprintf(name, sizeof(name), "c%02d_libretro.info", c);
            remove_file(name);
         }
   }
   core_info_deinit_list();
   printf("ok    core_info_cache: %d cores and their firmware come back whole from the cache\n",
         MANY_CORES);
   return 0;
}

static uint32_t id_hash(const char *s)
{
   unsigned char ch;
   uint32_t hash = (uint32_t)0x811c9dc5;
   while ((ch = (unsigned char)*(s++)) != '\0')
      hash = ((hash * (uint32_t)0x01000193) ^ (uint32_t)ch);
   return hash ? hash : 1;
}

/* A cache cut off inside an entry's firmware list is thrown away
 * without leaking the half-read entry, and the info file is read. */
static int check_truncated_cache(const char *root)
{
   char text[1024], core_path[1100];
   core_info_t *info = NULL;

   use_dir(root, "truncated");
   if (     !write_file("t_libretro.so", "\177ELF")
         || !write_file("t_libretro.info", "display_name = \"From Info\"\n"))
      return 1;
   snprintf(text, sizeof(text),
         "{\n  \"version\": \"1.2\",\n  \"items\": [\n"
         "    {\n      \"display_name\": \"Whole\",\n"
         "      \"core_file_id\": { \"str\": \"other_libretro\", \"hash\": %u }\n    },\n"
         "    {\n      \"display_name\": \"Cut\",\n"
         "      \"core_file_id\": { \"str\": \"t_libretro\", \"hash\": %u },\n"
         "      \"firmware\": [ { \"path\": \"a.bin\", \"desc\": \"A\" }, { \"path\": \"b",
         (unsigned)id_hash("other_libretro"), (unsigned)id_hash("t_libretro"));
   if (!write_file("core_info.cache", text))
      return 1;

   build_list();
   snprintf(core_path, sizeof(core_path), "%s/t_libretro.so", dir);
   if (     !core_info_find(core_path, &info) || !info
         || !info->display_name || strcmp(info->display_name, "From Info"))
   {
      fprintf(stderr, "FAIL  core_info_cache: a cut-off cache was trusted\n");
      return 1;
   }
   core_info_deinit_list();
   printf("ok    core_info_cache: a cache cut off inside an entry is discarded\n");
   return 0;
}

int main(int argc, char **argv)
{
   int i;
   unsigned long ino;
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

   /* Settled: the next start is served from the cache and writes nothing */
   ino = cache_ino();
   list_up();
   if (ino != cache_ino())
   {
      fprintf(stderr, "FAIL  core_info_cache: an unchanged core set rewrote the cache\n");
      return 1;
   }

   core_info_deinit_list();
   printf("ok    core_info_cache: a late info file is read, and rebuilds keep the cache whole\n");

   if (check_many_cores(argv[1]) || check_truncated_cache(argv[1]))
      return 1;
   return 0;
}
