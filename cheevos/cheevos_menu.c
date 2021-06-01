/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019-2021 - Brian Weiss
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

#include "cheevos_locals.h"

#include "badges.h"

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_entries.h"
#endif

#ifdef HAVE_MENU
void rcheevos_get_achievement_state(unsigned index,
      char *buffer, size_t len)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   enum msg_hash_enums enum_idx;
   rcheevos_racheevo_t *cheevo = NULL;
   bool check_measured         = false;

   if (index < rcheevos_locals->patchdata.core_count)
   {
      enum_idx    = MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY;
      if (rcheevos_locals->patchdata.core)
         cheevo   = &rcheevos_locals->patchdata.core[index];
   }
   else
   {
      enum_idx    = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY;
      if (rcheevos_locals->patchdata.unofficial)
         cheevo   = &rcheevos_locals->patchdata.unofficial[index - 
            rcheevos_locals->patchdata.core_count];
   }

   if (!cheevo || !cheevo->memaddr)
      enum_idx       = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY;
   else if (!(cheevo->active & RCHEEVOS_ACTIVE_HARDCORE))
      enum_idx       = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE;
   else if (!(cheevo->active & RCHEEVOS_ACTIVE_SOFTCORE))
   {
      enum_idx       = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY;
      /* if in hardcore mode, track progress towards hardcore unlock */
      check_measured = rcheevos_locals->hardcore_active;
   }
   /* Use either "Locked" for core or "Unofficial" 
    * for unofficial as set above and track progress */
   else
      check_measured = true;

   strlcpy(buffer, msg_hash_to_str(enum_idx), len);

   if (check_measured)
   {
      unsigned value, target;
      if (rc_runtime_get_achievement_measured(&rcheevos_locals->runtime,
            cheevo->id, &value, &target) && target > 0 && value > 0)
      {
         char measured_buffer[12];
         const unsigned int clamped_value = MIN(value, target);
         const int percent = (int)(((unsigned long)clamped_value) * 100 / target);

         snprintf(measured_buffer, sizeof(measured_buffer),
               " - %d%%", percent);
         strlcat(buffer, measured_buffer, len);
      }
   }
}

static void rcheevos_append_menu_achievement(
      menu_displaylist_info_t* info, size_t idx,
      rcheevos_racheevo_t* cheevo)
{
   bool badge_grayscale;

   menu_entries_append_enum(info->list, cheevo->title,
      cheevo->description, MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
      (unsigned)(MENU_SETTINGS_CHEEVOS_START + idx), 0, 0);

   /* TODO/FIXME - can we refactor this?
    * Make badge_grayscale true by default, then
    * have one conditional (second one here) that sets it
    * to false */
   if (!cheevo->memaddr)
      badge_grayscale = true;  /* unsupported */
   else if (!(cheevo->active & RCHEEVOS_ACTIVE_HARDCORE) || 
            !(cheevo->active & RCHEEVOS_ACTIVE_SOFTCORE))
      badge_grayscale = false; /* unlocked */
   else
      badge_grayscale = true;  /* locked */

   cheevos_set_menu_badge((int)idx, cheevo->badge, badge_grayscale);
}
#endif

void rcheevos_populate_hardcore_pause_menu(void* data)
{
#ifdef HAVE_MENU
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   menu_displaylist_info_t* info = (menu_displaylist_info_t*)data;
   settings_t* settings = config_get_ptr();
   bool cheevos_hardcore_mode_enable = settings->bools.cheevos_hardcore_mode_enable;

   if (cheevos_hardcore_mode_enable && rcheevos_locals->loaded)
   {
      if (rcheevos_locals->hardcore_active)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL),
               msg_hash_to_str(MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_CANCEL,
               MENU_SETTING_ACTION_CLOSE, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE,
               MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS, 0, 0);
      }
      else
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL),
               msg_hash_to_str(MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL),
               MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_CANCEL,
               MENU_SETTING_ACTION_CLOSE, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_RESUME),
               MENU_ENUM_LABEL_ACHIEVEMENT_RESUME,
               MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS, 0, 0);
      }
   }
#endif
}

void rcheevos_populate_menu(void* data)
{
#ifdef HAVE_MENU
   int i                             = 0;
   int count                         = 0;
   rcheevos_racheevo_t* cheevo       = NULL;
   menu_displaylist_info_t* info     = (menu_displaylist_info_t*)data;
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   settings_t* settings              = config_get_ptr();
   bool cheevos_enable               = settings->bools.cheevos_enable;
   bool cheevos_hardcore_mode_enable = settings->bools.cheevos_hardcore_mode_enable;
   bool cheevos_test_unofficial      = settings->bools.cheevos_test_unofficial;

   CHEEVOS_LOG(RCHEEVOS_TAG "populate_menu");

   if (   cheevos_enable
       && cheevos_hardcore_mode_enable
       && rcheevos_locals->loaded)
   {
      if (rcheevos_locals->hardcore_active)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_MENU),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU,
               MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS, 0, 0);
      else
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_MENU),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU,
               MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS, 0, 0);
   }

   cheevo = rcheevos_locals->patchdata.core;
   for (count = rcheevos_locals->patchdata.core_count; count > 0; count--)
      rcheevos_append_menu_achievement(info, i++, cheevo++);

   if (cheevos_test_unofficial)
   {
      cheevo = rcheevos_locals->patchdata.unofficial;
      for (count = rcheevos_locals->patchdata.unofficial_count; count > 0; count--)
         rcheevos_append_menu_achievement(info, i++, cheevo++);
   }

   if (i == 0)
   {
      if (!rcheevos_locals->core_supports)
      {
         menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
            msg_hash_to_str(MENU_ENUM_LABEL_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
            MENU_ENUM_LABEL_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
            FILE_TYPE_NONE, 0, 0);
      }
      else if (!settings->arrays.cheevos_token[0])
      {
         menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN),
            msg_hash_to_str(MENU_ENUM_LABEL_NOT_LOGGED_IN),
            MENU_ENUM_LABEL_NOT_LOGGED_IN,
            FILE_TYPE_NONE, 0, 0);
      }
      else
      {
         menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0);
      }
   }
#endif
}

