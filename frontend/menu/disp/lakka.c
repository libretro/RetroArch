/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
 *  Copyright (C) 2014      - Jean-André Santoni
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

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../backend/menu_common_backend.h"
#include "../menu_common.h"
#include "../file_list.h"
#include "../../../general.h"
#include "../../../gfx/gfx_common.h"
#include "../../../gfx/gl_common.h"
#include "../../../gfx/shader_common.h"
#include "../../../config.def.h"
#include "../../../file.h"
#include "../../../dynamic.h"
#include "../../../compat/posix_string.h"
#include "../../../gfx/shader_parse.h"
#include "../../../performance.h"
#include "../../../input/input_common.h"

#include "../../../screenshot.h"
#include "../../../gfx/fonts/bitmap.h"

#include "lakka.h"
#include <png.h>

// Category variables
menu_category_t *categories;
int depth = 0;
int num_categories = 0;
int menu_active_category = 0;
int dim = 192;
float all_categories_x = 0;
float global_alpha = 0;

// Font variables
void *font;
const gl_font_renderer_t *font_driver;

enum
{
   TEXTURE_MAIN = 0,
   TEXTURE_FONT,
   TEXTURE_SETTINGS,
   TEXTURE_SETTING,
   TEXTURE_SUBSETTING,
   TEXTURE_ARROW,
   TEXTURE_RUN,
   TEXTURE_RESUME,
   TEXTURE_SAVESTATE,
   TEXTURE_LOADSTATE,
   TEXTURE_SCREENSHOT,
   TEXTURE_RELOAD,
   TEXTURE_LAST
};

struct lakka_texture_item
{
   GLuint id;
   char path[PATH_MAX];
};

struct lakka_texture_item textures[TEXTURE_LAST];

static tween_t* tweens = NULL;
int numtweens = 0;

static char *str_replace (const char *string, const char *substr, const char *replacement)
{
   char *tok, *newstr, *oldstr, *head;

   /* if either substr or replacement is NULL, duplicate string a let caller handle it */
   if (!substr || !replacement)
      return strdup (string);

   newstr = strdup (string);
   head = newstr;
   while ( (tok = strstr ( head, substr )))
   {
      oldstr = newstr;
      newstr = (char*)malloc(strlen(oldstr) - strlen(substr) + strlen(replacement) + 1);

      if (!newstr)
      {
         /*failed to alloc mem, free old string and return NULL */
         free (oldstr);
         return NULL;
      }
      memcpy ( newstr, oldstr, tok - oldstr );
      memcpy ( newstr + (tok - oldstr), replacement, strlen ( replacement ) );
      memcpy ( newstr + (tok - oldstr) + strlen( replacement ), tok + strlen ( substr ), strlen ( oldstr ) - strlen ( substr ) - ( tok - oldstr ) );
      memset ( newstr + strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) , 0, 1 );
      /* move back head right after the last replacement */
      head = newstr + (tok - oldstr) + strlen( replacement );
      free (oldstr);
   }
   return newstr;
}

float inOutQuad(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 2) + b;
   return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
}

void add_tween(float duration, float target_value, float* subject, easingFunc easing, tweenCallback callback)
{
   tween_t *tween;

   numtweens++;

   tweens = (tween_t*)realloc(tweens, numtweens * sizeof(tween_t));
   tween = (tween_t*)&tweens[numtweens-1];

   if (!tween)
      return;

   tween->alive = 1;
   tween->duration = duration;
   tween->running_since = 0;
   tween->initial_value = *subject;
   tween->target_value = target_value;
   tween->subject = subject;
   tween->easing = easing;
   tween->callback = callback;
}

static void update_tween(void *data, float dt)
{
   tween_t *tween = (tween_t*)data;

   if (!tween)
      return;

#if 0
   RARCH_LOG("delta: %f\n", dt);
   RARCH_LOG("tween running since: %f\n", tween->running_since);
   RARCH_LOG("tween duration: %f\n", tween->duration);
#endif

   if (tween->running_since < tween->duration)
   {
      tween->running_since += dt;

      if (tween->easing)
         *tween->subject = tween->easing(
               tween->running_since,
               tween->initial_value,
               tween->target_value - tween->initial_value,
               tween->duration);

      if (tween->running_since >= tween->duration)
      {
         *tween->subject = tween->target_value;

         if (tween->callback)
            tween->callback();
      }
   }
}

static void update_tweens(float dt)
{
   int i, active_tweens;

   active_tweens = 0;

   for(i = 0; i < numtweens; i++)
   {
      update_tween(&tweens[i], dt);
      active_tweens += tweens[i].running_since < tweens[i].duration ? 1 : 0;
   }

   if (numtweens && !active_tweens)
      numtweens = 0;
}

static void lakka_draw_text(const char *str, float x, float y, float scale, float alpha)
{
   gl_t *gl = (gl_t*)driver.video_data;
   if (!gl)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   struct font_params params = {0};
   params.x = x / gl->vp.width;
   params.y = 1.0f - y / gl->vp.height;

   if (alpha > global_alpha)
      alpha = global_alpha;

   params.scale = scale;
   params.color = FONT_COLOR_RGBA(255, 255, 255, (uint8_t)(255 * alpha));

   if (font_driver)
      font_driver->render_msg(font, str, &params);
}

void lakka_draw_background(void)
{
   GLfloat background_color[] = {
      0.1, 0.74, 0.61, global_alpha,
      0.1, 0.74, 0.61, global_alpha,
      0.1, 0.74, 0.61, global_alpha,
      0.1, 0.74, 0.61, global_alpha,
   };

   gl_t *gl = (gl_t*)driver.video_data;
   if (!gl)
      return;

   glEnable(GL_BLEND);

   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = background_color;
   glBindTexture(GL_TEXTURE_2D, 0);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   gl->coords.vertices = 4;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
   gl->coords.color = gl->white_color_ptr;
}

void lakka_draw_icon(GLuint texture, float x, float y, float alpha, float rotation, float scale)
{
   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   static const GLfloat vtest[] = {
      0, 0,
      1, 0,
      0, 1,
      1, 1
   };

   gl_t *gl = (gl_t*)driver.video_data;

   if (!gl)
      return;
   
   if (alpha > global_alpha)
      alpha = global_alpha;

   glViewport(x, gl->win_height - y, dim, dim);

   glEnable(GL_BLEND);

   gl->coords.vertex = vtest;
   gl->coords.tex_coord = vtest;
   gl->coords.color = color;
   glBindTexture(GL_TEXTURE_2D, texture);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   math_matrix mymat;

   math_matrix mrot;
   matrix_rotate_z(&mrot, rotation);
   matrix_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   math_matrix mscal;
   matrix_scale(&mscal, scale, scale, 1);
   matrix_multiply(&mymat, &mscal, &mymat);

   gl->coords.vertices = 4;
   gl_shader_set_coords(gl, &gl->coords, &mymat);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);

   gl->coords.vertex = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = gl->white_color_ptr;
}

static void lakka_draw_subitems(int i, int j)
{
   int k;
   menu_category_t *category = (menu_category_t*)&categories[i];
   menu_item_t *item = (menu_item_t*)&category->items[j];
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];
   menu_item_t *active_item = (menu_item_t*)&active_category->items[active_category->active_item];

   for(k = 0; k < item->num_subitems; k++)
   {
      menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

      if (!subitem)
         continue;

      if (k == 0 && g_extern.main_is_init
            && !g_extern.libretro_dummy
            && strcmp(g_extern.fullpath, &active_item->rom) == 0)
      {
         lakka_draw_icon(textures[TEXTURE_RESUME].id, 
            156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
            300 + subitem->y + dim/2.0, 
            subitem->alpha, 
            0, 
            subitem->zoom);
         lakka_draw_text("Resume", 
            156 + HSPACING*(i+2) + all_categories_x + dim/2.0, 
            300 + subitem->y + 15, 
            1, 
            subitem->alpha);
      }
      else if(k == 0 ||
            menu_active_category == 0 ||
            (g_extern.main_is_init && 
            !g_extern.libretro_dummy &&
            strcmp(g_extern.fullpath, &active_item->rom) == 0))
      {
         lakka_draw_icon(subitem->icon, 
               156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
               300 + subitem->y + dim/2.0, 
               subitem->alpha, 
               0, 
               subitem->zoom);
         lakka_draw_text(subitem->name, 
               156 + HSPACING * (i+2) + all_categories_x + dim/2.0, 
               300 + subitem->y + 15, 
               1, 
               subitem->alpha);
      }

   }
}

static void lakka_draw_items(int i)
{
   int j;
   menu_category_t *category = (menu_category_t*)&categories[i];
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];
   menu_item_t *active_item = (menu_item_t*)&active_category->items[active_category->active_item];

   for(j = 0; j < category->num_items; j++)
   {
      menu_item_t *item = (menu_item_t*)&category->items[j];

      if (!item)
         continue;

      if (i == menu_active_category &&
         j > active_category->active_item - 4 &&
         j < active_category->active_item + 10) // performance improvement
      {
         lakka_draw_icon(category->item_icon,
            156 + HSPACING*(i+1) + all_categories_x - dim/2.0, 
            300 + item->y + dim/2.0, 
            item->alpha, 
            0, 
            item->zoom);

         if (depth == 0)
            lakka_draw_text(item->name,
               156 + HSPACING * (i+1) + all_categories_x + dim/2.0, 
               300 + item->y + 15, 
               1, 
               item->alpha);
      }

      if (i == menu_active_category && j == category->active_item && depth == 1) // performance improvement
         lakka_draw_subitems(i, j);
   }
}

static void lakka_draw_categories(void)
{
   int i;

   for(i = 0; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      // draw items
      lakka_draw_items(i);

      // draw category icon
      lakka_draw_icon(category->icon, 
            156 + (HSPACING*(i+1)) + all_categories_x - dim/2.0, 
            300 + dim/2.0, 
            category->alpha, 
            0, 
            category->zoom);
   }
}

static void lakka_frame(void)
{
   struct font_output_list *msg;
   gl_t *gl = (gl_t*)driver.video_data;
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];
   menu_item_t *active_item;

   if (!driver.menu || !gl || !active_category)
      return;

   active_item = (menu_item_t*)&active_category->items[active_category->active_item];

   update_tweens(0.002);

   glViewport(0, 0, gl->win_width, gl->win_height);

   lakka_draw_background();

   lakka_draw_categories();

   if (depth == 0)
   {
      if (active_category)
         lakka_draw_text(active_category->name, 15.0, 40.0, 1, 1.0);
   }
   else
   {
      if (active_item)
         lakka_draw_text(active_item->name, 15.0, 40.0, 1, 1.0);

      lakka_draw_icon(textures[TEXTURE_ARROW].id,
            156 + HSPACING*(menu_active_category+1) + all_categories_x + 150 +-dim/2.0,
            300 + VSPACING*2.4 + (dim/2.0), 1, 0, I_ACTIVE_ZOOM);
   }

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
}

// thanks to https://github.com/DavidEGrayson/ahrs-visualizer/blob/master/png_texture.cpp
static GLuint png_texture_load(const char * file_name, int * width, int * height)
{
    png_byte header[8];

    FILE *fp = fopen(file_name, "rb");
    if (fp == 0)
    {
        perror(file_name);
        return 0;
    }

    // read the header
    fread(header, 1, 8, fp);

    if (png_sig_cmp(header, 0, 8))
    {
        fprintf(stderr, "error: %s is not a PNG.\n", file_name);
        fclose(fp);
        return 0;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fprintf(stderr, "error: png_create_read_struct returned 0.\n");
        fclose(fp);
        return 0;
    }

    // create png info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        fclose(fp);
        return 0;
    }

    // create png info struct
    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info)
    {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
        fclose(fp);
        return 0;
    }

    // the code in this if statement gets called if libpng encounters an error
    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "error from libpng\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return 0;
    }

    // init png reading
    png_init_io(png_ptr, fp);

    // let libpng know you already read the first 8 bytes
    png_set_sig_bytes(png_ptr, 8);

    // read all the info up to the image data
    png_read_info(png_ptr, info_ptr);

    // variables to pass to get info
    int bit_depth, color_type;
    png_uint_32 temp_width, temp_height;

    // get info about png
    png_get_IHDR(png_ptr, info_ptr, &temp_width, &temp_height, &bit_depth, &color_type,
        NULL, NULL, NULL);

    if (width){ *width = temp_width; }
    if (height){ *height = temp_height; }

    //printf("%s: %lux%lu %d\n", file_name, temp_width, temp_height, color_type);

    if (bit_depth != 8)
    {
        fprintf(stderr, "%s: Unsupported bit depth %d.  Must be 8.\n", file_name, bit_depth);
        return 0;
    }

    GLint format;
    switch(color_type)
    {
    case PNG_COLOR_TYPE_RGB:
        format = GL_RGB;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        format = GL_RGBA;
        break;
    default:
        fprintf(stderr, "%s: Unknown libpng color type %d.\n", file_name, color_type);
        return 0;
    }

    // Update the png info struct.
    png_read_update_info(png_ptr, info_ptr);

    // Row size in bytes.
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    // glTexImage2d requires rows to be 4-byte aligned
    rowbytes += 3 - ((rowbytes-1) % 4);

    // Allocate the image_data as a big block, to be given to opengl
    png_byte * image_data = (png_byte *)malloc(rowbytes * temp_height * sizeof(png_byte)+15);
    if (image_data == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for PNG image data\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return 0;
    }

    // row_pointers is for pointing to image_data for reading the png with libpng
    png_byte ** row_pointers = (png_byte **)malloc(temp_height * sizeof(png_byte *));
    if (row_pointers == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for PNG row pointers\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        free(image_data);
        fclose(fp);
        return 0;
    }

    // set the individual row_pointers to point at the correct offsets of image_data
    for (unsigned int i = 0; i < temp_height; i++)
    {
        row_pointers[temp_height - 1 - i] = image_data + i * rowbytes;
    }

    // read the png into image_data through row_pointers
    png_read_image(png_ptr, row_pointers);

    // Generate the OpenGL texture object
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, temp_width, temp_height, 0, format, GL_UNSIGNED_BYTE, image_data);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // clean up
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    free(image_data);
    free(row_pointers);
    fclose(fp);
    return texture;
}

static void lakka_context_destroy(void *data)
{
   int i, j, k;
   gl_t *gl = (gl_t*)driver.video_data;

   for (i = 0; i < TEXTURE_LAST; i++)
      glDeleteTextures(1, &textures[i].id);

   for (i = 1; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      glDeleteTextures(1, &category->icon);
      glDeleteTextures(1, &category->item_icon);

      for (j = 0; j < category->num_items; j++)
      {
         menu_item_t *item;
         menu_subitem_t *subitem;

         item = (menu_item_t*)&category->items[j];

         if (item)
         {
            subitem = (menu_subitem_t*)&item->subitems[j];

            if (subitem)
               glDeleteTextures(1, &subitem->icon);
         }
      }
   }

   if (font_driver)
   {
      font_driver->free(font);
      font_driver = NULL;
   }

   //if (numtweens)
   //   free(tweens);
}

void lakka_init_settings(void)
{
   menu_category_t *category = (menu_category_t*)&categories[0];

   strlcpy(category->name, "Settings", sizeof(category->name));
   category->alpha = 1.0;
   category->zoom = C_ACTIVE_ZOOM;
   category->active_item = 0;
   category->num_items   = 0;
   category->items       = (menu_item_t*)calloc(category->num_items, sizeof(menu_item_t));

   int j, k;

   // General options item

   j = 0;
   category->num_items++;
   category->items = (menu_item_t*)realloc(category->items, category->num_items * sizeof(menu_item_t));

   menu_item_t *item0  = (menu_item_t*)&category->items[j];

   strlcpy(item0->name, "General Options", sizeof(item0->name));
   item0->alpha          = j ? 0.5 : 1.0;
   item0->zoom           = j ? I_PASSIVE_ZOOM : I_ACTIVE_ZOOM;
   item0->y              = j ? VSPACING*(3+j) : VSPACING*2.4;
   item0->active_subitem = 0;
   item0->num_subitems   = 0;

   // General options subitems

   k = 0;
   item0->num_subitems++;
   //item0->subitems = (menu_subitem_t*)realloc(item0->subitems, item0->num_subitems * sizeof(menu_subitem_t));
   item0->subitems = (menu_subitem_t*)calloc(item0->num_subitems, sizeof(menu_subitem_t));

   menu_subitem_t *subitem0 = (menu_subitem_t*)&item0->subitems[k];

   strlcpy(subitem0->name, "Libretro Logging Level", sizeof(subitem0->name));
   subitem0->alpha = k ? 1.0 : 0.5;
   subitem0->zoom = k ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
   subitem0->y = k ? VSPACING * (3+k) : VSPACING * 2.4;

   k = 1;
   item0->num_subitems++;
   item0->subitems = (menu_subitem_t*)realloc(item0->subitems, item0->num_subitems * sizeof(menu_subitem_t));
   //item0->subitems = (menu_subitem_t*)calloc(item0->num_subitems, sizeof(menu_subitem_t));

   menu_subitem_t *subitem1 = (menu_subitem_t*)&item0->subitems[k];

   strlcpy(subitem1->name, "Logging Verbosity", sizeof(subitem1->name));
   subitem1->alpha = k ? 1.0 : 0.5;
   subitem1->zoom = k ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
   subitem1->y = k ? VSPACING * (3+k) : VSPACING * 2.4;

   k = 2;
   item0->num_subitems++;
   item0->subitems = (menu_subitem_t*)realloc(item0->subitems, item0->num_subitems * sizeof(menu_subitem_t));
   //item0->subitems = (menu_subitem_t*)calloc(item0->num_subitems, sizeof(menu_subitem_t));

   menu_subitem_t *subitem2 = (menu_subitem_t*)&item0->subitems[k];

   strlcpy(subitem2->name, "Configuration Save On Exit", sizeof(subitem2->name));
   subitem2->alpha = k ? 1.0 : 0.5;
   subitem2->zoom = k ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
   subitem2->y = k ? VSPACING * (3+k) : VSPACING * 2.4;

   // Quit item

   j = 1;
   category->num_items++;
   category->items = (menu_item_t*)realloc(category->items, category->num_items * sizeof(menu_item_t));

   menu_item_t *item1 = (menu_item_t*)&category->items[j];

   strlcpy(item1->name, "Quit RetroArch", sizeof(item1->name));
   item1->alpha          = j ? 0.5 : 1.0;
   item1->zoom           = j ? I_PASSIVE_ZOOM : I_ACTIVE_ZOOM;
   item1->y              = j ? VSPACING*(3+j) : VSPACING*2.4;
   item1->active_subitem = 0;
   item1->num_subitems   = 0;
}

void lakka_settings_context_reset(void)
{
   menu_item_t *item;
   int k;
   menu_category_t *category = (menu_category_t*)&categories[0];

   if (!category)
      return;

   category->icon = textures[TEXTURE_SETTINGS].id;
   category->item_icon = textures[TEXTURE_SETTING].id;

   // General options item

   item = (menu_item_t*)&category->items[0];

   // General options subitems
   for (k = 0; k < 2; k++)
   {
      menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];
      subitem->icon = textures[TEXTURE_SUBSETTING].id;
   }

   // Quit item

   item = (menu_item_t*)&category->items[1];
}


static void lakka_context_reset(void *data)
{
   int i, j, k;
   char path[256], dirpath[256];;
   menu_handle_t *menu = (menu_handle_t*)data;
   gl_t *gl = (gl_t*)driver.video_data;

   if (!menu)
      return;

   gl_font_init_first(&font_driver, &font, gl,
         *g_settings.video.font_path ? g_settings.video.font_path : NULL, g_settings.video.font_size);

   fill_pathname_join(dirpath, g_settings.assets_directory, "lakka", sizeof(dirpath));
   fill_pathname_slash(dirpath, sizeof(dirpath));

   fill_pathname_join(textures[TEXTURE_SETTINGS].path, dirpath, "settings.png", sizeof(textures[TEXTURE_SETTINGS].path));
   fill_pathname_join(textures[TEXTURE_SETTING].path, dirpath, "setting.png", sizeof(textures[TEXTURE_SETTING].path));
   fill_pathname_join(textures[TEXTURE_SUBSETTING].path, dirpath, "subsetting.png", sizeof(textures[TEXTURE_SUBSETTING].path));
   fill_pathname_join(textures[TEXTURE_ARROW].path, dirpath, "arrow.png", sizeof(textures[TEXTURE_ARROW].path));
   fill_pathname_join(textures[TEXTURE_RUN].path, dirpath, "run.png", sizeof(textures[TEXTURE_RUN].path));
   fill_pathname_join(textures[TEXTURE_RESUME].path, dirpath, "resume.png", sizeof(textures[TEXTURE_RESUME].path));
   fill_pathname_join(textures[TEXTURE_SAVESTATE].path, dirpath, "savestate.png", sizeof(textures[TEXTURE_SAVESTATE].path));
   fill_pathname_join(textures[TEXTURE_LOADSTATE].path, dirpath, "loadstate.png", sizeof(textures[TEXTURE_LOADSTATE].path));
   fill_pathname_join(textures[TEXTURE_SCREENSHOT].path, dirpath, "screenshot.png", sizeof(textures[TEXTURE_SCREENSHOT].path));
   fill_pathname_join(textures[TEXTURE_RELOAD].path, dirpath, "reload.png", sizeof(textures[TEXTURE_RELOAD].path));

   for (k = 0; k < TEXTURE_LAST; k++)
      textures[k].id = png_texture_load(textures[k].path, &dim, &dim);

   lakka_settings_context_reset();
   for (i = 1; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      char core_id[256], texturepath[256], content_texturepath[256], dirpath[256];
      core_info_t *info;
      core_info_list_t *info_list;

      fill_pathname_join(dirpath, g_settings.assets_directory, "lakka", sizeof(dirpath));
      fill_pathname_slash(dirpath, sizeof(dirpath));

      info_list = (core_info_list_t*)menu->core_info;
      info = NULL;

      if (info_list)
         info = (core_info_t*)&info_list->list[i-1];

      strlcpy(core_id, basename(info->path), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, ".so", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, ".dll", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, ".dylib", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, "-libretro", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, "_libretro", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, "libretro-", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, "libretro_", ""), sizeof(core_id));

      strlcpy(texturepath, dirpath, sizeof(texturepath));
      strlcat(texturepath, core_id, sizeof(texturepath));
      strlcat(texturepath, ".png", sizeof(texturepath));

      strlcpy(content_texturepath, dirpath, sizeof(content_texturepath));
      strlcat(content_texturepath, core_id, sizeof(content_texturepath));
      strlcat(content_texturepath, "-content.png", sizeof(content_texturepath));

      category->icon = png_texture_load(texturepath, &dim, &dim);
      category->item_icon = png_texture_load(content_texturepath, &dim, &dim);
      
      for (j = 0; j < category->num_items; j++)
      {
         menu_item_t *item = (menu_item_t*)&category->items[j];

         for (k = 0; k < item->num_subitems; k++)
         {
            menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

            switch (k)
            {
               case 0:
                  subitem->icon = textures[TEXTURE_RUN].id;
                  break;
               case 1:
                  subitem->icon = textures[TEXTURE_SAVESTATE].id;
                  break;
               case 2:
                  subitem->icon = textures[TEXTURE_LOADSTATE].id;
                  break;
               case 3:
                  subitem->icon = textures[TEXTURE_SCREENSHOT].id;
                  break;
               case 4:
                  subitem->icon = textures[TEXTURE_RELOAD].id;
                  break;
            }
         }
      }
   }
}

static void lakka_init_items(int i, menu_category_t *category, core_info_t *info, const char* path)
{
   int num_items, j, n, k;
   struct string_list *list = (struct string_list*)dir_list_new(path, info->supported_extensions, true);

   dir_list_sort(list, true);

   num_items = list ? list->size : 0;

   for (j = 0; j < num_items; j++)
   {
      if (list->elems[j].attr.b) // is a directory
         lakka_init_items(i, category, info, list->elems[j].data);
      else
      {
         menu_item_t *item;

         n = category->num_items;

         category->num_items++;
         category->items = (menu_item_t*)realloc(category->items, category->num_items * sizeof(menu_item_t));
         item = (menu_item_t*)&category->items[n];

         strlcpy(item->name, path_basename(list->elems[j].data), sizeof(item->name));
         strlcpy(item->rom, list->elems[j].data, sizeof(item->rom));
         item->alpha          = i != menu_active_category ? 0 : n ? 0.5 : 1;
         item->zoom           = n ? I_PASSIVE_ZOOM : I_ACTIVE_ZOOM;
         item->y              = n ? VSPACING*(3+n) : VSPACING*2.4;
         item->active_subitem = 0;
         item->num_subitems   = 5;
         item->subitems       = (menu_subitem_t*)calloc(item->num_subitems, sizeof(menu_subitem_t));

         for (k = 0; k < item->num_subitems; k++)
         {
            menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

            if (!subitem)
               continue;

            switch (k)
            {
               case 0:
                  strlcpy(subitem->name, "Run", sizeof(subitem->name));
                  break;
               case 1:
                  strlcpy(subitem->name, "Save State", sizeof(subitem->name));
                  break;
               case 2:
                  strlcpy(subitem->name, "Load State", sizeof(subitem->name));
                  break;
               case 3:
                  strlcpy(subitem->name, "Take Screenshot", sizeof(subitem->name));
                  break;
               case 4:
                  strlcpy(subitem->name, "Reset", sizeof(subitem->name));
                  break;
            }
            subitem->alpha = 0;
            subitem->zoom = k ? I_PASSIVE_ZOOM : I_ACTIVE_ZOOM;
            subitem->y = k ? VSPACING * (3+k) : VSPACING * 2.4;
         }
      }
   }
}

static void lakka_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);
}

static int lakka_input_postprocess(uint64_t old_state)
{
   if ((driver.menu && driver.menu->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      global_alpha = 0;
      g_extern.lifecycle_state |= (1ULL << MODE_GAME);
      return -1;
   }

   if (! global_alpha)
      add_tween(DELAY, 1.0, &global_alpha, &inOutQuad, NULL);

   return 0;
}

static void lakka_init_core_info(void *data)
{
   core_info_list_t *core;
   menu_handle_t *menu = (menu_handle_t*)data;

   core_info_list_free(menu->core_info);
   menu->core_info = NULL;

   menu->core_info = (core_info_list_t*)core_info_list_new(*g_settings.libretro_directory ? g_settings.libretro_directory : "/usr/lib/libretro");

   if (menu->core_info)
   {
      core = (core_info_list_t*)menu->core_info;
      num_categories = menu->core_info ? core->count + 1 : 1;
   }
}

static void *lakka_init(void)
{
   int i, j;
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));
   gl_t *gl = (gl_t*)driver.video_data;
   if (!menu || !gl)
      return NULL;

   lakka_init_core_info(menu);
   categories = (menu_category_t*)calloc(num_categories, sizeof(menu_category_t));

   lakka_init_settings();

   for (i = 1; i < num_categories; i++)
   {
      core_info_t *info;
      core_info_list_t *info_list;
      menu_category_t *category = (menu_category_t*)&categories[i];

      info_list = (core_info_list_t*)menu->core_info;
      info = NULL;

      if (info_list)
         info = (core_info_t*)&info_list->list[i-1];

      strlcpy(category->name, info->display_name, sizeof(category->name));
      strlcpy(category->libretro, info->path, sizeof(category->libretro));
      category->alpha       = 0.5;
      category->zoom        = C_PASSIVE_ZOOM;
      category->active_item = 0;
      category->num_items   = 0;
      category->items       = (menu_item_t*)calloc(category->num_items, sizeof(menu_item_t));

      lakka_init_items(i, category, info, g_settings.content_directory);
   }

   return menu;
}

const menu_ctx_driver_t menu_ctx_lakka = {
   NULL,
   NULL,
   NULL,
   lakka_frame,
   lakka_init,
   lakka_free,
   lakka_context_reset,
   lakka_context_destroy,
   NULL,
   NULL,
   lakka_input_postprocess,
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
   NULL,
   lakka_init_core_info,
   &menu_ctx_backend_lakka,
   "lakka",
};
