/* core_info_get_core_updater_info() names the system a core runs, for
 * the Core Downloader's entries: "<manufacturer> <systemname>", the
 * system name alone where the manufacturer adds nothing, and nothing
 * where the info file names no system.
 *
 * Run with: make core_updater_info_test SANITIZER=address,undefined
 *           ./core_updater_info_test <writable dir>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <lists/dir_list.h>

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

static int failures = 0;

/* Writes an info file holding 'keys' and checks the system name read
 * back from it; 'want' NULL means no system name. */
static void check(const char *dir, const char *what, const char *keys,
      const char *want)
{
   char path[1100];
   FILE *f;
   core_updater_info_t *info;
   const char *got;

   snprintf(path, sizeof(path), "%s/test_libretro.info", dir);
   if (!(f = fopen(path, "wb")))
   {
      printf("FAIL %s: cannot write %s\n", what, path);
      failures++;
      return;
   }
   fputs("display_name = \"Test\"\n", f);
   fputs(keys, f);
   fclose(f);

   if (!(info = core_info_get_core_updater_info(path)))
   {
      printf("FAIL %s: info file not read\n", what);
      failures++;
      return;
   }

   got = info->system_name;
   if (want ? (got && !strcmp(got, want)) : !got)
      printf("ok   %s\n", what);
   else
   {
      printf("FAIL %s: want %s%s%s, got %s%s%s\n", what,
            want ? "\"" : "", want ? want : "NULL", want ? "\"" : "",
            got  ? "\"" : "", got  ? got  : "NULL", got  ? "\"" : "");
      failures++;
   }
   core_info_free_core_updater_info(info);
}

int main(int argc, char **argv)
{
   const char *dir;

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s <writable dir>\n", argv[0]);
      return 2;
   }
   dir = argv[1];

   check(dir, "manufacturer and system joined",
         "manufacturer = \"Watara\"\nsystemname = \"Supervision\"\n",
         "Watara Supervision");
   check(dir, "system name already starts with the manufacturer",
         "manufacturer = \"Atari\"\nsystemname = \"Atari 2600\"\n",
         "Atari 2600");
   check(dir, "system name alone",
         "systemname = \"WASM-4\"\n",
         "WASM-4");
   check(dir, "manufacturer N/A is left out",
         "manufacturer = \"N/A\"\nsystemname = \"2048 Game Clone\"\n",
         "2048 Game Clone");
   check(dir, "manufacturer Various is left out",
         "manufacturer = \"Various\"\nsystemname = \"MSX\"\n",
         "MSX");
   check(dir, "several manufacturers are left out",
         "manufacturer = \"Coleco|VTech\"\n"
         "systemname = \"ColecoVision/CreatiVision\"\n",
         "ColecoVision/CreatiVision");
   check(dir, "manufacturer alone names no system",
         "manufacturer = \"Nintendo\"\n",
         NULL);
   check(dir, "empty system name names no system",
         "manufacturer = \"Nintendo\"\nsystemname = \"\"\n",
         NULL);
   check(dir, "no keys names no system",
         "",
         NULL);

   printf("%s\n", failures ? "FAILED" : "PASSED");
   return failures ? 1 : 0;
}
