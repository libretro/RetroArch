/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018-2020 - natinusala
 *  Copyright (C) 2019      - Patrick Scheurenbrand
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

#include "ozone.h"
#include "ozone_display.h"
#include "ozone_theme.h"
#include "ozone_texture.h"
#include "ozone_sidebar.h"

#if 0
#include "discord/discord.h"
#endif

#include <file/file_path.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>
#include <formats/image.h>
#include <math/float_minmax.h>

#include "../../../gfx/gfx_animation.h"
#include "../../../gfx/gfx_display.h"
#include "../../runtime_file.h"

#include "../../input/input_osk.h"

#include "../../../configuration.h"
#include "../../../content.h"
#include "../../../core_info.h"
#include "../../../verbosity.h"

static const char *OZONE_TEXTURES_FILES[OZONE_TEXTURE_LAST] = {
   "retroarch",
   "cursor_border"
};

static const char *OZONE_TAB_TEXTURES_FILES[OZONE_TAB_TEXTURE_LAST] = {
   "retroarch",
   "settings",
   "history",
   "favorites",
   "music",
   "video",
   "image",
   "netplay",
   "add"
};

static void ozone_set_thumbnail_content(void *data, const char *s)
{
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return;

   if (ozone->is_playlist)
   {
      /* Playlist content */
      if (string_is_empty(s))
      {
         size_t selection      = menu_navigation_get_selection();
         size_t list_size      = menu_entries_get_size();
         file_list_t *list     = menu_entries_get_selection_buf_ptr(0);

         /* Get playlist index corresponding
          * to the selected entry */
         if (list &&
             (selection < list_size) &&
             (list->list[selection].type == FILE_TYPE_RPL_ENTRY))
         {
            size_t playlist_index = list->list[selection].entry_idx;
            gfx_thumbnail_set_content_playlist(ozone->thumbnail_path_data,
                  playlist_get_cached(), playlist_index);
         }
         else
            gfx_thumbnail_set_content_playlist(ozone->thumbnail_path_data,
                  NULL, selection);
      }
   }
   else if (ozone->is_db_manager_list)
   {
      /* Database list content */
      if (string_is_empty(s))
      {
         menu_entry_t entry;
         size_t selection         = menu_navigation_get_selection();

         MENU_ENTRY_INIT(entry);
         entry.label_enabled      = false;
         entry.rich_label_enabled = false;
         entry.value_enabled      = false;
         entry.sublabel_enabled   = false;
         menu_entry_get(&entry, 0, selection, NULL, true);

         if (!string_is_empty(entry.path))
            gfx_thumbnail_set_content(ozone->thumbnail_path_data, entry.path);
      }
   }
   else if (string_is_equal(s, "imageviewer"))
   {
      /* Filebrowser image updates */
      menu_entry_t entry;
      size_t selection           = menu_navigation_get_selection();
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
      ozone_node_t *node         = (ozone_node_t*)selection_buf->list[selection].userdata;

      if (node)
      {
         MENU_ENTRY_INIT(entry);
         entry.label_enabled      = false;
         entry.rich_label_enabled = false;
         entry.value_enabled      = false;
         entry.sublabel_enabled   = false;
         menu_entry_get(&entry, 0, selection, NULL, true);
         if (!string_is_empty(entry.path) && !string_is_empty(node->fullpath))
            gfx_thumbnail_set_content_image(ozone->thumbnail_path_data, node->fullpath, entry.path);
      }
   }
   else if (!string_is_empty(s))
   {
      /* Annoying leftovers...
       * This is required to ensure that thumbnails are
       * updated correctly when navigating deeply through
       * the sublevels of database manager lists.
       * Showing thumbnails on database entries is a
       * pointless nuisance and a waste of CPU cycles, IMHO... */
      gfx_thumbnail_set_content(ozone->thumbnail_path_data, s);
   }

   ozone_update_content_metadata(ozone);
}

/* Returns true if specified category is currently
 * displayed on screen */
static bool INLINE ozone_category_onscreen(
      ozone_handle_t *ozone, size_t idx)
{
   return (idx >= ozone->first_onscreen_category) &&
          (idx <= ozone->last_onscreen_category);
}

/* If current category is on screen, returns its
 * index. If current category is off screen, returns
 * index of centremost on screen category. */
static size_t ozone_get_onscreen_category_selection(
      ozone_handle_t *ozone)
{
   /* Check whether selected category is already on screen */
   if (ozone_category_onscreen(ozone, ozone->categories_selection_ptr))
      return ozone->categories_selection_ptr;

   /* Return index of centremost category */
   return (ozone->first_onscreen_category >> 1) +
         (ozone->last_onscreen_category >> 1);
}

/* Returns true if specified entry is currently
 * displayed on screen */
/* Check whether selected item is already on screen */
#define OZONE_ENTRY_ONSCREEN(ozone, idx) (((idx) >= (ozone)->first_onscreen_entry) && ((idx) <= (ozone)->last_onscreen_entry))

/* If currently selected entry is off screen,
 * moves selection to specified on screen target
 * > Does nothing if currently selected item is
 *   already on screen */
static void ozone_auto_select_onscreen_entry(
      ozone_handle_t *ozone,
      enum ozone_onscreen_entry_position_type target_entry)
{
   size_t selection = 0;

   /* Update selection index */
   switch (target_entry)
   {
      case OZONE_ONSCREEN_ENTRY_FIRST:
         selection = ozone->first_onscreen_entry;
         break;
      case OZONE_ONSCREEN_ENTRY_LAST:
         selection = ozone->last_onscreen_entry;
         break;
      case OZONE_ONSCREEN_ENTRY_CENTRE:
      default:
         selection = (ozone->first_onscreen_entry >> 1) +
               (ozone->last_onscreen_entry >> 1);
         break;
   }

   /* Apply new selection */
   menu_navigation_set_selection(selection);
}

static bool INLINE ozone_metadata_override_available(ozone_handle_t *ozone)
{
   /* Ugly construct...
    * Content metadata display override may be
    * toggled if the following are true:
    * - We are viewing playlist thumbnails
    * - This is *not* an image viewer playlist
    * - Both right and left thumbnails are
    *   enabled/available
    * Short circuiting means that in most cases
    * only 'ozone->is_playlist' will be evaluated,
    * so this isn't too much of a performance hog... */
   return ozone->is_playlist
      &&  ozone->show_thumbnail_bar 
      && !ozone->selection_core_is_viewer 
      && (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_MISSING)
      && gfx_thumbnail_is_enabled(ozone->thumbnail_path_data,
            GFX_THUMBNAIL_LEFT)
      && (ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_MISSING) 
      && gfx_thumbnail_is_enabled(ozone->thumbnail_path_data,
            GFX_THUMBNAIL_RIGHT);
}

static bool ozone_is_current_entry_settings(size_t current_selection)
{
   menu_entry_t last_entry;
   const char *entry_value;

   unsigned entry_type                = 0;
   enum msg_file_type entry_file_type = FILE_TYPE_NONE;

   MENU_ENTRY_INIT(last_entry);
   last_entry.path_enabled       = false;
   last_entry.label_enabled      = false;
   last_entry.rich_label_enabled = false;
   last_entry.sublabel_enabled   = false;

   menu_entry_get(&last_entry, 0, current_selection, NULL, true);

   if (last_entry.enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
      entry_value = last_entry.password_value;
   else
      entry_value = last_entry.value;

   entry_file_type = msg_hash_to_file_type(msg_hash_calculate(entry_value));
   entry_type      = last_entry.type;

   /* Logic below taken from materialui_pointer_up_swipe_horz_default */
   if (!string_is_empty(entry_value))
   {
      /* Toggle switch off */
      if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
          string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
      {
         return true;
      }
      /* Toggle switch on */
      else if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
               string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)))
      {
         return true;
      }
      /* Normal value text */
      else
      {
         switch (entry_file_type)
         {
            case FILE_TYPE_IN_CARCHIVE:
            case FILE_TYPE_MORE:
            case FILE_TYPE_CORE:
            case FILE_TYPE_DIRECT_LOAD:
            case FILE_TYPE_RDB:
            case FILE_TYPE_CURSOR:
            case FILE_TYPE_PLAIN:
            case FILE_TYPE_DIRECTORY:
            case FILE_TYPE_MUSIC:
            case FILE_TYPE_IMAGE:
            case FILE_TYPE_MOVIE:
               break;
            case FILE_TYPE_COMPRESSED:
               /* Note that we have to perform a backup check here,
                * since the 'manual content scan - file extensions'
                * setting may have a value of 'zip' or '7z' etc, which
                * means it would otherwise get incorreclty identified as
                * an archive file... */
               if (entry_type != FILE_TYPE_CARCHIVE)
                  return true;
               break;
            default:
               return true;
               break;
         }
      }
   }

   return false;
}

static enum menu_action ozone_parse_menu_entry_action(
      ozone_handle_t *ozone, settings_t *settings,
      enum menu_action action)
{
   uintptr_t tag;
   int new_selection;
   enum menu_action new_action   = action;
   file_list_t *selection_buf    = NULL;
   unsigned horizontal_list_size = 0;
   bool menu_navigation_wraparound_enable;
   bool is_current_entry_settings;
   size_t selection;
   size_t selection_total;

   /* If fullscreen thumbnail view is active, any
    * valid menu action will disable it... */
   if (ozone->show_fullscreen_thumbnails)
   {
      if (action != MENU_ACTION_NOOP)
      {
         ozone_hide_fullscreen_thumbnails(ozone, true);

         /* ...and any action other than Select/OK
          * is ignored
          * > We allow pass-through of Select/OK since
          *   users may want to run content directly
          *   after viewing fullscreen thumbnails,
          *   and having to press RetroPad A or the Return
          *   key twice is navigationally confusing
          * > Note that we can only do this for non-pointer
          *   input
          * > Note that we don't do this when viewing a
          *   file list, since there is no quick menu
          *   in this case - i.e. content loads directly,
          *   and a sudden transition from fullscreen
          *   thumbnail to content is jarring...
          * > We also don't do this when viewing a database
          *   manager list, because the menu transition
          *   detection becomes too cumbersome... */
         if (ozone->is_file_list ||
             ozone->is_db_manager_list ||
             ((action != MENU_ACTION_SELECT) &&
              (action != MENU_ACTION_OK)))
            return MENU_ACTION_NOOP;
      }
   }

   horizontal_list_size       = (unsigned)ozone->horizontal_list.size;

   ozone->messagebox_state    = menu_input_dialog_get_display_kb();
   selection_buf              = menu_entries_get_selection_buf_ptr(0);
   tag                        = (uintptr_t)selection_buf;
   selection                  = menu_navigation_get_selection();
   selection_total            = menu_entries_get_size();

   menu_navigation_wraparound_enable   = settings->bools.menu_navigation_wraparound_enable;

   /* Don't wiggle left or right if the current entry is a setting. This is
      partially wrong because some settings don't use left and right to change their value, such as
      free input fields (passwords...). This is good enough. */
   is_current_entry_settings = ozone_is_current_entry_settings(selection);

   /* Scan user inputs */
   switch (action)
   {
      case MENU_ACTION_START:
         ozone->cursor_mode = false;
         /* If this is a menu with thumbnails and cursor
          * is not in the sidebar, attempt to show
          * fullscreen thumbnail view */
         if (ozone->fullscreen_thumbnails_available &&
             !ozone->cursor_in_sidebar)
         {
            ozone_show_fullscreen_thumbnails(ozone);
            new_action = MENU_ACTION_NOOP;
         }
         break;
      case MENU_ACTION_DOWN:
         if (ozone->cursor_in_sidebar)
         {
            /* If cursor is active, ensure we target
             * an on screen category */
            size_t selection = (ozone->cursor_mode)
               ? ozone_get_onscreen_category_selection(ozone)
               : ozone->categories_selection_ptr;

            new_selection    = (int)(selection + 1);

            if (new_selection >= (int)(ozone->system_tab_end
                     + horizontal_list_size + 1))
               new_selection = 0;

            ozone_sidebar_goto(ozone, new_selection);

            new_action         = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            ozone->cursor_mode = false;
            break;
         }
         else if (!menu_navigation_wraparound_enable && selection == selection_total - 1)
         {
            ozone_start_cursor_wiggle(ozone, MENU_ACTION_DOWN);
         }

         /* If pointer is active and current selection
          * is off screen, auto select *centre* item */
         if (ozone->cursor_mode)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_CENTRE);
         ozone->cursor_mode = false;
         break;
      case MENU_ACTION_UP:
         if (ozone->cursor_in_sidebar)
         {
            /* If cursor is active, ensure we target
             * an on screen category */
            size_t selection   = (ozone->cursor_mode)
               ? ozone_get_onscreen_category_selection(ozone)
               : ozone->categories_selection_ptr;
            new_selection      = (int)selection - 1;

            if (new_selection < 0)
               new_selection   = horizontal_list_size + ozone->system_tab_end;

            ozone_sidebar_goto(ozone, new_selection);

            new_action         = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            ozone->cursor_mode = false;
            break;
         }
         else if (!menu_navigation_wraparound_enable && selection == 0)
         {
            ozone_start_cursor_wiggle(ozone, MENU_ACTION_UP);
         }

         /* If pointer is active and current selection
          * is off screen, auto select *centre* item */
         if (ozone->cursor_mode)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_CENTRE);
         ozone->cursor_mode = false;
         break;
      case MENU_ACTION_LEFT:
         ozone->cursor_mode = false;

         if (ozone->cursor_in_sidebar)
         {
            new_action      = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            ozone_start_cursor_wiggle(ozone, MENU_ACTION_LEFT);

            break;
         }
         else if (ozone->depth > 1)
         {
            if (!menu_navigation_wraparound_enable && selection == 0 && !is_current_entry_settings)
            {
               /* Pressing left goes up but faster, so
                  wiggle up to say that there is nothing more upwards
                  even though the user pressed the left button */
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_DOWN);
            }

            break;
         }

         ozone_go_to_sidebar(ozone, settings, tag);

         new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
         break;
      case MENU_ACTION_RIGHT:
         ozone->cursor_mode = false;
         if (!ozone->cursor_in_sidebar)
         {
            if (ozone->depth == 1)
            {
               new_action = MENU_ACTION_NOOP;
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_RIGHT);
            }
            else if (!menu_navigation_wraparound_enable && selection == selection_total - 1 && !is_current_entry_settings)
            {
               /* Pressing right goes down but faster, so
                  wiggle down to say that there is nothing more downwards
                  even though the user pressed the right button */
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_DOWN);
            }

            break;
         }

         ozone_leave_sidebar(ozone, settings, tag);

         new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_LABEL;
         break;
      case MENU_ACTION_OK:
         ozone->cursor_mode = false;
         if (ozone->cursor_in_sidebar)
         {
            ozone_leave_sidebar(ozone, settings, tag);
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_LABEL;
            break;
         }
         break;
      case MENU_ACTION_CANCEL:
         ozone->cursor_mode = false;

         /* If this is a playlist, handle 'backing out'
          * of a search, if required */
         if (ozone->is_playlist)
            if (menu_entries_search_get_terms())
               break;

         if (ozone->cursor_in_sidebar)
         {
            /* Go back to main menu tab */
            if (ozone->categories_selection_ptr != 0)
               ozone_sidebar_goto(ozone, 0);

            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            break;
         }

         if (menu_entries_get_stack_size(0) == 1)
         {
            ozone_go_to_sidebar(ozone, settings, tag);
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
         }
         break;

      case MENU_ACTION_SCROLL_UP:
         /* Descend alphabet (Z towards A) */

         /* Ignore if cursor is in sidebar */
         if (ozone->cursor_in_sidebar)
         {
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            break;
         }

         /* If pointer is active and current selection
          * is off screen, auto select *last* item */
         if (ozone->cursor_mode)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_LAST);
         ozone->cursor_mode = false;
         break;
      case MENU_ACTION_SCROLL_DOWN:
         /* Ascend alphabet (A towards Z) */

         /* > Ignore if cursor is in sidebar */
         if (ozone->cursor_in_sidebar)
         {
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            break;
         }

         /* If pointer is active and current selection
          * is off screen, auto select *first* item */
         if (ozone->cursor_mode)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_FIRST);
         ozone->cursor_mode = false;
         break;

      case MENU_ACTION_INFO:
         /* If we currently viewing a playlist with
          * dual thumbnails, toggle the content metadata
          * override */
         if (ozone_metadata_override_available(ozone))
         {
            ozone_toggle_metadata_override(ozone);
            new_action = MENU_ACTION_NOOP;
         }
         /* ...and since the user is likely to trigger
          * 'INFO' actions on invalid playlist entries,
          * suppress this action entirely when viewing
          * playlists under all other conditions
          * > Playlists have no 'INFO' entries - the
          *   user is just greeted with a useless
          *   'no information available' message
          * > It is incredibly annoying to inadvertently
          *   trigger this message when you just want to
          *   toggle metadata... */
         else if (ozone->is_playlist && ozone->show_thumbnail_bar)
            new_action = MENU_ACTION_NOOP;

         ozone->cursor_mode = false;
         break;

      default:
         /* In all other cases, pass through input
          * menu action without intervention */
         break;
   }

   return new_action;
}


/* Menu entry action callback */
static int ozone_menu_entry_action(
      void *userdata, menu_entry_t *entry,
      size_t i, enum menu_action action)
{
   menu_entry_t new_entry;
   ozone_handle_t *ozone       = (ozone_handle_t*)userdata;
   menu_entry_t *entry_ptr     = entry;
   settings_t *settings        = config_get_ptr();
   size_t selection            = i;
   /* Process input action */
   enum menu_action new_action = ozone_parse_menu_entry_action(ozone, settings,
         action);
   /* Check whether current selection has changed
    * (due to automatic on screen entry selection...) */
   size_t new_selection        = menu_navigation_get_selection();

   if (new_selection != selection)
   {
      /* Selection has changed - must update
       * entry pointer */
      MENU_ENTRY_INIT(new_entry);
      new_entry.path_enabled       = false;
      new_entry.label_enabled      = false;
      new_entry.rich_label_enabled = false;
      new_entry.value_enabled      = false;
      new_entry.sublabel_enabled   = false;
      menu_entry_get(&new_entry, 0, new_selection, NULL, true);
      entry_ptr                    = &new_entry;
   }

   /* Call standard generic_menu_entry_action() function */
   return generic_menu_entry_action(userdata, entry_ptr,
         new_selection, new_action);
}

static void ozone_free_node(ozone_node_t *node)
{
   if (!node)
      return;

   if (node->console_name)
      free(node->console_name);

   node->console_name = NULL;

   if (node->fullpath)
      free(node->fullpath);

   node->fullpath = NULL;

   free(node);
}

static void ozone_menu_animation_update_time(
      float *ticker_pixel_increment,
      unsigned video_width, unsigned video_height)
{
   gfx_display_t *p_disp      = disp_get_ptr();
   settings_t *settings       = config_get_ptr();
   /* Ozone uses DPI scaling
    * > Smooth ticker scaling multiplier is
    *   gfx_display_get_dpi_scale() multiplied by
    *   a small correction factor to achieve a
    *   default scroll speed equal to that of the
    *   non-smooth ticker */
   *(ticker_pixel_increment) *= gfx_display_get_dpi_scale(p_disp,
         settings, video_width, video_height) * 0.5f;
}

static void *ozone_init(void **userdata, bool video_is_threaded)
{
   unsigned i;
   bool fallback_color_theme           = false;
   unsigned width, height, color_theme = 0;
   ozone_handle_t *ozone               = NULL;
   settings_t *settings                = config_get_ptr();
   gfx_animation_t *p_anim             = anim_get_ptr();
   gfx_display_t *p_disp               = disp_get_ptr();
   menu_handle_t *menu                 = (menu_handle_t*)calloc(1, sizeof(*menu));
   const char *directory_assets        = settings->paths.directory_assets;

   if (!menu)
      return NULL;

   video_driver_get_size(&width, &height);

   ozone = (ozone_handle_t*)calloc(1, sizeof(ozone_handle_t));

   if (!ozone)
      goto error;

   *userdata = ozone;

   for (i = 0; i < 15; i++)
      ozone->pure_white[i]  = 1.00f;

   ozone->last_width        = width;
   ozone->last_height       = height;
   ozone->last_scale_factor = gfx_display_get_dpi_scale(p_disp,
         settings, width, height);

   file_list_initialize(&ozone->selection_buf_old);

   ozone->draw_sidebar                          = true;
   ozone->sidebar_offset                        = 0;
   ozone->pending_message                       = NULL;
   ozone->is_playlist                           = false;
   ozone->categories_selection_ptr              = 0;
   ozone->pending_message                       = NULL;
   ozone->show_cursor                           = false;
   ozone->show_screensaver                      = false;

   ozone->first_frame                           = true;
   ozone->cursor_mode                           = false;

   ozone->sidebar_collapsed                     = false;
   ozone->animations.sidebar_text_alpha         = 1.0f;

   ozone->animations.thumbnail_bar_position     = 0.0f;
   ozone->show_thumbnail_bar                    = false;
   ozone->dimensions_sidebar_width              = 0.0f;

   ozone->num_search_terms_old                  = 0;

   ozone->cursor_wiggle_state.wiggling          = false;

   ozone->thumbnail_path_data = gfx_thumbnail_path_init();
   if (!ozone->thumbnail_path_data)
      goto error;

   ozone->screensaver = menu_screensaver_init();
   if (!ozone->screensaver)
      goto error;

   ozone->fullscreen_thumbnails_available       = false;
   ozone->show_fullscreen_thumbnails            = false;
   ozone->animations.fullscreen_thumbnail_alpha = 0.0f;
   ozone->fullscreen_thumbnail_selection        = 0;
   ozone->fullscreen_thumbnail_label[0]         = '\0';

   ozone->animations.left_thumbnail_alpha       = 1.0f;
   ozone->force_metadata_display                = false;

   gfx_thumbnail_set_stream_delay(-1.0f);
   gfx_thumbnail_set_fade_duration(-1.0f);
   gfx_thumbnail_set_fade_missing(false);

   ozone_sidebar_update_collapse(ozone, settings, false);

   ozone->system_tab_end                        = 0;
   ozone->tabs[ozone->system_tab_end]           = OZONE_SYSTEM_TAB_MAIN;
   if (      settings->bools.menu_content_show_settings 
         && !settings->bools.kiosk_mode_enable)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_SETTINGS;
   if (settings->bools.menu_content_show_favorites)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_FAVORITES;
   if (settings->bools.menu_content_show_history)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_HISTORY;
#ifdef HAVE_IMAGEVIEWER
   if (settings->bools.menu_content_show_images)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_IMAGES;
#endif
   if (settings->bools.menu_content_show_music)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_MUSIC;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   if (settings->bools.menu_content_show_video)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_VIDEO;
#endif
#ifdef HAVE_NETWORKING
   if (settings->bools.menu_content_show_netplay)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_NETPLAY;
#endif

   if (      settings->bools.menu_content_show_add 
         && !settings->bools.kiosk_mode_enable)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_ADD;

#if defined(HAVE_LIBRETRODB)
   if (settings->bools.menu_content_show_explore)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_EXPLORE;
#endif

   menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   gfx_display_set_width(width);
   gfx_display_set_height(height);

   gfx_display_init_white_texture(gfx_display_white_texture);

   file_list_initialize(&ozone->horizontal_list);

   ozone_init_horizontal_list(ozone, settings);

   /* Theme */
   if (settings->bools.menu_use_preferred_system_color_theme)
   {
#ifdef HAVE_LIBNX
      if (R_SUCCEEDED(setsysInitialize()))
      {
         ColorSetId theme;
         setsysGetColorSetId(&theme);
         color_theme = (theme == ColorSetId_Dark) ? 1 : 0;
         ozone_set_color_theme(ozone, color_theme);
         configuration_set_uint(settings,
               settings->uints.menu_ozone_color_theme, color_theme);
         configuration_set_bool(settings,
               settings->bools.menu_preferred_system_color_theme_set, true);
         setsysExit();
      }
      else
#endif
         fallback_color_theme = true;
   }
   else
      fallback_color_theme    = true;

   if (fallback_color_theme)
   {
      color_theme = settings->uints.menu_ozone_color_theme;
      ozone_set_color_theme(ozone, color_theme);
   }

   ozone->need_compute                 = false;
   ozone->animations.scroll_y          = 0.0f;
   ozone->animations.scroll_y_sidebar  = 0.0f;

   ozone->first_onscreen_entry         = 0;
   ozone->last_onscreen_entry          = 0;
   ozone->first_onscreen_category      = 0;
   ozone->last_onscreen_category       = 0;

   /* Assets path */
   fill_pathname_join(
      ozone->assets_path,
      directory_assets,
      "ozone",
      sizeof(ozone->assets_path)
   );

   /* PNG path */
   fill_pathname_join(
      ozone->png_path,
      ozone->assets_path,
      "png",
      sizeof(ozone->png_path)
   );

   /* Sidebar path */
   fill_pathname_join(
      ozone->tab_path,
      ozone->png_path,
      "sidebar",
      sizeof(ozone->tab_path)
   );

   /* Icons path */
   fill_pathname_application_special(ozone->icons_path,
       sizeof(ozone->icons_path),
       APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE_ICONS);

   last_use_preferred_system_color_theme = settings->bools.menu_use_preferred_system_color_theme;
   p_anim->updatetime_cb = ozone_menu_animation_update_time;

   /* set word_wrap function pointer */
   ozone->word_wrap = msg_hash_get_wideglyph_str() ? word_wrap_wideglyph : word_wrap;

   return menu;

error:
   if (ozone)
   {
      ozone_free_list_nodes(&ozone->horizontal_list, false);
      ozone_free_list_nodes(&ozone->selection_buf_old, false);
      file_list_deinitialize(&ozone->horizontal_list);
      file_list_deinitialize(&ozone->selection_buf_old);
   }

   if (menu)
      free(menu);

   return NULL;
}

static void ozone_free(void *data)
{
   ozone_handle_t *ozone   = (ozone_handle_t*) data;

   if (ozone)
   {
      video_coord_array_free(&ozone->fonts.footer.raster_block.carr);
      video_coord_array_free(&ozone->fonts.title.raster_block.carr);
      video_coord_array_free(&ozone->fonts.time.raster_block.carr);
      video_coord_array_free(&ozone->fonts.entries_label.raster_block.carr);
      video_coord_array_free(&ozone->fonts.entries_sublabel.raster_block.carr);
      video_coord_array_free(&ozone->fonts.sidebar.raster_block.carr);

      ozone_free_list_nodes(&ozone->selection_buf_old, false);
      ozone_free_list_nodes(&ozone->horizontal_list, false);
      file_list_deinitialize(&ozone->selection_buf_old);
      file_list_deinitialize(&ozone->horizontal_list);

      if (!string_is_empty(ozone->pending_message))
         free(ozone->pending_message);

      if (ozone->thumbnail_path_data)
         free(ozone->thumbnail_path_data);

      menu_screensaver_free(ozone->screensaver);
   }

   if (gfx_display_white_texture)
      video_driver_texture_unload(&gfx_display_white_texture);

   font_driver_bind_block(NULL, NULL);
}

static void ozone_update_thumbnail_image(void *data)
{
   ozone_handle_t *ozone             = (ozone_handle_t*)data;
   size_t selection                  = menu_navigation_get_selection();
   settings_t *settings              = config_get_ptr();
   playlist_t *playlist              = playlist_get_cached();
   unsigned gfx_thumbnail_upscale_threshold = settings->uints.gfx_thumbnail_upscale_threshold;
   bool network_on_demand_thumbnails = settings->bools.network_on_demand_thumbnails;

   if (!ozone)
      return;

   gfx_thumbnail_cancel_pending_requests();

   gfx_thumbnail_request(
         ozone->thumbnail_path_data,
         GFX_THUMBNAIL_RIGHT,
         playlist,
         selection,
         &ozone->thumbnails.right,
         gfx_thumbnail_upscale_threshold,
         network_on_demand_thumbnails
         );

   /* Image (and video/music) content requires special
    * treatment... */
   if (ozone->selection_core_is_viewer)
   {
      /* Left thumbnail is simply reset */
      gfx_thumbnail_reset(&ozone->thumbnails.left);
   }
   else
   {
      /* Left thumbnail */
      gfx_thumbnail_request(
         ozone->thumbnail_path_data,
         GFX_THUMBNAIL_LEFT,
         playlist,
         selection,
         &ozone->thumbnails.left,
         gfx_thumbnail_upscale_threshold,
         network_on_demand_thumbnails);
   }
}

static void ozone_refresh_thumbnail_image(void *data, unsigned i)
{
   ozone_handle_t *ozone            = (ozone_handle_t*)data;

   if (!ozone)
      return;

   /* Only refresh thumbnails if thumbnails are enabled
    * and we are currently viewing a playlist */
   if ((gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
        gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT)) &&
       (ozone->is_playlist && ozone->depth == 1))
      ozone_update_thumbnail_image(ozone);
}

static bool ozone_init_font(
      ozone_font_data_t *font_data,
      bool is_threaded, char *font_path, float font_size)
{
   int glyph_width       = 0;
   gfx_display_t *p_disp = disp_get_ptr();
   const char *wideglyph_str = msg_hash_get_wideglyph_str();

   /* Free existing */
   if (font_data->font)
   {
      gfx_display_font_free(font_data->font);
      font_data->font = NULL;
   }

   /* Cache approximate dimensions */
   font_data->line_height = (int)(font_size + 0.5f);
   font_data->glyph_width = (int)((font_size * (3.0f / 4.0f)) + 0.5f);

   /* Create font */
   font_data->font = gfx_display_font_file(p_disp, 
         font_path, font_size, is_threaded);

   if (!font_data->font)
      return false;

   /* Get font metadata */
   glyph_width = font_driver_get_message_width(font_data->font, "a", 1, 1.0f);
   if (glyph_width > 0)
      font_data->glyph_width     = glyph_width;

   font_data->wideglyph_width = 100;

   if (wideglyph_str)
   {
      int wideglyph_width =
         font_driver_get_message_width(font_data->font, wideglyph_str, strlen(wideglyph_str), 1.0f);
      
      if (wideglyph_width > 0 && glyph_width > 0) 
         font_data->wideglyph_width = wideglyph_width * 100 / glyph_width;
   }

   font_data->line_height        = font_driver_get_line_height(font_data->font, 1.0f);
   font_data->line_ascender      = font_driver_get_line_ascender(font_data->font, 1.0f);
   font_data->line_centre_offset = font_driver_get_line_centre_offset(font_data->font, 1.0f);

   return true;
}

static void ozone_cache_footer_label(ozone_handle_t *ozone,
      ozone_footer_label_t *label, enum msg_hash_enums enum_idx)
{
   const char *str = msg_hash_to_str(enum_idx);
   /* Determine pixel width */
   unsigned length = (unsigned)strlen(str);

   /* Assign string */
   label->str      = str;
   label->width    = font_driver_get_message_width(
         ozone->fonts.footer.font,
         label->str, length, 1.0f);
   /* If font_driver_get_message_width() fails,
    * use predetermined glyph_width as a fallback */
   if (label->width < 0)
      label->width = length * ozone->fonts.footer.glyph_width;
}

/* Assigns footer label strings (based on current
 * menu language) and calculates pixel widths */
static void ozone_cache_footer_labels(ozone_handle_t *ozone)
{
   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.ok,
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK);

   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.back,
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK);

   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.search,
         MENU_ENUM_LABEL_VALUE_SEARCH);

   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.fullscreen_thumbs,
         MSG_TOGGLE_FULLSCREEN_THUMBNAILS);

   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.metadata_toggle,
         MSG_TOGGLE_CONTENT_METADATA);

   /* Record current language setting */
   ozone->footer_labels_language = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
}

/* Determines the size of all menu elements */
static void ozone_set_layout(
      ozone_handle_t *ozone, 
      settings_t *settings,
      bool is_threaded)
{
   char s1[PATH_MAX_LENGTH];
   char font_path[PATH_MAX_LENGTH];
   float scale_factor  = 0.0f;
   bool font_inited    = false;

   font_path[0] = s1[0]= '\0';

   if (!ozone)
      return;

   scale_factor = ozone->last_scale_factor;

   /* Calculate dimensions */
   ozone->dimensions.header_height                 = HEADER_HEIGHT * scale_factor;
   ozone->dimensions.footer_height                 = FOOTER_HEIGHT * scale_factor;

   ozone->dimensions.entry_padding_horizontal_half = ENTRY_PADDING_HORIZONTAL_HALF * scale_factor;
   ozone->dimensions.entry_padding_horizontal_full = ENTRY_PADDING_HORIZONTAL_FULL * scale_factor;
   ozone->dimensions.entry_padding_vertical        = ENTRY_PADDING_VERTICAL * scale_factor;
   ozone->dimensions.entry_height                  = ENTRY_HEIGHT * scale_factor;
   ozone->dimensions.entry_spacing                 = ENTRY_SPACING * scale_factor;
   ozone->dimensions.entry_icon_size               = ENTRY_ICON_SIZE * scale_factor;
   ozone->dimensions.entry_icon_padding            = ENTRY_ICON_PADDING * scale_factor;

   ozone->dimensions.sidebar_entry_height           = SIDEBAR_ENTRY_HEIGHT * scale_factor;
   ozone->dimensions.sidebar_padding_horizontal     = SIDEBAR_X_PADDING * scale_factor;
   ozone->dimensions.sidebar_padding_vertical       = SIDEBAR_Y_PADDING * scale_factor;
   ozone->dimensions.sidebar_entry_padding_vertical = SIDEBAR_ENTRY_Y_PADDING * scale_factor;
   ozone->dimensions.sidebar_entry_icon_size        = SIDEBAR_ENTRY_ICON_SIZE * scale_factor;
   ozone->dimensions.sidebar_entry_icon_padding     = SIDEBAR_ENTRY_ICON_PADDING * scale_factor;
   ozone->dimensions.sidebar_gradient_height        = SIDEBAR_GRADIENT_HEIGHT * scale_factor;

   ozone->dimensions.sidebar_width_normal             = SIDEBAR_WIDTH * scale_factor;
   ozone->dimensions.sidebar_width_collapsed          =
         ozone->dimensions.sidebar_entry_icon_size +
         ozone->dimensions.sidebar_entry_icon_padding * 2 +
         ozone->dimensions.sidebar_padding_horizontal * 2;

   if (ozone->dimensions_sidebar_width == 0)
      ozone->dimensions_sidebar_width = (float)ozone->dimensions.sidebar_width_normal;

   ozone->dimensions.thumbnail_bar_width          =
         ozone->dimensions.sidebar_width_normal -
         ozone->dimensions.sidebar_entry_icon_size -
         ozone->dimensions.sidebar_entry_icon_padding;

   ozone->dimensions.cursor_size                  = CURSOR_SIZE * scale_factor;

   ozone->dimensions.fullscreen_thumbnail_padding = FULLSCREEN_THUMBNAIL_PADDING * scale_factor;

   ozone->dimensions.spacer_1px = (scale_factor > 1.0f) ? (unsigned)(scale_factor + 0.5f) : 1;
   ozone->dimensions.spacer_2px = ozone->dimensions.spacer_1px * 2;
   ozone->dimensions.spacer_3px = (unsigned)((scale_factor * 3.0f) + 0.5f);
   ozone->dimensions.spacer_5px = (unsigned)((scale_factor * 5.0f) + 0.5f);

   /* Determine movement delta size for activating
    * pointer input (note: not a dimension as such,
    * so not included in the 'dimensions' struct) */
   ozone->pointer_active_delta = CURSOR_ACTIVE_DELTA * scale_factor;

   /* Initialise fonts */
   switch (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE))
   {
      case RETRO_LANGUAGE_ARABIC:
      case RETRO_LANGUAGE_PERSIAN:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "chinese-fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_KOREAN:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "korean-fallback-font.ttf", sizeof(font_path));
         break;
      default:
         fill_pathname_join(font_path, ozone->assets_path, "bold.ttf", sizeof(font_path));
   }

   font_inited = ozone_init_font(&ozone->fonts.title,
         is_threaded, font_path, FONT_SIZE_TITLE * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   switch (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE))
   {
      case RETRO_LANGUAGE_ARABIC:
      case RETRO_LANGUAGE_PERSIAN:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "chinese-fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_KOREAN:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "korean-fallback-font.ttf", sizeof(font_path));
         break;
      default:
         fill_pathname_join(font_path, ozone->assets_path, "regular.ttf", sizeof(font_path));
   }

   font_inited = ozone_init_font(&ozone->fonts.footer,
         is_threaded, font_path, FONT_SIZE_FOOTER * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   font_inited = ozone_init_font(&ozone->fonts.time,
         is_threaded, font_path, FONT_SIZE_TIME * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   font_inited = ozone_init_font(&ozone->fonts.entries_label,
         is_threaded, font_path, FONT_SIZE_ENTRIES_LABEL * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   font_inited = ozone_init_font(&ozone->fonts.entries_sublabel,
         is_threaded, font_path, FONT_SIZE_ENTRIES_SUBLABEL * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   font_inited = ozone_init_font(&ozone->fonts.sidebar,
         is_threaded, font_path, FONT_SIZE_SIDEBAR * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   /* Cache footer text labels
    * > Fonts have been (re)initialised, so need
    *   to recalculate label widths */
   ozone_cache_footer_labels(ozone);

   /* Multiple sidebar parameters are set via animations
    * > ozone_refresh_sidebars() cancels any existing
    *   animations and 'force updates' the affected
    *   variables with newly scaled values */
   ozone_refresh_sidebars(ozone, settings, ozone->last_height);

   /* Entry dimensions must be recalculated after
    * updating menu layout */
   ozone->need_compute = true;
}

static void ozone_context_reset(void *data, bool is_threaded)
{
   unsigned i;
   ozone_handle_t *ozone = (ozone_handle_t*) data;
   settings_t *settings  = config_get_ptr();

   if (ozone)
   {
      ozone->has_all_assets = true;

      ozone_set_layout(ozone, settings, is_threaded);

      /* Textures init */
      for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
         filename[0] = '\0';
#if 0
         if (i == OZONE_TEXTURE_DISCORD_OWN_AVATAR && discord_avatar_is_ready())
            strlcpy(filename, discord_get_own_avatar(), sizeof(filename));
         else
#endif
            strlcpy(filename, OZONE_TEXTURES_FILES[i], sizeof(filename));

         strlcat(filename, FILE_PATH_PNG_EXTENSION, sizeof(filename));

#if 0
         if (i == OZONE_TEXTURE_DISCORD_OWN_AVATAR && discord_avatar_is_ready())
         {
            char buf[PATH_MAX_LENGTH];
            fill_pathname_application_special(buf,
               sizeof(buf),
               APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS);
            if (!gfx_display_reset_textures_list(filename, buf, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
               RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", ozone->png_path,
                     PATH_DEFAULT_SLASH(), filename);
         }
         else
         {
#endif
            if (!gfx_display_reset_textures_list(filename, ozone->png_path, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
               ozone->has_all_assets = false;
#if 0
         }
#endif
      }

      /* Sidebar textures */
      for (i = 0; i < OZONE_TAB_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];

         filename[0]        = '\0';
         strlcpy(filename,
               OZONE_TAB_TEXTURES_FILES[i], sizeof(filename));
         strlcat(filename, FILE_PATH_PNG_EXTENSION, sizeof(filename));

         if (!gfx_display_reset_textures_list(filename, ozone->tab_path, &ozone->tab_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
         {
            ozone->has_all_assets = false;
            RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", ozone->tab_path,
                  PATH_DEFAULT_SLASH(), filename);
         }
      }

      /* Theme textures */
      if (!ozone_reset_theme_textures(ozone))
         ozone->has_all_assets = false;

      /* Icons textures init */
      for (i = 0; i < OZONE_ENTRIES_ICONS_TEXTURE_LAST; i++)
      {
         if (!gfx_display_reset_textures_list(ozone_entries_icon_texture_path(i), ozone->icons_path, &ozone->icons_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
         {
            ozone->has_all_assets = false;
            RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", ozone->icons_path,
                  PATH_DEFAULT_SLASH(), ozone_entries_icon_texture_path(i));
         }
      }

      if (gfx_display_white_texture)
         video_driver_texture_unload(&gfx_display_white_texture);
      gfx_display_init_white_texture(gfx_display_white_texture);

      /* Horizontal list */
      ozone_context_reset_horizontal_list(ozone);

      /* State reset */
      ozone->fade_direction               = false;
      ozone->cursor_in_sidebar            = false;
      ozone->cursor_in_sidebar_old        = false;
      ozone->draw_old_list                = false;
      ozone->messagebox_state             = false;
      ozone->messagebox_state_old         = false;

      ozone->cursor_wiggle_state.wiggling = false;

      /* Animations */
      ozone->animations.cursor_alpha   = 1.0f;
      ozone->animations.scroll_y       = 0.0f;
      ozone->animations.list_alpha     = 1.0f;

      /* Missing assets message */
      if (!ozone->has_all_assets)
         runloop_msg_queue_push(msg_hash_to_str(MSG_MISSING_ASSETS), 1, 256, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      /* Thumbnails */
      ozone_update_thumbnail_image(ozone);

      /* TODO: update savestate thumbnail image */

      ozone_restart_cursor_animation(ozone);

      /* Screensaver */
      menu_screensaver_context_destroy(ozone->screensaver);
   }
   video_driver_monitor_reset();
}

static void ozone_collapse_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->draw_sidebar = false;
}

static void ozone_unload_thumbnail_textures(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!ozone)
      return;

   gfx_thumbnail_cancel_pending_requests();
   gfx_thumbnail_reset(&ozone->thumbnails.right);
   gfx_thumbnail_reset(&ozone->thumbnails.left);
}

static void INLINE ozone_font_free(ozone_font_data_t *font_data)
{
   if (font_data->font)
      gfx_display_font_free(font_data->font);

   font_data->font = NULL;
}

static void ozone_context_destroy(void *data)
{
   unsigned i;
   uintptr_t tag;
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone)
      return;

   /* Theme */
   ozone_unload_theme_textures(ozone);

   /* Icons */
   for (i = 0; i < OZONE_ENTRIES_ICONS_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->icons_textures[i]);

   /* Textures */
   for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->textures[i]);

   /* Icons */
   for (i = 0; i < OZONE_TAB_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->tab_textures[i]);

   /* Thumbnails */
   ozone_unload_thumbnail_textures(ozone);

   video_driver_texture_unload(&gfx_display_white_texture);

   /* Fonts */
   ozone_font_free(&ozone->fonts.footer);
   ozone_font_free(&ozone->fonts.title);
   ozone_font_free(&ozone->fonts.time);
   ozone_font_free(&ozone->fonts.entries_label);
   ozone_font_free(&ozone->fonts.entries_sublabel);
   ozone_font_free(&ozone->fonts.sidebar);

   tag = (uintptr_t) &ozone_default_theme;
   gfx_animation_kill_by_tag(&tag);

   /* Horizontal list */
   ozone_context_destroy_horizontal_list(ozone);

   /* Screensaver */
   menu_screensaver_context_destroy(ozone->screensaver);
}

static void *ozone_list_get_entry(void *data,
      enum menu_list_type type, unsigned i)
{
   size_t list_size        = 0;
   ozone_handle_t* ozone   = (ozone_handle_t*) data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         {
            file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
            list_size  = menu_entries_get_stack_size(0);
            if (i < list_size)
               return (void*)&menu_stack->list[i];
         }
         break;
      case MENU_LIST_HORIZONTAL:
         list_size = ozone->horizontal_list.size;
         if (i < list_size)
            return (void*)&ozone->horizontal_list.list[i];
         break;
      default:
         break;
   }

   return NULL;
}

static int ozone_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   int ret                = -1;
   core_info_list_t *list = NULL;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         {
            settings_t             *settings = config_get_ptr();
            bool menu_content_show_playlists = settings->bools.menu_content_show_playlists;
            bool kiosk_mode_enable           = settings->bools.kiosk_mode_enable;

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION_FAVORITES_DIR, 0, 0);

            core_info_get_list(&list);
            if (list->info_count > 0)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                     MENU_SETTING_ACTION, 0, 0);
            }

            if (menu_content_show_playlists)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                     msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                     MENU_ENUM_LABEL_PLAYLISTS_TAB,
                     MENU_SETTING_ACTION, 0, 0);

            if (frontend_driver_parse_drive_list(info->list, true) != 0)
               menu_entries_append_enum(info->list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0);

            if (!kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                     MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                     MENU_SETTING_ACTION, 0, 0);
            }

            info->need_push    = true;
            info->need_refresh = true;
            ret = 0;
         }
         break;
      case DISPLAYLIST_MAIN_MENU:
         {
            settings_t   *settings      = config_get_ptr();
            rarch_system_info_t *system = runloop_get_system_info();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            if (rarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL))
            {
               if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                        info->list,
                        MENU_ENUM_LABEL_CONTENT_SETTINGS,
                        PARSE_ACTION,
                        false);
               }
            }
            else
            {
               if (system->load_no_content)
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                        info->list,
                        MENU_ENUM_LABEL_START_CORE,
                        PARSE_ACTION,
                        false);
               }

#ifndef HAVE_DYNAMIC
               if (frontend_driver_has_fork())
#endif
               {
                  if (settings->bools.menu_show_load_core)
                  {
                     MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                           info->list,
                           MENU_ENUM_LABEL_CORE_LIST,
                           PARSE_ACTION,
                           false);
                  }
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
                     PARSE_ACTION,
                     false);

               if (menu_displaylist_has_subsystems())
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                        info->list,
                        MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS,
                        PARSE_ACTION,
                        false);
               }
            }

            if (settings->bools.menu_show_load_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_LOAD_DISC,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_dump_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_DUMP_DISC,
                     PARSE_ACTION,
                     false);
            }

            MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_ADD_CONTENT_LIST,
                  PARSE_ACTION,
                  false);
#ifdef HAVE_QT
            if (settings->bools.desktop_menu_enable)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_SHOW_WIMP,
                     PARSE_ACTION,
                     false);
            }
#endif
#if defined(HAVE_NETWORKING)
#if defined(HAVE_ONLINE_UPDATER)
            if (settings->bools.menu_show_online_updater && !settings->bools.kiosk_mode_enable)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_ONLINE_UPDATER,
                     PARSE_ACTION,
                     false);
            }
#endif
#endif
            if (!settings->bools.menu_content_show_settings && !string_is_empty(settings->paths.menu_content_show_settings_password))
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.kiosk_mode_enable && !string_is_empty(settings->paths.kiosk_mode_password))
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_information)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_INFORMATION_LIST,
                     PARSE_ACTION,
                     false);
            }

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
            MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_SWITCH_CPU_PROFILE,
                  PARSE_ACTION,
                  false);
#endif

#ifdef HAVE_LAKKA_SWITCH
            MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_SWITCH_GPU_PROFILE,
                  PARSE_ACTION,
                  false);
#endif

            if (settings->bools.menu_show_configurations && !settings->bools.kiosk_mode_enable)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_help)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_HELP_LIST,
                     PARSE_ACTION,
                     false);
            }

#if !defined(IOS)
            if (settings->bools.menu_show_restart_retroarch)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_RESTART_RETROARCH,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_quit_retroarch)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_QUIT_RETROARCH,
                     PARSE_ACTION,
                     false);
            }
#endif

            if (settings->bools.menu_show_reboot)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_REBOOT,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_shutdown)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_SHUTDOWN,
                     PARSE_ACTION,
                     false);
            }

            info->need_push    = true;
            ret = 0;
         }
         break;
   }
   return ret;
}

static size_t ozone_list_get_selection(void *data)
{
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return 0;

   return ozone->categories_selection_ptr;
}

static void ozone_list_clear(file_list_t *list)
{
   uintptr_t tag = (uintptr_t)list;
   gfx_animation_kill_by_tag(&tag);

   ozone_free_list_nodes(list, false);
}

static void ozone_list_free(file_list_t *list, size_t a, size_t b)
{
   ozone_list_clear(list);
}

static void ozone_render(void *data,
      unsigned width, unsigned height,
      bool is_idle)
{
   size_t i;
   float scale_factor;
   unsigned entries_end             = (unsigned)menu_entries_get_size();
   bool pointer_enabled             = false;
   unsigned language                = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
   ozone_handle_t *ozone            = (ozone_handle_t*)data;
   gfx_display_t *p_disp            = disp_get_ptr();
   gfx_animation_t          *p_anim = anim_get_ptr();
   settings_t             *settings = config_get_ptr();
   if (!ozone)
      return;

   /* Check whether screen dimensions or menu scale
    * factor have changed */
   scale_factor = gfx_display_get_dpi_scale(p_disp, settings, width, height);

   if ((scale_factor != ozone->last_scale_factor) ||
       (width != ozone->last_width) ||
       (height != ozone->last_height))
   {
      ozone->last_scale_factor = scale_factor;
      ozone->last_width        = width;
      ozone->last_height       = height;

      /* Note: We don't need a full context reset here
       * > Just rescale layout, and reset frame time counter */
      ozone_set_layout(ozone, settings, video_driver_is_threaded());
      video_driver_monitor_reset();
   }

   if (ozone->need_compute)
   {
      ozone_compute_entries_position(ozone, settings, entries_end);
      ozone->need_compute = false;
   }

   /* Check whether menu language has changed
    * > If so, need to re-cache footer text labels */
   if (ozone->footer_labels_language != language)
   {
      ozone->footer_labels_language = language;
      ozone_cache_footer_labels(ozone);
   }

   ozone->selection = menu_navigation_get_selection();

   /* Need to update this each frame, otherwise touchscreen
    * input breaks when changing orientation */
   p_disp->framebuf_width  = width;
   p_disp->framebuf_height = height;

   /* Read pointer state */
   menu_input_get_pointer_state(&ozone->pointer);

   /* If menu screensaver is active, update
    * screensaver and return */
   if (ozone->show_screensaver)
   {
      menu_screensaver_iterate(
            ozone->screensaver,
            p_disp, p_anim,
            (enum menu_screensaver_effect)settings->uints.menu_screensaver_animation,
            settings->floats.menu_screensaver_animation_speed,
            ozone->theme->screensaver_tint,
            width, height,
            settings->paths.directory_assets);
      GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
      return;
   }

   /* Check whether pointer is enabled */
   if (ozone->pointer.type != MENU_POINTER_DISABLED)
   {
      /* When using a mouse, entry under pointer is
       * automatically selected
       * > Must therefore filter out small movements,
       *   otherwise scrolling with the mouse wheel
       *   becomes impossible... */
      if (ozone->pointer.type == MENU_POINTER_MOUSE)
      {
         int16_t cursor_x_delta = ozone->pointer.x - ozone->cursor_x_old;
         int16_t cursor_y_delta = ozone->pointer.y - ozone->cursor_y_old;

         if ((cursor_x_delta > ozone->pointer_active_delta) ||
             (cursor_x_delta < -ozone->pointer_active_delta) ||
             (cursor_y_delta > ozone->pointer_active_delta) ||
             (cursor_y_delta < -ozone->pointer_active_delta))
            ozone->cursor_mode = true;
      }
      /* On touchscreens, just check for any movement */
      else
      {
         if ((ozone->pointer.x != ozone->cursor_x_old) ||
             (ozone->pointer.y != ozone->cursor_y_old))
            ozone->cursor_mode = true;
      }
   }

   ozone->cursor_x_old = ozone->pointer.x;
   ozone->cursor_y_old = ozone->pointer.y;

   /* Pointer is disabled when:
    * - Showing fullscreen thumbnails
    * - On-screen keyboard is active
    * - A message box is being displayed */
   pointer_enabled = ozone->cursor_mode &&
         !ozone->show_fullscreen_thumbnails &&
         !menu_input_dialog_get_display_kb() &&
         !ozone->should_draw_messagebox;

   /* Process pointer input, if required */
   if (pointer_enabled)
   {
      file_list_t *selection_buf  = menu_entries_get_selection_buf_ptr(0);
      uintptr_t     animation_tag = (uintptr_t)selection_buf;

      int entry_padding           = (ozone->depth == 1) ?
            ozone->dimensions.entry_padding_horizontal_half :
                  ozone->dimensions.entry_padding_horizontal_full;
      float entry_x               = ozone->dimensions_sidebar_width +
            ozone->sidebar_offset + entry_padding;
      float entry_width           = width - ozone->dimensions_sidebar_width -
            ozone->sidebar_offset - entry_padding * 2 -
            ozone->animations.thumbnail_bar_position;
      bool first_entry_found      = false;
      bool last_entry_found       = false;

      unsigned horizontal_list_size = (unsigned)ozone->horizontal_list.size;
      float category_height         = ozone->dimensions.sidebar_entry_height +
            ozone->dimensions.sidebar_entry_padding_vertical;
      bool first_category_found     = false;
      bool last_category_found      = false;

      /* Check whether pointer is operating on entries
       * or sidebar
       * > Note 1: Since touchscreens effectively 'lose their
       *   place' when a touch is released, we can only perform
       *   this this check if the pointer is currently
       *   pressed - i.e. we must preserve the values set the
       *   last time the screen was touched.
       *   With mouse input we have a permanent cursor, so this
       *   is not an issue
       * > Note 2: Windows seems to report negative pointer
       *   coordinates when the cursor goes off the left hand
       *   side of the screen/window, so checking whether
       *   pointer.x is less than the effective sidebar width
       *   generates a false positive when ozone->depth > 1.
       *   We therefore must also check whether the sidebar
       *   is currently being drawn */
      ozone->last_pointer_in_sidebar = ozone->pointer_in_sidebar;
      if ((ozone->pointer.type == MENU_POINTER_MOUSE) ||
           ozone->pointer.pressed)
         ozone->pointer_in_sidebar   = ozone->draw_sidebar &&
               (ozone->pointer.x < ozone->dimensions_sidebar_width 
                + ozone->sidebar_offset);

      /* If pointer has switched from entries to sidebar
       * or vice versa, must reset pointer acceleration */
      if (ozone->pointer_in_sidebar != ozone->last_pointer_in_sidebar)
      {
         menu_input_set_pointer_y_accel(0.0f);
         ozone->pointer.y_accel = 0.0f;
      }

      /* If pointer is a mouse, then automatically follow
       * mouse focus from entries to sidebar (and vice versa) */
      if (ozone->pointer.type == MENU_POINTER_MOUSE)
      {
         if (ozone->pointer_in_sidebar &&
             !ozone->last_pointer_in_sidebar &&
             !ozone->cursor_in_sidebar)
            ozone_go_to_sidebar(ozone, settings, animation_tag);
         else if (!ozone->pointer_in_sidebar &&
                  ozone->last_pointer_in_sidebar &&
                  ozone->cursor_in_sidebar)
            ozone_leave_sidebar(ozone, settings, animation_tag);
      }

      /* Update scrolling - must be done first, otherwise
       * cannot determine entry/category positions
       * > Entries */
      if (!ozone->pointer_in_sidebar)
      {
         float entry_bottom_boundary = height - ozone->dimensions.header_height -
               ozone->dimensions.spacer_1px - ozone->dimensions.footer_height -
               ozone->dimensions.entry_padding_vertical * 2;

         ozone->animations.scroll_y += ozone->pointer.y_accel;

         if (ozone->animations.scroll_y + ozone->entries_height < entry_bottom_boundary)
            ozone->animations.scroll_y = entry_bottom_boundary - ozone->entries_height;

         if (ozone->animations.scroll_y > 0.0f)
            ozone->animations.scroll_y = 0.0f;
      }
      /* > Sidebar
       * Only process sidebar input here if the
       * cursor is currently *in* the sidebar */
      else if (ozone->cursor_in_sidebar)
      {
         float sidebar_bottom_boundary = height -
               (ozone->dimensions.header_height + ozone->dimensions.spacer_1px) -
               ozone->dimensions.footer_height -
               ozone->dimensions.sidebar_padding_vertical;
         float sidebar_height          = ozone_get_sidebar_height(ozone);

         ozone->animations.scroll_y_sidebar += ozone->pointer.y_accel;

         if (ozone->animations.scroll_y_sidebar + sidebar_height < sidebar_bottom_boundary)
            ozone->animations.scroll_y_sidebar = sidebar_bottom_boundary - sidebar_height;

         if (ozone->animations.scroll_y_sidebar > 0.0f)
            ozone->animations.scroll_y_sidebar = 0.0f;
      }

      /* Regardless of pointer location, have to process
       * all entries/categories in order to determine
       * the indices of the first and last entries/categories
       * displayed on screen
       * > Needed so we can determine proper cursor positions
       *   when mixing pointer + gamepad/keyboard input */

      /* >> Loop over all entries */
      ozone->first_onscreen_entry = 0;
      ozone->last_onscreen_entry  = (entries_end > 0) ? entries_end - 1 : 0;

      for (i = 0; i < entries_end; i++)
      {
         float entry_y;
         ozone_node_t *node = (ozone_node_t*)selection_buf->list[i].userdata;

         /* Sanity check */
         if (!node)
            break;

         /* Get current entry y position */
         entry_y = ozone->dimensions.header_height + ozone->dimensions.spacer_1px +
               ozone->dimensions.entry_padding_vertical + ozone->animations.scroll_y +
               node->position_y;

         /* Check whether this is the first on screen entry */
         if (!first_entry_found)
         {
            if ((entry_y + node->height) > ozone->dimensions.header_height)
            {
               ozone->first_onscreen_entry = i;
               first_entry_found = true;
            }
         }
         /* Check whether this is the last on screen entry */
         else if (!last_entry_found)
         {
            if (entry_y > (height - ozone->dimensions.footer_height))
            {
               /* Current entry is off screen - get index
                * of previous entry */
               if (i > 0)
               {
                  ozone->last_onscreen_entry = i - 1;
                  last_entry_found = true;
               }
            }
         }

         /* Track pointer input, if required */
         if (!ozone->pointer_in_sidebar &&
             first_entry_found &&
             !last_entry_found)
         {
            /* Check whether pointer is within the bounds
             * of the current entry */
            if ((ozone->pointer.x > entry_x) &&
                (ozone->pointer.x < entry_x + entry_width) &&
                (ozone->pointer.y > entry_y) &&
                (ozone->pointer.y < entry_y + node->height))
            {
               /* Pointer selection is always updated */
               menu_input_set_pointer_selection((unsigned)i);

               /* If pointer is a mouse, then automatically
                * select entry under cursor */
               if (ozone->pointer.type == MENU_POINTER_MOUSE)
               {
                  /* Note the fudge factor - cannot auto select
                   * items while drag-scrolling the entry list,
                   * so have to wait until pointer acceleration
                   * drops below a 'sensible' level... */
                  if (!ozone->cursor_in_sidebar &&
                      (i != ozone->selection) &&
                      (ozone->pointer.y_accel < ozone->last_scale_factor) &&
                      (ozone->pointer.y_accel > -ozone->last_scale_factor))
                  {
                     menu_navigation_set_selection(i);

                     /* If this is a playlist, must update thumbnails */
                     if (ozone->is_playlist && (ozone->depth == 1))
                     {
                        ozone_set_thumbnail_content(ozone, "");
                        ozone_update_thumbnail_image(ozone);
                     }
                  }
               }

               /* If pointer is pressed and stationary, and
                * if pointer has been held for at least
                * MENU_INPUT_PRESS_TIME_SHORT ms, automatically
                * select current entry */
               if (ozone->pointer.pressed &&
                   !ozone->pointer.dragged &&
                   (ozone->pointer.press_duration >= MENU_INPUT_PRESS_TIME_SHORT) &&
                   (i != ozone->selection))
               {
                  menu_navigation_set_selection(i);

                  /* If we are currently in the sidebar, leave it */
                  if (ozone->cursor_in_sidebar)
                     ozone_leave_sidebar(ozone, settings, animation_tag);
                  /* If this is a playlist, must update thumbnails */
                  else if (ozone->is_playlist && (ozone->depth == 1))
                  {
                     ozone_set_thumbnail_content(ozone, "");
                     ozone_update_thumbnail_image(ozone);
                  }
               }
            }
         }

         if (last_entry_found)
            break;
      }

      /* >> Loop over all categories */
      ozone->first_onscreen_category = 0;
      ozone->last_onscreen_category  = ozone->system_tab_end + horizontal_list_size;

      for (i = 0; i < ozone->system_tab_end + horizontal_list_size + 1; i++)
      {
         /* Get current category y position */
         float category_y = ozone->dimensions.header_height + ozone->dimensions.spacer_1px +
               ozone->dimensions.sidebar_padding_vertical + (category_height * i) +
               ((i > ozone->system_tab_end) ?
                     (ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px) : 0) +
               ozone->animations.scroll_y_sidebar;

         /* Check whether this is the first on screen category */
         if (!first_category_found)
         {
            if ((category_y + category_height) > ozone->dimensions.header_height)
            {
               ozone->first_onscreen_category = i;
               first_category_found = true;
            }
         }
         /* Check whether this is the last on screen category */
         else if (!last_category_found)
         {
            if (category_y > (height - ozone->dimensions.footer_height))
            {
               /* Current category is off screen - get index
                * of previous category */
               if (i > 0)
               {
                  ozone->last_onscreen_category = i - 1;
                  last_category_found = true;
               }
            }
         }

         /* Track pointer input, if required */
         if (ozone->pointer_in_sidebar &&
             ozone->cursor_in_sidebar &&
             first_category_found &&
             !last_category_found)
         {
            /* If pointer is within the bounds of the
             * current category, cache category index
             * (for use in next 'pointer up' event) */
            if ((ozone->pointer.y > category_y) &&
                (ozone->pointer.y < category_y + category_height))
               ozone->pointer_categories_selection = i;
         }

         if (last_category_found)
            break;
      }
   }

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (i >= entries_end)
   {
      i = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   }

   GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
}

static void ozone_draw_header(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool battery_level_enable,
      bool timedate_enable)
{
   char title[255];
   gfx_animation_ctx_ticker_t ticker;
   gfx_animation_ctx_ticker_smooth_t ticker_smooth;
   static const char* const ticker_spacer = OZONE_TICKER_SPACER;
   unsigned ticker_x_offset  = 0;
   unsigned timedate_offset  = 0;
   bool use_smooth_ticker    = settings->bools.menu_ticker_smooth;
   float scale_factor        = ozone->last_scale_factor;
   unsigned logo_icon_size   = 60 * scale_factor;
   unsigned status_icon_size = 92 * scale_factor;
   unsigned seperator_margin = 30 * scale_factor;
   enum gfx_animation_ticker_type
      menu_ticker_type       = (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   float *col                        = ozone->theme->entries_icon;

   /* Initial ticker configuration */
   if (use_smooth_ticker)
   {
      ticker_smooth.idx           = p_anim->ticker_pixel_idx;
      ticker_smooth.font_scale    = 1.0f;
      ticker_smooth.type_enum     = menu_ticker_type;
      ticker_smooth.spacer        = ticker_spacer;
      ticker_smooth.x_offset      = &ticker_x_offset;
      ticker_smooth.dst_str_width = NULL;
   }
   else
   {
      ticker.idx                  = p_anim->ticker_idx;
      ticker.type_enum            = menu_ticker_type;
      ticker.spacer               = ticker_spacer;
   }

   /* Separator */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         seperator_margin,
         ozone->dimensions.header_height,
         video_width - seperator_margin * 2,
         ozone->dimensions.spacer_1px,
         video_width,
         video_height,
         ozone->theme->header_footer_separator);

   /* Title */
   if (use_smooth_ticker)
   {
      ticker_smooth.font        = ozone->fonts.title.font;
      ticker_smooth.selected    = true;
      ticker_smooth.field_width = (video_width - (128 + 47 + 180) * scale_factor);
      ticker_smooth.src_str     = ozone->show_fullscreen_thumbnails ? ozone->fullscreen_thumbnail_label : ozone->title;
      ticker_smooth.dst_str     = title;
      ticker_smooth.dst_str_len = sizeof(title);

      gfx_animation_ticker_smooth(&ticker_smooth);
   }
   else
   {
      ticker.s        = title;
      ticker.len      = (video_width - (128 + 47 + 180) * scale_factor) / ozone->fonts.title.glyph_width;
      ticker.str      = ozone->show_fullscreen_thumbnails ? ozone->fullscreen_thumbnail_label : ozone->title;
      ticker.selected = true;

      gfx_animation_ticker(&ticker);
   }

   gfx_display_draw_text(
         ozone->fonts.title.font,
         title,
         ticker_x_offset + 128 * scale_factor,
           ozone->dimensions.header_height / 2 
         + ozone->fonts.title.line_centre_offset,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* Icon */
   if (dispctx)
   {
      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      if (dispctx->draw)
      {
#if 0
         if (discord_avatar_is_ready())
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  logo_icon_size,
                  logo_icon_size,
                  ozone->textures[OZONE_TEXTURE_DISCORD_OWN_AVATAR],
                  47 * scale_factor,
                  14 * scale_factor, /* Where does this come from...? */
                  video_width,
                  video_height,
                  0, 1, col);
         else
#endif
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  logo_icon_size,
                  logo_icon_size,
                  ozone->textures[OZONE_TEXTURE_RETROARCH],
                  47 * scale_factor,
                  (ozone->dimensions.header_height - logo_icon_size) / 2,
                  video_width,
                  video_height,
                  0, 1, col);
      }
      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }

   /* Battery */
   if (battery_level_enable)
   {
      gfx_display_ctx_powerstate_t powerstate;
      char msg[12];

      msg[0] = '\0';

      powerstate.s   = msg;
      powerstate.len = sizeof(msg);

      menu_display_powerstate(&powerstate);

      if (powerstate.battery_enabled)
      {
         timedate_offset = 95 * scale_factor;

         gfx_display_draw_text(
               ozone->fonts.time.font,
               msg,
               video_width - 85 * scale_factor,
                 ozone->dimensions.header_height / 2 
               + ozone->fonts.time.line_centre_offset,
               video_width,
               video_height,
               ozone->theme->text_rgba,
               TEXT_ALIGN_RIGHT,
               1.0f,
               false,
               1.0f,
               false);

         if (dispctx)
         {
            if (dispctx->blend_begin)
               dispctx->blend_begin(userdata);
            if (dispctx->draw)
               ozone_draw_icon(
                     p_disp,
                     userdata,
                     video_width,
                     video_height,
                     status_icon_size,
                     status_icon_size,
                     ozone->icons_textures[powerstate.charging? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING : (powerstate.percent > 80)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL : (powerstate.percent > 60)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_80 : (powerstate.percent > 40)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_60 : (powerstate.percent > 20)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_40 : OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_20],
                     video_width - (60 + 56) * scale_factor,
                     0,
                     video_width,
                     video_height,
                     0, 1, col);
            if (dispctx->blend_end)
               dispctx->blend_end(userdata);
         }
      }
   }

   /* Timedate */
   if (timedate_enable)
   {
      gfx_display_ctx_datetime_t datetime;
      char timedate[255];

      timedate[0]             = '\0';

      datetime.s              = timedate;
      datetime.time_mode      = settings->uints.menu_timedate_style;
      datetime.date_separator = settings->uints.menu_timedate_date_separator;
      datetime.len            = sizeof(timedate);

      menu_display_timedate(&datetime);

      gfx_display_draw_text(
            ozone->fonts.time.font,
            timedate,
            video_width - (85 * scale_factor) - timedate_offset,
              ozone->dimensions.header_height / 2 
            + ozone->fonts.time.line_centre_offset,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_RIGHT,
            1.0f,
            false,
            1.0f,
            false);

      if (dispctx)
      {
         if (dispctx->blend_begin)
            dispctx->blend_begin(userdata);
         if (dispctx->draw)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  status_icon_size,
                  status_icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOCK],
                  video_width - (60 + 56) * scale_factor - timedate_offset,
                  0,
                  video_width,
                  video_height,
                  0, 1, col);
         if (dispctx->blend_end)
            dispctx->blend_end(userdata);
      }
   }
}

static void ozone_draw_footer(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool input_menu_swap_ok_cancel_buttons,
      settings_t *settings)
{
   bool menu_core_enable                  = settings->bools.menu_core_enable;
   float scale_factor                     = ozone->last_scale_factor;
   unsigned seperator_margin              = 30 * scale_factor;
   float footer_margin                    = 59 * scale_factor;
   float footer_text_y                    = (float)video_height -
         (ozone->dimensions.footer_height / 2.0f) +
         ozone->fonts.footer.line_centre_offset;
   float icon_size                        = 35 * scale_factor;
   float icon_padding                     = 12 * scale_factor;
   float icon_y                           = (float)video_height -
         (ozone->dimensions.footer_height / 2.0f) -
         (icon_size / 2.0f);
   /* Button enable states
    * > Note: Only show 'metadata_toggle' if
    *   'fullscreen_thumbs' is shown. This condition
    *   should be guaranteed anyway, but enforce it
    *   here to prevent 'gaps' in the button list in
    *   the event of unknown errors */
   bool fullscreen_thumbnails_available =
         ozone->fullscreen_thumbnails_available &&
         !ozone->cursor_in_sidebar &&
         ozone->show_thumbnail_bar &&
         ((ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_MISSING) ||
          (ozone->thumbnails.left.status  != GFX_THUMBNAIL_STATUS_MISSING)) &&
         (gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
          gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT));
   bool metadata_override_available     =
         fullscreen_thumbnails_available &&
         ozone_metadata_override_available(ozone);
   /* Determine x origin positions of each
    * button
    * > From right to left, these are ordered:
    *   - ok
    *   - back
    *   - search
    *   - toggle fullscreen thumbs (playlists only)
    *   - toggle metadata (playlists only) */
   float ok_x                = (float)video_width - footer_margin -
         ozone->footer_labels.ok.width - icon_size - icon_padding;
   float back_x              = ok_x -
         ozone->footer_labels.back.width - icon_size - (2.0f * icon_padding);
   float search_x            = back_x -
         ozone->footer_labels.search.width - icon_size - (2.0f * icon_padding);
   float fullscreen_thumbs_x = search_x -
         ozone->footer_labels.fullscreen_thumbs.width - icon_size - (2.0f * icon_padding);
   float metadata_toggle_x   = fullscreen_thumbs_x -
         ozone->footer_labels.metadata_toggle.width - icon_size - (2.0f * icon_padding);
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   float *col                        = ozone->theme_dynamic.entries_icon;

   /* Separator */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         seperator_margin,
         video_height - ozone->dimensions.footer_height,
         video_width - seperator_margin * 2,
         ozone->dimensions.spacer_1px,
         video_width,
         video_height,
         ozone->theme->header_footer_separator);

   /* Buttons */

   /* Draw icons */
   if (dispctx)
   {
      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      gfx_display_set_alpha(ozone->theme_dynamic.entries_icon, 1.0f);

      if (dispctx->draw)
      {
         /* > ok */
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               icon_size,
               icon_size,
               input_menu_swap_ok_cancel_buttons ?
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D] :
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R],
               ok_x,
               icon_y,
               video_width,
               video_height,
               0, 1, col);

         /* > back */
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               icon_size,
               icon_size,
               input_menu_swap_ok_cancel_buttons ?
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R] :
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D],
               back_x,
               icon_y,
               video_width,video_height,
               0, 1, col);

         /* > search */
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               icon_size,
               icon_size,
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U],
               search_x,
               icon_y,
               video_width,video_height,
               0, 1, col);

         /* > fullscreen_thumbs */
         if (fullscreen_thumbnails_available)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START],
                  fullscreen_thumbs_x,
                  icon_y,
                  video_width,video_height,
                  0, 1, col);

         /* > metadata_toggle */
         if (metadata_override_available)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT],
                  metadata_toggle_x,
                  icon_y,
                  video_width,video_height,
                  0, 1, col);
      }

      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }

   /* Draw labels */

   /* > ok */
   gfx_display_draw_text(
         ozone->fonts.footer.font,
         ozone->footer_labels.ok.str,
         ok_x + icon_size + icon_padding,
         footer_text_y,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* > back */
   gfx_display_draw_text(
         ozone->fonts.footer.font,
         ozone->footer_labels.back.str,
         back_x + icon_size + icon_padding,
         footer_text_y,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* > search */
   gfx_display_draw_text(
         ozone->fonts.footer.font,
         ozone->footer_labels.search.str,
         search_x + icon_size + icon_padding,
         footer_text_y,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* > fullscreen_thumbs */
   if (fullscreen_thumbnails_available)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.fullscreen_thumbs.str,
            fullscreen_thumbs_x + icon_size + icon_padding,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > metadata_toggle */
   if (metadata_override_available)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.metadata_toggle.str,
            metadata_toggle_x + icon_size + icon_padding,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* Core title or Switch icon */
   if (menu_core_enable)
   {
      gfx_animation_ctx_ticker_t ticker;
      gfx_animation_ctx_ticker_smooth_t ticker_smooth;
      char core_title[255];
      char core_title_buf[255];
      int usable_width;
      bool use_smooth_ticker                          =
            settings->bools.menu_ticker_smooth;
      enum gfx_animation_ticker_type menu_ticker_type =
            (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
            static const char* const ticker_spacer    = OZONE_TICKER_SPACER;
      unsigned ticker_x_offset                        = 0;

      core_title[0]     = '\0';
      core_title_buf[0] = '\0';

      /* Determine available width for core
       * title string */
      usable_width = metadata_override_available ?
            metadata_toggle_x :
                  fullscreen_thumbnails_available ?
                        fullscreen_thumbs_x :
                        search_x;
      usable_width -= footer_margin + (icon_padding * 3);

      if (usable_width > 0)
      {
         /* Get core title */
         menu_entries_get_core_title(core_title, sizeof(core_title));

         /* Configure and run ticker */
         if (use_smooth_ticker)
         {
            ticker_smooth.idx           = p_anim->ticker_pixel_idx;
            ticker_smooth.font_scale    = 1.0f;
            ticker_smooth.type_enum     = menu_ticker_type;
            ticker_smooth.spacer        = ticker_spacer;
            ticker_smooth.x_offset      = &ticker_x_offset;
            ticker_smooth.dst_str_width = NULL;

            ticker_smooth.font          = ozone->fonts.footer.font;
            ticker_smooth.selected      = true;
            ticker_smooth.field_width   = usable_width;
            ticker_smooth.src_str       = core_title;
            ticker_smooth.dst_str       = core_title_buf;
            ticker_smooth.dst_str_len   = sizeof(core_title_buf);

            gfx_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.idx                  = p_anim->ticker_idx;
            ticker.type_enum            = menu_ticker_type;
            ticker.spacer               = ticker_spacer;

            ticker.s                    = core_title_buf;
            ticker.len                  = usable_width / ozone->fonts.footer.glyph_width;
            ticker.str                  = core_title;
            ticker.selected             = true;

            gfx_animation_ticker(&ticker);
         }

         /* Draw text */
         gfx_display_draw_text(
               ozone->fonts.footer.font,
               core_title_buf,
               ticker_x_offset + footer_margin,
               footer_text_y,
               video_width,
               video_height,
               ozone->theme->text_rgba,
               TEXT_ALIGN_LEFT,
               1.0f,
               false,
               1.0f,
               false);
      }
   }
#ifdef HAVE_LIBNX
   else
   {
      if (dispctx)
      {
         if (dispctx->blend_begin)
            dispctx->blend_begin(userdata);
         if (dispctx->draw)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  69 * scale_factor,
                  30 * scale_factor,
                  ozone->theme->textures[OZONE_THEME_TEXTURE_SWITCH],
                  footer_margin,
                  video_height - ozone->dimensions.footer_height / 2 - 15 * scale_factor,
                  video_width,
                  video_height,
                  0,
                  1,
                  ozone->pure_white);
         if (dispctx->blend_end)
            dispctx->blend_end(userdata);
      }
   }
#endif
}

static void ozone_set_thumbnail_system(void *data, char*s, size_t len)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!ozone)
      return;

   gfx_thumbnail_set_system(
         ozone->thumbnail_path_data, s, playlist_get_cached());
}

static void ozone_get_thumbnail_system(void *data, char*s, size_t len)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   const char *system    = NULL;
   if (!ozone)
      return;

   if (gfx_thumbnail_get_system(ozone->thumbnail_path_data, &system))
      strlcpy(s, system, len);
}

static void ozone_selection_changed(ozone_handle_t *ozone, bool allow_animation)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t new_selection       = menu_navigation_get_selection();
   ozone_node_t *node         = (ozone_node_t*)selection_buf->list[new_selection].userdata;

   if (!node)
      return;

   if (ozone->selection != new_selection)
   {
      menu_entry_t entry;
      unsigned entry_type;
      uintptr_t tag                = (uintptr_t)selection_buf;
      size_t selection             = menu_navigation_get_selection();

      MENU_ENTRY_INIT(entry);
      entry.path_enabled           = false;
      entry.label_enabled          = false;
      entry.rich_label_enabled     = false;
      entry.value_enabled          = false;
      entry.sublabel_enabled       = false;
      menu_entry_get(&entry, 0, selection, NULL, true);

      entry_type                   = entry.type;

      ozone->selection_old         = ozone->selection;
      ozone->selection             = new_selection;

      ozone->cursor_in_sidebar_old = ozone->cursor_in_sidebar;

      gfx_animation_kill_by_tag(&tag);
      ozone_update_scroll(ozone, allow_animation, node);

      /* Update thumbnail */
      if (gfx_thumbnail_is_enabled(
               ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
          gfx_thumbnail_is_enabled(
             ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      {
         bool update_thumbnails = false;

         /* Playlist updates */
         if (ozone->is_playlist && ozone->depth == 1)
         {
            ozone_set_thumbnail_content(ozone, "");
            update_thumbnails = true;
         }
         /* Database list updates
          * (pointless nuisance...) */
         else if (ozone->depth == 4 && ozone->is_db_manager_list)
         {
            ozone_set_thumbnail_content(ozone, "");
            update_thumbnails = true;
         }
         /* Filebrowser image updates */
         else if (ozone->is_file_list)
         {
            if ((entry_type == FILE_TYPE_IMAGEVIEWER) ||
                (entry_type == FILE_TYPE_IMAGE))
            {
               ozone_set_thumbnail_content(ozone, "imageviewer");
               update_thumbnails = true;
            }
            else
            {
               /* If this is a file list and current
                * entry is not an image, have to 'reset'
                * content + right/left thumbnails
                * (otherwise last loaded thumbnail will
                * persist, and be shown on the wrong entry) */
               gfx_thumbnail_set_content(ozone->thumbnail_path_data, NULL);
               ozone_unload_thumbnail_textures(ozone);
            }
         }

         if (update_thumbnails)
            ozone_update_thumbnail_image(ozone);
      }

      /* TODO: update savestate thumbnail and path */
   }
}

static void ozone_navigation_clear(void *data, bool pending_push)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!pending_push)
      ozone_selection_changed(ozone, true);
}

static void ozone_navigation_pointer_changed(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_navigation_set(void *data, bool scroll)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_navigation_alphabet(void *data, size_t *unused)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_messagebox_fadeout_cb(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   free(ozone->pending_message);
   ozone->pending_message = NULL;

   ozone->should_draw_messagebox = false;
}

static void INLINE ozone_font_bind(ozone_font_data_t *font_data)
{
   font_driver_bind_block(font_data->font, &font_data->raster_block);
   font_data->raster_block.carr.coords.vertices = 0;
}

static void INLINE ozone_font_unbind(ozone_font_data_t *font_data)
{
   font_driver_bind_block(font_data->font, NULL);
}

static void ozone_frame(void *data, video_frame_info_t *video_info)
{
   gfx_animation_ctx_entry_t entry;
   ozone_handle_t* ozone                  = (ozone_handle_t*) data;
   settings_t  *settings                  = config_get_ptr();
   unsigned color_theme                   = settings->uints.menu_ozone_color_theme;
   bool use_preferred_system_color_theme  = settings->bools.menu_use_preferred_system_color_theme;
   uintptr_t messagebox_tag               = (uintptr_t)ozone->pending_message;
   bool draw_osk                          = menu_input_dialog_get_display_kb();
   static bool draw_osk_old               = false;
   float *background_color                = NULL;
   void *userdata                         = video_info->userdata;
   unsigned video_width                   = video_info->width;
   unsigned video_height                  = video_info->height;
   float menu_framebuffer_opacity         = video_info->menu_framebuffer_opacity;
   bool libretro_running                  = video_info->libretro_running;
   bool video_fullscreen                  = video_info->fullscreen;
   bool mouse_grabbed                     = video_info->input_driver_grab_mouse_state;
   bool menu_mouse_enable                 = video_info->menu_mouse_enable;
   bool input_menu_swap_ok_cancel_buttons = video_info->input_menu_swap_ok_cancel_buttons;
   bool battery_level_enable              = video_info->battery_level_enable;
   bool timedate_enable                   = video_info->timedate_enable;
   gfx_display_t            *p_disp       = (gfx_display_t*)video_info->disp_userdata;
   gfx_animation_t *p_anim                = anim_get_ptr();
   gfx_display_ctx_driver_t *dispctx      = p_disp->dispctx;

#if 0
   static bool reset                      = false;

   if (discord_avatar_is_ready() && !reset)
   {
      ozone_context_reset(data, false);
      reset = true;
   }
#endif


   if (!ozone)
      return;

   if (ozone->first_frame)
   {
      menu_input_get_pointer_state(&ozone->pointer);

      ozone->cursor_x_old = ozone->pointer.x;
      ozone->cursor_y_old = ozone->pointer.y;
      ozone->first_frame  = false;
   }

   /* OSK Fade detection */
   if (draw_osk != draw_osk_old)
   {
      draw_osk_old = draw_osk;
      if (!draw_osk)
      {
         ozone->should_draw_messagebox       = false;
         ozone->messagebox_state             = false;
         ozone->messagebox_state_old         = false;
         ozone->animations.messagebox_alpha  = 0.0f;
      }
   }

   /* Change theme on the fly */
   if ((color_theme != last_color_theme) ||
       (last_use_preferred_system_color_theme != use_preferred_system_color_theme))
   {
      if (use_preferred_system_color_theme)
      {
         color_theme                            = ozone_get_system_theme();
         configuration_set_uint(settings,
               settings->uints.menu_ozone_color_theme, color_theme);
      }

      ozone_set_color_theme(ozone, color_theme);
      ozone_set_background_running_opacity(ozone, menu_framebuffer_opacity);

      last_use_preferred_system_color_theme = use_preferred_system_color_theme;
   }

   /* If menu screensaver is active, draw
    * screensaver and return */
   if (ozone->show_screensaver)
   {
      menu_screensaver_frame(ozone->screensaver,
            video_info, p_disp);
      return;
   }

   video_driver_set_viewport(video_width, video_height, true, false);

   /* Clear text */
   ozone_font_bind(&ozone->fonts.footer);
   ozone_font_bind(&ozone->fonts.title);
   ozone_font_bind(&ozone->fonts.time);
   ozone_font_bind(&ozone->fonts.entries_label);
   ozone_font_bind(&ozone->fonts.entries_sublabel);
   ozone_font_bind(&ozone->fonts.sidebar);

   /* Background */
   if (libretro_running &&
       (menu_framebuffer_opacity < 1.0f))
   {
      if (menu_framebuffer_opacity != last_framebuffer_opacity)
         ozone_set_background_running_opacity(ozone, menu_framebuffer_opacity);

      background_color = ozone->theme->background_libretro_running;
   }
   else
      background_color = ozone->theme->background;

   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         0, 0, video_width, video_height,
         video_width, video_height,
         background_color
         );

   /* Header, footer */
   ozone_draw_header(
         ozone,
         p_disp,
         p_anim,
         settings,
         userdata,
         video_width,
         video_height,
         battery_level_enable,
         timedate_enable);
   ozone_draw_footer(ozone,
         p_disp, p_anim,
         userdata,
         video_width,
         video_height,
         input_menu_swap_ok_cancel_buttons,
         settings);

   /* Sidebar */
   if (ozone->draw_sidebar)
      ozone_draw_sidebar(
            ozone,
            p_disp,
            p_anim,
            settings,
            userdata,
            video_width,
            video_height,
            libretro_running,
            menu_framebuffer_opacity);

   /* Menu entries */
   gfx_display_scissor_begin(p_disp,
         userdata,
         video_width,
         video_height,
         ozone->sidebar_offset + (unsigned) ozone->dimensions_sidebar_width,
         ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
         video_width - (unsigned) ozone->dimensions_sidebar_width 
         + (-ozone->sidebar_offset),
         video_height - ozone->dimensions.header_height - ozone->dimensions.footer_height - ozone->dimensions.spacer_1px);

   /* Current list */
   ozone_draw_entries(
         ozone,
         p_disp,
         p_anim,
         settings,
         userdata,
         video_width,
         video_height,
         (unsigned)ozone->selection,
         (unsigned)ozone->selection_old,
         menu_entries_get_selection_buf_ptr(0),
         ozone->animations.list_alpha,
         ozone->animations.scroll_y,
         ozone->is_playlist
         );

   /* Old list */
   if (ozone->draw_old_list)
      ozone_draw_entries(
            ozone,
            p_disp,
            p_anim,
            settings,
            userdata,
            video_width,
            video_height,
            (unsigned)ozone->selection_old_list,
            (unsigned)ozone->selection_old_list,
            &ozone->selection_buf_old,
            ozone->animations.list_alpha,
            ozone->scroll_old,
            ozone->is_playlist_old
      );

   /* Thumbnail bar */
   if (ozone->show_thumbnail_bar)
      ozone_draw_thumbnail_bar(ozone,
            p_disp,
            p_anim,
            settings,
            userdata,
            video_width,
            video_height,
            libretro_running,
            menu_framebuffer_opacity);

   if (dispctx && dispctx->scissor_end)
      dispctx->scissor_end(userdata,
            video_width, video_height);

   /* Flush first layer of text */
   ozone_font_flush(video_width, video_height, &ozone->fonts.footer);
   ozone_font_flush(video_width, video_height, &ozone->fonts.title);
   ozone_font_flush(video_width, video_height, &ozone->fonts.time);
   ozone_font_flush(video_width, video_height, &ozone->fonts.entries_label);
   ozone_font_flush(video_width, video_height, &ozone->fonts.entries_sublabel);
   ozone_font_flush(video_width, video_height, &ozone->fonts.sidebar);

   /* Draw fullscreen thumbnails, if required */
   ozone_draw_fullscreen_thumbnails(ozone,
         userdata,
         video_info->disp_userdata,
         video_width,
         video_height);

   /* Message box & OSK - second layer of text */
   if (ozone->should_draw_messagebox || draw_osk)
   {
      /* Fade in animation */
      if (ozone->messagebox_state_old != ozone->messagebox_state && ozone->messagebox_state)
      {
         ozone->messagebox_state_old = ozone->messagebox_state;

         gfx_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 0.0f;

         entry.cb = NULL;
         entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum = EASING_OUT_QUAD;
         entry.subject = &ozone->animations.messagebox_alpha;
         entry.tag = messagebox_tag;
         entry.target_value = 1.0f;
         entry.userdata = NULL;

         gfx_animation_push(&entry);
      }
      /* Fade out animation */
      else if (ozone->messagebox_state_old != ozone->messagebox_state && !ozone->messagebox_state)
      {
         ozone->messagebox_state_old = ozone->messagebox_state;
         ozone->messagebox_state = false;

         gfx_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 1.0f;

         entry.cb = ozone_messagebox_fadeout_cb;
         entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum = EASING_OUT_QUAD;
         entry.subject = &ozone->animations.messagebox_alpha;
         entry.tag = messagebox_tag;
         entry.target_value = 0.0f;
         entry.userdata = ozone;

         gfx_animation_push(&entry);
      }

      ozone_draw_backdrop(
            userdata,
            video_info->disp_userdata,
            video_width,
            video_height,
            float_min(ozone->animations.messagebox_alpha, 0.75f));

      if (draw_osk)
      {
         const char *label = menu_input_dialog_get_label_buffer();
         const char *str   = menu_input_dialog_get_buffer();

         ozone_draw_osk(ozone,
               userdata,
               video_info->disp_userdata,
               video_width,
               video_height,
               label, str);
      }
      else
         ozone_draw_messagebox(
               ozone,
               p_disp,
               userdata,
               video_width,
               video_height,
               ozone->pending_message);

      /* Flush second layer of text */
      ozone_font_flush(video_width, video_height, &ozone->fonts.footer);
      ozone_font_flush(video_width, video_height, &ozone->fonts.entries_label);
   }

   /* Cursor */
   if (ozone->show_cursor && (ozone->pointer.type != MENU_POINTER_DISABLED))
   {
      bool cursor_visible = (video_fullscreen || mouse_grabbed) &&
            menu_mouse_enable;

      gfx_display_set_alpha(ozone->pure_white, 1.0f);
      if (cursor_visible)
         gfx_display_draw_cursor(
               p_disp,
               userdata,
               video_width,
               video_height,
               cursor_visible,
               ozone->pure_white,
               ozone->dimensions.cursor_size,
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POINTER],
               ozone->pointer.x,
               ozone->pointer.y,
               video_width,
               video_height
               );
   }

   /* Unbind fonts */
   ozone_font_unbind(&ozone->fonts.footer);
   ozone_font_unbind(&ozone->fonts.title);
   ozone_font_unbind(&ozone->fonts.time);
   ozone_font_unbind(&ozone->fonts.entries_label);
   ozone_font_unbind(&ozone->fonts.entries_sublabel);
   ozone_font_unbind(&ozone->fonts.sidebar);

   video_driver_set_viewport(video_width, video_height, false, true);
}

static void ozone_set_header(ozone_handle_t *ozone)
{
   if (ozone->categories_selection_ptr <= ozone->system_tab_end)
      menu_entries_get_title(ozone->title, sizeof(ozone->title));
   else
   {
      ozone_node_t *node = (ozone_node_t*)file_list_get_userdata_at_offset(&ozone->horizontal_list, ozone->categories_selection_ptr - ozone->system_tab_end-1);

      if (node && node->console_name)
      {
         strlcpy(ozone->title, node->console_name, sizeof(ozone->title));

         /* Add current search terms */
         menu_entries_search_append_terms_string(
               ozone->title, sizeof(ozone->title));
      }
   }
}

static void ozone_animation_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->draw_old_list             = false;
   ozone->animations.cursor_alpha   = 1.0f;
}

static void ozone_list_open(ozone_handle_t *ozone, settings_t *settings)
{
   struct gfx_animation_ctx_entry entry;
   uintptr_t sidebar_tag        = (uintptr_t)&ozone->sidebar_offset;

   ozone->draw_old_list         = true;

   /* Left/right animation */
   ozone->animations.list_alpha = 0.0f;

   entry.cb                     = ozone_animation_end;
   entry.duration               = ANIMATION_PUSH_ENTRY_DURATION;
   entry.easing_enum            = EASING_OUT_QUAD;
   entry.subject                = &ozone->animations.list_alpha;
   entry.tag                    = (uintptr_t)NULL;
   entry.target_value           = 1.0f;
   entry.userdata               = ozone;

   gfx_animation_push(&entry);

   /* Sidebar animation */
   ozone_sidebar_update_collapse(ozone, settings, true);

   if (ozone->depth == 1)
   {
      ozone->draw_sidebar = true;

      entry.cb = NULL;
      entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
      entry.easing_enum = EASING_OUT_QUAD;
      entry.subject = &ozone->sidebar_offset;
      entry.tag = sidebar_tag;
      entry.target_value = 0.0f;
      entry.userdata = NULL;

      gfx_animation_push(&entry);
   }
   else if (ozone->depth > 1)
   {
      struct gfx_animation_ctx_entry entry;

      entry.cb           = ozone_collapse_end;
      entry.duration     = ANIMATION_PUSH_ENTRY_DURATION;
      entry.easing_enum  = EASING_OUT_QUAD;
      entry.subject      = &ozone->sidebar_offset;
      entry.tag          = sidebar_tag;
      entry.target_value = -ozone->dimensions_sidebar_width;
      entry.userdata     = (void*)ozone;

      gfx_animation_push(&entry);
   }
}

static void ozone_populate_entries(void *data,
      const char *path, const char *label, unsigned k)
{
   settings_t *settings  = NULL;
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   int new_depth;
   bool animate;

   if (!ozone)
      return;

   settings              = config_get_ptr();

   ozone_set_header(ozone);

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
      ozone_selection_changed(ozone, false);

      /* Handle playlist searches
       * (Ozone is a fickle beast...) */
      if (ozone->is_playlist)
      {
         menu_serch_terms_t *menu_search_terms =
               menu_entries_search_get_terms();
         size_t num_search_terms               =
               menu_search_terms ? menu_search_terms->size : 0;

         if (ozone->num_search_terms_old != num_search_terms)
         {
            /* Refresh thumbnails */
            ozone_unload_thumbnail_textures(ozone);

            if (gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
                gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
            {
               ozone_set_thumbnail_content(ozone, "");
               ozone_update_thumbnail_image(ozone);
            }

            /* If we are currently inside an empty
             * playlist, return to the sidebar */
            if (!ozone->cursor_in_sidebar)
            {
               file_list_t *list       = menu_entries_get_selection_buf_ptr(0);
               uintptr_t animation_tag = (uintptr_t)&ozone->animations.cursor_alpha;
               bool goto_sidebar       = false;

               if (!list || (list->size < 1))
                  goto_sidebar = true;

               if (!goto_sidebar &&
                   (list->list[0].type != FILE_TYPE_RPL_ENTRY))
                  goto_sidebar = true;

               if (goto_sidebar)
               {
                  gfx_animation_kill_by_tag(&animation_tag);
                  ozone->empty_playlist = true;
                  ozone_go_to_sidebar(ozone, settings, animation_tag);
               }
            }

            ozone->num_search_terms_old = num_search_terms;
         }
      }

      return;
   }

   ozone->need_compute = true;

   ozone->first_onscreen_entry    = 0;
   ozone->last_onscreen_entry     = 0;

   new_depth = (int)ozone_list_get_size(ozone, MENU_LIST_PLAIN);

   animate                    = new_depth != ozone->depth;
   ozone->fade_direction      = new_depth <= ozone->depth;
   ozone->depth               = new_depth;
   ozone->is_playlist         = ozone_is_playlist(ozone, true);
   ozone->is_db_manager_list  = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST));
   ozone->is_file_list        = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES));
   ozone->is_quick_menu       = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS)) ||
                                string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS));

   if (animate)
      if (ozone->categories_selection_ptr == ozone->categories_active_idx_old)
         ozone_list_open(ozone, settings);

   /* Thumbnails
    * > Note: Leave current thumbnails loaded when
    *   opening the quick menu - allows proper fade
    *   out of the fullscreen thumbnail viewer */
   if (!ozone->is_quick_menu)
   {
      ozone_unload_thumbnail_textures(ozone);

      if (gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
          gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      {
         /* Only auto-load thumbnails if we are viewing
          * a playlist or a database manager list
          * > Note that we can ignore file browser lists,
          *   since the first selected item on such a list
          *   can never have a thumbnail */
         if (ozone->is_playlist ||
             (ozone->depth == 4 && ozone->is_db_manager_list))
         {
            ozone_set_thumbnail_content(ozone, "");
            ozone_update_thumbnail_image(ozone);
         }
      }
   }

   /* Fullscreen thumbnails are only enabled on
    * playlists, database manager lists and file
    * lists */
   ozone->fullscreen_thumbnails_available =
         (ozone->is_playlist        && ozone->depth == 1) ||
         (ozone->is_db_manager_list && ozone->depth == 4) ||
          ozone->is_file_list;
}

/* TODO: Fancy toggle animation */

static void ozone_toggle(void *userdata, bool menu_on)
{
   settings_t *settings  = NULL;
   bool tmp              = false;
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   if (!ozone)
      return;

   settings              = config_get_ptr();
   tmp                   = !menu_entries_ctl(
         MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL);

   if (tmp)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   if (ozone->depth == 1)
   {
      ozone->draw_sidebar = true;
      ozone->sidebar_offset = 0.0f;
   }

   ozone_sidebar_update_collapse(ozone, settings, false);
}

static bool ozone_menu_init_list(void *data)
{
   menu_displaylist_info_t info;

   settings_t *settings         = config_get_ptr();
   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf   = menu_entries_get_selection_buf_ptr(0);

   menu_displaylist_info_init(&info);

   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.exts                    = strdup("lpl");
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append_enum(menu_stack, info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0);

   info.list  = selection_buf;

   if (!menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info, settings))
      goto error;

   info.need_push = true;

   if (!menu_displaylist_process(&info))
      goto error;

   menu_displaylist_info_free(&info);
   return true;

error:
   menu_displaylist_info_free(&info);
   return false;
}

static ozone_node_t *ozone_copy_node(const ozone_node_t *old_node)
{
   ozone_node_t *new_node = (ozone_node_t*)malloc(sizeof(*new_node));

   *new_node            = *old_node;
   new_node->fullpath   = old_node->fullpath ? strdup(old_node->fullpath) : NULL;

   return new_node;
}

static void ozone_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *label,
      size_t list_size,
      unsigned type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone_node_t *node = NULL;
   int i = (int)list_size;

   if (!ozone || !list)
      return;

   ozone->need_compute = true;

   node = (ozone_node_t*)list->list[i].userdata;

   if (!node)
      node = ozone_alloc_node();

   if (!node)
   {
      RARCH_ERR("ozone node could not be allocated.\n");
      return;
   }

   if (!string_is_empty(fullpath))
   {
      if (node->fullpath)
         free(node->fullpath);

      node->fullpath = strdup(fullpath);
   }

   list->list[i].userdata = node;
}

static void ozone_list_deep_copy(const file_list_t *src, file_list_t *dst,
      size_t first, size_t last)
{
   size_t i, j   = 0;
   uintptr_t tag = (uintptr_t)dst;

   gfx_animation_kill_by_tag(&tag);

   ozone_free_list_nodes(dst, true);

   file_list_clear(dst);
   file_list_reserve(dst, (last + 1) - first);

   for (i = first; i <= last; ++i)
   {
      struct item_file *d = &dst->list[j];
      struct item_file *s = &src->list[i];
      void     *src_udata = s->userdata;
      void     *src_adata = s->actiondata;

      *d       = *s;
      d->alt   = string_is_empty(d->alt)   ? NULL : strdup(d->alt);
      d->path  = string_is_empty(d->path)  ? NULL : strdup(d->path);
      d->label = string_is_empty(d->label) ? NULL : strdup(d->label);

      if (src_udata)
         dst->list[j].userdata = (void*)ozone_copy_node((const ozone_node_t*)src_udata);

      if (src_adata)
      {
         void *data = malloc(sizeof(menu_file_list_cbs_t));
         memcpy(data, src_adata, sizeof(menu_file_list_cbs_t));
         dst->list[j].actiondata = data;
      }

      ++j;
   }

   dst->size = j;
}

void ozone_list_cache(void *data,
      enum menu_list_type type, unsigned action)
{
   size_t y, entries_end;
   unsigned i;
   unsigned video_info_height;
   float bottom_boundary;
   ozone_node_t *first_node;
   float scale_factor;
   unsigned first             = 0;
   unsigned last              = 0;
   file_list_t *selection_buf = NULL;
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return;

   scale_factor               = ozone->last_scale_factor;
   ozone->need_compute        = true;
   ozone->selection_old_list  = ozone->selection;
   ozone->scroll_old          = ozone->animations.scroll_y;
   ozone->is_playlist_old     = ozone->is_playlist;

   /* Deep copy visible elements */
   video_driver_get_size(NULL, &video_info_height);
   y                          = ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical;
   entries_end                = menu_entries_get_size();
   selection_buf              = menu_entries_get_selection_buf_ptr(0);
   bottom_boundary            = video_info_height - ozone->dimensions.header_height - ozone->dimensions.footer_height;

   for (i = 0; i < entries_end; i++)
   {
      ozone_node_t *node = (ozone_node_t*)selection_buf->list[i].userdata;

      if (!node)
         continue;

      if (y + ozone->animations.scroll_y + node->height + 20 * scale_factor < ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical)
      {
         first++;
         goto text_iterate;
      }
      else if (y + ozone->animations.scroll_y - node->height - 20 * scale_factor > bottom_boundary)
         goto text_iterate;

      last++;
text_iterate:
      y += node->height;
   }

   last                    -= 1;
   last                    += first;

   first_node               = (ozone_node_t*)selection_buf->list[first].userdata;
   ozone->old_list_offset_y = first_node->position_y;

   ozone_list_deep_copy(selection_buf,
         &ozone->selection_buf_old, first, last);
}

static int ozone_environ_cb(enum menu_environ_cb type, void *data, void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   if (!ozone)
      return -1;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         ozone->show_cursor = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         ozone->show_cursor = false;
         break;
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         if (!ozone)
            return -1;
         {
            settings_t *settings              = config_get_ptr();
            ozone_refresh_horizontal_list(ozone, settings);
         }
         break;
      case MENU_ENVIRON_ENABLE_SCREENSAVER:
         ozone->show_screensaver = true;
         break;
      case MENU_ENVIRON_DISABLE_SCREENSAVER:
         ozone->show_screensaver = false;
         break;
      default:
         return -1;
   }

   return 0;
}

static void ozone_messagebox(void *data, const char *message)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone || string_is_empty(message))
      return;

   if (ozone->pending_message)
   {
      free(ozone->pending_message);
      ozone->pending_message = NULL;
   }

   ozone->pending_message        = strdup(message);
   ozone->messagebox_state       = true;
   ozone->should_draw_messagebox = true;
}

static int ozone_deferred_push_content_actions(menu_displaylist_info_t *info)
{
   settings_t *settings         = config_get_ptr();
   if (!menu_displaylist_ctl(
            DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, info, settings))
      return -1;
   menu_displaylist_process(info);
   menu_displaylist_info_free(info);
   return 0;
}

static int ozone_list_bind_init_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CONTENT_ACTIONS:
            cbs->action_deferred_push = ozone_deferred_push_content_actions;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int ozone_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (ozone_list_bind_init_compare_label(cbs) == 0)
      return 0;

   return -1;
}

static int ozone_pointer_up(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned width, height;
   ozone_handle_t *ozone             = (ozone_handle_t*)userdata;
   file_list_t *selection_buf        = menu_entries_get_selection_buf_ptr(0);
   uintptr_t sidebar_tag             = (uintptr_t)selection_buf;
   size_t selection                  = menu_navigation_get_selection();
   size_t entries_end                = menu_entries_get_size();
   settings_t *settings              = config_get_ptr();

   if (!ozone)
      return -1;

   /* If fullscreen thumbnail view is enabled,
    * all input will disable it and otherwise
    * be ignored */
   if (ozone->show_fullscreen_thumbnails)
   {
      /* Must reset scroll acceleration, in case
       * user performed a swipe (don't want menu
       * list to 'drift' after hiding fullscreen
       * thumbnails...) */
      menu_input_set_pointer_y_accel(0.0f);

      ozone_hide_fullscreen_thumbnails(ozone, true);
      return 0;
   }

   video_driver_get_size(&width, &height);

   switch (gesture)
   {
      case MENU_INPUT_GESTURE_TAP:
      case MENU_INPUT_GESTURE_SHORT_PRESS:
         /* Tap/press header or footer: Menu back/cancel */
         if ((y < ozone->dimensions.header_height) ||
             (y > height - ozone->dimensions.footer_height))
            return ozone_menu_entry_action(ozone, entry, selection, MENU_ACTION_CANCEL);
         /* Tap/press entries: Activate and/or select item */
         else if ((ptr < entries_end) &&
                  (x > ozone->dimensions_sidebar_width 
                   + ozone->sidebar_offset) &&
                  (x < width - ozone->animations.thumbnail_bar_position))
         {
            if (gesture == MENU_INPUT_GESTURE_TAP)
            {
               /* A 'tap' always produces a menu action */

               /* If current 'pointer' item is not active,
                * activate it immediately */
               if (ptr != selection)
                  menu_navigation_set_selection(ptr);

               /* If we are not currently in the sidebar,
                * perform a MENU_ACTION_SELECT on currently
                * active item
                * > NOTE 1: Cannot perform a 'leave sidebar' operation
                *   and a MENU_ACTION_SELECT at the same time...
                * > NOTE 2: We still use 'selection' (i.e. old selection
                *   value) here. This ensures that ozone_menu_entry_action()
                *   registers any change due to the above automatic
                *   'pointer item' activation, and thus operates
                *   on the correct target entry */
               if (!ozone->cursor_in_sidebar)
                  return ozone_menu_entry_action(ozone, entry,
                        selection, MENU_ACTION_SELECT);

               /* If we currently in the sidebar, leave it */
               ozone_leave_sidebar(ozone, settings, sidebar_tag);
            }
            else
            {
               /* A 'short' press is used only to activate (highlight)
                * an item - it does not invoke a MENU_ACTION_SELECT
                * action */
               menu_input_set_pointer_y_accel(0.0f);

               if (ptr != selection)
                  menu_navigation_set_selection(ptr);

               /* If we are currently in the sidebar, leave it */
               if (ozone->cursor_in_sidebar)
                  ozone_leave_sidebar(ozone, settings, sidebar_tag);
               /* If this is a playlist and the selection
                * has changed, must update thumbnails */
               else if (ozone->is_playlist &&
                        (ozone->depth == 1) &&
                        (ptr != selection))
               {
                  ozone_set_thumbnail_content(ozone, "");
                  ozone_update_thumbnail_image(ozone);
               }
            }
         }
         /* Tap/press thumbnail bar: toggle content metadata
          * override */
         else if (x > width - ozone->animations.thumbnail_bar_position)
         {
            /* Want to capture all input here, but only act
             * upon it if the content metadata toggle is
             * available (i.e. viewing a playlist with dual
             * thumbnails) */
            if (ozone_metadata_override_available(ozone))
               return ozone_menu_entry_action(ozone, entry, selection, MENU_ACTION_INFO);
         }
         /* Tap/press sidebar: return to sidebar or select
          * category */
         else if (ozone->pointer_in_sidebar)
         {
            /* If cursor is not in sidebar, return to sidebar */
            if (!ozone->cursor_in_sidebar)
               ozone_go_to_sidebar(ozone, settings, sidebar_tag);
            /* Otherwise, select current category */
            else if (
                     ozone->pointer_categories_selection 
                  != ozone->categories_selection_ptr)
            {
               unsigned horizontal_list_size = (unsigned)ozone->horizontal_list.size;

               /* Ensure that current category is valid */
               if (ozone->pointer_categories_selection <= ozone->system_tab_end + horizontal_list_size)
                  ozone_sidebar_goto(ozone, (unsigned)ozone->pointer_categories_selection);
            }
         }
         break;
      case MENU_INPUT_GESTURE_LONG_PRESS:
         /* 'Reset to default' action */
         if ((y > ozone->dimensions.header_height) &&
             (y < height - ozone->dimensions.footer_height) &&
             (ptr < entries_end) &&
             (ptr == selection) &&
             (x > ozone->dimensions_sidebar_width + ozone->sidebar_offset) &&
             (x < width - ozone->animations.thumbnail_bar_position))
            return ozone_menu_entry_action(ozone,
                  entry, selection, MENU_ACTION_START);
         break;
      case MENU_INPUT_GESTURE_SWIPE_LEFT:
         /* If this is a playlist, descend alphabet
          * > Note: Can only do this if we are not using
          *   a mouse, since it conflicts with auto selection
          *   of entry under cursor */
         if ((ozone->pointer.type != MENU_POINTER_MOUSE) &&
             ozone->is_playlist &&
             (ozone->depth == 1))
            return ozone_menu_entry_action(ozone, entry, (size_t)ptr, MENU_ACTION_SCROLL_UP);
         break;
      case MENU_INPUT_GESTURE_SWIPE_RIGHT:
         /* If this is a playlist, ascend alphabet
          * > Note: Can only do this if we are not using
          *   a mouse, since it conflicts with auto selection
          *   of entry under cursor */
         if ((ozone->pointer.type != MENU_POINTER_MOUSE) &&
             ozone->is_playlist &&
             (ozone->depth == 1))
            return ozone_menu_entry_action(ozone, entry, (size_t)ptr, MENU_ACTION_SCROLL_DOWN);
         break;
      default:
         /* Ignore input */
         break;
   }

   return 0;
}

ozone_node_t *ozone_alloc_node(void)
{
   ozone_node_t *node   = (ozone_node_t*)malloc(sizeof(*node));

   node->height         = 0;
   node->position_y     = 0;
   node->console_name   = NULL;
   node->icon           = 0;
   node->content_icon   = 0;
   node->fullpath       = NULL;
   node->sublabel_lines = 0;
   node->wrap           = false;

   return node;
}

size_t ozone_list_get_size(void *data, enum menu_list_type type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone)
      return 0;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_HORIZONTAL:
         return ozone->horizontal_list.size;
      case MENU_LIST_TABS:
         return ozone->system_tab_end;
   }

   return 0;
}

void ozone_free_list_nodes(file_list_t *list, bool actiondata)
{
   unsigned i, size = list ? (unsigned)list->size : 0;

   for (i = 0; i < size; ++i)
   {
      ozone_free_node((ozone_node_t*)file_list_get_userdata_at_offset(list, i));

      list->list[i].userdata = NULL;

      if (actiondata)
         file_list_free_actiondata(list, i);
   }
}

void ozone_update_content_metadata(ozone_handle_t *ozone)
{
   const char *core_name             = NULL;
   size_t selection                  = menu_navigation_get_selection();
   playlist_t *playlist              = playlist_get_cached();
   settings_t *settings              = config_get_ptr();
   bool scroll_content_metadata      = settings->bools.ozone_scroll_content_metadata;
   bool show_entry_idx               = settings->bools.playlist_show_entry_idx;
   bool content_runtime_log          = settings->bools.content_runtime_log;
   bool content_runtime_log_aggr     = settings->bools.content_runtime_log_aggregate;
   const char *directory_runtime_log = settings->paths.directory_runtime_log;
   const char *directory_playlist    = settings->paths.directory_playlist;
   unsigned runtime_type             = settings->uints.playlist_sublabel_runtime_type;
   enum playlist_sublabel_last_played_style_type
         runtime_last_played_style   =
               (enum playlist_sublabel_last_played_style_type)
                     settings->uints.playlist_sublabel_last_played_style;
   enum playlist_sublabel_last_played_date_separator_type
         runtime_date_separator      =
               (enum playlist_sublabel_last_played_date_separator_type)
                     settings->uints.menu_timedate_date_separator;

   /* Must check whether core corresponds to 'viewer'
    * content even when not using a playlist, otherwise
    * file browser image updates are mishandled */
   if (gfx_thumbnail_get_core_name(ozone->thumbnail_path_data, &core_name))
      ozone->selection_core_is_viewer = string_is_equal(core_name, "imageviewer")
            || string_is_equal(core_name, "musicplayer")
            || string_is_equal(core_name, "movieplayer");
   else
      ozone->selection_core_is_viewer = false;

   if (ozone->is_playlist && playlist)
   {
      const char *core_label             = NULL;
      const struct playlist_entry *entry = NULL;
      size_t list_size                   = menu_entries_get_size();
      file_list_t *list                  = menu_entries_get_selection_buf_ptr(0);
      bool playlist_valid                = false;
      size_t playlist_index              = selection;

      /* Get playlist index corresponding
       * to the selected entry */
      if (list &&
          (selection < list_size) &&
          (list->list[selection].type == FILE_TYPE_RPL_ENTRY))
      {
         playlist_valid = true;
         playlist_index = list->list[selection].entry_idx;
      }

      /* Fill entry enumeration */
      if (show_entry_idx)
         snprintf(ozone->selection_entry_enumeration, sizeof(ozone->selection_entry_enumeration),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX),
            (unsigned long)(playlist_index + 1), (unsigned long)list_size);
      else
         ozone->selection_entry_enumeration[0] = '\0';

      /* Fill core name */
      if (!core_name || string_is_equal(core_name, "DETECT"))
         core_label = msg_hash_to_str(MSG_AUTODETECT);
      else
         core_label = core_name;

      snprintf(ozone->selection_core_name, sizeof(ozone->selection_core_name),
         "%s %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE), core_label);

      /* Word wrap core name string, if required */
      if (!scroll_content_metadata)
      {
         char tmpstr[sizeof(ozone->selection_core_name)];
         unsigned metadata_len =
               (ozone->dimensions.thumbnail_bar_width - ((ozone->dimensions.sidebar_entry_icon_padding * 2) * 2)) /
                     ozone->fonts.footer.glyph_width;

         strlcpy(tmpstr, ozone->selection_core_name, sizeof(tmpstr));
         (ozone->word_wrap)(ozone->selection_core_name, sizeof(ozone->selection_core_name),
               tmpstr, metadata_len, ozone->fonts.footer.wideglyph_width, 0);
         ozone->selection_core_name_lines = ozone_count_lines(ozone->selection_core_name);
      }
      else
         ozone->selection_core_name_lines = 1;

      /* Fill play time if applicable */
      if (playlist_valid &&
          (content_runtime_log || content_runtime_log_aggr))
         playlist_get_index(playlist, playlist_index, &entry);

      if (entry)
      {
         if (entry->runtime_status == PLAYLIST_RUNTIME_UNKNOWN)
            runtime_update_playlist(
                  playlist, playlist_index,
                  directory_runtime_log,
                  directory_playlist,
                  (runtime_type == PLAYLIST_RUNTIME_PER_CORE),
                  runtime_last_played_style,
                  runtime_date_separator);

         if (!string_is_empty(entry->runtime_str))
            strlcpy(ozone->selection_playtime, entry->runtime_str, sizeof(ozone->selection_playtime));
         if (!string_is_empty(entry->last_played_str))
            strlcpy(ozone->selection_lastplayed, entry->last_played_str, sizeof(ozone->selection_lastplayed));
      }
      else
      {
         snprintf(ozone->selection_playtime, sizeof(ozone->selection_playtime), "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED));

         snprintf(ozone->selection_lastplayed, sizeof(ozone->selection_lastplayed), "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED));
      }

      /* Word wrap last played string, if required */
      if (!scroll_content_metadata)
      {
         /* Note: Have to use a fixed length of '30' here, to
          * avoid awkward wrapping for certain last played time
          * formats. Last played strings are well defined, however
          * (unlike core names), so this should never overflow the
          * side bar */
         char tmpstr[sizeof(ozone->selection_lastplayed)];

         strlcpy(tmpstr, ozone->selection_lastplayed, sizeof(tmpstr));
         (ozone->word_wrap)(ozone->selection_lastplayed, sizeof(ozone->selection_lastplayed), tmpstr, 30, 100, 0);
         ozone->selection_lastplayed_lines = ozone_count_lines(ozone->selection_lastplayed);
      }
      else
         ozone->selection_lastplayed_lines = 1;
   }
}

void ozone_font_flush(
      unsigned video_width, unsigned video_height,
      ozone_font_data_t *font_data)
{
   /* Flushing is slow - only do it if font
    * has actually been used */
   if (!font_data ||
       (font_data->raster_block.carr.coords.vertices == 0))
      return;

   font_driver_flush(video_width, video_height, font_data->font);
   font_data->raster_block.carr.coords.vertices = 0;
}

void ozone_hide_fullscreen_thumbnails(ozone_handle_t *ozone, bool animate)
{
   uintptr_t alpha_tag                = (uintptr_t)
      &ozone->animations.fullscreen_thumbnail_alpha;
   gfx_thumbnail_state_t *p_gfx_thumb = gfx_thumb_get_ptr();

   /* Kill any existing fade in/out animations */
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Check whether animations are enabled */
   if (animate && (ozone->animations.fullscreen_thumbnail_alpha > 0.0f))
   {
      gfx_animation_ctx_entry_t animation_entry;

      /* Configure fade out animation */
      animation_entry.easing_enum  = EASING_OUT_QUAD;
      animation_entry.tag          = alpha_tag;
      animation_entry.duration     = p_gfx_thumb->fade_duration;
      animation_entry.target_value = 0.0f;
      animation_entry.subject      = &ozone->animations.fullscreen_thumbnail_alpha;
      animation_entry.cb           = NULL;
      animation_entry.userdata     = NULL;

      /* Push animation */
      gfx_animation_push(&animation_entry);
   }
   /* No animation - just set thumbnail alpha to zero */
   else
      ozone->animations.fullscreen_thumbnail_alpha = 0.0f;

   /* Disable fullscreen thumbnails */
   ozone->show_fullscreen_thumbnails = false;
}

void ozone_show_fullscreen_thumbnails(ozone_handle_t *ozone)
{
   menu_entry_t selected_entry;
   gfx_animation_ctx_entry_t animation_entry;
   const char *thumbnail_label        = NULL;
   file_list_t *selection_buf         = menu_entries_get_selection_buf_ptr(0);
   uintptr_t alpha_tag                = (uintptr_t)&ozone->animations.fullscreen_thumbnail_alpha;
   uintptr_t scroll_tag               = (uintptr_t)selection_buf;
   gfx_thumbnail_state_t *p_gfx_thumb = gfx_thumb_get_ptr();

   /* Before showing fullscreen thumbnails, must
    * ensure that any existing fullscreen thumbnail
    * view is disabled... */
   ozone_hide_fullscreen_thumbnails(ozone, false);

   /* Sanity check: Return immediately if this is
    * a menu without thumbnail support, or cursor
    * is currently in the sidebar */
   if (!ozone->fullscreen_thumbnails_available ||
       ozone->cursor_in_sidebar)
      return;

   /* We can only enable fullscreen thumbnails if
    * current selection has at least one valid thumbnail
    * and all thumbnails for current selection are already
    * loaded/available */
   if (ozone->selection_core_is_viewer)
   {
      /* imageviewer content requires special treatment,
       * since only the right thumbnail is ever loaded */
      if (!gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT))
         return;

      if (ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_AVAILABLE)
         return;
   }
   else
   {
      bool left_thumbnail_enabled = gfx_thumbnail_is_enabled(
            ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT);

      if (!left_thumbnail_enabled &&
          !gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT))
         return;

      if ((ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_AVAILABLE) &&
          (left_thumbnail_enabled &&
               ((ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_MISSING) &&
                (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_AVAILABLE))))
         return;

      if ((ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_MISSING) &&
          (!left_thumbnail_enabled ||
               (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_AVAILABLE)))
         return;
   }

   /* Menu list must be stationary while fullscreen
    * thumbnails are shown
    * > Kill any existing scroll animations and
    *   reset scroll acceleration */
   gfx_animation_kill_by_tag(&scroll_tag);
   menu_input_set_pointer_y_accel(0.0f);

   /* Cache selected entry label
    * (used as title when fullscreen thumbnails
    * are shown) */
   ozone->fullscreen_thumbnail_label[0] = '\0';

   /* > Get menu entry */
   MENU_ENTRY_INIT(selected_entry);
   selected_entry.path_enabled     = false;
   selected_entry.value_enabled    = false;
   selected_entry.sublabel_enabled = false;
   menu_entry_get(&selected_entry, 0, (size_t)ozone->selection, NULL, true);

   /* > Get entry label */
   if (!string_is_empty(selected_entry.rich_label))
      thumbnail_label  = selected_entry.rich_label;
   else
      thumbnail_label  = selected_entry.path;

   /* > Sanity check */
   if (!string_is_empty(thumbnail_label))
      strlcpy(
            ozone->fullscreen_thumbnail_label,
            thumbnail_label,
            sizeof(ozone->fullscreen_thumbnail_label));

   /* Configure fade in animation */
   animation_entry.easing_enum  = EASING_OUT_QUAD;
   animation_entry.tag          = alpha_tag;
   animation_entry.duration     = p_gfx_thumb->fade_duration;
   animation_entry.target_value = 1.0f;
   animation_entry.subject      = &ozone->animations.fullscreen_thumbnail_alpha;
   animation_entry.cb           = NULL;
   animation_entry.userdata     = NULL;

   /* Push animation */
   gfx_animation_push(&animation_entry);

   /* Enable fullscreen thumbnails */
   ozone->fullscreen_thumbnail_selection = (size_t)ozone->selection;
   ozone->show_fullscreen_thumbnails     = true;
}

void ozone_toggle_metadata_override(ozone_handle_t *ozone)
{
   gfx_animation_ctx_entry_t animation_entry;
   uintptr_t alpha_tag                = (uintptr_t)
      &ozone->animations.left_thumbnail_alpha;
   gfx_thumbnail_state_t *p_gfx_thumb = gfx_thumb_get_ptr();

   /* Kill any existing fade in/out animations */
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Set common animation parameters */
   animation_entry.easing_enum = EASING_OUT_QUAD;
   animation_entry.tag         = alpha_tag;
   animation_entry.duration    = p_gfx_thumb->fade_duration;
   animation_entry.subject     = &ozone->animations.left_thumbnail_alpha;
   animation_entry.cb          = NULL;
   animation_entry.userdata    = NULL;

   /* Check whether metadata override is
    * currently enabled */
   if (ozone->force_metadata_display)
   {
      /* Thumbnail will fade in */
      animation_entry.target_value  = 1.0f;
      ozone->force_metadata_display = false;
   }
   else
   {
      /* Thumbnail will fade out */
      animation_entry.target_value  = 0.0f;
      ozone->force_metadata_display = true;
   }

   /* Push animation */
   gfx_animation_push(&animation_entry);
}

void ozone_start_cursor_wiggle(ozone_handle_t* ozone, enum menu_action direction)
{
   /* Don't start another wiggle animation on top of another */
   if (!ozone || ozone->cursor_wiggle_state.wiggling)
      return;

   /* Don't allow wiggling in invalid directions */
   if (!(
         direction == MENU_ACTION_UP ||
         direction == MENU_ACTION_DOWN ||
         direction == MENU_ACTION_LEFT ||
         direction == MENU_ACTION_RIGHT
   ))
      return;

   /* Start wiggling */
   ozone->cursor_wiggle_state.start_time = menu_driver_get_current_time() / 1000;
   ozone->cursor_wiggle_state.direction  = direction;
   ozone->cursor_wiggle_state.amplitude  = rand() % 15 + 10;
   ozone->cursor_wiggle_state.wiggling   = true;
}

static int ozone_wiggle(ozone_handle_t* ozone, float t)
{
   float a = ozone->cursor_wiggle_state.amplitude;

   /* Damped sine wave */
   float w = 0.8f;   /* period */
   float c = 0.35f;  /* damp factor */
   return roundf(a * exp(-(c * t)) * sin(w * t));
}

void ozone_apply_cursor_wiggle_offset(ozone_handle_t* ozone, int* x, size_t* y)
{
   retro_time_t cur_time;
   retro_time_t t;

   /* Don't do anything if we are not wiggling */
   if (!ozone || !ozone->cursor_wiggle_state.wiggling)
      return;

   cur_time = menu_driver_get_current_time() / 1000;
   t        = (cur_time - ozone->cursor_wiggle_state.start_time) / 10;

   /* Has the animation ended? */
   if (t >= OZONE_WIGGLE_DURATION)
   {
      ozone->cursor_wiggle_state.wiggling = false;
      return;
   }

   /* Change cursor position depending on wiggle direction */
   switch (ozone->cursor_wiggle_state.direction)
   {
      case MENU_ACTION_RIGHT:
         *x += ozone_wiggle(ozone, t);
         break;
      case MENU_ACTION_LEFT:
         *x -= ozone_wiggle(ozone, t);
         break;
      case MENU_ACTION_DOWN:
         *y += ozone_wiggle(ozone, t);
         break;
      case MENU_ACTION_UP:
         *y -= ozone_wiggle(ozone, t);
         break;
      default:
         break;
   }
}

menu_ctx_driver_t menu_ctx_ozone = {
   NULL,                         /* set_texture */
   ozone_messagebox,
   ozone_render,
   ozone_frame,
   ozone_init,
   ozone_free,
   ozone_context_reset,
   ozone_context_destroy,
   ozone_populate_entries,
   ozone_toggle,
   ozone_navigation_clear,
   NULL,
   NULL,
   ozone_navigation_set,
   ozone_navigation_pointer_changed,
   ozone_navigation_alphabet,
   ozone_navigation_alphabet,
   ozone_menu_init_list,
   ozone_list_insert,
   NULL,                         /* list_prepend */
   ozone_list_free,
   ozone_list_clear,
   ozone_list_cache,
   ozone_list_push,
   ozone_list_get_selection,
   ozone_list_get_size,
   ozone_list_get_entry,
   NULL,                         /* list_set_selection */
   ozone_list_bind_init,
   NULL,
   "ozone",
   ozone_environ_cb,
   NULL,
   ozone_update_thumbnail_image,
   ozone_refresh_thumbnail_image,
   ozone_set_thumbnail_system,
   ozone_get_thumbnail_system,
   ozone_set_thumbnail_content,
   gfx_display_osk_ptr_at_pos,
   NULL,                         /* update_savestate_thumbnail_path */
   NULL,                         /* update_savestate_thumbnail_image */
   NULL,                         /* pointer_down */
   ozone_pointer_up,
   ozone_menu_entry_action
};
