#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define fixture_mkdir(path) _mkdir(path)
#define fixture_pid() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define fixture_mkdir(path) mkdir(path, 0700)
#define fixture_pid() getpid()
#endif
#include <compat/strl.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include "../../../command.h"
#include "../../../runloop.h"

static settings_t settings;
static runloop_state_t runloop;
static char directory[64];
static char created[64][256];
static unsigned created_count;
static unsigned checks;

settings_t *config_get_ptr(void) { return &settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &runloop; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
bool verbosity_is_enabled(void) { return false; }
bool content_state_file_in_use(const char *path) { (void)path; return false; }
const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return "test"; }


static void clear_files(void)
{
   while (created_count)
      remove(created[--created_count]);
}

static void set_base(const char *base)
{
   clear_files();
   snprintf(runloop.name.savestate, sizeof(runloop.name.savestate),
         "%s/%s", directory, base);
}

static void add_file(const char *name)
{
   FILE *file;
   assert(created_count < 64);
   snprintf(created[created_count], sizeof(created[0]), "%s/%s", directory, name);
   file = fopen(created[created_count], "wb");
   assert(file);
   assert(fputc(0x5a, file) != EOF);
   assert(fclose(file) == 0);
   created_count++;
}

static void slot_path(char *path, size_t size, int slot)
{
   size_t len = strlcpy(path, runloop.name.savestate, size);
   if (slot > 0)
      snprintf(path + len, size - len, "%d", slot);
}

static void add_slot(unsigned slot)
{
   char name[192];
   size_t len = strlcpy(name, path_basename(runloop.name.savestate), sizeof(name));
   if (slot)
      snprintf(name + len, sizeof(name) - len, "%u", slot);
   add_file(name);
}

static void choose(unsigned max_keep, int stale, int expected, bool exists)
{
   char path[256];
   settings.uints.savestate_max_keep = max_keep;
   settings.ints.state_slot = stale;
   command_event_set_savestate_auto_index(&settings);
   assert(settings.ints.state_slot == expected);
   slot_path(path, sizeof(path), expected);
   assert(path_is_valid(path) == exists);
   checks++;
}

static size_t file_count(void)
{
   struct string_list *files = dir_list_new(directory, NULL, false, true, false, false);
   size_t count;
   assert(files);
   count = files->size;
   dir_list_free(files);
   return count;
}

static void test_selection(void)
{
   unsigned i;
   char name[160];
   set_base("game.state");
   choose(10, -1, 10, false);
   choose(0, 7, 0, false);
   add_slot(0);
   choose(10, 9, 0, true);
   add_slot(10);
   choose(10, 9, 0, true); /* A manually removed middle slot has no chronology. */
   choose(0, 9, 10, true);

   set_base("game.state");
   for (i = 1; i <= 9; i++)
      add_slot(i);
   choose(10, 0, 9, true);
   add_slot(0);
   add_slot(10);
   choose(10, 3, 3, true);
   choose(10, -1, 10, true);
   add_slot(12);
   choose(10, 3, 12, true);

   set_base("game.state");
   add_slot(3);
   add_slot(12);
   choose(0, 9, 12, true);
   choose(2, 9, 12, true);
   choose(12, 9, 12, true);
   choose(10, 9, 3, true);
   add_file("game.state.auto");
   add_file("game.state1.bak");
   add_file("game.state1.png");
   add_file("game.state01");
   add_file("game.state0");
   add_file("game.statex");
   add_file("game.stateExtra.state8");
   add_file("game.state2147483648");
   add_file("game.state999999999999999999999999");
   choose(10, 9, 3, true);
   choose(0, 9, 12, true);

   memset(name, 'g', 150);
   strcpy(name + 150, ".state");
   set_base(name);
   add_slot(3);
   choose(10, 9, 3, true);
   settings.ints.state_slot = INT_MAX;
   assert(command_event_get_next_savestate_auto_index(&settings) == 0);
   settings.ints.state_slot = -1;
   assert(command_event_get_next_savestate_auto_index(&settings) == 0);
}

static void test_retention(void)
{
   unsigned limit, write;
   char path[256];
   for (limit = 1; limit <= 5; limit++)
   {
      set_base("game.state");
      settings.uints.savestate_max_keep = limit;
      settings.ints.state_slot = (int)limit;
      for (write = 0; write < 40; write++)
      {
         int slot = command_event_get_next_savestate_auto_index(&settings);
         add_slot((unsigned)slot);
         slot_path(path, sizeof(path), slot);
         command_event_set_savestate_garbage_collect(path, slot, limit, true);
         choose(limit, -1, slot, true);
         assert(file_count() == (write + 1 < limit ? write + 1 : limit));
      }
   }
}

static void test_prune_path(void)
{
   unsigned i;
   const char *invalid[] = {"game.state01", "game.state.auto", "game.state1.tmp"};
   char path[256];
   set_base("game.state");
   for (i = 0; i <= 10; i++)
      add_slot(i);
   slot_path(path, sizeof(path), 10);
   command_event_set_savestate_garbage_collect(NULL, 10, 10, true);
   command_event_set_savestate_garbage_collect(path, -1, 10, true);
   command_event_set_savestate_garbage_collect(path, 10, 0, true);
   assert(file_count() == 11);
   command_event_set_savestate_garbage_collect(path, 9, 10, true);
   assert(file_count() == 11);
   for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
   {
      snprintf(path, sizeof(path), "%s/%s", directory, invalid[i]);
      command_event_set_savestate_garbage_collect(path, 1, 10, true);
      assert(file_count() == 11);
   }
   set_base("custom");
   for (i = 0; i <= 10; i++)
      add_slot(i);
   slot_path(path, sizeof(path), 10);
   command_event_set_savestate_garbage_collect(path, 10, 10, true);
   assert(file_count() == 11);
}

/* Replays number slot 0 as "0" and keep counting from the highest
 * slot on disk, whatever the folder's path length. */
static void test_replay_index(void)
{
   const char *names[] = {"game.replay0", "game.replay3", "game.replay12",
         "game.replay.auto", "game.replay07", "game.replay",
         "game.replay3.png", "other.replay40", "game.replay99x"};
   unsigned i;
   clear_files();
   snprintf(runloop.name.replay, sizeof(runloop.name.replay),
         "%s/game.replay", directory);
   settings.bools.replay_auto_index = true;
   settings.ints.replay_slot = 5;
   command_event_set_replay_auto_index(&settings);
   assert(settings.ints.replay_slot == 0);
   add_file("game.replay0");
   command_event_set_replay_auto_index(&settings);
   assert(settings.ints.replay_slot == 0);
   for (i = 1; i < sizeof(names) / sizeof(names[0]); i++)
      add_file(names[i]);
   command_event_set_replay_auto_index(&settings);
   assert(settings.ints.replay_slot == 12);
   settings.bools.replay_auto_index = false;
   settings.ints.replay_slot = 4;
   command_event_set_replay_auto_index(&settings);
   assert(settings.ints.replay_slot == 4);
   checks += 4;
   clear_files();
}

static void replay_path(char *path, size_t size, const char *name)
{
   snprintf(path, size, "%s/%s", directory, name);
}

/* A recording past the replay limit deletes the lowest slot of its
 * own name, one at a time, never itself, and nothing for a path that
 * is not a replay slot. */
static void test_replay_prune(void)
{
   const char *ignored[] = {"game.replay05", "game.replay.auto",
         "game.state5", "game.replay"};
   char path[256];
   unsigned i;
   clear_files();
   for (i = 0; i <= 5; i++)
   {
      char name[64];
      snprintf(name, sizeof(name), "game.replay%u", i);
      add_file(name);
   }
   add_file("other.replay0");
   replay_path(path, sizeof(path), "game.replay5");
   command_event_replay_prune(path, 0, true);
   assert(file_count() == 7);
   command_event_replay_prune(path, 3, true);
   replay_path(path, sizeof(path), "game.replay0");
   assert(!path_is_valid(path) && file_count() == 6);
   replay_path(path, sizeof(path), "game.replay5");
   command_event_replay_prune(path, 3, true);
   replay_path(path, sizeof(path), "game.replay1");
   assert(!path_is_valid(path) && file_count() == 5);
   for (i = 0; i < sizeof(ignored) / sizeof(ignored[0]); i++)
   {
      replay_path(path, sizeof(path), ignored[i]);
      command_event_replay_prune(path, 1, true);
      assert(file_count() == 5);
   }
   /* the recording is the lowest slot: the next one up goes */
   clear_files();
   add_file("game.replay0");
   add_file("game.replay7");
   add_file("game.replay8");
   replay_path(path, sizeof(path), "game.replay0");
   command_event_replay_prune(path, 2, true);
   assert(path_is_valid(path) && file_count() == 2);
   replay_path(path, sizeof(path), "game.replay7");
   assert(!path_is_valid(path));
   checks += 6;
   clear_files();
}

int main(void)
{
   unsigned attempt;
   for (attempt = 0; attempt < 1000; attempt++)
   {
      snprintf(directory, sizeof(directory), "rs-%lu-%u",
            (unsigned long)fixture_pid(), attempt);
      if (fixture_mkdir(directory) == 0)
         break;
      assert(errno == EEXIST);
   }
   assert(attempt < 1000);
   settings.bools.savestate_auto_index = true;
   test_selection();
   test_retention();
   test_prune_path();
   test_replay_index();
   test_replay_prune();
   clear_files();
   assert(path_rmdir(directory));
   printf("real-directory rolling scan: %u selections passed\n", checks);
   return 0;
}
