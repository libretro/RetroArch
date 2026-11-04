/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../menu/drivers/ios_menu.c"

#define MSG_HASH_LBL_ROW(Id, str) str,
const struct msg_hash_us_lbl_blob_s msg_hash_us_lbl_blob = {
#include "../../../intl/msg_hash_lbl_rows.h"
};
#undef MSG_HASH_LBL_ROW

static struct menu_state test_menu;
static settings_t test_settings;
static menu_ctx_driver_t legacy;
const menu_ctx_driver_t *menu_ctx_drivers[] = { &legacy, &menu_ctx_ios, NULL };
static struct item_file test_items[205];
static file_list_t test_list;
static file_list_t test_stack;
static struct item_file test_stack_items[3];
static file_list_t *test_buffers[] = { &test_list };
static file_list_t *test_stacks[] = { &test_stack };
static menu_list_t test_lists;
static menu_file_list_cbs_t test_cbs;
static rarch_setting_t pointer_setting;
static setting_actions_t pointer_actions;
static bool init_ok = true;
static bool mount_ok = true;
static bool loading;
static bool saving;
static bool restoring;
static bool destroy_on_action;
static bool core_running;
static unsigned commits;
static unsigned additions;
static unsigned slice_items;
static unsigned saved;
static unsigned reinits;
static unsigned dispatched;
static unsigned settings_closed;
static uint64_t published_session;
static uint64_t published_revision;
static size_t published_offset;
static size_t published_rows;
static unsigned published_tab;
static unsigned published_tabs;

struct menu_state *menu_state_get_ptr(void) { return &test_menu; }
settings_t *config_get_ptr(void) { return &test_settings; }
const char *path_get(enum rarch_path_type type)
{ assert(type == RARCH_PATH_CONTENT); return "game.bin"; }
bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{ (void)data; assert(state == RARCH_CTL_IS_DUMMY_CORE); return !core_running; }
bool task_content_load_pending(void) { return loading; }
bool content_save_state_in_progress(void *data) { (void)data; return saving; }
bool content_load_state_in_progress(void *data) { (void)data; return restoring; }

void task_nbio_slice_open(nbio_budget_t *budget)
{ (void)budget; slice_items = 0; }
void task_nbio_slice_close(nbio_budget_t *budget) { (void)budget; }
bool task_nbio_slice_within_budget(void *data, size_t avail, size_t len)
{ (void)data; (void)avail; (void)len; return ++slice_items < 5; }

bool command_event(enum event_command command, void *data)
{
   (void)data;
   if (command == CMD_EVENT_MENU_SAVE_CURRENT_CONFIG)
      saved++;
   if (command == CMD_EVENT_REINIT)
      reinits++;
   return true;
}

size_t menu_entries_get_title(char *s, size_t len)
{ (void)len; strcpy(s, "Library"); return strlen(s); }

void menu_entry_get(menu_entry_t *entry, size_t stack, size_t index,
      void *data, bool representation)
{
   (void)stack;
   (void)data;
   (void)representation;
   assert(index < test_list.size);
   entry->type = test_items[index].type;
   entry->setting_type = index == 1 ? ST_INT : ST_ACTION;
   strcpy(entry->rich_label, "Game");
}

rarch_setting_t *menu_setting_find_enum(enum msg_hash_enums label)
{
   assert(label == MENU_ENUM_LABEL_POINTER_ENABLE);
   return &pointer_setting;
}

static int test_pointer_right(rarch_setting_t *setting, size_t index, bool wrap)
{
   (void)setting;
   (void)index;
   (void)wrap;
   test_settings.bools.menu_pointer_enable = !test_settings.bools.menu_pointer_enable;
   return 0;
}

int generic_menu_entry_action(void *userdata, menu_entry_t *entry,
      size_t index, enum menu_action action)
{
   (void)entry;
   (void)index;
   (void)action;
   dispatched++;
   if (destroy_on_action)
   {
      menu_ctx_ios.free(userdata);
      free(userdata);
      free(test_menu.driver_data);
      test_menu.driver_data = NULL;
      test_menu.userdata = NULL;
   }
   return 0;
}

bool ios_menu_bridge_init(uint64_t session)
{ (void)session; return init_ok; }
void ios_menu_bridge_free(void) { }
bool ios_menu_bridge_visible(bool visible) { (void)visible; return mount_ok; }
void ios_menu_bridge_begin(uint64_t session, uint64_t revision,
      const char *title, size_t total, size_t offset, bool back,
      const char *driver, bool pointer, unsigned tab, unsigned tabs)
{
   (void)title; (void)back; (void)driver; (void)pointer;
   assert(total == test_list.size);
   published_session = session;
   published_revision = revision;
   published_offset = offset;
   published_rows = 0;
   published_tab = tab;
   published_tabs = tabs;
}
void ios_menu_bridge_add(size_t index, unsigned type, const char *title,
      const char *value, const char *sublabel, bool enabled, bool boolean,
      bool checked, bool adjustable)
{
   (void)title; (void)value; (void)sublabel; (void)enabled;
   (void)boolean; (void)checked;
   assert(index < test_list.size && type == test_items[index].type);
   assert(adjustable == (index == 1));
   additions++;
   published_rows++;
}
void ios_menu_bridge_commit(void) { commits++; }
void ios_menu_bridge_status(size_t selection, bool busy, const char *dialog)
{ (void)selection; (void)busy; (void)dialog; }
void ios_menu_bridge_message(const char *message) { (void)message; }
void ios_menu_bridge_close_settings(void) { settings_closed++; }

static void test_refresh(void)
{
   unsigned frame;
   for (frame = 0; frame < 25; frame++)
   {
      unsigned before = additions;
      ios_menu_update();
      assert(additions - before <= 5);
   }
   assert(!ios_menu_current()->dirty);
}

static bool test_request(size_t index, unsigned type, unsigned action)
{
   return ios_menu_request(published_session, published_revision,
         index, type, action);
}

static void test_start(void)
{
   free(test_stack_items[0].label);
   test_stack_items[0].label = NULL;
   test_stack.size = 0;
   test_menu.driver_ctx = &menu_ctx_ios;
   test_menu.driver_data = (menu_handle_t*)menu_ctx_ios.init(&test_menu.userdata, false);
   assert(test_menu.driver_data);
   assert(menu_ctx_ios.lists_init(test_menu.driver_data));
   menu_ctx_ios.toggle(test_menu.userdata, true);
   test_refresh();
}

static int test_ok(const char *path, const char *label, unsigned type,
      size_t index, size_t entry)
{ (void)path; (void)label; (void)type; (void)index; (void)entry; return 0; }

static int test_direction(unsigned type, const char *label, bool wrap)
{ (void)type; (void)label; (void)wrap; return 0; }

bool menu_entries_append(file_list_t *list, const char *path, const char *label,
      enum msg_hash_enums enum_idx, unsigned type, size_t directory,
      size_t entry, rarch_setting_t *setting)
{
   (void)path; (void)enum_idx; (void)directory; (void)entry; (void)setting;
   assert(list == &test_stack && !list->size);
   list->list[0].label = (char*)malloc(strlen(label) + 1);
   assert(list->list[0].label);
   strcpy(list->list[0].label, label);
   list->list[0].type = type;
   list->size = 1;
   return true;
}

void file_list_free_label(file_list_t *list, size_t index)
{ free(list->list[index].label); list->list[index].label = NULL; }

void menu_entries_flush_stack(const char *needle, unsigned type)
{ (void)needle; (void)type; test_stack.size = 1; }

void filebrowser_clear_type(void) { }

int menu_driver_deferred_push_content_list(file_list_t *list)
{
   assert(list == &test_list);
   test_menu.flags &= ~MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   menu_ctx_ios.populate_entries(test_menu.userdata, "", test_stack_items[0].label,
         test_stack_items[0].type);
   return 0;
}

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data)
{
   (void)data;
   assert(state == RARCH_MENU_CTL_SET_PENDING_QUICK_MENU);
   test_menu.flags |= MENU_ST_FLAG_PENDING_QUICK_MENU;
   return true;
}

int main(void)
{
   size_t i;
   uint64_t old_session;
   uint64_t old_revision;
   menu_entry_t *entry;
   legacy.ident = "glui";
   strcpy(test_settings.arrays.menu_driver, "ios");
   test_list.list = test_items;
   test_list.size = 205;
   test_stack.list = test_stack_items;
   test_lists.selection_buf = test_buffers;
   test_lists.menu_stack = test_stacks;
   test_menu.entries.list = &test_lists;
   test_settings.bools.menu_content_show_history = true;
   test_settings.bools.menu_content_show_favorites = true;
   test_settings.bools.menu_content_show_settings = true;
   test_cbs.action_ok = test_ok;
   test_cbs.action_left = test_direction;
   test_cbs.action_right = test_direction;
   pointer_setting.actions = &pointer_actions;
   pointer_actions.right = test_pointer_right;
   for (i = 0; i < test_list.size; i++)
   {
      test_items[i].type = 100 + (unsigned)i;
      test_items[i].actiondata = &test_cbs;
   }

   init_ok = false;
   assert(!menu_ctx_ios.init(&test_menu.userdata, false));
   assert(!test_menu.userdata);
   init_ok = true;
   test_start();
   assert(commits == 1 && published_rows == 100);
   assert(published_tab == IOS_MENU_TAB_LIBRARY && published_tabs == 15);
   ios_menu_update();
   assert(commits == 1);

   /* A rebuild between enqueue and dispatch must not retarget a tap. */
   assert(test_request(3, 103, IOS_MENU_OK));
   assert(!test_request(3, 103, IOS_MENU_OK));
   old_revision = published_revision;
   menu_ctx_ios.list_clear(&test_list);
   assert(ios_menu_take_action(MENU_ACTION_NOOP) == MENU_ACTION_NOOP);
   test_refresh();
   assert(!ios_menu_request(published_session, old_revision, 3, 103, IOS_MENU_OK));
   assert(test_request(3, 999, IOS_MENU_OK));
   assert(ios_menu_take_action(MENU_ACTION_NOOP) == MENU_ACTION_NOOP);
   assert(test_request(205, 0, IOS_MENU_OK));
   assert(ios_menu_take_action(MENU_ACTION_NOOP) == MENU_ACTION_NOOP);
   assert(test_request(3, 103, IOS_MENU_OK));
   assert(ios_menu_take_action(MENU_ACTION_DOWN) == MENU_ACTION_DOWN);

   loading = true;
   assert(!test_request(3, 103, IOS_MENU_OK));
   loading = false;
   saving = true;
   assert(!test_request(3, 103, IOS_MENU_OK));
   saving = false;
   restoring = true;
   assert(!test_request(3, 103, IOS_MENU_OK));
   restoring = false;

   ios_menu_settings_visible(published_session, true);
   assert(ios_menu_take_action(MENU_ACTION_DOWN) == MENU_ACTION_NOOP);
   assert(ios_menu_take_action(MENU_ACTION_CANCEL) == MENU_ACTION_NOOP);
   assert(settings_closed == 1);
   ios_menu_settings_visible(published_session, false);
   assert(ios_menu_take_action(MENU_ACTION_DOWN) == MENU_ACTION_DOWN);

   for (i = IOS_MENU_TAB_HISTORY; i <= IOS_MENU_TAB_SETTINGS; i++)
   {
      assert(test_request(i, 0, IOS_MENU_TAB));
      ios_menu_take_action(MENU_ACTION_NOOP);
      test_refresh();
      assert(published_tab == i && test_stack.size == 1);
   }
   assert(test_request(IOS_MENU_TAB_QUICK, 0, IOS_MENU_TAB));
   ios_menu_take_action(MENU_ACTION_NOOP);
   assert(!(test_menu.flags & MENU_ST_FLAG_PENDING_QUICK_MENU));
   test_refresh();
   core_running = true;
   assert(test_request(IOS_MENU_TAB_QUICK, 0, IOS_MENU_TAB));
   ios_menu_take_action(MENU_ACTION_NOOP);
   assert(test_menu.flags & MENU_ST_FLAG_PENDING_QUICK_MENU);
   test_menu.flags &= ~MENU_ST_FLAG_PENDING_QUICK_MENU;
   test_stack_items[1].label = (char*)MENU_ENUM_LABEL_CONTENT_SETTINGS_STR;
   test_stack.size = 2;
   menu_ctx_ios.list_insert(test_menu.userdata, &test_list, "", "", 0, 0, 0);
   test_refresh();
   assert(published_tab == IOS_MENU_TAB_QUICK && published_tabs == 31);
   test_stack_items[2].label = (char*)"state_submenu";
   test_stack.size = 3;
   assert(ios_menu_stack_tab(&test_stack, test_menu.driver_data) == IOS_MENU_TAB_QUICK);
   test_stack_items[1].label = (char*)MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS_STR;
   strcpy(test_menu.driver_data->deferred_path, "other.bin");
   assert(ios_menu_stack_tab(&test_stack, test_menu.driver_data) == IOS_MENU_TAB_SETTINGS);
   strcpy(test_menu.driver_data->deferred_path, "game.bin");
   assert(ios_menu_stack_tab(&test_stack, test_menu.driver_data) == IOS_MENU_TAB_QUICK);
   core_running = false;
   assert(test_request(0, 0, IOS_MENU_HOME));
   ios_menu_take_action(MENU_ACTION_NOOP);
   test_refresh();
   assert(published_tab == IOS_MENU_TAB_LIBRARY);
   assert(!strcmp(test_stack_items[0].label, MENU_ENUM_LABEL_MAIN_MENU_STR));

   assert(test_request(0, 0, IOS_MENU_NEXT_PAGE));
   ios_menu_take_action(MENU_ACTION_NOOP);
   test_refresh();
   assert(published_offset == 100 && published_rows == 100);
   assert(test_request(0, 0, IOS_MENU_NEXT_PAGE));
   ios_menu_take_action(MENU_ACTION_NOOP);
   test_refresh();
   assert(published_offset == 200 && published_rows == 5);
   test_menu.selection_ptr = 4;
   menu_ctx_ios.navigation_set(test_menu.userdata, true);
   test_refresh();
   assert(published_offset == 0);

   assert(test_request(0, 0, IOS_MENU_POINTER));
   ios_menu_take_action(MENU_ACTION_NOOP);
   test_refresh();
   assert(test_settings.bools.menu_pointer_enable);
   assert(test_request(0, 0, IOS_MENU_SET_DRIVER));
   ios_menu_take_action(MENU_ACTION_NOOP);
   test_refresh();
   assert(!strcmp(test_settings.arrays.menu_driver, "glui"));
   assert(test_request(0, 0, IOS_MENU_SAVE_CONFIG));
   ios_menu_take_action(MENU_ACTION_NOOP);
   test_refresh();
   assert(saved == 1);

   assert(test_request(3, 103, IOS_MENU_OK));
   menu_ctx_ios.toggle(test_menu.userdata, false);
   assert(!ios_menu_visible());
   assert(ios_menu_take_action(MENU_ACTION_NOOP) == MENU_ACTION_NOOP);
   assert(!test_request(3, 103, IOS_MENU_OK));
   menu_ctx_ios.toggle(test_menu.userdata, true);
   test_refresh();

   /* Reinit during a generic action must leave no stale userdata access. */
   old_session = published_session;
   destroy_on_action = true;
   entry = &ios_menu_current()->entry;
   menu_ctx_ios.entry_action(test_menu.userdata, entry, 0, MENU_ACTION_OK);
   assert(dispatched == 1 && !test_menu.userdata);
   destroy_on_action = false;
   test_start();
   assert(!ios_menu_request(old_session, published_revision, 0, 100, IOS_MENU_OK));
   mount_ok = false;
   ios_menu_update();
   assert(reinits == 1 && saved == 2);
   assert(!strcmp(test_settings.arrays.menu_driver, "glui"));
   menu_ctx_ios.free(test_menu.userdata);
   free(test_menu.userdata);
   free(test_menu.driver_data);
   free(test_stack_items[0].label);
   puts("PASS: iOS menu snapshots, stale actions, paging, busy state, settings and lifecycle");
   return 0;
}
