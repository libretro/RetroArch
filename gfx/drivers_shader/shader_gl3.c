/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019 - Hans-Kristian Arntzen
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

#include <retro_common_api.h>

#include "shader_gl3.h"
#include "glslang_util.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <string/stdstring.h>
#include <formats/image.h>
#include <retro_miscellaneous.h>

#include "slang_process.h"
#include "spirv_opengl.h"
/* The vendored SPIRV-Cross headers end their enumerator lists with a
 * comma, which the C89 lane rejects under -pedantic; they are upstream
 * files and are not edited here. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <spirv_cross_c.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "../common/gl3_defines.h"

/* GL_ARB_gl_spirv is desktop only, and glsym does not carry ShaderBinary,
 * SpecializeShader or GetStringi for the GLES targets, so the whole path
 * has to compile out there. */
#if !defined(HAVE_OPENGLES3) && !defined(HAVE_OPENGLES)
#define GL3_HAVE_SPIRV_BINARY 1
#ifndef GL_SHADER_BINARY_FORMAT_SPIR_V_ARB
#define GL_SHADER_BINARY_FORMAT_SPIR_V_ARB 0x9551
#endif
#endif

#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../msg_hash.h"
#include "../../input/input_driver.h"

/* The header declares these as struct tags; C has no implicit typedef. */
typedef struct gl3_filter_chain_texture gl3_filter_chain_texture;
typedef struct gl3_viewport gl3_viewport;
typedef struct gl3_filter_chain_pass_info gl3_filter_chain_pass_info;
typedef struct gl3_buffer_locations gl3_buffer_locations;
typedef struct gl3_common_resources gl3_common_resources;

static void gl3_build_default_matrix(float *data)
{
   data[0]  =  2.0f;
   data[1]  =  0.0f;
   data[2]  =  0.0f;
   data[3]  =  0.0f;
   data[4]  =  0.0f;
   data[5]  =  2.0f;
   data[6]  =  0.0f;
   data[7]  =  0.0f;
   data[8]  =  0.0f;
   data[9]  =  0.0f;
   data[10] =  2.0f;
   data[11] =  0.0f;
   data[12] = -1.0f;
   data[13] = -1.0f;
   data[14] =  0.0f;
   data[15] =  1.0f;
}

RETRO_BEGIN_DECLS

   void gl3_framebuffer_copy(
         GLuint fb_id,
         GLuint quad_program,
         GLuint quad_vbo,
         GLint flat_ubo_vertex,
         unsigned size_width, unsigned size_height,
         GLuint image);

   void gl3_framebuffer_copy_partial(
         GLuint fb_id,
         GLuint quad_program,
         GLint flat_ubo_vertex,
         unsigned size_width, unsigned size_height,
         GLuint image,
         float rx, float ry);

   GLuint gl3_compile_shader(GLenum stage, const char *source);
   uint32_t gl3_get_cross_compiler_target_version(void);

   static GLenum address_to_gl(glslang_filter_chain_address type)
   {
      switch (type)
      {
#ifdef HAVE_OPENGLES3
         case GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_BORDER:
#if 0
            RARCH_WARN("[GLCore] No CLAMP_TO_BORDER in GLES3. Falling back to edge clamp.\n");
#endif
            return GL_CLAMP_TO_EDGE;
#else
         case GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_BORDER:
            return GL_CLAMP_TO_BORDER;
#endif
         case GLSLANG_FILTER_CHAIN_ADDRESS_REPEAT:
            return GL_REPEAT;
         case GLSLANG_FILTER_CHAIN_ADDRESS_MIRRORED_REPEAT:
            return GL_MIRRORED_REPEAT;
         case GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE:
         default:
            break;
      }

      return GL_CLAMP_TO_EDGE;
   }

   static GLenum convert_filter_to_mag_gl(glslang_filter_chain_filter filter)
   {
      switch (filter)
      {
         case GLSLANG_FILTER_CHAIN_LINEAR:
            return GL_LINEAR;
         case GLSLANG_FILTER_CHAIN_NEAREST:
         default:
            break;
      }

      return GL_NEAREST;
   }

   static GLenum convert_filter_to_min_gl(glslang_filter_chain_filter filter, glslang_filter_chain_filter mipfilter)
   {
      if (     (filter    == GLSLANG_FILTER_CHAIN_LINEAR)
            && (mipfilter == GLSLANG_FILTER_CHAIN_LINEAR)
         )
         return GL_LINEAR_MIPMAP_LINEAR;
      else if (filter == GLSLANG_FILTER_CHAIN_LINEAR)
         return GL_LINEAR_MIPMAP_NEAREST;
      else if (mipfilter == GLSLANG_FILTER_CHAIN_LINEAR)
         return GL_NEAREST_MIPMAP_LINEAR;
      return GL_NEAREST_MIPMAP_NEAREST;
   }

RETRO_END_DECLS

/* Fetch one reflected-resource list; mirrors slang_process.c's helper. */
static bool gl3_spvc_list(spvc_resources resources, spvc_resource_type type,
      const spvc_reflected_resource **list, size_t *count)
{
   *list  = NULL;
   *count = 0;
   return spvc_resources_get_resource_list_for_type(
         resources, type, list, count) == SPVC_SUCCESS;
}

/* Rename a stage variable to RARCH_<prefix>_<location> and drop the
 * location decoration, the way the C++ path did. */
static void gl3_spvc_rename_by_location(spvc_compiler compiler,
      const spvc_reflected_resource *list, size_t count, const char *prefix)
{
   size_t i;
   for (i = 0; i < count; i++)
   {
      char name[64];
      unsigned location = spvc_compiler_get_decoration(compiler,
            list[i].id, SpvDecorationLocation);
      snprintf(name, sizeof(name), "%s%u", prefix, location);
      spvc_compiler_set_name(compiler, list[i].id, name);
      spvc_compiler_unset_decoration(compiler, list[i].id,
            SpvDecorationLocation);
   }
}

GLuint gl3_cross_compile_program(
      const uint32_t *vertex, size_t vertex_size,
      const uint32_t *fragment, size_t fragment_size,
      gl3_buffer_locations *loc, bool flatten)
{
   GLuint program                  = 0;
   GLuint vertex_shader            = 0;
   GLuint fragment_shader          = 0;
   GLint status                    = 0;
   spvc_context ctx                = NULL;
   spvc_parsed_ir vs_ir            = NULL;
   spvc_parsed_ir ps_ir            = NULL;
   spvc_compiler vs_compiler       = NULL;
   spvc_compiler ps_compiler       = NULL;
   spvc_resources vs_resources     = NULL;
   spvc_resources ps_resources     = NULL;
   spvc_compiler_options vs_opts   = NULL;
   spvc_compiler_options ps_opts   = NULL;
   const char *vertex_source       = NULL;
   const char *fragment_source     = NULL;
   uint32_t *texture_fixups        = NULL;
   size_t num_texture_fixups       = 0;
   const spvc_reflected_resource *list = NULL;
   const spvc_reflected_resource *vs_inputs = NULL;
   size_t num_vs_inputs            = 0;
   size_t count                    = 0;
   size_t i;

   if (spvc_context_create(&ctx) != SPVC_SUCCESS)
      return 0;

   if (   spvc_context_parse_spirv(ctx, (const SpvId*)vertex,
            vertex_size / 4, &vs_ir) != SPVC_SUCCESS
       || spvc_context_parse_spirv(ctx, (const SpvId*)fragment,
            fragment_size / 4, &ps_ir) != SPVC_SUCCESS
       || spvc_context_create_compiler(ctx, SPVC_BACKEND_GLSL, vs_ir,
            SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &vs_compiler) != SPVC_SUCCESS
       || spvc_context_create_compiler(ctx, SPVC_BACKEND_GLSL, ps_ir,
            SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &ps_compiler) != SPVC_SUCCESS
       || spvc_compiler_create_shader_resources(vs_compiler,
            &vs_resources) != SPVC_SUCCESS
       || spvc_compiler_create_shader_resources(ps_compiler,
            &ps_resources) != SPVC_SUCCESS
       || spvc_compiler_create_compiler_options(vs_compiler,
            &vs_opts) != SPVC_SUCCESS
       || spvc_compiler_create_compiler_options(ps_compiler,
            &ps_opts) != SPVC_SUCCESS)
      goto error;

   for (i = 0; i < 2; i++)
   {
      spvc_compiler_options o = i ? ps_opts : vs_opts;
      spvc_compiler_options_set_bool(o, SPVC_COMPILER_OPTION_GLSL_ES,
#ifdef HAVE_OPENGLES3
            SPVC_TRUE
#else
            SPVC_FALSE
#endif
            );
      spvc_compiler_options_set_uint(o, SPVC_COMPILER_OPTION_GLSL_VERSION,
            gl3_get_cross_compiler_target_version());
      spvc_compiler_options_set_bool(o,
            SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_FLOAT_PRECISION_HIGHP,
            SPVC_TRUE);
      spvc_compiler_options_set_bool(o,
            SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_INT_PRECISION_HIGHP,
            SPVC_TRUE);
      spvc_compiler_options_set_bool(o,
            SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, SPVC_FALSE);
   }

   if (   spvc_compiler_install_compiler_options(vs_compiler,
            vs_opts) != SPVC_SUCCESS
       || spvc_compiler_install_compiler_options(ps_compiler,
            ps_opts) != SPVC_SUCCESS)
      goto error;

   if (!gl3_spvc_list(vs_resources, SPVC_RESOURCE_TYPE_STAGE_INPUT,
            &vs_inputs, &num_vs_inputs))
      goto error;
   gl3_spvc_rename_by_location(vs_compiler, vs_inputs, num_vs_inputs,
         "RARCH_ATTRIBUTE_");

   if (!gl3_spvc_list(vs_resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT,
            &list, &count))
      goto error;
   gl3_spvc_rename_by_location(vs_compiler, list, count, "RARCH_VARYING_");

   if (!gl3_spvc_list(ps_resources, SPVC_RESOURCE_TYPE_STAGE_INPUT,
            &list, &count))
      goto error;
   gl3_spvc_rename_by_location(ps_compiler, list, count, "RARCH_VARYING_");

   if (!gl3_spvc_list(vs_resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT,
            &list, &count))
      goto error;
   if (count > 1)
   {
      RARCH_ERR("[GLCore] Cannot have more than one push constant buffer.\n");
      goto error;
   }
   for (i = 0; i < count; i++)
   {
      spvc_compiler_set_name(vs_compiler, list[i].id,
            "RARCH_PUSH_VERTEX_INSTANCE");
      spvc_compiler_set_name(vs_compiler, list[i].base_type_id,
            "RARCH_PUSH_VERTEX");
   }

   if (!gl3_spvc_list(vs_resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER,
            &list, &count))
      goto error;
   if (count > 1)
   {
      RARCH_ERR("[GLCore] Cannot have more than one uniform buffer.\n");
      goto error;
   }
   for (i = 0; i < count; i++)
   {
      if (flatten)
         spvc_compiler_flatten_buffer_block(vs_compiler, list[i].id);
      spvc_compiler_set_name(vs_compiler, list[i].id,
            "RARCH_UBO_VERTEX_INSTANCE");
      spvc_compiler_set_name(vs_compiler, list[i].base_type_id,
            "RARCH_UBO_VERTEX");
      spvc_compiler_unset_decoration(vs_compiler, list[i].id,
            SpvDecorationDescriptorSet);
      spvc_compiler_unset_decoration(vs_compiler, list[i].id,
            SpvDecorationBinding);
   }

   if (!gl3_spvc_list(ps_resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT,
            &list, &count))
      goto error;
   if (count > 1)
   {
      RARCH_ERR("[GLCore] Cannot have more than one push constant block.\n");
      goto error;
   }
   for (i = 0; i < count; i++)
   {
      spvc_compiler_set_name(ps_compiler, list[i].id,
            "RARCH_PUSH_FRAGMENT_INSTANCE");
      spvc_compiler_set_name(ps_compiler, list[i].base_type_id,
            "RARCH_PUSH_FRAGMENT");
   }

   if (!gl3_spvc_list(ps_resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER,
            &list, &count))
      goto error;
   if (count > 1)
   {
      RARCH_ERR("[GLCore] Cannot have more than one uniform buffer.\n");
      goto error;
   }
   for (i = 0; i < count; i++)
   {
      if (flatten)
         spvc_compiler_flatten_buffer_block(ps_compiler, list[i].id);
      spvc_compiler_set_name(ps_compiler, list[i].id,
            "RARCH_UBO_FRAGMENT_INSTANCE");
      spvc_compiler_set_name(ps_compiler, list[i].base_type_id,
            "RARCH_UBO_FRAGMENT");
      spvc_compiler_unset_decoration(ps_compiler, list[i].id,
            SpvDecorationDescriptorSet);
      spvc_compiler_unset_decoration(ps_compiler, list[i].id,
            SpvDecorationBinding);
   }

   if (!gl3_spvc_list(ps_resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE,
            &list, &count))
      goto error;
   if (count)
   {
      if (!(texture_fixups = (uint32_t*)malloc(count * sizeof(*texture_fixups))))
         goto error;
      for (i = 0; i < count; i++)
      {
         char name[64];
         unsigned binding = spvc_compiler_get_decoration(ps_compiler,
               list[i].id, SpvDecorationBinding);
         snprintf(name, sizeof(name), "RARCH_TEXTURE_%u", binding);
         spvc_compiler_set_name(ps_compiler, list[i].id, name);
         spvc_compiler_unset_decoration(ps_compiler, list[i].id,
               SpvDecorationDescriptorSet);
         spvc_compiler_unset_decoration(ps_compiler, list[i].id,
               SpvDecorationBinding);
         texture_fixups[num_texture_fixups++] = binding;
      }
   }

   if (   spvc_compiler_compile(vs_compiler, &vertex_source) != SPVC_SUCCESS
       || spvc_compiler_compile(ps_compiler, &fragment_source) != SPVC_SUCCESS)
      goto error;

   vertex_shader   = gl3_compile_shader(GL_VERTEX_SHADER, vertex_source);
   fragment_shader = gl3_compile_shader(GL_FRAGMENT_SHADER, fragment_source);

   if (!vertex_shader || !fragment_shader)
   {
      RARCH_ERR("[GLCore] One or more shaders failed to compile.\n");
      if (vertex_shader)
         glDeleteShader(vertex_shader);
      if (fragment_shader)
         glDeleteShader(fragment_shader);
      goto error;
   }

   program = glCreateProgram();
   glAttachShader(program, vertex_shader);
   glAttachShader(program, fragment_shader);

   for (i = 0; i < num_vs_inputs; i++)
   {
      char name[64];
      unsigned _loc = spvc_compiler_get_decoration(vs_compiler,
            vs_inputs[i].id, SpvDecorationLocation);
      snprintf(name, sizeof(name), "RARCH_ATTRIBUTE_%u", _loc);
      glBindAttribLocation(program, _loc, name);
   }

   glLinkProgram(program);
   glDeleteShader(vertex_shader);
   glDeleteShader(fragment_shader);

   glGetProgramiv(program, GL_LINK_STATUS, &status);
   if (!status)
   {
      GLint length;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
      if (length > 0)
      {
         char *info_log = (char*)malloc(length);

         if (info_log)
         {
            glGetProgramInfoLog(program, length, &length, info_log);
            RARCH_ERR("[GLCore] Failed to link program: %s\n", info_log);
            free(info_log);
            glDeleteProgram(program);
            program = 0;
            goto error;
         }
      }
   }

   glUseProgram(program);

   if (loc)
   {
      loc->flat_ubo_fragment            = -1;
      loc->flat_ubo_vertex              = -1;
      loc->flat_push_vertex             = -1;
      loc->flat_push_fragment           = -1;
      loc->buffer_index_ubo_vertex      = GL_INVALID_INDEX;
      loc->buffer_index_ubo_fragment    = GL_INVALID_INDEX;
      loc->buffer_index_push_vertex     = GL_INVALID_INDEX;
      loc->buffer_index_push_fragment   = GL_INVALID_INDEX;

      if (flatten)
      {
         loc->flat_ubo_vertex           = glGetUniformLocation(program, "RARCH_UBO_VERTEX");
         loc->flat_ubo_fragment         = glGetUniformLocation(program, "RARCH_UBO_FRAGMENT");
         loc->flat_push_vertex          = glGetUniformLocation(program, "RARCH_PUSH_VERTEX");
         loc->flat_push_fragment        = glGetUniformLocation(program, "RARCH_PUSH_FRAGMENT");
      }
      else
      {
         loc->buffer_index_ubo_vertex   = glGetUniformBlockIndex(program, "RARCH_UBO_VERTEX");
         loc->buffer_index_ubo_fragment = glGetUniformBlockIndex(program, "RARCH_UBO_FRAGMENT");
      }
   }

   /* Force proper bindings for textures. */
   for (i = 0; i < num_texture_fixups; i++)
   {
      char name[64];
      GLint location;
      snprintf(name, sizeof(name), "RARCH_TEXTURE_%u", texture_fixups[i]);
      if ((location = glGetUniformLocation(program, name)) >= 0)
         glUniform1i(location, texture_fixups[i]);
   }

   glUseProgram(0);

   free(texture_fixups);
   spvc_context_destroy(ctx);
   return program;

error:
   if (ctx)
   {
      const char *err = spvc_context_get_last_error_string(ctx);
      if (err && *err)
         RARCH_ERR("[GLCore] Failed to cross compile program: %s\n", err);
      spvc_context_destroy(ctx);
   }
   free(texture_fixups);
   if (program != 0)
      glDeleteProgram(program);
   return 0;
}

#ifdef GL3_HAVE_SPIRV_BINARY
static GLuint gl3_spirv_compile_stage(GLenum stage,
      const uint32_t *spirv, size_t words)
{
   GLint status = 0;
   GLuint shader;

   /* ShaderBinary reports an unsupported module through the GL error
    * queue, so start from a known state. */
   while (glGetError() != GL_NO_ERROR);

   shader = glCreateShader(stage);
   if (!shader)
      return 0;

   glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB,
         spirv, (GLsizei)(words * sizeof(uint32_t)));

   if (glGetError() != GL_NO_ERROR)
   {
      glDeleteShader(shader);
      return 0;
   }

   /* No specialization constants: slang has no way to express them. */
   if (glSpecializeShader)
      glSpecializeShader(shader, "main", 0, NULL, NULL);
   else
      glSpecializeShaderARB(shader, "main", 0, NULL, NULL);

   glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

   if (!status)
   {
      GLint length = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
      if (length > 1)
      {
         char *info_log = (char*)malloc(length);
         if (info_log)
         {
            glGetShaderInfoLog(shader, length, &length, info_log);
            RARCH_WARN("[GLCore] Failed to specialize SPIR-V shader: %s\n",
                  info_log);
            free(info_log);
         }
      }
      glDeleteShader(shader);
      return 0;
   }

   while (glGetError() != GL_NO_ERROR);

   return shader;
}

GLuint gl3_spirv_link_program(
      const uint32_t *vertex, size_t vertex_words,
      const uint32_t *fragment, size_t fragment_words,
      unsigned push_binding)
{
   GLint status = 0;
   GLuint program         = 0;
   GLuint vertex_shader   = 0;
   GLuint fragment_shader = 0;
   size_t vertex_gl_cap     = vertex_words   + SPIRV_OPENGL_LOWER_EXTRA_WORDS;
   size_t fragment_gl_cap   = fragment_words + SPIRV_OPENGL_LOWER_EXTRA_WORDS;
   size_t vertex_gl_words   = 0;
   size_t fragment_gl_words = 0;
   /* The vectors zero-filled; spirv_opengl_lower writes every word it
    * reports, so plain malloc is equivalent. */
   uint32_t *vertex_gl      = (uint32_t*)malloc(vertex_gl_cap   * sizeof(uint32_t));
   uint32_t *fragment_gl    = (uint32_t*)malloc(fragment_gl_cap * sizeof(uint32_t));

   if (!vertex_gl || !fragment_gl)
      goto done;

   vertex_gl_words   = spirv_opengl_lower(vertex, vertex_words,
         vertex_gl, vertex_gl_cap, push_binding);
   fragment_gl_words = spirv_opengl_lower(fragment, fragment_words,
         fragment_gl, fragment_gl_cap, push_binding);

   /* Both stages have to make it, or neither does: the lowered push
    * constant block must match across the program. */
   if (!vertex_gl_words || !fragment_gl_words)
      goto done;

   if (!(vertex_shader = gl3_spirv_compile_stage(GL_VERTEX_SHADER,
               vertex_gl, vertex_gl_words)))
      goto done;

   if (!(fragment_shader = gl3_spirv_compile_stage(GL_FRAGMENT_SHADER,
               fragment_gl, fragment_gl_words)))
   {
      glDeleteShader(vertex_shader);
      vertex_shader = 0;
      goto done;
   }

   program = glCreateProgram();
   glAttachShader(program, vertex_shader);
   glAttachShader(program, fragment_shader);
   /* BindAttribLocation has no effect on SPIR-V shaders; the Location
    * decorations in the module are authoritative. slang already pins
    * Position to 0 and TexCoord to 1, which is what the vertex arrays
    * are set up for. */
   glLinkProgram(program);
   glDeleteShader(vertex_shader);
   glDeleteShader(fragment_shader);

   glGetProgramiv(program, GL_LINK_STATUS, &status);

   if (!status)
   {
      GLint length = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
      if (length > 1)
      {
         char *info_log = (char*)malloc(length);
         if (info_log)
         {
            glGetProgramInfoLog(program, length, &length, info_log);
            RARCH_WARN("[GLCore] Failed to link SPIR-V program: %s\n",
                  info_log);
            free(info_log);
         }
      }
      glDeleteProgram(program);
      program = 0;
      goto done;
   }

   free(vertex_gl);
   free(fragment_gl);
   return program;

done:
   free(vertex_gl);
   free(fragment_gl);
   return 0;
}
#else
GLuint gl3_spirv_link_program(
      const uint32_t *vertex, size_t vertex_words,
      const uint32_t *fragment, size_t fragment_words,
      unsigned push_binding)
{
   (void)vertex;
   (void)vertex_words;
   (void)fragment;
   (void)fragment_words;
   (void)push_binding;
   return 0;
}
#endif

static const uint32_t gl3_opaque_vert[] =
#include "../drivers/vulkan_shaders/opaque.vert.inc"
;

static const uint32_t gl3_opaque_frag[] =
#include "../drivers/vulkan_shaders/opaque.frag.inc"
;

typedef struct
{
   gl3_filter_chain_texture texture;
   glslang_filter_chain_filter filter;
   glslang_filter_chain_filter mip_filter;
   glslang_filter_chain_address address;
} gl3_texture_t;

static GLenum convert_glslang_format(glslang_format fmt)
{
#undef FMT
#define FMT(x, r) case SLANG_FORMAT_##x: return GL_##r
   switch (fmt)
   {
      FMT(R8_UNORM, R8);
      FMT(R8_SINT, R8I);
      FMT(R8_UINT, R8UI);
      FMT(R8G8_UNORM, RG8);
      FMT(R8G8_SINT, RG8I);
      FMT(R8G8_UINT, RG8UI);
      FMT(R8G8B8A8_UNORM, RGBA8);
      FMT(R8G8B8A8_SINT, RGBA8I);
      FMT(R8G8B8A8_UINT, RGBA8UI);
      FMT(R8G8B8A8_SRGB, SRGB8_ALPHA8);

      FMT(A2B10G10R10_UNORM_PACK32, RGB10_A2);
      FMT(A2B10G10R10_UINT_PACK32, RGB10_A2UI);

      FMT(R16_UINT, R16UI);
      FMT(R16_SINT, R16I);
      FMT(R16_SFLOAT, R16F);
      FMT(R16G16_UINT, RG16UI);
      FMT(R16G16_SINT, RG16I);
      FMT(R16G16_SFLOAT, RG16F);
      FMT(R16G16B16A16_UINT, RGBA16UI);
      FMT(R16G16B16A16_SINT, RGBA16I);
      FMT(R16G16B16A16_SFLOAT, RGBA16F);

      FMT(R32_UINT, R32UI);
      FMT(R32_SINT, R32I);
      FMT(R32_SFLOAT, R32F);
      FMT(R32G32_UINT, RG32UI);
      FMT(R32G32_SINT, RG32I);
      FMT(R32G32_SFLOAT, RG32F);
      FMT(R32G32B32A32_UINT, RGBA32UI);
      FMT(R32G32B32A32_SINT, RGBA32I);
      FMT(R32G32B32A32_SFLOAT, RGBA32F);

      default:
         break;
   }

   return 0;
}

struct gl3_static_texture
{
   char *id;        /* owned (strdup'd) */
   GLuint image;
   gl3_texture_t texture;
};

static void gl3_static_texture_init(struct gl3_static_texture *tex,
      const char *id_, GLuint image_,
      unsigned width, unsigned height, bool linear, bool mipmap,
      glslang_filter_chain_address address)
{
   GLenum gl_address         = address_to_gl(address);
   gl3_texture_t *texture          = &tex->texture;

   tex->id                   = id_ ? strdup(id_) : NULL;
   tex->image                = image_;

   texture->filter            = GLSLANG_FILTER_CHAIN_NEAREST;
   texture->mip_filter        = GLSLANG_FILTER_CHAIN_NEAREST;
   texture->address           = address;
   texture->texture.width     = width;
   texture->texture.height    = height;
   texture->texture.format    = 0;
   texture->texture.image     = image_;

   if (linear)
   {
      texture->filter         = GLSLANG_FILTER_CHAIN_LINEAR;
      if (mipmap)
         texture->mip_filter  = GLSLANG_FILTER_CHAIN_LINEAR;
   }

   glBindTexture(GL_TEXTURE_2D, image_);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_address);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_address);

   if (linear)
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      if (mipmap)
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
               GL_LINEAR_MIPMAP_LINEAR);
      else
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
               GL_LINEAR);
   }
   else
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

static void gl3_static_texture_free(struct gl3_static_texture *tex)
{
   if (!tex)
      return;
   if (tex->image != 0)
      glDeleteTextures(1, &tex->image);
   tex->image = 0;
   free(tex->id);
   tex->id    = NULL;
}

struct gl3_common_resources
{
   gl3_texture_t *original_history;
   size_t num_original_history;
   gl3_texture_t *framebuffer_feedback;
   size_t num_framebuffer_feedback;
   gl3_texture_t *pass_outputs;
   size_t num_pass_outputs;
   struct gl3_static_texture *luts;
   size_t num_luts;

   slang_texture_semantic_name_map texture_semantic_map;
   slang_texture_semantic_name_map texture_semantic_uniform_map;
   struct video_shader *shader_preset;

   GLuint quad_program;
   GLuint quad_vbo;
   gl3_buffer_locations quad_loc;
};

/* Every default was zero, so a memset covers the initializers the
 * in-class ones used to supply. */
static void gl3_common_resources_init(gl3_common_resources *common)
{
   static float quad_data[] = {
      0.0f, 0.0f, 0.0f, 0.0f,
      1.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
   };

   memset(common, 0, sizeof(*common));

   glGenBuffers(1, &common->quad_vbo);
   glBindBuffer(GL_ARRAY_BUFFER, common->quad_vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(quad_data), quad_data, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   common->quad_program = gl3_cross_compile_program(
         gl3_opaque_vert, sizeof(gl3_opaque_vert),
         gl3_opaque_frag, sizeof(gl3_opaque_frag), &common->quad_loc, true);
}

static void gl3_common_resources_free(gl3_common_resources *common)
{
   size_t i;
   /* The unique_ptr vector used to free these on destruction; the plain
    * array does not, so every LUT has to be released by hand. */
   for (i = 0; i < common->num_luts; i++)
      gl3_static_texture_free(&common->luts[i]);
   free(common->luts);
   common->luts     = NULL;
   common->num_luts = 0;
   /* the three gl3_texture_t vectors and the common->shader_preset unique_ptr were
    * released by the implicit member destructors */
   free(common->original_history);
   free(common->framebuffer_feedback);
   free(common->pass_outputs);
   common->original_history       = NULL;
   common->framebuffer_feedback   = NULL;
   common->pass_outputs           = NULL;
   common->num_original_history   = 0;
   common->num_framebuffer_feedback = 0;
   common->num_pass_outputs       = 0;
   free(common->shader_preset);
   common->shader_preset          = NULL;
   slang_texture_semantic_name_map_free(&common->texture_semantic_map);
   slang_texture_semantic_name_map_free(&common->texture_semantic_uniform_map);
   if (common->quad_program != 0)
      glDeleteProgram(common->quad_program);
   if (common->quad_vbo != 0)
      glDeleteBuffers(1, &common->quad_vbo);
}

struct gl3_framebuffer
{
   GLuint image;
   unsigned size_width;
      unsigned size_height;
   GLenum format;
   unsigned max_levels;
   unsigned levels;
   GLuint framebuffer;
   bool complete;
};

static void gl3_framebuffer_build(struct gl3_framebuffer *fb);

/* Returns NULL rather than a half-built object; the caller owns the
 * result and releases it with gl3_framebuffer_delete. */
static struct gl3_framebuffer *gl3_framebuffer_new(GLenum format_,
      unsigned max_levels_)
{
   struct gl3_framebuffer *fb = (struct gl3_framebuffer*)
      calloc(1, sizeof(*fb));

   if (!fb)
      return NULL;

   fb->size_width  = 1;
   fb->size_height = 1;
   fb->format      = format_;
   fb->max_levels  = max_levels_;

   glGenFramebuffers(1, &fb->framebuffer);

   /* Need to bind to create */
   glBindFramebuffer(GL_FRAMEBUFFER, fb->framebuffer);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   if (fb->format == 0)
      fb->format = GL_RGBA8;

   return fb;
}

static void gl3_framebuffer_set_size(struct gl3_framebuffer *fb,
      unsigned width_, unsigned height_, GLenum format_)
{
   fb->size_width  = width_;
   fb->size_height = height_;
   if (format_ != 0)
      fb->format = format_;

   gl3_framebuffer_build(fb);
}

static void gl3_framebuffer_build(struct gl3_framebuffer *fb)
{
   GLenum status;

   glBindFramebuffer(GL_FRAMEBUFFER, fb->framebuffer);
   if (fb->image != 0)
   {
      glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
      glDeleteTextures(1, &fb->image);
   }

   glGenTextures(1, &fb->image);
   glBindTexture(GL_TEXTURE_2D, fb->image);

   if (fb->size_width == 0)
      fb->size_width = 1;
   if (fb->size_height == 0)
      fb->size_height = 1;

   fb->levels = glslang_num_miplevels(fb->size_width, fb->size_height);
   if (fb->max_levels < fb->levels)
      fb->levels = fb->max_levels;
   if (fb->levels == 0)
      fb->levels = 1;

   glTexStorage2D(GL_TEXTURE_2D, fb->levels,
                  fb->format,
                  fb->size_width, fb->size_height);

   glFramebufferTexture2D(GL_FRAMEBUFFER,
         GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->image, 0);

   status   = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   fb->complete = true;

   if (status != GL_FRAMEBUFFER_COMPLETE)
   {
      fb->complete = false;

      switch (status)
      {
         case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            RARCH_ERR("[GLCore] Incomplete attachment.\n");
            break;

         case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            RARCH_ERR("[GLCore] Incomplete, missing attachment.\n");
            break;

         case GL_FRAMEBUFFER_UNSUPPORTED:
            {
               unsigned levels;

               RARCH_ERR("[GLCore] Unsupported FBO, falling back to RGBA8.\n");

               glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
               glDeleteTextures(1, &fb->image);
               glGenTextures(1, &fb->image);
               glBindTexture(GL_TEXTURE_2D, fb->image);

               levels = glslang_num_miplevels(fb->size_width, fb->size_height);
               if (fb->max_levels < levels)
                  levels = fb->max_levels;
               glTexStorage2D(GL_TEXTURE_2D, levels,
                     GL_RGBA8,
                     fb->size_width, fb->size_height);
               glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->image, 0);
               fb->complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            }
            break;
      }
   }

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glBindTexture(GL_TEXTURE_2D, 0);
}

static void gl3_framebuffer_delete(struct gl3_framebuffer *fb)
{
   if (!fb)
      return;
   if (fb->framebuffer != 0)
      glDeleteFramebuffers(1, &fb->framebuffer);
   if (fb->image != 0)
      glDeleteTextures(1, &fb->image);
   free(fb);
}

/* The ring has always been sixteen buffers deep; a fixed array removes the
 * allocation the vector was doing for a compile-time constant. */
#define GL3_UBO_RING_SIZE 16

struct gl3_pass_parameter
{
   char *id;                /* owned */
   unsigned index;
   unsigned semantic_index;
};

struct gl3_ubo_ring
{
   GLuint buffers[GL3_UBO_RING_SIZE];
   unsigned num_buffers;
   unsigned buffer_index;
};

/* Every resize below follows a clear and every element is written before it
 * is read, so allocating fresh zeroed storage matches what vector::resize
 * did. */
static bool gl3_texture_array_resize(gl3_texture_t **arr, size_t *count,
      size_t want)
{
   gl3_texture_t *next;
   free(*arr);
   *arr   = NULL;
   *count = 0;
   if (!want)
      return true;
   if (!(next = (gl3_texture_t*)calloc(want, sizeof(*next))))
      return false;
   *arr   = next;
   *count = want;
   return true;
}

static void gl3_ubo_ring_free(struct gl3_ubo_ring *ring)
{
   if (ring->num_buffers)
      glDeleteBuffers((GLsizei)ring->num_buffers, ring->buffers);
   ring->num_buffers  = 0;
   ring->buffer_index = 0;
}

struct gl3_pass
{

#ifdef GL3_ROLLING_SCANLINE_SIMULATION

#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

   bool final_pass;

   GLuint pipeline;
   gl3_common_resources *common;

   unsigned current_framebuffer_size_width;
      unsigned current_framebuffer_size_height;
   gl3_viewport curr_vp;
   gl3_filter_chain_pass_info pass_info;

   uint32_t *vertex_shader;
   size_t num_vertex_shader;
   uint32_t *fragment_shader;
   size_t num_fragment_shader;
   struct gl3_framebuffer *framebuffer;
   struct gl3_framebuffer *framebuffer_feedback;

   /* Plain C struct: must be explicitly zero-initialized, since the
       * first build() and a teardown before any build() both run
       * slang_reflection_free() on it.  The previous C++ type had a
       * default constructor doing this implicitly. */
      slang_reflection reflection;

   uint8_t *uniforms;
   size_t uniforms_size;

   uint64_t frame_count;
   unsigned frame_count_period;
   int32_t frame_direction;
   uint32_t frame_time_delta;
   float original_fps;
   uint32_t rotation;
   float core_aspect;
   float core_aspect_rot;
   unsigned pass_number;
   uint32_t total_subframes;
   uint32_t current_subframe;
#ifdef GL3_ROLLING_SCANLINE_SIMULATION
   bool simulate_scanline;
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

   size_t ubo_offset;
   char *pass_name;   /* owned */

   struct gl3_pass_parameter *parameters;
   size_t num_parameters;
   /* Indices into parameters[]; the vector of copies this replaces stayed
    * valid across a push_back reallocation, and indices do too. */
   unsigned *filtered_parameters;
   size_t num_filtered_parameters;
   uint8_t *push_constant_buffer;
   size_t push_constant_buffer_size;
   gl3_buffer_locations locations;
   struct gl3_ubo_ring ubo_ring;
   /* Only allocated on the GL_ARB_gl_spirv path, where the push constant
    * block becomes a second uniform buffer. */
   struct gl3_ubo_ring push_ring;
   /* SPIR-V modules carry no name reflection, so glGetUniformLocation()
    * cannot be used to find individual block members. */
   bool spirv_binary;
};

static const struct gl3_framebuffer * gl3_pass_get_framebuffer(struct gl3_pass *pass);
static struct gl3_framebuffer * gl3_pass_get_feedback_framebuffer(struct gl3_pass *pass);
static void gl3_pass_set_frame_count(struct gl3_pass *pass, uint64_t count);
static void gl3_pass_set_frame_count_period(struct gl3_pass *pass, unsigned period);
static void gl3_pass_set_frame_direction(struct gl3_pass *pass, int32_t direction);
static void gl3_pass_set_frame_time_delta(struct gl3_pass *pass, uint32_t time_delta);
static void gl3_pass_set_original_fps(struct gl3_pass *pass, float fps);
static void gl3_pass_set_rotation(struct gl3_pass *pass, uint32_t rot);
static void gl3_pass_set_core_aspect(struct gl3_pass *pass, float coreaspect);
static void gl3_pass_set_core_aspect_rot(struct gl3_pass *pass, float coreaspectrot);
static void gl3_pass_set_shader_subframes(struct gl3_pass *pass, uint32_t tot_subframes);
static void gl3_pass_set_current_shader_subframe(struct gl3_pass *pass, uint32_t cur_subframe);
static void gl3_pass_set_simulate_scanline(struct gl3_pass *pass, bool simulate);
static void gl3_pass_set_name(struct gl3_pass *pass, const char *name);
static const char * gl3_pass_get_name(struct gl3_pass *pass);
static glslang_filter_chain_filter gl3_pass_get_source_filter(struct gl3_pass *pass);
static glslang_filter_chain_filter gl3_pass_get_mip_filter(struct gl3_pass *pass);
static glslang_filter_chain_address gl3_pass_get_address_mode(struct gl3_pass *pass);
static void gl3_pass_set_common_resources(struct gl3_pass *pass, gl3_common_resources *common);
static const slang_reflection *gl3_pass_get_reflection(struct gl3_pass *pass);
static void gl3_pass_set_pass_number(struct gl3_pass *pass, unsigned number);
static bool gl3_pass_build(struct gl3_pass *pass);
static void gl3_pass_reflect_parameter(struct gl3_pass *pass, const char *name, slang_semantic_meta *meta);
static void gl3_pass_reflect_texture_parameter(struct gl3_pass *pass, const char *name, slang_texture_semantic_meta *meta);
static void gl3_pass_reflect_parameter_array(struct gl3_pass *pass, const char *name, slang_texture_semantic_array *meta);
static bool gl3_pass_init_pipeline(struct gl3_pass *pass);
static void gl3_pass_set_pass_info(struct gl3_pass *pass, const gl3_filter_chain_pass_info info);
static void gl3_pass_get_output_size(struct gl3_pass *pass,
      unsigned original_width, unsigned original_height,
      unsigned source_width, unsigned source_height,
      unsigned *out_width, unsigned *out_height);
static void gl3_pass_end_frame(struct gl3_pass *pass);
static void gl3_pass_build_semantic_vec4(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
      unsigned width, unsigned height);
static void gl3_pass_build_semantic_parameter(struct gl3_pass *pass, uint8_t *data, unsigned index, float value);
static void gl3_pass_build_semantic_uint(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
                               uint32_t value);
static void gl3_pass_build_semantic_int(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
                              int32_t value);
static void gl3_pass_build_semantic_float(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
                              float value);
static void gl3_pass_build_semantic_vec3(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
                              const float *values);
static void gl3_pass_build_semantic_texture(struct gl3_pass *pass, uint8_t *buffer,
      enum slang_texture_semantic semantic, const gl3_texture_t *texture);
static void gl3_pass_build_semantic_texture_array_vec4(struct gl3_pass *pass, uint8_t *data, enum slang_texture_semantic semantic,
      unsigned index, unsigned width, unsigned height);
static void gl3_pass_build_semantic_texture_vec4(struct gl3_pass *pass, uint8_t *data, enum slang_texture_semantic semantic,
      unsigned width, unsigned height);
static bool gl3_pass_init_feedback(struct gl3_pass *pass);
static void gl3_pass_set_shader(struct gl3_pass *pass, GLenum stage,
      const uint32_t *spirv,
      size_t spirv_words);
static void gl3_pass_add_parameter(struct gl3_pass *pass, unsigned index, const char *id);
static void gl3_pass_set_semantic_texture(struct gl3_pass *pass, enum slang_texture_semantic semantic,
      const gl3_texture_t *texture);
static void gl3_pass_build_semantic_texture_array(struct gl3_pass *pass, uint8_t *buffer,
      enum slang_texture_semantic semantic, unsigned index, const gl3_texture_t *texture);
static void gl3_pass_build_semantics(struct gl3_pass *pass, uint8_t *buffer,
      const float *mvp, const gl3_texture_t *original, const gl3_texture_t *source);
static void gl3_pass_build_commands(struct gl3_pass *pass,
      const gl3_texture_t *original,
      const gl3_texture_t *source,
      const gl3_viewport *vp,
      const float *mvp);
static void gl3_pass_free(struct gl3_pass *pass);

/* Every default was zero except these four, which the in-class
 * initializers used to supply. */
static struct gl3_pass *gl3_pass_new(bool final_pass)
{
   struct gl3_pass *pass = (struct gl3_pass*)calloc(1, sizeof(*pass));

   if (!pass)
      return NULL;

   pass->final_pass       = final_pass;
   pass->frame_direction  = 1;
   pass->total_subframes  = 1;
   pass->current_subframe = 1;
   return pass;
}

static const struct gl3_framebuffer * gl3_pass_get_framebuffer(struct gl3_pass *pass)
{
      return pass->framebuffer;
   }

static struct gl3_framebuffer * gl3_pass_get_feedback_framebuffer(struct gl3_pass *pass)
{
      return pass->framebuffer_feedback;
   }

static void gl3_pass_set_frame_count(struct gl3_pass *pass, uint64_t count)
{
      pass->frame_count = count;
   }

static void gl3_pass_set_frame_count_period(struct gl3_pass *pass, unsigned period)
{
      pass->frame_count_period = period;
   }

static void gl3_pass_set_frame_direction(struct gl3_pass *pass, int32_t direction)
{
      pass->frame_direction = direction;
   }

static void gl3_pass_set_frame_time_delta(struct gl3_pass *pass, uint32_t time_delta)
{
      pass->frame_time_delta = time_delta;
   }

static void gl3_pass_set_original_fps(struct gl3_pass *pass, float fps)
{
      pass->original_fps = fps;
   }

static void gl3_pass_set_rotation(struct gl3_pass *pass, uint32_t rot)
{
      pass->rotation = rot;
   }

static void gl3_pass_set_core_aspect(struct gl3_pass *pass, float coreaspect)
{
      pass->core_aspect = coreaspect;
   }

static void gl3_pass_set_core_aspect_rot(struct gl3_pass *pass, float coreaspectrot)
{
      pass->core_aspect_rot = coreaspectrot;
   }

static void gl3_pass_set_shader_subframes(struct gl3_pass *pass, uint32_t tot_subframes)
{
      pass->total_subframes = tot_subframes;
   }

static void gl3_pass_set_current_shader_subframe(struct gl3_pass *pass, uint32_t cur_subframe)
{
      pass->current_subframe = cur_subframe;
   }

static void gl3_pass_set_simulate_scanline(struct gl3_pass *pass, bool simulate)
{
      pass->simulate_scanline = simulate;
   }

static void gl3_pass_set_name(struct gl3_pass *pass, const char *name)
{
      free(pass->pass_name);
      pass->pass_name = name ? strdup(name) : NULL;
   }

static const char * gl3_pass_get_name(struct gl3_pass *pass)
{
      return pass->pass_name ? pass->pass_name : "";
   }

static glslang_filter_chain_filter gl3_pass_get_source_filter(struct gl3_pass *pass)
{
      return pass->pass_info.source_filter;
   }

static glslang_filter_chain_filter gl3_pass_get_mip_filter(struct gl3_pass *pass)
{
      return pass->pass_info.mip_filter;
   }

static glslang_filter_chain_address gl3_pass_get_address_mode(struct gl3_pass *pass)
{
      return pass->pass_info.address;
   }

static void gl3_pass_set_common_resources(struct gl3_pass *pass,
      gl3_common_resources *common)
{
   pass->common = common;
}

static const slang_reflection *gl3_pass_get_reflection(struct gl3_pass *pass)
{
      return &pass->reflection;
   }

static void gl3_pass_set_pass_number(struct gl3_pass *pass, unsigned number)
{
   pass->pass_number = number;
}


static bool gl3_pass_build(struct gl3_pass *pass)
{
   slang_semantic_name_map semantic_map = { 0 };
   unsigned i;
   unsigned j = 0;

   gl3_framebuffer_delete(pass->framebuffer);
   pass->framebuffer          = NULL;
   gl3_framebuffer_delete(pass->framebuffer_feedback);
   pass->framebuffer_feedback = NULL;

   if (!pass->final_pass)
      pass->framebuffer = gl3_framebuffer_new(pass->pass_info.rt_format,
            pass->pass_info.max_levels);

   for (i = 0; i < pass->num_parameters; i++)
   {
      if (!slang_semantic_name_map_set_unique(
               &semantic_map, pass->parameters[i].id, NULL,
               SLANG_SEMANTIC_FLOAT_PARAMETER, j))
      {
         slang_semantic_name_map_free(&semantic_map);
         return false;
      }
      j++;
   }

   slang_reflection_free(&pass->reflection);
   if (!slang_reflection_init(&pass->reflection))
   {
      slang_semantic_name_map_free(&semantic_map);
      return false;
   }
   pass->reflection.pass_number                  = pass->pass_number;
   pass->reflection.texture_semantic_map         = &pass->common->texture_semantic_map;
   pass->reflection.texture_semantic_uniform_map = &pass->common->texture_semantic_uniform_map;
   pass->reflection.semantic_map                 = &semantic_map;

   {
      bool refl_ok = slang_reflect_spirv(
            pass->vertex_shader, pass->num_vertex_shader,
            pass->fragment_shader, pass->num_fragment_shader,
            &pass->reflection);
      /* The parameter map is only needed during pass->reflection; the
       * pass->reflection keeps a dangling pointer otherwise. */
      slang_semantic_name_map_free(&semantic_map);
      pass->reflection.semantic_map = NULL;
      if (!refl_ok)
         return false;
   }

   /* Filter out pass->parameters which we will never use anyways. */
   pass->num_filtered_parameters = 0;

   for (i = 0; i < pass->reflection.num_float_parameters; i++)
   {
      if (pass->reflection.semantic_float_parameters[i].uniform ||
          pass->reflection.semantic_float_parameters[i].push_constant)
      {
         unsigned *next = (unsigned*)realloc(pass->filtered_parameters,
               (pass->num_filtered_parameters + 1) * sizeof(*pass->filtered_parameters));
         if (!next)
            return false;
         pass->filtered_parameters                          = next;
         pass->filtered_parameters[pass->num_filtered_parameters] = i;
         pass->num_filtered_parameters++;
      }
   }

   if (!gl3_pass_init_pipeline(pass))
      return false;

   return true;
}

/* The four instance-block prefixes are literals, so the qualified name
 * fits a fixed buffer the same way reflect_parameter_array's does. */
static int gl3_uniform_location_prefixed(GLuint pipeline,
      const char *prefix, const char *name)
{
   char n[256];
   size_t _len = strlcpy(n, prefix, sizeof(n));
   strlcpy(n + _len, name, sizeof(n) - _len);
   return glGetUniformLocation(pipeline, n);
}

static void gl3_pass_reflect_parameter(struct gl3_pass *pass, const char *name, slang_semantic_meta *meta)
{
   if (pass->spirv_binary)
      return;

   if (meta->uniform)
   {
      int vert = gl3_uniform_location_prefixed(pass->pipeline,
            "RARCH_UBO_VERTEX_INSTANCE.", name);
      int frag = gl3_uniform_location_prefixed(pass->pipeline,
            "RARCH_UBO_FRAGMENT_INSTANCE.", name);

      if (vert >= 0)
         meta->location.ubo_vertex = vert;
      if (frag >= 0)
         meta->location.ubo_fragment = frag;
   }

   if (meta->push_constant)
   {
      int vert = gl3_uniform_location_prefixed(pass->pipeline,
            "RARCH_PUSH_VERTEX_INSTANCE.", name);
      int frag = gl3_uniform_location_prefixed(pass->pipeline,
            "RARCH_PUSH_FRAGMENT_INSTANCE.", name);

      if (vert >= 0)
         meta->location.push_vertex = vert;
      if (frag >= 0)
         meta->location.push_fragment = frag;
   }
}

static void gl3_pass_reflect_texture_parameter(struct gl3_pass *pass, const char *name, slang_texture_semantic_meta *meta)
{
   if (pass->spirv_binary)
      return;

   if (meta->uniform)
   {
      int vert = gl3_uniform_location_prefixed(pass->pipeline,
            "RARCH_UBO_VERTEX_INSTANCE.", name);
      int frag = gl3_uniform_location_prefixed(pass->pipeline,
            "RARCH_UBO_FRAGMENT_INSTANCE.", name);

      if (vert >= 0)
         meta->location.ubo_vertex = vert;
      if (frag >= 0)
         meta->location.ubo_fragment = frag;
   }

   if (meta->push_constant)
   {
      int vert = gl3_uniform_location_prefixed(pass->pipeline,
            "RARCH_PUSH_VERTEX_INSTANCE.", name);
      int frag = gl3_uniform_location_prefixed(pass->pipeline,
            "RARCH_PUSH_FRAGMENT_INSTANCE.", name);

      if (vert >= 0)
         meta->location.push_vertex = vert;
      if (frag >= 0)
         meta->location.push_fragment = frag;
   }
}

static void gl3_pass_reflect_parameter_array(struct gl3_pass *pass, const char *name, slang_texture_semantic_array *meta)
{
   size_t i;

   if (pass->spirv_binary)
      return;

   for (i = 0; i < meta->size; i++)
   {
      char n[128];
      size_t _len;
      slang_texture_semantic_meta *m = &meta->data[i];
      _len = strlcpy(n, name, sizeof(n));
      snprintf(n + _len, sizeof(n) - _len, "%u", (unsigned)i);

      if (m->uniform)
      {
         int vert, frag;
         char vert_n[256];
         char frag_n[256];
         size_t _len  = strlcpy_lit(vert_n, "RARCH_UBO_VERTEX_INSTANCE.",   sizeof(vert_n));
         size_t _len2 = strlcpy_lit(frag_n, "RARCH_UBO_FRAGMENT_INSTANCE.", sizeof(frag_n));
         strlcpy(vert_n + _len,  n, sizeof(vert_n) - _len);
         strlcpy(frag_n + _len2, n, sizeof(frag_n) - _len2);
         vert = glGetUniformLocation(pass->pipeline, vert_n);
         frag = glGetUniformLocation(pass->pipeline, frag_n);

         if (vert >= 0)
            m->location.ubo_vertex   = vert;
         if (frag >= 0)
            m->location.ubo_fragment = frag;
      }

      if (m->push_constant)
      {
         int vert, frag;
         char vert_n[256];
         char frag_n[256];
         size_t _len  = strlcpy_lit(vert_n, "RARCH_PUSH_VERTEX_INSTANCE.",   sizeof(vert_n));
         size_t _len2 = strlcpy_lit(frag_n, "RARCH_PUSH_FRAGMENT_INSTANCE.", sizeof(frag_n));
         strlcpy(vert_n + _len,  n, sizeof(vert_n) - _len);
         strlcpy(frag_n + _len2, n, sizeof(frag_n) - _len2);
         vert = glGetUniformLocation(pass->pipeline, vert_n);
         frag = glGetUniformLocation(pass->pipeline, frag_n);

         if (vert >= 0)
            m->location.push_vertex   = vert;
         if (frag >= 0)
            m->location.push_fragment = frag;
      }
   }
}

static void gl3_ubo_ring_init(struct gl3_ubo_ring *ring, size_t size)
{
   unsigned i;

   /* Re-init would strand the previous generation of buffers. */
   gl3_ubo_ring_free(ring);

   ring->num_buffers = GL3_UBO_RING_SIZE;
   glGenBuffers((GLsizei)ring->num_buffers, ring->buffers);

   for (i = 0; i < ring->num_buffers; i++)
   {
      glBindBuffer(GL_UNIFORM_BUFFER, ring->buffers[i]);
      glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STREAM_DRAW);
   }

   glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static bool gl3_pass_init_pipeline(struct gl3_pass *pass)
{
   size_t mi;
   /* Handing the SPIR-V straight to the driver skips both SPIRV-Cross and
    * the driver's GLSL front end, which is most of the cost of bringing up
    * a multi-pass preset. Not every module can be expressed under the
    * OpenGL SPIR-V environment though, so this quietly falls back. */
   if (gl3_spirv_binary_supported())
   {
      /* The lowered push constant block needs a binding of its own. slang
       * only ever declares a single UBO, so one other slot always exists. */
      unsigned push_binding = (pass->reflection.ubo_binding == 0) ? 1 : 0;

      if ((pass->pipeline = gl3_spirv_link_program(
                  pass->vertex_shader,   pass->num_vertex_shader,
                  pass->fragment_shader, pass->num_fragment_shader,
                  push_binding)))
      {
         pass->spirv_binary                       = true;

         pass->locations.flat_ubo_vertex          = -1;
         pass->locations.flat_ubo_fragment        = -1;
         pass->locations.flat_push_vertex         = -1;
         pass->locations.flat_push_fragment       = -1;

         /* Block bindings in a SPIR-V shader are immutable and cannot be
          * looked up by name, so they come from pass->reflection rather than
          * glGetUniformBlockIndex(). */
         pass->locations.buffer_index_ubo_vertex  =
            (pass->reflection.ubo_size
             && (pass->reflection.ubo_stage_mask & SLANG_STAGE_VERTEX_MASK))
            ? pass->reflection.ubo_binding : GL_INVALID_INDEX;
         pass->locations.buffer_index_ubo_fragment =
            (pass->reflection.ubo_size
             && (pass->reflection.ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK))
            ? pass->reflection.ubo_binding : GL_INVALID_INDEX;

         pass->locations.buffer_index_push_vertex =
            (pass->reflection.push_constant_size
             && (pass->reflection.push_constant_stage_mask & SLANG_STAGE_VERTEX_MASK))
            ? push_binding : GL_INVALID_INDEX;
         pass->locations.buffer_index_push_fragment =
            (pass->reflection.push_constant_size
             && (pass->reflection.push_constant_stage_mask & SLANG_STAGE_FRAGMENT_MASK))
            ? push_binding : GL_INVALID_INDEX;
      }
   }

   if (!pass->pipeline)
   {
      if (!(pass->pipeline = gl3_cross_compile_program(
                  pass->vertex_shader,   pass->num_vertex_shader   * sizeof(uint32_t),
                  pass->fragment_shader, pass->num_fragment_shader * sizeof(uint32_t),
                  &pass->locations, false)))
         return false;
   }

   free(pass->uniforms);
   pass->uniforms      = NULL;
   pass->uniforms_size = 0;
   if (pass->reflection.ubo_size)
   {
      if (!(pass->uniforms = (uint8_t*)calloc(1, pass->reflection.ubo_size)))
         return false;
      pass->uniforms_size = pass->reflection.ubo_size;
   }
   if (pass->reflection.ubo_size)
      gl3_ubo_ring_init(&pass->ubo_ring, pass->reflection.ubo_size);

   free(pass->push_constant_buffer);
   pass->push_constant_buffer      = NULL;
   pass->push_constant_buffer_size = 0;
   if (pass->reflection.push_constant_size)
   {
      if (!(pass->push_constant_buffer = (uint8_t*)calloc(1,
                  pass->reflection.push_constant_size)))
         return false;
      pass->push_constant_buffer_size = pass->reflection.push_constant_size;
   }
   if (     pass->locations.buffer_index_push_vertex   != GL_INVALID_INDEX
         || pass->locations.buffer_index_push_fragment != GL_INVALID_INDEX)
   {
      /* A push constant block is packed std430, so its declared size need
       * not be a multiple of 16 the way a std140 block's is. Round up so
       * the buffer can never come up short of the block size the driver
       * derives from the SPIR-V offsets. */
      gl3_ubo_ring_init(&pass->push_ring,
            (pass->reflection.push_constant_size + 15) & ~((size_t)15));
   }

   gl3_pass_reflect_parameter(pass, "MVP", &pass->reflection.semantics[SLANG_SEMANTIC_MVP]);
   gl3_pass_reflect_parameter(pass, "OutputSize", &pass->reflection.semantics[SLANG_SEMANTIC_OUTPUT]);
   gl3_pass_reflect_parameter(pass, "FinalViewportSize", &pass->reflection.semantics[SLANG_SEMANTIC_FINAL_VIEWPORT]);
   gl3_pass_reflect_parameter(pass, "FrameCount", &pass->reflection.semantics[SLANG_SEMANTIC_FRAME_COUNT]);
   gl3_pass_reflect_parameter(pass, "FrameDirection", &pass->reflection.semantics[SLANG_SEMANTIC_FRAME_DIRECTION]);
   gl3_pass_reflect_parameter(pass, "FrameTimeDelta", &pass->reflection.semantics[SLANG_SEMANTIC_FRAME_TIME_DELTA]);
   gl3_pass_reflect_parameter(pass, "OriginalFPS", &pass->reflection.semantics[SLANG_SEMANTIC_ORIGINAL_FPS]);
   gl3_pass_reflect_parameter(pass, "Rotation", &pass->reflection.semantics[SLANG_SEMANTIC_ROTATION]);
   gl3_pass_reflect_parameter(pass, "OriginalAspect", &pass->reflection.semantics[SLANG_SEMANTIC_CORE_ASPECT]);
   gl3_pass_reflect_parameter(pass, "OriginalAspectRotated", &pass->reflection.semantics[SLANG_SEMANTIC_CORE_ASPECT_ROT]);
   gl3_pass_reflect_parameter(pass, "TotalSubFrames", &pass->reflection.semantics[SLANG_SEMANTIC_TOTAL_SUBFRAMES]);
   gl3_pass_reflect_parameter(pass, "CurrentSubFrame", &pass->reflection.semantics[SLANG_SEMANTIC_CURRENT_SUBFRAME]);
   gl3_pass_reflect_parameter(pass, "Gyroscope", &pass->reflection.semantics[SLANG_SEMANTIC_GYROSCOPE]);
   gl3_pass_reflect_parameter(pass, "Accelerometer", &pass->reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER]);
   gl3_pass_reflect_parameter(pass, "AccelerometerRest", &pass->reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER_REST]);

   {
      const slang_semantic_meta *g =
         &pass->reflection.semantics[SLANG_SEMANTIC_GYROSCOPE];
      const slang_semantic_meta *a =
         &pass->reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER];
      const slang_semantic_meta *r =
         &pass->reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER_REST];
      if (g->uniform || g->push_constant ||
          a->uniform || a->push_constant ||
          r->uniform || r->push_constant)
         input_state_get_ptr()->shader_uses_sensors = true;
   }

   gl3_pass_reflect_texture_parameter(pass, "OriginalSize",
         &pass->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL].data[0]);
   gl3_pass_reflect_texture_parameter(pass, "SourceSize",
         &pass->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_SOURCE].data[0]);
   gl3_pass_reflect_parameter_array(pass, "OriginalHistorySize", &pass->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY]);
   gl3_pass_reflect_parameter_array(pass, "PassOutputSize", &pass->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT]);
   gl3_pass_reflect_parameter_array(pass, "PassFeedbackSize", &pass->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK]);
   gl3_pass_reflect_parameter_array(pass, "UserSize", &pass->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_USER]);
   for (mi = 0; mi < pass->common->texture_semantic_uniform_map.count; mi++)
   {
      const slang_texture_semantic_map_entry *ent =
         &pass->common->texture_semantic_uniform_map.entries[mi];
      slang_texture_semantic_array *array =
         &pass->reflection.semantic_textures[ent->semantic];
      if (ent->index < array->size)
         gl3_pass_reflect_texture_parameter(pass, ent->name,
               &array->data[ent->index]);
   }

   {
      size_t f;
      for (f = 0; f < pass->num_filtered_parameters; f++)
      {
         const struct gl3_pass_parameter *m = &pass->parameters[pass->filtered_parameters[f]];
         if (m->semantic_index < pass->reflection.num_float_parameters)
            gl3_pass_reflect_parameter(pass, m->id,
                  &pass->reflection.semantic_float_parameters[m->semantic_index]);
      }
   }

   return true;
}

static void gl3_pass_set_pass_info(struct gl3_pass *pass, const gl3_filter_chain_pass_info info)
{
   pass->pass_info = info;
}

static void gl3_pass_get_output_size(struct gl3_pass *pass,
      unsigned original_width, unsigned original_height,
      unsigned source_width, unsigned source_height,
      unsigned *out_width, unsigned *out_height)
{
   float width  = 0.0f;
   float height = 0.0f;
   switch (pass->pass_info.scale_type_x)
   {
      case GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL:
         width = (float)(original_width) * pass->pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         width = (float)(source_width) * pass->pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         width = (retroarch_get_rotation() % 2 ? pass->curr_vp.height : pass->curr_vp.width) * pass->pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE:
         width = pass->pass_info.scale_x;
         break;

      default:
         break;
   }

   switch (pass->pass_info.scale_type_y)
   {
      case GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL:
         height = (float)(original_height) * pass->pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         height = (float)(source_height) * pass->pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         height = (retroarch_get_rotation() % 2 ? pass->curr_vp.width : pass->curr_vp.height) * pass->pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE:
         height = pass->pass_info.scale_y;
         break;

      default:
         break;
   }

   *out_width  = (unsigned)(roundf(width));
   *out_height = (unsigned)(roundf(height));
}

static void gl3_pass_end_frame(struct gl3_pass *pass)
{
   struct gl3_framebuffer *tmp = pass->framebuffer;
   pass->framebuffer                 = pass->framebuffer_feedback;
   pass->framebuffer_feedback        = tmp;
}

static void gl3_pass_build_semantic_vec4(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
      unsigned width, unsigned height)
{
   slang_semantic_meta *refl = (slang_semantic_meta*)
      &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
   {
      if (refl->location.ubo_vertex >= 0 || refl->location.ubo_fragment >= 0)
      {
         float v4[4];
         v4[0] = (float)(width);
         v4[1] = (float)(height);
         v4[2] = 1.0f / (float)(width);
         v4[3] = 1.0f / (float)(height);
         if (refl->location.ubo_vertex >= 0)
            glUniform4fv(refl->location.ubo_vertex, 1, v4);
         if (refl->location.ubo_fragment >= 0)
            glUniform4fv(refl->location.ubo_fragment, 1, v4);
      }
      else
      {
         float *_data = ((float *)(data + refl->ubo_offset));
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }

   if (refl->push_constant)
   {
      if (  refl->location.push_vertex   >= 0 ||
            refl->location.push_fragment >= 0)
      {
         float v4[4];
         v4[0] = (float)(width);
         v4[1] = (float)(height);
         v4[2] = 1.0f / (float)(width);
         v4[3] = 1.0f / (float)(height);
         if (refl->location.push_vertex >= 0)
            glUniform4fv(refl->location.push_vertex, 1, v4);
         if (refl->location.push_fragment >= 0)
            glUniform4fv(refl->location.push_fragment, 1, v4);
      }
      else
      {
         float *_data = (float*)
               (pass->push_constant_buffer + refl->push_constant_offset);
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }
}

static void gl3_pass_build_semantic_parameter(struct gl3_pass *pass, uint8_t *data, unsigned index, float value)
{
   slang_semantic_meta *refl = (slang_semantic_meta*)
      &pass->reflection.semantic_float_parameters[index];

   /* We will have filtered out stale pass->parameters. */
   if (data && refl->uniform)
   {
      if (refl->location.ubo_vertex >= 0 || refl->location.ubo_fragment >= 0)
      {
         if (refl->location.ubo_vertex >= 0)
            glUniform1f(refl->location.ubo_vertex, value);
         if (refl->location.ubo_fragment >= 0)
            glUniform1f(refl->location.ubo_fragment, value);
      }
      else
         *((float *)(data + refl->ubo_offset)) = value;
   }

   if (refl->push_constant)
   {
      if (refl->location.push_vertex >= 0 || refl->location.push_fragment >= 0)
      {
         if (refl->location.push_vertex >= 0)
            glUniform1f(refl->location.push_vertex, value);
         if (refl->location.push_fragment >= 0)
            glUniform1f(refl->location.push_fragment, value);
      }
      else
         *((float *)(pass->push_constant_buffer + refl->push_constant_offset)) = value;
   }
}

static void gl3_pass_build_semantic_uint(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
                               uint32_t value)
{
   slang_semantic_meta *refl = &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
   {
      if (refl->location.ubo_vertex >= 0 || refl->location.ubo_fragment >= 0)
      {
         if (refl->location.ubo_vertex >= 0)
            glUniform1ui(refl->location.ubo_vertex, value);
         if (refl->location.ubo_fragment >= 0)
            glUniform1ui(refl->location.ubo_fragment, value);
      }
      else
         *((uint32_t *)(data + pass->reflection.semantics[semantic].ubo_offset)) = value;
   }

   if (refl->push_constant)
   {
      if (refl->location.push_vertex >= 0 || refl->location.push_fragment >= 0)
      {
         if (refl->location.push_vertex >= 0)
            glUniform1ui(refl->location.push_vertex, value);
         if (refl->location.push_fragment >= 0)
            glUniform1ui(refl->location.push_fragment, value);
      }
      else
         *((uint32_t *)(pass->push_constant_buffer + refl->push_constant_offset)) = value;
   }
}

static void gl3_pass_build_semantic_int(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
                              int32_t value)
{
   slang_semantic_meta *refl = &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
   {
      if (refl->location.ubo_vertex >= 0 || refl->location.ubo_fragment >= 0)
      {
         if (refl->location.ubo_vertex >= 0)
            glUniform1i(refl->location.ubo_vertex, value);
         if (refl->location.ubo_fragment >= 0)
            glUniform1i(refl->location.ubo_fragment, value);
      }
      else
         *((int32_t *)(data + pass->reflection.semantics[semantic].ubo_offset)) = value;
   }

   if (refl->push_constant)
   {
      if (refl->location.push_vertex >= 0 || refl->location.push_fragment >= 0)
      {
         if (refl->location.push_vertex >= 0)
            glUniform1i(refl->location.push_vertex, value);
         if (refl->location.push_fragment >= 0)
            glUniform1i(refl->location.push_fragment, value);
      }
      else
         *((int32_t *)(pass->push_constant_buffer + refl->push_constant_offset)) = value;
   }
}

static void gl3_pass_build_semantic_float(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
                              float value)
{
   slang_semantic_meta *refl = &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
   {
      if (refl->location.ubo_vertex >= 0 || refl->location.ubo_fragment >= 0)
      {
         if (refl->location.ubo_vertex >= 0)
            glUniform1f(refl->location.ubo_vertex, value);
         if (refl->location.ubo_fragment >= 0)
            glUniform1f(refl->location.ubo_fragment, value);
      }
      else
         *((float *)(data + pass->reflection.semantics[semantic].ubo_offset)) = value;
   }

   if (refl->push_constant)
   {
      if (refl->location.push_vertex >= 0 || refl->location.push_fragment >= 0)
      {
         if (refl->location.push_vertex >= 0)
            glUniform1f(refl->location.push_vertex, value);
         if (refl->location.push_fragment >= 0)
            glUniform1f(refl->location.push_fragment, value);
      }
      else
         *((float *)(pass->push_constant_buffer + refl->push_constant_offset)) = value;
   }
}

static void gl3_pass_build_semantic_vec3(struct gl3_pass *pass, uint8_t *data, enum slang_semantic semantic,
                              const float *values)
{
   slang_semantic_meta *refl = &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
   {
      if (refl->location.ubo_vertex >= 0 || refl->location.ubo_fragment >= 0)
      {
         if (refl->location.ubo_vertex >= 0)
            glUniform3fv(refl->location.ubo_vertex, 1, values);
         if (refl->location.ubo_fragment >= 0)
            glUniform3fv(refl->location.ubo_fragment, 1, values);
      }
      else
         memcpy(data + refl->ubo_offset, values, 3 * sizeof(float));
   }

   if (refl->push_constant)
   {
      if (refl->location.push_vertex >= 0 || refl->location.push_fragment >= 0)
      {
         if (refl->location.push_vertex >= 0)
            glUniform3fv(refl->location.push_vertex, 1, values);
         if (refl->location.push_fragment >= 0)
            glUniform3fv(refl->location.push_fragment, 1, values);
      }
      else
         memcpy(pass->push_constant_buffer + refl->push_constant_offset, values, 3 * sizeof(float));
   }
}

static void gl3_pass_build_semantic_texture(struct gl3_pass *pass, uint8_t *buffer,
      enum slang_texture_semantic semantic, const gl3_texture_t *texture)
{
   gl3_pass_build_semantic_texture_vec4(pass, buffer, semantic,
         texture->texture.width, texture->texture.height);
   gl3_pass_set_semantic_texture(pass, semantic, texture);
}

static void gl3_pass_build_semantic_texture_array_vec4(struct gl3_pass *pass, uint8_t *data, enum slang_texture_semantic semantic,
      unsigned index, unsigned width, unsigned height)
{
   const slang_texture_semantic_array *arr =
      &pass->reflection.semantic_textures[semantic];
   const slang_texture_semantic_meta *refl;
   if (index >= arr->size)
      return;
   refl = &arr->data[index];

   if (data && refl->uniform)
   {
      if (refl->location.ubo_vertex >= 0 || refl->location.ubo_fragment >= 0)
      {
         float v4[4];
         v4[0] = (float)(width);
         v4[1] = (float)(height);
         v4[2] = 1.0f / (float)(width);
         v4[3] = 1.0f / (float)(height);
         if (refl->location.ubo_vertex >= 0)
            glUniform4fv(refl->location.ubo_vertex, 1, v4);
         if (refl->location.ubo_fragment >= 0)
            glUniform4fv(refl->location.ubo_fragment, 1, v4);
      }
      else
      {
         float *_data = ((float *)(data + refl->ubo_offset));
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }

   if (refl->push_constant)
   {
      if (refl->location.push_vertex >= 0 || refl->location.push_fragment >= 0)
      {
         float v4[4];
         v4[0] = (float)(width);
         v4[1] = (float)(height);
         v4[2] = 1.0f / (float)(width);
         v4[3] = 1.0f / (float)(height);
         if (refl->location.push_vertex >= 0)
            glUniform4fv(refl->location.push_vertex, 1, v4);
         if (refl->location.push_fragment >= 0)
            glUniform4fv(refl->location.push_fragment, 1, v4);
      }
      else
      {
         float *_data = ((float *)(pass->push_constant_buffer + refl->push_constant_offset));
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }
}

static void gl3_pass_build_semantic_texture_vec4(struct gl3_pass *pass, uint8_t *data, enum slang_texture_semantic semantic,
      unsigned width, unsigned height)
{
   gl3_pass_build_semantic_texture_array_vec4(pass, data, semantic, 0, width, height);
}

static bool gl3_pass_init_feedback(struct gl3_pass *pass)
{
   if (pass->final_pass)
      return false;

   gl3_framebuffer_delete(pass->framebuffer_feedback);
   pass->framebuffer_feedback = gl3_framebuffer_new(pass->pass_info.rt_format,
         pass->pass_info.max_levels);
   return true;
}

static void gl3_pass_free(struct gl3_pass *pass)
{
   if (!pass)
      return;

   if (pass->pipeline != 0)
      glDeleteProgram(pass->pipeline);
   /* the unique_ptrs these replaced were released by ~Pass itself */
   gl3_framebuffer_delete(pass->framebuffer);
   gl3_framebuffer_delete(pass->framebuffer_feedback);
   /* likewise ~UBORing, which ran as a member destructor */
   gl3_ubo_ring_free(&pass->ubo_ring);
   gl3_ubo_ring_free(&pass->push_ring);
   /* and the string / vector members these replaced */
   {
      size_t p;
      for (p = 0; p < pass->num_parameters; p++)
         free(pass->parameters[p].id);
   }
   free(pass->parameters);
   free(pass->filtered_parameters);
   free(pass->pass_name);
   free(pass->vertex_shader);
   free(pass->fragment_shader);
   free(pass->uniforms);
   free(pass->push_constant_buffer);
   slang_reflection_free(&pass->reflection);
   /* the destructor released the members; `delete` released the object,
    * and nothing else does now */
   free(pass);
}

static void gl3_pass_set_shader(struct gl3_pass *pass, GLenum stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   switch (stage)
   {
      case GL_VERTEX_SHADER:
         free(pass->vertex_shader);
         pass->vertex_shader     = NULL;
         pass->num_vertex_shader = 0;
         if (spirv_words &&
               (pass->vertex_shader = (uint32_t*)malloc(
                     spirv_words * sizeof(uint32_t))))
         {
            memcpy(pass->vertex_shader, spirv, spirv_words * sizeof(uint32_t));
            pass->num_vertex_shader = spirv_words;
         }
         break;
      case GL_FRAGMENT_SHADER:
         free(pass->fragment_shader);
         pass->fragment_shader     = NULL;
         pass->num_fragment_shader = 0;
         if (spirv_words &&
               (pass->fragment_shader = (uint32_t*)malloc(
                     spirv_words * sizeof(uint32_t))))
         {
            memcpy(pass->fragment_shader, spirv, spirv_words * sizeof(uint32_t));
            pass->num_fragment_shader = spirv_words;
         }
         break;
      default:
         break;
   }
}

static void gl3_pass_add_parameter(struct gl3_pass *pass, unsigned index, const char *id)
{
   struct gl3_pass_parameter *next = (struct gl3_pass_parameter*)realloc(pass->parameters,
         (pass->num_parameters + 1) * sizeof(*pass->parameters));

   if (!next)
      return;

   pass->parameters                        = next;
   pass->parameters[pass->num_parameters].id     = id ? strdup(id) : NULL;
   pass->parameters[pass->num_parameters].index  = index;
   pass->parameters[pass->num_parameters].semantic_index = (unsigned)pass->num_parameters;
   pass->num_parameters++;
}

static void gl3_pass_set_semantic_texture(struct gl3_pass *pass, enum slang_texture_semantic semantic,
      const gl3_texture_t *texture)
{
   if (pass->reflection.semantic_textures[semantic].data[0].texture)
   {
      unsigned binding = pass->reflection.semantic_textures[semantic].data[0].binding;
      glActiveTexture(GL_TEXTURE0 + binding);
      glBindTexture(GL_TEXTURE_2D, texture->texture.image);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, convert_filter_to_mag_gl(texture->filter));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, convert_filter_to_min_gl(texture->filter, texture->mip_filter));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, address_to_gl(texture->address));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, address_to_gl(texture->address));
   }
}

static void gl3_pass_build_semantic_texture_array(struct gl3_pass *pass, uint8_t *buffer,
      enum slang_texture_semantic semantic, unsigned index, const gl3_texture_t *texture)
{
   gl3_pass_build_semantic_texture_array_vec4(pass, buffer, semantic, index,
         texture->texture.width, texture->texture.height);

   if (index < pass->reflection.semantic_textures[semantic].size &&
         pass->reflection.semantic_textures[semantic].data[index].texture)
   {
      unsigned binding = pass->reflection.semantic_textures[semantic].data[index].binding;
      glActiveTexture(GL_TEXTURE0 + binding);
      glBindTexture(GL_TEXTURE_2D, texture->texture.image);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, convert_filter_to_mag_gl(texture->filter));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, convert_filter_to_min_gl(texture->filter, texture->mip_filter));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, address_to_gl(texture->address));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, address_to_gl(texture->address));
   }
}

static void gl3_pass_build_semantics(struct gl3_pass *pass, uint8_t *buffer,
      const float *mvp, const gl3_texture_t *original, const gl3_texture_t *source)
{
   unsigned i;

   /* MVP */
   if (buffer && pass->reflection.semantics[SLANG_SEMANTIC_MVP].uniform)
   {
      size_t offset = pass->reflection.semantics[
         SLANG_SEMANTIC_MVP].ubo_offset;
      if (mvp)
         memcpy(buffer + offset,
               mvp, sizeof(float) * 16);
      else
         gl3_build_default_matrix((float*)(
                  buffer + offset));
   }

   if (pass->reflection.semantics[SLANG_SEMANTIC_MVP].push_constant)
   {
      size_t offset = pass->reflection.semantics[
         SLANG_SEMANTIC_MVP].push_constant_offset;

      if (mvp)
         memcpy(pass->push_constant_buffer + offset,
               mvp, sizeof(float) * 16);
      else
         gl3_build_default_matrix((float*)(
                  pass->push_constant_buffer + offset));
   }

   /* Output information */
   gl3_pass_build_semantic_vec4(pass, buffer, SLANG_SEMANTIC_OUTPUT,
                       pass->current_framebuffer_size_width,
                       pass->current_framebuffer_size_height);
   gl3_pass_build_semantic_vec4(pass, buffer, SLANG_SEMANTIC_FINAL_VIEWPORT,
                       (unsigned)(pass->curr_vp.width),
                       (unsigned)(pass->curr_vp.height));

   gl3_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_FRAME_COUNT,
                       pass->frame_count_period
                       ? (uint32_t)(pass->frame_count % pass->frame_count_period)
                       : (uint32_t)(pass->frame_count));

   gl3_pass_build_semantic_int(pass, buffer, SLANG_SEMANTIC_FRAME_DIRECTION,
                      pass->frame_direction);

   gl3_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_FRAME_TIME_DELTA,
                      pass->frame_time_delta);

   gl3_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_ORIGINAL_FPS,
                      pass->original_fps);

   gl3_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_ROTATION,
                      pass->rotation);

   gl3_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_CORE_ASPECT,
                      pass->core_aspect);

   gl3_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_CORE_ASPECT_ROT,
                      pass->core_aspect_rot);

   gl3_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_TOTAL_SUBFRAMES,
                      pass->total_subframes);
   gl3_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_CURRENT_SUBFRAME,
                      pass->current_subframe);

   /* Sensor pass->uniforms — per-frame snapshot cached
    * by input_driver_poll() on the main thread */
   {
      input_driver_state_t *input_st = input_state_get_ptr();
      gl3_pass_build_semantic_vec3(pass, buffer, SLANG_SEMANTIC_GYROSCOPE,
                        input_st->sensor_gyroscope_cache);
      gl3_pass_build_semantic_vec3(pass, buffer, SLANG_SEMANTIC_ACCELEROMETER,
                        input_st->sensor_accelerometer_cache);
      gl3_pass_build_semantic_vec3(pass, buffer, SLANG_SEMANTIC_ACCELEROMETER_REST,
                        input_st->sensor_accelerometer_rest);
   }

   /* Standard inputs */
   gl3_pass_build_semantic_texture(pass, buffer, SLANG_TEXTURE_SEMANTIC_ORIGINAL, original);
   gl3_pass_build_semantic_texture(pass, buffer, SLANG_TEXTURE_SEMANTIC_SOURCE, source);

   /* ORIGINAL_HISTORY[0] is an alias of ORIGINAL. */
   gl3_pass_build_semantic_texture_array(pass, buffer,
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0, original);

   /* Parameters. */
   for (i = 0; i < pass->num_filtered_parameters; i++)
   {
      const struct gl3_pass_parameter *m = &pass->parameters[pass->filtered_parameters[i]];
      gl3_pass_build_semantic_parameter(pass, buffer, m->semantic_index,
            pass->common->shader_preset->parameters[m->index].current);
   }

   /* Previous inputs. */
   for (i = 0; i < pass->common->num_original_history; i++)
      gl3_pass_build_semantic_texture_array(pass, buffer,
            SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, i + 1,
            &pass->common->original_history[i]);

   /* Previous passes. */
   for (i = 0; i < pass->common->num_pass_outputs; i++)
      gl3_pass_build_semantic_texture_array(pass, buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i,
            &pass->common->pass_outputs[i]);

   /* Feedback FBOs. */
   for (i = 0; i < pass->common->num_framebuffer_feedback; i++)
      gl3_pass_build_semantic_texture_array(pass, buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i,
            &pass->common->framebuffer_feedback[i]);

   /* LUTs. */
   for (i = 0; i < pass->common->num_luts; i++)
      gl3_pass_build_semantic_texture_array(pass, buffer,
            SLANG_TEXTURE_SEMANTIC_USER, i,
            &pass->common->luts[i].texture);
}

static void gl3_pass_build_commands(struct gl3_pass *pass,
      const gl3_texture_t *original,
      const gl3_texture_t *source,
      const gl3_viewport *vp,
      const float *mvp)
{
   unsigned size_width;
      unsigned size_height;
   unsigned size_orig_width;
      unsigned size_orig_height;
   unsigned size_src_width;
      unsigned size_src_height;

   pass->curr_vp    = *vp;
   size_orig_width  = original->texture.width;
   size_orig_height = original->texture.height;
   size_src_width   = source->texture.width;
   size_src_height  = source->texture.height;
   gl3_pass_get_output_size(pass, size_orig_width, size_orig_height,
         size_src_width, size_src_height, &size_width, &size_height);

   if (pass->framebuffer &&
       (size_width  != pass->framebuffer->size_width ||
        size_height != pass->framebuffer->size_height))
      gl3_framebuffer_set_size(pass->framebuffer, size_width, size_height, 0);

   pass->current_framebuffer_size_width  = size_width;
   pass->current_framebuffer_size_height = size_height;

   glUseProgram(pass->pipeline);

   gl3_pass_build_semantics(pass, pass->uniforms, mvp, original, source);

   if (pass->locations.flat_ubo_vertex >= 0)
      glUniform4fv(pass->locations.flat_ubo_vertex,
                   (GLsizei)((pass->reflection.ubo_size + 15) / 16),
                   (const float*)pass->uniforms);

   if (pass->locations.flat_ubo_fragment >= 0)
      glUniform4fv(pass->locations.flat_ubo_fragment,
                   (GLsizei)((pass->reflection.ubo_size + 15) / 16),
                   (const float*)pass->uniforms);

   if (pass->locations.flat_push_vertex >= 0)
      glUniform4fv(pass->locations.flat_push_vertex,
                   (GLsizei)((pass->reflection.push_constant_size + 15) / 16),
                   (const float*)pass->push_constant_buffer);

   if (pass->locations.flat_push_fragment >= 0)
      glUniform4fv(pass->locations.flat_push_fragment,
                   (GLsizei)((pass->reflection.push_constant_size + 15) / 16),
                   (const float*)pass->push_constant_buffer);

   if (!(      pass->locations.buffer_index_ubo_vertex   == GL_INVALID_INDEX
            && pass->locations.buffer_index_ubo_fragment == GL_INVALID_INDEX))
   {
      /* UBO Ring - update and bind */
      unsigned vertex_binding   = pass->locations.buffer_index_ubo_vertex;
      unsigned fragment_binding = pass->locations.buffer_index_ubo_fragment;
      const void *data          = pass->uniforms;
      size_t _len               = pass->reflection.ubo_size;
      GLuint id                 = pass->ubo_ring.buffers[pass->ubo_ring.buffer_index];

      glBindBuffer(GL_UNIFORM_BUFFER, id);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, _len, data);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
      if (vertex_binding != GL_INVALID_INDEX)
         glBindBufferBase(GL_UNIFORM_BUFFER, vertex_binding, id);
      if (     fragment_binding != GL_INVALID_INDEX
            && fragment_binding != vertex_binding)
         glBindBufferBase(GL_UNIFORM_BUFFER, fragment_binding, id);

      pass->ubo_ring.buffer_index++;
      if (pass->ubo_ring.buffer_index >= pass->ubo_ring.num_buffers)
         pass->ubo_ring.buffer_index = 0;
   }

   if (!(      pass->locations.buffer_index_push_vertex   == GL_INVALID_INDEX
            && pass->locations.buffer_index_push_fragment == GL_INVALID_INDEX))
   {
      /* Push constant ring - the GL_ARB_gl_spirv path carries the push
       * constant block in a uniform buffer of its own. */
      unsigned vertex_binding   = pass->locations.buffer_index_push_vertex;
      unsigned fragment_binding = pass->locations.buffer_index_push_fragment;
      const void *data          = pass->push_constant_buffer;
      size_t _len               = pass->reflection.push_constant_size;
      GLuint id                 = pass->push_ring.buffers[pass->push_ring.buffer_index];

      glBindBuffer(GL_UNIFORM_BUFFER, id);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, _len, data);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
      if (vertex_binding != GL_INVALID_INDEX)
         glBindBufferBase(GL_UNIFORM_BUFFER, vertex_binding, id);
      if (     fragment_binding != GL_INVALID_INDEX
            && fragment_binding != vertex_binding)
         glBindBufferBase(GL_UNIFORM_BUFFER, fragment_binding, id);

      pass->push_ring.buffer_index++;
      if (pass->push_ring.buffer_index >= pass->push_ring.num_buffers)
         pass->push_ring.buffer_index = 0;
   }

   /* The final pass is always executed inside
    * another render pass since the frontend will
    * want to overlay various things on top for
    * the passes that end up on-screen. */
   if (!pass->final_pass && pass->framebuffer->complete)
   {
      glBindFramebuffer(GL_FRAMEBUFFER, pass->framebuffer->framebuffer);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
   }

#ifdef GL3_ROLLING_SCANLINE_SIMULATION
   if (pass->simulate_scanline)
   {
      glEnable(GL_SCISSOR_TEST);
   }
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

   if (pass->final_pass)
   {
      glViewport(pass->curr_vp.x, pass->curr_vp.y,
                 pass->curr_vp.width, pass->curr_vp.height);
#ifdef GL3_ROLLING_SCANLINE_SIMULATION
      if (pass->simulate_scanline)
      {
         glScissor(  pass->curr_vp.x,
                     (int32_t)(((float)(pass->curr_vp.height) / (float)(pass->total_subframes))
                              * (float)(pass->current_subframe - 1)),
                     pass->curr_vp.width,
                     (uint32_t)((float)(pass->curr_vp.height) / (float)(pass->total_subframes))
         );
      }
      else
      {
         glScissor(  pass->curr_vp.x,     pass->curr_vp.y,
                     pass->curr_vp.width, pass->curr_vp.height);
      }
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */
   }
   else
   {
      glViewport(0, 0, size_width, size_height);

#ifdef GL3_ROLLING_SCANLINE_SIMULATION
      if (pass->simulate_scanline)
      {
         glScissor(  0,
                     (int32_t)(((float)(size_height) / (float)(pass->total_subframes))
                              * (float)(pass->current_subframe - 1)),
                     size_width,
                     (uint32_t)((float)(size_height) / (float)(pass->total_subframes))
         );
      }
      else
      {
         glScissor(0, 0, size_width, size_height);
      }
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */
   }

#if !defined(HAVE_OPENGLES)
   if (pass->framebuffer && pass->framebuffer->format == GL_SRGB8_ALPHA8)
      glEnable(GL_FRAMEBUFFER_SRGB);
   else
      glDisable(GL_FRAMEBUFFER_SRGB);
#endif

   /* Draw quad */
   glDisable(GL_CULL_FACE);
   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glBindBuffer(GL_ARRAY_BUFFER, pass->common->quad_vbo);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void*)(uintptr_t)0);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void*)(uintptr_t)(2 * sizeof(float)));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
#ifdef GL3_ROLLING_SCANLINE_SIMULATION
   if (pass->simulate_scanline)
   {
      glDisable(GL_SCISSOR_TEST);
   }
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

#if !defined(HAVE_OPENGLES)
   glDisable(GL_FRAMEBUFFER_SRGB);
#endif

   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   if (!pass->final_pass)
      if (pass->framebuffer->levels > 1)
      {
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
         glBindTexture(GL_TEXTURE_2D, pass->framebuffer->image);
         glGenerateMipmap(GL_TEXTURE_2D);
         glBindTexture(GL_TEXTURE_2D, 0);
      }
}

struct gl3_filter_chain
{
   struct gl3_pass **passes;
   size_t num_passes;
   gl3_filter_chain_pass_info *pass_info;
   size_t num_pass_info;
   struct gl3_framebuffer *copy_framebuffer;
   gl3_common_resources common;
   gl3_filter_chain_texture input_texture;
   struct gl3_framebuffer **original_history;
   size_t num_original_history;
   bool require_clear;
   bool alias_initialized;
};

static void gl3_chain_update_history_info(struct gl3_filter_chain *chain);
static void gl3_chain_update_feedback_info(struct gl3_filter_chain *chain);
static void gl3_chain_build_offscreen_passes(struct gl3_filter_chain *chain, const gl3_viewport vp);
static void gl3_chain_end_frame(struct gl3_filter_chain *chain);
static void gl3_chain_build_viewport_pass(struct gl3_filter_chain *chain, const gl3_viewport vp, const float *mvp);
static bool gl3_chain_init_history(struct gl3_filter_chain *chain);
static bool gl3_chain_init_feedback(struct gl3_filter_chain *chain);
static bool gl3_chain_init_alias(struct gl3_filter_chain *chain);
static void gl3_chain_set_pass_info(struct gl3_filter_chain *chain, unsigned pass, const gl3_filter_chain_pass_info info);
static void gl3_chain_set_num_passes(struct gl3_filter_chain *chain, unsigned num_passes_);
static void gl3_chain_set_shader(struct gl3_filter_chain *chain, unsigned pass, GLenum stage, const uint32_t *spirv, size_t spirv_words);
static void gl3_chain_add_parameter(struct gl3_filter_chain *chain, unsigned pass,
      unsigned index, const char *id);
static bool gl3_chain_init(struct gl3_filter_chain *chain);
static bool gl3_chain_init_single_pass(struct gl3_filter_chain *chain, unsigned pass_idx);
static bool gl3_chain_compile_full_pass(struct gl3_filter_chain *chain, unsigned pass_idx,
      glslang_filter_chain_filter default_filter);
static bool gl3_chain_init_alias_early(struct gl3_filter_chain *chain);
static bool gl3_chain_finalize(struct gl3_filter_chain *chain);
static void gl3_chain_clear_history_and_feedback(struct gl3_filter_chain *chain);
static void gl3_chain_set_input_texture(struct gl3_filter_chain *chain, const gl3_filter_chain_texture texture);
static bool gl3_chain_add_static_texture(struct gl3_filter_chain *chain, const struct gl3_static_texture *texture);
static void gl3_chain_set_frame_count(struct gl3_filter_chain *chain, uint64_t count);
static void gl3_chain_set_frame_count_period(struct gl3_filter_chain *chain, unsigned pass, unsigned period);
static void gl3_chain_set_frame_direction(struct gl3_filter_chain *chain, int32_t direction);
static void gl3_chain_set_frame_time_delta(struct gl3_filter_chain *chain, uint32_t time_delta);
static void gl3_chain_set_original_fps(struct gl3_filter_chain *chain, float fps);
static void gl3_chain_set_rotation(struct gl3_filter_chain *chain, uint32_t rot);
static void gl3_chain_set_core_aspect(struct gl3_filter_chain *chain, float coreaspect);
static void gl3_chain_set_core_aspect_rot(struct gl3_filter_chain *chain, float coreaspectrot);
static void gl3_chain_set_shader_subframes(struct gl3_filter_chain *chain, uint32_t tot_subframes);
static void gl3_chain_set_current_shader_subframe(struct gl3_filter_chain *chain, uint32_t cur_subframe);
static void gl3_chain_set_simulate_scanline(struct gl3_filter_chain *chain, bool simulate_scanline);
static void gl3_chain_set_pass_name(struct gl3_filter_chain *chain, unsigned pass, const char *name);
static void gl3_chain_set_shader_preset(struct gl3_filter_chain *chain,
      struct video_shader *shader);
static struct video_shader *gl3_chain_get_shader_preset(
      struct gl3_filter_chain *chain);
static void gl3_chain_free(struct gl3_filter_chain *chain);

static struct gl3_filter_chain *gl3_chain_new(unsigned num_passes)
{
   struct gl3_filter_chain *chain = (struct gl3_filter_chain*)
      calloc(1, sizeof(*chain));

   if (!chain)
      return NULL;

   /* ran as the member constructor until gl3_common_resources flattened */
   gl3_common_resources_init(&chain->common);

   gl3_chain_set_num_passes(chain, num_passes);
   return chain;
}

static void gl3_chain_free(struct gl3_filter_chain *chain)
{
   size_t h;

   if (!chain)
      return;

   for (h = 0; h < chain->num_original_history; h++)
      gl3_framebuffer_delete(chain->original_history[h]);
   free(chain->original_history);
   gl3_framebuffer_delete(chain->copy_framebuffer);
   for (h = 0; h < chain->num_passes; h++)
      gl3_pass_free(chain->passes[h]);
   free(chain->passes);
   free(chain->pass_info);
   /* ran as the member destructor until gl3_common_resources flattened */
   gl3_common_resources_free(&chain->common);
   free(chain);
}

static void gl3_chain_set_shader_preset(struct gl3_filter_chain *chain,
      struct video_shader *shader)
{
   if (chain->common.shader_preset != shader)
      free(chain->common.shader_preset);
   chain->common.shader_preset = shader;
}

static struct video_shader *gl3_chain_get_shader_preset(
      struct gl3_filter_chain *chain)
{
   return chain->common.shader_preset;
}



static void gl3_chain_update_history_info(struct gl3_filter_chain *chain)
{
   unsigned i;

   for (i = 0; i < chain->num_original_history; i++)
   {
      gl3_texture_t *source = (gl3_texture_t*)
         &chain->common.original_history[i];

      if (!source)
         continue;

      source->texture.image  = chain->original_history[i]->image;
      source->texture.width  = chain->original_history[i]->size_width;
      source->texture.height = chain->original_history[i]->size_height;
      source->filter         = gl3_pass_get_source_filter(chain->passes[0]);
      source->mip_filter     = gl3_pass_get_mip_filter(chain->passes[0]);
      source->address        = gl3_pass_get_address_mode(chain->passes[0]);
   }
}

static void gl3_chain_update_feedback_info(struct gl3_filter_chain *chain)
{
   unsigned i;

   for (i = 0; i < chain->num_passes - 1; i++)
   {
      struct gl3_framebuffer *fb = gl3_pass_get_feedback_framebuffer(chain->passes[i]);
      gl3_texture_t *source;
      if (!fb)
         continue;

      source = (gl3_texture_t*)&chain->common.framebuffer_feedback[i];

      if (!source)
         continue;

      source->texture.image  = fb->image;
      source->texture.width  = fb->size_width;
      source->texture.height = fb->size_height;
      source->filter         = gl3_pass_get_source_filter(chain->passes[i]);
      source->mip_filter     = gl3_pass_get_mip_filter(chain->passes[i]);
      source->address        = gl3_pass_get_address_mode(chain->passes[i]);
   }
}

static void gl3_chain_build_offscreen_passes(struct gl3_filter_chain *chain, const gl3_viewport vp)
{
   unsigned i;

   /* First frame, make sure our history and feedback textures
    * are in a clean state. */
   gl3_texture_t original;
   gl3_texture_t source;

   if (chain->require_clear)
   {
      gl3_chain_clear_history_and_feedback(chain);
      chain->require_clear = false;
   }

   gl3_chain_update_history_info(chain);
   if (chain->common.num_framebuffer_feedback)
      gl3_chain_update_feedback_info(chain);

   original.texture    = chain->input_texture;
   original.filter     = gl3_pass_get_source_filter(chain->passes[0]);
   original.mip_filter = gl3_pass_get_mip_filter(chain->passes[0]);
   original.address    = gl3_pass_get_address_mode(chain->passes[0]);
   source              = original;

   for (i = 0; i < chain->num_passes - 1; i++)
   {
      const struct gl3_framebuffer *fb;
      gl3_pass_build_commands(chain->passes[i], &original, &source, &vp, NULL);

      fb = gl3_pass_get_framebuffer(chain->passes[i]);

      source.texture.image             = fb->image;
      source.texture.width             = fb->size_width;
      source.texture.height            = fb->size_height;
      source.filter                    = gl3_pass_get_source_filter(chain->passes[i + 1]);
      source.mip_filter                = gl3_pass_get_mip_filter(chain->passes[i + 1]);
      source.address                   = gl3_pass_get_address_mode(chain->passes[i + 1]);

      chain->common.pass_outputs[i]           = source;
   }
}

static void gl3_chain_end_frame(struct gl3_filter_chain *chain)
{
   /* If we need to keep old frames, copy it after fragment is complete.
    * TODO: We can improve pipelining by figuring out which
    * pass is the last that reads from
    * the history and dispatch the copy earlier. */
   if (chain->num_original_history)
   {
      /* Update history */
      size_t h;
      struct gl3_framebuffer *tmp =
         chain->original_history[chain->num_original_history - 1];
      chain->original_history[chain->num_original_history - 1] = NULL;

      if (chain->input_texture.width      != tmp->size_width  ||
            chain->input_texture.height     != tmp->size_height ||
            (chain->input_texture.format    != 0
             && chain->input_texture.format != tmp->format))
      {
         unsigned new_size_width;
      unsigned new_size_height;
         new_size_width  = chain->input_texture.width;
         new_size_height = chain->input_texture.height;
         gl3_framebuffer_set_size(tmp, new_size_width, new_size_height,
               chain->input_texture.format);
      }

      if (tmp->complete)
         gl3_framebuffer_copy(
               tmp->framebuffer,
               chain->common.quad_program,
               chain->common.quad_vbo,
               chain->common.quad_loc.flat_ubo_vertex,
               tmp->size_width, tmp->size_height,
               chain->input_texture.image);

      /* Should ring buffer, but we don't have *that* many chain->passes. */
      for (h = chain->num_original_history - 1; h > 0; h--)
         chain->original_history[h] = chain->original_history[h - 1];
      chain->original_history[0] = tmp;
   }
}

static void gl3_chain_build_viewport_pass(struct gl3_filter_chain *chain, const gl3_viewport vp, const float *mvp)
{
   unsigned i;
   /* First frame, make sure our history and
    * feedback textures are in a clean state. */
   gl3_texture_t source;
   gl3_texture_t original;

   if (chain->require_clear)
   {
      gl3_chain_clear_history_and_feedback(chain);
      chain->require_clear = false;
   }

   original.texture    = chain->input_texture;
   original.filter     = gl3_pass_get_source_filter(chain->passes[0]);
   original.mip_filter = gl3_pass_get_mip_filter(chain->passes[0]);
   original.address    = gl3_pass_get_address_mode(chain->passes[0]);

   if (chain->num_passes == 1)
   {
      source.texture    = chain->input_texture;
      source.filter     = gl3_pass_get_source_filter(chain->passes[chain->num_passes - 1]);
      source.mip_filter = gl3_pass_get_mip_filter(chain->passes[chain->num_passes - 1]);
      source.address    =
         gl3_pass_get_address_mode(chain->passes[chain->num_passes - 1]);
   }
   else
   {
      const struct gl3_framebuffer *fb =
         gl3_pass_get_framebuffer(chain->passes[chain->num_passes - 2]);
      source.texture.image           = fb->image;
      source.texture.width           = fb->size_width;
      source.texture.height          = fb->size_height;
      source.filter                  = gl3_pass_get_source_filter(chain->passes[chain->num_passes - 1]);
      source.mip_filter              = gl3_pass_get_mip_filter(chain->passes[chain->num_passes - 1]);
      source.address                 = gl3_pass_get_address_mode(chain->passes[chain->num_passes - 1]);
   }

   gl3_pass_build_commands(chain->passes[chain->num_passes - 1],
         &original, &source, &vp, mvp);

   /* For feedback FBOs, swap current and previous. */
   for (i = 0; i < chain->num_passes; i++)
   {
      struct gl3_framebuffer *fb = gl3_pass_get_feedback_framebuffer(chain->passes[i]);
      if (fb)
         gl3_pass_end_frame(chain->passes[i]);
   }
}

static bool gl3_chain_init_history(struct gl3_filter_chain *chain)
{
   unsigned i;
   size_t required_images = 0;

   {
      size_t h;
      for (h = 0; h < chain->num_original_history; h++)
         gl3_framebuffer_delete(chain->original_history[h]);
      free(chain->original_history);
      chain->original_history     = NULL;
      chain->num_original_history = 0;
   }
   gl3_texture_array_resize(&chain->common.original_history,
         &chain->common.num_original_history, 0);

   for (i = 0; i < chain->num_passes; i++)
   {
      size_t _y = gl3_pass_get_reflection(chain->passes[i])->semantic_textures[
                SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size;
      required_images = MAX(required_images, _y);
   }

   if (required_images < 2)
   {
      RARCH_LOG("[GLCore] Not using frame history.\n");
      return true;
   }

   /* We don't need to store array element #0,
    * since it's aliased with the actual original. */
   required_images--;
   chain->original_history = (struct gl3_framebuffer**)
      calloc(required_images, sizeof(*chain->original_history));
   if (!chain->original_history)
      return false;
   chain->num_original_history = required_images;
   if (!gl3_texture_array_resize(&chain->common.original_history,
            &chain->common.num_original_history, required_images))
      return false;

   for (i = 0; i < required_images; i++)
   {
      chain->original_history[i] = gl3_framebuffer_new(0, 1);
      if (!chain->original_history[i])
         return false;
   }

   RARCH_LOG("[GLCore] Using history of %u frames.\n", (unsigned)(required_images));

   /* On first frame, we need to clear the textures to
    * a known state, but we need
    * a command buffer for that, so just defer to first frame.
    */
   chain->require_clear = true;
   return true;
}

static bool gl3_chain_init_feedback(struct gl3_filter_chain *chain)
{
   unsigned i;
   bool use_feedbacks = false;

   gl3_texture_array_resize(&chain->common.framebuffer_feedback,
         &chain->common.num_framebuffer_feedback, 0);

   /* Final pass cannot have feedback. */
   for (i = 0; i < chain->num_passes - 1; i++)
   {
      bool use_feedback = false;
      size_t q;
      for (q = 0; q < chain->num_passes; q++)
      {
         const slang_reflection *r          =
            gl3_pass_get_reflection(chain->passes[q]);
         const slang_texture_semantic_array *feedbacks =
            &r->semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK];

         if (i < feedbacks->size && feedbacks->data[i].texture)
         {
            use_feedback  = true;
            use_feedbacks = true;
            break;
         }
      }

      if (use_feedback && !gl3_pass_init_feedback(chain->passes[i]))
         return false;

      if (use_feedback)
         RARCH_LOG("[GLCore] Using framebuffer feedback for pass #%u.\n", i);
   }

   if (!use_feedbacks)
   {
      RARCH_LOG("[GLCore] Not using framebuffer feedback.\n");
      return true;
   }

   if (!gl3_texture_array_resize(&chain->common.framebuffer_feedback,
            &chain->common.num_framebuffer_feedback, chain->num_passes - 1))
      return false;
   chain->require_clear = true;
   return true;
}

static bool gl3_chain_init_alias(struct gl3_filter_chain *chain)
{
   int i;

   slang_texture_semantic_name_map_free(&chain->common.texture_semantic_map);
   slang_texture_semantic_name_map_free(&chain->common.texture_semantic_uniform_map);

   for (i = 0; i < (int)chain->num_passes; i++)
   {
      unsigned j;
      const char *name = gl3_pass_get_name(chain->passes[i]);
      if (string_is_empty(name))
         continue;

      j = (unsigned)i;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_map, name, NULL,
               SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, j))
         return false;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_uniform_map, name, "Size",
               SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, j))
         return false;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_map, name, "Feedback",
               SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, j))
         return false;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_uniform_map, name,
               "FeedbackSize",
               SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, j))
         return false;
   }

   for (i = 0; i < (int)chain->common.num_luts; i++)
   {
      unsigned j = (unsigned)i;
      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_map,
               chain->common.luts[i].id, NULL,
               SLANG_TEXTURE_SEMANTIC_USER, j))
         return false;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_uniform_map,
               chain->common.luts[i].id, "Size",
               SLANG_TEXTURE_SEMANTIC_USER, j))
         return false;
   }

   return true;
}

static void gl3_chain_set_pass_info(struct gl3_filter_chain *chain, unsigned pass, const gl3_filter_chain_pass_info info)
{
   if (pass >= chain->num_pass_info)
   {
      /* Growth here keeps the entries already set, so this is a realloc
       * with the new tail zeroed -- not the clear-and-allocate the other
       * resizes in this file do. */
      gl3_filter_chain_pass_info *next = (gl3_filter_chain_pass_info*)
         realloc(chain->pass_info, (pass + 1) * sizeof(*chain->pass_info));
      if (!next)
         return;
      memset(next + chain->num_pass_info, 0,
            ((pass + 1) - chain->num_pass_info) * sizeof(*next));
      chain->pass_info     = next;
      chain->num_pass_info = pass + 1;
   }
   chain->pass_info[pass] = info;
}

static void gl3_chain_set_num_passes(struct gl3_filter_chain *chain, unsigned num_passes_)
{
   unsigned i;

   /* The vectors released the old generation on resize; the arrays do
    * not, so drop any previous set first. */
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_free(chain->passes[i]);
   free(chain->passes);
   chain->passes         = NULL;
   chain->num_passes     = 0;
   free(chain->pass_info);
   chain->pass_info      = NULL;
   chain->num_pass_info  = 0;

   if (!num_passes_)
      return;

   if (!(chain->pass_info = (gl3_filter_chain_pass_info*)
            calloc(num_passes_, sizeof(*chain->pass_info))))
      return;
   chain->num_pass_info  = num_passes_;

   if (!(chain->passes = (struct gl3_pass**)
            calloc(num_passes_, sizeof(*chain->passes))))
      return;

   for (i = 0; i < num_passes_; i++)
   {
      if (!(chain->passes[i] = gl3_pass_new(i + 1 == num_passes_)))
         return;
      gl3_pass_set_common_resources(chain->passes[i], &chain->common);
      gl3_pass_set_pass_number(chain->passes[i], i);
      chain->num_passes++;
   }
}

static void gl3_chain_set_shader(struct gl3_filter_chain *chain, unsigned pass, GLenum stage, const uint32_t *spirv, size_t spirv_words)
{
   gl3_pass_set_shader(chain->passes[pass], stage, spirv, spirv_words);
}

static void gl3_chain_add_parameter(struct gl3_filter_chain *chain, unsigned pass,
      unsigned index, const char *id)
{
   gl3_pass_add_parameter(chain->passes[pass], index, id);
}

static bool gl3_chain_init(struct gl3_filter_chain *chain)
{
   unsigned i;

   if (!gl3_chain_init_alias(chain))
      return false;
   chain->alias_initialized = true;

   for (i = 0; i < chain->num_passes; i++)
   {
      RARCH_LOG("[GLCore] Building pass #%u (%s)\n", i,
            string_is_empty(gl3_pass_get_name(chain->passes[i])) ?
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) :
            gl3_pass_get_name(chain->passes[i]));

      gl3_pass_set_pass_info(chain->passes[i], chain->pass_info[i]);
      if (!gl3_pass_build(chain->passes[i]))
         return false;
   }

   chain->require_clear = false;
   if (!gl3_chain_init_history(chain))
      return false;
   if (!gl3_chain_init_feedback(chain))
      return false;
   if (!gl3_texture_array_resize(&chain->common.pass_outputs,
            &chain->common.num_pass_outputs, chain->num_passes))
      return false;
   return true;
}

static bool gl3_chain_init_single_pass(struct gl3_filter_chain *chain, unsigned pass_idx)
{
   if (pass_idx >= chain->num_passes)
      return false;

   RARCH_LOG("[GLCore] Building pass #%u (%s)\n", pass_idx,
         string_is_empty(gl3_pass_get_name(chain->passes[pass_idx])) ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) :
         gl3_pass_get_name(chain->passes[pass_idx]));

   gl3_pass_set_pass_info(chain->passes[pass_idx], chain->pass_info[pass_idx]);
   if (!gl3_pass_build(chain->passes[pass_idx]))
      return false;

   return true;
}

static bool gl3_chain_compile_full_pass(struct gl3_filter_chain *chain, unsigned pass_idx,
      glslang_filter_chain_filter default_filter)
{
   size_t j;
   glslang_output output;
   struct gl3_filter_chain_pass_info p_info;
   bool explicit_format;
   struct video_shader_parameter *itr;
   const struct video_shader_pass *pass;
   const struct video_shader_pass *next_pass;
   struct video_shader *shader = chain->common.shader_preset;

   if (!shader || pass_idx >= chain->num_passes)
      return false;

   /* For the extra opaque pass appended when last_pass_is_fbo,
    * the SPIRV was already set in create_deferred — just build. */
   if (pass_idx >= shader->passes)
      return gl3_chain_init_single_pass(chain, pass_idx);

   pass      = &shader->pass[pass_idx];
   next_pass =
      pass_idx + 1 < shader->passes
      ? &shader->pass[pass_idx + 1] : NULL;

   /* ---- SPIRV cross-compile (CPU) ---- */
   if (!glslang_compile_shader(pass->source.path, &output))
   {
      RARCH_ERR("[GLCore] Failed to compile shader: \"%s\".\n",
            pass->source.path);
      return false;
   }

   /* ---- Extract parameters ---- */
   for (j = 0; j < output.meta.num_parameters; j++)
   {
      const glslang_parameter *meta_param = &output.meta.parameters[j];

      if (shader->num_parameters >= GFX_MAX_PARAMETERS)
      {
         RARCH_ERR("[GLCore] Exceeded maximum number of parameters (%u).\n",
               GFX_MAX_PARAMETERS);
         glslang_output_free(&output);
         return false;
      }

      itr = NULL;
      {
         unsigned k;
         size_t mid_len = strlen(meta_param->id);
         for (k = 0; k < shader->num_parameters; k++)
         {
            /* Gate the memcmp behind two byte loads; the scan is
             * O(n^2) across Mega Bezel-scale parameter counts. */
            const char *sid = shader->parameters[k].id;
            if (sid[0] == meta_param->id[0] && sid[mid_len] == '\0'
                  && !memcmp(sid, meta_param->id, mid_len))
            {
               itr = &shader->parameters[k];
               break;
            }
         }
      }

      if (itr)
      {
         if (   strcmp(meta_param->desc, itr->desc)
             || meta_param->initial != itr->initial
             || meta_param->minimum != itr->minimum
             || meta_param->maximum != itr->maximum
             || meta_param->step    != itr->step)
         {
            RARCH_ERR("[GLCore] Duplicate parameters found for \"%s\","
                  " but arguments do not match.\n", itr->id);
            glslang_output_free(&output);
            return false;
         }
         gl3_chain_add_parameter(chain, pass_idx,
               (unsigned)(itr - shader->parameters), meta_param->id);
      }
      else
      {
         struct video_shader_parameter *param =
            &shader->parameters[shader->num_parameters];
         strlcpy(param->id, meta_param->id, sizeof(param->id));
         strlcpy(param->desc, meta_param->desc, sizeof(param->desc));
         param->initial = meta_param->initial;
         param->minimum = meta_param->minimum;
         param->maximum = meta_param->maximum;
         param->step    = meta_param->step;
         gl3_chain_add_parameter(chain, pass_idx, shader->num_parameters, meta_param->id);
         shader->num_parameters++;
      }
   }

   /* ---- Set SPIRV on the pass ---- */
   gl3_chain_set_shader(chain, pass_idx, GL_VERTEX_SHADER,
         output.vertex, output.vertex_len);
   gl3_chain_set_shader(chain, pass_idx, GL_FRAGMENT_SHADER,
         output.fragment, output.fragment_len);

   gl3_chain_set_frame_count_period(chain, pass_idx, pass->frame_count_mod);

   /* ---- Pass name (from shader #pragma or preset alias) ---- */
   if (output.meta.name[0])
      gl3_chain_set_pass_name(chain, pass_idx, output.meta.name);
   if (*pass->alias)
      gl3_chain_set_pass_name(chain, pass_idx, pass->alias);

   /* Update the alias map so later chain->passes can reference this one.
    * Re-running init_alias is safe — it clears and repopulates. */
   if (!string_is_empty(gl3_pass_get_name(chain->passes[pass_idx])))
   {
      chain->alias_initialized = false;
      if (!gl3_chain_init_alias_early(chain))
      {
         glslang_output_free(&output);
         return false;
      }
   }

   /* ---- Pass info (scale, filter, format) ---- */
   p_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
   p_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
   p_info.scale_x       = 0.0f;
   p_info.scale_y       = 0.0f;
   p_info.rt_format     = 0;
   p_info.source_filter = GLSLANG_FILTER_CHAIN_LINEAR;
   p_info.mip_filter    = GLSLANG_FILTER_CHAIN_LINEAR;
   p_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_REPEAT;
   p_info.max_levels    = 0;

   if (pass->filter == RARCH_FILTER_UNSPEC)
      p_info.source_filter = default_filter;
   else
   {
      p_info.source_filter =
         pass->filter == RARCH_FILTER_LINEAR
         ? GLSLANG_FILTER_CHAIN_LINEAR
         : GLSLANG_FILTER_CHAIN_NEAREST;
   }
   p_info.address    = rarch_wrap_to_address(pass->wrap);
   p_info.max_levels = 1;

   if (next_pass && next_pass->mipmap)
      p_info.max_levels = ~0u;

   p_info.mip_filter = pass->filter != RARCH_FILTER_NEAREST
      && p_info.max_levels > 1
      ? GLSLANG_FILTER_CHAIN_LINEAR
      : GLSLANG_FILTER_CHAIN_NEAREST;

   explicit_format = output.meta.rt_format != SLANG_FORMAT_UNKNOWN;

   if (output.meta.rt_format == SLANG_FORMAT_UNKNOWN)
      output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_UNORM;

   if (!(pass->fbo.flags & FBO_SCALE_FLAG_VALID))
   {
      bool scale_viewport = pass_idx + 1 == shader->passes;
      if (scale_viewport)
      {
         p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
         p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
      }
      else
      {
         p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
         p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
      }
      p_info.scale_x = 1.0f;
      p_info.scale_y = 1.0f;

      if (scale_viewport)
      {
         p_info.rt_format = 0;
         if (explicit_format)
            RARCH_WARN("[GLCore] Using explicit format for last pass in chain,"
                  " but it is not rendered to framebuffer,"
                  " using swapchain format instead.\n");
      }
      else
         p_info.rt_format =
            convert_glslang_format(output.meta.rt_format);
   }
   else
   {
      if (pass->fbo.flags & FBO_SCALE_FLAG_SRGB_FBO)
         output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_SRGB;
      else if (pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
         output.meta.rt_format = SLANG_FORMAT_R16G16B16A16_SFLOAT;
      else if (pass->fbo.flags & FBO_SCALE_FLAG_RGB10_FBO)
         output.meta.rt_format = SLANG_FORMAT_A2B10G10R10_UNORM_PACK32;

      p_info.rt_format =
         convert_glslang_format(output.meta.rt_format);

      switch (pass->fbo.type_x)
      {
         case RARCH_SCALE_INPUT:
            p_info.scale_x      = pass->fbo.scale_x;
            p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
            break;
         case RARCH_SCALE_ABSOLUTE:
            p_info.scale_x      = (float)(pass->fbo.abs_x);
            p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE;
            break;
         case RARCH_SCALE_VIEWPORT:
            p_info.scale_x      = pass->fbo.scale_x;
            p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
            break;
      }

      switch (pass->fbo.type_y)
      {
         case RARCH_SCALE_INPUT:
            p_info.scale_y      = pass->fbo.scale_y;
            p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
            break;
         case RARCH_SCALE_ABSOLUTE:
            p_info.scale_y      = (float)(pass->fbo.abs_y);
            p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE;
            break;
         case RARCH_SCALE_VIEWPORT:
            p_info.scale_y      = pass->fbo.scale_y;
            p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
            break;
      }
   }

   gl3_chain_set_pass_info(chain, pass_idx, p_info);
   glslang_output_free(&output);

   /* ---- GPU compile/link (the expensive GL part) ---- */
   return gl3_chain_init_single_pass(chain, pass_idx);
}

static bool gl3_chain_init_alias_early(struct gl3_filter_chain *chain)
{
   if (chain->alias_initialized)
      return true;
   if (!gl3_chain_init_alias(chain))
      return false;
   chain->alias_initialized = true;
   return true;
}

static bool gl3_chain_finalize(struct gl3_filter_chain *chain)
{
   /* init_alias may have been called early for deferred loading;
    * skip it if already done. */
   if (!chain->alias_initialized)
   {
      if (!gl3_chain_init_alias(chain))
         return false;
      chain->alias_initialized = true;
   }

   chain->require_clear = false;
   if (!gl3_chain_init_history(chain))
      return false;
   if (!gl3_chain_init_feedback(chain))
      return false;
   if (!gl3_texture_array_resize(&chain->common.pass_outputs,
            &chain->common.num_pass_outputs, chain->num_passes))
      return false;
   return true;
}

static void gl3_chain_clear_history_and_feedback(struct gl3_filter_chain *chain)
{
   unsigned i;
   for (i = 0; i < chain->num_original_history; i++)
   {
      if (chain->original_history[i]->complete)
      {
         GLuint id = chain->original_history[i]->framebuffer;
         glBindFramebuffer(GL_FRAMEBUFFER, id);
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
         glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
         glClear(GL_COLOR_BUFFER_BIT);
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }
   }
   for (i = 0; i < chain->num_passes; i++)
   {
      struct gl3_framebuffer *fb = gl3_pass_get_feedback_framebuffer(chain->passes[i]);
      if (fb && fb->complete)
      {
         GLuint id = fb->framebuffer;
         glBindFramebuffer(GL_FRAMEBUFFER, id);
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
         glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
         glClear(GL_COLOR_BUFFER_BIT);
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }
   }
}

static void gl3_chain_set_input_texture(struct gl3_filter_chain *chain, const gl3_filter_chain_texture texture)
{
   chain->input_texture = texture;

   /* Need a copy to remove padding.
    * GL HW render interface in libretro is kinda garbage now ... */
   if (chain->input_texture.padded_width  != chain->input_texture.width ||
       chain->input_texture.padded_height != chain->input_texture.height)
   {
      if (!chain->copy_framebuffer)
         chain->copy_framebuffer = gl3_framebuffer_new(texture.format, 1);
      if (!chain->copy_framebuffer)
         return;

      if (chain->input_texture.width   != chain->copy_framebuffer->size_width  ||
          chain->input_texture.height  != chain->copy_framebuffer->size_height ||
          (chain->input_texture.format != 0                                   &&
           chain->input_texture.format != chain->copy_framebuffer->format))
      {
         unsigned copy_size_width;
      unsigned copy_size_height;
         copy_size_width  = chain->input_texture.width;
         copy_size_height = chain->input_texture.height;
         gl3_framebuffer_set_size(chain->copy_framebuffer,
               copy_size_width, copy_size_height,
               chain->input_texture.format);
      }

      if (chain->copy_framebuffer->complete)
         gl3_framebuffer_copy_partial(
               chain->copy_framebuffer->framebuffer,
               chain->common.quad_program,
               chain->common.quad_loc.flat_ubo_vertex,
               chain->copy_framebuffer->size_width,
               chain->copy_framebuffer->size_height,
               chain->input_texture.image,
               (float)(chain->input_texture.width)
               / chain->input_texture.padded_width,
               (float)(chain->input_texture.height)
               / chain->input_texture.padded_height);
      chain->input_texture.image = chain->copy_framebuffer->image;
   }
}

static bool gl3_chain_add_static_texture(struct gl3_filter_chain *chain, const struct gl3_static_texture *texture)
{
   struct gl3_static_texture *next =
      (struct gl3_static_texture*)realloc(chain->common.luts,
            (chain->common.num_luts + 1) * sizeof(*chain->common.luts));
   if (!next)
      return false;
   chain->common.luts                  = next;
   chain->common.luts[chain->common.num_luts] = *texture;
   chain->common.num_luts++;
   return true;
}

static void gl3_chain_set_frame_count(struct gl3_filter_chain *chain, uint64_t count)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_frame_count(chain->passes[i], count);
}

static void gl3_chain_set_frame_count_period(struct gl3_filter_chain *chain, unsigned pass, unsigned period)
{
   gl3_pass_set_frame_count_period(chain->passes[pass], period);
}

static void gl3_chain_set_frame_direction(struct gl3_filter_chain *chain, int32_t direction)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_frame_direction(chain->passes[i], direction);
}

static void gl3_chain_set_frame_time_delta(struct gl3_filter_chain *chain, uint32_t time_delta)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_frame_time_delta(chain->passes[i], time_delta);
}

static void gl3_chain_set_original_fps(struct gl3_filter_chain *chain, float fps)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_original_fps(chain->passes[i], fps);
}

static void gl3_chain_set_rotation(struct gl3_filter_chain *chain, uint32_t rot)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_rotation(chain->passes[i], rot);
}

static void gl3_chain_set_core_aspect(struct gl3_filter_chain *chain, float coreaspect)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_core_aspect(chain->passes[i], coreaspect);
}

static void gl3_chain_set_core_aspect_rot(struct gl3_filter_chain *chain, float coreaspectrot)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_core_aspect_rot(chain->passes[i], coreaspectrot);
}


static void gl3_chain_set_shader_subframes(struct gl3_filter_chain *chain, uint32_t tot_subframes)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_shader_subframes(chain->passes[i], tot_subframes);
}

static void gl3_chain_set_current_shader_subframe(struct gl3_filter_chain *chain, uint32_t cur_subframe)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_current_shader_subframe(chain->passes[i], cur_subframe);
}

#ifdef GL3_ROLLING_SCANLINE_SIMULATION
static void gl3_chain_set_simulate_scanline(struct gl3_filter_chain *chain, bool simulate_scanline)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      gl3_pass_set_simulate_scanline(chain->passes[i], simulate_scanline);
}
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

static void gl3_chain_set_pass_name(struct gl3_filter_chain *chain, unsigned pass, const char *name)
{
   gl3_pass_set_name(chain->passes[pass], name);
}

static bool gl3_filter_chain_load_lut(
      struct gl3_filter_chain *chain,
      const struct video_shader_lut *shader,
      struct gl3_static_texture *out)
{
   struct texture_image image;
   unsigned levels;
   GLuint tex                      = 0;

   image.width                     = 0;
   image.height                    = 0;
   image.pixels                    = NULL;
   image.supports_rgba             = true;

   if (!image_texture_load(&image, shader->path))
      return false;

   levels = shader->mipmap ? glslang_num_miplevels(image.width, image.height) : 1;

   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexStorage2D(GL_TEXTURE_2D, levels,
                  GL_RGBA8, image.width, image.height);

   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                   image.width, image.height,
                   GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);

   if (levels > 1)
      glGenerateMipmap(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, 0);

   if (image.pixels)
      image_texture_free(&image);

   gl3_static_texture_init(out, shader->id,
         tex, image.width, image.height,
         shader->filter != RARCH_FILTER_NEAREST,
         levels > 1,
         rarch_wrap_to_address(shader->wrap));
   return true;
}

static bool gl3_filter_chain_load_luts(
      struct gl3_filter_chain *chain,
      struct video_shader *shader)
{
   unsigned i;
   for (i = 0; i < shader->luts; i++)
   {
      struct gl3_static_texture image;
      if (!gl3_filter_chain_load_lut(chain, &shader->lut[i], &image))
      {
         RARCH_ERR("[GLCore] Failed to load LUT \"%s\".\n", shader->lut[i].path);
         return false;
      }

      if (!gl3_chain_add_static_texture(chain, &image))
      {
         gl3_static_texture_free(&image);
         return false;
      }
   }

   return true;
}

gl3_filter_chain_t *gl3_filter_chain_create_default(
      glslang_filter_chain_filter filter)
{
   struct gl3_filter_chain_pass_info pass_info;

   struct gl3_filter_chain *chain = gl3_chain_new(1);
   if (!chain)
      return NULL;

   pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_x       = 1.0f;
   pass_info.scale_y       = 1.0f;
   pass_info.rt_format     = 0;
   pass_info.source_filter = filter;
   pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_NEAREST;
   pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;
   pass_info.max_levels    = 0;

   gl3_chain_set_pass_info(chain, 0, pass_info);

   gl3_chain_set_shader(chain, 0, GL_VERTEX_SHADER,
         gl3_opaque_vert,
         sizeof(gl3_opaque_vert) / sizeof(uint32_t));
   gl3_chain_set_shader(chain, 0, GL_FRAGMENT_SHADER,
         gl3_opaque_frag,
         sizeof(gl3_opaque_frag) / sizeof(uint32_t));

   if (!gl3_chain_init(chain))
   {
      gl3_chain_free(chain);
      return NULL;
   }

   return chain;
}

gl3_filter_chain_t *gl3_filter_chain_create_from_preset(
      const char *path, glslang_filter_chain_filter filter)
{
   size_t j;
   unsigned i;
   unsigned total_passes;
   bool last_pass_is_fbo;
   bool explicit_format;
   struct video_shader_parameter *itr;
   struct gl3_filter_chain *chain;
   struct video_shader *shader = (struct video_shader*)calloc(1, sizeof(*shader));
   if (!shader)
      return NULL;

   if (!video_shader_load_preset_into_shader(path, shader))
   {
      free(shader);
      return NULL;
   }

   last_pass_is_fbo = shader->pass[shader->passes - 1].fbo.flags &
      FBO_SCALE_FLAG_VALID;

   chain = gl3_chain_new(shader->passes + (last_pass_is_fbo ? 1 : 0));
   if (!chain)
      {
         free(shader);
         gl3_chain_free(chain);
         return NULL;
      }

   if (      shader->luts
         && !gl3_filter_chain_load_luts(chain, shader))
      {
         free(shader);
         gl3_chain_free(chain);
         return NULL;
      }

   shader->num_parameters = 0;

   /* One include cache for every pass of this preset.  The passes share
    * helper .inc files, so without this each pass re-reads them: a
    * 24-pass preset over 8 shared helpers issues 216 reads for 32
    * distinct files.  The guard frees it on every exit from here,
    * including the error paths below. */
   {
   /* The C++ guard freed this on every exit; in C each exit below frees
    * it explicitly, next to the chain and preset it already released. */
   void *include_cache = glslang_include_cache_new();

   for (i = 0; i < shader->passes; i++)
   {
      glslang_output output;
      struct gl3_filter_chain_pass_info pass_info;
      const struct video_shader_pass *pass      = &shader->pass[i];
      const struct video_shader_pass *next_pass =
         i + 1 < shader->passes ? &shader->pass[i + 1] : NULL;

      pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
      pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
      pass_info.scale_x       = 0.0f;
      pass_info.scale_y       = 0.0f;
      pass_info.rt_format     = 0;
      pass_info.source_filter = GLSLANG_FILTER_CHAIN_LINEAR;
      pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_LINEAR;
      pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_REPEAT;
      pass_info.max_levels    = 0;

      if (!glslang_compile_shader_cached(pass->source.path, &output,
               include_cache))
      {
         RARCH_ERR("[GLCore] Failed to compile shader: \"%s\".\n",
               pass->source.path);
         {
         free(shader);
         gl3_chain_free(chain);
         glslang_include_cache_free(include_cache);
         return NULL;
      }
      }

      for (j = 0; j < output.meta.num_parameters; j++)
      {
         const glslang_parameter *meta_param = &output.meta.parameters[j];

         if (shader->num_parameters >= GFX_MAX_PARAMETERS)
         {
            RARCH_ERR("[GLCore] Exceeded maximum number of parameters (%u).\n", GFX_MAX_PARAMETERS);
            glslang_output_free(&output);
            {
         free(shader);
         gl3_chain_free(chain);
         glslang_include_cache_free(include_cache);
         return NULL;
      }
         }

         itr = NULL;
         {
            unsigned k;
            {
               /* Gated memcmp: O(n^2) across Mega Bezel-scale
                * parameter counts. */
               size_t mid_len = strlen(meta_param->id);
               for (k = 0; k < shader->num_parameters; k++)
               {
                  const char *sid = shader->parameters[k].id;
                  if (sid[0] == meta_param->id[0] && sid[mid_len] == '\0'
                        && !memcmp(sid, meta_param->id, mid_len))
                  {
                     itr = &shader->parameters[k];
                     break;
                  }
               }
            }
         }

         if (itr)
         {
            /* Allow duplicate #pragma parameter, but
             * only if they are exactly the same. */
            if (   strcmp(meta_param->desc, itr->desc)
                || meta_param->initial != itr->initial
                || meta_param->minimum != itr->minimum
                || meta_param->maximum != itr->maximum
                || meta_param->step    != itr->step)
            {
               RARCH_ERR("[GLCore] Duplicate parameters found for \"%s\", but arguments do not match.\n",
                     itr->id);
               glslang_output_free(&output);
               {
         free(shader);
         gl3_chain_free(chain);
         glslang_include_cache_free(include_cache);
         return NULL;
      }
            }
            gl3_chain_add_parameter(chain, i, (unsigned)(itr - shader->parameters), meta_param->id);
         }
         else
         {
            struct video_shader_parameter *param = &shader->parameters[shader->num_parameters];
            strlcpy(param->id, meta_param->id, sizeof(param->id));
            strlcpy(param->desc, meta_param->desc, sizeof(param->desc));
            param->initial = meta_param->initial;
            param->minimum = meta_param->minimum;
            param->maximum = meta_param->maximum;
            param->step    = meta_param->step;
            gl3_chain_add_parameter(chain, i, shader->num_parameters, meta_param->id);
            shader->num_parameters++;
         }
      }

      gl3_chain_set_shader(chain, i,
            GL_VERTEX_SHADER,
            output.vertex,
            output.vertex_len);

      gl3_chain_set_shader(chain, i,
            GL_FRAGMENT_SHADER,
            output.fragment,
            output.fragment_len);

      gl3_chain_set_frame_count_period(chain, i, pass->frame_count_mod);

      if (output.meta.name[0])
         gl3_chain_set_pass_name(chain, i, output.meta.name);

      /* Preset overrides. */
      if (*pass->alias)
         gl3_chain_set_pass_name(chain, i, pass->alias);

      if (pass->filter == RARCH_FILTER_UNSPEC)
         pass_info.source_filter = filter;
      else
      {
         pass_info.source_filter =
            pass->filter == RARCH_FILTER_LINEAR
            ? GLSLANG_FILTER_CHAIN_LINEAR
            : GLSLANG_FILTER_CHAIN_NEAREST;
      }
      pass_info.address       = rarch_wrap_to_address(pass->wrap);
      pass_info.max_levels    = 1;

      /* TODO: Expose max_levels in slangp.
       * CGP format is a bit awkward in that it uses mipmap_input,
       * so we much check if next pass needs the mipmapping.
       */
      if (next_pass && next_pass->mipmap)
         pass_info.max_levels = ~0u;

      pass_info.mip_filter    = pass->filter != RARCH_FILTER_NEAREST && pass_info.max_levels > 1
         ? GLSLANG_FILTER_CHAIN_LINEAR
         : GLSLANG_FILTER_CHAIN_NEAREST;

      explicit_format = output.meta.rt_format != SLANG_FORMAT_UNKNOWN;

      /* Set a reasonable default. */
      if (output.meta.rt_format == SLANG_FORMAT_UNKNOWN)
         output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_UNORM;

      if (!(pass->fbo.flags & FBO_SCALE_FLAG_VALID))
      {
         bool scale_viewport       = i + 1 == shader->passes;
         if (scale_viewport)
         {
            pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
            pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
         }
         else
         {
            pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
            pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
         }
         pass_info.scale_x         = 1.0f;
         pass_info.scale_y         = 1.0f;

         if (scale_viewport)
         {
            pass_info.rt_format    = 0;

            if (explicit_format)
               RARCH_WARN("[GLCore] Using explicit format for last pass in chain,"
                     " but it is not rendered to framebuffer, using swapchain format instead.\n");
         }
         else
         {
            pass_info.rt_format = convert_glslang_format(output.meta.rt_format);
            RARCH_LOG("[GLCore] Using render target format %s for pass output #%u.\n",
                  glslang_format_to_string(output.meta.rt_format), i);
         }
      }
      else
      {
         /* Preset overrides shader.
          * Kinda ugly ... */
         if (pass->fbo.flags & FBO_SCALE_FLAG_SRGB_FBO)
            output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_SRGB;
         else if (pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
            output.meta.rt_format = SLANG_FORMAT_R16G16B16A16_SFLOAT;
         else if (pass->fbo.flags & FBO_SCALE_FLAG_RGB10_FBO)
            output.meta.rt_format = SLANG_FORMAT_A2B10G10R10_UNORM_PACK32;

         pass_info.rt_format = convert_glslang_format(output.meta.rt_format);
         RARCH_LOG("[GLCore] Using render target format %s for pass output #%u.\n",
               glslang_format_to_string(output.meta.rt_format), i);

         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               pass_info.scale_x      = pass->fbo.scale_x;
               pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
               break;

            case RARCH_SCALE_ABSOLUTE:
               pass_info.scale_x      = (float)(pass->fbo.abs_x);
               pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE;
               break;

            case RARCH_SCALE_VIEWPORT:
               pass_info.scale_x      = pass->fbo.scale_x;
               pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
               break;
         }

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               pass_info.scale_y      = pass->fbo.scale_y;
               pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
               break;

            case RARCH_SCALE_ABSOLUTE:
               pass_info.scale_y      = (float)(pass->fbo.abs_y);
               pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE;
               break;

            case RARCH_SCALE_VIEWPORT:
               pass_info.scale_y      = pass->fbo.scale_y;
               pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
               break;
         }
      }

      gl3_chain_set_pass_info(chain, i, pass_info);
      glslang_output_free(&output);
   }
   glslang_include_cache_free(include_cache);
   }   /* include cache scope: freed here, and on any early return above */

   if (last_pass_is_fbo)
   {
      struct gl3_filter_chain_pass_info pass_info;

      pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_x       = 1.0f;
      pass_info.scale_y       = 1.0f;

      pass_info.rt_format     = 0;

      pass_info.source_filter = filter;
      pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_NEAREST;
      pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;

      pass_info.max_levels    = 0;

      gl3_chain_set_pass_info(chain, shader->passes, pass_info);

      gl3_chain_set_shader(chain, shader->passes,
            GL_VERTEX_SHADER,
            gl3_opaque_vert,
            sizeof(gl3_opaque_vert) / sizeof(uint32_t));

      gl3_chain_set_shader(chain, shader->passes,
            GL_FRAGMENT_SHADER,
            gl3_opaque_frag,
            sizeof(gl3_opaque_frag) / sizeof(uint32_t));
   }

   gl3_chain_set_shader_preset(chain, shader);
   shader = NULL;   /* the chain owns it now */

   if (!gl3_chain_init(chain))
   {
      gl3_chain_free(chain);
      return NULL;
   }

   return chain;
}

/* ---- Deferred (per-frame) filter chain construction ---- */

gl3_filter_chain_t *gl3_filter_chain_create_deferred(
      const char *path,
      glslang_filter_chain_filter filter,
      unsigned *out_num_passes)
{
   unsigned i;
   unsigned total_passes;
   bool last_pass_is_fbo;
   bool explicit_format;
   struct video_shader_parameter *itr;
   struct gl3_filter_chain *chain;
   struct video_shader *shader = (struct video_shader*)calloc(1, sizeof(*shader));
   if (!shader)
      return NULL;

   if (!video_shader_load_preset_into_shader(path, shader))
   {
      free(shader);
      return NULL;
   }

   last_pass_is_fbo = shader->pass[shader->passes - 1].fbo.flags &
      FBO_SCALE_FLAG_VALID;

   total_passes = shader->passes + (last_pass_is_fbo ? 1 : 0);
   chain = gl3_chain_new(total_passes);
   if (!chain)
      {
         free(shader);
         gl3_chain_free(chain);
         return NULL;
      }

   if (      shader->luts
         && !gl3_filter_chain_load_luts(chain, shader))
      {
         free(shader);
         gl3_chain_free(chain);
         return NULL;
      }

   shader->num_parameters = 0;

   /* Set pass names from preset aliases only (no SPIRV needed).
    * Names from #pragma in shader source will be set during
    * compile_full_pass. */
   for (i = 0; i < shader->passes; i++)
   {
      if (*shader->pass[i].alias)
         gl3_chain_set_pass_name(chain, i, shader->pass[i].alias);
   }

   if (last_pass_is_fbo)
   {
      struct gl3_filter_chain_pass_info pass_info;

      pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_x       = 1.0f;
      pass_info.scale_y       = 1.0f;
      pass_info.rt_format     = 0;
      pass_info.source_filter = filter;
      pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_NEAREST;
      pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;
      pass_info.max_levels    = 0;

      gl3_chain_set_pass_info(chain, shader->passes, pass_info);

      gl3_chain_set_shader(chain, shader->passes,
            GL_VERTEX_SHADER,
            gl3_opaque_vert,
            sizeof(gl3_opaque_vert) / sizeof(uint32_t));

      gl3_chain_set_shader(chain, shader->passes,
            GL_FRAGMENT_SHADER,
            gl3_opaque_frag,
            sizeof(gl3_opaque_frag) / sizeof(uint32_t));
   }

   gl3_chain_set_shader_preset(chain, shader);
   shader = NULL;   /* the chain owns it now */

   /* Populate the alias map with preset-defined aliases.
    * compile_full_pass() will incrementally update the map
    * when shaders add names via #pragma name. */
   if (!gl3_chain_init_alias_early(chain))
   {
      RARCH_ERR("[GLCore] Deferred: failed to initialize alias map.\n");
      {
         free(shader);
         gl3_chain_free(chain);
         return NULL;
      }
   }

   /* NOTE: We do NOT call gl3_chain_init(chain) here.
    * Passes are compiled one per frame via
    * gl3_filter_chain_compile_pass / gl3_filter_chain_finalize. */

   if (out_num_passes)
      *out_num_passes = total_passes;

   return chain;
}

bool gl3_filter_chain_compile_pass(
      gl3_filter_chain_t *chain,
      unsigned pass_index,
      glslang_filter_chain_filter filter)
{
   return gl3_chain_compile_full_pass(chain, pass_index, filter);
}

bool gl3_filter_chain_finalize(gl3_filter_chain_t *chain)
{
   return gl3_chain_finalize(chain);
}

struct video_shader *gl3_filter_chain_get_preset(
      gl3_filter_chain_t *chain) { return gl3_chain_get_shader_preset(chain); }
void gl3_filter_chain_free(gl3_filter_chain_t *chain)
{
   gl3_chain_free(chain);
   input_state_get_ptr()->shader_uses_sensors = false;
}

void gl3_filter_chain_set_shader(
      gl3_filter_chain_t *chain,
      unsigned pass,
      GLenum shader_stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   gl3_chain_set_shader(chain, pass, shader_stage, spirv, spirv_words);
}

void gl3_filter_chain_set_pass_info(
      gl3_filter_chain_t *chain,
      unsigned pass,
      const struct gl3_filter_chain_pass_info *info)
{
   gl3_chain_set_pass_info(chain, pass, *info);
}

bool gl3_filter_chain_init(gl3_filter_chain_t *chain)
{
   return gl3_chain_init(chain);
}

void gl3_filter_chain_set_input_texture(
      gl3_filter_chain_t *chain,
      const struct gl3_filter_chain_texture *texture)
{
   gl3_chain_set_input_texture(chain, *texture);
}

void gl3_filter_chain_set_frame_count(
      gl3_filter_chain_t *chain,
      uint64_t count)
{
   gl3_chain_set_frame_count(chain, count);
}

void gl3_filter_chain_set_frame_direction(
      gl3_filter_chain_t *chain,
      int32_t direction)
{
   gl3_chain_set_frame_direction(chain, direction);
}

void gl3_filter_chain_set_frame_time_delta(
      gl3_filter_chain_t *chain,
      uint32_t time_delta)
{
   gl3_chain_set_frame_time_delta(chain, time_delta);
}

void gl3_filter_chain_set_original_fps(
      gl3_filter_chain_t *chain,
      float fps)
{
   gl3_chain_set_original_fps(chain, fps);
}

void gl3_filter_chain_set_rotation(
      gl3_filter_chain_t *chain,
      uint32_t rot)
{
   gl3_chain_set_rotation(chain, rot);
}

void gl3_filter_chain_set_core_aspect(
      gl3_filter_chain_t *chain,
      float coreaspect)
{
   gl3_chain_set_core_aspect(chain, coreaspect);
}

void gl3_filter_chain_set_core_aspect_rot(
      gl3_filter_chain_t *chain,
      float coreaspectrot)
{
   gl3_chain_set_core_aspect_rot(chain, coreaspectrot);
}

void gl3_filter_chain_set_shader_subframes(
      gl3_filter_chain_t *chain,
      uint32_t tot_subframes)
{
   gl3_chain_set_shader_subframes(chain, tot_subframes);
}

void gl3_filter_chain_set_current_shader_subframe(
      gl3_filter_chain_t *chain,
      uint32_t cur_subframe)
{
   gl3_chain_set_current_shader_subframe(chain, cur_subframe);
}

#ifdef GL3_ROLLING_SCANLINE_SIMULATION
void gl3_filter_chain_set_simulate_scanline(
      gl3_filter_chain_t *chain,
      bool simulate_scanline)
{
   gl3_chain_set_simulate_scanline(chain, simulate_scanline);
}
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

void gl3_filter_chain_set_frame_count_period(
      gl3_filter_chain_t *chain,
      unsigned pass,
      unsigned period)
{
   gl3_chain_set_frame_count_period(chain, pass, period);
}

void gl3_filter_chain_set_pass_name(
      gl3_filter_chain_t *chain,
      unsigned pass,
      const char *name)
{
   gl3_chain_set_pass_name(chain, pass, name);
}

void gl3_filter_chain_build_offscreen_passes(
      gl3_filter_chain_t *chain,
      const gl3_viewport *vp)
{
   gl3_chain_build_offscreen_passes(chain, *vp);
}

void gl3_filter_chain_build_viewport_pass(
      gl3_filter_chain_t *chain,
      const gl3_viewport *vp, const float *mvp)
{
   gl3_chain_build_viewport_pass(chain, *vp, mvp);
}

void gl3_filter_chain_end_frame(gl3_filter_chain_t *chain)
{
   gl3_chain_end_frame(chain);
}
