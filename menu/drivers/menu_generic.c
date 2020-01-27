/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <compat/strl.h>
#include <string/stdstring.h>

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../widgets/menu_dialog.h"
#include "../widgets/menu_input_bind_dialog.h"

#include "../../verbosity.h"
#include "../../content.h"
#ifdef HAVE_ACCESSIBILITY
#include "../../accessibility.h"
#endif
#include "../../retroarch.h"

static enum action_iterate_type action_iterate_type(const char *label)
{
   if (string_is_equal(label, "info_screen"))
      return ITERATE_TYPE_INFO;
   if (
         string_is_equal(label, "help") ||
         string_is_equal(label, "help_controls") ||
         string_is_equal(label, "help_what_is_a_core") ||
         string_is_equal(label, "help_loading_content") ||
         string_is_equal(label, "help_scanning_content") ||
         string_is_equal(label, "help_change_virtual_gamepad") ||
         string_is_equal(label, "help_audio_video_troubleshooting") ||
         string_is_equal(label, "help_send_debug_info") ||
         string_is_equal(label, "cheevos_description")
         )
      return ITERATE_TYPE_HELP;
   if (
         string_is_equal(label, "custom_bind") ||
         string_is_equal(label, "custom_bind_all") ||
         string_is_equal(label, "custom_bind_defaults")
      )
         return ITERATE_TYPE_BIND;

   return ITERATE_TYPE_DEFAULT;
}


/**
 * menu_iterate:
 * @input                    : input sample for this frame
 * @old_input                : input sample of the previous frame
 * @trigger_input            : difference' input sample - difference
 *                             between 'input' and 'old_input'
 *
 * Runs RetroArch menu for one frame.
 *
 * Returns: 0 on success, -1 if we need to quit out of the loop.
 **/
int generic_menu_iterate(void *data, void *userdata, enum menu_action action)
{
   static enum action_iterate_type last_iterate_type = ITERATE_TYPE_DEFAULT;

   enum action_iterate_type iterate_type;
   unsigned file_type             = 0;
   int ret                        = 0;
   const char *label              = NULL;
   menu_handle_t *menu            = (menu_handle_t*)data;

   (void)last_iterate_type;

   if (!menu)
      return 0;

   menu_entries_get_last_stack(NULL, &label, &file_type, NULL, NULL);

   menu->menu_state_msg[0]   = '\0';

   iterate_type              = action_iterate_type(label);

   menu_driver_set_binding_state(iterate_type == ITERATE_TYPE_BIND);

   if (     action != MENU_ACTION_NOOP
         || menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL)
         || menu_display_get_update_pending())
   {
      BIT64_SET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER);
   }
   switch (iterate_type)
   {
      case ITERATE_TYPE_HELP:
         ret = menu_dialog_iterate(
               menu->menu_state_msg, sizeof(menu->menu_state_msg), label);

#ifdef HAVE_ACCESSIBILITY
         if (iterate_type != last_iterate_type && is_accessibility_enabled())
            accessibility_speak_priority(menu->menu_state_msg, 10);
#endif

         BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);
         if (ret == 1 || action == MENU_ACTION_OK)
         {
            BIT64_SET(menu->state, MENU_STATE_POP_STACK);
            menu_dialog_set_active(false);
         }

         if (action == MENU_ACTION_CANCEL)
         {
            BIT64_SET(menu->state, MENU_STATE_POP_STACK);
            menu_dialog_set_active(false);
         }
         break;
      case ITERATE_TYPE_BIND:
         {
            menu_input_ctx_bind_t bind;

            bind.s   = menu->menu_state_msg;
            bind.len = sizeof(menu->menu_state_msg);

            if (menu_input_key_bind_iterate(&bind))
            {
               size_t selection = menu_navigation_get_selection();
               menu_entries_pop_stack(&selection, 0, 0);
               menu_navigation_set_selection(selection);
            }
            else
               BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         }
         break;
      case ITERATE_TYPE_INFO:
         {
            file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
            size_t selection           = menu_navigation_get_selection();
            menu_file_list_cbs_t *cbs  = selection_buf ?
               (menu_file_list_cbs_t*)
			   file_list_get_actiondata_at_offset(selection_buf, selection)
               : NULL;

            if (cbs && cbs->enum_idx != MSG_UNKNOWN)
            {
               ret = menu_hash_get_help_enum(cbs->enum_idx,
                     menu->menu_state_msg, sizeof(menu->menu_state_msg));

#ifdef HAVE_ACCESSIBILITY
               if (iterate_type != last_iterate_type && is_accessibility_enabled())
               {
                  if (string_is_equal(menu->menu_state_msg, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE)))
                  {
                     char current_sublabel[255];
                     get_current_menu_sublabel(current_sublabel, sizeof(current_sublabel));
                     if (string_is_equal(current_sublabel, ""))
                        accessibility_speak_priority(menu->menu_state_msg, 10);
                     else
                        accessibility_speak_priority(current_sublabel, 10);
                  }
                  else
                     accessibility_speak_priority(menu->menu_state_msg, 10);
               }
#endif
            }
            else
            {
               unsigned type = 0;
               enum msg_hash_enums enum_idx = MSG_UNKNOWN;
               size_t selection             = menu_navigation_get_selection();
               menu_entries_get_at_offset(selection_buf, selection,
                     NULL, NULL, &type, NULL, NULL);

               switch (type)
               {
                  case FILE_TYPE_FONT:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_FONT;
                     break;
                  case FILE_TYPE_RDB:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_RDB;
                     break;
                  case FILE_TYPE_OVERLAY:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY;
                     break;
#ifdef HAVE_VIDEO_LAYOUT
                  case FILE_TYPE_VIDEO_LAYOUT:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_VIDEO_LAYOUT;
                     break;
#endif
                  case FILE_TYPE_CHEAT:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_CHEAT;
                     break;
                  case FILE_TYPE_SHADER_PRESET:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET;
                     break;
                  case FILE_TYPE_SHADER:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_SHADER;
                     break;
                  case FILE_TYPE_REMAP:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_REMAP;
                     break;
                  case FILE_TYPE_RECORD_CONFIG:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG;
                     break;
                  case FILE_TYPE_CURSOR:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_CURSOR;
                     break;
                  case FILE_TYPE_CONFIG:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_CONFIG;
                     break;
                  case FILE_TYPE_CARCHIVE:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE;
                     break;
                  case FILE_TYPE_DIRECTORY:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
                     break;
                  case FILE_TYPE_VIDEOFILTER:            /* TODO/FIXME */
                  case FILE_TYPE_AUDIOFILTER:            /* TODO/FIXME */
                  case FILE_TYPE_SHADER_SLANG:           /* TODO/FIXME */
                  case FILE_TYPE_SHADER_GLSL:            /* TODO/FIXME */
                  case FILE_TYPE_SHADER_HLSL:            /* TODO/FIXME */
                  case FILE_TYPE_SHADER_CG:              /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_GLSLP:    /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_HLSLP:    /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_CGP:      /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_SLANGP:   /* TODO/FIXME */
                  case FILE_TYPE_PLAIN:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE;
                     break;
                  default:
                     break;
               }

               if (enum_idx != MSG_UNKNOWN)
                  ret = menu_hash_get_help_enum(enum_idx,
                        menu->menu_state_msg, sizeof(menu->menu_state_msg));

            }
         }
         BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);
         if (action == MENU_ACTION_OK || action == MENU_ACTION_CANCEL)
         {
            BIT64_SET(menu->state, MENU_STATE_POP_STACK);
         }
         menu_dialog_set_active(false);
         break;
      case ITERATE_TYPE_DEFAULT:
         {
            menu_entry_t entry;
            size_t selection = menu_navigation_get_selection();
            /* FIXME: Crappy hack, needed for mouse controls
             * to not be completely broken in case we press back.
             *
             * We need to fix this entire mess, mouse controls
             * should not rely on a hack like this in order to work. */
            selection = MAX(MIN(selection, (menu_entries_get_size() - 1)), 0);

            menu_entry_init(&entry);
            /* Note: If menu_entry_action() is modified,
             * will have to verify that these parameters
             * remain unused... */
            entry.rich_label_enabled = false;
            entry.value_enabled      = false;
            entry.sublabel_enabled   = false;
            menu_entry_get(&entry, 0, selection, NULL, false);
            ret = menu_entry_action(&entry,
                  selection, (enum menu_action)action);
            if (ret)
               goto end;

            BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);

            /* Have to defer it so we let settings refresh. */
            menu_dialog_push();
         }
         break;
   }

#ifdef HAVE_ACCESSIBILITY
   if ((last_iterate_type == ITERATE_TYPE_HELP || last_iterate_type == ITERATE_TYPE_INFO) && last_iterate_type != iterate_type && is_accessibility_enabled())
      accessibility_speak_priority("Closed dialog.", 10);
#endif

   last_iterate_type = iterate_type;
   BIT64_SET(menu->state, MENU_STATE_BLIT);

   if (BIT64_GET(menu->state, MENU_STATE_POP_STACK))
   {
      size_t selection         = menu_navigation_get_selection();
      size_t new_selection_ptr = selection;
      menu_entries_pop_stack(&new_selection_ptr, 0, 0);
      menu_navigation_set_selection(selection);
   }

   if (BIT64_GET(menu->state, MENU_STATE_POST_ITERATE))
      menu_input_post_iterate(&ret, action);

end:
   if (ret)
      return -1;
   return 0;
}

bool generic_menu_init_list(void *data)
{
   menu_displaylist_info_t info;
   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf   = menu_entries_get_selection_buf_ptr(0);

   menu_displaylist_info_init(&info);

   info.label    = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.enum_idx = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append_enum(menu_stack,
         info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0);

   info.list  = selection_buf;

   if (menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info))
      menu_displaylist_process(&info);

   menu_displaylist_info_free(&info);

   return true;
}

int generic_menu_entry_action(
      void *userdata, menu_entry_t *entry, size_t i, enum menu_action action)
{
   int ret                    = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (cbs && cbs->action_up)
            ret = cbs->action_up(entry->type, entry->label);
         break;
      case MENU_ACTION_DOWN:
         if (cbs && cbs->action_down)
            ret = cbs->action_down(entry->type, entry->label);
         break;
      case MENU_ACTION_SCROLL_UP:
         menu_driver_ctl(MENU_NAVIGATION_CTL_DESCEND_ALPHABET, NULL);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_driver_ctl(MENU_NAVIGATION_CTL_ASCEND_ALPHABET, NULL);
         break;
      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            ret = cbs->action_cancel(entry->path,
                  entry->label, entry->type, i);
         break;
      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            ret = cbs->action_ok(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_START:
         if (cbs && cbs->action_start)
            ret = cbs->action_start(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_LEFT:
         if (cbs && cbs->action_left)
            ret = cbs->action_left(entry->type, entry->label, false);
         break;
      case MENU_ACTION_RIGHT:
         if (cbs && cbs->action_right)
            ret = cbs->action_right(entry->type, entry->label, false);
         break;
      case MENU_ACTION_INFO:
         if (cbs && cbs->action_info)
            ret = cbs->action_info(entry->type, entry->label);
         break;
      case MENU_ACTION_SELECT:
         if (cbs && cbs->action_select)
            ret = cbs->action_select(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_SEARCH:
         menu_input_dialog_start_search();
         break;
      case MENU_ACTION_SCAN:
         if (cbs && cbs->action_scan)
            ret = cbs->action_scan(entry->path,
                  entry->label, entry->type, i);
         break;
      default:
         break;
   }

   cbs = selection_buf ? (menu_file_list_cbs_t*)
      selection_buf->list[i].actiondata : NULL;

   if (cbs && cbs->action_refresh)
   {
      if (menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL))
      {
         bool refresh            = false;
         file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);

         cbs->action_refresh(selection_buf, menu_stack);
         menu_entries_ctl(MENU_ENTRIES_CTL_UNSET_REFRESH, &refresh);
      }
   }

#ifdef HAVE_ACCESSIBILITY
   if (     action != 0 
         && is_accessibility_enabled() 
         && !is_input_keyboard_display_on())
   {
      char current_label[255];
      char current_value[255];
      char title_name[255];
      char speak_string[512];

      strlcpy(title_name, "", sizeof(title_name));
      strlcpy(current_label, "", sizeof(current_label));
      get_current_menu_value(current_value, sizeof(current_value));

      switch (action)
      {
         case MENU_ACTION_INFO:
            break;
         case MENU_ACTION_OK:
         case MENU_ACTION_LEFT:
         case MENU_ACTION_RIGHT:
         case MENU_ACTION_CANCEL:
            menu_entries_get_title(title_name, sizeof(title_name));
         case MENU_ACTION_UP:
         case MENU_ACTION_DOWN:
         case MENU_ACTION_SCROLL_UP:
         case MENU_ACTION_SCROLL_DOWN:
            get_current_menu_label(current_label, sizeof(current_label));
            break;
         case MENU_ACTION_START:
         case MENU_ACTION_SELECT:
         case MENU_ACTION_SEARCH:
            get_current_menu_label(current_label, sizeof(current_label));
         case MENU_ACTION_SCAN:
         default:
            break;
      }

      strlcpy(speak_string, "", sizeof(speak_string));
      if (!string_is_equal(title_name, ""))
      {
         strlcpy(speak_string, title_name, sizeof(speak_string));
         strlcat(speak_string, " ", sizeof(speak_string));
      }
      strlcat(speak_string, current_label, sizeof(speak_string));
      if (!string_is_equal(current_value, "..."))
      {
         strlcat(speak_string, " ", sizeof(speak_string));
         strlcat(speak_string, current_value, sizeof(speak_string));
      }

      if (!string_is_equal(speak_string, ""))
         accessibility_speak_priority(speak_string, 10);
   }
#endif

   return ret;
}
