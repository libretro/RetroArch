/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-Andr� Santoni
 *  Copyright (C) 2016      - Andr�s Su�rez
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

#include <streams/file_stream.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION

#include "nk_common.h"

#include "../menu_display.h"
#include "../../gfx/video_shader_driver.h"

#include "../../gfx/drivers/gl_shaders/pipeline_zahnrad.glsl.vert.h"
#include "../../gfx/drivers/gl_shaders/pipeline_zahnrad.glsl.frag.h"

struct nk_font *font;
struct nk_font_atlas atlas;
struct nk_user_font usrfnt;
struct nk_allocator nk_alloc;
struct nk_device device;

struct nk_image nk_common_image_load(const char *filename)
{
    int x,y,n;
    GLuint tex;
    unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
    if (!data) printf("Failed to load image: %s\n", filename);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
#endif

    stbi_image_free(data);
    return nk_image_id((int)tex);
}

char* nk_common_file_load(const char* path, size_t* size)
{
   void *buf;
   ssize_t *length = (ssize_t*)size;
   filestream_read_file(path, &buf, length);
   return (char*)buf;
}

NK_API void nk_common_device_init(struct nk_device *dev)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLint status;

   dev->prog      = glCreateProgram();
   dev->vert_shdr = glCreateShader(GL_VERTEX_SHADER);
   dev->frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(dev->vert_shdr, 1, &zahnrad_vertex_shader, 0);
   glShaderSource(dev->frag_shdr, 1, &zahnrad_fragment_shader, 0);
   glCompileShader(dev->vert_shdr);
   glCompileShader(dev->frag_shdr);
   glGetShaderiv(dev->vert_shdr, GL_COMPILE_STATUS, &status);
   glGetShaderiv(dev->frag_shdr, GL_COMPILE_STATUS, &status);
   glAttachShader(dev->prog, dev->vert_shdr);
   glAttachShader(dev->prog, dev->frag_shdr);
   glLinkProgram(dev->prog);
   glGetProgramiv(dev->prog, GL_LINK_STATUS, &status);

   dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
   dev->attrib_pos   = glGetAttribLocation(dev->prog, "Position");
   dev->attrib_uv    = glGetAttribLocation(dev->prog, "TexCoord");
   dev->attrib_col   = glGetAttribLocation(dev->prog, "Color");

   {
      /* buffer setup */
      GLsizei vs = sizeof(struct nk_draw_vertex);
      size_t vp = offsetof(struct nk_draw_vertex, position);
      size_t vt = offsetof(struct nk_draw_vertex, uv);
      size_t vc = offsetof(struct nk_draw_vertex, col);

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

void device_upload_atlas(struct nk_device *dev, const void *image, int width, int height)
{
    glGenTextures(1, &dev->font_tex);
    glBindTexture(GL_TEXTURE_2D, dev->font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, image);
}

void nk_common_device_shutdown(struct nk_device *dev)
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

void nk_common_device_draw(struct nk_device *dev,
      struct nk_context *ctx, int width, int height,
      enum nk_anti_aliasing AA)
{
   video_shader_ctx_info_t shader_info;
   struct nk_buffer vbuf, ebuf;
   struct nk_convert_config config;
   uintptr_t                 last_prog;
   const struct nk_draw_command *cmd = NULL;
   void                    *vertices = NULL;
   void                    *elements = NULL;
   const nk_draw_index       *offset = NULL;
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLint last_tex;
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
   glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&last_prog);
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
   glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_vao);
   glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_ebo);
   glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vbo);
#endif

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_BEGIN, NULL);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   glActiveTexture(GL_TEXTURE0);
#endif

   /* setup program */
   shader_info.data       = NULL;
   shader_info.idx        = dev->prog;
   shader_info.set_active = false;
   video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   glUniformMatrix4fv(dev->uniform_proj, 1, GL_FALSE, &ortho[0][0]);

   /* convert from command queue into draw list and draw to screen */

   /* allocate vertex and element buffer */
   glBindVertexArray(dev->vao);
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
   //config.line_thickness       = 1.0f;
   config.null                 = dev->null;

   /* setup buffers to load vertices and elements */
   nk_buffer_init_fixed(&vbuf, vertices, MAX_VERTEX_MEMORY);
   nk_buffer_init_fixed(&ebuf, elements, MAX_ELEMENT_MEMORY);
   nk_convert(ctx, &dev->cmds, &vbuf, &ebuf, &config);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   glUnmapBuffer(GL_ARRAY_BUFFER);
   glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
#endif

   /* iterate over and execute each draw command */
   nk_draw_foreach(cmd, ctx, &dev->cmds)
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
   nk_clear(ctx);

   /* restore old state */
   shader_info.data       = NULL;
   shader_info.idx        = (GLint)last_prog;
   shader_info.set_active = false;
   video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   glBindTexture(GL_TEXTURE_2D, (GLuint)last_tex);
   glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_vbo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_ebo);
   glBindVertexArray((GLuint)last_vao);
#endif

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_END, NULL);
}

//void nk_mem_alloc(nk_handle a, void *old, nk_size b);
void* nk_common_mem_alloc(nk_handle a, void *old, nk_size b)
{
   (void)a;
   return calloc(1, b);
}

void nk_common_mem_free(nk_handle unused, void *ptr)
{
   (void)unused;
   free(ptr);
}
