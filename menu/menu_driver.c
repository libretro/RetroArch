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

#include <retro_timers.h>
#include "menu_driver.h"

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

static bool menu_input_key_bind_poll_find_hold(
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

               input_config_get_bind_string(desc[i],
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
