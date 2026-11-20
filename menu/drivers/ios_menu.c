/* RetroArch - A frontend for libretro.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../menu_driver.h"
#include "../menu_setting.h"
#include "../../ui/ios/RetroArchMenuBridge.h"
#include "../../configuration.h"
#include "../../content.h"
#include "../../retroarch.h"
#include "../../msg_hash_lbl_str.h"
#include "../../tasks/task_content.h"
#include "../../tasks/tasks_internal.h"

#define IOS_MENU_PAGE_SIZE 100

typedef struct ios_menu
{
   uint64_t session;
   uint64_t revision;
   uint64_t request_revision;
   char *request_value;
   menu_entry_t entry;
   size_t offset;
   size_t cursor;
   size_t request_index;
   unsigned request_type;
   unsigned request_action;
   unsigned tab;
   bool pending;
   bool dirty;
   bool building;
   bool visible;
} ios_menu_t;

static uint64_t ios_menu_session;

static ios_menu_t *ios_menu_current(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   return menu_st->driver_ctx == &menu_ctx_ios
      ? (ios_menu_t*)menu_st->userdata : NULL;
}

static void ios_menu_invalidate(ios_menu_t *ios)
{
   if (!ios)
      return;
   ios->revision++;
   ios->dirty    = true;
   ios->building = false;
}

static bool ios_menu_busy(void)
{
   return task_content_load_pending()
      || content_save_state_in_progress(NULL)
      || content_load_state_in_progress(NULL);
}

static unsigned ios_menu_value_kind(rarch_setting_t *setting)
{
   if (!setting)
      return IOS_MENU_VALUE_NONE;
   switch (setting->type)
   {
      case ST_INT:
         return IOS_MENU_VALUE_INT;
      case ST_UINT:
         if (setting->ui_type == ST_UI_TYPE_UINT_COLOR_BUTTON)
            return IOS_MENU_VALUE_COLOR;
         if (setting->ui_type == ST_UI_TYPE_UINT_COMBOBOX
               || setting->ui_type == ST_UI_TYPE_UINT_RADIO_BUTTONS)
            break;
         return IOS_MENU_VALUE_UINT;
      case ST_SIZE:
         return IOS_MENU_VALUE_SIZE;
      case ST_FLOAT:
         return IOS_MENU_VALUE_FLOAT;
      case ST_STRING:
         if (!setting->value.target.string)
            break;
         return setting->ui_type == ST_UI_TYPE_PASSWORD_LINE_EDIT
            ? IOS_MENU_VALUE_PASSWORD : IOS_MENU_VALUE_TEXT;
      default:
         break;
   }
   return IOS_MENU_VALUE_NONE;
}

static bool ios_menu_has_submenu(menu_entry_t *entry, menu_file_list_cbs_t *cbs)
{
   rarch_setting_t *setting = cbs ? cbs->setting : NULL;
   if (!cbs || !cbs->action_ok || entry->type == MENU_SETTINGS_CORE_INFO_NONE
         || entry->type == MENU_INFO_MESSAGE || entry->type == MENU_SETTING_NO_ITEM
         || (entry->flags & MENU_ENTRY_FLAG_CHECKED)
         || ios_menu_value_kind(setting) != IOS_MENU_VALUE_NONE)
      return false;
   if (!strcmp(entry->value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MORE)))
      return true;
   if (setting)
   {
      if ((setting->flags & SD_FLAG_BROWSER_ACTION)
            || setting->type == ST_PATH || setting->type == ST_DIR
            || setting->type == ST_GROUP || setting->type == ST_SUB_GROUP
            || setting->type == ST_STRING_OPTIONS
            || setting->ui_type == ST_UI_TYPE_UINT_COMBOBOX
            || setting->ui_type == ST_UI_TYPE_UINT_RADIO_BUTTONS)
         return true;
      if (setting->type == ST_ACTION && setting->cmd_trigger_idx == CMD_EVENT_NONE)
         return true;
   }
   switch (entry->type)
   {
      case MENU_SETTINGS:
      case MENU_SETTING_GROUP:
      case MENU_SETTING_SUBGROUP:
      case MENU_SETTING_ACTION_CORE_OPTIONS:
      case MENU_SETTING_ACTION_CORE_OPTION_OVERRIDE_LIST:
      case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
      case MENU_SETTING_ACTION_REMAP_FILE_MANAGER_LIST:
      case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
      case MENU_SETTING_ACTION_CORE_MANAGER_OPTIONS:
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
      case MENU_SETTING_ACTION_CORE_SHADER_OPTIONS:
      case MENU_HISTORY_TAB:
      case MENU_FAVORITES_TAB:
      case MENU_PLAYLISTS_TAB:
      case FILE_TYPE_DIRECTORY:
      case FILE_TYPE_PARENT_DIRECTORY:
      case FILE_TYPE_COMPRESSED:
      case FILE_TYPE_PLAYLIST_COLLECTION:
      case FILE_TYPE_RPL_ENTRY:
      case FILE_TYPE_CONTENTLIST_ENTRY:
         return true;
      default:
         break;
   }
   return false;
}

static unsigned ios_menu_tabs(void)
{
   settings_t *settings = config_get_ptr();
   unsigned tabs       = (1 << IOS_MENU_TAB_MAIN) | (1 << IOS_MENU_TAB_LIBRARY);
   if (settings->bools.menu_content_show_settings && !settings->bools.kiosk_mode_enable)
      tabs |= 1 << IOS_MENU_TAB_SETTINGS;
   return tabs;
}

static unsigned ios_menu_stack_tab(file_list_t *stack)
{
   size_t i;
   for (i = 0; stack && i < stack->size; i++)
   {
      const char *label = stack->list[i].label;
      if (!label)
         continue;
      if (!strcmp(label, MENU_ENUM_LABEL_SETTINGS_TAB_STR))
         return IOS_MENU_TAB_SETTINGS;
      if (!strcmp(label, MENU_ENUM_LABEL_PLAYLISTS_TAB_STR))
         return IOS_MENU_TAB_LIBRARY;
      if (!strcmp(label, MENU_ENUM_LABEL_MAIN_MENU_STR))
         return IOS_MENU_TAB_MAIN;
   }
   return IOS_MENU_TAB_MAIN;
}

static bool ios_menu_switch_tab(ios_menu_t *ios, unsigned tab)
{
   static const char *const labels[] = {
      MENU_ENUM_LABEL_MAIN_MENU_STR,
      MENU_ENUM_LABEL_PLAYLISTS_TAB_STR,
      MENU_ENUM_LABEL_SETTINGS_TAB_STR
   };
   static const unsigned types[] = {
      MENU_SETTINGS, MENU_PLAYLISTS_TAB, MENU_SETTINGS
   };
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *stack;
   file_list_t *list = MENU_ENTRIES_GET_SELECTION_BUF_PTR_INTERNAL(menu_st, 0);
   char *label;
   const char *target;
   if (tab > IOS_MENU_TAB_SETTINGS || !(ios_menu_tabs() & (1 << tab)) || !list)
      return false;
   target = labels[tab];
   label  = (char*)malloc(strlen(target) + 1);
   if (!label)
      return false;
   strcpy(label, target);
   menu_entries_flush_stack(NULL, MENU_SETTINGS);
   stack = MENU_LIST_GET(menu_st->entries.list, 0);
   if (!stack || !stack->size)
   {
      free(label);
      return false;
   }
   file_list_free_label(stack, stack->size - 1);
   stack->list[stack->size - 1].label = label;
   stack->list[stack->size - 1].type  = types[tab];
   filebrowser_clear_type();
   ios->tab = tab;
   menu_st->selection_ptr = 0;
   return menu_driver_deferred_push_content_list(list) == 0;
}

bool ios_menu_visible(void)
{
   ios_menu_t *ios = ios_menu_current();
   return ios && ios->visible;
}

bool ios_menu_request(uint64_t session, uint64_t revision,
      size_t index, unsigned type, unsigned action)
{
   ios_menu_t *ios = ios_menu_current();
   if (!ios || !ios->visible || ios->pending || ios_menu_busy()
         || session != ios->session || revision != ios->revision
         || ios->dirty || action > IOS_MENU_SET_VALUE)
      return false;
   ios->request_revision = revision;
   ios->request_index    = index;
   ios->request_type     = type;
   ios->request_action   = action;
   ios->pending          = true;
   return true;
}

bool ios_menu_set_value(uint64_t session, uint64_t revision,
      size_t index, unsigned type, const char *value)
{
   char *copy;
   size_t len;
   if (!value || (len = strlen(value)) >= 8192)
      return false;
   copy = (char*)malloc(len + 1);
   if (!copy)
      return false;
   memcpy(copy, value, len + 1);
   if (!ios_menu_request(session, revision, index, type, IOS_MENU_SET_VALUE))
   {
      free(copy);
      return false;
   }
   ios_menu_current()->request_value = copy;
   return true;
}

unsigned ios_menu_take_action(unsigned action)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   ios_menu_t *ios            = ios_menu_current();
   file_list_t *list;
   unsigned request;
   size_t index;
   char *value;

   if (!ios)
      return action;
   if (!ios->pending)
      return action;
   ios->pending = false;
   value = ios->request_value;
   ios->request_value = NULL;
   if (!ios->visible || ios->request_revision != ios->revision
         || ios_menu_busy() || action != MENU_ACTION_NOOP)
   {
      free(value);
      return action;
   }
   request = ios->request_action;
   index   = ios->request_index;
   list    = MENU_ENTRIES_GET_SELECTION_BUF_PTR_INTERNAL(menu_st, 0);
   if (request == IOS_MENU_SET_VALUE)
   {
      menu_file_list_cbs_t *cbs = list && index < list->size
         ? (menu_file_list_cbs_t*)list->list[index].actiondata : NULL;
      rarch_setting_t *setting = cbs ? cbs->setting : NULL;
      if (value && setting && list->list[index].type == ios->request_type
            && ios_menu_value_kind(setting) != IOS_MENU_VALUE_NONE
            && !(menu_st->driver_data && *menu_st->driver_data->menu_state_msg))
      {
         menu_st->selection_ptr = index;
         ios_menu_invalidate(ios);
         if (setting->type == ST_STRING)
         {
            strlcpy(setting->value.target.string, value, setting->size);
            menu_setting_generic(setting, 0, false);
         }
         else
            menu_setting_set_with_string_representation(setting, value);
      }
      free(value);
      return action;
   }
   if (request <= IOS_MENU_RIGHT)
   {
      if (!list || index >= list->size
            || list->list[index].type != ios->request_type
            || (menu_st->driver_data && *menu_st->driver_data->menu_state_msg))
         return action;
      menu_st->selection_ptr = index;
      return request == IOS_MENU_OK ? MENU_ACTION_OK
         : request == IOS_MENU_LEFT ? MENU_ACTION_LEFT : MENU_ACTION_RIGHT;
   }
   switch (request)
   {
      case IOS_MENU_TAB:
         ios_menu_switch_tab(ios, (unsigned)index);
         break;
      case IOS_MENU_BACK:
         return MENU_ACTION_CANCEL;
      case IOS_MENU_CONFIRM:
         return MENU_ACTION_OK;
      case IOS_MENU_RESUME:
         return MENU_ACTION_RESUME;
      case IOS_MENU_PREVIOUS_PAGE:
         ios->offset = ios->offset >= IOS_MENU_PAGE_SIZE
            ? ios->offset - IOS_MENU_PAGE_SIZE : 0;
         break;
      case IOS_MENU_NEXT_PAGE:
         if (list && ios->offset + IOS_MENU_PAGE_SIZE < list->size)
            ios->offset += IOS_MENU_PAGE_SIZE;
         break;
      default:
         break;
   }
   ios_menu_invalidate(ios);
   return action;
}

static void *ios_menu_init(void **userdata, bool video_is_threaded)
{
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));
   ios_menu_t *ios     = (ios_menu_t*)calloc(1, sizeof(*ios));
   (void)video_is_threaded;
   if (!menu || !ios || !ios_menu_bridge_init(++ios_menu_session))
   {
      free(menu);
      free(ios);
      return NULL;
   }
   ios->session  = ios_menu_session;
   ios->dirty    = true;
   ios->revision = 1;
   *userdata     = ios;
   return menu;
}

static void ios_menu_free(void *userdata)
{
   ios_menu_t *ios = (ios_menu_t*)userdata;
   if (ios)
      free(ios->request_value);
   ios_menu_bridge_free();
}

static void ios_menu_context_reset(void *userdata, bool video_is_threaded)
{
   (void)video_is_threaded;
   ios_menu_invalidate((ios_menu_t*)userdata);
}

static bool ios_menu_lists_init(void *data)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *stack = MENU_LIST_GET(menu_st->entries.list, 0);
   file_list_t *list  = MENU_ENTRIES_GET_SELECTION_BUF_PTR_INTERNAL(menu_st, 0);
   (void)data;
   if (!stack || !list || !menu_entries_append(stack, NULL,
            MENU_ENUM_LABEL_MAIN_MENU_STR, MENU_ENUM_LABEL_MAIN_MENU,
            MENU_SETTINGS, 0, 0, NULL))
      return false;
   return ios_menu_switch_tab((ios_menu_t*)menu_st->userdata,
         IOS_MENU_TAB_MAIN);
}

static void ios_menu_populate(void *userdata, const char *path,
      const char *label, unsigned type)
{
   ios_menu_t *ios = (ios_menu_t*)userdata;
   (void)path;
   (void)label;
   (void)type;
   ios->offset = 0;
   ios_menu_invalidate(ios);
}

static void ios_menu_toggle(void *userdata, bool visible)
{
   ios_menu_t *ios = (ios_menu_t*)userdata;
   ios->visible   = visible;
   ios->pending   = false;
   free(ios->request_value);
   ios->request_value = NULL;
   ios_menu_invalidate(ios);
}

static void ios_menu_navigation_set(void *userdata, bool scroll)
{
   ios_menu_t *ios = (ios_menu_t*)userdata;
   size_t offset  = menu_state_get_ptr()->selection_ptr
      / IOS_MENU_PAGE_SIZE * IOS_MENU_PAGE_SIZE;
   (void)scroll;
   if (ios->offset != offset)
   {
      ios->offset = offset;
      ios_menu_invalidate(ios);
   }
}

static void ios_menu_list_insert(void *userdata, file_list_t *list,
      const char *path, const char *fullpath, const char *label,
      size_t index, unsigned type)
{
   (void)list;
   (void)path;
   (void)fullpath;
   (void)label;
   (void)index;
   (void)type;
   ios_menu_invalidate((ios_menu_t*)userdata);
}

static void ios_menu_list_clear(file_list_t *list)
{
   (void)list;
   ios_menu_invalidate(ios_menu_current());
}

static int ios_menu_list_prepend(void *userdata, file_list_t *list,
      const char *path, const char *label, size_t index)
{
   (void)list;
   (void)path;
   (void)label;
   (void)index;
   ios_menu_invalidate((ios_menu_t*)userdata);
   return 0;
}

static void ios_menu_list_free(file_list_t *list, size_t index, size_t size)
{
   (void)list;
   (void)index;
   (void)size;
   ios_menu_invalidate(ios_menu_current());
}

static int ios_menu_entry_action(void *userdata, menu_entry_t *entry,
      size_t index, enum menu_action action)
{
   /* An action may recreate the driver; invalidate before dispatch. */
   if (action != MENU_ACTION_NOOP && action != MENU_ACTION_UP
         && action != MENU_ACTION_DOWN && action != MENU_ACTION_POINTER_MOVED)
      ios_menu_invalidate((ios_menu_t*)userdata);
   return generic_menu_entry_action(userdata, entry, index, action);
}

void ios_menu_update(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   ios_menu_t *ios            = ios_menu_current();
   file_list_t *list;
   file_list_t *stack;
   nbio_budget_t budget;
   size_t end;
   if (!ios)
      return;
   if (!ios_menu_bridge_visible(ios->visible))
   {
      configuration_set_string(config_get_ptr(),
            config_get_ptr()->arrays.menu_driver, "glui");
      command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
      command_event(CMD_EVENT_REINIT, NULL);
      return;
   }
   if (!ios->visible || !menu_st->driver_data)
      return;
   ios_menu_bridge_status(menu_st->selection_ptr,
         ios->pending || ios_menu_busy() || ios->dirty,
         menu_st->driver_data->menu_state_msg);
   if (!ios->dirty || MENU_ENTRIES_NEEDS_REFRESH(menu_st)
         || (menu_st->flags & MENU_ST_FLAG_PENDING_QUICK_MENU))
      return;
   list  = MENU_ENTRIES_GET_SELECTION_BUF_PTR_INTERNAL(menu_st, 0);
   stack = MENU_LIST_GET(menu_st->entries.list, 0);
   if (!list)
      return;
   if (!ios->building)
   {
      char title[MENU_LABEL_MAX_LENGTH];
      if (ios->offset >= list->size)
         ios->offset = 0;
      title[0] = '\0';
      menu_entries_get_title(title, sizeof(title));
      ios->tab = ios_menu_stack_tab(stack);
      ios_menu_bridge_begin(ios->session, ios->revision, title,
            list->size, ios->offset, stack && stack->size ? stack->size - 1 : 0,
            ios->tab, ios_menu_tabs(),
            !retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL));
      ios->cursor   = ios->offset;
      ios->building = true;
   }
   task_nbio_slice_open(&budget);
   end   = ios->offset + IOS_MENU_PAGE_SIZE;
   if (end > list->size)
      end = list->size;
   do
   {
      menu_entry_t *entry = &ios->entry;
      menu_file_list_cbs_t *cbs;
      const char *title;
      bool boolean;
      bool checked;
      bool enabled;
      rarch_setting_t *setting;
      ios_menu_value_t value;
      char input[64];
      if (ios->cursor >= end)
         break;
      MENU_ENTRY_INITIALIZE((*entry));
      entry->flags = MENU_ENTRY_FLAG_PATH_ENABLED
         | MENU_ENTRY_FLAG_LABEL_ENABLED | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
         | MENU_ENTRY_FLAG_VALUE_ENABLED | MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
      menu_entry_get(entry, 0, ios->cursor, NULL, true);
      cbs     = (menu_file_list_cbs_t*)list->list[ios->cursor].actiondata;
      title   = *entry->rich_label ? entry->rich_label : entry->path;
      if (!*title)
         title = list->list[ios->cursor].path;
      boolean = entry->setting_type == ST_BOOL && cbs && cbs->setting;
      checked = boolean && cbs->setting->value.target.boolean
         ? *cbs->setting->value.target.boolean
         : (entry->flags & MENU_ENTRY_FLAG_CHECKED) != 0;
      enabled = cbs && (cbs->action_ok || cbs->action_select || cbs->action_right)
         && entry->type != MENU_SETTINGS_CORE_INFO_NONE
         && entry->type != MENU_INFO_MESSAGE && entry->type != MENU_SETTING_NO_ITEM;
      setting = cbs ? cbs->setting : NULL;
      value.kind = ios_menu_value_kind(setting);
      value.text = "";
      value.limit = setting ? setting->size : 0;
      value.minimum = setting ? setting->min : 0;
      value.maximum = setting ? setting->max : 0;
      value.has_minimum = setting && (setting->flags & SD_FLAG_HAS_RANGE)
         && (setting->flags & SD_FLAG_ENFORCE_MINRANGE);
      value.has_maximum = setting && (setting->flags & SD_FLAG_HAS_RANGE)
         && (setting->flags & SD_FLAG_ENFORCE_MAXRANGE);
      input[0] = '\0';
      switch (value.kind)
      {
         case IOS_MENU_VALUE_INT:
            snprintf(input, sizeof(input), "%d", setting_int_get(setting));
            value.text = input;
            break;
         case IOS_MENU_VALUE_UINT:
         case IOS_MENU_VALUE_COLOR:
            snprintf(input, sizeof(input), "%u", setting_uint_get(setting));
            value.text = input;
            break;
         case IOS_MENU_VALUE_SIZE:
            snprintf(input, sizeof(input), "%" PRIu64,
                  (uint64_t)*setting->value.target.sizet);
            value.text = input;
            break;
         case IOS_MENU_VALUE_FLOAT:
            snprintf(input, sizeof(input), "%.9g", *setting->value.target.fraction);
            value.text = input;
            break;
         case IOS_MENU_VALUE_TEXT:
            value.text = setting->value.target.string;
            break;
         default:
            break;
      }
      ios_menu_bridge_add(ios->cursor, entry->type, title,
            *entry->password_value ? entry->password_value : entry->value,
            entry->sublabel, enabled, boolean, checked,
            enabled && ios_menu_has_submenu(entry, cbs), &value);
      ios->cursor++;
   } while (task_nbio_slice_within_budget(&budget, 0, 0));
   task_nbio_slice_close(&budget);
   if (ios->cursor >= end)
   {
      ios->dirty    = false;
      ios->building = false;
      ios_menu_bridge_commit();
      ios_menu_bridge_status(menu_st->selection_ptr, ios->pending || ios_menu_busy(),
            menu_st->driver_data->menu_state_msg);
   }
}

menu_ctx_driver_t menu_ctx_ios = {
   NULL, NULL, NULL, NULL,
   ios_menu_init,
   ios_menu_free,
   ios_menu_context_reset,
   NULL,
   ios_menu_populate,
   ios_menu_toggle,
   NULL, NULL, NULL, ios_menu_navigation_set, NULL, NULL, NULL, ios_menu_lists_init,
   ios_menu_list_insert,
   ios_menu_list_prepend, ios_menu_list_free,
   ios_menu_list_clear,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   "ios",
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   ios_menu_entry_action,
   NULL
};
