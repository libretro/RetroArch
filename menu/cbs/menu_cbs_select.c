/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <queues/task_queue.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_entries.h"
#include "../menu_setting.h"
#include "../../tasks/tasks_internal.h"

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#include "../../network/netplay/netplay_discovery.h"
#endif

#ifndef BIND_ACTION_SELECT
#define BIND_ACTION_SELECT(cbs, name) \
   cbs->action_select = name; \
   cbs->action_select_ident = #name;
#endif

static int action_select_default(const char *path, const char *label, unsigned type,
      size_t idx, size_t entry_idx)
{
   menu_entry_t entry;
   int ret                    = 0;
   enum menu_action action    = MENU_ACTION_NOOP;
   menu_file_list_cbs_t *cbs  = NULL;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   menu_entry_init(&entry);
   /* Note: If menu_entry_action() is modified,
    * will have to verify that these parameters
    * remain unused... */
   entry.rich_label_enabled = false;
   entry.value_enabled      = false;
   entry.sublabel_enabled   = false;
   menu_entry_get(&entry, 0, idx, NULL, false);

   if (selection_buf)
      cbs                     = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(selection_buf, idx);

   if (!cbs)
      return -1;

   if (cbs->setting)
   {
      switch (setting_get_type(cbs->setting))
      {
         case ST_BOOL:
         case ST_INT:
         case ST_UINT:
         case ST_SIZE:
         case ST_FLOAT:
         case ST_STRING_OPTIONS:
            if (cbs->action_ok)
               action     = MENU_ACTION_OK;
            else
               action = MENU_ACTION_RIGHT;
            break;
         case ST_PATH:
         case ST_DIR:
         case ST_ACTION:
         case ST_STRING:
         case ST_HEX:
         case ST_BIND:
            action = MENU_ACTION_OK;
            break;
         default:
            break;
      }
   }

   if (action == MENU_ACTION_NOOP)
   {
       if (cbs->action_ok)
           action     = MENU_ACTION_OK;
       else
       {
           if (cbs->action_start)
               action = MENU_ACTION_START;
           if (cbs->action_right)
               action = MENU_ACTION_RIGHT;
       }
   }

   if (action != MENU_ACTION_NOOP)
       ret = menu_entry_action(&entry, idx, action);

   task_queue_check();

   return ret;
}

static int action_select_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_path_use_directory(path, label, type, idx, 0 /* unused */);
}

static int action_select_core_setting(const char *path, const char *label, unsigned type,
      size_t idx, size_t entry_idx)
{
   return action_ok_core_option_dropdown_list(path, label, type, idx, 0);
}

static int action_select_input_desc(const char *path, const char *label, unsigned type,
      size_t idx, size_t entry_idx)
{
   return action_right_input_desc(type, label, true);
}

static int action_select_input_desc_kbd(const char *path,
      const char *label, unsigned type,
      size_t idx, size_t entry_idx)
{
   return action_right_input_desc_kbd(type, label, true);
}

#ifdef HAVE_NETWORKING
static int action_select_netplay_connect_room(const char *path,
      const char *label, unsigned type,
      size_t idx, size_t entry_idx)
{
   char tmp_hostname[4115];
   unsigned offset = idx - 4;

   tmp_hostname[0] = '\0';

   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
   netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

   if (netplay_room_list[offset].host_method == NETPLAY_HOST_METHOD_MITM)
      snprintf(tmp_hostname,
            sizeof(tmp_hostname),
            "%s|%d",
            netplay_room_list[offset].mitm_address,
            netplay_room_list[offset].mitm_port);
   else
      snprintf(tmp_hostname,
            sizeof(tmp_hostname),
            "%s|%d",
            netplay_room_list[offset].address,
            netplay_room_list[offset].port);

   task_push_netplay_crc_scan(
         netplay_room_list[offset].gamecrc,
         netplay_room_list[offset].gamename,
         tmp_hostname,
         netplay_room_list[offset].corename,
         netplay_room_list[offset].subsystem_name);

   return 0;
}
#endif

static int menu_cbs_init_bind_select_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      BIND_ACTION_SELECT(cbs, action_select_input_desc);
   }
   else if (type >= MENU_SETTINGS_INPUT_DESC_KBD_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
   {
      BIND_ACTION_SELECT(cbs, action_select_input_desc_kbd);
   }
   else
   {

      switch (type)
      {
         case FILE_TYPE_USE_DIRECTORY:
            BIND_ACTION_SELECT(cbs, action_select_path_use_directory);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_select_compare_label(menu_file_list_cbs_t *cbs,
      const char *label)
{
   return -1;
}

int menu_cbs_init_bind_select(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_SELECT(cbs, action_select_default);

#ifdef HAVE_NETWORKING
   if (cbs->enum_idx == MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM)
   {
      BIND_ACTION_SELECT(cbs, action_select_netplay_connect_room);
      return 0;
   }
#endif

   if ((type >= MENU_SETTINGS_CORE_OPTION_START) &&
       (type < MENU_SETTINGS_CHEEVOS_START))
   {
      BIND_ACTION_SELECT(cbs, action_select_core_setting);
      return 0;
   }

   if (menu_cbs_init_bind_select_compare_label(cbs, label) == 0)
      return 0;

   if (menu_cbs_init_bind_select_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
