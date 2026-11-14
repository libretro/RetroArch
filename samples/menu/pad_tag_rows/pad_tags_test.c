/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Controllers of one name told apart. Links the shipping objects with
 * only main() replaced (the playlist_nav pattern) and drives the real
 * frame loop with the test joypad driver, whose script connects pads
 * named "(vid:pid) name@@port@@serial". Each phase checks the tag the
 * frontend gives each pad and the menu places that show it. Then,
 * while two pads share a name, a press of a pad's button marks its
 * rows "Pressed" for a second, on the menu's clock, the rows'
 * sublabels say a press finds one (never with Ozone), and Ozone's
 * footer hint is up on those pages only.
 * Requires a completed non-Qt build:
 *   ./configure --disable-qt && make
 *   samples/menu/pad_tag_rows/build.sh
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <lists/file_list.h>
#include <queues/task_queue.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>

#include "../../../msg_hash_lbl_str.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../menu/menu_cbs.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../input/input_driver.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../gfx/video_driver.h"
#include "../../../frontend/frontend_driver.h"

/* The script's frames: a phase is checked SETTLE frames after its last
 * step, with every connect it made applied. */
#define F_CONNECT     10
#define F_SWAP_OUT    60
#define F_SWAP_IN     80
#define F_SERIAL_OUT 130
#define F_SERIAL_IN  150
#define F_ORDER_OUT  170
#define F_ORDER_IN   180
#define F_CLASH_OUT  200
#define F_CLASH_IN   220
#define F_NOID_OUT   270
#define F_NOID_IN    290
#define F_SHOWN_OUT  340
#define F_SHOWN_IN   360
#define SETTLE         8

#define F_PRESS_ROW    410
#define F_PRESS_OTHER  460
#define F_PRESS_DI     490
#define F_PRESS_DD     540
#define F_PRESS_TOP    590
#define F_HELD         640
#define F_HELD_UP      660
#define F_HELD_AGAIN   670
#define F_PRESS_ADD    700
#define F_PRESS_STALE  740
#define F_PRESS_APART  780
/* button 10: no menu action is bound to it */
#define BUTTON_10     1024

#define PRESSED msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_PAD_PRESSED)

#define PAD "(045e:028e) Harness Pad"

/* The identities PAD's ports, address and serials give, and their tags
 * (samples/input/pad_tag pins both) */
#define ID_PORT1   0x8c6c6606UL
#define ID_PORT2   0x8b6c6473UL
#define ID_BT      0x3541d9beUL
#define ID_SER11   0xad61c0ffUL
#define ID_SER22   0xd271601fUL

#define BT_ADDR    "A4:C1:38:9F:2B:7E"
#define SER11      "AB-000123"
#define SER22      "CD-000123"
#define CLONE      "00:00:00:00:00:00"

static unsigned failures;
static unsigned steps;

static void check(int ok, const char *fmt, ...)
{
   va_list ap;
   if (ok)
      return;
   fprintf(stderr, "FAIL: ");
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   fprintf(stderr, "\n");
   failures++;
}

/* A test joypad step: action 1 adds pad @num named @str, 2 removes it,
 * 16 + pad presses the buttons in mask @num, 32 + pad releases them. */
static void step(FILE *f, unsigned frame, unsigned action, unsigned num,
      const char *str)
{
   fprintf(f, "%s{ \"action\": %u, \"param_num\": %u, ",
         steps++ ? "," : "", action, num);
   if (str)
      fprintf(f, "\"param_str\": \"%s\", ", str);
   fprintf(f, "\"frame\": %u }\n", frame);
}

static void write_script(FILE *f)
{
   fprintf(f, "[\n");
   step(f, F_CONNECT,    1, 0, PAD "@@port-1");
   step(f, F_CONNECT,    1, 1, PAD "@@port-2");
   step(f, F_CONNECT,    1, 2, "(045e:028e) Other Pad@@port-3");
   step(f, F_SWAP_OUT,   2, 0, NULL);
   step(f, F_SWAP_OUT,   2, 1, NULL);
   step(f, F_SWAP_IN,    1, 0, PAD "@@port-2@@" CLONE);
   step(f, F_SWAP_IN,    1, 1, PAD "@@port-1@@" CLONE);
   step(f, F_SERIAL_OUT, 2, 1, NULL);
   step(f, F_SERIAL_IN,  1, 1, PAD "@@port-1@@" BT_ADDR);
   step(f, F_ORDER_OUT,  2, 0, NULL);
   step(f, F_ORDER_OUT,  2, 1, NULL);
   step(f, F_ORDER_IN,   1, 0, PAD "@@port-11@@" SER11);
   step(f, F_ORDER_IN,   1, 1, PAD "@@port-22@@" SER22);
   step(f, F_CLASH_OUT,  2, 0, NULL);
   step(f, F_CLASH_OUT,  2, 1, NULL);
   step(f, F_CLASH_IN,   1, 0, PAD "@@port-22@@" SER22);
   step(f, F_CLASH_IN,   1, 1, PAD "@@port-11@@" SER11);
   step(f, F_NOID_OUT,   2, 1, NULL);
   step(f, F_NOID_IN,    1, 1, PAD);
   step(f, F_SHOWN_OUT,  2, 1, NULL);
   step(f, F_SHOWN_IN,   1, 1, "(045e:028e) Wireless Pad@@port-1");
   step(f, F_PRESS_ROW,       17, BUTTON_10, NULL);
   step(f, F_PRESS_ROW + 2,   33, BUTTON_10, NULL);
   step(f, F_PRESS_OTHER,     17, BUTTON_10, NULL);
   step(f, F_PRESS_OTHER + 2, 33, BUTTON_10, NULL);
   step(f, F_PRESS_DI,        16, BUTTON_10, NULL);
   step(f, F_PRESS_DI + 2,    32, BUTTON_10, NULL);
   step(f, F_PRESS_DD,        17, BUTTON_10, NULL);
   step(f, F_PRESS_DD + 2,    33, BUTTON_10, NULL);
   step(f, F_PRESS_TOP,       16, BUTTON_10, NULL);
   step(f, F_PRESS_TOP + 2,   32, BUTTON_10, NULL);
   step(f, F_HELD,            17, BUTTON_10, NULL);
   step(f, F_HELD_UP,         33, BUTTON_10, NULL);
   step(f, F_HELD_AGAIN,      17, BUTTON_10, NULL);
   step(f, F_HELD_AGAIN + 2,  33, BUTTON_10, NULL);
   step(f, F_PRESS_ADD,       17, BUTTON_10, NULL);
   step(f, F_PRESS_ADD + 2,   33, BUTTON_10, NULL);
   step(f, F_PRESS_STALE,     17, BUTTON_10, NULL);
   step(f, F_PRESS_STALE + 2, 33, BUTTON_10, NULL);
   step(f, F_PRESS_APART,     17, BUTTON_10, NULL);
   step(f, F_PRESS_APART + 2, 33, BUTTON_10, NULL);
   fprintf(f, "]\n");
}

/* Real frames, each followed by the task queue as rarch_main() runs
 * it, until the script is SETTLE frames past @frame and every connect
 * and disconnect it made has been applied. */
static void run_until(unsigned frame)
{
   unsigned n = 0;
   while (     (video_driver_get_frame_count() <= (uint64_t)(frame + SETTLE)
            || input_autoconfigure_pending())
         && n++ < 2000)
   {
      runloop_iterate();
      task_queue_check();
   }
   check(n <= 2000, "frame %u: the script never settled", frame);
}

/* @fmt with its %s the tag of @ident's pad; four calls fit in one
 * expression */
static const char *with_tag(const char *fmt, unsigned long ident)
{
   static char bufs[4][NAME_MAX_LENGTH];
   static unsigned next;
   char *s         = bufs[next++ & 3];
   const char *tag = "?";
   switch (ident)
   {
      case ID_PORT1:
         tag = "e6675c";
         break;
      case ID_PORT2:
         tag = "9c309a";
         break;
      case ID_BT:
         tag = "9f2b7e";
         break;
      case ID_SER11:
      case ID_SER22:
         tag = "000123";
         break;
   }
   snprintf(s, NAME_MAX_LENGTH, fmt, tag);
   return s;
}

static void check_tag(unsigned slot, const char *want, const char *when)
{
   char s[32];
   input_config_get_device_tag(slot, s, sizeof(s));
   check(string_is_equal(s, want), "%s: slot %u's tag is \"%s\", not \"%s\"",
         when, slot, s, want);
}

static void check_ident(unsigned slot, unsigned long want, const char *when)
{
   unsigned long got = (unsigned long)input_config_get_device_ident(slot);
   check(got == want, "%s: slot %u's identity is %08lx, not %08lx",
         when, slot, got, want);
}

/* An unplugged @slot keeps no identity or tag */
static void check_gone(unsigned slot, const char *when)
{
   const char *tag = input_config_get_device_ident_tag(slot);
   check_ident(slot, 0, when);
   check(string_is_empty(tag), "%s: slot %u keeps the tag \"%s\"",
         when, slot, tag);
}

#define DEVICE_INDEX_1 "input_player1_joypad_index"
#define DEVICE_INDEX_2 "input_player2_joypad_index"

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

/* Index of the row labelled @label, or -1 */
static int row(const char *label)
{
   size_t i;
   file_list_t *buf = selection_buf();
   for (i = 0; buf && i < buf->size; i++)
      if (buf->list[i].label && string_is_equal(buf->list[i].label, label))
         return (int)i;
   return -1;
}

/* Index of the first row of type @type, or -1 */
static int row_of_type(unsigned type)
{
   size_t i;
   file_list_t *buf = selection_buf();
   for (i = 0; buf && i < buf->size; i++)
      if (buf->list[i].type == type)
         return (int)i;
   return -1;
}

/* The Device Index dropdown's entry for @slot, or -1 */
static int dropdown_item(unsigned slot)
{
   size_t i;
   file_list_t *buf = selection_buf();
   for (i = 0; buf && i < buf->size; i++)
      if (     buf->list[i].type == MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM
            && buf->list[i].entry_idx == slot)
         return (int)i;
   return -1;
}

/* Add Controller's item for @slot, or -1 */
static int add_item(unsigned slot)
{
   size_t i;
   file_list_t *buf = selection_buf();
   for (i = 0; buf && i < buf->size; i++)
      if (     buf->list[i].type == MENU_SETTING_DROPDOWN_ITEM_INPUT_PLAYS_AS_ADD
            && buf->list[i].entry_idx == slot)
         return (int)i;
   return -1;
}

static const char *row_path(int i)
{
   file_list_t *buf = selection_buf();
   if (!buf || i < 0 || (size_t)i >= buf->size || !buf->list[i].path)
      return "";
   return buf->list[i].path;
}

/* Row @i's value as the menu drivers draw it */
static void row_value(int i, char *s, size_t len)
{
   menu_entry_t entry;
   *s = '\0';
   if (i < 0)
      return;
   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_VALUE_ENABLED;
   menu_entry_get(&entry, 0, (size_t)i, NULL, true);
   strlcpy(s, entry.value, len);
}

static void row_sublabel(int i, char *s, size_t len)
{
   menu_entry_t entry;
   *s = '\0';
   if (i < 0)
      return;
   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
   menu_entry_get(&entry, 0, (size_t)i, NULL, true);
   strlcpy(s, entry.sublabel, len);
}

/* Input Information's first row for @slot, the one naming it, or -1 */
static int info_row(unsigned slot)
{
   size_t i;
   file_list_t *buf = selection_buf();
   for (i = 0; buf && i < buf->size; i++)
   {
      menu_file_list_cbs_t *cbs =
         (menu_file_list_cbs_t*)buf->list[i].actiondata;
      if (     cbs
            && cbs->enum_idx == MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY
            && buf->list[i].entry_idx == slot)
         return (int)i;
   }
   return -1;
}

/* One menu frame on the real clock; no video frame, no input poll */
static void menu_frame(void)
{
   menu_driver_iterate(menu_state_get_ptr(), disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), MENU_ACTION_NOOP, cpu_features_get_time_usec());
}

static void open_top(void)
{
   menu_entries_flush_stack(MENU_ENUM_LABEL_MAIN_MENU_STR, 0);
   menu_frame();
}

/* What OK on Settings > Input > Controllers does */
static void open_controllers(void)
{
   open_top();
   generic_action_ok_displaylist_push("", NULL, "", 0, 0, 0,
         ACTION_OK_DL_INPUT_CONTROLLERS_LIST);
   menu_frame();
}

/* RetroPad Binds > Port @n Controls */
static void open_port_controls(unsigned n)
{
   char label[32];
   open_top();
   snprintf(label, sizeof(label), "%u_input_binds_list", n);
   generic_action_ok_displaylist_push("", NULL, label, 0, 0, 0,
         ACTION_OK_DL_USER_BINDS_LIST);
   menu_frame();
}

/* Information > Input Information */
static void open_input_info(void)
{
   open_top();
   generic_action_ok_displaylist_push("", NULL,
         MENU_ENUM_LABEL_INPUT_INFORMATION_STR, 0, 0, 0,
         ACTION_OK_DL_PUSH_DEFAULT);
   menu_frame();
}

/* Quick Menu > Controls > Port @n Controls */
static void open_remap_port(unsigned n)
{
   char label[16];
   open_top();
   snprintf(label, sizeof(label), "%u", n - 1);
   generic_action_ok_displaylist_push("", NULL, label, 0, 0, 0,
         ACTION_OK_DL_REMAPPINGS_PORT_LIST);
   menu_frame();
}

/* OK on row @i, as the menu presses it */
static void press_ok(int i)
{
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *buf          = selection_buf();
   if (buf && i >= 0 && (size_t)i < buf->size)
      cbs = (menu_file_list_cbs_t*)buf->list[i].actiondata;
   check(cbs && cbs->action_ok, "fixture: row %d has no OK", i);
   if (!cbs || !cbs->action_ok)
      return;
   menu_state_get_ptr()->selection_ptr = (size_t)i;
   cbs->action_ok(buf->list[i].path, buf->list[i].label,
         buf->list[i].type, (size_t)i, buf->list[i].entry_idx);
   menu_frame();
}

/* RetroPad Binds > Port @n Controls > Controllers */
static void open_port_controllers(unsigned n)
{
   open_port_controls(n);
   press_ok(row(MENU_ENUM_LABEL_INPUT_PORT_CONTROLLERS_STR));
}

static void open_device_index_dropdown(void)
{
   open_port_controls(1);
   press_ok(row(DEVICE_INDEX_1));
}

/* Slot @slot's Plays As row on the open list reads @want */
static void check_row(unsigned slot, const char *want, const char *when)
{
   int i = row_of_type(MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + slot);
   check(i >= 0 && string_is_equal(row_path(i), want),
         "%s: slot %u's row reads \"%s\", not \"%s\"", when, slot,
         i >= 0 ? row_path(i) : "(no row)", want);
}

static void check_controllers(const char *s0, const char *s1,
      const char *s2, const char *when)
{
   open_controllers();
   check_row(0, s0, when);
   check_row(1, s1, when);
   check_row(2, s2, when);
}

/* Port 1 Controls' Device Index reads @want */
static void check_device_index(const char *want, const char *when)
{
   char v[NAME_MAX_LENGTH];
   open_port_controls(1);
   row_value(row(DEVICE_INDEX_1), v, sizeof(v));
   check(string_is_equal(v, want),
         "%s: Device Index reads \"%s\", not \"%s\"", when, v, want);
}

/* Input Information names slot @slot @name; with @display, its
 * sublabel and RGUI's row below it give that display name */
static void check_input_info(unsigned slot, const char *name,
      const char *display, const char *when)
{
   char want[NAME_MAX_LENGTH];
   char v[NAME_MAX_LENGTH];
   const char *config = input_config_get_device_config_name(slot);
   size_t _len;

   open_input_info();
   _len = snprintf(want, sizeof(want),
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME),
         (int)slot + 1, name);
   if (!input_config_get_device_autoconfigured(slot) && _len < sizeof(want))
      snprintf(want + _len, sizeof(want) - _len, " (%s)",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NO_PROFILE));
   check(string_is_equal(row_path(info_row(slot)), want),
         "%s: Input Information names slot %u \"%s\", not \"%s\"",
         when, slot, row_path(info_row(slot)), want);
   if (!display)
      return;
   snprintf(want, sizeof(want),
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO), display,
         config ? config : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
         input_config_get_device_vid(slot),
         input_config_get_device_pid(slot));
   row_sublabel(info_row(slot), v, sizeof(v));
   check(string_is_equal(v, want),
         "%s: Input Information's sublabel for slot %u is \"%s\", not \"%s\"",
         when, slot, v, want);
   snprintf(want, sizeof(want), "- Device Display Name: %s", display);
   check(info_row(slot) >= 0
         && string_is_equal(row_path(info_row(slot) + 1), want),
         "%s: Input Information's RGUI row for slot %u is \"%s\", not \"%s\"",
         when, slot, row_path(info_row(slot) + 1), want);
}

/* Quick Menu > Controls > Port 1 Controls: Mapped Port's sublabel
 * ends in port 1's controller, @name */
static void check_remap_port(const char *name, const char *when)
{
   char key[32];
   char want[NAME_MAX_LENGTH];
   char v[NAME_MAX_LENGTH];
   size_t _len;

   open_remap_port(1);
   snprintf(key, sizeof(key), MENU_ENUM_LABEL_INPUT_REMAP_PORT_STR, 1);
   _len = snprintf(want, sizeof(want),
         msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT), 1);
   /* less its trailing dot */
   snprintf(want + _len - 1, sizeof(want) - _len + 1, ": %s", name);
   row_sublabel(row(key), v, sizeof(v));
   check(string_is_equal(v, want),
         "%s: Mapped Port's sublabel is \"%s\", not \"%s\"", when, v, want);
}

/* Two pads picking one tag: the dropdown, Add Controller's items and
 * a sharer's link carry the numbered tags too */
static void check_clash_places(void)
{
   int i;
   char used[NAME_MAX_LENGTH];
   char shared[64];
   const char *want;
   const char *link = NULL;
   file_list_t *buf;

   open_device_index_dropdown();
   want = with_tag("#1: Harness Pad [%s #2]", ID_SER22);
   check(string_is_equal(row_path(dropdown_item(0)), want),
         "Device Index's dropdown lists slot 0 as \"%s\", not \"%s\"",
         row_path(dropdown_item(0)), want);
   /* port 2 has slot 1, and the entry says so after the name */
   snprintf(shared, sizeof(shared),
         msg_hash_to_str(MSG_INPUT_DEVICE_INDEX_SHARED), 2);
   snprintf(used, sizeof(used), "%s (%s)",
         with_tag("#2: Harness Pad [%s #1]", ID_SER11), shared);
   check(string_is_equal(row_path(dropdown_item(1)), used),
         "Device Index's dropdown lists slot 1 as \"%s\", not \"%s\"",
         row_path(dropdown_item(1)), used);

   open_port_controllers(1);
   press_ok(row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR));
   want = with_tag("Harness Pad [%s #1] (Player 2)", ID_SER11);
   check(string_is_equal(row_path(add_item(1)), want),
         "Add Controller lists slot 1 as \"%s\", not \"%s\"",
         row_path(add_item(1)), want);

   /* both play as player 1, slot 0 first: slot 1 shares from
    * another port, and Port 1 Controls links it */
   input_config_plays_as_set(0, 1);
   input_autoconfigure_reallocate_ports(0);
   input_config_plays_as_set(1, 1);
   input_autoconfigure_reallocate_ports(1);
   open_port_controllers(1);
   buf = selection_buf();
   for (i = 0; buf && (size_t)i < buf->size; i++)
      if (     buf->list[i].label
            && string_ends_with(buf->list[i].label, "_input_binds_list"))
         link = row_path(i);
   want = with_tag("Harness Pad [%s #1] (Port ", ID_SER11);
   check(link && string_starts_with(link, want),
         "Port 1 Controls > Controllers links its sharer as \"%s\", not \"%s...\"",
         link ? link : "(no link)", want);
   input_config_plays_as_set(1, 0);
   input_autoconfigure_reallocate_ports(1);
   input_config_plays_as_set(0, 0);
   input_autoconfigure_reallocate_ports(0);
}

/* The find hint's line, on a line of its own after the row's own */
static bool has_find_line(const char *s)
{
   const char *line = msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_PAD_FIND);
   size_t n         = strlen(s);
   size_t l         = strlen(line);
   return n > l + 1 && *s != '\n' && s[n - l - 1] == '\n'
      && string_is_equal(s + n - l, line);
}

#ifdef HAVE_OZONE
/* Row @i's sublabel with the running menu driver named "ozone" */
static void row_sublabel_as_ozone(int i, char *s, size_t len)
{
   static menu_ctx_driver_t as_ozone;
   struct menu_state *menu_st    = menu_state_get_ptr();
   const menu_ctx_driver_t *real = menu_st->driver_ctx;

   as_ozone            = *real;
   as_ozone.ident      = "ozone";
   menu_st->driver_ctx = &as_ozone;
   row_sublabel(i, s, len);
   menu_st->driver_ctx = real;
}
#endif

/* While two connected pads show one name, the Plays As rows and Device
 * Index say on a second line that a press finds one; not with names of
 * their own, not in the Device Index dropdown or Add Controller, never
 * with Ozone. Ozone's footer hint is up on Controllers, Port 1 Controls,
 * its Controllers, the dropdown and Add Controller then, and nowhere
 * else. */
static void check_find_hint(bool shared, const char *when)
{
   char v[NAME_MAX_LENGTH];
   char want[NAME_MAX_LENGTH];
   struct menu_state *menu_st = menu_state_get_ptr();
   const char *line = msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_PAD_FIND);
   const char *di   = msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX);
   const char *verb = shared ? "lacks" : "has";
   const char *up   = shared ? "down" : "up";

   open_controllers();
   check(menu_st->pad_find_hint == shared,
         "%s: Controllers: Ozone's hint is %s", when, up);
   row_sublabel(row_of_type(MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN),
         v, sizeof(v));
   check(has_find_line(v) == shared,
         "%s: Controllers: slot 0's sublabel %s the find line: \"%s\"",
         when, verb, v);
   row_sublabel(row_of_type(MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 2),
         v, sizeof(v));
   check(has_find_line(v) == shared,
         "%s: Controllers: Other Pad's sublabel %s the find line: \"%s\"",
         when, verb, v);
#ifdef HAVE_OZONE
   row_sublabel_as_ozone(row_of_type(MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN),
         v, sizeof(v));
   check(!strstr(v, line),
         "%s: Controllers: with Ozone, slot 0's sublabel has the find line",
         when);
#endif

   open_port_controllers(1);
   check(menu_st->pad_find_hint == shared,
         "%s: Port 1 Controls > Controllers: Ozone's hint is %s", when, up);
   row_sublabel(row_of_type(MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN),
         v, sizeof(v));
   check(has_find_line(v) == shared,
         "%s: Port 1 Controls > Controllers: the Plays As row's sublabel "
         "%s the find line: \"%s\"", when, verb, v);

   open_port_controls(1);
   check(menu_st->pad_find_hint == shared,
         "%s: Port 1 Controls: Ozone's hint is %s", when, up);
   snprintf(want, sizeof(want), "%s%s%s", di, shared ? "\n" : "",
         shared ? line : "");
   row_sublabel(row(DEVICE_INDEX_1), v, sizeof(v));
   check(string_is_equal(v, want),
         "%s: Device Index's sublabel is \"%s\", not \"%s\"", when, v, want);
#ifdef HAVE_OZONE
   row_sublabel_as_ozone(row(DEVICE_INDEX_1), v, sizeof(v));
   check(string_is_equal(v, di),
         "%s: with Ozone, Device Index's sublabel is \"%s\"", when, v);
#endif

   open_device_index_dropdown();
   check(menu_st->pad_find_hint == shared,
         "%s: the Device Index dropdown: Ozone's hint is %s", when, up);
   row_sublabel(dropdown_item(0), v, sizeof(v));
   check(!strstr(v, line),
         "%s: the Device Index dropdown's entry has the find line", when);

   open_port_controllers(1);
   press_ok(row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR));
   check(menu_st->pad_find_hint == shared,
         "%s: Add Controller: Ozone's hint is %s", when, up);
   row_sublabel(0, v, sizeof(v));
   check(!strstr(v, line), "%s: Add Controller's item has the find line",
         when);

   open_input_info();
   check(!menu_st->pad_find_hint,
         "%s: Input Information: Ozone's hint is up", when);
   open_top();
   check(!menu_st->pad_find_hint,
         "%s: the main menu: Ozone's hint is up", when);
}

/* Two pads of one name and one of its own */
static void phase_connect(void)
{
   run_until(F_CONNECT);
   check_tag(0, with_tag(" [%s]", ID_PORT1), "two Harness Pads");
   check_tag(1, with_tag(" [%s]", ID_PORT2), "two Harness Pads");
   check_tag(2, "",         "a name of its own");
   check_ident(0, ID_PORT1, "port-1");
   check_ident(1, ID_PORT2, "port-2");
   check_controllers(with_tag("Harness Pad [%s]", ID_PORT1),
         with_tag("Harness Pad [%s]", ID_PORT2), "Other Pad",
         "two Harness Pads");
   check_device_index(with_tag("#1: Harness Pad [%s]", ID_PORT1),
         "two Harness Pads");
   open_port_controllers(1);
   check_row(0, with_tag("Harness Pad [%s]", ID_PORT1),
         "two Harness Pads: Port 1 Controls > Controllers");
   check_input_info(0, with_tag("Harness Pad [%s]", ID_PORT1), NULL,
         "two Harness Pads");
   check_input_info(1, with_tag("Harness Pad [%s]", ID_PORT2), NULL,
         "two Harness Pads");
   check_input_info(2, "Other Pad", NULL, "a name of its own");
   check_remap_port(with_tag("Harness Pad [%s]", ID_PORT1),
         "two Harness Pads");
   check_find_hint(true, "two Harness Pads");
}

/* The tag follows the pad, not the slot; a placeholder serial
 * counts as none, so the ports tell two clones apart */
static void phase_swap(void)
{
   run_until(F_SWAP_OUT);
   check_gone(0, "both unplugged before the swap");
   check_gone(1, "both unplugged before the swap");
   run_until(F_SWAP_IN);
   check_ident(0, ID_PORT2, "two clones");
   check_ident(1, ID_PORT1, "two clones");
   check_tag(0, with_tag(" [%s]", ID_PORT2),
         "the pads in each other's slots");
   check_tag(1, with_tag(" [%s]", ID_PORT1),
         "the pads in each other's slots");
   check_controllers(with_tag("Harness Pad [%s]", ID_PORT2),
         with_tag("Harness Pad [%s]", ID_PORT1), "Other Pad",
         "the pads in each other's slots");
}

/* Alone, no tag; its own Bluetooth address wins over the port */
static void phase_serial(void)
{
   run_until(F_SERIAL_OUT);
   check_gone(1, "one Harness Pad left");
   check_tag(0, "", "one Harness Pad left");
   check_ident(0, ID_PORT2, "one Harness Pad left");
   open_controllers();
   check_row(0, "Harness Pad", "one Harness Pad left");
   check_find_hint(false, "one Harness Pad left");
   run_until(F_SERIAL_IN);
   check_tag(1, with_tag(" [%s]", ID_BT), "a pad with an address");
   check_ident(1, ID_BT, "a pad with an address");
   check_tag(0, with_tag(" [%s]", ID_PORT2), "beside a pad with an address");
   check_controllers(with_tag("Harness Pad [%s]", ID_PORT2),
         with_tag("Harness Pad [%s]", ID_BT), "Other Pad",
         "a pad with an address");
}

/* Two serials ending alike, one tag: numbered in identity order,
 * port-11's the lower, in either slot */
static void phase_clash_order(void)
{
   run_until(F_ORDER_OUT);
   check_gone(0, "both unplugged before port-11 first");
   check_gone(1, "both unplugged before port-11 first");
   run_until(F_ORDER_IN);
   check_ident(0, ID_SER11, "port-11 first");
   check_ident(1, ID_SER22, "port-11 first");
   check_tag(0, with_tag(" [%s #1]", ID_SER11), "port-11 first");
   check_tag(1, with_tag(" [%s #2]", ID_SER22), "port-11 first");
}

static void phase_clash(void)
{
   run_until(F_CLASH_OUT);
   check_gone(0, "both unplugged before the clash");
   check_gone(1, "both unplugged before the clash");
   run_until(F_CLASH_IN);
   check_ident(0, ID_SER22, "port-22");
   check_ident(1, ID_SER11, "port-11");
   check_tag(0, with_tag(" [%s #2]", ID_SER22), "two pads picking one tag");
   check_tag(1, with_tag(" [%s #1]", ID_SER11), "two pads picking one tag");
   check_controllers(with_tag("Harness Pad [%s #2]", ID_SER22),
         with_tag("Harness Pad [%s #1]", ID_SER11), "Other Pad",
         "two pads picking one tag");
   check_device_index(with_tag("#1: Harness Pad [%s #2]", ID_SER22),
         "two pads picking one tag");
   check_clash_places();
}

/* One identity twice (a cloned serial): the port path decides; with
 * one path too, the slot */
static void phase_clash_tie(void)
{
   char phys[NAME_MAX_LENGTH];
   const char *own = input_config_get_device_phys(1);

   strlcpy(phys, own ? own : "", sizeof(phys));
   input_config_set_device_ident(1, ID_SER22, "000123");
   check_tag(0, with_tag(" [%s #2]", ID_SER22), "one identity twice");
   check_tag(1, with_tag(" [%s #1]", ID_SER22), "one identity twice");
   input_config_set_device_phys(1, input_config_get_device_phys(0));
   check_tag(0, with_tag(" [%s #1]", ID_SER22), "one identity and path");
   check_tag(1, with_tag(" [%s #2]", ID_SER22), "one identity and path");
   input_config_set_device_phys(1, phys);
   input_config_set_device_ident(1, ID_SER11, "000123");
}

/* No identity: the number */
static void phase_noid(void)
{
   run_until(F_NOID_OUT);
   check_gone(1, "unplugged before no identity");
   run_until(F_NOID_IN);
   check_tag(1, " (#2)", "a pad with no identity");
   check_ident(1, 0, "a pad with no identity");
   check_tag(0, with_tag(" [%s]", ID_SER22),
         "beside a pad with no identity");
   check_controllers(with_tag("Harness Pad [%s]", ID_SER22),
         "Harness Pad (#2)", "Other Pad", "a pad with no identity");
}

/* The name the menus show decides, not the driver's: as a profile's
 * input_device_display_name would, which the harness sets directly
 * (its pads load no profile). */
static void phase_shown(void)
{
   input_config_set_device_display_name(1, "Wireless Pad");
   check_tag(0, "", "one name, shown apart");
   check_tag(1, "", "one name, shown apart");
   run_until(F_SHOWN_IN);
   check_tag(0, "", "two names");
   check_tag(1, "", "two names");
   input_config_set_device_display_name(1, "Harness Pad");
   check(!input_config_get_device_name_index(1),
         "two names: slot 1's name_index is %u",
         input_config_get_device_name_index(1));
   check_tag(0, with_tag(" [%s]", ID_SER22), "two names shown as one");
   check_tag(1, with_tag(" [%s]", ID_PORT1), "two names shown as one");
   check_tag(2, "",         "beside two names shown as one");
   check_input_info(1, with_tag("Wireless Pad [%s]", ID_PORT1),
         with_tag("Harness Pad [%s]", ID_PORT1), "two names shown as one");
}

/* One menu frame at @t on the menu's clock: the marks follow it, not
 * the real time a loaded machine takes between frames */
static void frame_at(retro_time_t t)
{
   menu_driver_iterate(menu_state_get_ptr(), disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), MENU_ACTION_NOOP, t);
}

/* @slot's press, released, keeps its rows marked until a second after
 * the frame that saw it go down; RGUI is asked to draw only when the
 * mark goes. Leaves @slot unpressed for the next lane. */
static void check_hold(unsigned slot, const char *when)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   gfx_display_t *p_disp      = disp_get_ptr();
   retro_time_t t0            = menu_st->pad_pressed_us[slot];

   check(t0 != 0, "%s: no press seen on slot %u", when, slot);
   p_disp->flags &= ~GFX_DISP_FLAG_FB_DIRTY;
   frame_at(t0 + MENU_PAD_PRESSED_HOLD_US - 1);
   check(MENU_PAD_MARKED(menu_st, slot),
         "%s: slot %u's mark ended before a second", when, slot);
   check(!(p_disp->flags & GFX_DISP_FLAG_FB_DIRTY),
         "%s: RGUI asked to draw with no mark changed", when);
   frame_at(t0 + MENU_PAD_PRESSED_HOLD_US);
   check(!MENU_PAD_MARKED(menu_st, slot),
         "%s: slot %u still marked a second on", when, slot);
   check(p_disp->flags & GFX_DISP_FLAG_FB_DIRTY,
         "%s: RGUI not asked to draw when the mark went", when);
}

static void forget_press(unsigned slot)
{
   menu_state_get_ptr()->pad_pressed_us[slot] = 0;
}

/* Controllers: the pressed pad's row reads "Pressed", the other's not;
 * the cursor stays */
static void lane_press_row(void)
{
   char before0[NAME_MAX_LENGTH];
   char before1[NAME_MAX_LENGTH];
   char v[NAME_MAX_LENGTH];
   size_t cursor;
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned row0              = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN;
   unsigned row1              = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 1;

   open_controllers();
   row_value(row_of_type(row0), before0, sizeof(before0));
   row_value(row_of_type(row1), before1, sizeof(before1));
   check(!string_is_equal(before1, PRESSED),
         "fixture: slot 1 reads Pressed before its press");
   cursor = menu_st->selection_ptr;
   run_until(F_PRESS_ROW + 2);
   frame_at(menu_st->pad_pressed_us[1] + MENU_PAD_PRESSED_HOLD_US / 2);
   row_value(row_of_type(row1), v, sizeof(v));
   check(string_is_equal(v, PRESSED),
         "Controllers: the pressed pad's row reads \"%s\"", v);
   row_value(row_of_type(row0), v, sizeof(v));
   check(string_is_equal(v, before0),
         "Controllers: another pad's row reads \"%s\"", v);
   check(menu_st->selection_ptr == cursor,
         "Controllers: a press moved the cursor");
   check_hold(1, "Controllers");
   row_value(row_of_type(row1), v, sizeof(v));
   check(string_is_equal(v, before1),
         "Controllers: after the hold the row reads \"%s\"", v);
   forget_press(1);
}

/* Port 1 Controls' Device Index and its Controllers' Plays As row read
 * "Pressed" for the pad they name (slot 0) and not for another; so does Port 2's
 * Device Index, set to slot 0 so its slot is not its port */
static void lane_press_device_index(void)
{
   char before[NAME_MAX_LENGTH];
   char before2[NAME_MAX_LENGTH];
   char v[NAME_MAX_LENGTH];
   struct menu_state *menu_st = menu_state_get_ptr();
   settings_t *settings       = config_get_ptr();
   unsigned port2_slot        = settings->uints.input_joypad_index[1];

   settings->uints.input_joypad_index[1] = 0;
   open_port_controls(2);
   row_value(row(DEVICE_INDEX_2), before2, sizeof(before2));
   open_port_controls(1);
   row_value(row(DEVICE_INDEX_1), before, sizeof(before));
   run_until(F_PRESS_OTHER + 2);
   frame_at(menu_st->pad_pressed_us[1] + MENU_PAD_PRESSED_HOLD_US / 2);
   row_value(row(DEVICE_INDEX_1), v, sizeof(v));
   check(string_is_equal(v, before),
         "Device Index reads \"%s\" for another port's pad", v);
   open_port_controls(2);
   frame_at(menu_st->pad_pressed_us[1] + MENU_PAD_PRESSED_HOLD_US / 2);
   row_value(row(DEVICE_INDEX_2), v, sizeof(v));
   check(string_is_equal(v, before2),
         "Port 2's Device Index reads \"%s\" for slot 1's press", v);
   forget_press(1);
   run_until(F_PRESS_DI + 2);
   frame_at(menu_st->pad_pressed_us[0] + MENU_PAD_PRESSED_HOLD_US / 2);
   row_value(row(DEVICE_INDEX_2), v, sizeof(v));
   check(string_is_equal(v, PRESSED),
         "Port 2's Device Index reads \"%s\" for slot 0's press", v);
   open_port_controls(1);
   frame_at(menu_st->pad_pressed_us[0] + MENU_PAD_PRESSED_HOLD_US / 2);
   row_value(row(DEVICE_INDEX_1), v, sizeof(v));
   check(string_is_equal(v, PRESSED),
         "Device Index reads \"%s\" for its own pad's press", v);
   open_port_controllers(1);
   frame_at(menu_st->pad_pressed_us[0] + MENU_PAD_PRESSED_HOLD_US / 2);
   row_value(row_of_type(MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN), v, sizeof(v));
   check(string_is_equal(v, PRESSED),
         "Port 1 Controls > Controllers: the pressed pad's Plays As row "
         "reads \"%s\"", v);
   open_port_controls(1);
   frame_at(menu_st->pad_pressed_us[0] + MENU_PAD_PRESSED_HOLD_US / 2);
   check_hold(0, "Device Index");
   row_value(row(DEVICE_INDEX_1), v, sizeof(v));
   check(string_is_equal(v, before),
         "Device Index after the hold reads \"%s\"", v);
   forget_press(0);
   settings->uints.input_joypad_index[1] = port2_slot;
}

/* Device Index's dropdown: the pressed pad's entry */
static void lane_press_dropdown(void)
{
   char v[NAME_MAX_LENGTH];
   struct menu_state *menu_st = menu_state_get_ptr();

   open_device_index_dropdown();
   run_until(F_PRESS_DD + 2);
   frame_at(menu_st->pad_pressed_us[1] + MENU_PAD_PRESSED_HOLD_US / 2);
   row_value(dropdown_item(1), v, sizeof(v));
   check(string_is_equal(v, PRESSED),
         "the dropdown's entry for the pressed pad reads \"%s\"", v);
   row_value(dropdown_item(0), v, sizeof(v));
   check(!*v, "the dropdown's entry for another pad reads \"%s\"", v);
   check_hold(1, "Device Index dropdown");
   row_value(dropdown_item(1), v, sizeof(v));
   check(!*v, "the dropdown's entry after the hold reads \"%s\"", v);
   forget_press(1);
}

/* With no controller list shown, nothing is read or marked */
static void lane_press_elsewhere(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();

   open_top();
   run_until(F_PRESS_TOP + 2);
   check(!menu_st->pad_pressed_us[0] && !menu_st->pads_marked,
         "a press was marked with no controller list shown");
}

/* A button left down marks its pad for a second, not for as long as it
 * stays down; let up and pressed again, it marks again */
static void lane_press_held(void)
{
   char before[NAME_MAX_LENGTH];
   char v[NAME_MAX_LENGTH];
   input_bits_t b;
   retro_time_t t0;
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned row1              = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 1;

   open_controllers();
   row_value(row_of_type(row1), before, sizeof(before));
   run_until(F_HELD);
   t0 = menu_st->pad_pressed_us[1];
   check(t0 != 0, "held: no press seen on slot 1");
   frame_at(t0 + MENU_PAD_PRESSED_HOLD_US - 1);
   check(MENU_PAD_MARKED(menu_st, 1),
         "held: slot 1's mark ended before a second");
   frame_at(t0 + MENU_PAD_PRESSED_HOLD_US);
   frame_at(t0 + 3 * MENU_PAD_PRESSED_HOLD_US);
   BIT256_CLEAR_ALL(b);
   input_driver_capture_pad(1, false, &b, 16, NULL, 0, NULL, 0);
   check(BIT256_GET(b, 10), "fixture: slot 1's button was let up");
   check(!MENU_PAD_MARKED(menu_st, 1),
         "held: slot 1 still marked seconds after its press");
   row_value(row_of_type(row1), v, sizeof(v));
   check(string_is_equal(v, before),
         "held: the row reads \"%s\" with the button still down", v);
   run_until(F_HELD_AGAIN + 2);
   check(menu_st->pad_pressed_us[1] > t0,
         "held: let up and pressed again, the press was not seen");
   frame_at(menu_st->pad_pressed_us[1] + MENU_PAD_PRESSED_HOLD_US / 2);
   row_value(row_of_type(row1), v, sizeof(v));
   check(string_is_equal(v, PRESSED),
         "held: pressed again, the row reads \"%s\"", v);
   check_hold(1, "pressed again");
   forget_press(1);
}

/* Add Controller: the pressed pad's item reads "Pressed", another's
 * nothing; the cursor stays. With Other Pad shown as a Harness Pad
 * too, Port 1's list holds two of one name, slot 1 on row 0: an item
 * is not its row. */
static void lane_press_add(void)
{
   int i1, i2;
   size_t cursor;
   char other[NAME_MAX_LENGTH];
   char v[NAME_MAX_LENGTH];
   struct menu_state *menu_st = menu_state_get_ptr();
   const char *shown          = input_config_get_device_display_name(2);

   strlcpy(other, shown ? shown : "", sizeof(other));
   input_config_set_device_display_name(2, "Harness Pad");
   open_port_controllers(1);
   press_ok(row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR));
   i1 = add_item(1);
   i2 = add_item(2);
   check(i1 == 0 && i2 == 1,
         "fixture: Port 1's Add Controller lists slots 1 and 2 on rows "
         "%d and %d", i1, i2);
   check(menu_st->pad_find_hint,
         "Add Controller: Ozone's hint is down with three of one name");
   cursor = menu_st->selection_ptr;
   run_until(F_PRESS_ADD + 2);
   frame_at(menu_st->pad_pressed_us[1] + MENU_PAD_PRESSED_HOLD_US / 2);
   row_value(i1, v, sizeof(v));
   check(string_is_equal(v, PRESSED),
         "Add Controller: the pressed pad's item reads \"%s\"", v);
   row_value(i2, v, sizeof(v));
   check(!*v, "Add Controller: another pad's item reads \"%s\"", v);
   check(menu_st->selection_ptr == cursor,
         "Add Controller: a press moved the cursor");
   check_hold(1, "Add Controller");
   row_value(i1, v, sizeof(v));
   check(!*v, "Add Controller: after the hold the item reads \"%s\"", v);
   forget_press(1);
   if (*other)
      input_config_set_device_display_name(2, other);
   else
      input_config_clear_device_display_name(2);
}

/* Names of their own: a press marks nothing, so no value is hidden,
 * and a mark from before the names parted does not come back with
 * them. Slot 1 shows its driver's "Wireless Pad" with its display
 * name cleared. */
static void lane_press_apart(void)
{
   unsigned s;
   retro_time_t t0;
   char v[NAME_MAX_LENGTH];
   struct menu_state *menu_st = menu_state_get_ptr();

   open_controllers();
   run_until(F_PRESS_STALE + 2);
   t0 = menu_st->pad_pressed_us[1];
   check(t0 != 0, "fixture: no press seen on slot 1 with its name shared");
   input_config_clear_device_display_name(1);
   frame_at(t0 + MENU_PAD_PRESSED_HOLD_US / 4);
   check(!menu_st->pads_marked && !menu_st->pad_find_hint,
         "names apart: a mark or Ozone's hint stays up");
   input_config_set_device_display_name(1, "Harness Pad");
   frame_at(t0 + MENU_PAD_PRESSED_HOLD_US / 2);
   check(!MENU_PAD_MARKED(menu_st, 1),
         "names shared again: slot 1's mark from before is back");
   input_config_clear_device_display_name(1);
   run_until(F_PRESS_APART + 2);
   frame_at(cpu_features_get_time_usec());
   check(!menu_st->pad_pressed_us[1] && !menu_st->pads_marked,
         "names apart: a press was marked");
   for (s = 0; s < 3; s++)
   {
      row_value(row_of_type(MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + s),
            v, sizeof(v));
      check(!string_is_equal(v, PRESSED),
            "names apart: slot %u's row reads \"%s\"", s, v);
   }
   input_config_set_device_display_name(1, "Harness Pad");
}

int main(int argc, char *argv[])
{
   FILE *f;
   char dir[512];
   static char cfg_path[640];
   char ratst_path[640];
   char *rarch_argv[8];
   int rarch_argc  = 0;
   const char *tmp = getenv("TMPDIR");

   (void)argc;
   (void)argv;
   if (!tmp || !*tmp)
      tmp = "/tmp";
   snprintf(dir, sizeof(dir), "%s/pad_tags_%lu", tmp,
         (unsigned long)cpu_features_get_time_usec());
   if (!path_mkdir(dir))
      return 1;
   snprintf(cfg_path,   sizeof(cfg_path),   "%s/harness.cfg", dir);
   snprintf(ratst_path, sizeof(ratst_path), "%s/pads.ratst",  dir);

   if (!(f = fopen(ratst_path, "wb")))
      return 1;
   write_script(f);
   fclose(f);
   if (!(f = fopen(cfg_path, "wb")))
      return 1;
   fprintf(f, "video_driver = \"null\"\n");
   fprintf(f, "video_threaded = \"false\"\n");
   fprintf(f, "video_vsync = \"false\"\n");
   fprintf(f, "audio_driver = \"null\"\n");
   fprintf(f, "audio_enable = \"false\"\n");
   fprintf(f, "input_driver = \"null\"\n");
   fprintf(f, "input_joypad_driver = \"test\"\n");
   fprintf(f, "test_input_file_joypad = \"%s\"\n", ratst_path);
   /* no profile: the pads bind nothing, so a press does nothing in
    * the menu but what is checked here */
   fprintf(f, "input_autodetect_enable = \"false\"\n");
   fprintf(f, "menu_driver = \"rgui\"\n");
   fprintf(f, "rgui_config_directory = \"%s\"\n", dir);
   fprintf(f, "pause_nonactive = \"false\"\n");
   fprintf(f, "gamemode_enable = \"false\"\n");
   fprintf(f, "suspend_screensaver_enable = \"false\"\n");
   fprintf(f, "config_save_on_exit = \"false\"\n");
   fclose(f);

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   /* Same prelude as rarch_main(); see playlist_nav. */
   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   runloop_msg_queue_init();
   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   retroarch_menu_running();

   phase_connect();
   phase_swap();
   phase_serial();
   phase_clash_order();
   phase_clash();
   phase_clash_tie();
   phase_noid();
   phase_shown();
   lane_press_row();
   lane_press_device_index();
   lane_press_dropdown();
   lane_press_elsewhere();
   lane_press_held();
   lane_press_add();
   lane_press_apart();

   filestream_delete(ratst_path);
   filestream_delete(cfg_path);
   filestream_delete(dir);

   if (failures)
   {
      fprintf(stderr, "pad_tags_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("pad_tags_test: controllers of one name are told apart\n");
   return 0;
}
