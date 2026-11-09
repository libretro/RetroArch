/* The Security Warning answers only the request it shows.
 *
 * Links the shipping RetroArch objects with only main() replaced (the
 * load_content_filter pattern), core_signature.c built with the public
 * test key so the trust store is active, and boots the menu on the
 * null drivers, with a config and trust store of its own. Then:
 *  - a request pushed while the screen still shows an older one, of
 *    either kind: OK on the older rows chooses nothing for it, and the
 *    screen stays, now showing the newer request;
 *  - a request still pending when a stack flush (a tab switch) takes
 *    its screen away: the screen is back on the next frame;
 *  - OK and Back pop the screen before the callback runs, Info puts
 *    nothing over it, and a request answered before its screen came
 *    up leaves no screen;
 *  - a menu driver that shows no screens (null) takes no request, and
 *    a screen it can't push back doesn't take the frame.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/core_warning/build.sh
 *   samples/menu/core_warning/core_warning_test
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
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../msg_hash.h"
#include "../../../msg_hash_lbl_str.h"
#include "../../../core_trust_runtime.h"

static unsigned failures = 0;

#define CHECK(cond, what) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what); \
         failures++; \
      } \
   } while (0)

#define NO_ROW ((size_t)-1)

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

static bool shown(void)
{
   const char *label = NULL;
   menu_entries_get_last_stack(NULL, &label, NULL, NULL, NULL);
   return string_is_equal(label, MENU_ENUM_LABEL_DEFERRED_CORE_WARNING_STR);
}

/* The row a choice is on, in the list as it is now */
static size_t row(enum msg_hash_enums label)
{
   file_list_t *buf = selection_buf();
   const char *want = msg_hash_to_str(label);
   size_t i;
   for (i = 0; buf && i < buf->size; i++)
      if (buf->list[i].label && string_is_equal(buf->list[i].label, want))
         return i;
   return NO_ROW;
}

static size_t stack_size(void)
{
   menu_list_t *menu_list = menu_state_get_ptr()->entries.list;
   return menu_list ? MENU_LIST_GET(menu_list, 0)->size : 0;
}

/* Where the screen puts the cursor */
static bool cursor_on_cancel(void)
{
   return menu_state_get_ptr()->selection_ptr
      == row(MENU_ENUM_LABEL_CORE_WARNING_CANCEL);
}

static void act_on(size_t i, enum menu_action action)
{
   menu_entry_t entry;
   if (i == NO_ROW)
      return;
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

static void ok_on(size_t i)
{
   act_on(i, MENU_ACTION_OK);
}

struct seen
{
   enum core_warning_choice choice;
   unsigned calls;
   /* The screen was still on top when the callback ran */
   bool on_top;
};

static struct seen seen_a, seen_b;

static void warn_cb(enum core_warning_choice choice,
      const struct core_check *c, const char *core_path, void *userdata)
{
   struct seen *s = (struct seen*)userdata;
   s->calls++;
   s->choice = choice;
   s->on_top = shown();
}

static void fake_check(struct core_check *c, const char *name,
      uint8_t byte, enum core_check_result result)
{
   memset(c, 0, sizeof(*c));
   strlcpy(c->rec.name, name, sizeof(c->rec.name));
   memset(c->rec.sha256, byte, CORE_SHA256_LEN);
   c->result = result;
}

/* A shows; B arrives and the frame flags the refresh; OK lands on A's
 * row before the rows are built again, as the next frame's input does */
static void lane_newer(enum core_warning_kind kind_a,
      enum msg_hash_enums choice_a, enum core_warning_kind kind_b,
      const char *name)
{
   struct core_check a, b;
   enum core_warning_kind kind;
   char what[160];
   size_t i;

   memset(&seen_a, 0, sizeof(seen_a));
   memset(&seen_b, 0, sizeof(seen_b));
   fake_check(&a, "a_libretro.so", 0x41, CORE_CHECK_UNSIGNED);
   fake_check(&b, "b_libretro.so", 0x42, CORE_CHECK_MODIFIED);

   CHECK(core_warning_push(&a, kind_a, "/cores/a_libretro.so", warn_cb,
            &seen_a), "fixture: push A refused");
   pump(3);
   sprintf(what, "%s: A's screen is not shown", name);
   CHECK(shown(), what);
   sprintf(what, "%s: A's screen doesn't start on Cancel", name);
   CHECK(cursor_on_cancel(), what);
   i = row(choice_a);
   sprintf(what, "%s: A's choice row is missing", name);
   CHECK(i != NO_ROW, what);
   if (i == NO_ROW)
      return;

   CHECK(core_warning_push(&b, kind_b, "/cores/b_libretro.so", warn_cb,
            &seen_b), "fixture: push B refused");
   menu_core_warning_iterate(menu_state_get_ptr());
   ok_on(i);

   sprintf(what, "%s: OK on A's row answered B", name);
   CHECK(seen_b.calls == 0, what);
   sprintf(what, "%s: A was not cancelled exactly once",
         name);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL, what);
   sprintf(what, "%s: B is no longer pending", name);
   CHECK(core_warning_pending(&kind, NULL) && kind == kind_b, what);
   sprintf(what, "%s: OK on A's row closed the screen",
         name);
   CHECK(shown(), what);

   pump(3);
   sprintf(what, "%s: B's screen is not shown", name);
   CHECK(shown(), what);
   sprintf(what, "%s: the screen lacks B's choices", name);
   CHECK(row(kind_b == CORE_WARNING_LOAD
            ? MENU_ENUM_LABEL_CORE_WARNING_LOAD_ONCE
            : MENU_ENUM_LABEL_CORE_WARNING_INSTALL_TRUST) != NO_ROW, what);
   sprintf(what, "%s: B's screen doesn't start on Cancel", name);
   CHECK(cursor_on_cancel(), what);

   /* B's own rows answer B */
   ok_on(row(MENU_ENUM_LABEL_CORE_WARNING_CANCEL));
   pump(3);
   sprintf(what, "%s: Cancel on B's rows didn't answer B",
         name);
   CHECK(seen_b.calls == 1 && seen_b.choice == CORE_WARNING_CANCEL, what);
   sprintf(what, "%s: B's callback ran under its screen", name);
   CHECK(!seen_b.on_top, what);
   sprintf(what, "%s: the screen stayed after Cancel", name);
   CHECK(!shown() && !core_warning_pending(NULL, NULL), what);
}

/* A tab switch flushes the stack: the request comes back */
static void lane_flush(void)
{
   struct core_check a;
   memset(&seen_a, 0, sizeof(seen_a));
   fake_check(&a, "a_libretro.so", 0x43, CORE_CHECK_UNSIGNED);
   CHECK(core_warning_push(&a, CORE_WARNING_INSTALL, "/cores/a_libretro.so",
            warn_cb, &seen_a), "fixture: push refused");
   pump(3);
   CHECK(shown(), "flush: the screen is not shown");
   menu_entries_flush_stack(MENU_ENUM_LABEL_MAIN_MENU_STR, 0);
   CHECK(!shown(), "fixture: the flush left the screen");
   pump(3);
   CHECK(shown(), "flush: the pending request's screen didn't come back");
   CHECK(seen_a.calls == 0, "flush: the request was answered");
   ok_on(row(MENU_ENUM_LABEL_CORE_WARNING_CANCEL));
   pump(3);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "flush: Cancel didn't answer the request");
   CHECK(!shown(), "flush: the screen stayed after Cancel");
}

/* Info and Back on the screen, and a request answered before its
 * screen came up */
static void lane_back(void)
{
   struct core_check a, b;
   size_t depth;
   memset(&seen_a, 0, sizeof(seen_a));
   memset(&seen_b, 0, sizeof(seen_b));
   fake_check(&a, "a_libretro.so", 0x46, CORE_CHECK_UNSIGNED);
   fake_check(&b, "b_libretro.so", 0x47, CORE_CHECK_UNSIGNED);
   CHECK(core_warning_push(&a, CORE_WARNING_LOAD, "/cores/a_libretro.so",
            warn_cb, &seen_a), "fixture: push refused");
   pump(3);
   CHECK(shown(), "back: the screen is not shown");
   depth = stack_size();
   act_on(row(MENU_ENUM_LABEL_CORE_WARNING_LOAD_ONCE), MENU_ACTION_INFO);
   pump(3);
   CHECK(shown() && stack_size() == depth,
         "back: Info put a screen over the warning");
   act_on(row(MENU_ENUM_LABEL_CORE_WARNING_CANCEL), MENU_ACTION_CANCEL);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "back: Back didn't cancel the request");
   CHECK(!seen_a.on_top, "back: the callback ran under its screen");
   pump(3);
   CHECK(!shown(), "back: the screen stayed after Back");

   /* B arrives under A's screen, and Back there cancels B before the
    * screen is built for it */
   memset(&seen_a, 0, sizeof(seen_a));
   CHECK(core_warning_push(&a, CORE_WARNING_LOAD, "/cores/a_libretro.so",
            warn_cb, &seen_a), "fixture: push A refused");
   pump(3);
   CHECK(core_warning_push(&b, CORE_WARNING_LOAD, "/cores/b_libretro.so",
            warn_cb, &seen_b), "fixture: push B refused");
   act_on(row(MENU_ENUM_LABEL_CORE_WARNING_CANCEL), MENU_ACTION_CANCEL);
   CHECK(seen_b.calls == 1 && seen_b.choice == CORE_WARNING_CANCEL,
         "fixture: Back didn't cancel B");
   pump(3);
   CHECK(!shown(), "back: a warning came up with nothing pending");
}

/* A driver that shows no screens (null): no new request, and a pending
 * one whose screen can't come back doesn't take the frame */
static void lane_null(void)
{
   struct core_check a, b;
   struct menu_state *menu_st = menu_state_get_ptr();
   const menu_ctx_driver_t *real;
   menu_ctx_driver_t null_ctx;
   unsigned i;
   memset(&seen_a, 0, sizeof(seen_a));
   memset(&seen_b, 0, sizeof(seen_b));
   fake_check(&a, "a_libretro.so", 0x44, CORE_CHECK_UNSIGNED);
   fake_check(&b, "b_libretro.so", 0x45, CORE_CHECK_UNSIGNED);
   CHECK(core_warning_push(&a, CORE_WARNING_LOAD, "/cores/a_libretro.so",
            warn_cb, &seen_a), "fixture: push refused");
   pump(3);
   CHECK(shown(), "null: the screen is not shown");
   menu_entries_flush_stack(MENU_ENUM_LABEL_MAIN_MENU_STR, 0);

   real                = menu_st->driver_ctx;
   null_ctx            = *real;
   null_ctx.ident      = "null";
   menu_st->driver_ctx = &null_ctx;
   CHECK(!core_warning_push(&b, CORE_WARNING_LOAD, "/cores/b_libretro.so",
            warn_cb, &seen_b), "null: a push was taken");
   CHECK(seen_a.calls == 0 && seen_b.calls == 0,
         "null: a refused push ran a callback");
   for (i = 0; i < 3; i++)
      CHECK(!menu_core_warning_iterate(menu_st),
            "null: a push that didn't land took the frame");
   CHECK(!shown(), "fixture: the null driver pushed the screen");
   menu_st->driver_ctx = real;

   pump(3);
   CHECK(shown(), "null: the screen didn't come back with a real driver");
   ok_on(row(MENU_ENUM_LABEL_CORE_WARNING_CANCEL));
   pump(3);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "null: Cancel didn't answer the request");
}

int main(int argc, char *argv[])
{
   char dir[400];
   char pid[32];
   char cfg_path[512];
   char store[512];
   char *rarch_argv[8] = {0};
   int rarch_argc      = 0;
   FILE *f;
   (void)argc;
   (void)argv;

   {
      const char *tmp = getenv("TMPDIR");
      if (!tmp || !*tmp)
         tmp = getenv("TEMP");
      if (!tmp || !*tmp)
         tmp = "/tmp";
      sprintf(pid, "/core_warning_%ld", (long)getpid());
      strlcpy(dir, tmp, sizeof(dir));
      strlcat(dir, pid, sizeof(dir));
   }
   if (!path_mkdir(dir))
      return 1;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   runloop_msg_queue_init();

   strlcpy(cfg_path, dir, sizeof(cfg_path));
   strlcat(cfg_path, "/harness.cfg", sizeof(cfg_path));
   if (!(f = fopen(cfg_path, "wb")))
      return 1;
   fprintf(f, "video_driver = \"null\"\n");
   fprintf(f, "audio_driver = \"null\"\n");
   fprintf(f, "input_driver = \"null\"\n");
   fprintf(f, "input_joypad_driver = \"null\"\n");
   fprintf(f, "menu_driver = \"rgui\"\n");
   fprintf(f, "video_threaded = \"false\"\n");
   fprintf(f, "config_save_on_exit = \"false\"\n");
   fclose(f);

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;
   rarch_argv[rarch_argc++] = (char*)"--menu";

   /* Opened first, so main_init leaves the user's store and cores alone */
   strlcpy(store, dir, sizeof(store));
   strlcat(store, "/core_trust", sizeof(store));
   core_trust_runtime_init(store, NULL, NULL);

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   pump(5);
   if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   pump(2);

   CHECK(core_trust_runtime_active(),
         "fixture: the trust store is inactive (no test key?)");
   if (core_trust_runtime_active())
   {
      lane_newer(CORE_WARNING_INSTALL,
            MENU_ENUM_LABEL_CORE_WARNING_INSTALL_TRUST, CORE_WARNING_LOAD,
            "install, then a load");
      lane_newer(CORE_WARNING_LOAD,
            MENU_ENUM_LABEL_CORE_WARNING_ALWAYS_TRUST, CORE_WARNING_LOAD,
            "load, then another load");
      lane_flush();
      lane_back();
      lane_null();
   }

   main_exit(NULL);

   remove(cfg_path);
   remove(store);
   path_rmdir(dir);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "[pass] core_warning\n");
   return 0;
}
