// JM: The idea of this file is that these will be moved down into
// ../../../menu/something

void get_core_title(char *title_msg, size_t title_msg_len)
{
   global_t *global          = global_get_ptr();
   const char *core_name     = global->menu.info.library_name;
   const char *core_version  = global->menu.info.library_version;

   if (!core_name)
      core_name = global->system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   if (!core_version)
      core_version = global->system.info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(title_msg, title_msg_len, "%s - %s %s", PACKAGE_VERSION,
         core_name, core_version);
}

enum menu_entry_type
{
   MENU_ENTRY_ACTION = 0,
   MENU_ENTRY_BOOL,
   MENU_ENTRY_INT,
   MENU_ENTRY_UINT,
   MENU_ENTRY_FLOAT,
   MENU_ENTRY_PATH,
   MENU_ENTRY_DIR,
   MENU_ENTRY_STRING,
   MENU_ENTRY_HEX,
   MENU_ENTRY_BIND,
   MENU_ENTRY_ENUM,
};

rarch_setting_t *get_menu_entry_setting(uint32_t i)
{
   menu_handle_t *menu       = menu_driver_get_ptr();
   rarch_setting_t *setting;
   const char *path = NULL, *entry_label = NULL;
   unsigned type = 0;
   const char *dir           = NULL;
   const char *label         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();
   unsigned menu_type        = 0;

   menu_list_get_last_stack(menu_list, &dir, &label, &menu_type);

   menu_list_get_at_offset(menu_list->selection_buf, i, &path,
         &entry_label, &type);

   setting = setting_find_setting
      (menu->list_settings,
       menu_list->selection_buf->list[i].label);

   return setting;
}

enum menu_entry_type get_menu_entry_type(uint32_t i)
{
   rarch_setting_t *setting;
   const char *path = NULL, *entry_label = NULL;
   unsigned type = 0;
   const char *dir           = NULL;
   const char *label         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();
   unsigned menu_type        = 0;

   menu_list_get_last_stack(menu_list, &dir, &label, &menu_type);

   menu_list_get_at_offset(menu_list->selection_buf, i, &path,
         &entry_label, &type);

   setting = get_menu_entry_setting(i);

   // XXX Really a special kind of ST_ACTION, but this should be
   // changed
   if (setting_is_of_path_type(setting))
      return MENU_ENTRY_PATH;
   else if (setting && setting->type == ST_BOOL )
      return MENU_ENTRY_BOOL;
   else if (setting && setting->type == ST_BIND )
      return MENU_ENTRY_BIND;
   else if (setting_is_of_enum_type(setting))
      return MENU_ENTRY_ENUM;
   else if (setting && setting->type == ST_INT )
      return MENU_ENTRY_INT;
   else if (setting && setting->type == ST_UINT )
      return MENU_ENTRY_UINT;
   else if (setting && setting->type == ST_FLOAT )
      return MENU_ENTRY_FLOAT;
   else if (setting && setting->type == ST_PATH )
      return MENU_ENTRY_PATH;
   else if (setting && setting->type == ST_DIR )
      return MENU_ENTRY_DIR;
   else if (setting && setting->type == ST_STRING )
      return MENU_ENTRY_STRING;
   else if (setting && setting->type == ST_HEX )
      return MENU_ENTRY_HEX;
   else
      return MENU_ENTRY_ACTION;
}

const char *get_menu_entry_label(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   if (setting)
      return setting->short_description;
   return "";
}

uint32_t menu_entry_bool_value_get(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   return *setting->value.boolean;
}

void menu_entry_bool_value_set(uint32_t i, uint32_t new_val)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   *setting->value.boolean = new_val;
}

struct string_list *menu_entry_enum_values(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   return string_split(setting->values, "|");
}

void menu_entry_enum_value_set_with_string(uint32_t i, const char *s)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   setting_set_with_string_representation(setting, s);
}

int32_t menu_entry_bind_index(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   if (setting->index)
      return setting->index - 1;
   return 0;
}

void menu_entry_bind_key_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   BINDFOR(*setting).key = value;
}

void menu_entry_bind_joykey_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   BINDFOR(*setting).joykey = value;
}

void menu_entry_bind_joyaxis_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   BINDFOR(*setting).joyaxis = value;
}

void menu_entry_pathdir_selected(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   if (setting_is_of_path_type(setting))
      setting->action_toggle( setting, MENU_ACTION_RIGHT, false);
}

uint32_t menu_entry_pathdir_allow_empty(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   return setting->flags & SD_FLAG_ALLOW_EMPTY;
}

uint32_t menu_entry_pathdir_for_directory(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   return setting->flags & SD_FLAG_PATH_DIR;
}

const char *menu_entry_pathdir_value_get(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   return setting->value.string;
}

void menu_entry_pathdir_value_set(uint32_t i, const char *s)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   setting_set_with_string_representation(setting, s);
}

const char *menu_entry_pathdir_extensions(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   return setting->values;
}

void menu_entry_reset(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   setting_reset_setting(setting);
}

void menu_entry_value_get(uint32_t i, char *s, size_t len)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   setting_get_string_representation(setting, s, len);
}

void menu_entry_value_set(uint32_t i, const char *s)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   setting_set_with_string_representation(setting, s);
}

uint32_t menu_entry_num_has_range(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   return (setting->flags & SD_FLAG_HAS_RANGE);
}

float menu_entry_num_min(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   return setting->min;
}

float menu_entry_num_max(uint32_t i)
{
   rarch_setting_t *setting = get_menu_entry_setting(i);
   return setting->max;
}

/* Returns true if the menu should reload */
uint32_t menu_select_entry(uint32_t i)
{
   menu_entry_t entry;
   menu_file_list_cbs_t *cbs = NULL;
   menu_navigation_t *nav    = menu_navigation_get_ptr();
   menu_list_t    *menu_list = menu_list_get_ptr();
   rarch_setting_t *setting  = menu_setting_find(
         menu_list->selection_buf->list[i].label);

   menu_list_get_entry(&entry, i, NULL, false);

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(menu_list->selection_buf, i);

   if (setting_is_of_path_type(setting))
      return false;
   if (setting_is_of_general_type(setting))
   {
      nav->selection_ptr = i;
      if (cbs && cbs->action_ok)
         cbs->action_ok(entry.path, entry.label, entry.type, i);

      return false;
   }

   nav->selection_ptr = i;
   if (cbs && cbs->action_ok)
      cbs->action_ok(entry.path, entry.label, entry.type, i);
   else
   {
      if (cbs && cbs->action_start)
         cbs->action_start(entry.type, entry.label, MENU_ACTION_START);
      if (cbs && cbs->action_toggle)
         cbs->action_toggle(entry.type, entry.label, MENU_ACTION_RIGHT, true);
      menu_list_push(menu_list->menu_stack, "",
            "info_screen", 0, i);
   }
   return true;
}
