/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
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

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <file/file_path.h>
#include <file/dir_list.h>
#include <compat/posix_string.h>
#include <retro_log.h>
#include <retro_stat.h>

#include "menu_generic.h"

#include "../../config.def.h"

#include "../menu.h"
#include "../menu_animation.h"
#include "../menu_entry.h"
#include "../menu_display.h"
#include "../menu_hash.h"
#include "../../runloop_data.h"

#include "../../gfx/video_thread_wrapper.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/video_texture.h"

const GRfloat ZUI_NORMAL[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};
const GRfloat ZUI_HILITE[] = {
   1, 0, 0, 1,
   1, 0, 0, 1,
   1, 0, 0, 1,
   1, 0, 0, 1,
};
const GRfloat ZUI_PRESS[] = {
   0, 1, 0, 1,
   0, 1, 0, 1,
   0, 1, 0, 1,
   0, 1, 0, 1,
};
const GRfloat ZUI_BARBG[] = {
   0, 0, 1, 1,
   0, 0, 1, 1,
   0, 0, 1, 1,
   0, 0, 1, 1,
};

const uint32_t ZUI_FG_NORMAL = ~0;
const GRfloat ZUI_BG_PANEL[] = {
   0, 0, 0, 0.25,
   0, 0, 0, 0.25,
   0, 0, 0, 0.25,
   0, 0, 0, 0.25,
};
const GRfloat ZUI_BG_SCREEN[] = {
   0.07, 0.19, 0.26, 0.75,
   0.07, 0.19, 0.26, 0.75,
   0.15, 0.31, 0.47, 0.75,
   0.15, 0.31, 0.47, 0.75,
};
const GRfloat ZUI_BG_HILITE[] = {
   0.22, 0.60, 0.74, 1,
   0.22, 0.60, 0.74, 1,
   0.22, 0.60, 0.74, 1,
   0.22, 0.60, 0.74, 1,
};

const GRfloat ZUI_BG_PAD_HILITE[] = {
   0.30, 0.76, 0.93, 1,
   0.30, 0.76, 0.93, 1,
   0.30, 0.76, 0.93, 1,
   0.30, 0.76, 0.93, 1,
};

#define ZUI_MAX_MAINMENU_TABS 4

typedef struct zarch_handle
{
   ssize_t header_selection;
   size_t entries_selection;
   bool rendering;
   menu_handle_t *menu;
   math_matrix_4x4 mvp;
   unsigned width;
   unsigned height;
   gfx_font_raster_block_t tmp_block;
   unsigned hash;
   bool time_to_exit;
   bool time_to_quit;

   struct {
      bool enable;
      size_t idx;
   } pending_action_ok;

   struct {
      unsigned active;
      unsigned hot;
   } item;

   gfx_coord_array_t ca;

   struct
   {
      int    wheel;
   } mouse;

   /* LAY_ROOT's "Load" file browser */
   struct string_list *load_dlist;
   char *load_cwd;
   int  load_dlist_first;

   struct
   {
      struct
      {
         GRuint id;
         char path[PATH_MAX_LENGTH];
      } bg;

      GRuint white;
   } textures;

   /* LAY_ROOT's "Recent" */

   int  recent_dlist_first;

   /* LAY_PICK_CORE */
   int pick_first;
   char pick_content[PATH_MAX_LENGTH];
   const core_info_t *pick_cores;
   size_t pick_supported;

   void *fb_buf;
   int font_size;
   int header_height;
} zui_t;

typedef struct
{
   unsigned active;
   int x, y;
   int width;
   int height; /* unused */
   int tabline_size;
   bool update_width;
   bool vertical;
   int tab_width;
} zui_tabbed_t;

typedef struct
{
   float x, y;
   float xspeed, yspeed;
   float alpha;
   bool alive;
} part_t;

static enum
{
   LAY_HOME,
   LAY_PICK_CORE,
   LAY_SETTINGS
} layout = LAY_HOME;

static const GRfloat zarch_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GRfloat zarch_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static void zarch_zui_font(menu_handle_t *menu)
{
   int font_size;
   char mediapath[PATH_MAX_LENGTH], fontpath[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_SIZE, &font_size);

   fill_pathname_join(mediapath, settings->assets_directory, "zarch", sizeof(mediapath));
   fill_pathname_join(fontpath, mediapath, "Roboto-Condensed.ttf", sizeof(fontpath));

   if (!menu_display_init_main_font(menu, fontpath, font_size))
      RARCH_WARN("Failed to load font.");
}

static float zarch_zui_strwidth(void *fb_buf, const char *text, float scale)
{
   driver_t *driver = (driver_t*)driver_get_ptr();
   return driver->font_osd_driver->get_message_width(fb_buf, text, strlen(text), scale);
}

enum zarch_zui_input_state
{
    MENU_ZARCH_MOUSE_X = 0,
    MENU_ZARCH_MOUSE_Y,
    MENU_POINTER_ZARCH_X,
    MENU_POINTER_ZARCH_Y,
    MENU_ZARCH_PRESSED
};

static int16_t zarch_zui_input_state(enum zarch_zui_input_state state)
{
    switch (state)
    {
        case MENU_ZARCH_MOUSE_X:
            return menu_input_mouse_state(MENU_MOUSE_X_AXIS);
        case MENU_ZARCH_MOUSE_Y:
            return menu_input_mouse_state(MENU_MOUSE_Y_AXIS);
        case MENU_POINTER_ZARCH_X:
            return menu_input_pointer_state(MENU_POINTER_X_AXIS);
        case MENU_POINTER_ZARCH_Y:
            return menu_input_pointer_state(MENU_POINTER_Y_AXIS);
        case MENU_ZARCH_PRESSED:
            if (menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON) || menu_input_pointer_state(MENU_POINTER_PRESSED))
                return 1;
            break;
    }
    
    return 0;
}

static bool zarch_zui_check_button_down(zui_t *zui, unsigned id, int x1, int y1, int x2, int y2)
{
   bool result = false;
   bool inside = menu_input_mouse_check_hitbox(x1, y1, x2, y2);

   if (inside)
      zui->item.hot = id;

   if (zui->item.hot == id && zarch_zui_input_state(MENU_ZARCH_PRESSED))
   {
      result = true;
      zui->item.active = id;
   }

   return result;
}

static bool zarch_zui_check_button_up(zui_t *zui, unsigned id, int x1, int y1, int x2, int y2)
{
   bool result = false;
   bool inside = menu_input_mouse_check_hitbox(x1, y1, x2, y2);

   if (inside)
      zui->item.hot = id;

   if (zui->item.active == id && !zarch_zui_input_state(MENU_ZARCH_PRESSED))
   {
      if (zui->item.hot == id)
         result = true;

      zui->item.active = 0;
   }

   zarch_zui_check_button_down(zui, id, x1, y1, x2, y2);

   return result;
}

static unsigned zarch_zui_hash(zui_t *zui, const char *s)
{
   unsigned hval = zui->hash;

   while(*s!=0)
   {
      hval+=*s++;
      hval+=(hval<<10);
      hval^=(hval>>6);
      hval+=(hval<<3);
      hval^=(hval>>11);
      hval+=(hval<<15);
   }
   return zui->hash = hval;
}

static void zarch_zui_draw_text(zui_t *zui, uint32_t color, int x, int y, const char *text)
{
   struct font_params params = {0};

   /* need to use height-y because the gl font renderer uses a different mvp */
   params.x           = x / (float)zui->width;
   params.y           = (zui->height - y) / (float)zui->height;
   params.scale       = 1.0;
   params.color       = color;
   params.full_screen = true;
   params.text_align  = TEXT_ALIGN_LEFT;

   video_driver_set_osd_msg(text, &params, zui->fb_buf);
}

static void zarch_zui_push_quad(zui_t *zui, const GRfloat *colors, int x1, int y1,
      int x2, int y2)
{
   gfx_coords_t coords;
   GRfloat vertex[8];
   GRfloat tex_coord[8];

   tex_coord[0] = 0;
   tex_coord[1] = 1;
   tex_coord[2] = 1;
   tex_coord[3] = 1;
   tex_coord[4] = 0;
   tex_coord[5] = 0;
   tex_coord[6] = 1;
   tex_coord[7] = 0;

   vertex[0] = x1 / (float)zui->width;
   vertex[1] = y1 / (float)zui->height;
   vertex[2] = x2 / (float)zui->width;
   vertex[3] = y1 / (float)zui->height;
   vertex[4] = x1 / (float)zui->width;
   vertex[5] = y2 / (float)zui->height;
   vertex[6] = x2 / (float)zui->width;
   vertex[7] = y2 / (float)zui->height;

   coords.color         = colors;
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;
   coords.vertices      = 3;

   gfx_coord_array_add(&zui->ca, &coords, 3);

   coords.color         += 4;
   coords.vertex        += 2;
   coords.tex_coord     += 2;
   coords.lut_tex_coord += 2;

   gfx_coord_array_add(&zui->ca, &coords, 3);
}

static float zarch_zui_randf(float min, float max)
{
   return (rand() * ((max - min) / RAND_MAX)) + min;
}

static float zarch_zui_scalef(float val, float oldmin, float oldmax, float newmin, float newmax)
{
   return (((val - oldmin) * (newmax - newmin)) / (oldmax - oldmin)) + newmin;
}

#define NPARTICLES 100

static void zarch_zui_snow(zui_t *zui)
{
   static part_t particles[NPARTICLES];
   static bool initialized = false;
   static int timeout      = 0;
   unsigned i, max_gen  = 2;

   if (!initialized)
   {
      memset(particles, 0, sizeof(particles));
      initialized = true;
   }

   for (i = 0; i < NPARTICLES; ++i)
   {
      part_t *p = (part_t*)&particles[i];

      if (p->alive)
      {
         int16_t mouse_x  = zarch_zui_input_state(MENU_ZARCH_MOUSE_X);

         p->y            += p->yspeed;
         p->x            += zarch_zui_scalef(mouse_x, 0, zui->width, -0.3, 0.3) + p->xspeed;

         p->alive         = p->y >= 0 && p->y < (int)zui->height && p->x >= 0 && p->x < (int)zui->width;


      }
      else if (max_gen > 0 && timeout <= 0)
      {
         p->xspeed = zarch_zui_randf(-0.2, 0.2);
         p->yspeed = zarch_zui_randf(1, 2);
         p->y      = 0;
         p->x      = rand() % (int)zui->width;
         p->alpha  = (float)rand() / (float)RAND_MAX;
         p->alive  = true;

         max_gen--;
      }
   }

   if (max_gen == 0)
      timeout = 3;
   else
      timeout--;

   for (i = 0; i < NPARTICLES; ++i)
   {
      unsigned j;
      GRfloat alpha;
      GRfloat colors[16];
      part_t *p = &particles[i];

      if (!p->alive)
         continue;

      alpha = zarch_zui_randf(0, 100) > 90 ? p->alpha/2 : p->alpha;

      for (j = 0; j < 16; j++)
      {
         colors[j] = 1;
         if (j == 3 || j == 7 || j == 11 || j == 15)
            colors[j] = alpha;
      }

      zarch_zui_push_quad(zui, colors, p->x-2, p->y-2, p->x+2, p->y+2);

      j++;
   }
}

static bool zarch_zui_button_full(zui_t *zui, int x1, int y1, int x2, int y2, const char *label)
{
   unsigned       id = zarch_zui_hash(zui, label);
   bool       active = zarch_zui_check_button_up(zui, id, x1, y1, x2, y2);
   const GRfloat *bg = ZUI_BG_PANEL;

   if (zui->item.active == id || zui->item.hot == id)
      bg = ZUI_BG_HILITE;

   zarch_zui_push_quad(zui, bg, x1, y1, x2, y2);
   zarch_zui_draw_text(zui, ZUI_FG_NORMAL, x1+12, y1 + 41, label);

   return active;
}

static bool zarch_zui_button(zui_t *zui, int x1, int y1, const char *label)
{
   return zarch_zui_button_full(zui, x1, y1, x1 + zarch_zui_strwidth(zui->fb_buf, label, 1.0) + 24, y1 + 64, label);
}

static bool zarch_zui_list_item(zui_t *zui, int x1, int y1,
      const char *label, bool selected, const char *entry)
{
   char title_buf[PATH_MAX_LENGTH];
   unsigned ticker_size;
   unsigned           id = zarch_zui_hash(zui, label);
   int                x2 = x1 + zui->width - 290 - 40;
   int                y2 = y1 + 50;
   bool           active = zarch_zui_check_button_up(zui, id, x1, y1, x2, y2);
   const GRfloat     *bg = ZUI_BG_PANEL;
   uint64_t *frame_count = video_driver_get_frame_count();

   if (zui->item.active == id || zui->item.hot == id)
      bg = ZUI_BG_HILITE;
   else if (selected)
      bg = ZUI_BG_PAD_HILITE;

   ticker_size = x2 / 14;

   menu_animation_ticker_str(title_buf,
         ticker_size,
         *frame_count / 50,
         label,
         (bg == ZUI_BG_HILITE || bg == ZUI_BG_PAD_HILITE));

   zarch_zui_push_quad(zui, bg, x1, y1, x2, y2);
   zarch_zui_draw_text(zui, ZUI_FG_NORMAL, 12, y1 + 35, title_buf);

   if (entry)
      zarch_zui_draw_text(zui, ZUI_FG_NORMAL, x2 - 200, y1 + 35, entry);

   return active;
}

static void zarch_zui_tabbed_begin(zui_t *zui, zui_tabbed_t *tab, int x, int y)
{
   tab->x            = x;
   tab->y            = y;
   tab->tabline_size = 60 + 4;
}

static bool zarch_zui_tab(zui_t *zui, zui_tabbed_t *tab, const char *label, bool selected)
{
   bool active;
   int x1, y1, x2, y2;
   unsigned       id = zarch_zui_hash(zui, label);
   int         width = tab->tab_width;
   const GRfloat *bg = ZUI_BG_PANEL;

   if (!width)
      width          = zarch_zui_strwidth(zui->fb_buf, label, 1.0) + 24;

   x1                = tab->x;
   y1                = tab->y;
   x2                = x1 + width;
   y2                = y1 + 60;
   active            = zarch_zui_check_button_up(zui, id, x1, y1, x2, y2);

   if (zui->item.active == id || tab->active == ~0 || selected)
      tab->active    = id;

   if (selected)
      bg             = ZUI_BG_PAD_HILITE;
   else if (tab->active == id || zui->item.active == id || zui->item.hot == id)
      bg             = ZUI_BG_HILITE;

   zarch_zui_push_quad(zui, bg, x1+0, y1+0, x2, y2);
   zarch_zui_draw_text(zui, ZUI_FG_NORMAL, x1+12, y1 + 41, label);

   if (tab->vertical)
      tab->y        += y2 - y1;
   else
      tab->x         = x2;

   return (active || (tab->active == id) || selected);
}


static void zarch_zui_render_lay_settings(zui_t *zui)
{
   int width, x1, y1;
   static zui_tabbed_t tabbed = {~0};
   tabbed.vertical            = true;
   tabbed.tab_width           = 100;

   zarch_zui_tabbed_begin(zui, &tabbed, zui->width - 100, 20);

   width                      = 290;
   x1                         = zui->width - width - 20;
   y1                         = 20;
   y1                        += 64;

   if (zarch_zui_button_full(zui, x1, y1, x1 + width, y1 + 64, "Back"))
      layout = LAY_HOME;
}

static int zarch_zui_render_lay_root_recent(zui_t *zui, zui_tabbed_t *tabbed)
{
   if (zarch_zui_tab(zui, tabbed, "Recent", (zui->header_selection == 0)))
   {
      size_t    end = menu_entries_get_end();
      unsigned size = min(zui->height/30-2, end);
      unsigned i, j = 0;

      zui->recent_dlist_first += zui->mouse.wheel;

      if (zui->recent_dlist_first < 0)
         zui->recent_dlist_first = 0;
      else if (zui->recent_dlist_first > size - 5)
         zui->recent_dlist_first = size - 5;

      zui->recent_dlist_first = min(max(zui->recent_dlist_first, 0), size - 5);

      for (i = zui->recent_dlist_first; i < size; ++i)
      {
         menu_entry_t entry;
         int                x2 = 12 + zui->width - 290 - 50;

         menu_entries_get(i, &entry);

         if (zarch_zui_list_item(zui, 0, tabbed->tabline_size + j * 54,
                  entry.path, zui->entries_selection == i, entry.value))
         {
            zui->pending_action_ok.enable      = true;
            zui->pending_action_ok.idx         = i;
            return 1;
         }

         j++;
      }
   }

   return 0;
}

static int zarch_zui_render_lay_root_load(zui_t *zui, zui_tabbed_t *tabbed)
{
   settings_t *settings = config_get_ptr();
   global_t     *global = global_get_ptr();

   if (zarch_zui_tab(zui, tabbed, "Load", (zui->header_selection == 1)))
   {
      unsigned cwd_offset;

      if (!zui->load_cwd)
      {
         zui->load_cwd = strdup(settings->menu_content_directory);
         if (zui->load_cwd[strlen(zui->load_cwd)-1] == '/'
#ifdef _WIN32
               || (zui->load_cwd[strlen(zui->load_cwd)-1] == '\\')
#endif
            )
            zui->load_cwd[strlen(zui->load_cwd)-1] = 0;
      }

      if (!zui->load_dlist)
      {
         zui->load_dlist = dir_list_new(zui->load_cwd, global->core_info.current->supported_extensions, true, true);
         dir_list_sort(zui->load_dlist, true);
         zui->load_dlist_first = 0;
      }

      cwd_offset = min(strlen(zui->load_cwd), 30);

      zarch_zui_draw_text(zui, ZUI_FG_NORMAL, 15, tabbed->tabline_size + 5 + 41, &zui->load_cwd[strlen(zui->load_cwd) - cwd_offset]);

      if (zarch_zui_button(zui, zui->width - 290 - 129, tabbed->tabline_size + 5, "Home"))
      {
         char tmp[PATH_MAX_LENGTH];

         fill_pathname_expand_special(tmp, "~", sizeof(tmp));

         free(zui->load_cwd);
         zui->load_cwd = strdup(tmp);

         dir_list_free(zui->load_dlist);
         zui->load_dlist = NULL;
      }

      if (zui->load_dlist)
      {
         if (zarch_zui_list_item(zui, 0, tabbed->tabline_size + 73, " ..", false, NULL /* TODO/FIXME */))
         {
            path_basedir(zui->load_cwd);
            if (zui->load_cwd[strlen(zui->load_cwd)-1] == '/'
#ifdef _WIN32
                  || (zui->load_cwd[strlen(zui->load_cwd)-1] == '\\')
#endif
               )
               zui->load_cwd[strlen(zui->load_cwd)-1] = 0;

            dir_list_free(zui->load_dlist);
            zui->load_dlist = NULL;
         }
         else
         {
            unsigned size = zui->load_dlist->size;
            unsigned i, j = 1;
            unsigned skip = 0;

            for (i = 0; i < size; ++i)
            {
               const char *basename = path_basename(zui->load_dlist->elems[i].data);
               if (basename[0] == '.')
                  skip++;
               else
                  break;
            }

            zui->load_dlist_first += zui->mouse.wheel;

            if (zui->load_dlist_first < 0)
               zui->load_dlist_first = 0;
            else if (zui->load_dlist_first > size - 5)
               zui->load_dlist_first = size - 5;

            zui->load_dlist_first = min(max(zui->load_dlist_first, 0), size - 5 - skip);

            for (i = skip + zui->load_dlist_first; i < size; ++i)
            {
               char label[PATH_MAX_LENGTH];
               const char *path     = NULL;
               const char *basename = NULL;

               if (j > 10)
                  break;

               path = zui->load_dlist->elems[i].data;
               basename = path_basename(path);

               *label = 0;
               strncat(label, "  ", sizeof(label)-1);
               strncat(label, basename, sizeof(label)-1);

               if (path_is_directory(path))
                  strncat(label, "/", sizeof(label)-1);

               if (zarch_zui_list_item(zui, 0, tabbed->tabline_size + 73 + j * 54,
                        label, zui->entries_selection == i, NULL))
               {
                  if (path_is_directory(path))
                  {
                     free(zui->load_cwd);
                     zui->load_cwd = strdup(path);
                     dir_list_free(zui->load_dlist);
                     zui->load_dlist = NULL;
                     break;
                  }
                  else
                  {
                     zui->pick_cores     = NULL;
                     zui->pick_supported = 0;
                     strncpy(zui->pick_content, path, sizeof(zui->pick_content)-1);
                     core_info_list_get_supported_cores(global->core_info.list, path,
                           &zui->pick_cores, &zui->pick_supported);
                     layout = LAY_PICK_CORE;
                     break;
                  }
               }
               j++;
            }
         }
      }
   }
   else if (zui->load_dlist)
   {
      dir_list_free(zui->load_dlist);
      zui->load_dlist = NULL;
   }

   return 0;
}

static int zarch_zui_render_lay_root_collections(zui_t *zui, zui_tabbed_t *tabbed)
{
   if (zarch_zui_tab(zui, tabbed, "Collections", zui->header_selection == 2))
   {
      /* STUB/FIXME */
   }

   return 0;
}

static int zarch_zui_render_lay_root_downloads(zui_t *zui, zui_tabbed_t *tabbed)
{
   if (zarch_zui_tab(zui, tabbed, "Download", zui->header_selection == 3))
   {
      /* STUB/FIXME */
   }

   return 0;
}

static int zarch_zui_render_lay_root(zui_t *zui)
{
   static zui_tabbed_t tabbed = {~0};

   zarch_zui_tabbed_begin(zui, &tabbed, 0, 0);

   tabbed.width = zui->width - 290 - 40;

   if (zarch_zui_render_lay_root_recent(zui, &tabbed))
      return 0;
   if (zarch_zui_render_lay_root_load  (zui, &tabbed))
      return 0;
   if (zarch_zui_render_lay_root_collections(zui, &tabbed))
      return 0;
   if (zarch_zui_render_lay_root_downloads(zui, &tabbed))
      return 0;

   zarch_zui_push_quad(zui, ZUI_BG_HILITE, 0, 60, zui->width - 290 - 40, 60+4);

   return 0;
}

static int zarch_zui_render_sidebar(zui_t *zui)
{
   int width, x1, y1;
   static zui_tabbed_t tabbed = {~0};
   tabbed.vertical = true;
   tabbed.tab_width = 100;

   zarch_zui_tabbed_begin(zui, &tabbed, zui->width - 100, 20);

   width = 290;
   x1    = zui->width - width - 20;
   y1    = 20;

   if (zarch_zui_button_full(zui, x1, y1, x1 + width, y1 + 64, "Settings"))
      layout = LAY_SETTINGS;

   y1 += 64;

   if (zarch_zui_button_full(zui, x1, y1, x1 + width, y1 + 64, "Exit"))
   {
      zui->time_to_quit = true;
      return 1;
   }

   return 0;
}

static int zarch_zui_load_content(zui_t *zui, unsigned i)
{
   int ret = menu_common_load_content(zui->pick_cores[i].path,
         zui->pick_content, false, CORE_TYPE_PLAIN);

   layout = LAY_HOME;

   return ret;
}

static void zarch_zui_draw_cursor(gl_t *gl, float x, float y)
{
}

static void zarch_get_message(const char *message)
{

}

static void zarch_render(void)
{
   int bottom;
   unsigned width, height;
   zui_t         *zarch = NULL;
   menu_handle_t *menu  = menu_driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!menu || !menu->userdata)
      return;
    
   (void)settings;
   (void)bottom;
   (void)zarch;

   video_driver_get_size(&width, &height);

}

static void zarch_frame(void)
{
   unsigned i;
   GRfloat coord_color[16];
   GRfloat coord_color2[16];
   zui_t *zui           = NULL;
   const struct font_renderer *font_driver = NULL;
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   menu_handle_t *menu  = menu_driver_get_ptr();
   gl_t *gl             = (gl_t*)video_driver_get_ptr(NULL);
   
   if (!menu || !gl)
      return;

   (void)driver;
   
   zui      = (zui_t*)menu->userdata;
   zui->menu = menu;

   video_driver_get_size(&zui->width, &zui->height);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);
   menu_display_ctl(MENU_DISPLAY_CTL_FONT_BUF, &zui->fb_buf);

   for (i = 0; i < 16; i++)
   {
      coord_color[i]  = 0;
      coord_color2[i] = 2.0f;

      if (i == 3 || i == 7 || i == 11 || i == 15)
      {
         coord_color[i]  = 0.10f;
         coord_color2[i] = 0.10f;
      }
   }

   zui->rendering = true;
   zui->hash      = 0;
   zui->item.hot  = 0;

   /* why do i need this? */
   zui->mouse.wheel = menu_input_mouse_state(MENU_MOUSE_WHEEL_DOWN) - 
      menu_input_mouse_state(MENU_MOUSE_WHEEL_UP);

   zui->ca.coords.vertices = 0;

   zui->tmp_block.carr.coords.vertices = 0;

   font_driver = driver->font_osd_driver;

   menu_display_font_bind_block(zui->menu, font_driver, &zui->tmp_block);

   zarch_zui_push_quad(zui, ZUI_BG_SCREEN, 0, 0, zui->width, zui->height);
   zarch_zui_snow(zui);

   switch (layout)
   {
      case LAY_HOME:
         if (zarch_zui_render_sidebar(zui))
            return;
         if (zarch_zui_render_lay_root(zui))
            return;
         break;
      case LAY_SETTINGS:
         zarch_zui_render_lay_settings(zui);
         break;
      case LAY_PICK_CORE:
         if (zarch_zui_render_sidebar(zui))
            return;
         if (zui->pick_supported == 1)
         {
            int ret = zarch_zui_load_content(zui, 0);

            (void)ret;

            layout = LAY_HOME;
            zui->time_to_exit = true;
            return;
         }
         else
         {
            zarch_zui_draw_text(zui, ~0, 8, 18, "Select a core: ");

            if (zarch_zui_button(zui, 0, 18 + zui->font_size, "<- Back"))
               layout = LAY_HOME;

            if (zui->pick_supported)
            {
               unsigned i, j = 0;
               zui->pick_first += zui->mouse.wheel;

               zui->pick_first = min(max(zui->pick_first, 0), zui->pick_supported - 5);

               for (i = zui->pick_first; i < zui->pick_supported; ++i)
               {
                  if (j > 10)
                     break;

                  if (zarch_zui_list_item(zui, 0, 54 + j * 54,
                           zui->pick_cores[i].display_name, zui->entries_selection == i, NULL))
                  {
                     int ret = zarch_zui_load_content(zui, i);

                     (void)ret;

                     layout = LAY_HOME;

                     zui->time_to_exit = true;
                     break;
                  }
                  j++;
               }
            }
            else
            {
               zarch_zui_list_item(zui, 0, 54, "Content unsupported", false, NULL /* TODO/FIXME */);
            }
         }
         break;
   }

   /* fetch it again in case the pointer was invalidated by a core load */
   gl                   = (gl_t*)video_driver_get_ptr(NULL);

   if (settings->menu.mouse.enable)
      zarch_zui_draw_cursor(gl, zarch_zui_input_state(MENU_ZARCH_MOUSE_X), zarch_zui_input_state(MENU_ZARCH_MOUSE_Y));
         

   gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   if (!zarch_zui_input_state(MENU_ZARCH_PRESSED))
      zui->item.active = 0;
   else if (zui->item.active == 0)
      zui->item.active = -1;

   menu_display_draw_frame(
         0,
         0,
         zui->width,
         zui->height,
         gl->shader, (struct gfx_coords*)&zui->ca,
         &zui->mvp, true, zui->textures.white, zui->ca.coords.vertices,
         MENU_DISPLAY_PRIM_TRIANGLES);

   menu_display_frame_background(menu, settings,
         gl, zui->width, zui->height,
         zui->textures.bg.id, 0.75f, false,
         &coord_color[0],   &coord_color2[0],
         &zarch_vertexes[0], &zarch_tex_coords[0], 4,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);

   menu_display_font_flush_block(zui->menu, driver->font_osd_driver);

   zui->rendering = false;

   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}

static void *zarch_init(void)
{
   int unused;
   zui_t *zui                              = NULL;
   const video_driver_t *video_driver      = NULL;
   menu_handle_t                     *menu = NULL;
   settings_t *settings                    = config_get_ptr();
   gl_t *gl                                = (gl_t*)
      video_driver_get_ptr(&video_driver);

   if (video_driver != &video_gl || !gl)
   {
      RARCH_ERR("Cannot initialize GLUI menu driver: gl video driver is not active.\n");
      return NULL;
   }

   if (settings->menu.mouse.enable)
   {
      RARCH_WARN("Forcing menu_mouse_enable=false\n");
      settings->menu.mouse.enable = false;
   }

   menu                 = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

   menu->userdata       = (zui_t*)calloc(1, sizeof(zui_t));

   if (!menu->userdata)
      goto error;

   zui                  = (zui_t*)menu->userdata;

   unused = 1000;
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEADER_HEIGHT, &unused);

   unused = 28;
   menu_display_ctl(MENU_DISPLAY_CTL_SET_FONT_SIZE, &unused);

   (void)unused;

   zui->header_height  = 1000; /* dpi / 3; */
   zui->font_size       = 28;

   if (settings->menu.wallpaper[0] != '\0')
      rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE,
            settings->menu.wallpaper, "cb_menu_wallpaper", 0, 1, true);

   zui->ca.allocated     =  0;
   zui->header_selection = -1;

   matrix_4x4_ortho(&zui->mvp, 0, 1, 1, 0, 0, 1);

   zarch_zui_font(menu);

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void zarch_free(void *data)
{
   menu_handle_t *menu                     = (menu_handle_t*)data;
   driver_t      *driver                   = driver_get_ptr();
   zui_t        *zarch                     = (zui_t*)menu->userdata;
   const struct font_renderer *font_driver = 
      (const struct font_renderer*)driver->font_osd_driver;

   if (!zarch || !menu)
      return;

   gfx_coord_array_free(&zarch->tmp_block.carr);

   if (menu->userdata)
      free(menu->userdata);
   menu->userdata = NULL;

   if (font_driver->bind_block)
      font_driver->bind_block(driver->font_osd_data, NULL);

}

static void zarch_context_bg_destroy(zui_t *zarch)
{
   video_texture_unload((uintptr_t*)&zarch->textures.bg.id);
   video_texture_unload((uintptr_t*)&zarch->textures.white);
}

static void zarch_context_destroy(void)
{
   menu_handle_t *menu   = menu_driver_get_ptr();
   driver_t      *driver = driver_get_ptr();
   zui_t        *zarch   = menu ? (zui_t*)menu->userdata : NULL;
    
   if (!menu || !zarch || !driver)
      return;

   menu_display_free_main_font();

   zarch_context_bg_destroy(zarch);
}

static bool zarch_load_image(void *data, menu_image_type_t type)
{
   zui_t        *zarch = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return false;
   
   zarch = (zui_t*)menu->userdata;

   if (!zarch || !data)
      return false;

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         zarch_context_bg_destroy(zarch);
         zarch->textures.bg.id   = video_texture_load(data,
               TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);
         break;
      case MENU_IMAGE_BOXART:
         break;
   }

   return true;
}

static void zarch_allocate_white_texture(zui_t *zui)
{
   struct texture_image ti;
   static const uint32_t data = UINT32_MAX;

   ti.width  = 1;
   ti.height = 1;
   ti.pixels = (uint32_t*)&data;

   zui->textures.white = video_texture_load(&ti,
         TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_NEAREST);
}

static void zarch_context_reset(void)
{
   zui_t          *zui   = NULL;
   menu_handle_t *menu   = menu_driver_get_ptr();
   settings_t *settings  = config_get_ptr();
   const char *font_path = NULL;

   if (!menu || !menu->userdata || !settings)
      return;

   zui      = (zui_t*)menu->userdata;
   font_path = settings->video.font_enable ? settings->video.font_path : NULL;

   if (!menu_display_init_main_font(menu, font_path, zui->font_size))
      RARCH_WARN("Failed to load font.");

   zarch_context_bg_destroy(zui);

   rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE,
         settings->menu.wallpaper, "cb_menu_wallpaper", 0, 1, true);

   zarch_allocate_white_texture(zui);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_FONT_SIZE, &zui->font_size);
   zarch_zui_font(menu);
}

static int zarch_iterate(enum menu_action action)
{
   int ret = 0;
   size_t entries_selection = 0;
   menu_entry_t entry;
   zui_t *zui           = NULL;
   static ssize_t header_selection_last = 0;
   menu_handle_t *menu  = menu_driver_get_ptr();
   enum menu_action act = (enum menu_action)action;
   bool perform_action  = true;

   if (!menu)
      return 0;

   zui      = (zui_t*)menu->userdata;

   if (!zui)
      return -1;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &zui->entries_selection))
      return 0;

   BIT64_SET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER);
   BIT64_SET(menu->state, MENU_STATE_BLIT);

   /* FIXME: Crappy hack, needed for mouse controls to not be completely broken
    * in case we press back.
    *
    * We need to fix this entire mess, mouse controls should not rely on a 
    * hack like this in order to work. */
   zui->entries_selection = max(min(zui->entries_selection, (menu_entries_get_size() - 1)), 0);

   menu_entry_get(&entry,    zui->entries_selection, NULL, false);

   if (zui->pending_action_ok.enable)
   {
      menu_entry_get(&entry, zui->pending_action_ok.idx, NULL, false);
      zui->pending_action_ok.enable = false;
      
      act               = MENU_ACTION_OK;
      entries_selection = zui->pending_action_ok.idx;
      zui->pending_action_ok.idx = 0;
   }
   else
   {
      switch (act)
      {
         case MENU_ACTION_UP:
            if (zui->entries_selection == 0)
            {
               zui->header_selection = header_selection_last;
               perform_action = false;
            }
            break;
         case MENU_ACTION_DOWN:
            if (zui->header_selection != -1)
            {
               header_selection_last  = zui->header_selection;
               zui->header_selection  = -1;
               zui->entries_selection = 0;
               perform_action = false;
            }
            break;
         case MENU_ACTION_LEFT:
            if (zui->header_selection > 0)
            {
               zui->header_selection--;
               zui->entries_selection = 0;
               perform_action = false;
            }
            break;
         case MENU_ACTION_RIGHT:
            if (zui->header_selection > -1 && 
                  zui->header_selection < (ZUI_MAX_MAINMENU_TABS-1))
            {
               zui->header_selection++;
               zui->entries_selection = 0;
               perform_action = false;
            }
            break;
         default:
            break;
      }
      entries_selection = zui->entries_selection;
   }

   if (zui->header_selection != -1)
      perform_action = false;

   if (perform_action)
      ret = menu_entry_action(&entry, entries_selection, act);

   if (zui->time_to_exit)
   {
      zui->time_to_exit = false;
      return -1;
   }
   if (zui->time_to_quit)
   {
      zui->time_to_quit = false;
      if (!event_command(EVENT_CMD_QUIT))
         return -1;
      return 0;
   }

   return ret;
}

static bool zarch_menu_init_list(void *data)
{
   int ret;
   menu_displaylist_info_t info = {0};
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr();
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr();

   strlcpy(info.label, menu_hash_to_str(MENU_VALUE_HISTORY_TAB), sizeof(info.label));

   menu_entries_push(menu_stack, info.path, info.label, info.type, info.flags, 0);

   event_command(EVENT_CMD_HISTORY_INIT);

   info.list  = selection_buf;
   menu_displaylist_push_list(&info, DISPLAYLIST_HISTORY);

   info.need_push = true;

   (void)ret;

   menu_displaylist_push_list_process(&info);

   return true;
}

menu_ctx_driver_t menu_ctx_zarch = {
   NULL,
   zarch_get_message,
   zarch_iterate,
   zarch_render,
   zarch_frame,
   zarch_init,
   zarch_free,
   zarch_context_reset,
   zarch_context_destroy,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   zarch_menu_init_list,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   zarch_load_image,
   "zarch",
   MENU_VIDEO_DRIVER_OPENGL,
   NULL,
};
