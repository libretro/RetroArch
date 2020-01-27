/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <retro_assert.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include <gfx/gl_capabilities.h>
#include "../common/gl_common.h"
#endif

#include "shader_glsl.h"
#include "../../managers/state_manager.h"
#include "../../core.h"
#include "../../verbosity.h"

#define PREV_TEXTURES (GFX_MAX_TEXTURES - 1)

/* Cache the VBO. */
struct cache_vbo
{
   GLuint vbo_primary;
   GLuint vbo_secondary;
   size_t size_primary;
   size_t size_secondary;
   GLfloat *buffer_primary;
   GLfloat *buffer_secondary;
};

struct shader_program_glsl_data
{
   GLuint vprg;
   GLuint fprg;

   GLuint id;
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
   int frame_direction;

   int lut_texture[GFX_MAX_TEXTURES];
   unsigned frame_count_mod;

   struct shader_uniforms_frame orig;
   struct shader_uniforms_frame feedback;
   struct shader_uniforms_frame pass[GFX_MAX_SHADERS];
   struct shader_uniforms_frame prev[PREV_TEXTURES];
};

static const char *glsl_prefixes[] = {
   "",
   "ruby",
};

#include "../drivers/gl_shaders/modern_opaque.glsl.vert.h"
#include "../drivers/gl_shaders/modern_opaque.glsl.frag.h"
#include "../drivers/gl_shaders/core_opaque.glsl.vert.h"
#include "../drivers/gl_shaders/core_opaque.glsl.frag.h"
#include "../drivers/gl_shaders/legacy_opaque.glsl.vert.h"
#include "../drivers/gl_shaders/legacy_opaque.glsl.frag.h"
#include "../drivers/gl_shaders/modern_alpha_blend.glsl.vert.h"
#include "../drivers/gl_shaders/modern_alpha_blend.glsl.frag.h"
#include "../drivers/gl_shaders/core_alpha_blend.glsl.vert.h"
#include "../drivers/gl_shaders/core_alpha_blend.glsl.frag.h"

#ifdef HAVE_SHADERPIPELINE
#include "../drivers/gl_shaders/core_pipeline_snow.glsl.frag.h"
#include "../drivers/gl_shaders/core_pipeline_snow_simple.glsl.frag.h"
#include "../drivers/gl_shaders/core_pipeline_xmb_ribbon.glsl.frag.h"
#include "../drivers/gl_shaders/core_pipeline_xmb_ribbon_simple.glsl.frag.h"
#include "../drivers/gl_shaders/core_pipeline_bokeh.glsl.frag.h"
#include "../drivers/gl_shaders/core_pipeline_snowflake.glsl.frag.h"
#include "../drivers/gl_shaders/legacy_pipeline_xmb_ribbon_simple.glsl.vert.h"
#include "../drivers/gl_shaders/modern_pipeline_xmb_ribbon_simple.glsl.vert.h"
#include "../drivers/gl_shaders/pipeline_xmb_ribbon_simple.glsl.frag.h"
#include "../drivers/gl_shaders/pipeline_snow.glsl.frag.h"
#include "../drivers/gl_shaders/pipeline_snow.glsl.vert.h"
#include "../drivers/gl_shaders/pipeline_snow_core.glsl.vert.h"
#include "../drivers/gl_shaders/pipeline_snow_simple.glsl.frag.h"
#include "../drivers/gl_shaders/legacy_pipeline_snow.glsl.vert.h"
#include "../drivers/gl_shaders/legacy_pipeline_xmb_ribbon.glsl.vert.h"
#include "../drivers/gl_shaders/modern_pipeline_xmb_ribbon.glsl.vert.h"
#include "../drivers/gl_shaders/pipeline_xmb_ribbon.glsl.frag.h"
#include "../drivers/gl_shaders/pipeline_bokeh.glsl.frag.h"
#include "../drivers/gl_shaders/pipeline_snowflake.glsl.frag.h"
#endif

typedef struct glsl_shader_data
{
   char alias_define[1024];
   GLint attribs_elems[32 * PREV_TEXTURES + 2 + 4 + GFX_MAX_SHADERS];
   unsigned attribs_index;
   unsigned active_idx;
   unsigned current_idx;
   GLuint lut_textures[GFX_MAX_TEXTURES];
   float  current_mat_data[GFX_MAX_SHADERS];
   float* current_mat_data_pointer[GFX_MAX_SHADERS];
   struct shader_uniforms uniforms[GFX_MAX_SHADERS];
   struct cache_vbo vbo[GFX_MAX_SHADERS];
   struct shader_program_glsl_data prg[GFX_MAX_SHADERS];
   struct video_shader *shader;
} glsl_shader_data_t;

static bool glsl_core;
static unsigned glsl_major;
static unsigned glsl_minor;

static GLint gl_glsl_get_uniform(glsl_shader_data_t *glsl,
      GLuint prog, const char *base)
{
   unsigned i;
   GLint loc;
   char buf[80];

   buf[0] = '\0';

   strlcpy(buf, glsl->shader->prefix, sizeof(buf));
   strlcat(buf, base, sizeof(buf));
   loc = glGetUniformLocation(prog, buf);
   if (loc >= 0)
      return loc;

   for (i = 0; i < ARRAY_SIZE(glsl_prefixes); i++)
   {
      buf[0] = '\0';
      strlcpy(buf, glsl_prefixes[i], sizeof(buf));
      strlcat(buf, base, sizeof(buf));
      loc = glGetUniformLocation(prog, buf);
      if (loc >= 0)
         return loc;
   }

   return -1;
}

static GLint gl_glsl_get_attrib(glsl_shader_data_t *glsl,
      GLuint prog, const char *base)
{
   unsigned i;
   GLint loc;
   char buf[80];

   buf[0] = '\0';

   strlcpy(buf, glsl->shader->prefix, sizeof(buf));
   strlcat(buf, base, sizeof(buf));
   loc = glGetUniformLocation(prog, buf);
   if (loc >= 0)
      return loc;

   for (i = 0; i < ARRAY_SIZE(glsl_prefixes); i++)
   {
      strlcpy(buf, glsl_prefixes[i], sizeof(buf));
      strlcat(buf, base, sizeof(buf));
      loc = glGetAttribLocation(prog, buf);
      if (loc >= 0)
         return loc;
   }

   return -1;
}

static void gl_glsl_print_shader_log(GLuint obj)
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

static void gl_glsl_print_linker_log(GLuint obj)
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

static bool gl_glsl_compile_shader(glsl_shader_data_t *glsl,
      GLuint shader,
      const char *define, const char *program)
{
   GLint status;
   const char *source[4];
   char version[32];
   const char *existing_version = strstr(program, "#version");

   version[0]                   = '\0';

   if (existing_version)
   {
      const char* version_extra = "";
      unsigned version_no = (unsigned)strtoul(existing_version + 8, (char**)&program, 10);
#ifdef HAVE_OPENGLES
      if (version_no < 130)
         version_no = 100;
      else
      {
         version_extra = " es";
         version_no = 300;
      }
#endif
      snprintf(version, sizeof(version), "#version %u%s\n", version_no, version_extra);
      RARCH_LOG("[GLSL]: Using GLSL version %u%s.\n", version_no, version_extra);
   }
   else if (glsl_core)
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
      RARCH_LOG("[GLSL]: Using GLSL version %u.\n", version_no);
   }

   source[0] = version;
   source[1] = define;
   source[2] = glsl->alias_define;
   source[3] = program;

   glShaderSource(shader, ARRAY_SIZE(source), source, NULL);
   glCompileShader(shader);

   glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
   gl_glsl_print_shader_log(shader);

   return status == GL_TRUE;
}

static bool gl_glsl_link_program(GLuint prog)
{
   GLint status;

   glLinkProgram(prog);

   glGetProgramiv(prog, GL_LINK_STATUS, &status);
   gl_glsl_print_linker_log(prog);

   if (status != GL_TRUE)
      return false;

   glUseProgram(prog);
   return true;
}

static bool gl_glsl_compile_program(
      void *data,
      unsigned idx,
      void *program_data,
      struct shader_program_info *program_info)
{
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;
   struct shader_program_glsl_data *program = (struct shader_program_glsl_data*)program_data;
   GLuint prog = glCreateProgram();

   if (!program)
      program = &glsl->prg[idx];

   if (!prog)
      goto error;

   if (program_info->vertex)
   {
      RARCH_LOG("[GLSL]: Found GLSL vertex shader.\n");
      program->vprg = glCreateShader(GL_VERTEX_SHADER);

      if (!gl_glsl_compile_shader(
               glsl,
               program->vprg,
               "#define VERTEX\n#define PARAMETER_UNIFORM\n", program_info->vertex))
      {
         RARCH_ERR("Failed to compile vertex shader #%u\n", idx);
         goto error;
      }

      glAttachShader(prog, program->vprg);
   }

   if (program_info->fragment)
   {
      RARCH_LOG("[GLSL]: Found GLSL fragment shader.\n");
      program->fprg = glCreateShader(GL_FRAGMENT_SHADER);
      if (!gl_glsl_compile_shader(glsl, program->fprg,
               "#define FRAGMENT\n#define PARAMETER_UNIFORM\n", program_info->fragment))
      {
         RARCH_ERR("Failed to compile fragment shader #%u\n", idx);
         goto error;
      }

      glAttachShader(prog, program->fprg);
   }

   if (program_info->vertex || program_info->fragment)
   {
      RARCH_LOG("[GLSL]: Linking GLSL program.\n");
      if (!gl_glsl_link_program(prog))
         goto error;

      /* Clean up dead memory. We're not going to relink the program.
       * Detaching first seems to kill some mobile drivers
       * (according to the intertubes anyways). */
      if (program->vprg)
         glDeleteShader(program->vprg);
      if (program->fprg)
         glDeleteShader(program->fprg);
      program->vprg = 0;
      program->fprg = 0;

      glUseProgram(prog);
      glUniform1i(gl_glsl_get_uniform(glsl, prog, "Texture"), 0);
      glUseProgram(0);
   }

   program->id = prog;

   return true;

error:
   RARCH_ERR("Failed to link program #%u.\n", idx);
   program->id = 0;
   return false;
}

static void gl_glsl_strip_parameter_pragmas(char *source, const char *str)
{
   /* #pragma parameter lines tend to have " characters in them,
    * which is not legal GLSL. */
   char *s = strstr(source, str);

   while (s)
   {
      /* #pragmas have to be on a single line,
       * so we can just replace the entire line with spaces. */
      while (*s != '\0' && *s != '\n')
         *s++ = ' ';
      s = strstr(s, str);
   }
}

static bool gl_glsl_load_source_path(struct video_shader_pass *pass,
      const char *path)
{
   int64_t len    = 0;
   int64_t nitems = pass ? filestream_read_file(path,
         (void**)&pass->source.string.vertex, &len) : 0;

   if (nitems <= 0 || len <= 0)
      return false;

   gl_glsl_strip_parameter_pragmas(pass->source.string.vertex, "#pragma parameter");
   pass->source.string.fragment = strdup(pass->source.string.vertex);
   return pass->source.string.fragment && pass->source.string.vertex;
}

static bool gl_glsl_compile_programs(
      glsl_shader_data_t *glsl, struct shader_program_glsl_data *program)
{
   unsigned i;

   for (i = 0; i < glsl->shader->passes; i++)
   {
      struct shader_program_info shader_prog_info;
      const char *vertex           = NULL;
      const char *fragment         = NULL;
      struct video_shader_pass *pass = (struct video_shader_pass*)
         &glsl->shader->pass[i];

      if (!pass)
         continue;

      /* If we load from GLSLP (preset),
       * load the file here.
       */
      if (     !string_is_empty(pass->source.path)
            && !gl_glsl_load_source_path(pass, pass->source.path))
      {
         RARCH_ERR("Failed to load GLSL shader: %s.\n",
               pass->source.path);
         return false;
      }

      vertex                    = pass->source.string.vertex;
      fragment                  = pass->source.string.fragment;

      shader_prog_info.vertex   = vertex;
      shader_prog_info.fragment = fragment;
      shader_prog_info.is_file  = false;

      if (!gl_glsl_compile_program(glsl, i,
            &program[i],
            &shader_prog_info))
      {
         RARCH_ERR("Failed to create GL program #%u.\n", i);
         return false;
      }
   }

   return true;
}

static void gl_glsl_reset_attrib(glsl_shader_data_t *glsl)
{
   unsigned i;

   /* Add sanity check that we did not overflow. */
   retro_assert(glsl->attribs_index <= ARRAY_SIZE(glsl->attribs_elems));

   for (i = 0; i < glsl->attribs_index; i++)
      glDisableVertexAttribArray(glsl->attribs_elems[i]);
   glsl->attribs_index = 0;
}

static void gl_glsl_set_vbo(GLfloat **buffer, size_t *buffer_elems,
      const GLfloat *data, size_t elems)
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

static INLINE void gl_glsl_set_attribs(glsl_shader_data_t *glsl,
      GLuint vbo,
      GLfloat **buffer, size_t *buffer_elems,
      const GLfloat *data, size_t elems,
      const struct glsl_attrib *attrs, size_t num_attrs)
{
   size_t i;

   glBindBuffer(GL_ARRAY_BUFFER, vbo);

   if (elems != *buffer_elems ||
         memcmp(data, *buffer, elems * sizeof(GLfloat)))
      gl_glsl_set_vbo(buffer, buffer_elems, data, elems);

   for (i = 0; i < num_attrs; i++)
   {
      if (glsl->attribs_index < ARRAY_SIZE(glsl->attribs_elems))
      {
         GLint loc = attrs[i].loc;

         glEnableVertexAttribArray(loc);
         glVertexAttribPointer(loc, attrs[i].size, GL_FLOAT, GL_FALSE, 0,
               (const GLvoid*)(uintptr_t)attrs[i].offset);
         glsl->attribs_elems[glsl->attribs_index++] = loc;
      }
      else
         RARCH_WARN("Attrib array buffer was overflown!\n");
   }

   glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void gl_glsl_clear_uniforms_frame(struct shader_uniforms_frame *frame)
{
   frame->texture      = -1;
   frame->texture_size = -1;
   frame->input_size   = -1;
   frame->tex_coord    = -1;
}

static void gl_glsl_find_uniforms_frame(glsl_shader_data_t *glsl,
      GLuint prog,
      struct shader_uniforms_frame *frame, const char *base)
{
   char texture[64];
   char texture_size[64];
   char input_size[64];
   char tex_coord[64];

   texture[0] = texture_size[0] = input_size[0] = tex_coord[0] = '\0';

   strlcpy(texture,      base,          sizeof(texture));
   strlcat(texture,      "Texture",     sizeof(texture));
   strlcpy(texture_size, base,          sizeof(texture_size));
   strlcat(texture_size, "TextureSize", sizeof(texture_size));
   strlcpy(input_size,   base,          sizeof(input_size));
   strlcat(input_size,   "InputSize",   sizeof(input_size));
   strlcpy(tex_coord,    base,          sizeof(tex_coord));
   strlcat(tex_coord,    "TexCoord",    sizeof(tex_coord));

   if (frame->texture < 0)
      frame->texture = gl_glsl_get_uniform(glsl, prog, texture);
   if (frame->texture_size < 0)
      frame->texture_size = gl_glsl_get_uniform(glsl, prog, texture_size);
   if (frame->input_size < 0)
      frame->input_size = gl_glsl_get_uniform(glsl, prog, input_size);
   if (frame->tex_coord < 0)
      frame->tex_coord = gl_glsl_get_attrib(glsl, prog, tex_coord);
}

static void gl_glsl_find_uniforms(glsl_shader_data_t *glsl,
      unsigned pass, GLuint prog,
      struct shader_uniforms *uni)
{
   unsigned i;
   char frame_base[64];

   frame_base[0] = '\0';

   glUseProgram(prog);

   uni->mvp             = gl_glsl_get_uniform(glsl, prog, "MVPMatrix");
   uni->tex_coord       = gl_glsl_get_attrib(glsl, prog, "TexCoord");
   uni->vertex_coord    = gl_glsl_get_attrib(glsl, prog, "VertexCoord");
   uni->color           = gl_glsl_get_attrib(glsl, prog, "Color");
   uni->lut_tex_coord   = gl_glsl_get_attrib(glsl, prog, "LUTTexCoord");

   uni->input_size      = gl_glsl_get_uniform(glsl, prog, "InputSize");
   uni->output_size     = gl_glsl_get_uniform(glsl, prog, "OutputSize");
   uni->texture_size    = gl_glsl_get_uniform(glsl, prog, "TextureSize");

   uni->frame_count     = gl_glsl_get_uniform(glsl, prog, "FrameCount");
   uni->frame_direction = gl_glsl_get_uniform(glsl, prog, "FrameDirection");

   for (i = 0; i < glsl->shader->luts; i++)
      uni->lut_texture[i] = glGetUniformLocation(prog, glsl->shader->lut[i].id);

   gl_glsl_clear_uniforms_frame(&uni->orig);
   gl_glsl_find_uniforms_frame(glsl, prog, &uni->orig, "Orig");
   gl_glsl_clear_uniforms_frame(&uni->feedback);
   gl_glsl_find_uniforms_frame(glsl, prog, &uni->feedback, "Feedback");

   if (pass > 1)
   {
      snprintf(frame_base, sizeof(frame_base), "PassPrev%u", pass);
      gl_glsl_find_uniforms_frame(glsl, prog, &uni->orig, frame_base);
   }

   for (i = 0; i + 1 < pass; i++)
   {
      snprintf(frame_base, sizeof(frame_base), "Pass%u", i + 1);
      gl_glsl_clear_uniforms_frame(&uni->pass[i]);
      gl_glsl_find_uniforms_frame(glsl, prog, &uni->pass[i], frame_base);
      snprintf(frame_base, sizeof(frame_base), "PassPrev%u", pass - (i + 1));
      gl_glsl_find_uniforms_frame(glsl, prog, &uni->pass[i], frame_base);

      if (*glsl->shader->pass[i].alias)
         gl_glsl_find_uniforms_frame(glsl, prog, &uni->pass[i], glsl->shader->pass[i].alias);
   }

   gl_glsl_clear_uniforms_frame(&uni->prev[0]);
   gl_glsl_find_uniforms_frame(glsl, prog, &uni->prev[0], "Prev");
   for (i = 1; i < PREV_TEXTURES; i++)
   {
      snprintf(frame_base, sizeof(frame_base), "Prev%u", i);
      gl_glsl_clear_uniforms_frame(&uni->prev[i]);
      gl_glsl_find_uniforms_frame(glsl, prog, &uni->prev[i], frame_base);
   }

   glUseProgram(0);
}

static void gl_glsl_deinit_shader(glsl_shader_data_t *glsl)
{
   unsigned i;

   if (!glsl || !glsl->shader)
      return;

   for (i = 0; i < glsl->shader->passes; i++)
   {
      free(glsl->shader->pass[i].source.string.vertex);
      free(glsl->shader->pass[i].source.string.fragment);
   }

   free(glsl->shader);
   glsl->shader = NULL;
}

static void gl_glsl_destroy_resources(glsl_shader_data_t *glsl)
{
   unsigned i;

   if (!glsl)
      return;

   glsl->current_idx = 0;

   glUseProgram(0);

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      if (glsl->prg[i].id == 0 || (i && glsl->prg[i].id == glsl->prg[0].id))
         continue;
      if (!glIsProgram(glsl->prg[i].id))
         continue;

      glDeleteProgram(glsl->prg[i].id);
      glsl->prg[i].id = 0;
   }

   if (glsl->shader && glsl->shader->luts)
      glDeleteTextures(glsl->shader->luts, glsl->lut_textures);

   memset(glsl->prg, 0, sizeof(glsl->prg));
   memset(glsl->uniforms, 0, sizeof(glsl->uniforms));
   glsl->active_idx = 0;

   gl_glsl_deinit_shader(glsl);

   gl_glsl_reset_attrib(glsl);

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      if (glsl->vbo[i].vbo_primary)
         glDeleteBuffers(1, &glsl->vbo[i].vbo_primary);
      if (glsl->vbo[i].vbo_secondary)
         glDeleteBuffers(1, &glsl->vbo[i].vbo_secondary);

      free(glsl->vbo[i].buffer_primary);
      free(glsl->vbo[i].buffer_secondary);
   }
   memset(&glsl->vbo, 0, sizeof(glsl->vbo));
}

static void gl_glsl_deinit(void *data)
{
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;

   if (!glsl)
      return;

   gl_glsl_destroy_resources(glsl);

   free(glsl);
}

static void gl_glsl_init_menu_shaders(void *data)
{
#ifdef HAVE_SHADERPIPELINE
   struct shader_program_info shader_prog_info;
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;

   if (!glsl)
      return;

#ifdef HAVE_OPENGLES
   if (gl_query_extension("GL_OES_standard_derivatives"))
   {
      shader_prog_info.vertex = glsl_core ? stock_vertex_xmb_ribbon_modern : stock_vertex_xmb_ribbon_legacy;
      shader_prog_info.fragment = glsl_core ? core_stock_fragment_xmb : stock_fragment_xmb;
   }
   else
   {
      shader_prog_info.vertex = stock_vertex_xmb_ribbon_simple_legacy;
      shader_prog_info.fragment = stock_fragment_xmb_ribbon_simple;
   }
#else
   shader_prog_info.vertex = glsl_core ? stock_vertex_xmb_ribbon_modern : stock_vertex_xmb_ribbon_legacy;
   shader_prog_info.fragment = glsl_core ? core_stock_fragment_xmb : stock_fragment_xmb;
#endif
   shader_prog_info.is_file = false;

   RARCH_LOG("[GLSL]: Compiling ribbon shader..\n");
   gl_glsl_compile_program(
         glsl,
         VIDEO_SHADER_MENU,
         &glsl->prg[VIDEO_SHADER_MENU],
         &shader_prog_info);
   gl_glsl_find_uniforms(glsl, 0, glsl->prg[VIDEO_SHADER_MENU].id,
         &glsl->uniforms[VIDEO_SHADER_MENU]);

   shader_prog_info.vertex = glsl_core ? stock_vertex_xmb_simple_modern : stock_vertex_xmb_ribbon_simple_legacy;
   shader_prog_info.fragment = glsl_core ? stock_fragment_xmb_ribbon_simple_core : stock_fragment_xmb_ribbon_simple;

   RARCH_LOG("[GLSL]: Compiling simple ribbon shader..\n");
   gl_glsl_compile_program(
         glsl,
         VIDEO_SHADER_MENU_2,
         &glsl->prg[VIDEO_SHADER_MENU_2],
         &shader_prog_info);
   gl_glsl_find_uniforms(glsl, 0, glsl->prg[VIDEO_SHADER_MENU_2].id,
         &glsl->uniforms[VIDEO_SHADER_MENU_2]);

#if defined(HAVE_OPENGLES)
   shader_prog_info.vertex   = stock_vertex_xmb_snow;
   shader_prog_info.fragment = stock_fragment_xmb_simple_snow;
#else
   shader_prog_info.vertex   = glsl_core ? stock_vertex_xmb_snow_core : stock_vertex_xmb_snow_legacy;
   shader_prog_info.fragment = glsl_core ? stock_fragment_xmb_simple_snow_core : stock_fragment_xmb_simple_snow;
#endif

   RARCH_LOG("[GLSL]: Compiling snow shader..\n");
   gl_glsl_compile_program(
         glsl,
         VIDEO_SHADER_MENU_3,
         &glsl->prg[VIDEO_SHADER_MENU_3],
         &shader_prog_info);
   gl_glsl_find_uniforms(glsl, 0, glsl->prg[VIDEO_SHADER_MENU_3].id,
         &glsl->uniforms[VIDEO_SHADER_MENU_3]);

#if defined(HAVE_OPENGLES)
   shader_prog_info.vertex   = stock_vertex_xmb_snow;
   shader_prog_info.fragment = stock_fragment_xmb_snow;
#else
   shader_prog_info.vertex   = glsl_core ? stock_vertex_xmb_snow_core : stock_vertex_xmb_snow_legacy;
   shader_prog_info.fragment = glsl_core ? stock_fragment_xmb_snow_core : stock_fragment_xmb_snow;
#endif

   RARCH_LOG("[GLSL]: Compiling modern snow shader..\n");
   gl_glsl_compile_program(
         glsl,
         VIDEO_SHADER_MENU_4,
         &glsl->prg[VIDEO_SHADER_MENU_4],
         &shader_prog_info);
   gl_glsl_find_uniforms(glsl, 0, glsl->prg[VIDEO_SHADER_MENU_4].id,
         &glsl->uniforms[VIDEO_SHADER_MENU_4]);

#if defined(HAVE_OPENGLES)
   shader_prog_info.vertex   = stock_vertex_xmb_snow;
   shader_prog_info.fragment = stock_fragment_xmb_bokeh;
#else
   shader_prog_info.vertex   = glsl_core ? stock_vertex_xmb_snow_core  : stock_vertex_xmb_snow_legacy;
   shader_prog_info.fragment = glsl_core ? stock_fragment_xmb_bokeh_core : stock_fragment_xmb_bokeh;
#endif

   RARCH_LOG("[GLSL]: Compiling bokeh shader..\n");
   gl_glsl_compile_program(
         glsl,
         VIDEO_SHADER_MENU_5,
         &glsl->prg[VIDEO_SHADER_MENU_5],
         &shader_prog_info);
   gl_glsl_find_uniforms(glsl, 0, glsl->prg[VIDEO_SHADER_MENU_5].id,
         &glsl->uniforms[VIDEO_SHADER_MENU_5]);

#if defined(HAVE_OPENGLES)
   shader_prog_info.vertex   = stock_vertex_xmb_snow;
   shader_prog_info.fragment = stock_fragment_xmb_snowflake;
#else
   shader_prog_info.vertex   = glsl_core ? stock_vertex_xmb_snow_core : stock_vertex_xmb_snow_legacy;
   shader_prog_info.fragment = glsl_core ? stock_fragment_xmb_snowflake_core : stock_fragment_xmb_snowflake;
#endif

   RARCH_LOG("[GLSL]: Compiling snowflake shader..\n");
   gl_glsl_compile_program(
         glsl,
         VIDEO_SHADER_MENU_6,
         &glsl->prg[VIDEO_SHADER_MENU_6],
         &shader_prog_info);
   gl_glsl_find_uniforms(glsl, 0, glsl->prg[VIDEO_SHADER_MENU_6].id,
         &glsl->uniforms[VIDEO_SHADER_MENU_6]);
#endif
}

static void *gl_glsl_init(void *data, const char *path)
{
   unsigned i;
   struct shader_program_info shader_prog_info;
   bool shader_support        = false;
#ifdef GLSL_DEBUG
   char *error_string         = NULL;
#endif
   config_file_t *conf        = NULL;
   const char *stock_vertex   = NULL;
   const char *stock_fragment = NULL;
   glsl_shader_data_t   *glsl = (glsl_shader_data_t*)
      calloc(1, sizeof(glsl_shader_data_t));

   if (!glsl)
      return NULL;

   (void)shader_support;

#ifndef HAVE_OPENGLES
   RARCH_LOG("[GLSL]: Checking GLSL shader support ...\n");
   shader_support = glCreateProgram && glUseProgram && glCreateShader
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
      goto error;
   }
#endif

   glsl->shader = (struct video_shader*)calloc(1, sizeof(*glsl->shader));
   if (!glsl->shader)
      goto error;

   {
      bool is_preset;
      enum rarch_shader_type type =
         video_shader_get_type_from_ext(path_get_extension(path), &is_preset);

      if (!string_is_empty(path) && type != RARCH_SHADER_GLSL)
      {
         RARCH_ERR("[GL]: Invalid shader type, falling back to stock.\n");
         path = NULL;
      }

      if (!string_is_empty(path))
      {
         bool ret = false;

         if (is_preset)
         {
            conf = video_shader_read_preset(path);
            if (conf)
            {
               ret = video_shader_read_conf_preset(conf, glsl->shader);
               glsl->shader->modern = true;
            }
         }
         else
         {
            strlcpy(glsl->shader->pass[0].source.path, path,
                  sizeof(glsl->shader->pass[0].source.path));
            glsl->shader->passes = 1;
            glsl->shader->modern = true;
            ret = true;
         }

         if (!ret)
         {
            RARCH_ERR("[GL]: Failed to parse GLSL shader.\n");
            goto error;
         }
      }
      else
      {
         RARCH_WARN("[GL]: Stock GLSL shaders will be used.\n");
         glsl->shader->passes = 1;
         glsl->shader->pass[0].source.string.vertex   =
            strdup(glsl_core ? stock_vertex_core : stock_vertex_modern);
         glsl->shader->pass[0].source.string.fragment =
            strdup(glsl_core ? stock_fragment_core : stock_fragment_modern);
         glsl->shader->modern = true;
      }
   }

   video_shader_resolve_parameters(conf, glsl->shader);

   if (conf)
   {
      config_file_free(conf);
      conf = NULL;
   }

   stock_vertex = (glsl->shader->modern) ?
      stock_vertex_modern : stock_vertex_legacy;
   stock_fragment = (glsl->shader->modern) ?
      stock_fragment_modern : stock_fragment_legacy;

   if (glsl_core)
   {
      stock_vertex = stock_vertex_core;
      stock_fragment = stock_fragment_core;
   }

#ifdef HAVE_OPENGLES
   if (!glsl->shader->modern)
   {
      RARCH_ERR("[GL]: GLES context is used, but shader is not modern. Cannot use it.\n");
      goto error;
   }
#else
   if (glsl_core && !glsl->shader->modern)
   {
      RARCH_ERR("[GL]: GL core context is used, but shader is not core compatible. Cannot use it.\n");
      goto error;
   }
#endif

   /* Find all aliases we use in our GLSLP and add #defines for them so
    * that a shader can choose a fallback if we are not using a preset. */
   *glsl->alias_define = '\0';
   for (i = 0; i < glsl->shader->passes; i++)
   {
      if (*glsl->shader->pass[i].alias)
      {
         char define[128];

         define[0] = '\0';

         snprintf(define, sizeof(define), "#define %s_ALIAS\n",
               glsl->shader->pass[i].alias);
         strlcat(glsl->alias_define, define, sizeof(glsl->alias_define));
      }
   }

   shader_prog_info.vertex   = stock_vertex;
   shader_prog_info.fragment = stock_fragment;
   shader_prog_info.is_file  = false;

   if (!gl_glsl_compile_program(glsl, 0, &glsl->prg[0], &shader_prog_info))
   {
      RARCH_ERR("GLSL stock programs failed to compile.\n");
      goto error;
   }

   if (!gl_glsl_compile_programs(glsl, &glsl->prg[1]))
      goto error;

   if (!gl_load_luts(glsl->shader, glsl->lut_textures))
   {
      RARCH_ERR("[GL]: Failed to load LUTs.\n");
      goto error;
   }

   for (i = 0; i <= glsl->shader->passes; i++)
      gl_glsl_find_uniforms(glsl, i, glsl->prg[i].id, &glsl->uniforms[i]);

#ifdef GLSL_DEBUG
   if (!gl_check_error(&error_string))
   {
      RARCH_ERR("%s\n", error_string);
      free(error_string);
      RARCH_WARN("Detected GL error in GLSL.\n");
   }
#endif

   glsl->prg[glsl->shader->passes  + 1]     = glsl->prg[0];
   glsl->uniforms[glsl->shader->passes + 1] = glsl->uniforms[0];

   if (glsl->shader->modern)
   {
      shader_prog_info.vertex   =
            glsl_core ?
            stock_vertex_core_blend : stock_vertex_modern_blend;
      shader_prog_info.fragment =
            glsl_core ?
            stock_fragment_core_blend : stock_fragment_modern_blend;
      shader_prog_info.is_file  = false;

      gl_glsl_compile_program(
            glsl,
            VIDEO_SHADER_STOCK_BLEND,
            &glsl->prg[VIDEO_SHADER_STOCK_BLEND],
            &shader_prog_info
            );

      gl_glsl_find_uniforms(glsl, 0, glsl->prg[VIDEO_SHADER_STOCK_BLEND].id,
            &glsl->uniforms[VIDEO_SHADER_STOCK_BLEND]);
   }
   else
   {
      glsl->prg[VIDEO_SHADER_STOCK_BLEND] = glsl->prg[0];
      glsl->uniforms[VIDEO_SHADER_STOCK_BLEND] = glsl->uniforms[0];
   }

   gl_glsl_reset_attrib(glsl);

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      glGenBuffers(1, &glsl->vbo[i].vbo_primary);
      glGenBuffers(1, &glsl->vbo[i].vbo_secondary);
   }

   return glsl;

error:
   gl_glsl_destroy_resources(glsl);

   if (conf)
      config_file_free(conf);
   if (glsl)
      free(glsl);

   return NULL;
}

static void gl_glsl_set_uniform_parameter(
      void *data,
      struct uniform_info *param,
      void *uniform_data)
{
   GLint            location = 0;
   glsl_shader_data_t  *glsl = (glsl_shader_data_t*)data;

   if (!glsl || !param)
      return;

   if (param->lookup.enable)
      location = glGetUniformLocation(glsl->prg[param->lookup.idx].id, param->lookup.ident);
   else
      location = param->location;

   switch (param->type)
   {
      case UNIFORM_1F:
         glUniform1f(location, param->result.f.v0);
         break;
      case UNIFORM_2F:
         glUniform2f(location, param->result.f.v0,
               param->result.f.v1);
         break;
      case UNIFORM_3F:
         glUniform3f(location, param->result.f.v0,
               param->result.f.v1, param->result.f.v2);
         break;
      case UNIFORM_4F:
         glUniform4f(location, param->result.f.v0,
               param->result.f.v1, param->result.f.v2,
               param->result.f.v3);
         break;
      case UNIFORM_1FV:
         glUniform1fv(location, 1, param->result.floatv);
         break;
      case UNIFORM_2FV:
         glUniform2fv(location, 1, param->result.floatv);
         break;
      case UNIFORM_3FV:
         glUniform3fv(location, 1, param->result.floatv);
         break;
      case UNIFORM_4FV:
         glUniform4fv(location, 1, param->result.floatv);
         break;
      case UNIFORM_1I:
         glUniform1i(location, (GLint)param->result.integer.v0);
         break;
   }
}

static void gl_glsl_set_params(void *dat, void *shader_data)
{
   unsigned i;
   GLfloat buffer[512];
   struct glsl_attrib attribs[32];
   float input_size[2], output_size[2], texture_size[2];
   video_shader_ctx_params_t          *params = (video_shader_ctx_params_t*)dat;
   unsigned width                             = params->width;
   unsigned height                            = params->height;
   unsigned tex_width                         = params->tex_width;
   unsigned tex_height                        = params->tex_height;
   unsigned out_width                         = params->out_width;
   unsigned out_height                        = params->out_height;
   unsigned frame_count                       = params->frame_counter;
   const void *_info                          = params->info;
   const void *_prev_info                     = params->prev_info;
   const void *_feedback_info                 = params->feedback_info;
   const void *_fbo_info                      = params->fbo_info;
   unsigned fbo_info_cnt                      = params->fbo_info_cnt;
   unsigned                           texunit = 1;
   const struct          shader_uniforms *uni = NULL;
   size_t                                size = 0;
   size_t                        attribs_size = 0;
   const struct video_tex_info          *info = (const struct video_tex_info*)_info;
   const struct video_tex_info     *prev_info = (const struct video_tex_info*)_prev_info;
   const struct video_tex_info *feedback_info = (const struct video_tex_info*)_feedback_info;
   const struct video_tex_info      *fbo_info = (const struct video_tex_info*)_fbo_info;
   struct glsl_attrib                   *attr = (struct glsl_attrib*)attribs;
   glsl_shader_data_t                   *glsl = (glsl_shader_data_t*)shader_data;

   if (!glsl)
      return;

   uni = (const struct shader_uniforms*)&glsl->uniforms[glsl->active_idx];

   if (glsl->prg[glsl->active_idx].id == 0)
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

   if (uni->frame_count >= 0 && glsl->active_idx)
   {
      unsigned modulo = glsl->shader->pass[glsl->active_idx - 1].frame_count_mod;

      if (modulo)
         frame_count %= modulo;

      glUniform1i(uni->frame_count, frame_count);
   }

   if (uni->frame_direction >= 0)
      glUniform1i(uni->frame_direction, state_manager_frame_is_reversed() ? -1 : 1);

   /* Set lookup textures. */
   for (i = 0; i < glsl->shader->luts; i++)
   {
      if (uni->lut_texture[i] < 0)
         continue;

      /* Have to rebind as HW render could override this. */
      glActiveTexture(GL_TEXTURE0 + texunit);
      glBindTexture(GL_TEXTURE_2D, glsl->lut_textures[i]);
      glUniform1i(uni->lut_texture[i], texunit);
      texunit++;
   }

   if (glsl->active_idx)
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
         attr->loc    = uni->orig.tex_coord;
         attr->size   = 2;
         attr->offset = (GLsizei)(size * sizeof(GLfloat));
         attribs_size++;
         attr++;

         buffer[size ]       = info->coord[0];
         buffer[size + 1]    = info->coord[1];
         buffer[size + 2]    = info->coord[2];
         buffer[size + 3]    = info->coord[3];
         buffer[size + 4]    = info->coord[4];
         buffer[size + 5]    = info->coord[5];
         buffer[size + 6]    = info->coord[6];
         buffer[size + 7]    = info->coord[7];
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
         attr->loc    = uni->feedback.tex_coord;
         attr->size   = 2;
         attr->offset = (GLsizei)(size * sizeof(GLfloat));
         attribs_size++;
         attr++;

         buffer[size  ]      = feedback_info->coord[0];
         buffer[size + 1]    = feedback_info->coord[1];
         buffer[size + 2]    = feedback_info->coord[2];
         buffer[size + 3]    = feedback_info->coord[3];
         buffer[size + 4]    = feedback_info->coord[4];
         buffer[size + 5]    = feedback_info->coord[5];
         buffer[size + 6]    = feedback_info->coord[6];
         buffer[size + 7]    = feedback_info->coord[7];
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
            attr->loc    = uni->pass[i].tex_coord;
            attr->size   = 2;
            attr->offset = (GLsizei)(size * sizeof(GLfloat));
            attribs_size++;
            attr++;

            buffer[size  ]      = fbo_info[i].coord[0];
            buffer[size + 1]    = fbo_info[i].coord[1];
            buffer[size + 2]    = fbo_info[i].coord[2];
            buffer[size + 3]    = fbo_info[i].coord[3];
            buffer[size + 4]    = fbo_info[i].coord[4];
            buffer[size + 5]    = fbo_info[i].coord[5];
            buffer[size + 6]    = fbo_info[i].coord[6];
            buffer[size + 7]    = fbo_info[i].coord[7];
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
         attr->loc    = uni->prev[i].tex_coord;
         attr->size   = 2;
         attr->offset = (GLsizei)(size * sizeof(GLfloat));
         attribs_size++;
         attr++;

         buffer[size  ]      = prev_info[i].coord[0];
         buffer[size + 1]    = prev_info[i].coord[1];
         buffer[size + 2]    = prev_info[i].coord[2];
         buffer[size + 3]    = prev_info[i].coord[3];
         buffer[size + 4]    = prev_info[i].coord[4];
         buffer[size + 5]    = prev_info[i].coord[5];
         buffer[size + 6]    = prev_info[i].coord[6];
         buffer[size + 7]    = prev_info[i].coord[7];
         size += 8;
      }
   }

   if (size)
      gl_glsl_set_attribs(glsl, glsl->vbo[glsl->active_idx].vbo_secondary,
            &glsl->vbo[glsl->active_idx].buffer_secondary,
            &glsl->vbo[glsl->active_idx].size_secondary,
            buffer, size, attribs, attribs_size);

   glActiveTexture(GL_TEXTURE0);

   /* #pragma parameters. */
   for (i = 0; i < glsl->shader->num_parameters; i++)
   {

      int location = glGetUniformLocation(
            glsl->prg[glsl->active_idx].id,
            glsl->shader->parameters[i].id);
      glUniform1f(location, glsl->shader->parameters[i].current);
   }
}

static bool gl_glsl_set_mvp(void *shader_data, const void *mat_data)
{
   int loc;
   glsl_shader_data_t *glsl   = (glsl_shader_data_t*)shader_data;

   if (!glsl || !glsl->shader->modern)
      return false;

   loc = glsl->uniforms[glsl->active_idx].mvp;
   if (loc >= 0)
   {
      const math_matrix_4x4 *mat = (const math_matrix_4x4*)mat_data;

      if (  (glsl->current_idx != glsl->active_idx) ||
            (mat->data  != glsl->current_mat_data_pointer[glsl->active_idx]) ||
            (*mat->data != glsl->current_mat_data[glsl->active_idx]))
      {
         glUniformMatrix4fv(loc, 1, GL_FALSE, mat->data);
         glsl->current_idx                                = glsl->active_idx;
         glsl->current_mat_data_pointer[glsl->active_idx] = (float*)mat->data;
         glsl->current_mat_data[glsl->active_idx]         = *mat->data;
      }
   }

   return true;
}

#define gl_glsl_set_coord_array(attribs, coord1, coord2, coords, size, multiplier) \
   unsigned y; \
   attribs[attribs_size].loc            = (GLint)coord1; \
   attribs[attribs_size].size           = (GLsizei)multiplier; \
   attribs[attribs_size].offset         = (GLsizei)(size * sizeof(GLfloat)); \
   for (y = 0; y < (multiplier * coords->vertices); y++) \
      buffer[y + size]  = coord2[y]; \
   size                += multiplier * coords->vertices; \

static bool gl_glsl_set_coords(void *shader_data,
      const struct video_coords *coords)
{
   GLfloat short_buffer[4 * (2 + 2 + 4 + 2)];
   struct glsl_attrib attribs[4];
   size_t               attribs_size = 0;
   size_t                       size = 0;
   GLfloat *buffer                   = short_buffer;
   glsl_shader_data_t          *glsl = (glsl_shader_data_t*)shader_data;
   const struct shader_uniforms *uni = glsl
      ? &glsl->uniforms[glsl->active_idx] : NULL;

   if (!glsl || !glsl->shader->modern || !coords)
   {
      if (coords)
         return false;
      return true;
   }

   if (coords->vertices > 4)
   {
      /* Avoid hitting malloc on every single regular quad draw. */

      size_t elems  = 0;
      elems        += (uni->color >= 0)         * 4;
      elems        += (uni->tex_coord >= 0)     * 2;
      elems        += (uni->vertex_coord >= 0)  * 2;
      elems        += (uni->lut_tex_coord >= 0) * 2;

      elems        *= coords->vertices * sizeof(GLfloat);

      buffer        = (GLfloat*)malloc(elems);
   }

   if (!buffer)
      return false;

   if (uni->tex_coord >= 0)
   {
      gl_glsl_set_coord_array(attribs, uni->tex_coord,
            coords->tex_coord, coords, size, 2);
      attribs_size++;
   }

   if (uni->vertex_coord >= 0)
   {
      gl_glsl_set_coord_array(attribs, uni->vertex_coord,
            coords->vertex, coords, size, 2);
      attribs_size++;
   }

   if (uni->color >= 0)
   {
      gl_glsl_set_coord_array(attribs, uni->color,
            coords->color, coords, size, 4);
      attribs_size++;
   }

   if (uni->lut_tex_coord >= 0)
   {
      gl_glsl_set_coord_array(attribs, uni->lut_tex_coord,
            coords->lut_tex_coord, coords, size, 2);
      attribs_size++;
   }

   if (size)
      gl_glsl_set_attribs(glsl,
            glsl->vbo[glsl->active_idx].vbo_primary,
            &glsl->vbo[glsl->active_idx].buffer_primary,
            &glsl->vbo[glsl->active_idx].size_primary,
            buffer, size,
            attribs, attribs_size);

   if (buffer != short_buffer)
      free(buffer);

   return true;
}

static void gl_glsl_use(void *data, void *shader_data, unsigned idx, bool set_active)
{
   GLuint id;

   if (set_active)
   {
      glsl_shader_data_t *glsl = (glsl_shader_data_t*)shader_data;
      if (!glsl)
         return;

      gl_glsl_reset_attrib(glsl);
      glsl->active_idx        = idx;
      id                      = glsl->prg[idx].id;
   }
   else
      id = (GLuint)idx;

   glUseProgram(id);
}

static unsigned gl_glsl_num(void *data)
{
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;
   if (glsl && glsl->shader)
      return glsl->shader->passes;
   return 0;
}

static bool gl_glsl_filter_type(void *data, unsigned idx, bool *smooth)
{
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;
   if (glsl && idx
         && (glsl->shader->pass[idx - 1].filter != RARCH_FILTER_UNSPEC)
      )
   {
      *smooth = (glsl->shader->pass[idx - 1].filter == RARCH_FILTER_LINEAR);
      return true;
   }
   return false;
}

static enum gfx_wrap_type gl_glsl_wrap_type(void *data, unsigned idx)
{
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;
   if (glsl && idx)
      return glsl->shader->pass[idx - 1].wrap;
   return RARCH_WRAP_BORDER;
}

static void gl_glsl_shader_scale(void *data, unsigned idx, struct gfx_fbo_scale *scale)
{
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;
   if (glsl && idx)
      *scale = glsl->shader->pass[idx - 1].fbo;
   else
      scale->valid = false;
}

static unsigned gl_glsl_get_prev_textures(void *data)
{
   unsigned i, j;
   unsigned max_prev = 0;
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;

   if (!glsl)
      return 0;

   for (i = 1; i <= glsl->shader->passes; i++)
      for (j = 0; j < PREV_TEXTURES; j++)
         if (glsl->uniforms[i].prev[j].texture >= 0)
            max_prev = MAX(j + 1, max_prev);

   return max_prev;
}

static bool gl_glsl_mipmap_input(void *data, unsigned idx)
{
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;
   if (glsl && idx)
      return glsl->shader->pass[idx - 1].mipmap;
   return false;
}

static bool gl_glsl_get_feedback_pass(void *data, unsigned *index)
{
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;
   if (!glsl || glsl->shader->feedback_pass < 0)
      return false;

   *index = glsl->shader->feedback_pass;
   return true;
}

static struct video_shader *gl_glsl_get_current_shader(void *data)
{
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)data;
   if (!glsl)
      return NULL;
   return glsl->shader;
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

static void gl_glsl_get_flags(uint32_t *flags)
{
   BIT32_SET(*flags, GFX_CTX_FLAGS_SHADERS_GLSL);
}

const shader_backend_t gl_glsl_backend = {
   gl_glsl_init,
   gl_glsl_init_menu_shaders,
   gl_glsl_deinit,
   gl_glsl_set_params,
   gl_glsl_set_uniform_parameter,
   gl_glsl_compile_program,
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
   gl_glsl_get_flags,

   RARCH_SHADER_GLSL,
   "glsl"
};
