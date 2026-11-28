/* cheat_manager.c's game-specific cheat file with a User Cheat Files
 * folder, over real files: which folder a load reads, which one a
 * save goes back to, the delete in both, Save As, and the one-folder
 * behaviour when the folder is unset or is Cheat Files itself. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <compat/strl.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "cheat_manager.h"
#include "runloop.h"
#include "core.h"
#include "msg_hash.h"
#include "verbosity.h"

/* What cheat_manager.c calls outside itself */
static runloop_state_t test_runloop;
static bool test_core_ok = true;
runloop_state_t *runloop_state_get_ptr(void) { return &test_runloop; }
void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{ (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
  (void)title; (void)icon; (void)category; }
void runloop_frame_work_set(unsigned work, bool on) { (void)work; (void)on; }
const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return ""; }
bool core_get_memory(retro_ctx_memory_info_t *info) { (void)info; return false; }
bool core_get_system_info(struct retro_system_info *info)
{
   if (!test_core_ok)
      return false;
   memset(info, 0, sizeof(*info));
   info->library_name = "TestCore";
   return true;
}
bool core_set_cheat(retro_ctx_cheat_info_t *info) { (void)info; return true; }
bool core_reset_cheat(void) { return true; }
retro_time_t cpu_features_get_time_usec(void) { return 0; }
uint64_t cpu_features_get(void) { return 0; }
bool input_set_rumble_state(unsigned port, enum retro_rumble_effect e,
      uint16_t strength) { (void)port; (void)e; (void)strength; return true; }

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }

#define TMP     "cheat_user_dir_tmp"
#define USER    TMP "/user"
#define DB      TMP "/db"
#define U_CHT   USER "/TestCore/Game.cht"
#define D_CHT   DB "/TestCore/Game.cht"

static const char *test_name = "";
static int failures          = 0;
#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", test_name, __LINE__, #c); \
   failures++; } } while (0)

/* Empty folders and an empty cheat list, as command_event_init_cheats()
 * starts a content session. */
static void reset(const char *name, bool make_user)
{
   test_name    = name;
   test_core_ok = true;
   path_remove_tree(TMP);
   path_mkdir(DB);
   if (make_user)
      path_mkdir(USER);
   cheat_manager_state_free();
   cheat_manager_alloc_if_empty();
}

/* A cheat file with one cheat described as @tag */
static void write_cht(const char *path, const char *tag)
{
   char text[160];
   int  len = snprintf(text, sizeof(text),
         "cheats = 1\n"
         "cheat0_desc = \"%s\"\n"
         "cheat0_code = \"0000\"\n"
         "cheat0_enable = \"false\"\n", tag);
   filestream_write_file(path, text, len);
}

/* <root>/TestCore/Game.cht */
static void put(const char *root, const char *tag)
{
   char dir[128];
   char file[256];
   snprintf(dir,  sizeof(dir),  "%s/TestCore", root);
   snprintf(file, sizeof(file), "%s/Game.cht", dir);
   path_mkdir(dir);
   write_cht(file, tag);
}

static const char *desc_of(const char *path)
{
   static char desc[64];
   config_file_t *conf = config_file_new_from_path_to_string(path);
   desc[0]             = '\0';
   if (conf)
   {
      config_get_array(conf, "cheat0_desc", desc, sizeof(desc));
      config_file_free(conf);
   }
   return desc;
}

static const char *list_desc(void)
{
   const char *desc = cheat_manager_get_size()
      ? cheat_manager_get_desc(0) : NULL;
   return desc ? desc : "";
}

static void set_desc(const char *desc)
{
   size_t len = strlen(desc) + 1;
   struct item_cheat *c;
   if (!cheat_manager_get_size())
      return;
   c = &cheat_manager_state.cheats[0];
   free(c->desc);
   if ((c->desc = (char*)malloc(len)))
      memcpy(c->desc, desc, len);
}

static void add_new(const char *desc)
{
   cheat_manager_realloc(1, CHEAT_HANDLER_TYPE_EMU);
   cheat_manager_set_code(0, "0000");
   set_desc(desc);
}

/* realloc to 0 frees the list; the save deletes only on an
 * allocated empty one, which is what a session holds when the
 * game had no file. */
static void empty_list(void)
{
   cheat_manager_realloc(0, CHEAT_HANDLER_TYPE_EMU);
   cheat_manager_alloc_if_empty();
   CHECK(cheat_manager_state.cheats && !cheat_manager_get_size());
}

static void test_active_helper(void)
{
   test_name = "test_active_helper";
   CHECK(!cheat_manager_user_dir_active("a/db", NULL));
   CHECK(!cheat_manager_user_dir_active("a/db", ""));
   CHECK(!cheat_manager_user_dir_active("a/db", "a/db"));
   CHECK(!cheat_manager_user_dir_active("a/db", "a/db/"));
   CHECK(!cheat_manager_user_dir_active("a/db/", "a/db"));
   CHECK( cheat_manager_user_dir_active("a/db", "a/user"));
   CHECK( cheat_manager_user_dir_active("a/db", "a/dbx"));
   CHECK( cheat_manager_user_dir_active("a/dbx", "a/db"));
   CHECK(!cheat_manager_user_dir_active("a//db", "a//db//"));
   CHECK( cheat_manager_user_dir_active("a/db", "a/Db"));
#ifdef _WIN32
   CHECK(!cheat_manager_user_dir_active("a/db", "a\\db"));
   CHECK(!cheat_manager_user_dir_active("C:\\RA\\cheats", "c:/ra/cheats/"));
   CHECK( cheat_manager_user_dir_active("C:\\RA\\cheats", "c:/ra/cheatz"));
#else
   CHECK( cheat_manager_user_dir_active("a/db", "a\\db"));
#endif
}

/* One read of the file fills the list: the count may come last and
 * in any base strtoul takes, a key given twice counts once (its
 * first occurrence), indices past the count are dropped, and an
 * append lands after the cheats already held */
static void test_load_single_parse(void)
{
   static const char text[] =
      "cheat1_desc = \"second\"\n"
      "cheat0_desc = \"first\"\n"
      "cheat0_desc = \"shadowed\"\n"
      "cheat0_code = \"AAAA\"\n"
      "cheat0_enable = \"true\"\n"
      "cheat1_code = \"BBBB\"\n"
      "cheat1_address = \"0x10\"\n"
      "cheat2_desc = \"dropped\"\n"
      "cheats = 0x2\n";
   reset("test_load_single_parse", false);
   filestream_write_file(DB "/order.cht", text, sizeof(text) - 1);
   CHECK(cheat_manager_load(DB "/order.cht", false));
   CHECK(cheat_manager_get_size() == 2);
   CHECK(!strcmp(cheat_manager_get_desc(0), "first"));
   CHECK(!strcmp(cheat_manager_state.cheats[0].code, "AAAA"));
   CHECK(cheat_manager_state.cheats[0].state);
   CHECK(!strcmp(cheat_manager_get_desc(1), "second"));
   CHECK(!strcmp(cheat_manager_state.cheats[1].code, "BBBB"));
   CHECK(cheat_manager_state.cheats[1].address == 0x10);
   CHECK(cheat_manager_load(DB "/order.cht", true));
   CHECK(cheat_manager_get_size() == 4);
   CHECK(!strcmp(cheat_manager_get_desc(2), "first"));
   CHECK(!strcmp(cheat_manager_get_desc(3), "second"));
   CHECK(cheat_manager_state.cheats[3].address == 0x10);
   filestream_write_file(DB "/none.cht", "cheats = 0\n", 11);
   CHECK(!cheat_manager_load(DB "/none.cht", false));
   CHECK(!cheat_manager_load(DB "/missing.cht", false));
   CHECK(cheat_manager_get_size() == 4);
}

/* Each Load Cheat File folder is there to browse when picked */
static void test_browse_dir(void)
{
   reset("test_browse_dir", false);
   path_remove_tree(DB);
   CHECK(!strcmp(cheat_manager_browse_dir(DB, USER, true), USER));
   CHECK(path_is_directory(USER));
   CHECK(!path_is_directory(DB));
   CHECK(!strcmp(cheat_manager_browse_dir(DB, USER, false), DB));
   CHECK(path_is_directory(DB));
   CHECK(!strcmp(cheat_manager_browse_dir(DB, "", true), ""));
}

/* Save As with no cheats removes the named file in its folder and
 * nothing outside it */
static void test_save_as_empty_deletes_in_folder(void)
{
   reset("test_save_as_empty_deletes_in_folder", true);
   write_cht(USER "/Mine.cht", "mine");
   write_cht("Mine", "cwd");
   write_cht("Mine.cht", "cwd");
   empty_list();
   CHECK(!cheat_manager_save_as("Mine", DB, USER));
   CHECK(!path_is_valid(USER "/Mine.cht"));
   CHECK(!strcmp(desc_of("Mine"), "cwd"));
   CHECK(!strcmp(desc_of("Mine.cht"), "cwd"));
   filestream_delete("Mine");
   filestream_delete("Mine.cht");
}

/* Load reads Cheat Files, a save writes it, an empty list deletes
 * it, and nothing appears under the user folder. */
static void check_today(const char *user)
{
   put(DB, "db");
   cheat_manager_load_game_specific_cheats(DB, user);
   CHECK(!strcmp(list_desc(), "db"));
   set_desc("edited");
   cheat_manager_save_game_specific_cheats(DB, user);
   CHECK(!strcmp(desc_of(D_CHT), "edited"));
   empty_list();
   cheat_manager_save_game_specific_cheats(DB, user);
   CHECK(!path_is_valid(D_CHT));
   CHECK(!path_is_valid(USER "/TestCore"));
}

static void test_inactive_is_today(void)
{
   reset("test_inactive_is_today", true);
   check_today("");
}

static void test_inactive_same_folder(void)
{
   reset("test_inactive_same_folder", true);
   CHECK(!cheat_manager_user_dir_active(DB, DB "/"));
   check_today(DB "/");
}

static void test_user_wins(void)
{
   reset("test_user_wins", true);
   put(USER, "user");
   put(DB, "db");
   cheat_manager_load_game_specific_cheats(DB, USER);
   CHECK(cheat_manager_get_size() == 1);
   CHECK(!strcmp(list_desc(), "user"));
   set_desc("edited");
   cheat_manager_save_game_specific_cheats(DB, USER);
   CHECK(!strcmp(desc_of(U_CHT), "edited"));
   CHECK(!strcmp(desc_of(D_CHT), "db"));
}

static void test_db_only_written_back(void)
{
   reset("test_db_only_written_back", true);
   put(DB, "db");
   cheat_manager_load_game_specific_cheats(DB, USER);
   CHECK(!strcmp(list_desc(), "db"));
   set_desc("edited");
   cheat_manager_save_game_specific_cheats(DB, USER);
   CHECK(!strcmp(desc_of(D_CHT), "edited"));
   CHECK(!path_is_valid(U_CHT));
}

/* Load Cheat File (Replace) frees the list before loading another
 * file; the game's own file still goes back to Cheat Files. */
static void test_replace_keeps_db_home(void)
{
   reset("test_replace_keeps_db_home", true);
   put(DB, "db");
   write_cht(TMP "/Other.cht", "other");
   cheat_manager_load_game_specific_cheats(DB, USER);
   CHECK(!strcmp(list_desc(), "db"));
   cheat_manager_state_free();
   CHECK(cheat_manager_load(TMP "/Other.cht", false));
   CHECK(!strcmp(list_desc(), "other"));
   set_desc("edited");
   cheat_manager_save_game_specific_cheats(DB, USER);
   CHECK(!strcmp(desc_of(D_CHT), "edited"));
   CHECK(!path_is_valid(U_CHT));
}

static void test_new_file_goes_to_user(void)
{
   reset("test_new_file_goes_to_user", false);
   cheat_manager_load_game_specific_cheats(DB, USER);
   CHECK(cheat_manager_get_size() == 0);
   add_new("new");
   cheat_manager_save_game_specific_cheats(DB, USER);
   CHECK(!strcmp(desc_of(U_CHT), "new"));
   CHECK(!path_is_valid(D_CHT));
}

static void test_zero_cheats_deletes_both(void)
{
   reset("test_zero_cheats_deletes_both", true);
   put(USER, "user");
   put(DB, "db");
   cheat_manager_load_game_specific_cheats(DB, USER);
   empty_list();
   cheat_manager_save_game_specific_cheats(DB, USER);
   CHECK(!path_is_valid(U_CHT));
   CHECK(!path_is_valid(D_CHT));
}

static void test_no_core_name(void)
{
   reset("test_no_core_name", true);
   put(USER, "user");
   put(DB, "db");
   test_core_ok = false;
   cheat_manager_load_game_specific_cheats(DB, USER);
   CHECK(cheat_manager_get_size() == 0);
   cheat_manager_save_game_specific_cheats(DB, USER);
   add_new("new");
   cheat_manager_save_game_specific_cheats(DB, USER);
   CHECK(!strcmp(desc_of(U_CHT), "user"));
   CHECK(!strcmp(desc_of(D_CHT), "db"));

   reset("test_no_core_name", true);
   test_core_ok = false;
   cheat_manager_load_game_specific_cheats(DB, USER);
   cheat_manager_save_game_specific_cheats(DB, USER);
   add_new("new");
   cheat_manager_save_game_specific_cheats(DB, USER);
   CHECK(!path_is_valid(USER "/TestCore"));
   CHECK(!path_is_valid(DB "/TestCore"));
}

static void test_save_as_creates_user_dir(void)
{
   reset("test_save_as_creates_user_dir", false);
   add_new("mine");
   CHECK(cheat_manager_save_as("Mine", DB, USER));
   CHECK(!strcmp(desc_of(USER "/Mine.cht"), "mine"));
   CHECK(!path_is_valid(DB "/Mine.cht"));

   reset("test_save_as_creates_user_dir", false);
   add_new("mine");
   CHECK(cheat_manager_save_as("Mine", DB, ""));
   CHECK(!strcmp(desc_of(DB "/Mine.cht"), "mine"));
   CHECK(!path_is_valid(USER));
}

int main(void)
{
   strlcpy(test_runloop.name.cheatfile, "Game.cht",
         sizeof(test_runloop.name.cheatfile));
   test_active_helper();
   test_inactive_is_today();
   test_inactive_same_folder();
   test_user_wins();
   test_db_only_written_back();
   test_replace_keeps_db_home();
   test_new_file_goes_to_user();
   test_zero_cheats_deletes_both();
   test_no_core_name();
   test_save_as_creates_user_dir();
   test_load_single_parse();
   test_browse_dir();
   test_save_as_empty_deletes_in_folder();
   cheat_manager_state_free();
   path_remove_tree(TMP);
   printf("cheat_user_dir: %s (%d failures)\n",
         failures ? "FAIL" : "PASS", failures);
   return failures ? 1 : 0;
}
