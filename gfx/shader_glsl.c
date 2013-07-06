/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "../boolean.h"
#include <string.h>
#include "../general.h"
#include "shader_glsl.h"
#include "../compat/strl.h"
#include "../compat/posix_string.h"
#include "state_tracker.h"
#include "../dynamic.h"
#include "../file.h"

#define GLSL_DEBUG

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(IOS)
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#elif defined(__APPLE__) // Because they like to be "oh, so, special".
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#elif defined(HAVE_PSGL)
#include <PSGL/psgl.h>
#include <PSGL/psglu.h>
#include <GLES/glext.h>
#elif defined(HAVE_OPENGL_MODERN)
#include <EGL/egl.h>
#include <GL3/gl3.h>
#include <GL3/gl3ext.h>
#elif defined(HAVE_OPENGLES2)
#include <GLES2/gl2.h>
#elif defined(HAVE_OPENGLES1)
#include <GLES/gl.h>
#include <GLES/glext.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include "gfx_context.h"
#include <stdlib.h>

#include "gl_common.h"
#include "image.h"

#ifdef HAVE_OPENGLES2
#define BORDER_FUNC GL_CLAMP_TO_EDGE
#else
#define BORDER_FUNC GL_CLAMP_TO_BORDER
#endif

#define PREV_TEXTURES (TEXTURES - 1)

static struct gfx_shader *glsl_shader;
static bool glsl_core;
static unsigned glsl_major;
static unsigned glsl_minor;

static bool glsl_enable;
static GLuint gl_program[GFX_MAX_SHADERS];
static unsigned active_index;

static GLuint gl_teximage[GFX_MAX_TEXTURES];

static state_tracker_t *gl_state_tracker;

static GLint gl_attribs[PREV_TEXTURES + 1 + 4 + GFX_MAX_SHADERS];
static unsigned gl_attrib_index;

// Cache the VBO.
struct cache_vbo
{
   GLuint vbo_primary;
   GLfloat buffer_primary[128];
   size_t size_primary;

   GLuint vbo_secondary;
   GLfloat buffer_secondary[128];
   size_t size_secondary;
};
static struct cache_vbo glsl_vbo[GFX_MAX_SHADERS];

struct glsl_attrib
{
   GLint loc;
   GLsizei size;
   GLsizei offset;
};

static gfx_ctx_proc_t (*glsl_get_proc_address)(const char*);

struct shader_uniforms_frame
{
   int texture;
   int input_size;
   int texture_size;
   int tex_coord;
};

struct shader_uniforms
{
   int mvp;
   int tex_coord;
   int vertex_coord;
   int color;
   int lut_tex_coord;

   int input_size;
   int output_size;
   int texture_size;

   int frame_count;
   unsigned frame_count_mod;
   int frame_direction;

   int lut_texture[GFX_MAX_TEXTURES];
   
   struct shader_uniforms_frame orig;
   struct shader_uniforms_frame pass[GFX_MAX_SHADERS];
   struct shader_uniforms_frame prev[PREV_TEXTURES];
};

static struct shader_uniforms gl_uniforms[GFX_MAX_SHADERS];

static const char *glsl_prefixes[] = {
   "",
   "ruby",
};

// Need to duplicate these to work around broken stuff on Android.
// Must enforce alpha = 1.0 or 32-bit games can potentially go black.
static const char *stock_vertex_modern =
   "attribute vec2 TexCoord;\n"
   "attribute vec2 VertexCoord;\n"
   "attribute vec4 Color;\n"
   "uniform mat4 MVPMatrix;\n"
   "varying vec2 tex_coord;\n"
   "void main() {\n"
   "   gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);\n"
   "   tex_coord = TexCoord;\n"
   "}";

static const char *stock_fragment_modern =
   "#ifdef GL_ES\n"
   "precision mediump float;\n"
   "#endif\n"
   "uniform sampler2D Texture;\n"
   "varying vec2 tex_coord;\n"
   "void main() {\n"
   "   gl_FragColor = vec4(texture2D(Texture, tex_coord).rgb, 1.0);\n"
   "}";

static const char *stock_vertex_core =
   "in vec2 TexCoord;\n"
   "in vec2 VertexCoord;\n"
   "in vec4 Color;\n"
   "uniform mat4 MVPMatrix;\n"
   "out vec2 tex_coord;\n"
   "void main() {\n"
   "   gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);\n"
   "   tex_coord = TexCoord;\n"
   "}";

static const char *stock_fragment_core =
   "uniform sampler2D Texture;\n"
   "in vec2 tex_coord;\n"
   "out vec4 FragColor;\n"
   "void main() {\n"
   "   FragColor = vec4(texture(Texture, tex_coord).rgb, 1.0);\n"
   "}";

static const char *stock_vertex_legacy =
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
   "   gl_TexCoord[0] = gl_MultiTexCoord0;\n"
   "   color = gl_Color;\n"
   "}";

static const char *stock_fragment_legacy =
   "uniform sampler2D Texture;\n"
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_FragColor = color * texture2D(Texture, gl_TexCoord[0].xy);\n"
   "}";

static const char *stock_vertex_modern_blend =
   "attribute vec2 TexCoord;\n"
   "attribute vec2 VertexCoord;\n"
   "attribute vec4 Color;\n"
   "uniform mat4 MVPMatrix;\n"
   "varying vec2 tex_coord;\n"
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);\n"
   "   tex_coord = TexCoord;\n"
   "   color = Color;\n"
   "}";

static const char *stock_fragment_modern_blend =
   "#ifdef GL_ES\n"
   "precision mediump float;\n"
   "#endif\n"
   "uniform sampler2D Texture;\n"
   "varying vec2 tex_coord;\n"
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_FragColor = color * texture2D(Texture, tex_coord);\n"
   "}";

#ifndef HAVE_OPENGLES2
static const char *stock_vertex_core_blend =
   "in vec2 TexCoord;\n"
   "in vec2 VertexCoord;\n"
   "in vec4 Color;\n"
   "uniform mat4 MVPMatrix;\n"
   "out vec2 tex_coord;\n"
   "out vec4 color;\n"
   "void main() {\n"
   "   gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);\n"
   "   tex_coord = TexCoord;\n"
   "   color = Color;\n"
   "}";

static const char *stock_fragment_core_blend =
   "uniform sampler2D Texture;\n"
   "in vec2 tex_coord;\n"
   "in vec4 color;\n"
   "out vec4 FragColor;\n"
   "void main() {\n"
   "   FragColor = color * texture(Texture, tex_coord);\n"
   "}";
#endif

static GLint get_uniform(GLuint prog, const char *base)
{
   char buf[64];

   snprintf(buf, sizeof(buf), "%s%s", glsl_shader->prefix, base);
   GLint loc = glGetUniformLocation(prog, buf);
   if (loc >= 0)
      return loc;

   for (unsigned i = 0; i < ARRAY_SIZE(glsl_prefixes); i++)
   {
      snprintf(buf, sizeof(buf), "%s%s", glsl_prefixes[i], base);
      GLint loc = glGetUniformLocation(prog, buf);
      if (loc >= 0)
         return loc;
   }

   return -1;
}

static GLint get_attrib(GLuint prog, const char *base)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%s%s", glsl_shader->prefix, base);
   GLint loc = glGetUniformLocation(prog, buf);
   if (loc >= 0)
      return loc;

   for (unsigned i = 0; i < ARRAY_SIZE(glsl_prefixes); i++)
   {
      snprintf(buf, sizeof(buf), "%s%s", glsl_prefixes[i], base);
      GLint loc = glGetAttribLocation(prog, buf);
      if (loc >= 0)
         return loc;
   }

   return -1;
}

static bool load_luts(void)
{
   if (!glsl_shader->luts)
      return true;

   glGenTextures(1, gl_teximage);

   for (unsigned i = 0; i < glsl_shader->luts; i++)
   {
      RARCH_LOG("Loading texture image from: \"%s\" ...\n",
            glsl_shader->lut[i].path);

      struct texture_image img = {0};
      if (!texture_image_load(glsl_shader->lut[i].path, &img))
      {
         RARCH_ERR("Failed to load texture image from: \"%s\"\n", glsl_shader->lut[i].path);
         return false;
      }

      glBindTexture(GL_TEXTURE_2D, gl_teximage[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, BORDER_FUNC);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, BORDER_FUNC);

      GLenum filter = glsl_shader->lut[i].filter == RARCH_FILTER_NEAREST ?
         GL_NEAREST : GL_LINEAR;
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);

      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      glTexImage2D(GL_TEXTURE_2D,
            0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
            img.width, img.height, 0,
            driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
            RARCH_GL_FORMAT32, img.pixels);

      glBindTexture(GL_TEXTURE_2D, 0);
      free(img.pixels);
   }

   return true;
}

static void print_shader_log(GLuint obj)
{
   GLint info_len = 0;
   GLint max_len;

   glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &max_len);

   if (max_len == 0)
      return;

   char *info_log = (char*)malloc(max_len);
   if (!info_log)
      return;

   glGetShaderInfoLog(obj, max_len, &info_len, info_log);

   if (info_len > 0)
      RARCH_LOG("Shader log: %s\n", info_log);

   free(info_log);
}

static void print_linker_log(GLuint obj)
{
   GLint info_len = 0;
   GLint max_len;

   glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &max_len);

   if (max_len == 0)
      return;

   char *info_log = (char*)malloc(max_len);
   if (!info_log)
      return;

   glGetProgramInfoLog(obj, max_len, &info_len, info_log);

   if (info_len > 0)
      RARCH_LOG("Linker log: %s\n", info_log);

   free(info_log);
}

static bool compile_shader(GLuint shader, const char *define, const char *program)
{
   char version[32] = {0};
   if (glsl_core && !strstr(program, "#version"))
   {
      unsigned version_no = 0;
      unsigned gl_ver = glsl_major * 100 + glsl_minor * 10;
      switch (gl_ver)
      {
         case 300: version_no = 130; break;
         case 310: version_no = 140; break;
         case 320: version_no = 150; break;
         default: version_no = gl_ver; break;
      }

      snprintf(version, sizeof(version), "#version %u\n", version_no);
      RARCH_LOG("[GL]: Using GLSL version %u.\n", version_no);
   }

   const char *source[] = { version, define, program };
   glShaderSource(shader, ARRAY_SIZE(source), source, NULL);
   glCompileShader(shader);

   GLint status;
   glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
   print_shader_log(shader);

   return status == GL_TRUE;
}

static bool link_program(GLuint prog)
{
   glLinkProgram(prog);

   GLint status;
   glGetProgramiv(prog, GL_LINK_STATUS, &status);
   print_linker_log(prog);

   if (status == GL_TRUE)
   {
      glUseProgram(prog);
      return true;
   }
   else
      return false;
}

static GLuint compile_program(const char *vertex, const char *fragment, unsigned i)
{
   GLuint prog = glCreateProgram();
   if (!prog)
      return 0;

   if (vertex)
   {
      RARCH_LOG("Found GLSL vertex shader.\n");
      GLuint shader = glCreateShader(GL_VERTEX_SHADER);
      if (!compile_shader(shader, "#define VERTEX\n", vertex))
      {
         RARCH_ERR("Failed to compile vertex shader #%u\n", i);
         return false;
      }

      glAttachShader(prog, shader);
   }

   if (fragment)
   {
      RARCH_LOG("Found GLSL fragment shader.\n");
      GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);
      if (!compile_shader(shader, "#define FRAGMENT\n", fragment))
      {
         RARCH_ERR("Failed to compile fragment shader #%u\n", i);
         return false;
      }

      glAttachShader(prog, shader);
   }

   if (vertex || fragment)
   {
      RARCH_LOG("Linking GLSL program.\n");
      if (!link_program(prog))
      {
         RARCH_ERR("Failed to link program #%u.\n", i);
         return 0;
      }

      glUseProgram(prog);
      GLint location = get_uniform(prog, "Texture");
      glUniform1i(location, 0);
      glUseProgram(0);
   }

   return prog;
}

static bool load_source_path(struct gfx_shader_pass *pass, const char *path)
{
   if (read_file(path, (void**)&pass->source.xml.vertex) <= 0)
      return false;

   pass->source.xml.fragment = strdup(pass->source.xml.vertex);
   return pass->source.xml.fragment && pass->source.xml.vertex;
}

static bool compile_programs(GLuint *gl_prog)
{
   for (unsigned i = 0; i < glsl_shader->passes; i++)
   {
      struct gfx_shader_pass *pass = &glsl_shader->pass[i];

      // If we load from GLSLP (CGP),
      // load the file here, and pretend
      // we were really using XML all along.
      if (*pass->source.cg && !load_source_path(pass, pass->source.cg))
      {
         RARCH_ERR("Failed to load GLSL shader: %s.\n", pass->source.cg);
         return false;
      }
      *pass->source.cg = '\0';

      const char *vertex   = pass->source.xml.vertex;
      const char *fragment = pass->source.xml.fragment;

      gl_prog[i] = compile_program(vertex, fragment, i);

      if (!gl_prog[i])
      {
         RARCH_ERR("Failed to create GL program #%u.\n", i);
         return false;
      }
   }

   return true;
}

static void gl_glsl_reset_attrib(void)
{
   for (unsigned i = 0; i < gl_attrib_index; i++)
      glDisableVertexAttribArray(gl_attribs[i]);
   gl_attrib_index = 0;
}

static void gl_glsl_set_vbo(GLfloat *buffer, size_t *buffer_elems, const GLfloat *data, size_t elems)
{
   if (elems != *buffer_elems || memcmp(data, buffer, elems * sizeof(GLfloat)))
   {
      //RARCH_LOG("[GL]: VBO updated with %u elems.\n", (unsigned)elems);
      memcpy(buffer, data, elems * sizeof(GLfloat));
      glBufferData(GL_ARRAY_BUFFER, elems * sizeof(GLfloat), data, GL_STATIC_DRAW);
      *buffer_elems = elems;
   }
}

static void gl_glsl_set_attribs(GLuint vbo, GLfloat *buffer, size_t *buffer_elems,
      const GLfloat *data, size_t elems, const struct glsl_attrib *attrs, size_t num_attrs)
{
   glBindBuffer(GL_ARRAY_BUFFER, vbo);

   gl_glsl_set_vbo(buffer, buffer_elems, data, elems);

   for (size_t i = 0; i < num_attrs; i++)
   {
      GLint loc = attrs[i].loc;
      glEnableVertexAttribArray(loc);
      gl_attribs[gl_attrib_index++] = loc;

      glVertexAttribPointer(loc, attrs[i].size, GL_FLOAT, GL_FALSE, 0,
            (const GLvoid*)(uintptr_t)attrs[i].offset);
   }

   glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void clear_uniforms_frame(struct shader_uniforms_frame *frame)
{
   frame->texture      = -1;
   frame->texture_size = -1;
   frame->input_size   = -1;
   frame->tex_coord    = -1;
}

static void find_uniforms_frame(GLuint prog, struct shader_uniforms_frame *frame, const char *base)
{
   char texture[64];
   char texture_size[64];
   char input_size[64];
   char tex_coord[64];

   snprintf(texture, sizeof(texture), "%s%s", base, "Texture");
   snprintf(texture_size, sizeof(texture_size), "%s%s", base, "TextureSize");
   snprintf(input_size, sizeof(input_size), "%s%s", base, "InputSize");
   snprintf(tex_coord, sizeof(tex_coord), "%s%s", base, "TexCoord");

   if (frame->texture < 0)
      frame->texture = get_uniform(prog, texture);
   if (frame->texture_size < 0)
      frame->texture_size = get_uniform(prog, texture_size);
   if (frame->input_size < 0)
      frame->input_size = get_uniform(prog, input_size);
   if (frame->tex_coord < 0)
      frame->tex_coord = get_attrib(prog, tex_coord);
}

static void find_uniforms(unsigned pass, GLuint prog, struct shader_uniforms *uni)
{
   glUseProgram(prog);

   uni->mvp           = get_uniform(prog, "MVPMatrix");
   uni->tex_coord     = get_attrib(prog, "TexCoord");
   uni->vertex_coord  = get_attrib(prog, "VertexCoord");
   uni->color         = get_attrib(prog, "Color");
   uni->lut_tex_coord = get_attrib(prog, "LUTTexCoord");

   uni->input_size    = get_uniform(prog, "InputSize");
   uni->output_size   = get_uniform(prog, "OutputSize");
   uni->texture_size  = get_uniform(prog, "TextureSize");

   uni->frame_count     = get_uniform(prog, "FrameCount");
   uni->frame_direction = get_uniform(prog, "FrameDirection");

   for (unsigned i = 0; i < glsl_shader->luts; i++)
      uni->lut_texture[i] = glGetUniformLocation(prog, glsl_shader->lut[i].id);

   char frame_base[64];
   clear_uniforms_frame(&uni->orig);
   find_uniforms_frame(prog, &uni->orig, "Orig");
   if (pass > 1)
   {
      snprintf(frame_base, sizeof(frame_base), "PassPrev%u", pass);
      find_uniforms_frame(prog, &uni->orig, frame_base);
   }

   for (unsigned i = 0; i < GFX_MAX_SHADERS; i++)
   {
      snprintf(frame_base, sizeof(frame_base), "Pass%u", i + 1);
      clear_uniforms_frame(&uni->pass[i]);
      find_uniforms_frame(prog, &uni->pass[i], frame_base);
      if (i && pass > i + 1)
      {
         snprintf(frame_base, sizeof(frame_base), "PassPrev%u", pass - i);
         find_uniforms_frame(prog, &uni->pass[i], frame_base);
      }
   }

   clear_uniforms_frame(&uni->prev[0]);
   find_uniforms_frame(prog, &uni->prev[0], "Prev");
   for (unsigned i = 1; i < PREV_TEXTURES; i++)
   {
      snprintf(frame_base, sizeof(frame_base), "Prev%u", i);
      clear_uniforms_frame(&uni->prev[i]);
      find_uniforms_frame(prog, &uni->prev[i], frame_base);
   }

   glUseProgram(0);
}

static void gl_glsl_delete_shader(GLuint prog)
{
   GLsizei count;
   GLuint shaders[2] = {0};

   glGetAttachedShaders(prog, 2, &count, shaders);
   for (GLsizei i = 0; i < count; i++)
   {
      glDetachShader(prog, shaders[i]);
      glDeleteShader(shaders[i]);
   }

   glDeleteProgram(prog);
}

static void gl_glsl_free_shader(void)
{
   if (!glsl_shader)
      return;

   for (unsigned i = 0; i < glsl_shader->passes; i++)
   {
      free(glsl_shader->pass[i].source.xml.vertex);
      free(glsl_shader->pass[i].source.xml.fragment);
   }

   free(glsl_shader->script);
   free(glsl_shader);
   glsl_shader = NULL;
}

static void gl_glsl_deinit(void)
{
   glUseProgram(0);
   for (unsigned i = 0; i < GFX_MAX_SHADERS; i++)
   {
      if (gl_program[i] == 0 || (i && gl_program[i] == gl_program[0]))
         continue;

      gl_glsl_delete_shader(gl_program[i]);
   }

   if (glsl_shader && glsl_shader->luts)
      glDeleteTextures(glsl_shader->luts, gl_teximage);

   memset(gl_program, 0, sizeof(gl_program));
   memset(gl_uniforms, 0, sizeof(gl_uniforms));
   glsl_enable  = false;
   active_index = 0;

   gl_glsl_free_shader();

   if (gl_state_tracker)
      state_tracker_free(gl_state_tracker);
   gl_state_tracker = NULL;

   gl_glsl_reset_attrib();

   for (unsigned i = 0; i < GFX_MAX_SHADERS; i++)
   {
      if (glsl_vbo[i].vbo_primary)
         glDeleteBuffers(1, &glsl_vbo[i].vbo_primary);
      if (glsl_vbo[i].vbo_secondary)
         glDeleteBuffers(1, &glsl_vbo[i].vbo_secondary);
   }
   memset(&glsl_vbo, 0, sizeof(glsl_vbo));
}

static bool gl_glsl_init(const char *path)
{
#ifndef HAVE_OPENGLES2
   RARCH_LOG("Checking GLSL shader support ...\n");
   bool shader_support = glCreateProgram && glUseProgram && glCreateShader
      && glDeleteShader && glShaderSource && glCompileShader && glAttachShader
      && glDetachShader && glLinkProgram && glGetUniformLocation
      && glUniform1i && glUniform1f && glUniform2fv && glUniform4fv && glUniformMatrix4fv
      && glGetShaderiv && glGetShaderInfoLog && glGetProgramiv && glGetProgramInfoLog 
      && glDeleteProgram && glGetAttachedShaders
      && glGetAttribLocation && glEnableVertexAttribArray && glDisableVertexAttribArray
      && glVertexAttribPointer
      && glGenBuffers && glBufferData && glDeleteBuffers && glBindBuffer;

   if (!shader_support)
   {
      RARCH_ERR("GLSL shaders aren't supported by your OpenGL driver.\n");
      return false;
   }
#endif

   glsl_shader = (struct gfx_shader*)calloc(1, sizeof(*glsl_shader));
   if (!glsl_shader)
      return false;

   if (path)
   {
      bool ret;
      if (strcmp(path_get_extension(path), "glsl") == 0)
      {
         strlcpy(glsl_shader->pass[0].source.cg, path, sizeof(glsl_shader->pass[0].source.cg));
         glsl_shader->passes = 1;
         glsl_shader->modern = true;
         ret = true;
      }
      else if (strcmp(path_get_extension(path), "glslp") == 0)
      {
         config_file_t *conf = config_file_new(path);
         if (conf)
         {
            ret = gfx_shader_read_conf_cgp(conf, glsl_shader);
            glsl_shader->modern = true;
            config_file_free(conf);
         }
         else
            ret = false;
      }
      else
         ret = gfx_shader_read_xml(path, glsl_shader);

      if (!ret)
      {
         RARCH_ERR("[GL]: Failed to parse GLSL shader.\n");
         return false;
      }
   }
   else
   {
      RARCH_WARN("[GL]: Stock GLSL shaders will be used.\n");
      glsl_shader->passes = 1;
      glsl_shader->pass[0].source.xml.vertex   = strdup(glsl_core ? stock_vertex_core : stock_vertex_modern);
      glsl_shader->pass[0].source.xml.fragment = strdup(glsl_core ? stock_fragment_core : stock_fragment_modern);
      glsl_shader->modern = true;
   }

   gfx_shader_resolve_relative(glsl_shader, path);

   const char *stock_vertex = glsl_shader->modern ?
      stock_vertex_modern : stock_vertex_legacy;
   const char *stock_fragment = glsl_shader->modern ?
      stock_fragment_modern : stock_fragment_legacy;

#ifndef HAVE_OPENGLES2
   if (glsl_core)
   {
      stock_vertex = stock_vertex_core;
      stock_fragment = stock_fragment_core;
   }
#endif

#ifdef HAVE_OPENGLES2
   if (!glsl_shader->modern)
   {
      RARCH_ERR("[GL]: GLES context is used, but shader is not modern. Cannot use it.\n");
      goto error;
   }
#else
   if (glsl_core && !glsl_shader->modern)
   {
      RARCH_ERR("[GL]: GL core context is used, but shader is not core compatible. Cannot use it.\n");
      goto error;
   }
#endif

   if (!(gl_program[0] = compile_program(stock_vertex, stock_fragment, 0)))
   {
      RARCH_ERR("GLSL stock programs failed to compile.\n");
      goto error;
   }

   if (!compile_programs(&gl_program[1]))
      goto error;

   if (!load_luts())
   {
      RARCH_ERR("[GL]: Failed to load LUTs.\n");
      goto error;
   }

   for (unsigned i = 0; i <= glsl_shader->passes; i++)
      find_uniforms(i, gl_program[i], &gl_uniforms[i]);

#ifdef GLSL_DEBUG
   if (!gl_check_error())
      RARCH_WARN("Detected GL error in GLSL.\n");
#endif

   if (glsl_shader->variables)
   {
      struct state_tracker_info info = {0};
      info.wram      = (uint8_t*)pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
      info.info      = glsl_shader->variable;
      info.info_elem = glsl_shader->variables;

#ifdef HAVE_PYTHON
      info.script = glsl_shader->script;
      info.script_class = *glsl_shader->script_class ?
         glsl_shader->script_class : NULL;
#endif

      gl_state_tracker = state_tracker_init(&info);
      if (!gl_state_tracker)
         RARCH_WARN("Failed to init state tracker.\n");
   }
   
   glsl_enable = true;
   gl_program[glsl_shader->passes  + 1] = gl_program[0];
   gl_uniforms[glsl_shader->passes + 1] = gl_uniforms[0];

   if (glsl_shader->modern)
   {
#ifdef HAVE_OPENGLES2 // Avoid compiler warnings.
      gl_program[GL_SHADER_STOCK_BLEND] = compile_program(stock_vertex_modern_blend,
            stock_fragment_modern_blend, GL_SHADER_STOCK_BLEND);
#else
      gl_program[GL_SHADER_STOCK_BLEND] = compile_program(glsl_core ? stock_vertex_core_blend : stock_vertex_modern_blend,
            glsl_core ? stock_fragment_modern_blend : stock_fragment_modern_blend, GL_SHADER_STOCK_BLEND);
#endif

      find_uniforms(0, gl_program[GL_SHADER_STOCK_BLEND], &gl_uniforms[GL_SHADER_STOCK_BLEND]);
   }
   else
   {
      gl_program[GL_SHADER_STOCK_BLEND] = gl_program[0];
      gl_uniforms[GL_SHADER_STOCK_BLEND] = gl_uniforms[0];
   }

   gl_glsl_reset_attrib();

   for (unsigned i = 0; i < GFX_MAX_SHADERS; i++)
   {
      glGenBuffers(1, &glsl_vbo[i].vbo_primary);
      glGenBuffers(1, &glsl_vbo[i].vbo_secondary);
   }

   return true;

error:
   gl_glsl_deinit();
   return false;
}

static void gl_glsl_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info, 
      const struct gl_tex_info *prev_info, 
      const struct gl_tex_info *fbo_info, unsigned fbo_info_cnt)
{
   // We enforce a certain layout for our various texture types in the texunits.
   // - Regular frame (Texture) (always bound).
   // - LUT textures (always bound).
   // - Original texture (always bound if meaningful).
   // - FBO textures (always bound if available).
   // - Previous textures.

   if (!glsl_enable || (gl_program[active_index] == 0))
      return;

   GLfloat buffer[128];
   size_t size = 0;
   struct glsl_attrib attribs[32];
   size_t attribs_size = 0;
   struct glsl_attrib *attr = attribs;

   const struct shader_uniforms *uni = &gl_uniforms[active_index];

   float input_size[2] = {(float)width, (float)height};
   float output_size[2] = {(float)out_width, (float)out_height};
   float texture_size[2] = {(float)tex_width, (float)tex_height};

   if (uni->input_size >= 0)
      glUniform2fv(uni->input_size, 1, input_size);

   if (uni->output_size >= 0)
      glUniform2fv(uni->output_size, 1, output_size);

   if (uni->texture_size >= 0)
      glUniform2fv(uni->texture_size, 1, texture_size);

   if (uni->frame_count >= 0 && active_index)
   {
      unsigned modulo = glsl_shader->pass[active_index - 1].frame_count_mod;
      if (modulo)
         frame_count %= modulo;
      glUniform1i(uni->frame_count, frame_count);
   }

   if (uni->frame_direction >= 0)
      glUniform1i(uni->frame_direction, g_extern.frame_is_reverse ? -1 : 1);

   for (unsigned i = 0; i < glsl_shader->luts; i++)
   {
      if (uni->lut_texture[i] >= 0)
      {
         // Have to rebind as HW render could override this.
         glActiveTexture(GL_TEXTURE0 + i + 1);
         glBindTexture(GL_TEXTURE_2D, gl_teximage[i]);
         glUniform1i(uni->lut_texture[i], i + 1);
      }
   }

   unsigned texunit = glsl_shader->luts + 1;

   // Set original texture unless we're in first pass (pointless).
   if (active_index > 1)
   {
      if (uni->orig.texture >= 0)
      {
         // Bind original texture.
         glActiveTexture(GL_TEXTURE0 + texunit);
         glUniform1i(uni->orig.texture, texunit);
         glBindTexture(GL_TEXTURE_2D, info->tex);
      }

      texunit++;

      if (uni->orig.texture_size >= 0)
         glUniform2fv(uni->orig.texture_size, 1, info->tex_size);

      if (uni->orig.input_size >= 0)
         glUniform2fv(uni->orig.input_size, 1, info->input_size);

      // Pass texture coordinates.
      if (uni->orig.tex_coord >= 0)
      {
         attr->loc = uni->orig.tex_coord;
         attr->size = 2;
         attr->offset = size * sizeof(GLfloat);
         attribs_size++;
         attr++;

         memcpy(buffer + size, info->coord, 8 * sizeof(GLfloat));
         size += 8;
      }

      // Bind new texture in the chain.
      if (fbo_info_cnt > 0)
      {
         glActiveTexture(GL_TEXTURE0 + texunit + fbo_info_cnt - 1);
         glBindTexture(GL_TEXTURE_2D, fbo_info[fbo_info_cnt - 1].tex);
      }

      // Bind FBO textures.
      for (unsigned i = 0; i < fbo_info_cnt; i++)
      {
         if (uni->pass[i].texture)
            glUniform1i(uni->pass[i].texture, texunit);

         texunit++;

         if (uni->pass[i].texture_size >= 0)
            glUniform2fv(uni->pass[i].texture_size, 1, fbo_info[i].tex_size);

         if (uni->pass[i].input_size >= 0)
            glUniform2fv(uni->pass[i].input_size, 1, fbo_info[i].input_size);

         if (uni->pass[i].tex_coord >= 0)
         {
            attr->loc = uni->pass[i].tex_coord;
            attr->size = 2;
            attr->offset = size * sizeof(GLfloat);
            attribs_size++;
            attr++;

            memcpy(buffer + size, fbo_info[i].coord, 8 * sizeof(GLfloat));
            size += 8;
         }
      }
   }
   else
   {
      // First pass, so unbind everything to avoid collitions.
      // Unbind ORIG.
      glActiveTexture(GL_TEXTURE0 + texunit);
      glBindTexture(GL_TEXTURE_2D, 0);

      GLuint base_tex = texunit + 1;
      // Unbind any lurking FBO passes.
      // Rendering to a texture that is bound to a texture unit
      // sounds very shaky ... ;)
      for (unsigned i = 0; i < glsl_shader->passes; i++)
      {
         glActiveTexture(GL_TEXTURE0 + base_tex + i);
         glBindTexture(GL_TEXTURE_2D, 0);
      }
   }

   // Set previous textures. Only bind if they're actually used.
   for (unsigned i = 0; i < PREV_TEXTURES; i++)
   {
      if (uni->prev[i].texture >= 0)
      {
         glActiveTexture(GL_TEXTURE0 + texunit);
         glBindTexture(GL_TEXTURE_2D, prev_info[i].tex);
         glUniform1i(uni->prev[i].texture, texunit++);
      }

      texunit++;

      if (uni->prev[i].texture_size >= 0)
         glUniform2fv(uni->prev[i].texture_size, 1, prev_info[i].tex_size);

      if (uni->prev[i].input_size >= 0)
         glUniform2fv(uni->prev[i].input_size, 1, prev_info[i].input_size);

      // Pass texture coordinates.
      if (uni->prev[i].tex_coord >= 0)
      {
         attr->loc = uni->prev[i].tex_coord;
         attr->size = 2;
         attr->offset = size * sizeof(GLfloat);
         attribs_size++;
         attr++;

         memcpy(buffer + size, prev_info[i].coord, 8 * sizeof(GLfloat));
         size += 8;
      }
   }

   if (size)
   {
      gl_glsl_set_attribs(glsl_vbo[active_index].vbo_secondary,
            glsl_vbo[active_index].buffer_secondary,
            &glsl_vbo[active_index].size_secondary,
            buffer, size, attribs, attribs_size);
   }

   glActiveTexture(GL_TEXTURE0);

   if (gl_state_tracker)
   {
      static struct state_tracker_uniform info[GFX_MAX_VARIABLES];
      static unsigned cnt = 0;

      if (active_index == 1)
         cnt = state_get_uniform(gl_state_tracker, info, GFX_MAX_VARIABLES, frame_count);

      for (unsigned i = 0; i < cnt; i++)
      {
         int location = glGetUniformLocation(gl_program[active_index], info[i].id);
         glUniform1f(location, info[i].value);
      }
   }
}

static bool gl_glsl_set_mvp(const math_matrix *mat)
{
   if (!glsl_enable || !glsl_shader->modern)
      return false;

   int loc = gl_uniforms[active_index].mvp;
   if (loc >= 0)
      glUniformMatrix4fv(loc, 1, GL_FALSE, mat->data);

   return true;
}

static bool gl_glsl_set_coords(const struct gl_coords *coords)
{
   if (!glsl_enable || !glsl_shader->modern)
      return false;

   GLfloat buffer[128];
   size_t size = 0;

   struct glsl_attrib attribs[4];
   size_t attribs_size = 0;
   struct glsl_attrib *attr = attribs;

   const struct shader_uniforms *uni = &gl_uniforms[active_index];
   if (uni->tex_coord >= 0)
   {
      attr->loc    = uni->tex_coord;
      attr->size   = 2;
      attr->offset = size * sizeof(GLfloat);
      attribs_size++;
      attr++;

      memcpy(buffer + size, coords->tex_coord, 8 * sizeof(GLfloat));
      size += 8;
   }

   if (uni->vertex_coord >= 0)
   {
      attr->loc    = uni->vertex_coord;
      attr->size   = 2;
      attr->offset = size * sizeof(GLfloat);
      attribs_size++;
      attr++;

      memcpy(buffer + size, coords->vertex, 8 * sizeof(GLfloat));
      size += 8;
   }

   if (uni->color >= 0)
   {
      attr->loc    = uni->color;
      attr->size   = 4;
      attr->offset = size * sizeof(GLfloat);
      attribs_size++;
      attr++;

      memcpy(buffer + size, coords->color, 16 * sizeof(GLfloat));
      size += 16;
   }

   if (uni->lut_tex_coord >= 0)
   {
      attr->loc    = uni->lut_tex_coord;
      attr->size   = 2;
      attr->offset = size * sizeof(GLfloat);
      attribs_size++;
      attr++;

      memcpy(buffer + size, coords->lut_tex_coord, 8 * sizeof(GLfloat));
      size += 8;
   }

   if (size)
   {
      gl_glsl_set_attribs(glsl_vbo[active_index].vbo_primary,
            glsl_vbo[active_index].buffer_primary,
            &glsl_vbo[active_index].size_primary,
            buffer, size,
            attribs, attribs_size);
   }

   return true;
}

static void gl_glsl_use(unsigned index)
{
   if (glsl_enable)
   {
      gl_glsl_reset_attrib();

      active_index = index;
      glUseProgram(gl_program[index]);
   }
}

static unsigned gl_glsl_num(void)
{
   return glsl_shader->passes;
}

static bool gl_glsl_filter_type(unsigned index, bool *smooth)
{
   if (glsl_enable && index)
   {
      if (glsl_shader->pass[index - 1].filter == RARCH_FILTER_UNSPEC)
         return false;
      *smooth = glsl_shader->pass[index - 1].filter == RARCH_FILTER_LINEAR;
      return true;
   }
   else
      return false;
}

static void gl_glsl_shader_scale(unsigned index, struct gfx_fbo_scale *scale)
{
   if (glsl_enable && index)
      *scale = glsl_shader->pass[index - 1].fbo;
   else
      scale->valid = false;
}

void gl_glsl_set_get_proc_address(gfx_ctx_proc_t (*proc)(const char*))
{
   glsl_get_proc_address = proc;
}

void gl_glsl_set_context_type(bool core_profile, unsigned major, unsigned minor)
{
   glsl_core = core_profile;
   glsl_major = major;
   glsl_minor = minor;
}

const gl_shader_backend_t gl_glsl_backend = {
   gl_glsl_init,
   gl_glsl_deinit,
   gl_glsl_set_params,
   gl_glsl_use,
   gl_glsl_num,
   gl_glsl_filter_type,
   gl_glsl_shader_scale,
   gl_glsl_set_coords,
   gl_glsl_set_mvp,

   RARCH_SHADER_GLSL,
};

