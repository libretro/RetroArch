/* The explore index is built in steps, and comes out the same as in
 * one go.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * boots the frontend on the null drivers.  A collection is written to
 * a scratch directory - playlists naming games by CRC and by label,
 * databases carrying the games' categories - and the index is built
 * two ways:
 *
 *  - oracle: menu_explore_build_list() in one go, against
 *    begin/step/end with a budget that stops after every single work
 *    item (a playlist entry, an RDB item, a category sort); the two
 *    states must hash the same and carry the same entries, and the
 *    sliced build must have taken many steps;
 *  - task: the real task_push_menu_explore_init() with Threaded Tasks
 *    off, pumped from the frame loop: the handler must return
 *    unfinished across checks (the one-shot handler finishes on its
 *    first) and the installed state must match the oracle;
 *  - abort: a build abandoned mid-way frees everything (LeakSanitizer
 *    under the ASan run is the check).
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/explore_build/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <time/rtime.h>
#include <queues/task_queue.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../command.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../verbosity.h"
#include "../../../menu/menu_driver.h"
#include "../../../tasks/tasks_internal.h"

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

/* ------------------------------------------------------------------ */
/* The collection                                                      */
/* ------------------------------------------------------------------ */

#define SYSTEMS   3
/* Per system.  Large enough that the build cannot fit the shared
 * per-frame I/O window (4 ms) in one check, so the task lane can
 * see the handler return unfinished. */
#define GAMES     4000

typedef struct { uint8_t *d; size_t len, cap; } buf_t;

static void bput(buf_t *b, const void *p, size_t n)
{
   if (b->len + n > b->cap)
   {
      size_t want = b->cap ? b->cap * 2 : 256;
      while (want < b->len + n)
         want *= 2;
      b->d   = (uint8_t*)realloc(b->d, want);
      b->cap = want;
   }
   memcpy(b->d + b->len, p, n);
   b->len += n;
}
static void bbyte(buf_t *b, uint8_t v)     { bput(b, &v, 1); }
static void bfixmap(buf_t *b, unsigned n)  { bbyte(b, (uint8_t)(0x80 | n)); }
static void bstr(buf_t *b, const char *s)
{
   size_t n = strlen(s);
   if (n < 32)
      bbyte(b, (uint8_t)(0xa0 | n));
   else
   {
      bbyte(b, 0xd9);
      bbyte(b, (uint8_t)n);
   }
   bput(b, s, n);
}
static void bbin4(buf_t *b, uint32_t v)
{
   uint8_t c[4];
   c[0] = (uint8_t)(v >> 24); c[1] = (uint8_t)(v >> 16);
   c[2] = (uint8_t)(v >> 8);  c[3] = (uint8_t)v;
   bbyte(b, 0xc4); bbyte(b, 4); bput(b, c, 4);
}
static void buint(buf_t *b, unsigned v)
{
   if (v < 128)
      bbyte(b, (uint8_t)v);
   else
   {
      bbyte(b, 0xcd);
      bbyte(b, (uint8_t)(v >> 8));
      bbyte(b, (uint8_t)v);
   }
}

static uint32_t game_crc(unsigned sys, unsigned g)
{
   return 0x1000u * (sys + 1) + g * 7u + 1u;
}

/* One database: GAMES records, each with a name, a crc (except every
 * fifth, matched by name instead), and categories that repeat across
 * games so the category lists have order and duplicates to resolve. */
static int write_db(const char *path, unsigned sys)
{
   static const char *devs[]   = { "Aster", "Boreal", "Cinder", "Delta" };
   static const char *genres[] = { "Action", "Puzzle", "Racing" };
   buf_t body, meta;
   FILE *f;
   uint8_t hdr[16];
   uint64_t off;
   unsigned g;
   int i;

   memset(&body, 0, sizeof(body));
   memset(&meta, 0, sizeof(meta));
   for (g = 0; g < GAMES; g++)
   {
      char name[64], tags[64];
      snprintf(name, sizeof(name), "Game S%u N%04u", sys, g);
      snprintf(tags, sizeof(tags), "%s|%s", genres[g % 3], genres[(g + 1) % 3]);
      bfixmap(&body, (g % 5 == 4) ? 6 : 7);
      bstr(&body, "name");        bstr(&body, name);
      if (g % 5 != 4)
      {
         bstr(&body, "crc");      bbin4(&body, game_crc(sys, g));
      }
      bstr(&body, "developer");   bstr(&body, devs[g % 4]);
      bstr(&body, "publisher");   bstr(&body, devs[(g / 4) % 4]);
      bstr(&body, "genre");       bstr(&body, genres[g % 3]);
      bstr(&body, "releaseyear"); buint(&body, 1985 + (g % 12));
      bstr(&body, "tags");        bstr(&body, tags);
   }
   bbyte(&body, 0xc0);

   bfixmap(&meta, 1);
   bstr(&meta, "count");
   buint(&meta, GAMES);

   off = 16 + (uint64_t)body.len;
   memcpy(hdr, "RARCHDB", 7);
   hdr[7] = 0;
   for (i = 0; i < 8; i++)
      hdr[8 + i] = (uint8_t)(off >> (56 - 8 * i));

   if (!(f = fopen(path, "wb")))
      return 0;
   fwrite(hdr, 1, sizeof(hdr), f);
   fwrite(body.d, 1, body.len, f);
   fwrite(meta.d, 1, meta.len, f);
   fclose(f);
   free(body.d);
   free(meta.d);
   return 1;
}

/* One playlist per system, every game, the CRC where the database
 * has one and "00000000" where the match is by label. */
static int write_playlist(const char *path, unsigned sys, const char *db)
{
   unsigned g;
   FILE *f = fopen(path, "wb");
   if (!f)
      return 0;
   fprintf(f, "{\n  \"version\": \"1.5\",\n  \"items\": [\n");
   for (g = 0; g < GAMES; g++)
      fprintf(f,
            "    { \"path\": \"/nowhere/s%u/g%04u.bin\", \"label\": \"Game S%u N%04u\","
            " \"core_path\": \"DETECT\", \"core_name\": \"DETECT\","
            " \"crc32\": \"%08X|crc\", \"db_name\": \"%s\" }%s\n",
            sys, g, sys, g, (g % 5 == 4) ? 0u : game_crc(sys, g), db,
            (g + 1 < GAMES) ? "," : "");
   fprintf(f, "  ]\n}\n");
   fclose(f);
   return 1;
}

/* ------------------------------------------------------------------ */

static char dir_playlists[512];
static char dir_database[512];

/* A budget that allows exactly one work item per step: the first
 * ask in a step is granted, every later one refused - the shape of
 * the shared window's floor. */
static unsigned budget_calls;
static unsigned budget_left;
static bool one_item(void *ud)
{
   (void)ud;
   budget_calls++;
   if (!budget_left)
      return false;
   budget_left--;
   return true;
}
static int step_one(explore_build_t *b)
{
   budget_left = 1;
   return menu_explore_build_step(b, one_item, NULL);
}

static void lane_oracle(void)
{
   explore_state_t *once, *sliced;
   explore_build_t *b;
   unsigned steps = 0;
   unsigned had   = failures;
   unsigned i;
   int r;

   once = menu_explore_build_list(dir_playlists, dir_database);
   CHECK(once != NULL, "the one-go build failed");
   CHECK(menu_explore_state_entry_count(once) == SYSTEMS * GAMES,
         "the one-go build indexed %u entries, not %u",
         (unsigned)menu_explore_state_entry_count(once), SYSTEMS * GAMES);

   b = menu_explore_build_begin(dir_playlists, dir_database);
   CHECK(b != NULL, "begin failed");
   budget_calls = 0;
   while ((r = step_one(b)) == 0)
      steps++;
   CHECK(r == 1, "the sliced build failed");
   sliced = menu_explore_build_end(b);
   CHECK(sliced != NULL, "end returned no state");
   CHECK(steps > SYSTEMS * GAMES,
         "the sliced build took %u steps for %u items", steps, SYSTEMS * GAMES);
   CHECK(menu_explore_state_entry_count(sliced) == menu_explore_state_entry_count(once),
         "entry counts differ: sliced %u, one go %u",
         (unsigned)menu_explore_state_entry_count(sliced),
         (unsigned)menu_explore_state_entry_count(once));
   CHECK(menu_explore_state_hash(sliced) == menu_explore_state_hash(once),
         "the sliced index differs from the one built in one go "
         "(%08x vs %08x)", menu_explore_state_hash(sliced),
         menu_explore_state_hash(once));

   menu_explore_free_state(once);  free(once);
   menu_explore_free_state(sliced); free(sliced);

   /* Abandoned mid-way: nothing left behind. */
   b = menu_explore_build_begin(dir_playlists, dir_database);
   budget_calls = 0;
   for (i = 0; i < 50; i++)
      if (step_one(b) != 0)
         break;
   menu_explore_build_abort(b);

   if (failures == had)
      fprintf(stderr, "[pass] oracle lane (%u one-item steps)\n", steps);
}

static unsigned task_handler_calls;

static void lane_task(void)
{
   unsigned had = failures;
   unsigned i;
   uint32_t once_hash;
   explore_state_t *once = menu_explore_build_list(dir_playlists, dir_database);
   once_hash = menu_explore_state_hash(once);
   menu_explore_free_state(once); free(once);

   menu_explore_free();
   CHECK(task_push_menu_explore_init(dir_playlists, dir_database),
         "the explore task was not pushed");
   task_handler_calls = 0;
   for (i = 0; i < 2000 && menu_explore_init_in_progress(NULL); i++)
   {
      runloop_iterate();
      task_queue_check();
      task_handler_calls++;
   }
   CHECK(!menu_explore_init_in_progress(NULL), "the explore task did not finish");
   CHECK(task_handler_calls > 1,
         "the explore task finished on its first check (one-shot build)");
   CHECK(menu_explore_state_hash(NULL) == once_hash,
         "the task installed a different index than the one-go build");
   menu_explore_free();

   if (failures == had)
      fprintf(stderr, "[pass] task lane (%u checks)\n", task_handler_calls);
}

/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char dir[400];
   char *rarch_argv[8] = {0};
   int rarch_argc = 0;
   FILE *cfg;
   unsigned s;
   (void)argc; (void)argv;

   {
      const char *tmp = getenv("TMPDIR");
      if (!tmp || !*tmp)
         tmp = getenv("TEMP");
      if (!tmp || !*tmp)
         tmp = "/tmp";
      snprintf(dir, sizeof(dir), "%s/explore_build_harness_%ld",
            tmp, (long)getpid());
   }
   snprintf(dir_playlists, sizeof(dir_playlists), "%s/playlists", dir);
   snprintf(dir_database,  sizeof(dir_database),  "%s/rdb", dir);
   if (!path_mkdir(dir) || !path_mkdir(dir_playlists) || !path_mkdir(dir_database))
      return 1;
   for (s = 0; s < SYSTEMS; s++)
   {
      char path[600], db[64];
      snprintf(db, sizeof(db), "Harness System %u.lpl", s);
      snprintf(path, sizeof(path), "%s/%s", dir_playlists, db);
      if (!write_playlist(path, s, db))
         return 1;
      snprintf(path, sizeof(path), "%s/Harness System %u.rdb", dir_database, s);
      if (!write_db(path, s))
         return 1;
   }

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if ((cfg = fopen(cfg_path, "wb")))
   {
      fprintf(cfg, "video_driver = \"null\"\n");
      fprintf(cfg, "audio_driver = \"null\"\n");
      fprintf(cfg, "input_driver = \"null\"\n");
      fprintf(cfg, "input_joypad_driver = \"null\"\n");
      fprintf(cfg, "menu_driver = \"rgui\"\n");
      fprintf(cfg, "video_threaded = \"false\"\n");
      fprintf(cfg, "video_vsync = \"false\"\n");
      fprintf(cfg, "config_save_on_exit = \"false\"\n");
      fprintf(cfg, "threaded_data_runloop_enable = \"false\"\n");
      fclose(cfg);
   }
   if (getenv("HARNESS_VERBOSE"))
      verbosity_enable();

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;
   if (getenv("HARNESS_VERBOSE"))
      rarch_argv[rarch_argc++] = (char*)"-v";
   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   runloop_iterate();

   lane_oracle();
   lane_task();

   main_exit(NULL);

   for (s = 0; s < SYSTEMS; s++)
   {
      char path[600];
      snprintf(path, sizeof(path), "%s/Harness System %u.lpl", dir_playlists, s);
      remove(path);
      snprintf(path, sizeof(path), "%s/Harness System %u.rdb", dir_database, s);
      remove(path);
   }
   rmdir(dir_playlists);
   rmdir(dir_database);
   remove(cfg_path);
   rmdir(dir);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed\n");
   return 0;
}
