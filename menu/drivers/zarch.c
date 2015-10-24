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

const GLfloat ZUI_NORMAL[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};
const GLfloat ZUI_HILITE[] = {
   1, 0, 0, 1,
   1, 0, 0, 1,
   1, 0, 0, 1,
   1, 0, 0, 1,
};
const GLfloat ZUI_PRESS[] = {
   0, 1, 0, 1,
   0, 1, 0, 1,
   0, 1, 0, 1,
   0, 1, 0, 1,
};
const GLfloat ZUI_BARBG[] = {
   0, 0, 1, 1,
   0, 0, 1, 1,
   0, 0, 1, 1,
   0, 0, 1, 1,
};

const uint32_t ZUI_FG_NORMAL = ~0;
const GLfloat ZUI_BG_PANEL[] = {
   0, 0, 0, 0.25,
   0, 0, 0, 0.25,
   0, 0, 0, 0.25,
   0, 0, 0, 0.25,
};
const GLfloat ZUI_BG_SCREEN[] = {
   0.07, 0.19, 0.26, 1,
   0.07, 0.19, 0.26, 1,
   0.15, 0.31, 0.47, 1,
   0.15, 0.31, 0.47, 1,
};
const GLfloat ZUI_BG_HILITE[] = {
   0.22, 0.60, 0.74, 1,
   0.22, 0.60, 0.74, 1,
   0.22, 0.60, 0.74, 1,
   0.22, 0.60, 0.74, 1,
};


typedef struct zarch_handle
{
   bool rendering;
   menu_handle_t *menu;
   math_matrix_4x4 mvp;
   unsigned width;
   unsigned height;
   gfx_font_raster_block_t tmp_block;
   unsigned hash;
   bool time_to_exit;

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

static void zui_font(menu_handle_t *menu)
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

static float zui_strwidth(void *fb_buf, const char *text, float scale)
{
   driver_t *driver = (driver_t*)driver_get_ptr();
   return driver->font_osd_driver->get_message_width(fb_buf, text, strlen(text), scale);
}

static void zui_begin(void)
{
   zui_t *zui           = NULL;
   driver_t     *driver = (driver_t*)driver_get_ptr();
   menu_handle_t *menu  = menu_driver_get_ptr();
   gl_t            *gl  = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl || !menu)
      return;

   zui      = (zui_t*)menu->userdata;

   if (!zui || !driver)
      return;

   zui->rendering = true;
   zui->hash      = 0;
   zui->item.hot  = 0;

   glViewport(0, 0, zui->width, zui->height);

   if (gl && gl->shader && gl->shader->set_mvp)
      gl->shader->set_mvp(gl, &zui->mvp);

   /* why do i need this? */
   zui->mouse.wheel = menu_input_mouse_state(MENU_MOUSE_WHEEL_DOWN) - 
      menu_input_mouse_state(MENU_MOUSE_WHEEL_UP);

   zui->ca.coords.vertices = 0;

   zui->tmp_block.carr.coords.vertices = 0;
   menu_display_font_bind_block(zui->menu, driver->font_osd_driver, &zui->tmp_block);


}

static void zui_finish(zui_t *zui,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      bool blend,
      GLuint texture)
{
   driver_t *driver = (driver_t*)driver_get_ptr();
   gl_t         *gl = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl)
      return;

   if (!menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON))
      zui->item.active = 0;
   else if (zui->item.active == 0)
      zui->item.active = -1;

   menu_display_draw_frame(
         x,
         y,
         width,
         height,
         gl->shader, (struct gfx_coords*)&zui->ca,
         NULL, true, texture, zui->ca.coords.vertices,
         MENU_DISPLAY_PRIM_TRIANGLES);

   menu_display_font_flush_block(zui->menu, driver->font_osd_driver);

   zui->rendering = false;
}

static bool check_button_down(zui_t *zui, unsigned id, int x1, int y1, int x2, int y2)
{
   bool result = false;
   bool inside = menu_input_mouse_check_hitbox(x1, y1, x2, y2);

   if (inside)
      zui->item.hot = id;

   if (zui->item.hot == id && menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON))
   {
      result = true;
      zui->item.active = id;
   }

   return result;
}

static bool check_button_up(zui_t *zui, unsigned id, int x1, int y1, int x2, int y2)
{
   bool result = false;
   bool inside = menu_input_mouse_check_hitbox(x1, y1, x2, y2);

   if (inside)
      zui->item.hot = id;

   if (zui->item.active == id && !menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON))
   {
      if (zui->item.hot == id)
         result = true;

      zui->item.active = 0;
   }

   check_button_down(zui, id, x1, y1, x2, y2);

   return result;
}

static unsigned zui_hash(zui_t *zui, const char *s)
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

#if 0
static uint32_t zui_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
   return FONT_COLOR_RGBA(r, g, b, a);
}
#endif

static void zui_draw_text(zui_t *zui, uint32_t color, int x, int y, const char *text)
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

static void zui_push_quad(zui_t *zui, const GLfloat *colors, int x1, int y1,
      int x2, int y2)
{
   gfx_coords_t coords;
   GLfloat vertex[8];
   GLfloat tex_coord[8];

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

static float randf(float min, float max)
{
   return (rand() * ((max - min) / RAND_MAX)) + min;
}

static float scalef(float val, float oldmin, float oldmax, float newmin, float newmax)
{
   return (((val - oldmin) * (newmax - newmin)) / (oldmax - oldmin)) + newmin;
}

#define NPARTICLES 100

static void zui_snow(zui_t *zui)
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
      part_t *p = &particles[i];

      if (p->alive)
      {
         int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);

         p->y += p->yspeed;
         p->x += scalef(mouse_x, 0, zui->width, -0.3, 0.3) + p->xspeed;

         p->alive = p->y >= 0 && p->y < (int)zui->height && p->x >= 0 && p->x < (int)zui->width;
      }
      else if (max_gen > 0 && timeout <= 0)
      {
         p->xspeed = randf(-0.2, 0.2);
         p->yspeed = randf(1, 2);
         p->y = 0;
         p->x = rand() % (int)zui->width;
         p->alpha = (float)rand() / (float)RAND_MAX;
         p->alive = true;

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
      GLfloat alpha;
      GLfloat colors[16];
      part_t *p = &particles[i];

      if (!p->alive)
         continue;

      alpha = randf(0, 100) > 90 ? p->alpha/2 : p->alpha;

      for (j = 0; j < 16; j++)
      {
         colors[j] = 1;
         if (j == 3 || j == 7 || j == 11 || j == 15)
            colors[j] = alpha;
      }

      zui_push_quad(zui, colors, p->x-2, p->y-2, p->x+2, p->y+2);

      j++;
   }
}

static bool zui_button_full(zui_t *zui, int x1, int y1, int x2, int y2, const char *label)
{
   unsigned id = zui_hash(zui, label);
   bool active = check_button_up(zui, id, x1, y1, x2, y2);
   const GLfloat *bg = ZUI_BG_PANEL;

   if (zui->item.active == id || zui->item.hot == id)
      bg = ZUI_BG_HILITE;

   zui_push_quad(zui, bg, x1, y1, x2, y2);
   zui_draw_text(zui, ZUI_FG_NORMAL, x1+12, y1 + 41, label);

   return active;
}

static bool zui_button(zui_t *zui, int x1, int y1, const char *label)
{
   return zui_button_full(zui, x1, y1, x1 + zui_strwidth(zui->fb_buf, label, 1.0) + 24, y1 + 64, label);
}

static bool zui_list_item(zui_t *zui, int x1, int y1, const char *label)
{
   char title_buf[PATH_MAX_LENGTH];
   unsigned ticker_size;
   unsigned           id = zui_hash(zui, label);
   int                x2 = x1 + zui->width - 290 - 40;
   int                y2 = y1 + 50;
   bool           active = check_button_up(zui, id, x1, y1, x2, y2);
   const GLfloat     *bg = ZUI_BG_PANEL;
   uint64_t *frame_count = video_driver_get_frame_count();

   if (zui->item.active == id || zui->item.hot == id)
      bg = ZUI_BG_HILITE;

   zui_push_quad(zui, bg, x1, y1, x2, y2);

   ticker_size = x2 / 14;

   menu_animation_ticker_str(title_buf,
         ticker_size,
         *frame_count / 50,
         label,
         (bg == ZUI_BG_HILITE));

   zui_draw_text(zui, ZUI_FG_NORMAL, 12, y1 + 35, title_buf);

   return active;
}

static void zui_tabbed_begin(zui_t *zui, zui_tabbed_t *tab, int x, int y)
{
   tab->x = x;
   tab->y = y;
   tab->tabline_size = 60 + 4;
}

static bool zui_tab(zui_t *zui, zui_tabbed_t *tab, const char *label)
{
   bool active;
   int x1, y1, x2, y2;
   unsigned       id = zui_hash(zui, label);
   int         width = tab->tab_width;
   const GLfloat *bg = ZUI_BG_PANEL;

   if (!width)
      width = zui_strwidth(zui->fb_buf, label, 1.0) + 24;

   x1          = tab->x;
   y1          = tab->y;
   x2          = x1 + width;
   y2          = y1 + 60;

   active      = check_button_up(zui, id, x1, y1, x2, y2);

   if (zui->item.active == id || tab->active == ~0)
      tab->active = id;

   if (tab->active == id || zui->item.active == id || zui->item.hot == id)
      bg = ZUI_BG_HILITE;

   zui_push_quad(zui, bg, x1+0, y1+0, x2, y2);
   zui_draw_text(zui, ZUI_FG_NORMAL, x1+12, y1 + 41, label);

   if (tab->vertical)
      tab->y += y2 - y1;
   else
      tab->x = x2;

   return (active || (tab->active == id));
}


static void render_lay_settings(zui_t *zui)
{
   int width, x1, y1;
   static zui_tabbed_t tabbed = {~0};
   tabbed.vertical = true;
   tabbed.tab_width = 100;

   zui_tabbed_begin(zui, &tabbed, zui->width - 100, 20);

   width = 290;
   x1    = zui->width - width - 20;
   y1    = 20;

   y1 += 64;

   if (zui_button_full(zui, x1, y1, x1 + width, y1 + 64, "Back"))
      layout = LAY_HOME;
}

static int render_lay_root(zui_t *zui)
{
   static zui_tabbed_t tabbed = {~0};
   global_t     *global = global_get_ptr();
   settings_t *settings = config_get_ptr();

   zui_tabbed_begin(zui, &tabbed, 0, 0);

   tabbed.width = zui->width - 290 - 40;

   if (zui_tab(zui, &tabbed, "Recent"))
   {
      size_t    end = menu_entries_get_end();
      unsigned size = min(zui->height/30-2, end);
      unsigned    i = menu_entries_get_start();

      for (; i < size; ++i)
      {
         menu_entry_t entry;
         menu_entries_get(i, &entry);

         if (zui_list_item(zui, 0, tabbed.tabline_size + i * 54, entry.path))
         {
            menu_entry_action(&entry, i, MENU_ACTION_OK);
            return 1;
         }
      }
   }

   if (zui_tab(zui, &tabbed, "Load"))
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

      zui_draw_text(zui, ZUI_FG_NORMAL, 15, tabbed.tabline_size + 5 + 41, &zui->load_cwd[strlen(zui->load_cwd) - cwd_offset]);

      if (zui_button(zui, zui->width - 290 - 129, tabbed.tabline_size + 5, "Home"))
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
         if (zui_list_item(zui, 0, tabbed.tabline_size + 73, "^ .."))
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

               if (zui_list_item(zui, 0, tabbed.tabline_size + 73 + j * 54, label))
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

   if (zui_tab(zui, &tabbed, "Collections"))
   {
      /* STUB/FIXME */
   }

   if (zui_tab(zui, &tabbed, "Download"))
   {
      /* STUB/FIXME */
   }

   zui_push_quad(zui, ZUI_BG_HILITE, 0, 60, zui->width - 290 - 40, 60+4);

   return 0;
}

void render_sidebar(zui_t *zui)
{
   int width, x1, y1;
   static zui_tabbed_t tabbed = {~0};
   tabbed.vertical = true;
   tabbed.tab_width = 100;

   zui_tabbed_begin(zui, &tabbed, zui->width - 100, 20);

   width = 290;
   x1    = zui->width - width - 20;
   y1    = 20;

   if (zui_button_full(zui, x1, y1, x1 + width, y1 + 64, "Settings"))
      layout = LAY_SETTINGS;

   y1 += 64;

   if (zui_button_full(zui, x1, y1, x1 + width, y1 + 64, "Exit"))
      exit(0);
}

static int zui_load_content(zui_t *zui, unsigned i)
{
   int ret = menu_common_load_content(zui->pick_cores[i].path,
         zui->pick_content, false, CORE_TYPE_PLAIN);

   layout = LAY_HOME;

   return ret;
}

static void zui_render(void)
{
   menu_handle_t *menu  = menu_driver_get_ptr();
   zui_t *zui           = NULL;

   if (!menu)
      return;

   zui      = (zui_t*)menu->userdata;

   if (!zui || zui->rendering)
      return;

   zui_begin();

   zui_push_quad(zui, ZUI_BG_SCREEN, 0, 0, zui->width, zui->height);
   zui_snow(zui);


   switch (layout)
   {
      case LAY_HOME:
         render_sidebar(zui);
         if (render_lay_root(zui) == 1)
            return;
         break;
      case LAY_SETTINGS:
         render_lay_settings(zui);
         break;
      case LAY_PICK_CORE:
         render_sidebar(zui);
         if (zui->pick_supported == 1)
         {
            int ret =zui_load_content(zui, 0);

            (void)ret;

            layout = LAY_HOME;
            zui->time_to_exit = true;
            return;
         }
         else
         {
            zui_draw_text(zui, ~0, 8, 18, "Select a core: ");

            if (zui_button(zui, 0, 18 + zui->font_size, "<- Back"))
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

                  if (zui_list_item(zui, 0, 54 + j * 54, zui->pick_cores[i].display_name))
                  {
                     int ret =zui_load_content(zui, i);

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
               zui_list_item(zui, 0, 54, "Content unsupported");
            }
         }
         break;
   }

   zui_finish(zui, 0, 0, zui->width, zui->height, true, zui->textures.white);
}

static void zarch_draw_cursor(gl_t *gl, float x, float y)
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
   zui_render();

   /* fetch it again in case the pointer was invalidated by a core load */
   gl                   = (gl_t*)video_driver_get_ptr(NULL);

   if (settings->menu.mouse.enable)
      zarch_draw_cursor(gl, menu_input_mouse_state(MENU_MOUSE_X_AXIS), menu_input_mouse_state(MENU_MOUSE_Y_AXIS));

   menu_display_frame_background(menu, settings,
         gl, zui->width, zui->height,
         zui->textures.bg.id, 0.75f, false,
         &coord_color[0],   &coord_color2[0],
         &zarch_vertexes[0], &zarch_tex_coords[0], 4,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);

   gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}

static void *zarch_init(void)
{
   int tmpi;
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

   tmpi = 1000;
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEADER_HEIGHT, &tmpi);

   tmpi = 28;
   menu_display_ctl(MENU_DISPLAY_CTL_SET_FONT_SIZE, &tmpi);

   zui->header_height  = 1000; /* dpi / 3; */
   zui->font_size       = 28;

   if (settings->menu.wallpaper[0] != '\0')
      rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE,
            settings->menu.wallpaper, "cb_menu_wallpaper", 0, 1, true);

   zui->ca.allocated = 0;

   matrix_4x4_ortho(&zui->mvp, 0, 1, 1, 0, 0, 1);

   zui_font(menu);

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
   if (zarch->textures.bg.id)
      glDeleteTextures(1, (const GLuint*)&zarch->textures.bg.id);
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

static void zarch_allocate_white_texture(zui_t *zarch)
{
   struct texture_image ti;
   static const uint8_t white_data[] = { 0xff, 0xff, 0xff, 0xff };

   ti.width  = 1;
   ti.height = 1;
   ti.pixels = (uint32_t*)&white_data;

   zarch->textures.white   = video_texture_load(&ti,
         TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_NEAREST);
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
         zarch_allocate_white_texture(zarch);
         break;
      case MENU_IMAGE_BOXART:
         break;
   }

   return true;
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
   zarch_allocate_white_texture(zui);

   rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE,
         settings->menu.wallpaper, "cb_menu_wallpaper", 0, 1, true);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_FONT_SIZE, &zui->font_size);
   zui_font(menu);
}

static int zarch_iterate(enum menu_action action)
{
   zui_t *zui           = NULL;
   menu_handle_t *menu  = menu_driver_get_ptr();

   if (!menu)
      return 0;

   zui      = (zui_t*)menu->userdata;

   if (!zui)
      return -1;

   BIT64_SET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER);
   BIT64_SET(menu->state, MENU_STATE_BLIT);

   switch (action)
   {
      case MENU_ACTION_UP:
         zui->load_dlist_first--;
         break;
      case MENU_ACTION_DOWN:
         zui->load_dlist_first++;
         break;
      case MENU_ACTION_LEFT:
         zui->load_dlist_first -= 5;
         break;
      case MENU_ACTION_RIGHT:
         zui->load_dlist_first += 5;
         break;
      default:
         break;
   }

   if (zui->time_to_exit)
   {
      RARCH_LOG("Gets here.\n");
      zui->time_to_exit = false;
      return -1;
   }


   return 0;
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
