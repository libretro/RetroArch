/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2015-2018 - Andre Leiradella
 *  Copyright (C) 2018-2020 - natinusala
 *  Copyright (C) 2019-2020 - James Leaver
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

#include <string/stdstring.h>
#include <file/file_path.h>

#include "../gfx_widgets.h"
#include "../gfx_animation.h"
#include "../gfx_display.h"
#include "../../retroarch.h"
#include "../../core_info.h"
#include "../../playlist.h"
#include "../../paths.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#define LOAD_CONTENT_ANIMATION_FADE_IN_DURATION   466.0f
#define LOAD_CONTENT_ANIMATION_SLIDE_DURATION     666.0f
#define LOAD_CONTENT_ANIMATION_WAIT_DURATION      1000.0f
#define LOAD_CONTENT_ANIMATION_FADE_OUT_DURATION  433.0f

/* Widget state */

enum gfx_widget_load_content_animation_status
{
   GFX_WIDGET_LOAD_CONTENT_IDLE = 0,
   GFX_WIDGET_LOAD_CONTENT_BEGIN,
   GFX_WIDGET_LOAD_CONTENT_FADE_IN,
   GFX_WIDGET_LOAD_CONTENT_SLIDE,
   GFX_WIDGET_LOAD_CONTENT_WAIT,
   GFX_WIDGET_LOAD_CONTENT_FADE_OUT
};

struct gfx_widget_load_content_animation_state
{
   gfx_display_t *p_disp;
   uintptr_t icon_texture;
   unsigned bg_shadow_height;
   unsigned margin_shadow_width;
   unsigned icon_size;
   unsigned content_name_color;
   unsigned system_name_color;
   unsigned content_name_width;
   unsigned system_name_width;

   unsigned bg_width;
   unsigned bg_height;

   gfx_timer_t timer;      /* float alignment */
   float bg_x;
   float bg_y;
   float alpha;
   float slide_offset;
   float bg_shadow_top_y;
   float bg_shadow_bottom_y;
   float margin_shadow_left_x;
   float margin_shadow_right_x;
   float icon_x_start;
   float icon_x_end;
   float icon_y;

   float text_x_start;
   float text_x_end;
   float content_name_y;
   float system_name_y;

   float bg_alpha;
   float bg_underlay_alpha;
   float bg_shadow_alpha;

   float bg_color[16];
   float bg_underlay_color[16];
   float bg_shadow_top_color[16];
   float bg_shadow_bottom_color[16];
   float margin_shadow_left_color[16];
   float margin_shadow_right_color[16];
   float icon_color[16];

   enum gfx_widget_load_content_animation_status status;

   char content_name[512];
   char system_name[512];
   char icon_directory[PATH_MAX_LENGTH];
   char icon_file[PATH_MAX_LENGTH];

   bool has_icon;
};

typedef struct gfx_widget_load_content_animation_state gfx_widget_load_content_animation_state_t;

static gfx_widget_load_content_animation_state_t p_w_load_content_animation_st = {
   NULL,                               /* p_disp */
   0,                                  /* icon_texture */
   0,                                  /* bg_shadow_height */
   0,                                  /* margin_shadow_width */
   0,                                  /* icon_size */
   0xE0E0E0FF,                         /* content_name_color */
   0xCFCFCFFF,                         /* system_name_color */
   0,                                  /* content_name_width */
   0,                                  /* system_name_width */

   0,                                  /* bg_width */
   0,                                  /* bg_height */

   0.0f,                               /* timer */
   0.0f,                               /* bg_x */
   0.0f,                               /* bg_y */
   0.0f,                               /* alpha */
   0.0f,                               /* slide_offset */
   0.0f,                               /* bg_shadow_top_y */
   0.0f,                               /* bg_shadow_bottom_y */
   0.0f,                               /* margin_shadow_left_x */
   0.0f,                               /* margin_shadow_right_x */
   0.0f,                               /* icon_x_start */
   0.0f,                               /* icon_x_end */
   0.0f,                               /* icon_y */

   0.0f,                               /* text_x_start */
   0.0f,                               /* text_x_end */
   0.0f,                               /* content_name_y */
   0.0f,                               /* system_name_y */

   0.95f,                              /* bg_alpha */
   0.5f,                               /* bg_underlay_alpha */
   0.4f,                               /* bg_shadow_alpha */

   COLOR_HEX_TO_FLOAT(0x000000, 1.0f), /* bg_color */
   COLOR_HEX_TO_FLOAT(0x505050, 1.0f), /* bg_underlay_color */
   COLOR_HEX_TO_FLOAT(0x000000, 0.0f), /* bg_shadow_top_color */
   COLOR_HEX_TO_FLOAT(0x000000, 0.0f), /* bg_shadow_bottom_color */
   COLOR_HEX_TO_FLOAT(0x000000, 0.0f), /* margin_shadow_left_color */
   COLOR_HEX_TO_FLOAT(0x000000, 0.0f), /* margin_shadow_right_color */
   COLOR_HEX_TO_FLOAT(0xE0E0E0, 1.0f), /* icon_color */

   GFX_WIDGET_LOAD_CONTENT_IDLE,       /* status */

   {'\0'},                             /* content_name */
   {'\0'},                             /* system_name */
   {'\0'},                             /* icon_directory */
   {'\0'},                             /* icon_file */

   false                               /* has_icon */
};

/* Utilities */

static void gfx_widget_load_content_animation_reset(void)
{
   gfx_widget_load_content_animation_state_t *state = &p_w_load_content_animation_st;
   uintptr_t alpha_tag                              = (uintptr_t)&state->alpha;
   uintptr_t slide_offset_tag                       = (uintptr_t)&state->slide_offset;
   uintptr_t timer_tag                              = (uintptr_t)&state->timer;

   /* Kill any existing timers/animations */
   gfx_animation_kill_by_tag(&timer_tag);
   gfx_animation_kill_by_tag(&alpha_tag);
   gfx_animation_kill_by_tag(&slide_offset_tag);

   /* Reset pertinent state parameters */
   state->status             = GFX_WIDGET_LOAD_CONTENT_IDLE;
   state->alpha              = 0.0f;
   state->slide_offset       = 0.0f;
   state->content_name[0]    = '\0';
   state->system_name[0]     = '\0';
   state->icon_file[0]       = '\0';
   state->has_icon           = false;
   state->content_name_width = 0;
   state->system_name_width  = 0;

   /* Unload any icon texture */
   if (state->icon_texture)
   {
      video_driver_texture_unload(&state->icon_texture);
      state->icon_texture = 0;
   }
}

static void gfx_widget_load_content_animation_load_icon(void)
{
   gfx_widget_load_content_animation_state_t *state = &p_w_load_content_animation_st;

   /* In all cases, unload any existing icon texture */
   if (state->icon_texture)
   {
      video_driver_texture_unload(&state->icon_texture);
      state->icon_texture = 0;
   }

   /* If widget has a valid icon set, load it */
   if (state->has_icon)
      gfx_display_reset_textures_list(
            state->icon_file, state->icon_directory,
            &state->icon_texture,
            TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL);
}

/* Callbacks */

static void gfx_widget_load_content_animation_fade_out_cb(void *userdata)
{
   /* Animation is complete - reset widget */
   gfx_widget_load_content_animation_reset();
}

static void gfx_widget_load_content_animation_wait_cb(void *userdata)
{
   gfx_widget_load_content_animation_state_t *state = (gfx_widget_load_content_animation_state_t*)userdata;
   uintptr_t alpha_tag                              = (uintptr_t)&state->alpha;
   gfx_animation_ctx_entry_t animation_entry;

   /* Trigger fade out animation */
   state->alpha                 = 1.0f;

   animation_entry.easing_enum  = EASING_OUT_QUAD;
   animation_entry.tag          = alpha_tag;
   animation_entry.duration     = LOAD_CONTENT_ANIMATION_FADE_OUT_DURATION;
   animation_entry.target_value = 0.0f;
   animation_entry.subject      = &state->alpha;
   animation_entry.cb           = gfx_widget_load_content_animation_fade_out_cb;
   animation_entry.userdata     = NULL;

   gfx_animation_push(&animation_entry);
   state->status = GFX_WIDGET_LOAD_CONTENT_FADE_OUT;
}

static void gfx_widget_load_content_animation_slide_cb(void *userdata)
{
   gfx_widget_load_content_animation_state_t *state = (gfx_widget_load_content_animation_state_t*)userdata;
   gfx_timer_ctx_entry_t timer;

   /* Start wait timer */
   timer.duration = LOAD_CONTENT_ANIMATION_WAIT_DURATION;
   timer.cb       = gfx_widget_load_content_animation_wait_cb;
   timer.userdata = state;

   gfx_animation_timer_start(&state->timer, &timer);
   state->status = GFX_WIDGET_LOAD_CONTENT_WAIT;
}

static void gfx_widget_load_content_animation_fade_in_cb(void *userdata)
{
   gfx_widget_load_content_animation_state_t *state = (gfx_widget_load_content_animation_state_t*)userdata;
   uintptr_t slide_offset_tag                       = (uintptr_t)&state->slide_offset;
   gfx_animation_ctx_entry_t animation_entry;

   /* Trigger slide animation */
   state->slide_offset          = 0.0f;

   animation_entry.easing_enum  = EASING_IN_OUT_QUAD;
   animation_entry.tag          = slide_offset_tag;
   animation_entry.duration     = LOAD_CONTENT_ANIMATION_SLIDE_DURATION;
   animation_entry.target_value = 1.0f;
   animation_entry.subject      = &state->slide_offset;
   animation_entry.cb           = gfx_widget_load_content_animation_slide_cb;
   animation_entry.userdata     = state;

   gfx_animation_push(&animation_entry);
   state->status = GFX_WIDGET_LOAD_CONTENT_SLIDE;
}

/* Widget interface */

bool gfx_widget_start_load_content_animation(void)
{
   gfx_widget_load_content_animation_state_t *state = &p_w_load_content_animation_st;

   const char *content_path                         = path_get(RARCH_PATH_CONTENT);
   const char *core_path                            = path_get(RARCH_PATH_CORE);
   playlist_t *playlist                             = playlist_get_cached();
   core_info_t *core_info                           = NULL;

   bool playlist_entry_found                        = false;
   bool has_content                                 = false;
   bool has_system                                  = false;
   bool has_db_name                                 = false;

   char icon_path[PATH_MAX_LENGTH];

   icon_path[0] = '\0';

   /* To ensure we leave the widget in a well defined
    * state, perform a reset before parsing variables */
   gfx_widget_load_content_animation_reset();

   /* Sanity check - we require both content and
    * core path
    * > Note that we would prefer to enable the load
    *   content animation for 'content-less' cores as
    *   well, but allowing no content would mean we
    *   trigger a false positive every time the dummy
    *   core is started (this higher level behaviour is
    *   deeply ingrained in RetroArch, and too difficult
    *   to change...) */
   if (string_is_empty(content_path) ||
       string_is_empty(core_path) ||
       string_is_equal(core_path, "builtin"))
      return false;

   /* Check core validity */
   if (!core_info_find(core_path, &core_info))
      return false;

   core_path = core_info->path;

   /* Parse content path
    * > If we have a cached playlist, attempt to find
    *   the entry label for the current content */
   if (playlist)
   {
      const struct playlist_entry *entry = NULL;
#ifdef HAVE_MENU
      menu_handle_t *menu                = menu_state_get_ptr()->driver_data;

      /* If we have an active menu, playlist entry
       * index can be obtained directly */
      if (menu)
      {
         if (playlist_index_is_valid(
               playlist, menu->rpl_entry_selection_ptr,
               content_path, core_path))
            playlist_get_index(
                  playlist, menu->rpl_entry_selection_ptr,
                  &entry);
      }
      else
#endif
      {
         /* No menu - have to search playlist... */
         playlist_get_index_by_path(playlist, content_path,
               &entry);

         if (entry &&
             !string_is_empty(entry->core_path))
         {
            const char *entry_core_file = path_basename_nocompression(
                  entry->core_path);

            /* Check whether core matches... */
            if (string_is_empty(entry_core_file) ||
                !string_starts_with(entry_core_file,
                     core_info->core_file_id.str))
               entry = NULL;
         }
      }

      /* If playlist entry is valid, extract all
       * available information */
      if (entry)
      {
         playlist_entry_found = true;

         /* Get entry label */
         if (!string_is_empty(entry->label))
         {
            strlcpy(state->content_name, entry->label,
                  sizeof(state->content_name));
            has_content = true;
         }

         /* Get entry db_name, */
         if (!string_is_empty(entry->db_name))
         {
            strlcpy(state->system_name, entry->db_name,
                  sizeof(state->system_name));
            path_remove_extension(state->system_name);

            has_system  = true;
            has_db_name = true;
         }
      }

      /* If content was found in playlist but the entry
       * did not have a db_name, use playlist name itself
       * as the system name */
      if (playlist_entry_found && !has_system)
      {
         const char *playlist_path = playlist_get_conf_path(playlist);

         if (!string_is_empty(playlist_path))
         {
            fill_pathname_base_noext(state->system_name, playlist_path,
                  sizeof(state->system_name));

            /* Exclude history and favourites playlists */
            if (string_ends_with_size(state->system_name, "_history",
                     strlen(state->system_name), STRLEN_CONST("_history")) ||
                string_ends_with_size(state->system_name, "_favorites",
                     strlen(state->system_name), STRLEN_CONST("_favorites")))
               state->system_name[0] = '\0';

            /* Check whether a valid system name was found */
            if (!string_is_empty(state->system_name))
            {
               has_system  = true;
               has_db_name = true;
            }
         }
      }
   }

   /* If we haven't yet set the content name,
    * use content file name as a fallback */
   if (!has_content)
      fill_pathname_base_noext(state->content_name, content_path,
            sizeof(state->content_name));

   /* Check whether system name has been set */
   if (!has_system)
   {
      /* Use core display name, if available */
      if (!string_is_empty(core_info->display_name))
         strlcpy(state->system_name, core_info->display_name,
               sizeof(state->system_name));
      /* Otherwise, just use 'RetroArch' as a fallback */
      else
         strcpy_literal(state->system_name, "RetroArch");
   }

   /* > Content name has been determined
    * > System name has been determined
    * All that remains is the icon */

   /* Get icon filename
    * > Use db_name, if available */
   if (has_db_name)
   {
      strlcpy(state->icon_file, state->system_name,
            sizeof(state->icon_file));
      strlcat(state->icon_file, ".png",
            sizeof(state->icon_file));

      fill_pathname_join(icon_path,
            state->icon_directory, state->icon_file,
            sizeof(icon_path));

      state->has_icon = path_is_valid(icon_path);
   }

   /* > If db_name is unavailable (or was extracted
    *   from a playlist with non-standard naming),
    *   try to extract a valid system from the core
    *   itself */
   if (!state->has_icon)
   {
      const char *core_db_name           = NULL;
      struct string_list *databases_list =
            core_info->databases_list;

      /* We can only use the core db_name if the
       * core is associated with exactly one database */
      if (databases_list &&
          (databases_list->size == 1))
         core_db_name = databases_list->elems[0].data;

      if (!string_is_empty(core_db_name) &&
          !string_is_equal(core_db_name, state->system_name))
      {
         state->icon_file[0] = '\0';
         icon_path[0]        = '\0';

         strlcpy(state->icon_file, core_db_name,
               sizeof(state->icon_file));
         strlcat(state->icon_file, ".png",
               sizeof(state->icon_file));

         fill_pathname_join(icon_path,
               state->icon_directory, state->icon_file,
               sizeof(icon_path));

         state->has_icon = path_is_valid(icon_path);
      }
   }

   /* > If no system-specific icon is available,
    *   use default 'retroarch' icon as a fallback */
   if (!state->has_icon)
   {
      state->icon_file[0] = '\0';
      icon_path[0]        = '\0';

      strcpy_literal(state->icon_file, "retroarch.png");

      fill_pathname_join(icon_path,
            state->icon_directory, state->icon_file,
            sizeof(icon_path));

      state->has_icon = path_is_valid(icon_path);
   }

   /* All parameters are initialised
    * > Signal that animation should begin */
   state->status = GFX_WIDGET_LOAD_CONTENT_BEGIN;

   return true;
}

/* Widget layout() */

static void gfx_widget_load_content_animation_layout(
      void *data,
      bool is_threaded, const char *dir_assets, char *font_path)
{
   dispgfx_widget_t *p_dispwidget                   = (dispgfx_widget_t*)data;
   gfx_widget_load_content_animation_state_t *state = &p_w_load_content_animation_st;

   unsigned last_video_width                        = p_dispwidget->last_video_width;
   unsigned last_video_height                       = p_dispwidget->last_video_height;
   unsigned widget_padding                          = p_dispwidget->simple_widget_padding;

   gfx_widget_font_data_t *font_regular             = &p_dispwidget->gfx_widget_fonts.regular;
   gfx_widget_font_data_t *font_bold                = &p_dispwidget->gfx_widget_fonts.bold;

   /* Icon layout */
   state->icon_size = (unsigned)((((float)font_regular->line_height +
         (float)font_bold->line_height) * 1.6f) + 0.5f);
   state->icon_x_start = (float)(last_video_width  - state->icon_size) * 0.5f;
   state->icon_y       = (float)(last_video_height - state->icon_size) * 0.5f;
   /* > Note: cannot determine state->icon_x_end
    *   until text strings are set */

   /* Background layout */ 
   state->bg_width  = last_video_width;
   state->bg_height = state->icon_size + (widget_padding * 2);
   state->bg_x      = 0.0f;
   state->bg_y      = (float)(last_video_height - state->bg_height) * 0.5f;

   /* Background shadow layout */
   state->bg_shadow_height   = (unsigned)((float)state->bg_height * 0.3f);
   state->bg_shadow_top_y    = state->bg_y - (float)state->bg_shadow_height;
   state->bg_shadow_bottom_y = state->bg_y + (float)state->bg_height;

   /* Margin shadow layout */
   state->margin_shadow_width   = widget_padding;
   state->margin_shadow_left_x  = 0.0f;
   state->margin_shadow_right_x = (float)(last_video_width - widget_padding);

   /* Text layout */
   state->text_x_start   = state->icon_x_start +
         (float)(state->icon_size + widget_padding);
   state->content_name_y = state->icon_y +
         ((float)state->icon_size * 0.3f) +
         (float)font_bold->line_centre_offset;
   state->system_name_y  = state->icon_y +
         ((float)state->icon_size * 0.7f) +
         (float)font_regular->line_centre_offset;
   /* > Note: cannot determine state->text_x_end
    *   until text strings are set */
}

/* Widget iterate() */

static void gfx_widget_load_content_animation_iterate(void *user_data,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded)
{
   gfx_widget_load_content_animation_state_t *state = &p_w_load_content_animation_st;

   if (state->status == GFX_WIDGET_LOAD_CONTENT_BEGIN)
   {
      dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)user_data;
      unsigned last_video_width            = p_dispwidget->last_video_width;
      unsigned widget_padding              = p_dispwidget->simple_widget_padding;
      gfx_widget_font_data_t *font_regular = &p_dispwidget->gfx_widget_fonts.regular;
      gfx_widget_font_data_t *font_bold    = &p_dispwidget->gfx_widget_fonts.bold;

      uintptr_t alpha_tag                  = (uintptr_t)&state->alpha;

      int content_name_width;
      int system_name_width;
      int text_width;
      gfx_animation_ctx_entry_t animation_entry;

      /* Load icon texture */
      gfx_widget_load_content_animation_load_icon();

      /* Get overall text width */
      content_name_width = font_driver_get_message_width(
            font_bold->font, state->content_name,
            (unsigned)strlen(state->content_name), 1.0f);
      system_name_width = font_driver_get_message_width(
            font_regular->font, state->system_name,
            (unsigned)strlen(state->system_name), 1.0f);

      state->content_name_width = (content_name_width > 0) ?
            (unsigned)content_name_width : 0;
      state->system_name_width  = (system_name_width > 0) ?
            (unsigned)system_name_width : 0;

      text_width = (state->content_name_width > state->system_name_width) ?
            (int)state->content_name_width : (int)state->system_name_width;

      /* Now we have the text width, can determine
       * final icon/text x draw positions */
      state->icon_x_end = ((int)last_video_width - text_width -
            (int)state->icon_size - (3 * (int)widget_padding)) >> 1;
      if (state->icon_x_end < (int)widget_padding)
         state->icon_x_end = widget_padding;

      state->text_x_end = state->icon_x_end +
            (float)(state->icon_size + widget_padding);

      /* Trigger fade in animation */
      state->alpha                 = 0.0f;

      animation_entry.easing_enum  = EASING_OUT_QUAD;
      animation_entry.tag          = alpha_tag;
      animation_entry.duration     = LOAD_CONTENT_ANIMATION_FADE_IN_DURATION;
      animation_entry.target_value = 1.0f;
      animation_entry.subject      = &state->alpha;
      animation_entry.cb           = gfx_widget_load_content_animation_fade_in_cb;
      animation_entry.userdata     = state;

      gfx_animation_push(&animation_entry);
      state->status = GFX_WIDGET_LOAD_CONTENT_FADE_IN;
   }
}

/* Widget frame() */

static void gfx_widget_load_content_animation_frame(void *data, void *user_data)
{
   gfx_widget_load_content_animation_state_t *state = &p_w_load_content_animation_st;

   if (state->status != GFX_WIDGET_LOAD_CONTENT_IDLE)
   {
      float bg_alpha;
      float icon_alpha;
      float text_alpha;

      float icon_x;
      float text_x;
      video_frame_info_t *video_info       = (video_frame_info_t*)data;
      dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)user_data;

      unsigned video_width                 = video_info->width;
      unsigned video_height                = video_info->height;
      void *userdata                       = video_info->userdata;

      gfx_widget_font_data_t *font_regular = &p_dispwidget->gfx_widget_fonts.regular;
      gfx_widget_font_data_t *font_bold    = &p_dispwidget->gfx_widget_fonts.bold;
      size_t msg_queue_size                = p_dispwidget->current_msgs_size;
      gfx_display_t            *p_disp     = state->p_disp;
      gfx_display_ctx_driver_t *dispctx    = p_disp->dispctx;

#ifdef HAVE_MENU
      /* Draw nothing if menu is currently active */
      if (menu_state_get_ptr()->alive)
         return;
#endif

      /* Determine status-dependent opacity/position
       * values */
      switch (state->status)
      {
         case GFX_WIDGET_LOAD_CONTENT_FADE_IN:
            bg_alpha   = 1.0f;
            icon_alpha = state->alpha;
            text_alpha = 0.0f;
            icon_x     = state->icon_x_start;
            text_x     = state->text_x_start;
            break;
         case GFX_WIDGET_LOAD_CONTENT_SLIDE:
            bg_alpha   = 1.0f;
            icon_alpha = 1.0f;
            /* Use 'slide_offset' as the alpha value
             * > Saves having to trigger two animations */
            text_alpha = state->slide_offset;
            icon_x     = state->icon_x_start + (state->slide_offset *
                  (state->icon_x_end - state->icon_x_start));
            text_x     = state->text_x_start + (state->slide_offset *
                  (state->text_x_end - state->text_x_start));
            break;
         case GFX_WIDGET_LOAD_CONTENT_WAIT:
            bg_alpha   = 1.0f;
            icon_alpha = 1.0f;
            text_alpha = 1.0f;
            icon_x     = state->icon_x_end;
            text_x     = state->text_x_end;
            break;
         case GFX_WIDGET_LOAD_CONTENT_FADE_OUT:
            bg_alpha   = state->alpha;
            icon_alpha = state->alpha;
            text_alpha = state->alpha;
            icon_x     = state->icon_x_end;
            text_x     = state->text_x_end;
            break;
         case GFX_WIDGET_LOAD_CONTENT_BEGIN:
         default:
            bg_alpha   = 1.0f;
            icon_alpha = 0.0f;
            text_alpha = 0.0f;
            icon_x     = state->icon_x_start;
            text_x     = state->text_x_start;
            break;
      }

      /* Draw background */
      if (bg_alpha > 0.0f)
      {
         /* > Set opacity */
         state->bg_shadow_top_color[3]     = bg_alpha * state->bg_shadow_alpha;
         state->bg_shadow_top_color[7]     = bg_alpha * state->bg_shadow_alpha;
         state->bg_shadow_bottom_color[11] = bg_alpha * state->bg_shadow_alpha;
         state->bg_shadow_bottom_color[15] = bg_alpha * state->bg_shadow_alpha;

         gfx_display_set_alpha(state->bg_color, bg_alpha * state->bg_alpha);
         gfx_display_set_alpha(state->bg_underlay_color,
               bg_alpha * state->bg_underlay_alpha);

         /* > Background underlay */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               0,
               0,
               video_width,
               video_height,
               video_width,
               video_height,
               state->bg_underlay_color,
               NULL);

         /* > Background shadow */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               state->bg_x,
               state->bg_shadow_top_y,
               state->bg_width,
               state->bg_shadow_height,
               video_width,
               video_height,
               state->bg_shadow_top_color,
               NULL);

         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               state->bg_x,
               state->bg_shadow_bottom_y,
               state->bg_width,
               state->bg_shadow_height,
               video_width,
               video_height,
               state->bg_shadow_bottom_color,
               NULL);

         /* > Background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               state->bg_x,
               state->bg_y,
               state->bg_width,
               state->bg_height,
               video_width,
               video_height,
               state->bg_color,
               NULL);
      }

      /* Draw icon */
      if (icon_alpha > 0.0f)
      {
         gfx_display_set_alpha(state->icon_color, icon_alpha);

         if (state->icon_texture)
         {
            if (dispctx && dispctx->blend_begin)
               dispctx->blend_begin(userdata);

            gfx_widgets_draw_icon(
                  userdata,
                  p_disp,
                  video_width,
                  video_height,
                  state->icon_size,
                  state->icon_size,
                  state->icon_texture,
                  icon_x,
                  state->icon_y,
                  0.0f,
                  1.0f,
                  state->icon_color);

            if (dispctx && dispctx->blend_end)
               dispctx->blend_end(userdata);
         }
         /* If there is no icon, draw a placeholder
          * (otherwise layout will look terrible...) */
         else
            gfx_display_draw_quad(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_x,
                  state->icon_y,
                  state->icon_size,
                  state->icon_size,
                  video_width,
                  video_height,
                  state->icon_color,
		  NULL);
      }

      /* Draw text */
      if (text_alpha > 0.0f)
      {
         unsigned text_alpha_int = (unsigned)(text_alpha * 255.0f);

         /* > Set opacity */
         state->content_name_color = COLOR_TEXT_ALPHA(state->content_name_color,
               text_alpha_int);
         state->system_name_color  = COLOR_TEXT_ALPHA(state->system_name_color,
               text_alpha_int);

         /* > Content name */
         gfx_widgets_draw_text(
               font_bold,
               state->content_name,
               text_x,
               state->content_name_y,
               video_width,
               video_height,
               state->content_name_color,
               TEXT_ALIGN_LEFT,
               true);

         /* > System name */
         gfx_widgets_draw_text(
               font_regular,
               state->system_name,
               text_x,
               state->system_name_y,
               video_width,
               video_height,
               state->system_name_color,
               TEXT_ALIGN_LEFT,
               true);

         /* If the message queue is active, must flush the
          * text here to avoid overlaps */
         if (msg_queue_size > 0)
         {
            gfx_widgets_flush_text(video_width, video_height, font_regular);
            gfx_widgets_flush_text(video_width, video_height, font_bold);
         }
         /* Must also flush text if it overlaps the edge of
          * the screen (otherwise it will bleed through the
          * 'margin' shadows) */
         else
         {
            if (state->system_name_width > video_width -
                  (unsigned)text_x - state->margin_shadow_width)
               gfx_widgets_flush_text(video_width, video_height, font_regular);

            if (state->content_name_width > video_width -
                  (unsigned)text_x - state->margin_shadow_width)
               gfx_widgets_flush_text(video_width, video_height, font_bold);
         }
      }

      /* Draw 'margin' shadows
       * > This ensures rendered text is cleanly
       *   truncated when it exceeds the width of
       *   the screen */
      if (bg_alpha > 0.0f)
      {
         /* > Set opacity */
         state->margin_shadow_left_color[3]   = bg_alpha;
         state->margin_shadow_left_color[11]  = bg_alpha;
         state->margin_shadow_right_color[7]  = bg_alpha;
         state->margin_shadow_right_color[15] = bg_alpha;

         /* > Left */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               state->margin_shadow_left_x,
               state->bg_y,
               state->margin_shadow_width,
               state->bg_height,
               video_width,
               video_height,
               state->margin_shadow_left_color,
	       NULL);

         /* > Right */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               state->margin_shadow_right_x,
               state->bg_y,
               state->margin_shadow_width,
               state->bg_height,
               video_width,
               video_height,
               state->margin_shadow_right_color,
	       NULL);
      }
   }
}

/* Widget context_reset() */

static void gfx_widget_load_content_animation_context_reset(
      bool is_threaded,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      char* menu_png_path,
      char* widgets_png_path)
{
   gfx_widget_load_content_animation_state_t *state = &p_w_load_content_animation_st;

   /* Cache icon directory */
   if (string_is_empty(menu_png_path))
      state->icon_directory[0] = '\0';
   else
      strlcpy(state->icon_directory, menu_png_path,
            sizeof(state->icon_directory));

   /* Reload icon texture */
   gfx_widget_load_content_animation_load_icon();
}

/* Widget context_destroy() */

static void gfx_widget_load_content_animation_context_destroy(void)
{
   gfx_widget_load_content_animation_state_t *state = &p_w_load_content_animation_st;

   /* Unload any icon texture */
   if (state->icon_texture)
   {
      video_driver_texture_unload(&state->icon_texture);
      state->icon_texture = 0;
   }
}

/* Widget free() */

static void gfx_widget_load_content_animation_free(void)
{
   gfx_widget_load_content_animation_reset();
}

static bool gfx_widget_load_content_animation_init(
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      bool video_is_threaded, bool fullscreen)
{
   gfx_widget_load_content_animation_state_t *state = 
      &p_w_load_content_animation_st;

   state->p_disp = p_disp;

   return false;
}
/* Widget definition */

const gfx_widget_t gfx_widget_load_content_animation = {
   gfx_widget_load_content_animation_init,
   gfx_widget_load_content_animation_free,
   gfx_widget_load_content_animation_context_reset,
   gfx_widget_load_content_animation_context_destroy,
   gfx_widget_load_content_animation_layout,
   gfx_widget_load_content_animation_iterate,
   gfx_widget_load_content_animation_frame
};
