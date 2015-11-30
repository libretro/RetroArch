/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include <string.h>

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>

#include "../../general.h"
#include "shader_glsl.h"
#include "../video_state_tracker.h"
#include "../../dynamic.h"
#include "../../file_ops.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_OPENGL
#include "../common/gl_common.h"
#endif

#ifdef HAVE_OPENGLES2
#define BORDER_FUNC GL_CLAMP_TO_EDGE
#else
#define BORDER_FUNC GL_CLAMP_TO_BORDER
#endif

#define PREV_TEXTURES (GFX_MAX_TEXTURES - 1)

/* Cache the VBO. */
struct cache_vbo
{
   GLuint vbo_primary;
   GLfloat *buffer_primary;
   size_t size_primary;

   GLuint vbo_secondary;
   GLfloat *buffer_secondary;
   size_t size_secondary;
};

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
   struct shader_uniforms_frame feedback;
   struct shader_uniforms_frame pass[GFX_MAX_SHADERS];
   struct shader_uniforms_frame prev[PREV_TEXTURES];
};


static const char *glsl_prefixes[] = {
   "",
   "ruby",
};

/* Need to duplicate these to work around broken stuff on Android.
 * Must enforce alpha = 1.0 or 32-bit games can potentially go black. */
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
   "#ifdef GL_ES\n"
   "precision mediump float;\n"
   "#endif\n"
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

typedef struct glsl_shader_data
{
   struct video_shader *shader;
   struct shader_uniforms gl_uniforms[GFX_MAX_SHADERS];
   struct cache_vbo glsl_vbo[GFX_MAX_SHADERS];
   char glsl_alias_define[1024];
   unsigned glsl_active_index;
   unsigned gl_attrib_index;
   GLuint gl_program[GFX_MAX_SHADERS];
   GLuint gl_teximage[GFX_MAX_TEXTURES];
   GLint gl_attribs[PREV_TEXTURES + 2 + 4 + GFX_MAX_SHADERS];
   state_tracker_t *gl_state_tracker;
} glsl_shader_data_t;

static glsl_shader_data_t *glsl_data;

static bool glsl_core;
static unsigned glsl_major;
static unsigned glsl_minor;

static GLint glsl_get_uniform(GLuint prog, const char *base)
{
   unsigned i;
   GLint loc;
   char buf[64] = {0};

   snprintf(buf, sizeof(buf), "%s%s", glsl_data->shader->prefix, base);
   loc = glGetUniformLocation(prog, buf);
   if (loc >= 0)
      return loc;

   for (i = 0; i < ARRAY_SIZE(glsl_prefixes); i++)
   {
      snprintf(buf, sizeof(buf), "%s%s", glsl_prefixes[i], base);
      loc = glGetUniformLocation(prog, buf);
      if (loc >= 0)
         return loc;
   }

   return -1;
}

static GLint glsl_get_attrib(GLuint prog, const char *base)
{
   unsigned i;
   GLint loc;
   char buf[64] = {0};

   snprintf(buf, sizeof(buf), "%s%s", glsl_data->shader->prefix, base);
   loc = glGetUniformLocation(prog, buf);
   if (loc >= 0)
      return loc;

   for (i = 0; i < ARRAY_SIZE(glsl_prefixes); i++)
   {
      snprintf(buf, sizeof(buf), "%s%s", glsl_prefixes[i], base);
      loc = glGetAttribLocation(prog, buf);
      if (loc >= 0)
         return loc;
   }

   return -1;
}

static void print_shader_log(GLuint obj)
{
   char *info_log = NULL;
   GLint max_len, info_len = 0;

   glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &max_len);

   if (max_len == 0)
      return;

   info_log = (char*)malloc(max_len);
   if (!info_log)
      return;

   glGetShaderInfoLog(obj, max_len, &info_len, info_log);

   if (info_len > 0)
      RARCH_LOG("Shader log: %s\n", info_log);

   free(info_log);
}

static void glsl_print_linker_log(GLuint obj)
{
   char *info_log = NULL;
   GLint max_len, info_len = 0;

   glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &max_len);

   if (max_len == 0)
      return;

   info_log = (char*)malloc(max_len);
   if (!info_log)
      return;

   glGetProgramInfoLog(obj, max_len, &info_len, info_log);

   if (info_len > 0)
      RARCH_LOG("Linker log: %s\n", info_log);

   free(info_log);
}

static bool glsl_compile_shader(GLuint shader, const char *define, const char *program)
{
   GLint status;
   const char *source[4];
   char version[32] = {0};

   if (glsl_core && !strstr(program, "#version"))
   {
      unsigned version_no = 0;
      unsigned gl_ver = glsl_major * 100 + glsl_minor * 10;

      switch (gl_ver)
      {
         case 300:
            version_no = 130;
            break;
         case 310:
            version_no = 140;
            break;
         case 320:
            version_no = 150;
            break;
         default:
            version_no = gl_ver;
            break;
      }

      snprintf(version, sizeof(version), "#version %u\n", version_no);
      RARCH_LOG("[GL]: Using GLSL version %u.\n", version_no);
   }

   source[0] = version;
   source[1] = define;
   source[2] = glsl_data->glsl_alias_define;
   source[3] = program;

   glShaderSource(shader, ARRAY_SIZE(source), source, NULL);
   glCompileShader(shader);

   glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
   print_shader_log(shader);

   return status == GL_TRUE;
}

static bool link_program(GLuint prog)
{
   GLint status;
   
   glLinkProgram(prog);

   glGetProgramiv(prog, GL_LINK_STATUS, &status);
   glsl_print_linker_log(prog);

   if (status != GL_TRUE)
      return false;

   glUseProgram(prog);
   return true;
}

static GLuint glsl_compile_program(const char *vertex, const char *fragment, unsigned i)
{
   GLuint vert = 0, frag = 0, prog = glCreateProgram();
   if (!prog)
      return 0;

   if (vertex)
   {
      RARCH_LOG("Found GLSL vertex shader.\n");
      vert = glCreateShader(GL_VERTEX_SHADER);
      if (!glsl_compile_shader(
               vert, "#define VERTEX\n#define PARAMETER_UNIFORM\n", vertex))
      {
         RARCH_ERR("Failed to compile vertex shader #%u\n", i);
         return 0;
      }

      glAttachShader(prog, vert);
   }

   if (fragment)
   {
      RARCH_LOG("Found GLSL fragment shader.\n");
      frag = glCreateShader(GL_FRAGMENT_SHADER);
      if (!glsl_compile_shader(frag,
               "#define FRAGMENT\n#define PARAMETER_UNIFORM\n", fragment))
      {
         RARCH_ERR("Failed to compile fragment shader #%u\n", i);
         return 0;
      }

      glAttachShader(prog, frag);
   }

   if (vertex || fragment)
   {
      RARCH_LOG("Linking GLSL program.\n");
      if (!link_program(prog))
      {
         RARCH_ERR("Failed to link program #%u.\n", i);
         return 0;
      }

      /* Clean up dead memory. We're not going to relink the program.
       * Detaching first seems to kill some mobile drivers 
       * (according to the intertubes anyways). */
      if (vert)
         glDeleteShader(vert);
      if (frag)
         glDeleteShader(frag);

      glUseProgram(prog);
      glUniform1i(glsl_get_uniform(prog, "Texture"), 0);
      glUseProgram(0);
   }

   return prog;
}

static bool load_source_path(struct video_shader_pass *pass,
      const char *path)
{
   ssize_t len;
   bool ret = read_file(path, (void**)&pass->source.string.vertex, &len);
   if (!ret || len <= 0)
      return false;

   pass->source.string.fragment = strdup(pass->source.string.vertex);
   return pass->source.string.fragment && pass->source.string.vertex;
}

static bool glsl_compile_programs(GLuint *gl_prog)
{
   unsigned i;

   for (i = 0; i < glsl_data->shader->passes; i++)
   {
      const char *vertex           = NULL;
      const char *fragment         = NULL;
      struct video_shader_pass *pass = (struct video_shader_pass*)
         &glsl_data->shader->pass[i];

      /* If we load from GLSLP (CGP),
       * load the file here, and pretend
       * we were really using XML all along.
       */
      if (*pass->source.path && !load_source_path(pass, pass->source.path))
      {
         RARCH_ERR("Failed to load GLSL shader: %s.\n", pass->source.path);
         return false;
      }
      *pass->source.path = '\0';

      vertex   = pass->source.string.vertex;
      fragment = pass->source.string.fragment;

      gl_prog[i] = glsl_compile_program(vertex, fragment, i);

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
   unsigned i;

   /* Add sanity check that we did not overflow. */
   retro_assert(glsl_data->gl_attrib_index <= ARRAY_SIZE(glsl_data->gl_attribs));

   for (i = 0; i < glsl_data->gl_attrib_index; i++)
      glDisableVertexAttribArray(glsl_data->gl_attribs[i]);
   glsl_data->gl_attrib_index = 0;
}

static void gl_glsl_set_vbo(GLfloat **buffer, size_t *buffer_elems,
      const GLfloat *data, size_t elems)
{
   if (elems != *buffer_elems || 
         memcmp(data, *buffer, elems * sizeof(GLfloat)))
   {
      if (elems > *buffer_elems)
      {
         GLfloat *new_buffer = (GLfloat*)
            realloc(*buffer, elems * sizeof(GLfloat));
         retro_assert(new_buffer);
         *buffer = new_buffer;
      }

      memcpy(*buffer, data, elems * sizeof(GLfloat));
      glBufferData(GL_ARRAY_BUFFER, elems * sizeof(GLfloat),
            data, GL_STATIC_DRAW);
      *buffer_elems = elems;
   }
}

static void gl_glsl_set_attribs(
      GLuint vbo,
      GLfloat **buffer, size_t *buffer_elems,
      const GLfloat *data, size_t elems,
      const struct glsl_attrib *attrs, size_t num_attrs)
{
   size_t i;

   glBindBuffer(GL_ARRAY_BUFFER, vbo);

   gl_glsl_set_vbo(buffer, buffer_elems, data, elems);

   for (i = 0; i < num_attrs; i++)
   {
      GLint loc = attrs[i].loc;

      if (glsl_data->gl_attrib_index < ARRAY_SIZE(glsl_data->gl_attribs))
      {
         glEnableVertexAttribArray(loc);
         glVertexAttribPointer(loc, attrs[i].size, GL_FLOAT, GL_FALSE, 0,
               (const GLvoid*)(uintptr_t)attrs[i].offset);
         glsl_data->gl_attribs[glsl_data->gl_attrib_index++] = loc;
      }
      else
         RARCH_WARN("Attrib array buffer was overflown!\n");
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

static void glsl_find_uniforms_frame(GLuint prog,
      struct shader_uniforms_frame *frame, const char *base)
{
   char texture[64]      = {0};
   char texture_size[64] = {0};
   char input_size[64]   = {0};
   char tex_coord[64]    = {0};

   snprintf(texture, sizeof(texture), "%s%s", base, "Texture");
   snprintf(texture_size, sizeof(texture_size), "%s%s", base, "TextureSize");
   snprintf(input_size, sizeof(input_size), "%s%s", base, "InputSize");
   snprintf(tex_coord, sizeof(tex_coord), "%s%s", base, "TexCoord");

   if (frame->texture < 0)
      frame->texture = glsl_get_uniform(prog, texture);
   if (frame->texture_size < 0)
      frame->texture_size = glsl_get_uniform(prog, texture_size);
   if (frame->input_size < 0)
      frame->input_size = glsl_get_uniform(prog, input_size);
   if (frame->tex_coord < 0)
      frame->tex_coord = glsl_get_attrib(prog, tex_coord);
}

static void glsl_find_uniforms(
      unsigned pass, GLuint prog,
      struct shader_uniforms *uni)
{
   unsigned i;
   char frame_base[64] = {0};

   glUseProgram(prog);

   uni->mvp             = glsl_get_uniform(prog, "MVPMatrix");
   uni->tex_coord       = glsl_get_attrib(prog, "TexCoord");
   uni->vertex_coord    = glsl_get_attrib(prog, "VertexCoord");
   uni->color           = glsl_get_attrib(prog, "Color");
   uni->lut_tex_coord   = glsl_get_attrib(prog, "LUTTexCoord");

   uni->input_size      = glsl_get_uniform(prog, "InputSize");
   uni->output_size     = glsl_get_uniform(prog, "OutputSize");
   uni->texture_size    = glsl_get_uniform(prog, "TextureSize");

   uni->frame_count     = glsl_get_uniform(prog, "FrameCount");
   uni->frame_direction = glsl_get_uniform(prog, "FrameDirection");

   for (i = 0; i < glsl_data->shader->luts; i++)
      uni->lut_texture[i] = glGetUniformLocation(prog, glsl_data->shader->lut[i].id);

   clear_uniforms_frame(&uni->orig);
   glsl_find_uniforms_frame(prog, &uni->orig, "Orig");
   clear_uniforms_frame(&uni->feedback);
   glsl_find_uniforms_frame(prog, &uni->feedback, "Feedback");

   if (pass > 1)
   {
      snprintf(frame_base, sizeof(frame_base), "PassPrev%u", pass);
      glsl_find_uniforms_frame(prog, &uni->orig, frame_base);
   }

   for (i = 0; i + 1 < pass; i++)
   {
      snprintf(frame_base, sizeof(frame_base), "Pass%u", i + 1);
      clear_uniforms_frame(&uni->pass[i]);
      glsl_find_uniforms_frame(prog, &uni->pass[i], frame_base);
      snprintf(frame_base, sizeof(frame_base), "PassPrev%u", pass - (i + 1));
      glsl_find_uniforms_frame(prog, &uni->pass[i], frame_base);

      if (*glsl_data->shader->pass[i].alias)
         glsl_find_uniforms_frame(prog, &uni->pass[i], glsl_data->shader->pass[i].alias);
   }

   clear_uniforms_frame(&uni->prev[0]);
   glsl_find_uniforms_frame(prog, &uni->prev[0], "Prev");
   for (i = 1; i < PREV_TEXTURES; i++)
   {
      snprintf(frame_base, sizeof(frame_base), "Prev%u", i);
      clear_uniforms_frame(&uni->prev[i]);
      glsl_find_uniforms_frame(prog, &uni->prev[i], frame_base);
   }

   glUseProgram(0);
}

static void gl_glsl_deinit_shader(void)
{
   unsigned i;

   if (!glsl_data || !glsl_data->shader)
      return;

   for (i = 0; i < glsl_data->shader->passes; i++)
   {
      free(glsl_data->shader->pass[i].source.string.vertex);
      free(glsl_data->shader->pass[i].source.string.fragment);
   }

   free(glsl_data->shader->script);
   free(glsl_data->shader);
   glsl_data->shader = NULL;
}

static void gl_glsl_destroy_resources(void)
{
   unsigned i;

   if (!glsl_data)
      return;

   glUseProgram(0);
   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      if (glsl_data->gl_program[i] == 0 || (i && glsl_data->gl_program[i] == glsl_data->gl_program[0]))
         continue;

      glDeleteProgram(glsl_data->gl_program[i]);
   }

   if (glsl_data->shader && glsl_data->shader->luts)
      glDeleteTextures(glsl_data->shader->luts, glsl_data->gl_teximage);

   memset(glsl_data->gl_program, 0, sizeof(glsl_data->gl_program));
   memset(glsl_data->gl_uniforms, 0, sizeof(glsl_data->gl_uniforms));
   glsl_data->glsl_active_index = 0;

   gl_glsl_deinit_shader();

   if (glsl_data->gl_state_tracker)
      state_tracker_free(glsl_data->gl_state_tracker);
   glsl_data->gl_state_tracker = NULL;

   gl_glsl_reset_attrib();

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      if (glsl_data->glsl_vbo[i].vbo_primary)
         glDeleteBuffers(1, &glsl_data->glsl_vbo[i].vbo_primary);
      if (glsl_data->glsl_vbo[i].vbo_secondary)
         glDeleteBuffers(1, &glsl_data->glsl_vbo[i].vbo_secondary);

      free(glsl_data->glsl_vbo[i].buffer_primary);
      free(glsl_data->glsl_vbo[i].buffer_secondary);
   }
   memset(&glsl_data->glsl_vbo, 0, sizeof(glsl_data->glsl_vbo));
}

static void gl_glsl_deinit(void)
{
   if (!glsl_data)
      return;

   gl_glsl_destroy_resources();

   if (glsl_data)
      free(glsl_data);
   glsl_data = NULL;
}

static bool gl_glsl_init(void *data, const char *path)
{
   unsigned i;
   config_file_t *conf        = NULL;
   const char *stock_vertex   = NULL;
   const char *stock_fragment = NULL;

   glsl_data = (glsl_shader_data_t*)calloc(1, sizeof(glsl_shader_data_t));

   if (!glsl_data)
      return false;

#ifndef HAVE_OPENGLES2
   RARCH_LOG("Checking GLSL shader support ...\n");
   bool shader_support = glCreateProgram && glUseProgram && glCreateShader
      && glDeleteShader && glShaderSource && glCompileShader && glAttachShader
      && glDetachShader && glLinkProgram && glGetUniformLocation
      && glUniform1i && glUniform1f && glUniform2fv && glUniform4fv 
      && glUniformMatrix4fv
      && glGetShaderiv && glGetShaderInfoLog && glGetProgramiv 
      && glGetProgramInfoLog 
      && glDeleteProgram && glGetAttachedShaders
      && glGetAttribLocation && glEnableVertexAttribArray 
      && glDisableVertexAttribArray
      && glVertexAttribPointer
      && glGenBuffers && glBufferData && glDeleteBuffers && glBindBuffer;

   if (!shader_support)
   {
      RARCH_ERR("GLSL shaders aren't supported by your OpenGL driver.\n");
      free(glsl_data);
      glsl_data = NULL;
      return false;
   }
#endif

   glsl_data->shader = (struct video_shader*)calloc(1, sizeof(*glsl_data->shader));
   if (!glsl_data->shader)
   {
      free(glsl_data);
      glsl_data = NULL;
      return false;
   }

   if (path)
   {
      bool ret;
      const char *path_ext = path_get_extension(path);

      if (!strcmp(path_ext, "glsl"))
      {
         strlcpy(glsl_data->shader->pass[0].source.path, path,
               sizeof(glsl_data->shader->pass[0].source.path));
         glsl_data->shader->passes = 1;
         glsl_data->shader->modern = true;
         ret = true;
      }
      else if (!strcmp(path_ext, "glslp"))
      {
         conf = config_file_new(path);
         if (conf)
         {
            ret = video_shader_read_conf_cgp(conf, glsl_data->shader);
            glsl_data->shader->modern = true;
         }
         else
            ret = false;
      }
      else
         ret = false;

      if (!ret)
      {
         RARCH_ERR("[GL]: Failed to parse GLSL shader.\n");
         free(glsl_data->shader);
         free(glsl_data);
         glsl_data = NULL;
         return false;
      }
   }
   else
   {
      RARCH_WARN("[GL]: Stock GLSL shaders will be used.\n");
      glsl_data->shader->passes = 1;
      glsl_data->shader->pass[0].source.string.vertex   = 
         strdup(glsl_core ? stock_vertex_core : stock_vertex_modern);
      glsl_data->shader->pass[0].source.string.fragment = 
         strdup(glsl_core ? stock_fragment_core : stock_fragment_modern);
      glsl_data->shader->modern = true;
   }

   video_shader_resolve_relative(glsl_data->shader, path);
   video_shader_resolve_parameters(conf, glsl_data->shader);

   if (conf)
   {
      config_file_free(conf);
      conf = NULL;
   }

   stock_vertex = (glsl_data->shader->modern) ?
      stock_vertex_modern : stock_vertex_legacy;
   stock_fragment = (glsl_data->shader->modern) ?
      stock_fragment_modern : stock_fragment_legacy;

   if (glsl_core)
   {
      stock_vertex = stock_vertex_core;
      stock_fragment = stock_fragment_core;
   }

#ifdef HAVE_OPENGLES2
   if (!glsl_data->shader->modern)
   {
      RARCH_ERR("[GL]: GLES context is used, but shader is not modern. Cannot use it.\n");
      goto error;
   }
#else
   if (glsl_core && !glsl_data->shader->modern)
   {
      RARCH_ERR("[GL]: GL core context is used, but shader is not core compatible. Cannot use it.\n");
      goto error;
   }
#endif

   /* Find all aliases we use in our GLSLP and add #defines for them so
    * that a shader can choose a fallback if we are not using a preset. */
   *glsl_data->glsl_alias_define = '\0';
   for (i = 0; i < glsl_data->shader->passes; i++)
   {
      if (*glsl_data->shader->pass[i].alias)
      {
         char define[128] = {0};

         snprintf(define, sizeof(define), "#define %s_ALIAS\n",
               glsl_data->shader->pass[i].alias);
         strlcat(glsl_data->glsl_alias_define, define, sizeof(glsl_data->glsl_alias_define));
      }
   }

   if (!(glsl_data->gl_program[0] = glsl_compile_program(stock_vertex, stock_fragment, 0)))
   {
      RARCH_ERR("GLSL stock programs failed to compile.\n");
      goto error;
   }

   if (!glsl_compile_programs(&glsl_data->gl_program[1]))
      goto error;

   if (!gl_load_luts(glsl_data->shader, glsl_data->gl_teximage))
   {
      RARCH_ERR("[GL]: Failed to load LUTs.\n");
      goto error;
   }

   for (i = 0; i <= glsl_data->shader->passes; i++)
      glsl_find_uniforms(i, glsl_data->gl_program[i], &glsl_data->gl_uniforms[i]);

#ifdef GLSL_DEBUG
   if (!gl_check_error())
      RARCH_WARN("Detected GL error in GLSL.\n");
#endif

   if (glsl_data->shader->variables)
   {
      struct state_tracker_info info = {0};

      info.wram      = (uint8_t*)core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
      info.info      = glsl_data->shader->variable;
      info.info_elem = glsl_data->shader->variables;

#ifdef HAVE_PYTHON
      info.script = glsl_data->shader->script;
      info.script_class = *glsl_data->shader->script_class ?
         glsl_data->shader->script_class : NULL;
#endif

      glsl_data->gl_state_tracker = state_tracker_init(&info);
      if (!glsl_data->gl_state_tracker)
         RARCH_WARN("Failed to init state tracker.\n");
   }
   
   glsl_data->gl_program[glsl_data->shader->passes  + 1] = glsl_data->gl_program[0];
   glsl_data->gl_uniforms[glsl_data->shader->passes + 1] = glsl_data->gl_uniforms[0];

   if (glsl_data->shader->modern)
   {
      glsl_data->gl_program[GL_SHADER_STOCK_BLEND] = glsl_compile_program(
            glsl_core ? 
            stock_vertex_core_blend : stock_vertex_modern_blend,
            glsl_core ? 
            stock_fragment_core_blend : stock_fragment_modern_blend,
            GL_SHADER_STOCK_BLEND);
      glsl_find_uniforms(0, glsl_data->gl_program[GL_SHADER_STOCK_BLEND],
            &glsl_data->gl_uniforms[GL_SHADER_STOCK_BLEND]);
   }
   else
   {
      glsl_data->gl_program [GL_SHADER_STOCK_BLEND] = glsl_data->gl_program[0];
      glsl_data->gl_uniforms[GL_SHADER_STOCK_BLEND] = glsl_data->gl_uniforms[0];
   }

   gl_glsl_reset_attrib();

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      glGenBuffers(1, &glsl_data->glsl_vbo[i].vbo_primary);
      glGenBuffers(1, &glsl_data->glsl_vbo[i].vbo_secondary);
   }

   return true;

error:
   gl_glsl_destroy_resources();

   if (glsl_data)
      free(glsl_data);
   glsl_data = NULL;

   return false;
}

static void gl_glsl_set_params(void *data, unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const void *_info, 
      const void *_prev_info, 
      const void *_feedback_info,
      const void *_fbo_info, unsigned fbo_info_cnt)
{
   GLfloat buffer[512];
   struct glsl_attrib attribs[32];
   float input_size[2], output_size[2], texture_size[2];
   unsigned i, texunit = 1;
   const struct shader_uniforms *uni = NULL;
   size_t size = 0, attribs_size = 0;
   const struct gfx_tex_info *info = (const struct gfx_tex_info*)_info;
   const struct gfx_tex_info *prev_info = (const struct gfx_tex_info*)_prev_info;
   const struct gfx_tex_info *feedback_info = (const struct gfx_tex_info*)_feedback_info;
   const struct gfx_tex_info *fbo_info = (const struct gfx_tex_info*)_fbo_info;
   struct glsl_attrib *attr = (struct glsl_attrib*)attribs;

   if (!glsl_data)
      return;

   uni = (const struct shader_uniforms*)&glsl_data->gl_uniforms[glsl_data->glsl_active_index];

   (void)data;

   if (glsl_data->gl_program[glsl_data->glsl_active_index] == 0)
      return;

   input_size [0]  = (float)width;
   input_size [1]  = (float)height;
   output_size[0]  = (float)out_width;
   output_size[1]  = (float)out_height;
   texture_size[0] = (float)tex_width;
   texture_size[1] = (float)tex_height;

   if (uni->input_size >= 0)
      glUniform2fv(uni->input_size, 1, input_size);

   if (uni->output_size >= 0)
      glUniform2fv(uni->output_size, 1, output_size);

   if (uni->texture_size >= 0)
      glUniform2fv(uni->texture_size, 1, texture_size);

   if (uni->frame_count >= 0 && glsl_data->glsl_active_index)
   {
      unsigned modulo = glsl_data->shader->pass[glsl_data->glsl_active_index - 1].frame_count_mod;

      if (modulo)
         frame_count %= modulo;
      glUniform1i(uni->frame_count, frame_count);
   }

   if (uni->frame_direction >= 0)
      glUniform1i(uni->frame_direction, state_manager_frame_is_reversed() ? -1 : 1);


   for (i = 0; i < glsl_data->shader->luts; i++)
   {
      if (uni->lut_texture[i] < 0)
         continue;

      /* Have to rebind as HW render could override this. */
      glActiveTexture(GL_TEXTURE0 + texunit);
      glBindTexture(GL_TEXTURE_2D, glsl_data->gl_teximage[i]);
      glUniform1i(uni->lut_texture[i], texunit);
      texunit++;
   }

   if (glsl_data->glsl_active_index)
   {
      /* Set original texture. */
      if (uni->orig.texture >= 0)
      {
         /* Bind original texture. */
         glActiveTexture(GL_TEXTURE0 + texunit);
         glUniform1i(uni->orig.texture, texunit);
         glBindTexture(GL_TEXTURE_2D, info->tex);
         texunit++;
      }

      if (uni->orig.texture_size >= 0)
         glUniform2fv(uni->orig.texture_size, 1, info->tex_size);

      if (uni->orig.input_size >= 0)
         glUniform2fv(uni->orig.input_size, 1, info->input_size);

      /* Pass texture coordinates. */
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

      /* Set feedback texture. */
      if (uni->feedback.texture >= 0)
      {
         /* Bind original texture. */
         glActiveTexture(GL_TEXTURE0 + texunit);
         glUniform1i(uni->feedback.texture, texunit);
         glBindTexture(GL_TEXTURE_2D, feedback_info->tex);
         texunit++;
      }

      if (uni->feedback.texture_size >= 0)
         glUniform2fv(uni->feedback.texture_size, 1, feedback_info->tex_size);

      if (uni->feedback.input_size >= 0)
         glUniform2fv(uni->feedback.input_size, 1, feedback_info->input_size);

      /* Pass texture coordinates. */
      if (uni->feedback.tex_coord >= 0)
      {
         attr->loc = uni->feedback.tex_coord;
         attr->size = 2;
         attr->offset = size * sizeof(GLfloat);
         attribs_size++;
         attr++;

         memcpy(buffer + size, feedback_info->coord, 8 * sizeof(GLfloat));
         size += 8;
      }

      /* Bind FBO textures. */
      for (i = 0; i < fbo_info_cnt; i++)
      {
         if (uni->pass[i].texture)
         {
            glActiveTexture(GL_TEXTURE0 + texunit);
            glBindTexture(GL_TEXTURE_2D, fbo_info[i].tex);
            glUniform1i(uni->pass[i].texture, texunit);
            texunit++;
         }

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

   /* Set previous textures. Only bind if they're actually used. */
   for (i = 0; i < PREV_TEXTURES; i++)
   {
      if (uni->prev[i].texture >= 0)
      {
         glActiveTexture(GL_TEXTURE0 + texunit);
         glBindTexture(GL_TEXTURE_2D, prev_info[i].tex);
         glUniform1i(uni->prev[i].texture, texunit);
         texunit++;
      }

      if (uni->prev[i].texture_size >= 0)
         glUniform2fv(uni->prev[i].texture_size, 1, prev_info[i].tex_size);

      if (uni->prev[i].input_size >= 0)
         glUniform2fv(uni->prev[i].input_size, 1, prev_info[i].input_size);

      /* Pass texture coordinates. */
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
      gl_glsl_set_attribs(glsl_data->glsl_vbo[glsl_data->glsl_active_index].vbo_secondary,
            &glsl_data->glsl_vbo[glsl_data->glsl_active_index].buffer_secondary,
            &glsl_data->glsl_vbo[glsl_data->glsl_active_index].size_secondary,
            buffer, size, attribs, attribs_size);
   }

   glActiveTexture(GL_TEXTURE0);

   /* #pragma parameters. */
   for (i = 0; i < glsl_data->shader->num_parameters; i++)
   {
      int location = glGetUniformLocation(
            glsl_data->gl_program[glsl_data->glsl_active_index],
            glsl_data->shader->parameters[i].id);
      glUniform1f(location, glsl_data->shader->parameters[i].current);
   }

   /* Set state parameters. */
   if (glsl_data->gl_state_tracker)
   {
      static struct state_tracker_uniform state_info[GFX_MAX_VARIABLES];
      static unsigned cnt = 0;

      if (glsl_data->glsl_active_index == 1)
         cnt = state_tracker_get_uniform(glsl_data->gl_state_tracker, state_info,
               GFX_MAX_VARIABLES, frame_count);

      for (i = 0; i < cnt; i++)
      {
         int location = glGetUniformLocation(
               glsl_data->gl_program[glsl_data->glsl_active_index],
               state_info[i].id);
         glUniform1f(location, state_info[i].value);
      }
   }
}

static bool gl_glsl_set_mvp(void *data, const math_matrix_4x4 *mat)
{
   int loc;

   if (!glsl_data || !glsl_data->shader->modern)
   {
      gl_ff_matrix(mat);
      return false;
   }

   loc = glsl_data->gl_uniforms[glsl_data->glsl_active_index].mvp;
   if (loc >= 0)
      glUniformMatrix4fv(loc, 1, GL_FALSE, mat->data);

   return true;
}

static bool gl_glsl_set_coords(const void *data)
{
   /* Avoid hitting malloc on every single regular quad draw. */
   GLfloat short_buffer[4 * (2 + 2 + 4 + 2)];
   GLfloat *buffer;
   struct glsl_attrib attribs[4];
   size_t attribs_size = 0, size = 0;
   struct glsl_attrib *attr = NULL;
   const struct shader_uniforms *uni = NULL;
   const struct gfx_coords *coords = (const struct gfx_coords*)data;

   if (!glsl_data || !glsl_data->shader->modern || !coords)
   {
      gl_ff_vertex(coords);
      return false;
   }

   buffer = short_buffer;
   if (coords->vertices > 4)
      buffer = (GLfloat*)calloc(coords->vertices * 
            (2 + 2 + 4 + 2), sizeof(*buffer));

   if (!buffer)
   {
      gl_ff_vertex(coords);
      return false;
   }

   attr = attribs;
   uni  = &glsl_data->gl_uniforms[glsl_data->glsl_active_index];

   if (uni->tex_coord >= 0)
   {
      attr->loc    = uni->tex_coord;
      attr->size   = 2;
      attr->offset = size * sizeof(GLfloat);
      attribs_size++;
      attr++;

      memcpy(buffer + size, coords->tex_coord, 
            2 * coords->vertices * sizeof(GLfloat));
      size += 2 * coords->vertices;
   }

   if (uni->vertex_coord >= 0)
   {
      attr->loc    = uni->vertex_coord;
      attr->size   = 2;
      attr->offset = size * sizeof(GLfloat);
      attribs_size++;
      attr++;

      memcpy(buffer + size, coords->vertex, 
            2 * coords->vertices * sizeof(GLfloat));
      size += 2 * coords->vertices;
   }

   if (uni->color >= 0)
   {
      attr->loc    = uni->color;
      attr->size   = 4;
      attr->offset = size * sizeof(GLfloat);
      attribs_size++;
      attr++;

      memcpy(buffer + size, coords->color,
            4 * coords->vertices * sizeof(GLfloat));
      size += 4 * coords->vertices;
   }

   if (uni->lut_tex_coord >= 0)
   {
      attr->loc    = uni->lut_tex_coord;
      attr->size   = 2;
      attr->offset = size * sizeof(GLfloat);
      attribs_size++;
      attr++;

      memcpy(buffer + size, coords->lut_tex_coord,
            2 * coords->vertices * sizeof(GLfloat));
      size += 2 * coords->vertices;
   }

   if (size)
   {
      gl_glsl_set_attribs(
            glsl_data->glsl_vbo[glsl_data->glsl_active_index].vbo_primary,
            &glsl_data->glsl_vbo[glsl_data->glsl_active_index].buffer_primary,
            &glsl_data->glsl_vbo[glsl_data->glsl_active_index].size_primary,
            buffer, size,
            attribs, attribs_size);
   }

   if (buffer != short_buffer)
      free(buffer);

   return true;
}

static void gl_glsl_use(void *data, unsigned idx)
{
   if (!glsl_data)
      return;

   gl_glsl_reset_attrib();

   glsl_data->glsl_active_index = idx;
   glUseProgram(glsl_data->gl_program[idx]);
}

static unsigned gl_glsl_num(void)
{
   if (glsl_data && glsl_data->shader)
      return glsl_data->shader->passes;
   return 0;
}

static bool gl_glsl_filter_type(unsigned idx, bool *smooth)
{
   if (glsl_data && idx 
         && (glsl_data->shader->pass[idx - 1].filter != RARCH_FILTER_UNSPEC)
      )
   {
      *smooth = (glsl_data->shader->pass[idx - 1].filter == RARCH_FILTER_LINEAR);
      return true;
   }
   return false;
}

static enum gfx_wrap_type gl_glsl_wrap_type(unsigned idx)
{
   if (glsl_data && idx)
      return glsl_data->shader->pass[idx - 1].wrap;
   return RARCH_WRAP_BORDER;
}

static void gl_glsl_shader_scale(unsigned idx, struct gfx_fbo_scale *scale)
{
   if (glsl_data && idx)
      *scale = glsl_data->shader->pass[idx - 1].fbo;
   else
      scale->valid = false;
}

static unsigned gl_glsl_get_prev_textures(void)
{
   unsigned i, j;
   unsigned max_prev = 0;

   if (!glsl_data)
      return 0;

   for (i = 1; i <= glsl_data->shader->passes; i++)
      for (j = 0; j < PREV_TEXTURES; j++)
         if (glsl_data->gl_uniforms[i].prev[j].texture >= 0)
            max_prev = max(j + 1, max_prev);

   return max_prev;
}

static bool gl_glsl_mipmap_input(unsigned idx)
{
   if (glsl_data && idx)
      return glsl_data->shader->pass[idx - 1].mipmap;
   return false;
}

static bool gl_glsl_get_feedback_pass(unsigned *index)
{
   if (!glsl_data || glsl_data->shader->feedback_pass < 0)
      return false;

   *index = glsl_data->shader->feedback_pass;
   return true;
}

static struct video_shader *gl_glsl_get_current_shader(void)
{
   if (!glsl_data)
      return NULL;
   return glsl_data->shader;
}

void gl_glsl_set_get_proc_address(gfx_ctx_proc_t (*proc)(const char*))
{
   glsl_get_proc_address = proc;
}

void gl_glsl_set_context_type(bool core_profile,
      unsigned major, unsigned minor)
{
   glsl_core = core_profile;
   glsl_major = major;
   glsl_minor = minor;
}

const shader_backend_t gl_glsl_backend = {
   gl_glsl_init,
   gl_glsl_deinit,
   gl_glsl_set_params,
   gl_glsl_use,
   gl_glsl_num,
   gl_glsl_filter_type,
   gl_glsl_wrap_type,
   gl_glsl_shader_scale,
   gl_glsl_set_coords,
   gl_glsl_set_mvp,
   gl_glsl_get_prev_textures,
   gl_glsl_get_feedback_pass,
   gl_glsl_mipmap_input,
   gl_glsl_get_current_shader,

   RARCH_SHADER_GLSL,
   "glsl"
};

