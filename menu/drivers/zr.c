/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
 *  Copyright (C) 2016      - Andrés Suárez
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
#include <math.h>

#include <retro_assert.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <formats/image.h>
#include <gfx/math/matrix_4x4.h>
#include <string/stdstring.h>
#include <string/string_list.h>

#include "menu_generic.h"
#include "zr_common.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_navigation.h"
#include "../menu_hash.h"
#include "../menu_display.h"

#include "../../core_info.h"
#include "../../configuration.h"
#include "../../frontend/frontend_driver.h"
#include "../../system.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"
#include "../../deps/stb/stb_image.h"

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../../gfx/common/gl_common.h"
#endif

#define MAX_VERTEX_MEMORY     (512 * 1024)
#define MAX_ELEMENT_MEMORY    (128 * 1024)

#define ZR_SYSTEM_TAB_END     ZR_SYSTEM_TAB_SETTINGS

struct zr_device
{
   struct zr_buffer cmds;
   struct zr_draw_null_texture null;
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLuint vbo, vao, ebo;

   GLuint prog;
   GLuint vert_shdr;
   GLuint frag_shdr;

   GLint attrib_pos;
   GLint attrib_uv;
   GLint attrib_col;

   GLint uniform_proj;
   GLuint font_tex;
#endif
};

static struct zr_device device;
static struct zr_font font;

static struct zr_user_font usrfnt;
static struct zr_allocator zr_alloc;

static void zrmenu_main(zrmenu_handle_t *zr)
{
   struct zr_context *ctx = &zr->ctx;

   if (zr->window[ZRMENU_WND_MAIN].open)
      zrmenu_wnd_main(ctx, zr);
   if (zr->window[ZRMENU_WND_CONTROL].open)
      zrmenu_wnd_control(ctx, zr);
   if (zr->window[ZRMENU_WND_SHADER_PARAMETERS].open)
      zrmenu_wnd_shader_parameters(ctx, zr);
   if (zr->window[ZRMENU_WND_TEST].open)
      zrmenu_wnd_test(ctx, zr);
   if (zr->window[ZRMENU_WND_WIZARD].open)
      zrmenu_wnd_wizard(ctx, zr);

   zr->window[ZRMENU_WND_CONTROL].open = !zr_window_is_closed(ctx, "Control");
   zr->window[ZRMENU_WND_SHADER_PARAMETERS].open = !zr_window_is_closed(ctx, "Shader Parameters");
   zr->window[ZRMENU_WND_TEST].open = !zr_window_is_closed(ctx, "Test");
   zr->window[ZRMENU_WND_WIZARD].open = !zr_window_is_closed(ctx, "Setup Wizard");

   if(zr_window_is_closed(ctx, "Setup Wizard"))
      zr->window[ZRMENU_WND_MAIN].open = true;

   zr_buffer_info(&zr->status, &zr->ctx.memory);
}

static char* zrmenu_file_load(const char* path, size_t* size)
{
   char *buf;
   FILE *fd = fopen(path, "rb");

   fseek(fd, 0, SEEK_END);
   *size = (size_t)ftell(fd);
   fseek(fd, 0, SEEK_SET);
   buf = (char*)calloc(*size, 1);
   fread(buf, *size, 1, fd);
   fclose(fd);
   return buf;
}

static struct zr_image zr_icon_load(const char *filename)
{
    int x,y,n;
    GLuint tex;
    unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
    if (!data) printf("Failed to load image: %s\n", filename);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return zr_image_id((int)tex);
}

static void zr_device_init(struct zr_device *dev)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLint status;
   static const GLchar *vertex_shader =
      "#version 300 es\n"
      "uniform mat4 ProjMtx;\n"
      "in vec2 Position;\n"
      "in vec2 TexCoord;\n"
      "in vec4 Color;\n"
      "out vec2 Frag_UV;\n"
      "out vec4 Frag_Color;\n"
      "void main() {\n"
      "   Frag_UV = TexCoord;\n"
      "   Frag_Color = Color;\n"
      "   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
      "}\n";
   static const GLchar *fragment_shader =
      "#version 300 es\n"
      "precision mediump float;\n"
      "uniform sampler2D Texture;\n"
      "in vec2 Frag_UV;\n"
      "in vec4 Frag_Color;\n"
      "out vec4 Out_Color;\n"
      "void main(){\n"
      "   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
      "}\n";

   dev->prog = glCreateProgram();
   dev->vert_shdr = glCreateShader(GL_VERTEX_SHADER);
   dev->frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(dev->vert_shdr, 1, &vertex_shader, 0);
   glShaderSource(dev->frag_shdr, 1, &fragment_shader, 0);
   glCompileShader(dev->vert_shdr);
   glCompileShader(dev->frag_shdr);
   glGetShaderiv(dev->vert_shdr, GL_COMPILE_STATUS, &status);
   assert(status == GL_TRUE);
   glGetShaderiv(dev->frag_shdr, GL_COMPILE_STATUS, &status);
   assert(status == GL_TRUE);
   glAttachShader(dev->prog, dev->vert_shdr);
   glAttachShader(dev->prog, dev->frag_shdr);
   glLinkProgram(dev->prog);
   glGetProgramiv(dev->prog, GL_LINK_STATUS, &status);
   assert(status == GL_TRUE);

   dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
   dev->attrib_pos   = glGetAttribLocation(dev->prog, "Position");
   dev->attrib_uv    = glGetAttribLocation(dev->prog, "TexCoord");
   dev->attrib_col   = glGetAttribLocation(dev->prog, "Color");

   {
      /* buffer setup */
      GLsizei vs = sizeof(struct zr_draw_vertex);
      size_t vp = offsetof(struct zr_draw_vertex, position);
      size_t vt = offsetof(struct zr_draw_vertex, uv);
      size_t vc = offsetof(struct zr_draw_vertex, col);

      glGenBuffers(1, &dev->vbo);
      glGenBuffers(1, &dev->ebo);
      glGenVertexArrays(1, &dev->vao);

      glBindVertexArray(dev->vao);
      glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

      glEnableVertexAttribArray((GLuint)dev->attrib_pos);
      glEnableVertexAttribArray((GLuint)dev->attrib_uv);
      glEnableVertexAttribArray((GLuint)dev->attrib_col);

      glVertexAttribPointer((GLuint)dev->attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void*)vp);
      glVertexAttribPointer((GLuint)dev->attrib_uv, 2, GL_FLOAT, GL_FALSE, vs, (void*)vt);
      glVertexAttribPointer((GLuint)dev->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void*)vc);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   glBindVertexArray(0);
#endif
}

static struct zr_user_font font_bake_and_upload(
      struct zr_device *dev,
      struct zr_font *font,
      const char *path,
      unsigned int font_height,
      const zr_rune *range)
{
   int glyph_count;
   int img_width, img_height;
   struct zr_font_glyph *glyphes;
   struct zr_baked_font baked_font;
   struct zr_user_font user_font;
   struct zr_recti custom;

   memset(&baked_font, 0, sizeof(baked_font));
   memset(&user_font, 0, sizeof(user_font));
   memset(&custom, 0, sizeof(custom));

   {
      struct texture_image ti;
      /* bake and upload font texture */
      struct zr_font_config config;
      void *img, *tmp;
      size_t ttf_size;
      size_t tmp_size, img_size;
      const char *custom_data = "....";
      char *ttf_blob = zrmenu_file_load(path, &ttf_size);
       /* setup font configuration */
      memset(&config, 0, sizeof(config));

      config.ttf_blob     = ttf_blob;
      config.ttf_size     = ttf_size;
      config.font         = &baked_font;
      config.coord_type   = ZR_COORD_UV;
      config.range        = range;
      config.pixel_snap   = zr_false;
      config.size         = (float)font_height;
      config.spacing      = zr_vec2(0,0);
      config.oversample_h = 1;
      config.oversample_v = 1;

      /* query needed amount of memory for the font baking process */
      zr_font_bake_memory(&tmp_size, &glyph_count, &config, 1);
      glyphes = (struct zr_font_glyph*)
         calloc(sizeof(struct zr_font_glyph), (size_t)glyph_count);
      tmp = calloc(1, tmp_size);

      /* pack all glyphes and return needed image width, height and memory size*/
      custom.w = 2; custom.h = 2;
      zr_font_bake_pack(&img_size,
            &img_width,&img_height,&custom,tmp,tmp_size,&config, 1);

      /* bake all glyphes and custom white pixel into image */
      img = calloc(1, img_size);
      zr_font_bake(img, img_width,
            img_height, tmp, tmp_size, glyphes, glyph_count, &config, 1);
      zr_font_bake_custom_data(img,
            img_width, img_height, custom, custom_data, 2, 2, '.', 'X');

      {
         /* convert alpha8 image into rgba8 image */
         void *img_rgba = calloc(4, (size_t)(img_height * img_width));
         zr_font_bake_convert(img_rgba, img_width, img_height, img);
         free(img);
         img = img_rgba;
      }

      /* upload baked font image */
      ti.pixels = (uint32_t*)img;
      ti.width  = (GLsizei)img_width;
      ti.height = (GLsizei)img_height;

      video_driver_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_NEAREST, (uintptr_t*)&dev->font_tex);

      free(ttf_blob);
      free(tmp);
      free(img);
   }

   /* default white pixel in a texture which is needed to draw primitives */
   dev->null.texture.id = (int)dev->font_tex;
   dev->null.uv = zr_vec2((custom.x + 0.5f)/(float)img_width,
      (custom.y + 0.5f)/(float)img_height);

   /* setup font with glyphes. IMPORTANT: the font only references the glyphes
      this was done to have the possibility to have multible fonts with one
      total glyph array. Not quite sure if it is a good thing since the
      glyphes have to be freed as well. */
   zr_font_init(font,
         (float)font_height, '?', glyphes,
         &baked_font, dev->null.texture);
   user_font = zr_font_ref(font);
   return user_font;
}

static void zr_device_shutdown(struct zr_device *dev)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   glDetachShader(dev->prog, dev->vert_shdr);
   glDetachShader(dev->prog, dev->frag_shdr);
   glDeleteShader(dev->vert_shdr);
   glDeleteShader(dev->frag_shdr);
   glDeleteProgram(dev->prog);
   glDeleteTextures(1, &dev->font_tex);
   glDeleteBuffers(1, &dev->vbo);
   glDeleteBuffers(1, &dev->ebo);
#endif
}

static void zr_device_draw(struct zr_device *dev,
      struct zr_context *ctx, int width, int height,
      enum zr_anti_aliasing AA)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLint last_prog, last_tex;
   GLint last_ebo, last_vbo, last_vao;
   GLfloat ortho[4][4] = {
      {2.0f, 0.0f, 0.0f, 0.0f},
      {0.0f,-2.0f, 0.0f, 0.0f},
      {0.0f, 0.0f,-1.0f, 0.0f},
      {-1.0f,1.0f, 0.0f, 1.0f},
   };
   ortho[0][0] /= (GLfloat)width;
   ortho[1][1] /= (GLfloat)height;

   /* save previous opengl state */
   glGetIntegerv(GL_CURRENT_PROGRAM, &last_prog);
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
   glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_vao);
   glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_ebo);
   glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vbo);

   /* setup global state */
   glEnable(GL_BLEND);
   glBlendEquation(GL_FUNC_ADD);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glActiveTexture(GL_TEXTURE0);

   /* setup program */
   glUseProgram(dev->prog);
   glUniformMatrix4fv(dev->uniform_proj, 1, GL_FALSE, &ortho[0][0]);

   {
      /* convert from command queue into draw list and draw to screen */
      const struct zr_draw_command *cmd;
      void *vertices, *elements;
      const zr_draw_index *offset = NULL;

      /* allocate vertex and element buffer */
      glBindVertexArray(dev->vao);
      glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

      glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_MEMORY, NULL, GL_STREAM_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_ELEMENT_MEMORY, NULL, GL_STREAM_DRAW);

      /* load draw vertices & elements directly into vertex + element buffer */
      vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
      elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
      {
         struct zr_buffer vbuf, ebuf;

         /* fill converting configuration */
         struct zr_convert_config config;
         memset(&config, 0, sizeof(config));
         config.global_alpha = 1.0f;
         config.shape_AA = AA;
         config.line_AA = AA;
         config.circle_segment_count = 22;
         config.line_thickness = 1.0f;
         config.null = dev->null;

         /* setup buffers to load vertices and elements */
         zr_buffer_init_fixed(&vbuf, vertices, MAX_VERTEX_MEMORY);
         zr_buffer_init_fixed(&ebuf, elements, MAX_ELEMENT_MEMORY);
         zr_convert(ctx, &dev->cmds, &vbuf, &ebuf, &config);
      }
      glUnmapBuffer(GL_ARRAY_BUFFER);
      glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

      /* iterate over and execute each draw command */
      zr_draw_foreach(cmd, ctx, &dev->cmds)
      {
         if (!cmd->elem_count)
            continue;
         glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
         glScissor((GLint)cmd->clip_rect.x,
            height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h),
            (GLint)cmd->clip_rect.w, (GLint)cmd->clip_rect.h);
         glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count,
               GL_UNSIGNED_SHORT, offset);
         offset += cmd->elem_count;
       }
       zr_clear(ctx);
   }

   /* restore old state */
   glUseProgram((GLuint)last_prog);
   glBindTexture(GL_TEXTURE_2D, (GLuint)last_tex);
   glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_vbo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_ebo);
   glBindVertexArray((GLuint)last_vao);
   glDisable(GL_BLEND);
#endif
}

static void* zrmenu_mem_alloc(zr_handle unused, size_t size)
{
   (void)unused;
   return calloc(1, size);
}

static void zrmenu_mem_free(zr_handle unused, void *ptr)
{
   (void)unused;
   free(ptr);
}

static void zrmenu_input_mouse_movement(struct zr_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   zr_input_motion(ctx, mouse_x, mouse_y);
   zr_input_scroll(ctx, menu_input_mouse_state(MENU_MOUSE_WHEEL_UP) -
      menu_input_mouse_state(MENU_MOUSE_WHEEL_DOWN));

}

static void zrmenu_input_mouse_button(struct zr_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   zr_input_button(ctx, ZR_BUTTON_LEFT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON));
   zr_input_button(ctx, ZR_BUTTON_RIGHT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON));
}

static void zrmenu_input_keyboard(struct zr_context *ctx)
{
   /* placeholder, it just presses 1 on right click
      needs to be hooked up correctly
   */
   if(menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON))
      zr_input_char(ctx, '1');
}

static void zrmenu_context_reset_textures(zrmenu_handle_t *zr,
      const char *iconpath)
{
   unsigned i;

   for (i = 0; i < ZR_TEXTURE_LAST; i++)
   {
      struct texture_image ti     = {0};
      char path[PATH_MAX_LENGTH]  = {0};

      switch(i)
      {
         case ZR_TEXTURE_POINTER:
            fill_pathname_join(path, iconpath,
                  "pointer.png", sizeof(path));
            break;
      }

      if (string_is_empty(path) || !path_file_exists(path))
         continue;

      video_texture_image_load(&ti, path);
      video_driver_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_LINEAR, &zr->textures.list[i]);

      video_texture_image_free(&ti);
   }
}

static void zrmenu_get_message(void *data, const char *message)
{
   zrmenu_handle_t *zr   = (zrmenu_handle_t*)data;

   if (!zr || !message || !*message)
      return;

   strlcpy(zr->box_message, message, sizeof(zr->box_message));
}

static void zrmenu_draw_cursor(zrmenu_handle_t *zr,
      float *color,
      float x, float y, unsigned width, unsigned height)
{
   menu_display_ctx_draw_t draw;
   struct gfx_coords coords;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_BEGIN, NULL);

   draw.x           = x - 32;
   draw.y           = (int)height - y - 32;
   draw.width       = 64;
   draw.height      = 64;
   draw.coords      = &coords;
   draw.matrix_data = NULL;
   draw.texture     = zr->textures.list[ZR_TEXTURE_POINTER];
   draw.prim_type   = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

   menu_display_ctl(MENU_DISPLAY_CTL_DRAW, &draw);

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_END, NULL);
}

static void zrmenu_frame(void *data)
{
   float white_bg[16]=  {
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
   };

   unsigned width, height, ticker_limit, i;
   zrmenu_handle_t *zr = (zrmenu_handle_t*)data;
   settings_t *settings  = config_get_ptr();

   if (!zr)
      return;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   zr_input_begin(&zr->ctx);
   zrmenu_input_mouse_movement(&zr->ctx);
   zrmenu_input_mouse_button(&zr->ctx);
   zrmenu_input_keyboard(&zr->ctx);

   if (width != zr->size.x || height != zr->size.y)
   {
      zr->size.x = width;
      zr->size.y = height;
      zr->size_changed = true;
   }

   zr_input_end(&zr->ctx);
   zrmenu_main(zr);
   zr_device_draw(&device, &zr->ctx, width, height, ZR_ANTI_ALIASING_ON);

   if (settings->menu.mouse.enable && (settings->video.fullscreen
            || !video_driver_ctl(RARCH_DISPLAY_CTL_HAS_WINDOWED, NULL)))
   {
      int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
      int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      zrmenu_draw_cursor(zr, &white_bg[0], mouse_x, mouse_y, width, height);
   }

   menu_display_ctl(MENU_DISPLAY_CTL_RESTORE_CLEAR_COLOR, NULL);
   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}

static void zrmenu_layout(zrmenu_handle_t *zr)
{
   void *fb_buf;
   float scale_factor;
   unsigned width, height, new_header_height;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_GET_DPI, &scale_factor);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEADER_HEIGHT,
         &new_header_height);

}

static void zrmenu_init_device(zrmenu_handle_t *zr)
{
   char buf[PATH_MAX_LENGTH];
   fill_pathname_join(buf, zr->assets_directory,
         "DroidSans.ttf", sizeof(buf));

   zr_alloc.userdata.ptr = NULL;
   zr_alloc.alloc = zrmenu_mem_alloc;
   zr_alloc.free = zrmenu_mem_free;
   zr_buffer_init(&device.cmds, &zr_alloc, 1024);
   usrfnt = font_bake_and_upload(&device, &font, buf, 16,
      zr_font_default_glyph_ranges());
   zr_init(&zr->ctx, &zr_alloc, &usrfnt);
   zr_device_init(&device);

   fill_pathname_join(buf, zr->assets_directory, "folder.png", sizeof(buf));
   zr->icons.folder = zr_icon_load(buf);
   fill_pathname_join(buf, zr->assets_directory, "speaker.png", sizeof(buf));
   zr->icons.speaker = zr_icon_load(buf);

   zrmenu_set_style(&zr->ctx, THEME_DARK);
   zr->size_changed = true;
}

static void *zrmenu_init(void **userdata)
{
   settings_t *settings = config_get_ptr();
   zrmenu_handle_t   *zr = NULL;
   menu_handle_t *menu = (menu_handle_t*)
      calloc(1, sizeof(*menu));
   unsigned width, height = 0;

   video_driver_get_size(&width, &height);

   if (!menu)
      goto error;

   if (!menu_display_ctl(MENU_DISPLAY_CTL_INIT_FIRST_DRIVER, NULL))
      goto error;

   zr = (zrmenu_handle_t*)calloc(1, sizeof(zrmenu_handle_t));

   if (!zr)
      goto error;

   *userdata = zr;

   fill_pathname_join(zr->assets_directory, settings->assets_directory,
         "zahnrad", sizeof(zr->assets_directory));
   zrmenu_init_device(zr);

   zr->window[ZRMENU_WND_WIZARD].open = true;

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void zrmenu_free(void *data)
{
   zrmenu_handle_t *zr   = (zrmenu_handle_t*)data;

   if (!zr)
      return;

   free(font.glyphs);
   zr_free(&zr->ctx);
   zr_buffer_free(&device.cmds);
   zr_device_shutdown(&device);

   gfx_coord_array_free(&zr->list_block.carr);
   font_driver_bind_block(NULL, NULL);
}

static void wimp_context_bg_destroy(zrmenu_handle_t *zr)
{
   if (!zr)
      return;

}

static void zrmenu_context_destroy(void *data)
{
   unsigned i;
   zrmenu_handle_t *zr   = (zrmenu_handle_t*)data;

   if (!zr)
      return;

   for (i = 0; i < ZR_TEXTURE_LAST; i++)
      video_driver_texture_unload((uintptr_t*)&zr->textures.list[i]);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_MAIN_DEINIT, NULL);

   wimp_context_bg_destroy(zr);
}

static void zrmenu_context_reset(void *data)
{
   char iconpath[PATH_MAX_LENGTH] = {0};
   zrmenu_handle_t *zr              = (zrmenu_handle_t*)data;
   settings_t *settings           = config_get_ptr();
   unsigned width, height = 0;

   video_driver_get_size(&width, &height);

   if (!zr || !settings)
      return;

   fill_pathname_join(iconpath, settings->assets_directory,
         "zahnrad", sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   zrmenu_layout(zr);
   zrmenu_init_device(zr);

   wimp_context_bg_destroy(zr);
   zrmenu_context_reset_textures(zr, iconpath);

   rarch_task_push_image_load(settings->menu.wallpaper, "cb_menu_wallpaper",
         menu_display_handle_wallpaper_upload, NULL);
}

static int zrmenu_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   switch (type)
   {
      case 0:
      default:
         break;
   }

   return -1;
}

static bool zrmenu_init_list(void *data)
{
   menu_displaylist_info_t info = {0};
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   strlcpy(info.label,
         menu_hash_to_str(MENU_VALUE_HISTORY_TAB), sizeof(info.label));

   menu_entries_push(menu_stack,
         info.path, info.label, info.type, info.flags, 0);

   event_cmd_ctl(EVENT_CMD_HISTORY_INIT, NULL);

   info.list  = selection_buf;

   if (menu_displaylist_ctl(DISPLAYLIST_HISTORY, &info))
   {
      info.need_push = true;
      return menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info);
   }

   return false;
}

menu_ctx_driver_t menu_ctx_zr = {
   NULL,
   zrmenu_get_message,
   generic_menu_iterate,
   NULL,
   zrmenu_frame,
   zrmenu_init,
   zrmenu_free,
   zrmenu_context_reset,
   zrmenu_context_destroy,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   zrmenu_init_list,
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
   "zahnrad",
   zrmenu_environ,
   NULL,
};
