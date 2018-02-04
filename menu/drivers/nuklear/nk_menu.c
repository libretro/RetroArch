/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017 - Andrés Suárez
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

 /*  This file is intended for menu functions, custom controls, etc. */

#include "nk_menu.h"

/* sets window position and size */
void nk_wnd_set_state(nk_menu_handle_t *nk, const int id,
   struct nk_vec2 pos, struct nk_vec2 size)
{
   nk->window[id].position = pos;
   nk->window[id].size = size;
}

/* gets window position and size */
void nk_wnd_get_state(nk_menu_handle_t *nk, const int id,
   struct nk_vec2 *pos, struct nk_vec2 *size)
{
   *pos = nk->window[id].position;
   *size = nk->window[id].size;
}

/* sets the theme */
void nk_common_set_style(struct nk_context *ctx)
{
   /* standard nuklear colors */
   nk_colors[NK_COLOR_TEXT] = nk_rgba(158, 158, 158, 255);
   nk_colors[NK_COLOR_WINDOW] = nk_rgba(57, 67, 71, 215);
   nk_colors[NK_COLOR_HEADER] = nk_rgba(51, 51, 56, 220);
   nk_colors[NK_COLOR_BORDER] = nk_rgba(46, 46, 46, 255);
   nk_colors[NK_COLOR_BUTTON] = nk_rgba(255, 112, 67, 255);
   nk_colors[NK_COLOR_BUTTON_HOVER] = nk_rgba(58, 93, 121, 255);
   nk_colors[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(63, 98, 126, 255);
   nk_colors[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 255);
   nk_colors[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 53, 56, 255);
   nk_colors[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 255);
   nk_colors[NK_COLOR_SELECT] = nk_rgba(57, 67, 61, 255);
   nk_colors[NK_COLOR_SELECT_ACTIVE] = nk_rgba(48, 83, 111, 255);
   nk_colors[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 255);
   nk_colors[NK_COLOR_SLIDER_CURSOR] = nk_rgba(48, 83, 111, 245);
   nk_colors[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
   nk_colors[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
   nk_colors[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 255);
   nk_colors[NK_COLOR_EDIT] = nk_rgba(50, 58, 61, 225);
   nk_colors[NK_COLOR_EDIT_CURSOR] = nk_rgba(210, 210, 210, 255);
   nk_colors[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 255);
   nk_colors[NK_COLOR_CHART] = nk_rgba(50, 58, 61, 255);
   nk_colors[NK_COLOR_CHART_COLOR] = nk_rgba(48, 83, 111, 255);
   nk_colors[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
   nk_colors[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 0);
   nk_colors[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 0);
   nk_colors[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 50);
   nk_colors[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 100);
   nk_colors[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 255);
   nk_style_from_table(ctx, nk_colors);

   /* style */
   ctx->style.button.text_alignment = NK_TEXT_ALIGN_CENTERED;
}
