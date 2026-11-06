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
#include "../../../list_special.h"
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

struct string_list *dir_list_new_special(const char *path,
      enum dir_list_type type, const char *filter, bool hidden)
{
   assert(type == DIR_LIST_PLAIN);
   return dir_list_new(path, filter, false, hidden, false, false);
}

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

int main(int argc, char **argv)
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
   if (argc == 2 && !strcmp(argv[1], "--interrupt"))
   {
      add_file("interrupted.state");
      printf("interrupted fixture: %s\n", directory);
      fflush(stdout);
      _exit(1);
   }
   settings.bools.savestate_auto_index = true;
   test_selection();
   clear_files();
   assert(path_rmdir(directory));
   printf("real-directory rolling scan: %u selections passed\n", checks);
   return 0;
}
