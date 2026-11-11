/* The Security Warning is the menu's confirm box with named buttons.
 *
 * Links the shipping RetroArch objects with only main() replaced (the
 * load_content_filter pattern), core_signature.c built with the public
 * test key so the trust store is active, and boots the menu on the
 * null drivers, with a config and trust store of its own. Keys go in
 * through menu_driver_iterate(), the path a pad's input takes. Then:
 *  - the box says what the warning says and offers the request's
 *    choices, Cancel first and highlighted, and pushes no screen;
 *  - Left and Right move the highlight and stop at the ends; Up, Down
 *    and Select do nothing; OK presses the highlighted button; Back
 *    and Info are Cancel; the box is down before the callback runs
 *    and the list under it stays;
 *  - a press on a box an older request put up chooses nothing, and the
 *    box shows the newer request from Cancel;
 *  - a request answered elsewhere takes the box down; a stack flush
 *    leaves it up; a driver that shows nothing never gets it;
 *  - an open Cancel/OK confirm is answered first, keeps Cancel and OK
 *    with nothing highlighted, and ignores Left and Right; its OK's
 *    command still runs; one set while the box is up takes over;
 *  - an entry capture or the on-screen keyboard is answered first;
 *  - with narration on, the box is spoken when it comes up or is
 *    replaced, and so is the button the highlight moves to;
 *  - a command interface's buttons and MENU requests and the network
 *    pad neither answer nor move the box, and still drive the menu
 *    with nothing pending;
 *  - Settings > Core > Check Core Signatures: Off asks even with
 *    confirmations off and Back keeps the checks; On and Start ask
 *    nothing; a read-only store changes nothing; an inactive one hides
 *    the row.
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
#include <retro_timers.h>
#include <features/features_cpu.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../command.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../gfx/gfx_animation.h"
#include "../../../gfx/gfx_display.h"
#include "../../../input/input_driver.h"
#include "../../../menu/menu_cbs.h"
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

/* The confirm box as the drivers see it */
static menu_dialog_buttons_t box(void)
{
   menu_dialog_buttons_t b;
   menu_dialog_get_buttons(menu_state_get_ptr(), &b);
   return b;
}

/* The Security Warning's box is up */
static bool shown(void)
{
   menu_dialog_buttons_t b = box();
   return b.count && !b.classic;
}

/* The box offers exactly these, left to right */
static bool offers(unsigned n, const enum msg_hash_enums *want)
{
   menu_dialog_buttons_t b = box();
   unsigned i;
   if (b.count != n || b.classic)
      return false;
   for (i = 0; i < n; i++)
      if (b.label[i] != (unsigned)want[i])
         return false;
   return true;
}

/* The pad icon on each of the box's n buttons, left to right */
static bool icons(unsigned n, unsigned i0, unsigned i1, unsigned i2)
{
   menu_dialog_buttons_t b = box();
   unsigned want[3];
   unsigned i;
   want[0] = i0;
   want[1] = i1;
   want[2] = i2;
   if (b.count != n)
      return false;
   for (i = 0; i < n; i++)
      if (b.icon[i] != want[i])
         return false;
   return true;
}

/* The box's text, as the last frame composed it, holds msg */
static bool box_says(enum msg_hash_enums msg)
{
   menu_handle_t *menu = menu_state_get_ptr()->driver_data;
   return menu && strstr(menu->menu_state_msg, msg_hash_to_str(msg));
}

/* The menu's own input: what a pad's button press becomes */
static void answer(enum menu_action action)
{
   menu_driver_iterate(menu_state_get_ptr(), disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), action, cpu_features_get_time_usec());
   pump(3);
}

static void push_core_settings(void)
{
   generic_action_ok_displaylist_push("", NULL, "", 0, 0, 0,
         ACTION_OK_DL_CORE_SETTINGS_LIST);
   pump(3);
}

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
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
   /* The box was still up when the callback ran */
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

static const enum msg_hash_enums install_buttons[] = {
   MENU_ENUM_LABEL_VALUE_CORE_WARNING_CANCEL,
   MENU_ENUM_LABEL_VALUE_CORE_WARNING_INSTALL_TRUST
};
static const enum msg_hash_enums load_buttons[] = {
   MENU_ENUM_LABEL_VALUE_CORE_WARNING_CANCEL,
   MENU_ENUM_LABEL_VALUE_CORE_WARNING_LOAD_ONCE,
   MENU_ENUM_LABEL_VALUE_CORE_WARNING_ALWAYS_TRUST
};
static const enum msg_hash_enums cancel_only[] = {
   MENU_ENUM_LABEL_VALUE_CORE_WARNING_CANCEL
};

static void push(struct seen *s, enum core_warning_kind kind,
      uint8_t byte, enum core_check_result result)
{
   struct core_check c;
   memset(s, 0, sizeof(*s));
   fake_check(&c, "a_libretro.so", byte, result);
   CHECK(core_warning_push(&c, kind, "/cores/a_libretro.so", warn_cb, s),
         "fixture: push refused");
}

/* What the box says and offers, and that it is no screen of its own */
static void lane_open(void)
{
   size_t depth;
   menu_entries_flush_stack(MENU_ENUM_LABEL_MAIN_MENU_STR, 0);
   pump(3);
   depth = stack_size();

   push(&seen_a, CORE_WARNING_INSTALL, 0x40, CORE_CHECK_UNSIGNED);
   pump(3);
   CHECK(shown(), "open: the box is not up");
   CHECK(stack_size() == depth, "open: the warning pushed a screen");
   CHECK(offers(2, install_buttons),
         "open: an install doesn't offer Cancel, Install and Trust");
   CHECK(box().highlight == 0, "open: Cancel isn't highlighted");
   CHECK(   box_says(MENU_ENUM_LABEL_VALUE_CORE_WARNING)
         && box_says(MSG_CORE_WARNING_UNSIGNED)
         && box_says(MSG_CORE_WARNING_FULL_ACCESS)
         && box_says(MSG_CORE_WARNING_TURN_OFF),
         "open: the box text lacks a part");
   /* English is about 344 B of the 1024; a translation twice the bytes
    * still fits whole */
   CHECK(strlen(menu_state_get_ptr()->driver_data->menu_state_msg) < 512,
         "open: the box text is near the buffer size");
   answer(MENU_ACTION_CANCEL);
   CHECK(seen_a.calls == 1, "fixture: Back didn't answer");

   push(&seen_a, CORE_WARNING_LOAD, 0x41, CORE_CHECK_OLDER);
   pump(3);
   CHECK(offers(3, load_buttons),
         "open: a load doesn't offer Cancel, Load Once, Always Trust");
   CHECK(box_says(MSG_CORE_WARNING_OLDER),
         "open: the box doesn't give the reason");
   answer(MENU_ACTION_CANCEL);

   push(&seen_a, CORE_WARNING_LOAD, 0x42, CORE_CHECK_ERROR);
   pump(3);
   CHECK(offers(1, cancel_only),
         "open: a core that couldn't be read offers more than Cancel");
   CHECK(icons(1, MENU_DIALOG_ICON_CANCEL, 0, 0),
         "open: Cancel alone lacks its icon");
   answer(MENU_ACTION_CANCEL);
   CHECK(!shown() && seen_a.calls == 1, "open: Back left the box up");
}

/* Left/Up and Right/Down stop at the ends, Select does nothing, OK
 * presses the highlighted button with the box down first, and the list
 * under the box stays */
static void lane_keys(void)
{
   size_t depth;
   push_core_settings();
   depth = stack_size();

   push(&seen_a, CORE_WARNING_LOAD, 0x43, CORE_CHECK_UNSIGNED);
   pump(3);
   answer(MENU_ACTION_LEFT);
   CHECK(box().highlight == 0, "keys: Left at Cancel moved");
   /* Cancel always shows its icon, the others OK's, dimmed unless
    * highlighted */
   CHECK(icons(3, MENU_DIALOG_ICON_CANCEL, MENU_DIALOG_ICON_OK_DIM,
         MENU_DIALOG_ICON_OK_DIM),
         "keys: with Cancel highlighted, the others' OK icons aren't dimmed");
   answer(MENU_ACTION_RIGHT);
   CHECK(box().highlight == 1, "keys: Right didn't move to Load Once");
   CHECK(icons(3, MENU_DIALOG_ICON_CANCEL, MENU_DIALOG_ICON_OK,
         MENU_DIALOG_ICON_OK_DIM),
         "keys: OK's full icon isn't on Load Once alone with Cancel's kept");
   answer(MENU_ACTION_RIGHT);
   answer(MENU_ACTION_RIGHT);
   CHECK(box().highlight == 2, "keys: Right didn't stop at the last button");
   CHECK(icons(3, MENU_DIALOG_ICON_CANCEL, MENU_DIALOG_ICON_OK_DIM,
         MENU_DIALOG_ICON_OK),
         "keys: OK's full icon didn't follow the highlight to the last button");
   answer(MENU_ACTION_DOWN);
   CHECK(box().highlight == 2, "keys: Down didn't stop at the last button");
   answer(MENU_ACTION_UP);
   CHECK(box().highlight == 1, "keys: Up didn't move back");
   answer(MENU_ACTION_DOWN);
   CHECK(box().highlight == 2, "keys: Down didn't move on");
   answer(MENU_ACTION_SELECT);
   CHECK(box().highlight == 2 && shown() && seen_a.calls == 0,
         "keys: Up, Down or Select pressed a button");
   answer(MENU_ACTION_LEFT);
   CHECK(box().highlight == 1, "keys: Left didn't move back");
   answer(MENU_ACTION_OK);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_LOAD_ONCE,
         "keys: OK didn't press the highlighted button");
   CHECK(!seen_a.on_top, "keys: the callback ran under the box");
   CHECK(!shown(), "keys: the box stayed after OK");
   CHECK(stack_size() == depth, "keys: answering popped the list under the box");

   push(&seen_a, CORE_WARNING_LOAD, 0x44, CORE_CHECK_UNSIGNED);
   pump(3);
   answer(MENU_ACTION_RIGHT);
   answer(MENU_ACTION_RIGHT);
   answer(MENU_ACTION_OK);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_ALWAYS_TRUST,
         "keys: OK on the last button didn't press it");

   push(&seen_a, CORE_WARNING_INSTALL, 0x45, CORE_CHECK_UNSIGNED);
   pump(3);
   answer(MENU_ACTION_RIGHT);
   answer(MENU_ACTION_RIGHT);
   CHECK(box().highlight == 1, "keys: Right went past Install and Trust");
   answer(MENU_ACTION_OK);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_INSTALL_AND_TRUST,
         "keys: OK on Install and Trust didn't press it");
   CHECK(stack_size() == depth, "keys: the list under the box went away");
}

/* Back and Info are Cancel, whatever is highlighted */
static void lane_back(void)
{
   size_t depth = stack_size();
   push(&seen_a, CORE_WARNING_LOAD, 0x46, CORE_CHECK_UNSIGNED);
   pump(3);
   answer(MENU_ACTION_RIGHT);
   answer(MENU_ACTION_CANCEL);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "back: Back didn't cancel");
   CHECK(!seen_a.on_top, "back: the callback ran under the box");
   CHECK(!shown(), "back: the box stayed after Back");

   push(&seen_a, CORE_WARNING_LOAD, 0x47, CORE_CHECK_UNSIGNED);
   pump(3);
   answer(MENU_ACTION_RIGHT);
   answer(MENU_ACTION_INFO);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "back: Info didn't cancel");
   CHECK(stack_size() == depth, "back: Info put a screen up");
}

/* B is pushed while A's box is up; the next frame's input lands on A's
 * box before the box catches up: it chooses nothing, and the box shows
 * B from Cancel */
static void lane_newer(enum menu_action action, const char *name)
{
   char what[160];
   push(&seen_a, CORE_WARNING_INSTALL, 0x48, CORE_CHECK_UNSIGNED);
   pump(3);
   answer(MENU_ACTION_RIGHT);
   push(&seen_b, CORE_WARNING_LOAD, 0x49, CORE_CHECK_MODIFIED);
   menu_driver_iterate(menu_state_get_ptr(), disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), action, cpu_features_get_time_usec());
   sprintf(what, "%s on the older box: A wasn't cancelled exactly once", name);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL, what);
   sprintf(what, "%s on the older box answered B", name);
   CHECK(seen_b.calls == 0 && core_warning_pending(NULL, NULL), what);
   sprintf(what, "%s on the older box: the box doesn't show B from Cancel",
         name);
   CHECK(offers(3, load_buttons) && box().highlight == 0, what);
   pump(3);
   answer(MENU_ACTION_CANCEL);
   sprintf(what, "%s: Back on B's box didn't answer B", name);
   CHECK(seen_b.calls == 1 && seen_b.choice == CORE_WARNING_CANCEL, what);
}

/* A request answered away from the box takes the box down */
static void lane_gone(void)
{
   push(&seen_a, CORE_WARNING_INSTALL, 0x4a, CORE_CHECK_UNSIGNED);
   pump(3);
   CHECK(shown(), "gone: the box is not up");
   CHECK(core_warning_choose(CORE_WARNING_CANCEL), "fixture: choose refused");
   pump(3);
   CHECK(!shown() && box().count == 0,
         "gone: the box stayed with nothing pending");

   /* The same with no menu handle at that moment */
   {
      struct menu_state *menu_st = menu_state_get_ptr();
      menu_handle_t *handle;
      push(&seen_a, CORE_WARNING_INSTALL, 0x66, CORE_CHECK_UNSIGNED);
      pump(3);
      CHECK(shown(), "gone: the second box is not up");
      handle              = menu_st->driver_data;
      menu_st->driver_data = NULL;
      core_warning_choose(CORE_WARNING_CANCEL);
      menu_core_warning_iterate(menu_st);
      menu_st->driver_data = handle;
      CHECK(box().count == 0,
            "gone: a box with no menu handle stayed after its request went");
      pump(3);
   }
}

/* A tab switch flushes the menu stack; the box isn't on it */
static void lane_flush(void)
{
   push(&seen_a, CORE_WARNING_INSTALL, 0x4b, CORE_CHECK_UNSIGNED);
   pump(3);
   menu_entries_flush_stack(MENU_ENUM_LABEL_MAIN_MENU_STR, 0);
   pump(3);
   CHECK(shown() && seen_a.calls == 0, "flush: the box went with the stack");
   answer(MENU_ACTION_CANCEL);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "flush: Back didn't answer");
}

/* A driver that shows nothing (null): no new request, and the box for
 * a pending one neither comes up nor takes the frame */
static void lane_null(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   const menu_ctx_driver_t *real;
   menu_ctx_driver_t null_ctx;
   struct core_check b;
   unsigned i;
   push(&seen_a, CORE_WARNING_LOAD, 0x4c, CORE_CHECK_UNSIGNED);
   memset(&seen_b, 0, sizeof(seen_b));
   fake_check(&b, "b_libretro.so", 0x4d, CORE_CHECK_UNSIGNED);

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
            "null: a box that can't come up took the frame");
   CHECK(!shown(), "null: the box came up on a driver that shows nothing");
   menu_st->driver_ctx = real;

   pump(3);
   CHECK(shown(), "null: the box didn't come up with a real driver");
   answer(MENU_ACTION_CANCEL);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "null: Back didn't answer");
}

/* Synthetic input: a command interface (its buttons and MENU requests)
 * and the network pad. The pad's poll reads no socket with every user
 * off, so remote_st_ptr holds what the frame sees. */
#ifdef HAVE_COMMAND
static command_t syn_cmd;
static int syn_slot = -1;
static unsigned syn_cmd_press;
static char syn_reply[160];

static void syn_cmd_poll(command_t *cmd)
{
   unsigned i;
   for (i = 0; i < 16; i++)
      if (syn_cmd_press & (1u << i))
         cmd->state[i] = true;
   syn_cmd_press = 0;
}

static void syn_cmd_reply(command_t *cmd, const char *data, size_t len)
{
   (void)cmd;
   if (len >= sizeof(syn_reply))
      len = sizeof(syn_reply) - 1;
   memcpy(syn_reply, data, len);
   syn_reply[len] = '\0';
}
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
static char syn_remote;
static bool syn_remote_users[MAX_USERS];
#endif

static void synthetic_install(void)
{
   input_driver_state_t *input_st = input_state_get_ptr();
#ifdef HAVE_COMMAND
   int j;
   memset(&syn_cmd, 0, sizeof(syn_cmd));
   syn_cmd.poll    = syn_cmd_poll;
   syn_cmd.replier = syn_cmd_reply;
   for (j = 0; j < MAX_CMD_DRIVERS && syn_slot < 0; j++)
      if (!input_st->command[j])
         syn_slot = j;
   CHECK(syn_slot >= 0, "fixture: no free command slot");
   if (syn_slot >= 0)
      input_st->command[syn_slot] = &syn_cmd;
#endif
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   {
      settings_t *settings = config_get_ptr();
      unsigned i;
      for (i = 0; i < MAX_USERS; i++)
      {
         syn_remote_users[i] = settings->bools.network_remote_enable_user[i];
         settings->bools.network_remote_enable_user[i] = false;
      }
      CHECK(!input_st->remote, "fixture: a network pad is open");
      input_st->remote = (input_remote_t*)&syn_remote;
   }
#endif
   (void)input_st;
}

static void synthetic_remove(void)
{
   input_driver_state_t *input_st = input_state_get_ptr();
#ifdef HAVE_COMMAND
   if (syn_slot >= 0)
      input_st->command[syn_slot] = NULL;
   syn_slot = -1;
#endif
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   {
      settings_t *settings = config_get_ptr();
      unsigned i;
      input_st->remote = NULL;
      for (i = 0; i < MAX_USERS; i++)
         settings->bools.network_remote_enable_user[i] = syn_remote_users[i];
   }
#endif
   (void)input_st;
}

/* A frame with buttons down on the command interface, then frames
 * with them up */
static void press_cmd(unsigned buttons)
{
#ifdef HAVE_COMMAND
   syn_cmd_press = buttons;
#endif
   pump(1);
   pump(3);
}

static void press_remote(unsigned buttons)
{
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   input_driver_state_t *input_st = input_state_get_ptr();
   input_st->remote_st_ptr.buttons[0] = buttons;
   pump(1);
   input_st->remote_st_ptr.buttons[0] = 0;
#endif
   pump(3);
}

/* A MENU request; true when it was refused */
static bool menu_request(const char *arg)
{
#ifdef HAVE_COMMAND
   syn_reply[0] = '\0';
   command_menu(&syn_cmd, arg);
   pump(4);
   return string_starts_with(syn_reply, "MENU ERROR");
#else
   return true;
#endif
}

#define BUTTON(id) (1u << (id))

static void lane_synthetic(void)
{
   static const unsigned dir[2] = {
      RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_LEFT
   };
   struct menu_state *menu_st = menu_state_get_ptr();
   char what[160];
   unsigned i;
   menu_entries_flush_stack(MENU_ENUM_LABEL_MAIN_MENU_STR, 0);
   pump(3);
   push(&seen_a, CORE_WARNING_LOAD, 0x4f, CORE_CHECK_UNSIGNED);
   pump(3);
   CHECK(shown(), "synthetic: the box is not up");
   answer(MENU_ACTION_RIGHT);
   CHECK(box().highlight == 1, "fixture: the user's Right didn't move");
   press_cmd(BUTTON(RETRO_DEVICE_ID_JOYPAD_A));
   CHECK(seen_a.calls == 0, "synthetic: a command's A answered the warning");
   press_remote(BUTTON(RETRO_DEVICE_ID_JOYPAD_A));
   CHECK(seen_a.calls == 0,
         "synthetic: the network pad's A answered the warning");
   CHECK(menu_request("a"), "synthetic: a MENU request wasn't refused");
   CHECK(seen_a.calls == 0, "synthetic: a MENU request answered the warning");
   /* Right and Left from each source; the user's keys put the highlight
    * back, so one leak fails one line */
   for (i = 0; i < 4; i++)
   {
      if (i < 2)
         press_cmd(BUTTON(dir[i]));
      else
         press_remote(BUTTON(dir[i - 2]));
      sprintf(what, "synthetic: %s's %s moved the highlight",
            i < 2 ? "a command" : "the network pad",
            (i & 1) ? "Left" : "Right");
      CHECK(box().highlight == 1, what);
      if (box().highlight > 1)
         answer(MENU_ACTION_LEFT);
      else if (box().highlight < 1)
         answer(MENU_ACTION_RIGHT);
   }
   CHECK(menu_request("right"), "synthetic: MENU right wasn't refused");
   CHECK(box().highlight == 1, "synthetic: MENU right moved the highlight");
   CHECK(shown() && seen_a.calls == 0, "synthetic: the box went away");
   answer(MENU_ACTION_CANCEL);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "synthetic: the user's Back didn't answer");

   /* Nothing pending: the same input drives the menu */
   CHECK(!shown(), "synthetic: the box stayed after Back");
   menu_st->selection_ptr = 0;
   press_cmd(BUTTON(RETRO_DEVICE_ID_JOYPAD_DOWN));
#ifdef HAVE_COMMAND
   CHECK(menu_st->selection_ptr == 1, "synthetic: a command's Down did nothing");
   CHECK(!menu_request("down"), "synthetic: MENU down was refused");
   CHECK(menu_st->selection_ptr == 2, "synthetic: MENU down did nothing");
#endif
   menu_st->selection_ptr = 0;
   press_remote(BUTTON(RETRO_DEVICE_ID_JOYPAD_DOWN));
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   CHECK(menu_st->selection_ptr == 1,
         "synthetic: the network pad's Down did nothing");
#endif
}

/* The row is an On/Off switch showing value; false with no row */
static bool setting_shows(enum msg_hash_enums value)
{
   menu_entry_t entry;
   size_t i = row(MENU_ENUM_LABEL_CORE_SIGNATURE_CHECK);
   if (i == NO_ROW)
      return false;
   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_VALUE_ENABLED;
   menu_entry_get(&entry, 0, i, NULL, true);
   return   entry.setting_type == ST_BOOL
         && string_is_equal(entry.value, msg_hash_to_str(value));
}

/* An open Cancel/OK confirm is answered first: Cancel and OK with
 * nothing highlighted, Left and Right ignored, then the warning */
static void lane_classic(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_dialog_buttons_t b;
   if (!core_trust_runtime_checks_on())
      core_trust_runtime_set_checks(true);
   push_core_settings();
   ok_on(row(MENU_ENUM_LABEL_CORE_SIGNATURE_CHECK));
   b = box();
   CHECK(   b.classic && b.count == 2 && b.highlight == -1
         && b.label[0] == (unsigned)MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CANCEL
         && b.label[1] == (unsigned)MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
         "classic: the turn-off confirm isn't Cancel and OK, nothing highlighted");
   CHECK(icons(2, MENU_DIALOG_ICON_CANCEL, MENU_DIALOG_ICON_OK, 0),
         "classic: Cancel and OK lack their icons");
   /* A confirm set over another starts with nothing under the pointer */
   menu_dialog_set_hover(menu_st, 1);
   CHECK(box().hover == 1, "fixture: the confirm's OK isn't hovered");
   menu_dialog_confirm_set(menu_st, MSG_CORE_SIGNATURE_CHECK_OFF_CONFIRM,
         CMD_EVENT_CORE_SIGNATURE_CHECK_OFF);
   CHECK(box().hover == -1,
         "classic: a confirm set over another kept OK hovered");
   push(&seen_a, CORE_WARNING_INSTALL, 0x4e, CORE_CHECK_UNSIGNED);
   pump(3);
   CHECK(!shown() && menu_st->dialog_st.confirm_cmd
         == CMD_EVENT_CORE_SIGNATURE_CHECK_OFF,
         "classic: the warning took over an open confirm");
   answer(MENU_ACTION_RIGHT);
   CHECK(menu_st->dialog_st.confirm_cmd == CMD_EVENT_CORE_SIGNATURE_CHECK_OFF
         && menu_st->dialog_st.button_highlight == -1,
         "classic: Right did something on Cancel and OK");
   answer(MENU_ACTION_CANCEL);
   CHECK(core_trust_runtime_checks_on(), "classic: Back turned the checks off");
   pump(3);
   CHECK(shown() && seen_a.calls == 0,
         "classic: the warning didn't come up after the confirm");
   answer(MENU_ACTION_CANCEL);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "classic: Back didn't answer the warning");

   /* OK on the confirm runs its command before the warning comes up */
   push_core_settings();
   ok_on(row(MENU_ENUM_LABEL_CORE_SIGNATURE_CHECK));
   push(&seen_a, CORE_WARNING_INSTALL, 0x65, CORE_CHECK_UNSIGNED);
   pump(3);
   answer(MENU_ACTION_OK);
   pump(3);
   CHECK(!core_trust_runtime_checks_on(),
         "classic: OK on the confirm was lost to the warning");
   CHECK(shown() && seen_a.calls == 0,
         "classic: the warning didn't come up after OK");
   answer(MENU_ACTION_CANCEL);
   core_trust_runtime_set_checks(true);
}

/* A Cancel/OK confirm set while the box is up (the first-run prompt at
 * menu init) takes over; the warning comes back once it is answered */
static void lane_override(void)
{
   menu_dialog_buttons_t b;
   if (!core_trust_runtime_checks_on())
      core_trust_runtime_set_checks(true);
   push_core_settings();
   push(&seen_a, CORE_WARNING_INSTALL, 0x62, CORE_CHECK_UNSIGNED);
   pump(3);
   CHECK(shown(), "override: the box is not up");
   menu_dialog_confirm_set(menu_state_get_ptr(),
         MSG_CORE_SIGNATURE_CHECK_OFF_CONFIRM,
         CMD_EVENT_CORE_SIGNATURE_CHECK_OFF);
   pump(3);
   b = box();
   CHECK(b.classic && b.count == 2,
         "override: a Cancel/OK confirm set while the warning was up didn't show");
   answer(MENU_ACTION_CANCEL);
   CHECK(core_trust_runtime_checks_on(), "override: Back turned the checks off");
   pump(3);
   CHECK(shown() && seen_a.calls == 0,
         "override: the warning didn't come back after the confirm");
   answer(MENU_ACTION_CANCEL);
   CHECK(seen_a.calls == 1 && seen_a.choice == CORE_WARNING_CANCEL,
         "override: Back didn't answer the warning");
}

/* An entry capture (buttons held on a row) or the on-screen keyboard is
 * answered first; the box comes up after it */
static void lane_capture(void)
{
   unsigned i;
   menu_entries_flush_stack(MENU_ENUM_LABEL_MAIN_MENU_STR, 0);
   pump(3);
   CHECK(input_entry_capture_start(1), "fixture: no entry capture");
   push(&seen_a, CORE_WARNING_INSTALL, 0x63, CORE_CHECK_UNSIGNED);
   pump(3);
   CHECK(!shown(), "capture: the box came up under an entry capture");
   /* Nothing held: the capture ends on its own after 5 s
    * (INPUT_ENTRY_CAPTURE_USEC) */
   for (i = 0; i < 600 && input_entry_capture_running(); i++)
   {
      pump(1);
      retro_sleep(20);
   }
   CHECK(!input_entry_capture_running(), "fixture: the capture didn't end");
   pump(3);
   CHECK(shown() && seen_a.calls == 0,
         "capture: the box didn't come up after the capture");
   answer(MENU_ACTION_CANCEL);

   CHECK(menu_input_dialog_start_search(), "fixture: no on-screen keyboard");
   push(&seen_a, CORE_WARNING_INSTALL, 0x64, CORE_CHECK_UNSIGNED);
   pump(3);
   CHECK(!shown(), "capture: the box came up under the on-screen keyboard");
   menu_input_dialog_end();
   pump(3);
   CHECK(shown() && seen_a.calls == 0,
         "capture: the box didn't come up after the keyboard");
   answer(MENU_ACTION_CANCEL);
}

#ifdef HAVE_ACCESSIBILITY
static char spoken[8][256];
static unsigned spoken_count;

static bool spoke(int speed, const char *text, int priority)
{
   (void)speed;
   (void)priority;
   if (text && spoken_count < 8)
      strlcpy(spoken[spoken_count++], text, sizeof(spoken[0]));
   return true;
}

static bool spoken_has(enum msg_hash_enums msg)
{
   unsigned i;
   for (i = 0; i < spoken_count; i++)
      if (strstr(spoken[i], msg_hash_to_str(msg)))
         return true;
   return false;
}

/* With narration on: the box's text when it comes up and again when a
 * newer request replaces it, and the button the highlight moves to */
static void lane_narration(void)
{
   frontend_ctx_driver_t *real = frontend_state_get_ptr()->current_frontend_ctx;
   frontend_ctx_driver_t fake;
   settings_t *settings        = config_get_ptr();
   CHECK(real != NULL, "fixture: no frontend driver");
   if (!real)
      return;
   fake                     = *real;
   fake.accessibility_speak = spoke;
   frontend_state_get_ptr()->current_frontend_ctx = &fake;
   configuration_set_bool(settings, settings->bools.accessibility_enable, true);

   spoken_count = 0;
   push(&seen_a, CORE_WARNING_INSTALL, 0x60, CORE_CHECK_UNSIGNED);
   pump(3);
   CHECK(spoken_has(MSG_CORE_WARNING_UNSIGNED),
         "narration: the box's text wasn't spoken");
   spoken_count = 0;
   push(&seen_b, CORE_WARNING_LOAD, 0x61, CORE_CHECK_MODIFIED);
   pump(3);
   CHECK(spoken_has(MSG_CORE_WARNING_MODIFIED),
         "narration: the newer request's text wasn't spoken");
   spoken_count = 0;
   answer(MENU_ACTION_RIGHT);
   CHECK(spoken_has(MENU_ENUM_LABEL_VALUE_CORE_WARNING_LOAD_ONCE),
         "narration: the highlighted button wasn't spoken");
   answer(MENU_ACTION_CANCEL);

   configuration_set_bool(settings, settings->bools.accessibility_enable, false);
   frontend_state_get_ptr()->current_frontend_ctx = real;
}
#endif

static void lane_setting(const char *dir)
{
   char path[512];
   struct menu_state *menu_st = menu_state_get_ptr();
   settings_t *settings       = config_get_ptr();
   configuration_set_bool(settings, settings->bools.menu_show_confirm,
         false);
   if (!core_trust_runtime_checks_on())
      core_trust_runtime_set_checks(true);
   push_core_settings();
   CHECK(setting_shows(MENU_ENUM_LABEL_VALUE_ON),
         "setting: the row doesn't show an On switch");
   ok_on(row(MENU_ENUM_LABEL_CORE_SIGNATURE_CHECK));
   CHECK(menu_st->dialog_st.confirm_cmd == CMD_EVENT_CORE_SIGNATURE_CHECK_OFF,
         "setting: Off didn't ask");
   CHECK(core_trust_runtime_checks_on(),
         "setting: Off didn't wait for the answer");
   press_cmd(BUTTON(RETRO_DEVICE_ID_JOYPAD_A));
   CHECK(menu_st->dialog_st.confirm_cmd == CMD_EVENT_CORE_SIGNATURE_CHECK_OFF,
         "setting: a command's A answered the confirm");
   press_remote(BUTTON(RETRO_DEVICE_ID_JOYPAD_A));
   CHECK(menu_st->dialog_st.confirm_cmd == CMD_EVENT_CORE_SIGNATURE_CHECK_OFF,
         "setting: the network pad's A answered the confirm");
   CHECK(menu_request("a"), "setting: a MENU request wasn't refused");
   CHECK(core_trust_runtime_checks_on()
         && menu_st->dialog_st.confirm_cmd == CMD_EVENT_CORE_SIGNATURE_CHECK_OFF,
         "setting: synthetic input answered the confirm");
   answer(MENU_ACTION_CANCEL);
   CHECK(!menu_st->dialog_st.confirm_cmd,
         "setting: Back left the confirm open");
   CHECK(core_trust_runtime_checks_on(), "setting: Back turned the checks off");
   ok_on(row(MENU_ENUM_LABEL_CORE_SIGNATURE_CHECK));
   answer(MENU_ACTION_OK);
   CHECK(!core_trust_runtime_checks_on(), "setting: OK left the checks on");
   CHECK(setting_shows(MENU_ENUM_LABEL_VALUE_OFF),
         "setting: the row doesn't show Off");
   act_on(row(MENU_ENUM_LABEL_CORE_SIGNATURE_CHECK), MENU_ACTION_START);
   CHECK(core_trust_runtime_checks_on(), "setting: Start didn't reset to On");
   CHECK(!menu_st->dialog_st.confirm_cmd, "setting: Start asked");
   core_trust_runtime_set_checks(false);
   ok_on(row(MENU_ENUM_LABEL_CORE_SIGNATURE_CHECK));
   CHECK(core_trust_runtime_checks_on(), "setting: On didn't turn them on");
   CHECK(!menu_st->dialog_st.confirm_cmd, "setting: On asked");

   /* Every change went to the harness's own store */
   task_queue_wait(NULL, NULL);
   strlcpy(path, dir, sizeof(path));
   strlcat(path, "/core_trust", sizeof(path));
   CHECK(path_is_valid(path), "setting: the harness's store wasn't written");

   /* A store that couldn't be read */
   core_trust_runtime_deinit();
   strlcpy(path, dir, sizeof(path));
   strlcat(path, "/unreadable_store", sizeof(path));
   CHECK(path_mkdir(path), "fixture: can't make the unreadable store");
   CHECK(core_trust_runtime_init(path, NULL, NULL)
         && core_trust_runtime_read_only(),
         "fixture: the store isn't read-only");
   push_core_settings();
   ok_on(row(MENU_ENUM_LABEL_CORE_SIGNATURE_CHECK));
   CHECK(setting_shows(MENU_ENUM_LABEL_VALUE_ON),
         "setting: read-only changed the checks or hid the row");
   CHECK(!menu_st->dialog_st.confirm_cmd, "setting: read-only asked");
   core_trust_runtime_deinit();
   path_rmdir(path);

   push_core_settings();
   CHECK(row(MENU_ENUM_LABEL_CORE_SIGNATURE_CHECK) == NO_ROW,
         "setting: the row shows with the store inactive");
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
   fprintf(f, "input_joypad_driver = \"test\"\n");
   fprintf(f, "menu_driver = \"rgui\"\n");
   fprintf(f, "video_threaded = \"false\"\n");
   fprintf(f, "config_save_on_exit = \"false\"\n");
   fprintf(f, "gamemode_enable = \"false\"\n");
   fprintf(f, "suspend_screensaver_enable = \"false\"\n");
   fprintf(f, "pause_nonactive = \"false\"\n");
   fprintf(f, "microphone_enable = \"false\"\n");
   fprintf(f, "microphone_driver = \"null\"\n");
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
      lane_open();
      lane_keys();
      lane_back();
      lane_newer(MENU_ACTION_OK, "OK");
      lane_newer(MENU_ACTION_CANCEL, "Back");
      lane_gone();
      lane_flush();
      lane_null();
      lane_classic();
      lane_override();
      lane_capture();
#ifdef HAVE_ACCESSIBILITY
      lane_narration();
#endif
      synthetic_install();
      lane_synthetic();
      lane_setting(dir);
      synthetic_remove();
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
