/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Andrés Suárez
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

#if defined(HAVE_CONFIG_H)
#include "../config.h"
#endif

#include <locale.h>

#include <retro_timers.h>
#include <lists/dir_list.h>
#include <string/stdstring.h>
#include <compat/strcasestr.h>
#include <encodings/utf.h>
#include <streams/file_stream.h>
#include <time/rtime.h>

#include "menu_driver.h"
#include "menu_cbs.h"
#include "../list_special.h"
#include "../paths.h"
#include "../tasks/task_powerstate.h"
#include "../tasks/tasks_internal.h"
#include "../verbosity.h"

#include "frontend/frontend_driver.h"

#ifdef HAVE_LANGEXTRA
/* This file has a UTF8 BOM, we assume HAVE_LANGEXTRA
 * is only enabled for compilers that can support this. */
#include "../input/input_osk_utf8_pages.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos_menu.h"
#endif

#include "../input/input_driver.h"
#include "../input/input_remapping.h"
#include "../performance_counters.h"

static bool menu_should_pop_stack(const char *label)
{
   /* > Info box */
   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INFO_SCREEN)))
      return true;
   /* > Help box */
   if (string_starts_with_size(label, "help", STRLEN_CONST("help")))
      if (
               string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CONTROLS))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LOADING_CONTENT))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SCANNING_CONTENT))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SEND_DEBUG_INFO))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_DESCRIPTION)))
         return true;
   if (
         string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_DESCRIPTION)))
      return true;
   return false;
}

size_t menu_navigation_get_selection(void)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   return menu_st->selection_ptr;
}

void menu_navigation_set_selection(size_t val)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   menu_st->selection_ptr      = val;
}

void menu_entry_get(menu_entry_t *entry, size_t stack_idx,
      size_t i, void *userdata, bool use_representation)
{
   char newpath[255];
   const char *path            = NULL;
   const char *entry_label     = NULL;
   menu_file_list_cbs_t *cbs   = NULL;
   struct menu_state *menu_st  = menu_state_get_ptr();
   file_list_t *selection_buf  = MENU_ENTRIES_GET_SELECTION_BUF_PTR_INTERNAL(menu_st, stack_idx);
   file_list_t *list           = (userdata) ? (file_list_t*)userdata : selection_buf;
   bool path_enabled           = entry->path_enabled;

   newpath[0]                  = '\0';

   if (!list)
      return;

   path                       = list->list[i].path;
   entry_label                = list->list[i].label;
   entry->type                = list->list[i].type;
   entry->entry_idx           = list->list[i].entry_idx;

   cbs                        = (menu_file_list_cbs_t*)list->list[i].actiondata;
   entry->idx                 = (unsigned)i;

   if (entry->label_enabled && !string_is_empty(entry_label))
      strlcpy(entry->label, entry_label, sizeof(entry->label));

   if (cbs)
   {
      const char *label             = NULL;

      entry->enum_idx               = cbs->enum_idx;
      entry->checked                = cbs->checked;

      file_list_get_last(MENU_LIST_GET(menu_st->entries.list, 0),
            NULL, &label, NULL, NULL);

      if (entry->rich_label_enabled && cbs->action_label)
      {
         cbs->action_label(list,
               entry->type, (unsigned)i,
               label, path,
               entry->rich_label,
               sizeof(entry->rich_label));

         if (string_is_empty(entry->rich_label))
            path_enabled = true;
      }

      if ((path_enabled || entry->value_enabled) &&
          cbs->action_get_value &&
          use_representation)
      {
         cbs->action_get_value(list,
               &entry->spacing, entry->type,
               (unsigned)i, label,
               entry->value,
               entry->value_enabled ? sizeof(entry->value) : 0,
               path,
               newpath,
               path_enabled ? sizeof(newpath) : 0);

         if (!string_is_empty(entry->value))
         {
            if (entry->enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
            {
               size_t j;
               size_t size = strlcpy(entry->password_value, entry->value,
                     sizeof(entry->password_value));
               for (j = 0; j < size; j++)
                  entry->password_value[j] = '*';
            }
         }
      }

      if (entry->sublabel_enabled)
      {
         if (!string_is_empty(cbs->action_sublabel_cache))
            strlcpy(entry->sublabel,
                     cbs->action_sublabel_cache, sizeof(entry->sublabel));
         else if (cbs->action_sublabel)
         {
            /* If this function callback returns true,
             * we know that the value won't change - so we
             * can cache it instead. */
            if (cbs->action_sublabel(list,
                     entry->type, (unsigned)i,
                     label, path,
                     entry->sublabel,
                     sizeof(entry->sublabel)) > 0)
               strlcpy(cbs->action_sublabel_cache,
                     entry->sublabel,
                     sizeof(cbs->action_sublabel_cache));
         }
      }
   }

   if (path_enabled)
   {
      if (!string_is_empty(path) && !use_representation)
         strlcpy(entry->path, path, sizeof(entry->path));
      else if (
                cbs
            &&  cbs->setting
            &&  cbs->setting->enum_value_idx != MSG_UNKNOWN
            && !cbs->setting->dont_use_enum_idx_representation)
         strlcpy(entry->path,
               msg_hash_to_str(cbs->setting->enum_value_idx),
               sizeof(entry->path));
      else
         if (!string_is_empty(newpath))
            strlcpy(entry->path, newpath, sizeof(entry->path));
   }
}

menu_file_list_cbs_t *menu_entries_get_last_stack_actiondata(void)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   if (menu_st->entries.list)
   {
      const file_list_t *list  = MENU_LIST_GET(menu_st->entries.list, 0);
      return (menu_file_list_cbs_t*)list->list[list->size - 1].actiondata;
   }
   return NULL;
}

file_list_t *menu_entries_get_menu_stack_ptr(size_t idx)
{
   struct menu_state   *menu_st   = menu_state_get_ptr();
   menu_list_t *menu_list         = menu_st->entries.list;
   if (!menu_list)
      return NULL;
   return MENU_LIST_GET(menu_list, (unsigned)idx);
}

file_list_t *menu_entries_get_selection_buf_ptr(size_t idx)
{
   struct menu_state   *menu_st   = menu_state_get_ptr();
   menu_list_t *menu_list         = menu_st->entries.list;
   if (!menu_list)
      return NULL;
   return MENU_LIST_GET_SELECTION(menu_list, (unsigned)idx);
}

size_t menu_entries_get_stack_size(size_t idx)
{
   struct menu_state    *menu_st  = menu_state_get_ptr();
   menu_list_t *menu_list         = menu_st->entries.list;
   if (!menu_list)
      return 0;
   return MENU_LIST_GET_STACK_SIZE(menu_list, idx);
}

size_t menu_entries_get_size(void)
{
   struct menu_state    *menu_st  = menu_state_get_ptr();
   menu_list_t *menu_list         = menu_st->entries.list;
   if (!menu_list)
      return 0;
   return MENU_LIST_GET_SELECTION(menu_list, 0)->size;
}

menu_search_terms_t *menu_entries_search_get_terms_internal(void)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   file_list_t *list           = MENU_LIST_GET(menu_st->entries.list, 0);
   menu_file_list_cbs_t *cbs   = NULL;

   if (!list ||
       (list->size < 1))
      return NULL;

   cbs = (menu_file_list_cbs_t*)list->list[list->size - 1].actiondata;

   if (!cbs)
      return NULL;

   return &cbs->search;
}

/* Searches current menu list for specified 'needle'
 * string. If string is found, returns true and sets
 * 'idx' to the matching list entry index. */
bool menu_entries_list_search(const char *needle, size_t *idx)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   menu_list_t *menu_list      = menu_st->entries.list;
   file_list_t *list           = MENU_LIST_GET_SELECTION(menu_list, (unsigned)0);
   bool match_found            = false;
   bool char_search            = false;
   char needle_char            = 0;
   size_t i;

   if (   !list
       || string_is_empty(needle)
       || !idx)
      return match_found;

   /* Check if we are searching for a single
    * Latin alphabet character */
   char_search    = ((needle[1] == '\0') && (ISALPHA(needle[0])));
   if (char_search)
      needle_char = TOLOWER(needle[0]);

   for (i = 0; i < list->size; i++)
   {
      const char *entry_label = NULL;
      menu_entry_t entry;

      /* Note the we have to get the actual menu
       * entry here, since we need the exact label
       * that is currently displayed by the menu
       * driver */
      MENU_ENTRY_INIT(entry);
      entry.value_enabled    = false;
      entry.sublabel_enabled = false;
      menu_entry_get(&entry, 0, i, NULL, true);

      /* When using the file browser, one or more
       * 'utility' entries will be added to the top
       * of the list (e.g. 'Parent Directory'). These
       * have no bearing on the actual content of the
       * list, and should be excluded from the search */
      if ((entry.type == FILE_TYPE_SCAN_DIRECTORY) ||
          (entry.type == FILE_TYPE_MANUAL_SCAN_DIRECTORY) ||
          (entry.type == FILE_TYPE_USE_DIRECTORY) ||
          (entry.type == FILE_TYPE_PARENT_DIRECTORY))
         continue;

      /* Get displayed entry label */
      if (!string_is_empty(entry.rich_label))
         entry_label = entry.rich_label;
      else
         entry_label = entry.path;

      if (string_is_empty(entry_label))
         continue;

      /* If we are performing a single character
       * search, jump to the first entry whose
       * first character matches */
      if (char_search)
      {
         if (needle_char == TOLOWER(entry_label[0]))
         {
            *idx        = i;
            match_found = true;
            break;
         }
      }
      /* Otherwise perform an exhaustive string
       * comparison */
      else
      {
         const char *found_str = (const char *)strcasestr(entry_label, needle);

         /* Found a match with the first characters
          * of the label -> best possible match,
          * so quit immediately */
         if (found_str == entry_label)
         {
            *idx        = i;
            match_found = true;
            break;
         }
         /* Found a mid-string match; this is a valid
          * result, but keep searching for the best
          * possible match */
         else if (found_str)
         {
            *idx        = i;
            match_found = true;
         }
      }
   }

   return match_found;
}

/* Time format strings with AM-PM designation require special
 * handling due to platform dependence */
static void strftime_am_pm(char* ptr, size_t maxsize, const char* format,
      const struct tm* timeptr)
{
   char *local = NULL;

   /* Ensure correct locale is set
    * > Required for localised AM/PM strings */
   setlocale(LC_TIME, "");

   strftime(ptr, maxsize, format, timeptr);
#if !(defined(__linux__) && !defined(ANDROID))
   local = local_to_utf8_string_alloc(ptr);

   if (!string_is_empty(local))
      strlcpy(ptr, local, maxsize);

   if (local)
   {
      free(local);
      local = NULL;
   }
#endif
}


/* Display the date and time - time_mode will influence how
 * the time representation will look like.
 * */
void menu_display_timedate(gfx_display_ctx_datetime_t *datetime)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   if (!datetime)
      return;

   /* Trigger an update, if required */
   if (menu_st->current_time_us - menu_st->datetime_last_time_us >=
         DATETIME_CHECK_INTERVAL)
   {
      time_t time_;
      struct tm tm_;
      bool has_am_pm         = false;
      const char *format_str = "";

      menu_st->datetime_last_time_us = menu_st->current_time_us;

      /* Get current time */
      time(&time_);
      rtime_localtime(&time_, &tm_);

      /* Format string representation */
      switch (datetime->time_mode)
      {
         case MENU_TIMEDATE_STYLE_YMD_HMS: /* YYYY-MM-DD HH:MM:SS */
            /* Using switch statements to set the format
             * string is verbose, but has far less performance
             * impact than setting the date separator dynamically
             * (i.e. no snprintf() or character replacement...) */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d %H:%M:%S";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d %H:%M:%S";
                  break;
               default:
                  format_str = "%Y-%m-%d %H:%M:%S";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_YMD_HM: /* YYYY-MM-DD HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d %H:%M";
                  break;
               default:
                  format_str = "%Y-%m-%d %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_YMD: /* YYYY-MM-DD */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d";
                  break;
               default:
                  format_str = "%Y-%m-%d";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_YM: /* YYYY-MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m";
                  break;
               default:
                  format_str = "%Y-%m";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HMS: /* MM-DD-YYYY HH:MM:SS */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y %H:%M:%S";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y %H:%M:%S";
                  break;
               default:
                  format_str = "%m-%d-%Y %H:%M:%S";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HM: /* MM-DD-YYYY HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y %H:%M";
                  break;
               default:
                  format_str = "%m-%d-%Y %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MD_HM: /* MM-DD HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d %H:%M";
                  break;
               default:
                  format_str = "%m-%d %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY: /* MM-DD-YYYY */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y";
                  break;
               default:
                  format_str = "%m-%d-%Y";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MD: /* MM-DD */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d";
                  break;
               default:
                  format_str = "%m-%d";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HMS: /* DD-MM-YYYY HH:MM:SS */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y %H:%M:%S";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y %H:%M:%S";
                  break;
               default:
                  format_str = "%d-%m-%Y %H:%M:%S";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HM: /* DD-MM-YYYY HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y %H:%M";
                  break;
               default:
                  format_str = "%d-%m-%Y %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMM_HM: /* DD-MM HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m %H:%M";
                  break;
               default:
                  format_str = "%d-%m %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY: /* DD-MM-YYYY */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y";
                  break;
               default:
                  format_str = "%d-%m-%Y";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMM: /* DD-MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m";
                  break;
               default:
                  format_str = "%d-%m";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_HMS: /* HH:MM:SS */
            format_str = "%H:%M:%S";
            break;
         case MENU_TIMEDATE_STYLE_HM: /* HH:MM */
            format_str = "%H:%M";
            break;
         case MENU_TIMEDATE_STYLE_YMD_HMS_AMPM: /* YYYY-MM-DD HH:MM:SS (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d %I:%M:%S %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d %I:%M:%S %p";
                  break;
               default:
                  format_str = "%Y-%m-%d %I:%M:%S %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_YMD_HM_AMPM: /* YYYY-MM-DD HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d %I:%M %p";
                  break;
               default:
                  format_str = "%Y-%m-%d %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HMS_AMPM: /* MM-DD-YYYY HH:MM:SS (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y %I:%M:%S %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y %I:%M:%S %p";
                  break;
               default:
                  format_str = "%m-%d-%Y %I:%M:%S %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HM_AMPM: /* MM-DD-YYYY HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y %I:%M %p";
                  break;
               default:
                  format_str = "%m-%d-%Y %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MD_HM_AMPM: /* MM-DD HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d %I:%M %p";
                  break;
               default:
                  format_str = "%m-%d %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HMS_AMPM: /* DD-MM-YYYY HH:MM:SS (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y %I:%M:%S %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y %I:%M:%S %p";
                  break;
               default:
                  format_str = "%d-%m-%Y %I:%M:%S %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HM_AMPM: /* DD-MM-YYYY HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y %I:%M %p";
                  break;
               default:
                  format_str = "%d-%m-%Y %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMM_HM_AMPM: /* DD-MM HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m %I:%M %p";
                  break;
               default:
                  format_str = "%d-%m %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_HMS_AMPM: /* HH:MM:SS (AM/PM) */
            has_am_pm  = true;
            format_str = "%I:%M:%S %p";
            break;
         case MENU_TIMEDATE_STYLE_HM_AMPM: /* HH:MM (AM/PM) */
            has_am_pm  = true;
            format_str = "%I:%M %p";
            break;
      }

      if (has_am_pm)
         strftime_am_pm(menu_st->datetime_cache, sizeof(menu_st->datetime_cache),
               format_str, &tm_);
      else
         strftime(menu_st->datetime_cache, sizeof(menu_st->datetime_cache),
               format_str, &tm_);
   }

   /* Copy cached datetime string to input
    * menu_display_ctx_datetime_t struct */
   strlcpy(datetime->s, menu_st->datetime_cache, datetime->len);
}

/* Display current (battery) power state */
void menu_display_powerstate(gfx_display_ctx_powerstate_t *powerstate)
{
   int percent                    = 0;
   struct menu_state    *menu_st  = menu_state_get_ptr();
   enum frontend_powerstate state = FRONTEND_POWERSTATE_NONE;

   if (!powerstate)
      return;

   /* Trigger an update, if required */
   if (menu_st->current_time_us - menu_st->powerstate_last_time_us >=
         POWERSTATE_CHECK_INTERVAL)
   {
      menu_st->powerstate_last_time_us = menu_st->current_time_us;
      task_push_get_powerstate();
   }

   /* Get last recorded state */
   state                       = get_last_powerstate(&percent);

   /* Populate gfx_display_ctx_powerstate_t */
   powerstate->battery_enabled = (state != FRONTEND_POWERSTATE_NONE) &&
                                 (state != FRONTEND_POWERSTATE_NO_SOURCE);
   powerstate->percent         = 0;
   powerstate->charging        = false;

   if (powerstate->battery_enabled)
   {
      if (state == FRONTEND_POWERSTATE_CHARGING)
         powerstate->charging  = true;
      if (percent > 0)
         powerstate->percent   = (unsigned)percent;
      snprintf(powerstate->s, powerstate->len, "%u%%", powerstate->percent);
   }
}


/* Sets title to what the name of the current menu should be. */
int menu_entries_get_title(char *s, size_t len)
{
   unsigned menu_type            = 0;
   const char *path              = NULL;
   const char *label             = NULL;
   struct menu_state   *menu_st  = menu_state_get_ptr();
   const file_list_t *list       = menu_st->entries.list ?
      MENU_LIST_GET(menu_st->entries.list, 0) : NULL;
   menu_file_list_cbs_t *cbs     = list
      ? (menu_file_list_cbs_t*)list->list[list->size - 1].actiondata
      : NULL;

   if (!cbs)
      return -1;

   if (cbs && cbs->action_get_title)
   {
      int ret;
      if (!string_is_empty(cbs->action_title_cache))
      {
         strlcpy(s, cbs->action_title_cache, len);
         return 0;
      }
      file_list_get_last(MENU_LIST_GET(menu_st->entries.list, 0),
            &path, &label, &menu_type, NULL);
      ret = cbs->action_get_title(path, label, menu_type, s, len);
      if (ret == 1)
         strlcpy(cbs->action_title_cache, s, sizeof(cbs->action_title_cache));
      return ret;
   }
   return 0;
}

/* Used to close an active message box (help or info)
 * TODO/FIXME: The way that message boxes are handled
 * is complete garbage. generic_menu_iterate() and
 * message boxes in general need a total rewrite.
 * I consider this current 'close_messagebox' a hack,
 * but at least it prevents undefined/dangerous
 * behaviour... */
void menu_input_pointer_close_messagebox(struct menu_state *menu_st)
{
   const char *label            = NULL;

   /* Determine whether this is a help or info
    * message box */
   file_list_get_last(MENU_LIST_GET(menu_st->entries.list, 0),
         NULL, &label, NULL, NULL);

   /* Pop stack, if required */
   if (menu_should_pop_stack(label))
   {
      size_t selection            = menu_st->selection_ptr;
      size_t new_selection        = selection;

      menu_entries_pop_stack(&new_selection, 0, 0);
      menu_st->selection_ptr      = selection;
   }
}


float menu_input_get_dpi(
      menu_handle_t *menu,
      gfx_display_t *p_disp,
      unsigned video_width,
      unsigned video_height)
{
   static unsigned last_video_width  = 0;
   static unsigned last_video_height = 0;
   static float dpi                  = 0.0f;
   static bool dpi_cached            = false;

   /* Regardless of menu driver, need 'actual' screen DPI
    * Note: DPI is a fixed hardware property. To minimise performance
    * overheads we therefore only call video_context_driver_get_metrics()
    * on first run, or when the current video resolution changes */
   if (!dpi_cached ||
       (video_width  != last_video_width) ||
       (video_height != last_video_height))
   {
      gfx_ctx_metrics_t mets;
      /* Note: If video_context_driver_get_metrics() fails,
       * we don't know what happened to dpi - so ensure it
       * is reset to a sane value */

      mets.type  = DISPLAY_METRIC_DPI;
      mets.value = &dpi;
      if (!video_context_driver_get_metrics(&mets))
         dpi            = 0.0f;

      dpi_cached        = true;
      last_video_width  = video_width;
      last_video_height = video_height;
   }

   /* RGUI uses a framebuffer texture, which means we
    * operate in menu space, not screen space.
    * DPI in a traditional sense is therefore meaningless,
    * so generate a substitute value based upon framebuffer
    * dimensions */
   if (dpi > 0.0f)
   {
      bool menu_has_fb =
            menu->driver_ctx
         && menu->driver_ctx->set_texture;

      /* Read framebuffer info? */
      if (menu_has_fb)
      {
         unsigned fb_height         = p_disp->framebuf_height;
         /* Rationale for current 'DPI' determination method:
          * - Divide screen height by DPI, to get number of vertical
          *   '1 inch' squares
          * - Divide RGUI framebuffer height by number of vertical
          *   '1 inch' squares to get number of menu space pixels
          *   per inch
          * This is crude, but should be sufficient... */
         return ((float)fb_height / (float)video_height) * dpi;
      }
   }

   return dpi;
}

bool input_event_osk_show_symbol_pages(
      menu_handle_t *menu)
{
#if defined(HAVE_LANGEXTRA)
#if defined(HAVE_RGUI)
   bool menu_has_fb      = (menu &&
         menu->driver_ctx &&
         menu->driver_ctx->set_texture);
   unsigned language     = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
   return !menu_has_fb ||
         ((language == RETRO_LANGUAGE_JAPANESE) ||
          (language == RETRO_LANGUAGE_KOREAN) ||
          (language == RETRO_LANGUAGE_CHINESE_SIMPLIFIED) ||
          (language == RETRO_LANGUAGE_CHINESE_TRADITIONAL));
#else  /* HAVE_RGUI */
   return true;
#endif /* HAVE_RGUI */
#else  /* HAVE_LANGEXTRA */
   return false;
#endif /* HAVE_LANGEXTRA */
}

static void menu_driver_list_free(
      const menu_ctx_driver_t *menu_driver_ctx,
      menu_ctx_list_t *list)
{
   if (menu_driver_ctx)
      if (menu_driver_ctx->list_free)
         menu_driver_ctx->list_free(
               list->list, list->idx, list->list_size);

   if (list->list)
   {
      file_list_free_userdata  (list->list, list->idx);
      file_list_free_actiondata(list->list, list->idx);
   }
}

static void menu_list_free_list(
      const menu_ctx_driver_t *menu_driver_ctx,
      file_list_t *list)
{
   unsigned i;

   for (i = 0; i < list->size; i++)
   {
      menu_ctx_list_t list_info;

      list_info.list      = list;
      list_info.idx       = i;
      list_info.list_size = list->size;

      menu_driver_list_free(menu_driver_ctx, &list_info);
   }

   file_list_free(list);
}

bool menu_list_pop_stack(
      const menu_ctx_driver_t *menu_driver_ctx,
      void *menu_userdata,
      menu_list_t *list,
      size_t idx,
      size_t *directory_ptr)
{
   file_list_t *menu_list = MENU_LIST_GET(list, (unsigned)idx);

   if (!menu_list)
      return false;

   if (menu_list->size != 0)
   {
      menu_ctx_list_t list_info;

      list_info.list      = menu_list;
      list_info.idx       = menu_list->size - 1;
      list_info.list_size = menu_list->size - 1;

      menu_driver_list_free(menu_driver_ctx, &list_info);
   }

   file_list_pop(menu_list, directory_ptr);
   if (  menu_driver_ctx &&
         menu_driver_ctx->list_set_selection)
      menu_driver_ctx->list_set_selection(menu_userdata,
            menu_list);

   return true;
}

static int menu_list_flush_stack_type(const char *needle, const char *label,
      unsigned type, unsigned final_type)
{
   return needle ? !string_is_equal(needle, label) : (type != final_type);
}

void menu_list_flush_stack(
      const menu_ctx_driver_t *menu_driver_ctx,
      void *menu_userdata,
      struct menu_state *menu_st,
      menu_list_t *list,
      size_t idx, const char *needle, unsigned final_type)
{
   bool refresh                = false;
   const char *path            = NULL;
   const char *label           = NULL;
   unsigned type               = 0;
   size_t entry_idx            = 0;
   file_list_t *menu_list      = MENU_LIST_GET(list, (unsigned)idx);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

   if (menu_list && menu_list->size)
      file_list_get_at_offset(menu_list, menu_list->size - 1, &path, &label, &type, &entry_idx);

   while (menu_list_flush_stack_type(
            needle, label, type, final_type) != 0)
   {
      bool refresh             = false;
      size_t new_selection_ptr = menu_st->selection_ptr;
      bool wont_pop_stack      = (MENU_LIST_GET_STACK_SIZE(list, idx) <= 1);
      if (wont_pop_stack)
         break;

      if (menu_driver_ctx->list_cache)
         menu_driver_ctx->list_cache(menu_userdata,
               MENU_LIST_PLAIN, 0);

      menu_list_pop_stack(menu_driver_ctx,
            menu_userdata,
            list, idx, &new_selection_ptr);

      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

      menu_st->selection_ptr   = new_selection_ptr;
      menu_list                = MENU_LIST_GET(list, (unsigned)idx);

      if (menu_list && menu_list->size)
         file_list_get_at_offset(menu_list, menu_list->size - 1, &path, &label, &type, &entry_idx);
   }
}

static void menu_list_free(
      const menu_ctx_driver_t *menu_driver_ctx,
      menu_list_t *menu_list)
{
   if (!menu_list)
      return;

   if (menu_list->menu_stack)
   {
      unsigned i;

      for (i = 0; i < menu_list->menu_stack_size; i++)
      {
         if (!menu_list->menu_stack[i])
            continue;

         menu_list_free_list(menu_driver_ctx,
               menu_list->menu_stack[i]);
         menu_list->menu_stack[i]    = NULL;
      }

      free(menu_list->menu_stack);
   }

   if (menu_list->selection_buf)
   {
      unsigned i;

      for (i = 0; i < menu_list->selection_buf_size; i++)
      {
         if (!menu_list->selection_buf[i])
            continue;

         menu_list_free_list(menu_driver_ctx,
               menu_list->selection_buf[i]);
         menu_list->selection_buf[i] = NULL;
      }

      free(menu_list->selection_buf);
   }

   free(menu_list);
}

static menu_list_t *menu_list_new(const menu_ctx_driver_t *menu_driver_ctx)
{
   unsigned i;
   menu_list_t           *list = (menu_list_t*)malloc(sizeof(*list));

   if (!list)
      return NULL;

   list->menu_stack_size       = 1;
   list->selection_buf_size    = 1;
   list->selection_buf         = NULL;
   list->menu_stack            = (file_list_t**)
      calloc(list->menu_stack_size, sizeof(*list->menu_stack));

   if (!list->menu_stack)
      goto error;

   list->selection_buf         = (file_list_t**)
      calloc(list->selection_buf_size, sizeof(*list->selection_buf));

   if (!list->selection_buf)
      goto error;

   for (i = 0; i < list->menu_stack_size; i++)
   {
      list->menu_stack[i]           = (file_list_t*)
         malloc(sizeof(*list->menu_stack[i]));
      list->menu_stack[i]->list     = NULL;
      list->menu_stack[i]->capacity = 0;
      list->menu_stack[i]->size     = 0;
   }

   for (i = 0; i < list->selection_buf_size; i++)
   {
      list->selection_buf[i]           = (file_list_t*)
         malloc(sizeof(*list->selection_buf[i]));
      list->selection_buf[i]->list     = NULL;
      list->selection_buf[i]->capacity = 0;
      list->selection_buf[i]->size     = 0;
   }

   return list;

error:
   menu_list_free(menu_driver_ctx, list);
   return NULL;
}


int menu_input_key_bind_set_mode_common(
      struct menu_state *menu_st,
      struct menu_bind_state *binds,
      enum menu_input_binds_ctl_state state,
      rarch_setting_t  *setting,
      settings_t *settings)
{
   menu_displaylist_info_t info;
   unsigned bind_type             = 0;
   struct retro_keybind *keybind  = NULL;
   unsigned         index_offset  = setting->index_offset;
   menu_list_t *menu_list         = menu_st->entries.list;
   file_list_t *menu_stack        = menu_list ? MENU_LIST_GET(menu_list, (unsigned)0) : NULL;
   size_t selection               = menu_st->selection_ptr;

   menu_displaylist_info_init(&info);

   switch (state)
   {
      case MENU_INPUT_BINDS_CTL_BIND_SINGLE:
         keybind                  = (struct retro_keybind*)setting->value.target.keybind;

         if (!keybind)
            return -1;

         bind_type                = setting_get_bind_type(setting);

         binds->begin             = bind_type;
         binds->last              = bind_type;
         binds->output            = keybind;
         binds->buffer            = *(binds->output);
         binds->user              = index_offset;

         info.list                = menu_stack;
         info.type                = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info.directory_ptr       = selection;
         info.enum_idx            = MENU_ENUM_LABEL_CUSTOM_BIND;
         info.label               = strdup(
               msg_hash_to_str(MENU_ENUM_LABEL_CUSTOM_BIND));
         break;
      case MENU_INPUT_BINDS_CTL_BIND_ALL:
         binds->output            = &input_config_binds[index_offset][0];
         binds->buffer            = *(binds->output);
         binds->begin             = MENU_SETTINGS_BIND_BEGIN;
         binds->last              = MENU_SETTINGS_BIND_LAST;

         info.list                = menu_stack;
         info.type                = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info.directory_ptr       = selection;
         info.enum_idx            = MENU_ENUM_LABEL_CUSTOM_BIND_ALL;
         info.label               = strdup(
               msg_hash_to_str(MENU_ENUM_LABEL_CUSTOM_BIND_ALL));
         break;
      default:
      case MENU_INPUT_BINDS_CTL_BIND_NONE:
         return 0;
   }

   if (menu_displaylist_ctl(DISPLAYLIST_INFO, &info, settings))
      menu_displaylist_process(&info);
   menu_displaylist_info_free(&info);

   return 0;
}

#ifdef ANDROID
bool menu_input_key_bind_poll_find_hold_pad(
      struct menu_bind_state *new_state,
      struct retro_keybind * output,
      unsigned p)
{
   unsigned a, b, h;
   const struct menu_bind_state_port *n =
      (const struct menu_bind_state_port*)
      &new_state->state[p];

   for (b = 0; b < MENU_MAX_MBUTTONS; b++)
   {
      bool iterate = n->mouse_buttons[b];

      if (!iterate)
         continue;

      switch (b)
      {
         case RETRO_DEVICE_ID_MOUSE_LEFT:
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            output->mbutton = b;
            return true;
      }
   }

   for (b = 0; b < MENU_MAX_BUTTONS; b++)
   {
      bool iterate = n->buttons[b];

      if (!iterate)
         continue;

      output->joykey = b;
      output->joyaxis = AXIS_NONE;
      return true;
   }

   /* Axes are a bit tricky ... */
   for (a = 0; a < MENU_MAX_AXES; a++)
   {
      if (abs(n->axes[a]) >= 20000)
      {
         /* Take care of case where axis rests on +/- 0x7fff
          * (e.g. 360 controller on Linux) */
         output->joyaxis = n->axes[a] > 0
            ? AXIS_POS(a) : AXIS_NEG(a);
         output->joykey = NO_BTN;

         return true;
      }
   }

   for (h = 0; h < MENU_MAX_HATS; h++)
   {
      uint16_t      trigged = n->hats[h];
      uint16_t sane_trigger = 0;

      if (trigged & HAT_UP_MASK)
         sane_trigger = HAT_UP_MASK;
      else if (trigged & HAT_DOWN_MASK)
         sane_trigger = HAT_DOWN_MASK;
      else if (trigged & HAT_LEFT_MASK)
         sane_trigger = HAT_LEFT_MASK;
      else if (trigged & HAT_RIGHT_MASK)
         sane_trigger = HAT_RIGHT_MASK;

      if (sane_trigger)
      {
         output->joykey  = HAT_MAP(h, sane_trigger);
         output->joyaxis = AXIS_NONE;
         return true;
      }
   }

   return false;
}

bool menu_input_key_bind_poll_find_hold(
      unsigned max_users,
      struct menu_bind_state *new_state,
      struct retro_keybind * output)
{
   if (new_state)
   {
      unsigned i;

      for (i = 0; i < max_users; i++)
      {
         if (menu_input_key_bind_poll_find_hold_pad(new_state, output, i))
            return true;
      }
   }

   return false;
}
#endif

bool menu_input_key_bind_poll_find_trigger_pad(
      struct menu_bind_state *state,
      struct menu_bind_state *new_state,
      struct retro_keybind * output,
      unsigned p)
{
   unsigned a, b, h;
   const struct menu_bind_state_port *n = (const struct menu_bind_state_port*)
      &new_state->state[p];
   const struct menu_bind_state_port *o = (const struct menu_bind_state_port*)
      &state->state[p];

   for (b = 0; b < MENU_MAX_MBUTTONS; b++)
   {
      bool iterate = n->mouse_buttons[b] && !o->mouse_buttons[b];

      if (!iterate)
         continue;

      switch (b)
      {
         case RETRO_DEVICE_ID_MOUSE_LEFT:
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            output->mbutton = b;
            return true;
      }
   }

   for (b = 0; b < MENU_MAX_BUTTONS; b++)
   {
      bool iterate = n->buttons[b] && !o->buttons[b];

      if (!iterate)
         continue;

      output->joykey = b;
      output->joyaxis = AXIS_NONE;
      return true;
   }

   /* Axes are a bit tricky ... */
   for (a = 0; a < MENU_MAX_AXES; a++)
   {
      int locked_distance = abs(n->axes[a] -
            new_state->axis_state[p].locked_axes[a]);
      int rested_distance = abs(n->axes[a] -
            new_state->axis_state[p].rested_axes[a]);

      if (abs(n->axes[a]) >= 20000 &&
            locked_distance >= 20000 &&
            rested_distance >= 20000)
      {
         /* Take care of case where axis rests on +/- 0x7fff
          * (e.g. 360 controller on Linux) */
         output->joyaxis = n->axes[a] > 0
            ? AXIS_POS(a) : AXIS_NEG(a);
         output->joykey = NO_BTN;

         /* Lock the current axis */
         new_state->axis_state[p].locked_axes[a] =
            n->axes[a] > 0 ?
            0x7fff : -0x7fff;
         return true;
      }

      if (locked_distance >= 20000) /* Unlock the axis. */
         new_state->axis_state[p].locked_axes[a] = 0;
   }

   for (h = 0; h < MENU_MAX_HATS; h++)
   {
      uint16_t      trigged = n->hats[h] & (~o->hats[h]);
      uint16_t sane_trigger = 0;

      if (trigged & HAT_UP_MASK)
         sane_trigger = HAT_UP_MASK;
      else if (trigged & HAT_DOWN_MASK)
         sane_trigger = HAT_DOWN_MASK;
      else if (trigged & HAT_LEFT_MASK)
         sane_trigger = HAT_LEFT_MASK;
      else if (trigged & HAT_RIGHT_MASK)
         sane_trigger = HAT_RIGHT_MASK;

      if (sane_trigger)
      {
         output->joykey = HAT_MAP(h, sane_trigger);
         output->joyaxis = AXIS_NONE;
         return true;
      }
   }

   return false;
}

bool menu_input_key_bind_poll_find_trigger(
      unsigned max_users,
      struct menu_bind_state *state,
      struct menu_bind_state *new_state,
      struct retro_keybind * output)
{
   if (state && new_state)
   {
      unsigned i;

      for (i = 0; i < max_users; i++)
      {
         if (menu_input_key_bind_poll_find_trigger_pad(
                  state, new_state, output, i))
            return true;
      }
   }

   return false;
}


void menu_input_key_bind_poll_bind_get_rested_axes(
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      struct menu_bind_state *state)
{
   unsigned a;
   unsigned port                           = state->port;

   if (joypad)
   {
      /* poll only the relevant port */
      for (a = 0; a < MENU_MAX_AXES; a++)
      {
         if (AXIS_POS(a) != AXIS_NONE)
            state->axis_state[port].rested_axes[a]  =
               joypad->axis(port, AXIS_POS(a));
         if (AXIS_NEG(a) != AXIS_NONE)
            state->axis_state[port].rested_axes[a] +=
               joypad->axis(port, AXIS_NEG(a));
      }
   }

   if (sec_joypad)
   {
      /* poll only the relevant port */
      for (a = 0; a < MENU_MAX_AXES; a++)
      {
         if (AXIS_POS(a) != AXIS_NONE)
            state->axis_state[port].rested_axes[a]  = sec_joypad->axis(port, AXIS_POS(a));

         if (AXIS_NEG(a) != AXIS_NONE)
            state->axis_state[port].rested_axes[a] += sec_joypad->axis(port, AXIS_NEG(a));
      }
   }
}

void input_event_osk_iterate(
      void *osk_grid,
      enum osk_type osk_idx)
{
#ifndef HAVE_LANGEXTRA
   /* If HAVE_LANGEXTRA is not defined, define some ASCII-friendly pages. */
   static const char *uppercase_grid[] = {
      "1","2","3","4","5","6","7","8","9","0","Bksp",
      "Q","W","E","R","T","Y","U","I","O","P","Enter",
      "A","S","D","F","G","H","J","K","L","+","Lower",
      "Z","X","C","V","B","N","M"," ","_","/","Next"};
   static const char *lowercase_grid[] = {
      "1","2","3","4","5","6","7","8","9","0","Bksp",
      "q","w","e","r","t","y","u","i","o","p","Enter",
      "a","s","d","f","g","h","j","k","l","@","Upper",
      "z","x","c","v","b","n","m"," ","-",".","Next"};
   static const char *symbols_page1_grid[] = {
      "1","2","3","4","5","6","7","8","9","0","Bksp",
      "!","\"","#","$","%","&","'","*","(",")","Enter",
      "+",",","-","~","/",":",";","=","<",">","Lower",
      "?","@","[","\\","]","^","_","|","{","}","Next"};
#endif
   switch (osk_idx)
   {
#ifdef HAVE_LANGEXTRA
      case OSK_HIRAGANA_PAGE1:
         memcpy(osk_grid,
               hiragana_page1_grid,
               sizeof(hiragana_page1_grid));
         break;
      case OSK_HIRAGANA_PAGE2:
         memcpy(osk_grid,
               hiragana_page2_grid,
               sizeof(hiragana_page2_grid));
         break;
      case OSK_KATAKANA_PAGE1:
         memcpy(osk_grid,
               katakana_page1_grid,
               sizeof(katakana_page1_grid));
         break;
      case OSK_KATAKANA_PAGE2:
         memcpy(osk_grid,
               katakana_page2_grid,
               sizeof(katakana_page2_grid));
         break;
#endif
      case OSK_SYMBOLS_PAGE1:
         memcpy(osk_grid,
               symbols_page1_grid,
               sizeof(uppercase_grid));
         break;
      case OSK_UPPERCASE_LATIN:
         memcpy(osk_grid,
               uppercase_grid,
               sizeof(uppercase_grid));
         break;
      case OSK_LOWERCASE_LATIN:
      default:
         memcpy(osk_grid,
               lowercase_grid,
               sizeof(lowercase_grid));
         break;
   }
}

void menu_input_get_mouse_hw_state(
      gfx_display_t *p_disp,
      menu_handle_t *menu,
      input_driver_state_t *input_driver_st,
      input_driver_t *current_input,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      bool keyboard_mapping_blocked,
      bool menu_mouse_enable,
      bool input_overlay_enable,
      bool overlay_active,
      menu_input_pointer_hw_state_t *hw_state)
{
   rarch_joypad_info_t joypad_info;
   static int16_t last_x           = 0;
   static int16_t last_y           = 0;
   static bool last_select_pressed = false;
   static bool last_cancel_pressed = false;
   bool menu_has_fb                =
      (menu &&
       menu->driver_ctx &&
       menu->driver_ctx->set_texture);
   bool state_inited               = current_input &&
      current_input->input_state;
#ifdef HAVE_OVERLAY
   /* Menu pointer controls are ignored when overlays are enabled. */
   if (overlay_active)
      menu_mouse_enable            = false;
#endif

   /* Easiest to set inactive by default, and toggle
    * when input is detected */
   hw_state->active                = false;
   hw_state->x                     = 0;
   hw_state->y                     = 0;
   hw_state->select_pressed        = false;
   hw_state->cancel_pressed        = false;
   hw_state->up_pressed            = false;
   hw_state->down_pressed          = false;
   hw_state->left_pressed          = false;
   hw_state->right_pressed         = false;

   if (!menu_mouse_enable)
      return;

   joypad_info.joy_idx             = 0;
   joypad_info.auto_binds          = NULL;
   joypad_info.axis_threshold      = 0.0f;

   /* X/Y position */
   if (state_inited)
   {
      if ((hw_state->x = current_input->input_state(
                  input_driver_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RARCH_DEVICE_MOUSE_SCREEN,
                  0,
                  RETRO_DEVICE_ID_MOUSE_X)) != last_x)
         hw_state->active          = true;
      if ((hw_state->y = current_input->input_state(
                  input_driver_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RARCH_DEVICE_MOUSE_SCREEN,
                  0,
                  RETRO_DEVICE_ID_MOUSE_Y)) != last_y)
         hw_state->active          = true;
   }

   last_x                          = hw_state->x;
   last_y                          = hw_state->y;

   /* > X/Y position adjustment */
   if (menu_has_fb)
   {
      /* RGUI uses a framebuffer texture + custom viewports,
       * which means we have to convert from screen space to
       * menu space... */
      struct video_viewport vp     = {0};
      /* Read display/framebuffer info */
      unsigned fb_width            = p_disp->framebuf_width;
      unsigned fb_height           = p_disp->framebuf_height;

      video_driver_get_viewport_info(&vp);

      /* Adjust X position */
      hw_state->x                  = (int16_t)(((float)(hw_state->x - vp.x) / (float)vp.width) * (float)fb_width);
      hw_state->x                  = (hw_state->x <  0)         ? (0          ) : hw_state->x;
      hw_state->x                  = (hw_state->x >= fb_width)  ? (fb_width -1) : hw_state->x;

      /* Adjust Y position */
      hw_state->y                  = (int16_t)(((float)(hw_state->y - vp.y) / (float)vp.height) * (float)fb_height);
      hw_state->y                  = (hw_state->y <  0)         ? (0          ) : hw_state->y;
      hw_state->y                  = (hw_state->y >= fb_height) ? (fb_height-1) : hw_state->y;
   }

   if (state_inited)
   {
      /* Select (LMB)
       * Note that releasing select also counts as activity */
      hw_state->select_pressed = (bool)
         current_input->input_state(
               input_driver_st->current_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               RETRO_DEVICE_MOUSE,
               0,
               RETRO_DEVICE_ID_MOUSE_LEFT);
      /* Cancel (RMB)
       * Note that releasing cancel also counts as activity */
      hw_state->cancel_pressed = (bool)
         current_input->input_state(
               input_driver_st->current_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               RETRO_DEVICE_MOUSE,
               0,
               RETRO_DEVICE_ID_MOUSE_RIGHT);
      /* Up (mouse wheel up) */
      if ((hw_state->up_pressed = (bool)
               current_input->input_state(
                  input_driver_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RETRO_DEVICE_MOUSE,
                  0,
                  RETRO_DEVICE_ID_MOUSE_WHEELUP)))
         hw_state->active          = true;
      /* Down (mouse wheel down) */
      if ((hw_state->down_pressed = (bool)
               current_input->input_state(
                  input_driver_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RETRO_DEVICE_MOUSE,
                  0,
                  RETRO_DEVICE_ID_MOUSE_WHEELDOWN)))
         hw_state->active          = true;
      /* Left (mouse wheel horizontal left) */
      if ((hw_state->left_pressed = (bool)
               current_input->input_state(
                  input_driver_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RETRO_DEVICE_MOUSE,
                  0,
                  RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN)))
         hw_state->active          = true;
      /* Right (mouse wheel horizontal right) */
      if ((hw_state->right_pressed = (bool)
               current_input->input_state(
                  input_driver_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RETRO_DEVICE_MOUSE,
                  0,
                  RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP)))
         hw_state->active          = true;
   }

   if (hw_state->select_pressed || (hw_state->select_pressed != last_select_pressed))
      hw_state->active             = true;
   if (hw_state->cancel_pressed || (hw_state->cancel_pressed != last_cancel_pressed))
      hw_state->active             = true;
   last_select_pressed             = hw_state->select_pressed;
   last_cancel_pressed             = hw_state->cancel_pressed;
}

void menu_input_get_touchscreen_hw_state(
      gfx_display_t *p_disp,
      menu_handle_t *menu,
      input_driver_state_t *input_driver_st,
      input_driver_t *current_input,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      bool keyboard_mapping_blocked,
      bool overlay_active,
      bool pointer_enabled,
      unsigned input_touch_scale,
      menu_input_pointer_hw_state_t *hw_state)
{
   rarch_joypad_info_t joypad_info;
   unsigned fb_width, fb_height;
   int pointer_x                                = 0;
   int pointer_y                                = 0;
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   /* Is a background texture set for the current menu driver?
    * Checks if the menu framebuffer is set.
    * This would usually only return true
    * for framebuffer-based menu drivers, like RGUI. */
   int pointer_device                           =
         (menu && menu->driver_ctx && menu->driver_ctx->set_texture) ?
               RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;
   static int16_t last_x                        = 0;
   static int16_t last_y                        = 0;
   static bool last_select_pressed              = false;
   static bool last_cancel_pressed              = false;

   /* Easiest to set inactive by default, and toggle
    * when input is detected */
   hw_state->active                             = false;

   /* Touch screens don't have mouse wheels, so these
    * are always disabled */
   hw_state->up_pressed                         = false;
   hw_state->down_pressed                       = false;
   hw_state->left_pressed                       = false;
   hw_state->right_pressed                      = false;

#ifdef HAVE_OVERLAY
   /* Menu pointer controls are ignored when overlays are enabled. */
   if (overlay_active)
      pointer_enabled                           = false;
#endif

   /* If touchscreen is disabled, ignore all input */
   if (!pointer_enabled)
   {
      hw_state->x                               = 0;
      hw_state->y                               = 0;
      hw_state->select_pressed                  = false;
      hw_state->cancel_pressed                  = false;
      return;
   }

   /* TODO/FIXME - this should only be used for framebuffer-based
    * menu drivers like RGUI. Touchscreen input as a whole should
    * NOT be dependent on this
    */
   fb_width                                     = p_disp->framebuf_width;
   fb_height                                    = p_disp->framebuf_height;

   joypad_info.joy_idx                          = 0;
   joypad_info.auto_binds                       = NULL;
   joypad_info.axis_threshold                   = 0.0f;

   /* X pos */
   if (current_input->input_state)
      pointer_x                  = current_input->input_state(
            input_driver_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info, binds,
            keyboard_mapping_blocked,
            0, pointer_device,
            0, RETRO_DEVICE_ID_POINTER_X);
   hw_state->x  = ((pointer_x + 0x7fff) * (int)fb_width) / 0xFFFF;
   hw_state->x *= input_touch_scale;

   /* > An annoyance - we get different starting positions
    *   depending upon whether pointer_device is
    *   RETRO_DEVICE_POINTER or RARCH_DEVICE_POINTER_SCREEN,
    *   so different 'activity' checks are required to prevent
    *   false positives on first run */
   if (pointer_device == RARCH_DEVICE_POINTER_SCREEN)
   {
      if (hw_state->x != last_x)
         hw_state->active = true;
      last_x = hw_state->x;
   }
   else
   {
      if (pointer_x != last_x)
         hw_state->active = true;
      last_x = pointer_x;
   }

   /* Y pos */
   if (current_input->input_state)
      pointer_y = current_input->input_state(
            input_driver_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info, binds,
            keyboard_mapping_blocked,
            0, pointer_device,
            0, RETRO_DEVICE_ID_POINTER_Y);
   hw_state->y  = ((pointer_y + 0x7fff) * (int)fb_height) / 0xFFFF;
   hw_state->y *= input_touch_scale;

   if (pointer_device == RARCH_DEVICE_POINTER_SCREEN)
   {
      if (hw_state->y != last_y)
         hw_state->active = true;
      last_y = hw_state->y;
   }
   else
   {
      if (pointer_y != last_y)
         hw_state->active = true;
      last_y = pointer_y;
   }

   /* Select (touch screen contact)
    * Note that releasing select also counts as activity */
   if (current_input->input_state)
      hw_state->select_pressed = (bool)current_input->input_state(
            input_driver_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info, binds,
            keyboard_mapping_blocked,
            0, pointer_device,
            0, RETRO_DEVICE_ID_POINTER_PRESSED);
   if (hw_state->select_pressed || (hw_state->select_pressed != last_select_pressed))
      hw_state->active = true;
   last_select_pressed = hw_state->select_pressed;

   /* Cancel (touch screen 'back' - don't know what is this, but whatever...)
    * Note that releasing cancel also counts as activity */
   if (current_input->input_state)
      hw_state->cancel_pressed = (bool)current_input->input_state(
            input_driver_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info, binds,
            keyboard_mapping_blocked,
            0, pointer_device,
            0, RARCH_DEVICE_ID_POINTER_BACK);
   if (hw_state->cancel_pressed || (hw_state->cancel_pressed != last_cancel_pressed))
      hw_state->active = true;
   last_cancel_pressed = hw_state->cancel_pressed;
}

void menu_entries_settings_deinit(struct menu_state *menu_st)
{
   menu_setting_free(menu_st->entries.list_settings);
   if (menu_st->entries.list_settings)
      free(menu_st->entries.list_settings);
   menu_st->entries.list_settings = NULL;
}

static bool menu_driver_displaylist_push_internal(
      const char *label,
      menu_displaylist_info_t *info,
      settings_t *settings)
{
   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_HISTORY, info, settings))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_FAVORITES, info, settings))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_SETTINGS_ALL, info, settings))
         return true;
   }
#ifdef HAVE_CHEATS
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST, info, settings))
         return true;
   }
#endif
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strdup("lpl");
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
      menu_displaylist_ctl(DISPLAYLIST_MUSIC_HISTORY, info, settings);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strdup("lpl");
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
      menu_displaylist_ctl(DISPLAYLIST_VIDEO_HISTORY, info, settings);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strdup("lpl");
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#if 0
#ifdef HAVE_SCREENSHOTS
      if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT),
               msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT),
               MENU_ENUM_LABEL_TAKE_SCREENSHOT,
               MENU_SETTING_ACTION_SCREENSHOT, 0, 0);
      else
         info->need_push_no_playlist_entries = true;
#endif
#endif
      menu_displaylist_ctl(DISPLAYLIST_IMAGES_HISTORY, info, settings);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)))
   {
      const char *dir_playlist    = settings->paths.directory_playlist;

      filebrowser_clear_type();
      info->type                  = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strdup("lpl");
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      if (string_is_empty(dir_playlist))
      {
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         info->need_refresh                  = true;
         info->need_push_no_playlist_entries = true;
         info->need_push                     = true;

         return true;
      }

      if (!string_is_empty(info->path))
         free(info->path);

      info->path = strdup(dir_playlist);

      if (menu_displaylist_ctl(
               DISPLAYLIST_DATABASE_PLAYLISTS, info, settings))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_SCAN_DIRECTORY_LIST, info, settings))
         return true;
   }
#if defined(HAVE_LIBRETRODB)
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_EXPLORE, info, settings))
         return true;
   }
#endif
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_NETPLAY_ROOM_LIST, info, settings))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_HORIZONTAL, info, settings))
         return true;
   }

   return false;
}

bool menu_driver_displaylist_push(
      struct menu_state *menu_st,
      settings_t *settings,
      file_list_t *entry_list,
      file_list_t *entry_stack)
{
   menu_displaylist_info_t info;
   const char *path               = NULL;
   const char *label              = NULL;
   unsigned type                  = 0;
   bool ret                       = false;
   enum msg_hash_enums enum_idx   = MSG_UNKNOWN;
   file_list_t *list              = MENU_LIST_GET(menu_st->entries.list, 0);
   menu_file_list_cbs_t *cbs      = (menu_file_list_cbs_t*)
      list->list[list->size - 1].actiondata;

   menu_displaylist_info_init(&info);

   if (list && list->size)
      file_list_get_at_offset(list, list->size - 1, &path, &label, &type, NULL);

   if (cbs)
      enum_idx    = cbs->enum_idx;

   info.list      = entry_list;
   info.menu_list = entry_stack;
   info.type      = type;
   info.enum_idx  = enum_idx;

   if (!string_is_empty(path))
      info.path  = strdup(path);

   if (!string_is_empty(label))
      info.label = strdup(label);

   if (!info.list)
      goto error;

   if (menu_driver_displaylist_push_internal(label, &info, settings))
   {
      ret = menu_displaylist_process(&info);
      goto end;
   }

   cbs = (menu_file_list_cbs_t*)list->list[list->size - 1].actiondata;

   if (cbs && cbs->action_deferred_push)
      if (cbs->action_deferred_push(&info) != 0)
         goto error;

   ret = true;

end:
   menu_displaylist_info_free(&info);

   return ret;

error:
   menu_displaylist_info_free(&info);
   return false;
}

static void menu_input_key_bind_poll_bind_state_internal(
      const input_device_driver_t *joypad,
      struct menu_bind_state *state,
      unsigned port,
      bool timed_out)
{
   unsigned i;

   /* poll only the relevant port */
   for (i = 0; i < MENU_MAX_BUTTONS; i++)
      state->state[port].buttons[i] = joypad->button(port, i);

   for (i = 0; i < MENU_MAX_AXES; i++)
   {
      if (AXIS_POS(i) != AXIS_NONE)
         state->state[port].axes[i]  = joypad->axis(port, AXIS_POS(i));

      if (AXIS_NEG(i) != AXIS_NONE)
         state->state[port].axes[i] += joypad->axis(port, AXIS_NEG(i));
   }

   for (i = 0; i < MENU_MAX_HATS; i++)
   {
      if (joypad->button(port, HAT_MAP(i, HAT_UP_MASK)))
         state->state[port].hats[i] |= HAT_UP_MASK;
      if (joypad->button(port, HAT_MAP(i, HAT_DOWN_MASK)))
         state->state[port].hats[i] |= HAT_DOWN_MASK;
      if (joypad->button(port, HAT_MAP(i, HAT_LEFT_MASK)))
         state->state[port].hats[i] |= HAT_LEFT_MASK;
      if (joypad->button(port, HAT_MAP(i, HAT_RIGHT_MASK)))
         state->state[port].hats[i] |= HAT_RIGHT_MASK;
   }
}

/* This sets up all the callback functions for a menu entry.
 *
 * OK     : When we press the 'OK' button on an entry.
 * Cancel : When we press the 'Cancel' button on an entry.
 * Scan   : When we press the 'Scan' button on an entry.
 * Start  : When we press the 'Start' button on an entry.
 * Select : When we press the 'Select' button on an entry.
 * Info   : When we press the 'Info' button on an entry.
 * Left   : when we press 'Left' on the D-pad while this entry is selected.
 * Right  : when we press 'Right' on the D-pad while this entry is selected.
 * Deferred push : When pressing an entry results in spawning a new list, it waits until the next
 * frame to push this onto the stack. This function callback will then be invoked.
 * Get value: Each entry has associated 'text', which we call the value. This function callback
 * lets us render that text.
 * Title: Each entry can have a custom 'title'.
 * Label: Each entry has a label name. This function callback lets us render that label text.
 * Sublabel: each entry has a sublabel, which consists of one or more lines of additional information.
 * This function callback lets us render that text.
 */
void menu_cbs_init(
      struct menu_state *menu_st,
      const menu_ctx_driver_t *menu_driver_ctx,
      file_list_t *list,
      menu_file_list_cbs_t *cbs,
      const char *path, const char *label,
      unsigned type, size_t idx)
{
   const char *menu_label         = NULL;
   file_list_t *menu_list         = MENU_LIST_GET(menu_st->entries.list, 0);
#ifdef DEBUG_LOG
   menu_file_list_cbs_t *menu_cbs = (menu_file_list_cbs_t*)
      menu_list->list[list->size - 1].actiondata;
   enum msg_hash_enums enum_idx   = menu_cbs ? menu_cbs->enum_idx : MSG_UNKNOWN;
#endif

   if (menu_list && menu_list->size)
      file_list_get_at_offset(menu_list, menu_list->size - 1, NULL, &menu_label, NULL, NULL);

   if (!label || !menu_label)
      return;

#ifdef DEBUG_LOG
   RARCH_LOG("\n");

   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
      RARCH_LOG("\t\t\tenum_idx %d [%s]\n", cbs->enum_idx, msg_hash_to_str(cbs->enum_idx));
#endif

   /* It will try to find a corresponding callback function inside
    * menu_cbs_ok.c, then map this callback to the entry. */
   menu_cbs_init_bind_ok(cbs, path, label, type, idx, menu_label);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_cancel.c, then map this callback to the entry. */
   menu_cbs_init_bind_cancel(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_scan.c, then map this callback to the entry. */
   menu_cbs_init_bind_scan(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_start.c, then map this callback to the entry. */
   menu_cbs_init_bind_start(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_select.c, then map this callback to the entry. */
   menu_cbs_init_bind_select(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_info.c, then map this callback to the entry. */
   menu_cbs_init_bind_info(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_left.c, then map this callback to the entry. */
   menu_cbs_init_bind_left(cbs, path, label, type, idx, menu_label);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_right.c, then map this callback to the entry. */
   menu_cbs_init_bind_right(cbs, path, label, type, idx, menu_label);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_deferred_push.c, then map this callback to the entry. */
   menu_cbs_init_bind_deferred_push(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_get_string_representation.c, then map this callback to the entry. */
   menu_cbs_init_bind_get_string_representation(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_title.c, then map this callback to the entry. */
   menu_cbs_init_bind_title(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_label.c, then map this callback to the entry. */
   menu_cbs_init_bind_label(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_sublabel.c, then map this callback to the entry. */
   menu_cbs_init_bind_sublabel(cbs, path, label, type, idx);

   if (menu_driver_ctx && menu_driver_ctx->bind_init)
      menu_driver_ctx->bind_init(
            cbs,
            path,
            label,
            type,
            idx);
}

/* Pretty much a stub function. TODO/FIXME - Might as well remove this. */
int menu_cbs_exit(void)
{
   return -1;
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
void menu_driver_get_last_shader_path_int(
      settings_t *settings, enum rarch_shader_type type,
      const char *shader_dir, const char *shader_file_name,
      const char **dir_out, const char **file_name_out)
{
   bool remember_last_dir       = settings->bools.video_shader_remember_last_dir;
   const char *video_shader_dir = settings->paths.directory_video_shader;

   /* File name is NULL by default */
   if (file_name_out)
      *file_name_out = NULL;

   /* If any of the following are true:
    * - Directory caching is disabled
    * - No directory has been cached
    * - Cached directory is invalid
    * - Last selected shader is incompatible with
    *   the current video driver
    * ...use default settings */
   if (!remember_last_dir ||
       (type == RARCH_SHADER_NONE) ||
       string_is_empty(shader_dir) ||
       !path_is_directory(shader_dir) ||
       !video_shader_is_supported(type))
   {
      if (dir_out)
         *dir_out = video_shader_dir;
      return;
   }

   /* Assign last set directory */
   if (dir_out)
      *dir_out = shader_dir;

   /* Assign file name */
   if (file_name_out &&
       !string_is_empty(shader_file_name))
      *file_name_out = shader_file_name;
}

int menu_shader_manager_clear_num_passes(struct video_shader *shader)
{
   bool refresh                = false;

   if (!shader)
      return 0;

   shader->passes = 0;

#ifdef HAVE_MENU
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
#endif

   video_shader_resolve_parameters(shader);

   shader->modified = true;

   return 0;
}

int menu_shader_manager_clear_parameter(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_parameter *param = shader ?
      &shader->parameters[i] : NULL;

   if (!param)
      return 0;

   param->current = param->initial;
   param->current = MIN(MAX(param->minimum,
            param->current), param->maximum);

   shader->modified = true;

   return 0;
}

int menu_shader_manager_clear_pass_filter(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (!shader_pass)
      return -1;

   shader_pass->filter = RARCH_FILTER_UNSPEC;

   shader->modified = true;

   return 0;
}

void menu_shader_manager_clear_pass_scale(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (!shader_pass)
      return;

   shader_pass->fbo.scale_x = 0;
   shader_pass->fbo.scale_y = 0;
   shader_pass->fbo.valid   = false;

   shader->modified         = true;
}

void menu_shader_manager_clear_pass_path(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_pass
      *shader_pass              = shader
      ? &shader->pass[i]
      : NULL;

   if (shader_pass)
      *shader_pass->source.path = '\0';

   if (shader)
      shader->modified          = true;
}

/**
 * menu_shader_manager_get_type:
 * @shader                   : shader handle
 *
 * Gets type of shader.
 *
 * Returns: type of shader.
 **/
enum rarch_shader_type menu_shader_manager_get_type(
      const struct video_shader *shader)
{
   enum rarch_shader_type type       = RARCH_SHADER_NONE;
   /* All shader types must be the same, or we cannot use it. */
   size_t i                         = 0;

   if (!shader)
      return RARCH_SHADER_NONE;

   type = video_shader_parse_type(shader->path);

   if (!shader->passes)
      return type;

   if (type == RARCH_SHADER_NONE)
   {
      type = video_shader_parse_type(shader->pass[0].source.path);
      i    = 1;
   }

   for (; i < shader->passes; i++)
   {
      enum rarch_shader_type pass_type =
         video_shader_parse_type(shader->pass[i].source.path);

      switch (pass_type)
      {
         case RARCH_SHADER_CG:
         case RARCH_SHADER_GLSL:
         case RARCH_SHADER_SLANG:
            if (type != pass_type)
               return RARCH_SHADER_NONE;
            break;
         default:
            break;
      }
   }

   return type;
}

/**
 * menu_shader_manager_apply_changes:
 *
 * Apply shader state changes.
 **/
void menu_shader_manager_apply_changes(
      struct video_shader *shader,
      const char *dir_video_shader,
      const char *dir_menu_config)
{
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   if (!shader)
      return;

   type = menu_shader_manager_get_type(shader);

   if (shader->passes && type != RARCH_SHADER_NONE)
   {
      menu_shader_manager_save_preset(shader, NULL,
            dir_video_shader, dir_menu_config, true);
      return;
   }

   menu_shader_manager_set_preset(NULL, type, NULL, true);
}
#endif

enum action_iterate_type action_iterate_type(const char *label)
{
   if (string_is_equal(label, "info_screen"))
      return ITERATE_TYPE_INFO;
   if (string_starts_with_size(label, "help", STRLEN_CONST("help")))
      if (
            string_is_equal(label, "help") ||
            string_is_equal(label, "help_controls") ||
            string_is_equal(label, "help_what_is_a_core") ||
            string_is_equal(label, "help_loading_content") ||
            string_is_equal(label, "help_scanning_content") ||
            string_is_equal(label, "help_change_virtual_gamepad") ||
            string_is_equal(label, "help_audio_video_troubleshooting") ||
            string_is_equal(label, "help_send_debug_info")
         )
         return ITERATE_TYPE_HELP;
   if (string_is_equal(label, "cheevos_description"))
         return ITERATE_TYPE_HELP;
   if (string_starts_with_size(label, "custom_bind", STRLEN_CONST("custom_bind")))
      if (
            string_is_equal(label, "custom_bind") ||
            string_is_equal(label, "custom_bind_all") ||
            string_is_equal(label, "custom_bind_defaults")
         )
         return ITERATE_TYPE_BIND;

   return ITERATE_TYPE_DEFAULT;
}

/* Returns true if search filter is enabled
 * for the specified menu list */
bool menu_driver_search_filter_enabled(const char *label, unsigned type)
{
   bool filter_enabled = false;

   /* > Check for playlists */
   filter_enabled = (type == MENU_SETTING_HORIZONTAL_MENU) ||
                    (type == MENU_HISTORY_TAB) ||
                    (type == MENU_FAVORITES_TAB) ||
                    (type == MENU_IMAGES_TAB) ||
                    (type == MENU_MUSIC_TAB) ||
                    (type == MENU_VIDEO_TAB) ||
                    (type == FILE_TYPE_PLAYLIST_COLLECTION);

   if (!filter_enabled && !string_is_empty(label))
      filter_enabled = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY)) ||
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST)) ||
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST)) ||
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST)) ||
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST)) ||
                       /* > Core updater */
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST)) ||
                       /* > File browser (Load Content) */
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES)) ||
                       /* > Shader presets/passes */
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET)) ||
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PASS)) ||
                       /* > Cheat files */
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD)) ||
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND)) ||
                       /* > Cheats */
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)) ||
                       /* > Overlays */
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_OVERLAY)) ||
                       /* > Manage Cores */
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_LIST));

   return filter_enabled;
}

void menu_input_key_bind_poll_bind_state(
      input_driver_state_t *input_driver_st,
      const struct retro_keybind **binds,
      float input_axis_threshold,
      unsigned joy_idx,
      struct menu_bind_state *state,
      bool timed_out,
      bool keyboard_mapping_blocked)
{
   unsigned b;
   rarch_joypad_info_t joypad_info;
   input_driver_t *current_input           = input_driver_st->current_driver;
   void *input_data                        = input_driver_st->current_data;
   unsigned port                           = state->port;
   const input_device_driver_t *joypad     = input_driver_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t *sec_joypad = input_driver_st->secondary_joypad;
#else
   const input_device_driver_t *sec_joypad = NULL;
#endif

   memset(state->state, 0, sizeof(state->state));

   joypad_info.axis_threshold           = input_axis_threshold;
   joypad_info.joy_idx                  = joy_idx;
   joypad_info.auto_binds               = input_autoconf_binds[joy_idx];

   if (current_input->input_state)
   {
      /* Poll mouse (on the relevant port)
       *
       * Check if key was being pressed by 
       * user with mouse number 'port'
       *
       * NOTE: We start iterating on 2 (RETRO_DEVICE_ID_MOUSE_LEFT),
       * because we want to skip the axes
       */
      for (b = 2; b < MENU_MAX_MBUTTONS; b++)
      {
         state->state[port].mouse_buttons[b] =
            current_input->input_state(
                  input_driver_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  binds,
                  keyboard_mapping_blocked,
                  port,
                  RETRO_DEVICE_MOUSE, 0, b);
      }
   }

   joypad_info.joy_idx        = 0;
   joypad_info.auto_binds     = NULL;
   joypad_info.axis_threshold = 0.0f;

   state->skip                = timed_out;
   if (current_input->input_state)
      state->skip             |=
         current_input->input_state(
               input_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               RETRO_DEVICE_KEYBOARD,
               0,
               RETROK_RETURN);

   if (joypad)
   {
      if (joypad->poll)
         joypad->poll();
      menu_input_key_bind_poll_bind_state_internal(
            joypad, state, port, timed_out);
   }

   if (sec_joypad)
   {
      if (sec_joypad->poll)
         sec_joypad->poll();
      menu_input_key_bind_poll_bind_state_internal(
            sec_joypad, state, port, timed_out);
   }
}

int menu_dialog_iterate(
      menu_dialog_t *p_dialog,
      settings_t *settings,
      char *s, size_t len,
      retro_time_t current_time
)
{
   switch (p_dialog->current_type)
   {
      case MENU_DIALOG_WELCOME:
         {
            static rarch_timer_t timer;

            if (!timer.timer_begin)
            {
               timer.timeout_us  = 3 * 1000000;
               timer.current     = cpu_features_get_time_usec();
               timer.timeout_end = timer.current + timer.timeout_us;
               timer.timer_begin = true;
               timer.timer_end   = false;
            }

            timer.current    = current_time;
            timer.timeout_us = (timer.timeout_end = timer.current);

            msg_hash_get_help_enum(
                  MENU_ENUM_LABEL_WELCOME_TO_RETROARCH,
                  s, len);

            if (!timer.timer_end && (timer.timeout_us <= 0))
            {
               timer.timer_end        = true;
               timer.timer_begin      = false;
               timer.timeout_end      = 0;
               p_dialog->current_type = MENU_DIALOG_NONE;
               return 1;
            }
         }
         break;
      case MENU_DIALOG_HELP_CONTROLS:
         {
            unsigned i;
            char s2[PATH_MAX_LENGTH];
            const unsigned binds[] = {
               RETRO_DEVICE_ID_JOYPAD_UP,
               RETRO_DEVICE_ID_JOYPAD_DOWN,
               RETRO_DEVICE_ID_JOYPAD_A,
               RETRO_DEVICE_ID_JOYPAD_B,
               RETRO_DEVICE_ID_JOYPAD_SELECT,
               RETRO_DEVICE_ID_JOYPAD_START,
               RARCH_MENU_TOGGLE,
               RARCH_QUIT_KEY,
               RETRO_DEVICE_ID_JOYPAD_X,
               RETRO_DEVICE_ID_JOYPAD_Y,
            };
            char desc[ARRAY_SIZE(binds)][64];

            for (i = 0; i < ARRAY_SIZE(binds); i++)
               desc[i][0] = '\0';

            for (i = 0; i < ARRAY_SIZE(binds); i++)
            {
               const struct retro_keybind *keybind = &input_config_binds[0][binds[i]];
               const struct retro_keybind *auto_bind =
                  (const struct retro_keybind*)
                  input_config_get_bind_auto(0, binds[i]);

               input_config_get_bind_string(settings, desc[i],
                     keybind, auto_bind, sizeof(desc[i]));
            }

            s2[0] = '\0';

            msg_hash_get_help_enum(
                  MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG,
                  s2, sizeof(s2));

            snprintf(s, len,
                  "%s"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n",

                  s2,

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP),
                  desc[0],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN),
                  desc[1],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM),
                  desc[2],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK),
                  desc[3],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO),
                  desc[4],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START),
                  desc[5],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU),
                  desc[6],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT),
                  desc[7],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD),
                  desc[8]

                  );
         }
         break;

#ifdef HAVE_CHEEVOS
      case MENU_DIALOG_HELP_CHEEVOS_DESCRIPTION:
         if (!rcheevos_menu_get_sublabel(p_dialog->current_id, s, len))
            return 1;
         break;
#endif

      case MENU_DIALOG_HELP_WHAT_IS_A_CORE:
         msg_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_LOADING_CONTENT:
         msg_hash_get_help_enum(MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
               s, len);
         break;
      case MENU_DIALOG_HELP_CHANGE_VIRTUAL_GAMEPAD:
         msg_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         msg_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_SEND_DEBUG_INFO:
         msg_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_SCANNING_CONTENT:
         msg_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_EXTRACT:
         {
            bool bundle_finished        = settings->bools.bundle_finished;

            msg_hash_get_help_enum(
                  MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT,
                  s, len);

            if (bundle_finished)
            {
               configuration_set_bool(settings,
                     settings->bools.bundle_finished, false);
               p_dialog->current_type = MENU_DIALOG_NONE;
               return 1;
            }
         }
         break;
      case MENU_DIALOG_QUIT_CONFIRM:
      case MENU_DIALOG_INFORMATION:
      case MENU_DIALOG_QUESTION:
      case MENU_DIALOG_WARNING:
      case MENU_DIALOG_ERROR:
         msg_hash_get_help_enum(MSG_UNKNOWN,
               s, len);
         break;
      case MENU_DIALOG_NONE:
      default:
         break;
   }

   return 0;
}

void menu_entries_list_deinit(
      const menu_ctx_driver_t *menu_driver_ctx,
      struct menu_state *menu_st)
{
   if (menu_st->entries.list)
      menu_list_free(menu_driver_ctx, menu_st->entries.list);
   menu_st->entries.list          = NULL;
}

bool menu_entries_init(
      struct menu_state *menu_st,
      const menu_ctx_driver_t *menu_driver_ctx)
{
   if (!(menu_st->entries.list = (menu_list_t*)menu_list_new(menu_driver_ctx)))
      return false;
   if (!(menu_st->entries.list_settings = menu_setting_new()))
      return false;
   return true;
}

bool generic_menu_init_list(struct menu_state *menu_st,
      settings_t *settings)
{
   menu_displaylist_info_t info;
   menu_list_t *menu_list       = menu_st->entries.list;
   file_list_t *menu_stack      = NULL;
   file_list_t *selection_buf   = NULL;

   if (menu_list)
   {
      menu_stack                = MENU_LIST_GET(menu_list, (unsigned)0);
      selection_buf             = MENU_LIST_GET_SELECTION(menu_list, (unsigned)0);
   }

   menu_displaylist_info_init(&info);

   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.enum_idx                = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append_enum(menu_stack,
         info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0);

   info.list                    = selection_buf;

   if (menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info, settings))
      menu_displaylist_process(&info);

   menu_displaylist_info_free(&info);

   return true;
}

bool rarch_menu_init(
      struct menu_state *menu_st,
      menu_dialog_t        *p_dialog,
      const menu_ctx_driver_t *menu_driver_ctx,
      menu_input_t *menu_input,
      menu_input_pointer_hw_state_t *pointer_hw_state,
      settings_t *settings
      )
{
#ifdef HAVE_CONFIGFILE
   bool menu_show_start_screen = settings->bools.menu_show_start_screen;
   bool config_save_on_exit    = settings->bools.config_save_on_exit;
#endif

   /* Ensure that menu pointer input is correctly
    * initialised */
   memset(menu_input, 0, sizeof(menu_input_t));
   memset(pointer_hw_state, 0, sizeof(menu_input_pointer_hw_state_t));

   if (!menu_entries_init(menu_st, menu_driver_ctx))
   {
      menu_entries_settings_deinit(menu_st);
      menu_entries_list_deinit(menu_driver_ctx, menu_st);
      return false;
   }

#ifdef HAVE_CONFIGFILE
   if (menu_show_start_screen)
   {
      /* We don't want the welcome dialog screen to show up
       * again after the first startup, so we save to config
       * file immediately. */
      p_dialog->current_type         = MENU_DIALOG_WELCOME;

      configuration_set_bool(settings,
            settings->bools.menu_show_start_screen, false);
      if (config_save_on_exit)
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
   }
#endif

#ifdef HAVE_COMPRESSION
   if (      settings->bools.bundle_assets_extract_enable
         && !string_is_empty(settings->arrays.bundle_assets_src)
         && !string_is_empty(settings->arrays.bundle_assets_dst)
         && (settings->uints.bundle_assets_extract_version_current
            != settings->uints.bundle_assets_extract_last_version)
      )
   {
      p_dialog->current_type         = MENU_DIALOG_HELP_EXTRACT;
      task_push_decompress(
            settings->arrays.bundle_assets_src,
            settings->arrays.bundle_assets_dst,
            NULL,
            settings->arrays.bundle_assets_dst_subdir,
            NULL,
            bundle_decompressed,
            NULL,
            NULL,
            false);
      /* Support only 1 version - setting this would prevent the assets from being extracted every time */
      configuration_set_int(settings,
            settings->uints.bundle_assets_extract_last_version, 1);
   }
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   menu_shader_manager_init();
#endif

   return true;
}

void menu_input_set_pointer_visibility(
      menu_input_pointer_hw_state_t *pointer_hw_state,
      menu_input_t *menu_input,
      retro_time_t current_time)
{
   static bool cursor_shown                        = false;
   static bool cursor_hidden                       = false;
   static retro_time_t end_time                    = 0;

   /* Ensure that mouse cursor is hidden when not in use */
   if ((menu_input->pointer.type == MENU_POINTER_MOUSE)
         && pointer_hw_state->active)
   {
      /* Show cursor */
      if ((current_time > end_time) && !cursor_shown)
      {
         menu_ctx_environment_t menu_environ;
         menu_environ.type = MENU_ENVIRON_ENABLE_MOUSE_CURSOR;
         menu_environ.data = NULL;

         menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
         cursor_shown  = true;
         cursor_hidden = false;
      }

      end_time = current_time + MENU_INPUT_HIDE_CURSOR_DELAY;
   }
   else
   {
      /* Hide cursor */
      if ((current_time > end_time) && !cursor_hidden)
      {
         menu_ctx_environment_t menu_environ;
         menu_environ.type = MENU_ENVIRON_DISABLE_MOUSE_CURSOR;
         menu_environ.data = NULL;

         menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
         cursor_shown  = false;
         cursor_hidden = true;
      }
   }
}

/**
 * menu_entries_elem_get_first_char:
 * @list                     : File list handle.
 * @offset                   : Offset index of element.
 *
 * Gets the first character of an element in the
 * file list.
 *
 * Returns: first character of element in file list.
 **/
int menu_entries_elem_get_first_char(
      file_list_t *list, unsigned offset)
{
   const char *path =   list->list[offset].alt
                      ? list->list[offset].alt
                      : list->list[offset].path;
   int ret          = path ? TOLOWER((int)*path) : 0;

   /* "Normalize" non-alphabetical entries so they
    * are lumped together for purposes of jumping. */
   if (ret < 'a')
      return ('a' - 1);
   else if (ret > 'z')
      return ('z' + 1);
   return ret;
}

void menu_entries_build_scroll_indices(
      struct menu_state *menu_st,
      file_list_t *list)
{
   bool current_is_dir             = false;
   size_t i                        = 0;
   int current                     = menu_entries_elem_get_first_char(list, 0);
   unsigned type                   = list->list[0].type;

   menu_st->scroll.index_list[0]   = 0;
   menu_st->scroll.index_size      = 1;

   if (type == FILE_TYPE_DIRECTORY)
      current_is_dir               = true;

   for (i = 1; i < list->size; i++)
   {
      int first    = menu_entries_elem_get_first_char(list, (unsigned)i);
      bool is_dir  = false;
      unsigned idx = (unsigned)i;

      type         = list->list[idx].type;

      if (type == FILE_TYPE_DIRECTORY)
         is_dir = true;

      if ((current_is_dir && !is_dir) || (first > current))
      {
         /* Add scroll index */
         menu_st->scroll.index_list[menu_st->scroll.index_size]   = i;
         if (!((menu_st->scroll.index_size + 1) >= SCROLL_INDEX_SIZE))
            menu_st->scroll.index_size++;
      }

      current        = first;
      current_is_dir = is_dir;
   }

   /* Add scroll index */
   menu_st->scroll.index_list[menu_st->scroll.index_size]   = list->size - 1;
   if (!((menu_st->scroll.index_size + 1) >= SCROLL_INDEX_SIZE))
      menu_st->scroll.index_size++;
}

void menu_display_common_image_upload(
      const menu_ctx_driver_t *menu_driver_ctx,
      void *menu_userdata,
      struct texture_image *img,
      void *user_data,
      unsigned type)
{
   if (     menu_driver_ctx
         && menu_driver_ctx->load_image)
      menu_driver_ctx->load_image(menu_userdata,
            img, (enum menu_image_type)type);

   image_texture_free(img);
   free(img);
   free(user_data);
}

enum menu_driver_id_type menu_driver_set_id(
      const char *driver_name)
{
   if (!string_is_empty(driver_name))
   {
      if (string_is_equal(driver_name, "rgui"))
         return MENU_DRIVER_ID_RGUI;
      else if (string_is_equal(driver_name, "ozone"))
         return MENU_DRIVER_ID_OZONE;
      else if (string_is_equal(driver_name, "glui"))
         return MENU_DRIVER_ID_GLUI;
      else if (string_is_equal(driver_name, "xmb"))
         return MENU_DRIVER_ID_XMB;
      else if (string_is_equal(driver_name, "stripes"))
         return MENU_DRIVER_ID_STRIPES;
   }
   return MENU_DRIVER_ID_UNKNOWN;
}

const char *config_get_menu_driver_options(void)
{
   return char_list_new_special(STRING_LIST_MENU_DRIVERS, NULL);
}

bool menu_entries_search_push(const char *search_term)
{
   size_t i;
   menu_search_terms_t *search = menu_entries_search_get_terms_internal();
   char search_term_clipped[MENU_SEARCH_FILTER_MAX_LENGTH];

   search_term_clipped[0] = '\0';

   /* Sanity check + verify whether we have reached
    * the maximum number of allowed search terms */
   if (!search ||
       string_is_empty(search_term) ||
       (search->size >= MENU_SEARCH_FILTER_MAX_TERMS))
      return false;

   /* Check whether search term already exists
    * > Note that we clip the input search term
    *   to MENU_SEARCH_FILTER_MAX_LENGTH characters
    *   *before* comparing existing entries */
   strlcpy(search_term_clipped, search_term,
         sizeof(search_term_clipped));

   for (i = 0; i < search->size; i++)
   {
      if (string_is_equal(search_term_clipped,
            search->terms[i]))
         return false;
   }

   /* Add search term */
   strlcpy(search->terms[search->size], search_term_clipped,
         sizeof(search->terms[search->size]));
   search->size++;

   return true;
}

bool menu_entries_search_pop(void)
{
   menu_search_terms_t *search = menu_entries_search_get_terms_internal();

   /* Do nothing if list of search terms is empty */
   if (!search ||
       (search->size == 0))
      return false;

   /* Remove last item from the list */
   search->size--;
   search->terms[search->size][0] = '\0';

   return true;
}

menu_search_terms_t *menu_entries_search_get_terms(void)
{
   menu_search_terms_t *search = menu_entries_search_get_terms_internal();

   if (!search ||
       (search->size == 0))
      return NULL;

   return search;
}

void menu_entries_search_append_terms_string(char *s, size_t len)
{
   menu_search_terms_t *search = menu_entries_search_get_terms_internal();

   if (search &&
       (search->size > 0) &&
       s)
   {
      size_t current_len = strlen_size(s, len);
      size_t i;

      /* If buffer is already 'full', nothing
       * further can be added */
      if (current_len >= len)
         return;

      s   += current_len;
      len -= current_len;

      for (i = 0; i < search->size; i++)
      {
         strlcat(s, " > ", len);
         strlcat(s, search->terms[i], len);
      }
   }
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
bool menu_shader_manager_save_preset_internal(
      bool save_reference,
      const struct video_shader *shader,
      const char *basename,
      const char *dir_video_shader,
      bool apply,
      const char **target_dirs,
      size_t num_target_dirs)
{
   char fullname[PATH_MAX_LENGTH];
   char buffer[PATH_MAX_LENGTH];
   bool ret                       = false;
   enum rarch_shader_type type    = RARCH_SHADER_NONE;
   char *preset_path              = NULL;
   size_t i                       = 0;

   fullname[0] = buffer[0]        = '\0';

   if (!shader || !shader->passes)
      return false;

   type = menu_shader_manager_get_type(shader);

   if (type == RARCH_SHADER_NONE)
      return false;

   if (!string_is_empty(basename))
   {
      /* We are comparing against a fixed list of file
       * extensions, the longest (slangp) being 6 characters
       * in length. We therefore only need to extract the first
       * 7 characters from the extension of the input path
       * to correctly validate a match */
      char ext_lower[8];
      const char *ext = NULL;

      ext_lower[0]    = '\0';

      strlcpy(fullname, basename, sizeof(fullname));

      /* Get file extension */
      ext = strrchr(basename, '.');

      /* Copy and convert to lower case */
      if (ext && (*(++ext) != '\0'))
      {
         strlcpy(ext_lower, ext, sizeof(ext_lower));
         string_to_lower(ext_lower);
      }

      /* Append extension automatically as appropriate. */
      if (     !string_is_equal(ext_lower, "cgp")
            && !string_is_equal(ext_lower, "glslp")
            && !string_is_equal(ext_lower, "slangp"))
      {
         const char *preset_ext = video_shader_get_preset_extension(type);
         strlcat(fullname, preset_ext, sizeof(fullname));
      }
   }
   else
      snprintf(fullname, sizeof(fullname), "retroarch%s",
            video_shader_get_preset_extension(type));

   if (path_is_absolute(fullname))
   {
      preset_path = fullname;
      ret         = video_shader_write_preset(preset_path,
            dir_video_shader,
            shader, save_reference);

      if (ret)
         RARCH_LOG("[Shaders - Save Preset]: Saved shader preset to %s.\n", preset_path);
      else
         RARCH_ERR("[Shaders - Save Preset]: Failed writing shader preset to %s.\n", preset_path);
   }
   else
   {
      char basedir[PATH_MAX_LENGTH];

      for (i = 0; i < num_target_dirs; i++)
      {
         if (string_is_empty(target_dirs[i]))
            continue;

         fill_pathname_join(buffer, target_dirs[i],
               fullname, sizeof(buffer));

         strlcpy(basedir, buffer, sizeof(basedir));
         path_basedir(basedir);

         if (!path_is_directory(basedir))
         {
            ret = path_mkdir(basedir);

            if (!ret)
            {
               RARCH_WARN("[Shaders - Save Preset]: Failed to create preset directory %s.\n", basedir);
               continue;
            }
         }

         preset_path = buffer;

         ret = video_shader_write_preset(preset_path,
               dir_video_shader,
               shader, save_reference);

         if (ret)
         {
            RARCH_LOG("[Shaders - Save Preset]: Saved shader preset to %s.\n", preset_path);
            break;
         }
         else
            RARCH_WARN("[Shaders - Save Preset]: Failed writing shader preset to %s.\n", preset_path);
      }

      if (!ret)
         RARCH_ERR("[Shaders - Save Preset]: Failed to write shader preset. Make sure shader directory"
               " and/or config directory are writable.\n");
   }

   if (ret && apply)
      menu_shader_manager_set_preset(NULL, type, preset_path, true);

   return ret;
}

bool menu_shader_manager_operate_auto_preset(
      struct retro_system_info *system,
      bool video_shader_preset_save_reference_enable,
      enum auto_shader_operation op,
      const struct video_shader *shader,
      const char *dir_video_shader,
      const char *dir_menu_config,
      enum auto_shader_type type, bool apply)
{
   char old_presets_directory[PATH_MAX_LENGTH];
   char config_directory[PATH_MAX_LENGTH];
   char tmp[PATH_MAX_LENGTH];
   char file[PATH_MAX_LENGTH];
   static enum rarch_shader_type shader_types[] =
   {
      RARCH_SHADER_GLSL, RARCH_SHADER_SLANG, RARCH_SHADER_CG
   };
   const char *core_name            = system ? system->library_name : NULL;
   const char *auto_preset_dirs[3]  = {0};

   old_presets_directory[0] = config_directory[0] = tmp[0] = file[0] = '\0';

   if (type != SHADER_PRESET_GLOBAL && string_is_empty(core_name))
      return false;

   if (!path_is_empty(RARCH_PATH_CONFIG))
      fill_pathname_basedir(
            config_directory,
            path_get(RARCH_PATH_CONFIG),
            sizeof(config_directory));

   /* We are only including this directory for compatibility purposes with
    * versions 1.8.7 and older. */
   if (op != AUTO_SHADER_OP_SAVE && !string_is_empty(dir_video_shader))
      fill_pathname_join(
            old_presets_directory,
            dir_video_shader,
            "presets",
            sizeof(old_presets_directory));

   auto_preset_dirs[0] = dir_menu_config;
   auto_preset_dirs[1] = config_directory;
   auto_preset_dirs[2] = old_presets_directory;

   switch (type)
   {
      case SHADER_PRESET_GLOBAL:
         strcpy_literal(file, "global");
         break;
      case SHADER_PRESET_CORE:
         fill_pathname_join(file, core_name, core_name, sizeof(file));
         break;
      case SHADER_PRESET_PARENT:
         fill_pathname_parent_dir_name(tmp,
               path_get(RARCH_PATH_BASENAME), sizeof(tmp));
         fill_pathname_join(file, core_name, tmp, sizeof(file));
         break;
      case SHADER_PRESET_GAME:
         {
            const char *game_name =
               path_basename(path_get(RARCH_PATH_BASENAME));
            if (string_is_empty(game_name))
               return false;
            fill_pathname_join(file, core_name, game_name, sizeof(file));
            break;
         }
      default:
         return false;
   }

   switch (op)
   {
      case AUTO_SHADER_OP_SAVE:
         return menu_shader_manager_save_preset_internal(
               video_shader_preset_save_reference_enable,
               shader, file,
               dir_video_shader,
               apply,
               auto_preset_dirs,
               ARRAY_SIZE(auto_preset_dirs));
      case AUTO_SHADER_OP_REMOVE:
         {
            /* remove all supported auto-shaders of given type */
            char *end;
            size_t i, j, m;

            char preset_path[PATH_MAX_LENGTH];

            /* n = amount of relevant shader presets found
             * m = amount of successfully deleted shader presets */
            size_t n = m = 0;

            for (i = 0; i < ARRAY_SIZE(auto_preset_dirs); i++)
            {
               if (string_is_empty(auto_preset_dirs[i]))
                  continue;

               fill_pathname_join(preset_path,
                     auto_preset_dirs[i], file, sizeof(preset_path));
               end = preset_path + strlen(preset_path);

               for (j = 0; j < ARRAY_SIZE(shader_types); j++)
               {
                  const char *preset_ext;

                  if (!video_shader_is_supported(shader_types[j]))
                     continue;

                  preset_ext = video_shader_get_preset_extension(shader_types[j]);
                  strlcpy(end, preset_ext, sizeof(preset_path) - (end - preset_path));

                  if (path_is_valid(preset_path))
                  {
                     n++;

                     if (!filestream_delete(preset_path))
                     {
                        m++;
                        RARCH_LOG("[Shaders]: Deleted shader preset from \"%s\".\n", preset_path);
                     }
                     else
                        RARCH_WARN("[Shaders]: Failed to remove shader preset at \"%s\".\n", preset_path);
                  }
               }
            }

            return n == m;
         }
      case AUTO_SHADER_OP_EXISTS:
         {
            /* test if any supported auto-shaders of given type exists */
            char *end;
            size_t i, j;

            char preset_path[PATH_MAX_LENGTH];

            for (i = 0; i < ARRAY_SIZE(auto_preset_dirs); i++)
            {
               if (string_is_empty(auto_preset_dirs[i]))
                  continue;

               fill_pathname_join(preset_path,
                     auto_preset_dirs[i], file, sizeof(preset_path));
               end = preset_path + strlen(preset_path);

               for (j = 0; j < ARRAY_SIZE(shader_types); j++)
               {
                  const char *preset_ext;

                  if (!video_shader_is_supported(shader_types[j]))
                     continue;

                  preset_ext = video_shader_get_preset_extension(shader_types[j]);
                  strlcpy(end, preset_ext, sizeof(preset_path) - (end - preset_path));

                  if (path_is_valid(preset_path))
                     return true;
               }
            }
         }
         break;
   }

   return false;
}

void menu_driver_set_last_shader_path_int(
      const char *shader_path,
      enum rarch_shader_type *type,
      char *shader_dir, size_t dir_len,
      char *shader_file, size_t file_len)
{
   const char *file_name = NULL;

   if (!type ||
       !shader_dir ||
       (dir_len < 1) ||
       !shader_file ||
       (file_len < 1))
      return;

   /* Reset existing cache */
   *type          = RARCH_SHADER_NONE;
   shader_dir[0]  = '\0';
   shader_file[0] = '\0';

   /* If path is empty, do nothing */
   if (string_is_empty(shader_path))
      return;

   /* Get shader type */
   *type = video_shader_parse_type(shader_path);

   /* If type is invalid, do nothing */
   if (*type == RARCH_SHADER_NONE)
      return;

   /* Cache parent directory */
   fill_pathname_parent_dir(shader_dir, shader_path, dir_len);

   /* If parent directory is empty, then file name
    * is only valid if 'shader_path' refers to an
    * existing file in the root of the file system */
   if (string_is_empty(shader_dir) &&
       !path_is_valid(shader_path))
      return;

   /* Cache file name */
   file_name = path_basename_nocompression(shader_path);
   if (!string_is_empty(file_name))
      strlcpy(shader_file, file_name, file_len);
}

bool dir_init_shader_internal(
      bool shader_remember_last_dir,
      struct rarch_dir_shader_list *dir_list,
      const char *shader_dir,
      const char *shader_file_name,
      bool show_hidden_files)
{
   size_t i;
   struct string_list *new_list           = dir_list_new_special(
         shader_dir, DIR_LIST_SHADERS, NULL, show_hidden_files);
   bool search_file_name                  = shader_remember_last_dir &&
         !string_is_empty(shader_file_name);

   if (!new_list)
      return false;

   if (new_list->size < 1)
   {
      dir_list_free(new_list);
      return false;
   }

   dir_list_sort(new_list, false);

   dir_list->shader_list              = new_list;
   dir_list->directory                = strdup(shader_dir);
   dir_list->selection                = 0;
   dir_list->shader_loaded            = false;
   dir_list->remember_last_preset_dir = shader_remember_last_dir;

   if (search_file_name)
   {
      for (i = 0; i < new_list->size; i++)
      {
         const char *file_name = NULL;
         const char *file_path = new_list->elems[i].data;

         if (string_is_empty(file_path))
            continue;

         /* If a shader file name has been provided,
          * search the list for a match and set 'selection'
          * index if found */
         file_name = path_basename(file_path);

         if (!string_is_empty(file_name) &&
               string_is_equal(file_name, shader_file_name))
         {
            RARCH_LOG("[Shaders]: %s \"%s\"\n",
                  msg_hash_to_str(MSG_FOUND_SHADER),
                  file_path);

            dir_list->selection = i;
            break;
         }
      }
   }

   return true;
}
#endif

void get_current_menu_value(struct menu_state *menu_st,
      char *s, size_t len)
{
   menu_entry_t     entry;
   const char*      entry_label;

   MENU_ENTRY_INIT(entry);
   entry.path_enabled          = false;
   entry.label_enabled         = false;
   entry.rich_label_enabled    = false;
   entry.sublabel_enabled      = false;
   menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);

   if (entry.enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
      entry_label              = entry.password_value;
   else
      entry_label              = entry.value;

   strlcpy(s, entry_label, len);
}

void get_current_menu_label(struct menu_state *menu_st,
      char *s, size_t len)
{
   menu_entry_t     entry;
   const char*      entry_label;

   MENU_ENTRY_INIT(entry);
   menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);

   if (!string_is_empty(entry.rich_label))
      entry_label              = entry.rich_label;
   else
      entry_label              = entry.path;

   strlcpy(s, entry_label, len);
}

void get_current_menu_sublabel(struct menu_state *menu_st,
      char *s, size_t len)
{
   menu_entry_t     entry;

   MENU_ENTRY_INIT(entry);
   entry.path_enabled          = false;
   entry.label_enabled         = false;
   entry.rich_label_enabled    = false;
   entry.value_enabled         = false;
   menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);
   strlcpy(s, entry.sublabel, len);
}

void menu_entries_get_last_stack(const char **path, const char **label,
      unsigned *file_type, enum msg_hash_enums *enum_idx, size_t *entry_idx)
{
   file_list_t *list              = NULL;
   struct menu_state    *menu_st  = menu_state_get_ptr();
   if (!menu_st->entries.list)
      return;

   list                           = MENU_LIST_GET(menu_st->entries.list, 0);

   if (list && list->size)
      file_list_get_at_offset(list, list->size - 1, path, label, file_type, entry_idx);

   if (enum_idx)
   {
      menu_file_list_cbs_t *cbs  = (menu_file_list_cbs_t*)
         list->list[list->size - 1].actiondata;

      if (cbs)
         *enum_idx = cbs->enum_idx;
   }
}

int menu_driver_deferred_push_content_list(file_list_t *list)
{
   settings_t *settings           = config_get_ptr();
   struct menu_state    *menu_st  = menu_state_get_ptr();
   menu_list_t *menu_list         = menu_st->entries.list;
   file_list_t *selection_buf     = MENU_LIST_GET_SELECTION(menu_list, (unsigned)0);

   menu_st->selection_ptr         = 0;

   if (!menu_driver_displaylist_push(
            menu_st,
            settings,
            list,
            selection_buf))
      return -1;
   return 0;
}

bool menu_driver_screensaver_supported(void)
{
   struct menu_state    *menu_st  = menu_state_get_ptr();
   return menu_st->screensaver_supported;
}

retro_time_t menu_driver_get_current_time(void)
{
   struct menu_state    *menu_st  = menu_state_get_ptr();
   return menu_st->current_time_us;
}

const char *menu_driver_get_pending_selection(void)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   return menu_st->pending_selection;
}

void menu_driver_set_pending_selection(const char *pending_selection)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   char *selection             = menu_st->pending_selection;

   /* Reset existing cache */
   selection[0] = '\0';

   /* If path is empty, do nothing */
   if (string_is_empty(pending_selection))
      return;

   strlcpy(selection, pending_selection,
         sizeof(menu_st->pending_selection));
}

void menu_input_search_cb(void *userdata, const char *str)
{
   const char *label           = NULL;
   unsigned type               = MENU_SETTINGS_NONE;
   struct menu_state *menu_st  = menu_state_get_ptr();

   if (string_is_empty(str))
      goto end;

   /* Determine whether we are currently
    * viewing a menu list with 'search
    * filter' support */
   file_list_get_last(MENU_LIST_GET(menu_st->entries.list, 0),
         NULL, &label, &type, NULL);

   /* Do not apply search filter if string
    * consists of a single Latin alphabet
    * character */
   if (((str[1] != '\0') || (!ISALPHA(str[0]))) &&
       menu_driver_search_filter_enabled(label, type))
   {
      /* Add search term */
      if (menu_entries_search_push(str))
      {
         bool refresh = false;

         /* Reset navigation pointer */
         menu_st->selection_ptr = 0;
         menu_driver_navigation_set(false);

         /* Refresh menu */
         menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
         menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
      }
   }
   /* Perform a regular search: jump to the
    * first matching entry */
   else
   {
      size_t idx = 0;

      if (menu_entries_list_search(str, &idx))
      {
         menu_st->selection_ptr = idx;
         menu_driver_navigation_set(true);
      }
   }

end:
   menu_input_dialog_end();
}
