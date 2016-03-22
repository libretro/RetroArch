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

#include "zr_common.h"

#include "../menu_display.h"

struct zr_image zr_common_image_load(const char *filename)
{
   GLuint tex;
   struct texture_image ti;

   video_texture_image_load(&ti,
         filename);

   video_driver_texture_load(&ti,
         TEXTURE_FILTER_MIPMAP_NEAREST, (uintptr_t*)&tex);

   return zr_image_id((int)tex);
}

char* zr_common_file_load(const char* path, size_t* size)
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

void zr_common_device_init(struct zr_device *dev)
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

struct zr_user_font zr_common_font(
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
      char *ttf_blob = zr_common_file_load(path, &ttf_size);
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

void zr_common_device_shutdown(struct zr_device *dev)
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

void zr_common_device_draw(struct zr_device *dev,
      struct zr_context *ctx, int width, int height,
      enum zr_anti_aliasing AA)
{
   struct zr_buffer vbuf, ebuf;
   struct zr_convert_config config;
   const struct zr_draw_command *cmd = NULL;
   void                    *vertices = NULL;
   void                    *elements = NULL;
   const zr_draw_index       *offset = NULL;
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLint last_prog, last_tex;
   GLint last_ebo, last_vbo;
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
   glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_ebo);
   glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vbo);
#endif

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_BEGIN, NULL);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   glActiveTexture(GL_TEXTURE0);

   /* setup program */
   glUseProgram(dev->prog);
   glUniformMatrix4fv(dev->uniform_proj, 1, GL_FALSE, &ortho[0][0]);

   /* convert from command queue into draw list and draw to screen */

   /* allocate vertex and element buffer */
   glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

   glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_MEMORY, NULL, GL_STREAM_DRAW);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_ELEMENT_MEMORY, NULL, GL_STREAM_DRAW);

   /* load draw vertices & elements directly into vertex + element buffer */
   vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
   elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
#endif

   /* fill converting configuration */
   memset(&config, 0, sizeof(config));

   config.global_alpha         = 1.0f;
   config.shape_AA             = AA;
   config.line_AA              = AA;
   config.circle_segment_count = 22;
   config.line_thickness       = 1.0f;
   config.null                 = dev->null;

   /* setup buffers to load vertices and elements */
   zr_buffer_init_fixed(&vbuf, vertices, MAX_VERTEX_MEMORY);
   zr_buffer_init_fixed(&ebuf, elements, MAX_ELEMENT_MEMORY);
   zr_convert(ctx, &dev->cmds, &vbuf, &ebuf, &config);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   glUnmapBuffer(GL_ARRAY_BUFFER);
   glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
#endif

   /* iterate over and execute each draw command */
   zr_draw_foreach(cmd, ctx, &dev->cmds)
   {
      if (!cmd->elem_count)
         continue;

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
      glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
      glScissor((GLint)cmd->clip_rect.x,
            height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h),
            (GLint)cmd->clip_rect.w, (GLint)cmd->clip_rect.h);
      glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count,
            GL_UNSIGNED_SHORT, offset);
#endif

      offset += cmd->elem_count;
   }
   zr_clear(ctx);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   /* restore old state */
   glUseProgram((GLuint)last_prog);
   glBindTexture(GL_TEXTURE_2D, (GLuint)last_tex);
   glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_vbo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_ebo);
#endif

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_END, NULL);
}

void* zr_common_mem_alloc(zr_handle unused, size_t size)
{
   (void)unused;
   return calloc(1, size);
}

void zr_common_mem_free(zr_handle unused, void *ptr)
{
   (void)unused;
   free(ptr);
}
