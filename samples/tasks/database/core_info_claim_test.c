/* core_info_database_claim() on scan threads while the main thread
 * takes the core list down and builds it again, as closing content
 * does under a running scan. Every claim sees a whole list or none,
 * and no list outlives the last claim on it: ASan sees a list freed
 * under a claim, LSan one never freed, TSan a claim that races the
 * teardown.
 *
 * Run with: make core_info_claim_test SANITIZER=address,undefined
 *           ./core_info_claim_test <writable dir>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

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

#define READERS 4
#define CYCLES  400

static char info_dir[1024];
static char core_dir[1024];
static retro_atomic_int_t g_stop;
static retro_atomic_int_t g_claims;
static retro_atomic_int_t g_whole;
static retro_atomic_int_t g_bad;

static bool write_file(const char *dir, const char *name, const char *text)
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

static void list_up(void)
{
   bool cache_supported = false;
   core_info_init_list(info_dir, core_dir, "so", true, false,
         &cache_supported);
}

/* One claim: the core names "Test Alpha" with bin and cue, so a claim
 * gets both or, with the list down, nothing. */
static void claim_once(void)
{
   bool member              = false;
   struct string_list *exts = core_info_database_claim(
         "/db/Test Alpha.rdb", &member);
   retro_atomic_fetch_add_int(&g_claims, 1);
   if (!exts)
      return;
   if (     exts->size == 2
         && string_list_find_elem(exts, "bin")
         && string_list_find_elem(exts, "cue"))
      retro_atomic_fetch_add_int(&g_whole, 1);
   else
      retro_atomic_fetch_add_int(&g_bad, 1);
   string_list_free(exts);
}

#ifdef HAVE_THREADS
static void reader(void *data)
{
   (void)data;
   while (!retro_atomic_load_acquire_int(&g_stop))
      claim_once();
}

/* A lost list or a stuck claim fails here instead of stalling CI. */
static void watchdog(void *data)
{
   int i;
   (void)data;
   for (i = 0; i < 600; i++)
   {
      if (retro_atomic_load_acquire_int(&g_stop) == 2)
         return;
      retro_sleep(100);
   }
   fprintf(stderr, "FAIL  core_info_claim: no progress in 60 s\n");
   exit(1);
}
#endif

int main(int argc, char **argv)
{
   int i;
#ifdef HAVE_THREADS
   sthread_t *t[READERS];
   sthread_t *dog;
#endif

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s <writable dir>\n", argv[0]);
      return 2;
   }
   snprintf(info_dir, sizeof(info_dir), "%s/claim_info", argv[1]);
   snprintf(core_dir, sizeof(core_dir), "%s/claim_cores", argv[1]);
   path_mkdir(info_dir);
   path_mkdir(core_dir);
   if (   !write_file(info_dir, "claim_libretro.info",
            "display_name = \"Claim Test\"\n"
            "corename = \"ClaimTest\"\n"
            "supported_extensions = \"bin|cue\"\n"
            "database = \"Test Alpha\"\n")
       || !write_file(core_dir, "claim_libretro.so", "\177ELF"))
   {
      fprintf(stderr, "FAIL  core_info_claim: fixture\n");
      return 1;
   }

   list_up();
   claim_once();
   if (retro_atomic_load_acquire_int(&g_whole) != 1)
   {
      fprintf(stderr, "FAIL  core_info_claim: the fixture core claims nothing\n");
      return 1;
   }

#ifdef HAVE_THREADS
   dog = sthread_create(watchdog, NULL);
   for (i = 0; i < READERS; i++)
      t[i] = sthread_create(reader, NULL);
#endif
   for (i = 0; i < CYCLES; i++)
   {
      int j;
      core_info_deinit_list();
      list_up();
      /* Keeps the list up a while, for the readers to hold it. */
      for (j = 0; j < 20; j++)
         claim_once();
   }
   retro_atomic_store_release_int(&g_stop, 1);
#ifdef HAVE_THREADS
   for (i = 0; i < READERS; i++)
      if (t[i])
         sthread_join(t[i]);
#endif
   core_info_deinit_list();
   claim_once();
#ifdef HAVE_THREADS
   retro_atomic_store_release_int(&g_stop, 2);
   if (dog)
      sthread_join(dog);
#endif

   if (retro_atomic_load_acquire_int(&g_bad))
   {
      fprintf(stderr, "FAIL  core_info_claim: %d of %d claims saw part of a list\n",
            retro_atomic_load_acquire_int(&g_bad),
            retro_atomic_load_acquire_int(&g_claims));
      return 1;
   }
   printf("ok    core_info_claim: %d claims (%d whole) across %d list rebuilds\n",
         retro_atomic_load_acquire_int(&g_claims),
         retro_atomic_load_acquire_int(&g_whole), CYCLES);
   return 0;
}
