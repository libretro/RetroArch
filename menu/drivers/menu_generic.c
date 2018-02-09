/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2016-2017 - Brad Parker
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
#include "../../retroarch.h"

static enum action_iterate_type action_iterate_type(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_HELP:
      case MENU_LABEL_HELP_CONTROLS:
      case MENU_LABEL_HELP_WHAT_IS_A_CORE:
      case MENU_LABEL_HELP_LOADING_CONTENT:
      case MENU_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
      case MENU_LABEL_CHEEVOS_DESCRIPTION:
      case MENU_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
      case MENU_LABEL_HELP_SCANNING_CONTENT:
         return ITERATE_TYPE_HELP;
      case MENU_LABEL_INFO_SCREEN:
         return ITERATE_TYPE_INFO;
      case MENU_LABEL_CUSTOM_BIND:
      case MENU_LABEL_CUSTOM_BIND_ALL:
      case MENU_LABEL_CUSTOM_BIND_DEFAULTS:
         return ITERATE_TYPE_BIND;
   }

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
   enum action_iterate_type iterate_type;
   unsigned file_type             = 0;
   int ret                        = 0;
   uint32_t hash                  = 0;
   enum msg_hash_enums enum_idx   = MSG_UNKNOWN;
   const char *label              = NULL;
   menu_handle_t *menu            = (menu_handle_t*)data;
   size_t selection               = menu_navigation_get_selection();

   menu_entries_get_last_stack(NULL, &label, &file_type, &enum_idx, NULL);

   if (!menu)
      return 0;

   menu->menu_state_msg[0]   = '\0';

   if (!string_is_empty(label))
      hash                   = msg_hash_calculate(label);
   iterate_type              = action_iterate_type(hash);

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
            menu_file_list_cbs_t *cbs  = selection_buf ?
               (menu_file_list_cbs_t*)
			   file_list_get_actiondata_at_offset(selection_buf, selection)
               : NULL;

            if (cbs->enum_idx != MSG_UNKNOWN)
            {
               ret = menu_hash_get_help_enum(cbs->enum_idx,
                     menu->menu_state_msg, sizeof(menu->menu_state_msg));
            }
            else
            {
               unsigned type = 0;
               enum msg_hash_enums enum_idx = MSG_UNKNOWN;
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
            /* FIXME: Crappy hack, needed for mouse controls
             * to not be completely broken in case we press back.
             *
             * We need to fix this entire mess, mouse controls
             * should not rely on a hack like this in order to work. */
            selection = MAX(MIN(selection, (menu_entries_get_size() - 1)), 0);

            menu_entry_init(&entry);
            menu_entry_get(&entry, 0, selection, NULL, false);
            ret = menu_entry_action(&entry,
                  (unsigned)selection, (enum menu_action)action);
            menu_entry_free(&entry);
            if (ret)
               goto end;

            BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);

            /* Have to defer it so we let settings refresh. */
            menu_dialog_push();
         }
         break;
   }

   BIT64_SET(menu->state, MENU_STATE_BLIT);

   if (BIT64_GET(menu->state, MENU_STATE_POP_STACK))
   {
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
