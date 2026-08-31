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

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <utility>
#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <string/stdstring.h>
#include <formats/image.h>
#include <retro_miscellaneous.h>

#include "slang_process.h"
#include "spirv_opengl.h"
#include <spirv_cross_c.h>

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
         struct Size2D size,
         GLuint image);

   void gl3_framebuffer_copy_partial(
         GLuint fb_id,
         GLuint quad_program,
         GLint flat_ubo_vertex,
         struct Size2D size,
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

namespace gl3_shader
{
static const uint32_t opaque_vert[] =
#include "../drivers/vulkan_shaders/opaque.vert.inc"
;

static const uint32_t opaque_frag[] =
#include "../drivers/vulkan_shaders/opaque.frag.inc"
;

struct Texture
{
   gl3_filter_chain_texture texture;
   glslang_filter_chain_filter filter;
   glslang_filter_chain_filter mip_filter;
   glslang_filter_chain_address address;
};

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
   Texture texture;
};

static void gl3_static_texture_init(struct gl3_static_texture *tex,
      const char *id_, GLuint image_,
      unsigned width, unsigned height, bool linear, bool mipmap,
      glslang_filter_chain_address address)
{
   GLenum gl_address         = address_to_gl(address);
   Texture &texture          = tex->texture;

   tex->id                   = id_ ? strdup(id_) : NULL;
   tex->image                = image_;

   texture.filter            = GLSLANG_FILTER_CHAIN_NEAREST;
   texture.mip_filter        = GLSLANG_FILTER_CHAIN_NEAREST;
   texture.address           = address;
   texture.texture.width     = width;
   texture.texture.height    = height;
   texture.texture.format    = 0;
   texture.texture.image     = image_;

   if (linear)
   {
      texture.filter         = GLSLANG_FILTER_CHAIN_LINEAR;
      if (mipmap)
         texture.mip_filter  = GLSLANG_FILTER_CHAIN_LINEAR;
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

struct CommonResources
{
   CommonResources();
   ~CommonResources();

   Texture *original_history        = NULL;
   size_t num_original_history      = 0;
   Texture *framebuffer_feedback    = NULL;
   size_t num_framebuffer_feedback  = 0;
   Texture *pass_outputs            = NULL;
   size_t num_pass_outputs          = 0;
   struct gl3_static_texture *luts = NULL;
   size_t num_luts                 = 0;

   slang_texture_semantic_name_map texture_semantic_map        = {};
   slang_texture_semantic_name_map texture_semantic_uniform_map = {};
   video_shader *shader_preset = NULL;

   GLuint quad_program = 0;
   GLuint quad_vbo = 0;
   gl3_buffer_locations quad_loc = {};
};

CommonResources::CommonResources()
{
   static float quad_data[] = {
      0.0f, 0.0f, 0.0f, 0.0f,
      1.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
   };

   glGenBuffers(1, &quad_vbo);
   glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(quad_data), quad_data, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   quad_program = gl3_cross_compile_program(
         opaque_vert, sizeof(opaque_vert),
         opaque_frag, sizeof(opaque_frag), &quad_loc, true);
}

CommonResources::~CommonResources()
{
   size_t i;
   /* The unique_ptr vector used to free these on destruction; the plain
    * array does not, so every LUT has to be released by hand. */
   for (i = 0; i < num_luts; i++)
      gl3_static_texture_free(&luts[i]);
   free(luts);
   luts     = NULL;
   num_luts = 0;
   /* the three Texture vectors and the shader_preset unique_ptr were
    * released by the implicit member destructors */
   free(original_history);
   free(framebuffer_feedback);
   free(pass_outputs);
   original_history       = NULL;
   framebuffer_feedback   = NULL;
   pass_outputs           = NULL;
   num_original_history   = 0;
   num_framebuffer_feedback = 0;
   num_pass_outputs       = 0;
   delete shader_preset;
   shader_preset          = NULL;
   slang_texture_semantic_name_map_free(&texture_semantic_map);
   slang_texture_semantic_name_map_free(&texture_semantic_uniform_map);
   if (quad_program != 0)
      glDeleteProgram(quad_program);
   if (quad_vbo != 0)
      glDeleteBuffers(1, &quad_vbo);
}

struct gl3_framebuffer
{
   GLuint image;
   Size2D size;
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

   fb->size.width  = 1;
   fb->size.height = 1;
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
      const Size2D *size_, GLenum format_)
{
   fb->size = *size_;
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

   if (fb->size.width == 0)
      fb->size.width = 1;
   if (fb->size.height == 0)
      fb->size.height = 1;

   fb->levels = glslang_num_miplevels(fb->size.width, fb->size.height);
   if (fb->max_levels < fb->levels)
      fb->levels = fb->max_levels;
   if (fb->levels == 0)
      fb->levels = 1;

   glTexStorage2D(GL_TEXTURE_2D, fb->levels,
                  fb->format,
                  fb->size.width, fb->size.height);

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

               levels = glslang_num_miplevels(fb->size.width, fb->size.height);
               if (fb->max_levels < levels)
                  levels = fb->max_levels;
               glTexStorage2D(GL_TEXTURE_2D, levels,
                     GL_RGBA8,
                     fb->size.width, fb->size.height);
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

struct gl3_ubo_ring
{
   GLuint buffers[GL3_UBO_RING_SIZE];
   unsigned num_buffers  = 0;
   unsigned buffer_index = 0;
};

/* Every resize below follows a clear and every element is written before it
 * is read, so allocating fresh zeroed storage matches what vector::resize
 * did. */
static bool gl3_texture_array_resize(Texture **arr, size_t *count,
      size_t want)
{
   Texture *next;
   free(*arr);
   *arr   = NULL;
   *count = 0;
   if (!want)
      return true;
   if (!(next = (Texture*)calloc(want, sizeof(*next))))
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

class Pass
{
public:
   explicit Pass(bool final_pass) :
         final_pass(final_pass)
   {}

   ~Pass();

   Pass(Pass&&) = delete;
   void operator=(Pass&&) = delete;

   const struct gl3_framebuffer *get_framebuffer() const
   {
      return framebuffer;
   }

   struct gl3_framebuffer *get_feedback_framebuffer()
   {
      return framebuffer_feedback;
   }

   void set_pass_info(const gl3_filter_chain_pass_info &info);

   void set_shader(GLenum stage,
                   const uint32_t *spirv,
                   size_t spirv_words);

   bool build();
   bool init_feedback();

   void build_commands(
         const Texture &original,
         const Texture &source,
         const gl3_viewport &vp,
         const float *mvp);

   void set_frame_count(uint64_t count)
   {
      frame_count = count;
   }

   void set_frame_count_period(unsigned period)
   {
      frame_count_period = period;
   }

   void set_frame_direction(int32_t direction)
   {
      frame_direction = direction;
   }

   void set_frame_time_delta(uint32_t time_delta)
   {
      frame_time_delta = time_delta;
   }

   void set_original_fps(float fps)
   {
      original_fps = fps;
   }

   void set_rotation(uint32_t rot)
   {
      rotation = rot;
   }

   void set_core_aspect(float coreaspect)
   {
      core_aspect = coreaspect;
   }

   void set_core_aspect_rot(float coreaspectrot)
   {
      core_aspect_rot = coreaspectrot;
   }

   void set_shader_subframes(uint32_t tot_subframes)
   {
      total_subframes = tot_subframes;
   }

   void set_current_shader_subframe(uint32_t cur_subframe)
   {
      current_subframe = cur_subframe;
   }

#ifdef GL3_ROLLING_SCANLINE_SIMULATION
   void set_simulate_scanline(bool simulate)
   {
      simulate_scanline = simulate;
   }
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

   void set_name(const char *name)
   {
      free(pass_name);
      pass_name = name ? strdup(name) : NULL;
   }

   const char *get_name() const
   {
      return pass_name ? pass_name : "";
   }

   glslang_filter_chain_filter get_source_filter() const
   {
      return pass_info.source_filter;
   }

   glslang_filter_chain_filter get_mip_filter() const
   {
      return pass_info.mip_filter;
   }

   glslang_filter_chain_address get_address_mode() const
   {
      return pass_info.address;
   }

   void set_common_resources(CommonResources *common)
   {
      this->common = common;
   }

   const slang_reflection &get_reflection() const
   {
      return reflection;
   }

   void set_pass_number(unsigned pass)
   {
      pass_number = pass;
   }

   void add_parameter(unsigned parameter_index, const char *id);

   void end_frame();
   void allocate_buffers();

private:
   bool final_pass;

   Size2D get_output_size(const Size2D &original_size,
                          const Size2D &max_source) const;

   GLuint pipeline                 = 0;
   CommonResources *common         = nullptr;

   Size2D current_framebuffer_size = {};
   gl3_viewport curr_vp;
   gl3_filter_chain_pass_info pass_info;

   uint32_t *vertex_shader        = NULL;
   size_t num_vertex_shader       = 0;
   uint32_t *fragment_shader      = NULL;
   size_t num_fragment_shader     = 0;
   struct gl3_framebuffer *framebuffer          = NULL;
   struct gl3_framebuffer *framebuffer_feedback = NULL;

   bool init_pipeline();

   void set_semantic_texture(slang_texture_semantic semantic,
         const Texture &texture);

   /* Plain C struct: must be explicitly zero-initialized, since the
       * first build() and a teardown before any build() both run
       * slang_reflection_free() on it.  The previous C++ type had a
       * default constructor doing this implicitly. */
      slang_reflection reflection = {};

   uint8_t *uniforms              = NULL;
   size_t uniforms_size           = 0;

   void build_semantics(uint8_t *buffer,
                        const float *mvp,
                        const Texture &original, const Texture &source);
   void build_semantic_vec4(uint8_t *data, slang_semantic semantic,
                            unsigned width, unsigned height);
   void build_semantic_uint(uint8_t *data,
         slang_semantic semantic, uint32_t value);
   void build_semantic_int(uint8_t *data,
         slang_semantic semantic, int32_t value);
   void build_semantic_float(uint8_t *data,
         slang_semantic semantic, float value);
   void build_semantic_vec3(uint8_t *data,
         slang_semantic semantic, const float *values);
   void build_semantic_parameter(uint8_t *data, unsigned index, float value);
   void build_semantic_texture_vec4(uint8_t *data,
         slang_texture_semantic semantic,
         unsigned width, unsigned height);
   void build_semantic_texture_array_vec4(uint8_t *data,
         slang_texture_semantic semantic, unsigned index,
         unsigned width, unsigned height);
   void build_semantic_texture(uint8_t *buffer,
         slang_texture_semantic semantic, const Texture &texture);
   void build_semantic_texture_array(uint8_t *buffer,
         slang_texture_semantic semantic,
         unsigned index, const Texture &texture);

   uint64_t frame_count = 0;
   unsigned frame_count_period = 0;
   int32_t frame_direction = 1;
   uint32_t frame_time_delta = 0;
   float original_fps = 0;
   uint32_t rotation = 0;
   float core_aspect = 0;
   float core_aspect_rot = 0;
   unsigned pass_number = 0;
   uint32_t total_subframes = 1;
   uint32_t current_subframe = 1;
#ifdef GL3_ROLLING_SCANLINE_SIMULATION
   bool simulate_scanline = false;
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

   size_t ubo_offset = 0;
   char *pass_name   = NULL;   /* owned */

   struct Parameter
   {
      char *id;                /* owned */
      unsigned index;
      unsigned semantic_index;
   };

   struct Parameter *parameters      = NULL;
   size_t num_parameters             = 0;
   /* Indices into parameters[]; the vector of copies this replaces stayed
    * valid across a push_back reallocation, and indices do too. */
   unsigned *filtered_parameters     = NULL;
   size_t num_filtered_parameters    = 0;
   uint8_t *push_constant_buffer     = NULL;
   size_t push_constant_buffer_size  = 0;
   gl3_buffer_locations locations = {};
   struct gl3_ubo_ring ubo_ring;
   /* Only allocated on the GL_ARB_gl_spirv path, where the push constant
    * block becomes a second uniform buffer. */
   struct gl3_ubo_ring push_ring;
   /* SPIR-V modules carry no name reflection, so glGetUniformLocation()
    * cannot be used to find individual block members. */
   bool spirv_binary = false;

   void reflect_parameter(const char *name, slang_semantic_meta &meta);
   void reflect_parameter(const char *name, slang_texture_semantic_meta &meta);
   void reflect_parameter_array(const char *name, slang_texture_semantic_array &meta);
};

bool Pass::build()
{
   slang_semantic_name_map semantic_map = {};
   unsigned i;
   unsigned j = 0;

   gl3_framebuffer_delete(framebuffer);
   framebuffer          = NULL;
   gl3_framebuffer_delete(framebuffer_feedback);
   framebuffer_feedback = NULL;

   if (!final_pass)
      framebuffer = gl3_framebuffer_new(pass_info.rt_format,
            pass_info.max_levels);

   for (i = 0; i < num_parameters; i++)
   {
      if (!slang_semantic_name_map_set_unique(
               &semantic_map, parameters[i].id, NULL,
               SLANG_SEMANTIC_FLOAT_PARAMETER, j))
      {
         slang_semantic_name_map_free(&semantic_map);
         return false;
      }
      j++;
   }

   slang_reflection_free(&reflection);
   if (!slang_reflection_init(&reflection))
   {
      slang_semantic_name_map_free(&semantic_map);
      return false;
   }
   reflection.pass_number                  = pass_number;
   reflection.texture_semantic_map         = &common->texture_semantic_map;
   reflection.texture_semantic_uniform_map = &common->texture_semantic_uniform_map;
   reflection.semantic_map                 = &semantic_map;

   {
      bool refl_ok = slang_reflect_spirv(
            vertex_shader, num_vertex_shader,
            fragment_shader, num_fragment_shader,
            &reflection);
      /* The parameter map is only needed during reflection; the
       * reflection keeps a dangling pointer otherwise. */
      slang_semantic_name_map_free(&semantic_map);
      reflection.semantic_map = NULL;
      if (!refl_ok)
         return false;
   }

   /* Filter out parameters which we will never use anyways. */
   num_filtered_parameters = 0;

   for (i = 0; i < reflection.num_float_parameters; i++)
   {
      if (reflection.semantic_float_parameters[i].uniform ||
          reflection.semantic_float_parameters[i].push_constant)
      {
         unsigned *next = (unsigned*)realloc(filtered_parameters,
               (num_filtered_parameters + 1) * sizeof(*filtered_parameters));
         if (!next)
            return false;
         filtered_parameters                          = next;
         filtered_parameters[num_filtered_parameters] = i;
         num_filtered_parameters++;
      }
   }

   if (!init_pipeline())
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

void Pass::reflect_parameter(const char *name, slang_semantic_meta &meta)
{
   if (spirv_binary)
      return;

   if (meta.uniform)
   {
      int vert = gl3_uniform_location_prefixed(pipeline,
            "RARCH_UBO_VERTEX_INSTANCE.", name);
      int frag = gl3_uniform_location_prefixed(pipeline,
            "RARCH_UBO_FRAGMENT_INSTANCE.", name);

      if (vert >= 0)
         meta.location.ubo_vertex = vert;
      if (frag >= 0)
         meta.location.ubo_fragment = frag;
   }

   if (meta.push_constant)
   {
      int vert = gl3_uniform_location_prefixed(pipeline,
            "RARCH_PUSH_VERTEX_INSTANCE.", name);
      int frag = gl3_uniform_location_prefixed(pipeline,
            "RARCH_PUSH_FRAGMENT_INSTANCE.", name);

      if (vert >= 0)
         meta.location.push_vertex = vert;
      if (frag >= 0)
         meta.location.push_fragment = frag;
   }
}

void Pass::reflect_parameter(const char *name, slang_texture_semantic_meta &meta)
{
   if (spirv_binary)
      return;

   if (meta.uniform)
   {
      int vert = gl3_uniform_location_prefixed(pipeline,
            "RARCH_UBO_VERTEX_INSTANCE.", name);
      int frag = gl3_uniform_location_prefixed(pipeline,
            "RARCH_UBO_FRAGMENT_INSTANCE.", name);

      if (vert >= 0)
         meta.location.ubo_vertex = vert;
      if (frag >= 0)
         meta.location.ubo_fragment = frag;
   }

   if (meta.push_constant)
   {
      int vert = gl3_uniform_location_prefixed(pipeline,
            "RARCH_PUSH_VERTEX_INSTANCE.", name);
      int frag = gl3_uniform_location_prefixed(pipeline,
            "RARCH_PUSH_FRAGMENT_INSTANCE.", name);

      if (vert >= 0)
         meta.location.push_vertex = vert;
      if (frag >= 0)
         meta.location.push_fragment = frag;
   }
}

void Pass::reflect_parameter_array(const char *name, slang_texture_semantic_array &meta)
{
   size_t i;

   if (spirv_binary)
      return;

   for (i = 0; i < meta.size; i++)
   {
      char n[128];
      size_t _len = strlcpy(n, name, sizeof(n));
      snprintf(n + _len, sizeof(n) - _len, "%u", (unsigned)i);
      slang_texture_semantic_meta *m = &meta.data[i];

      if (m->uniform)
      {
         int vert, frag;
         char vert_n[256];
         char frag_n[256];
         size_t _len  = strlcpy_lit(vert_n, "RARCH_UBO_VERTEX_INSTANCE.",   sizeof(vert_n));
         size_t _len2 = strlcpy_lit(frag_n, "RARCH_UBO_FRAGMENT_INSTANCE.", sizeof(frag_n));
         strlcpy(vert_n + _len,  n, sizeof(vert_n) - _len);
         strlcpy(frag_n + _len2, n, sizeof(frag_n) - _len2);
         vert = glGetUniformLocation(pipeline, vert_n);
         frag = glGetUniformLocation(pipeline, frag_n);

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
         vert = glGetUniformLocation(pipeline, vert_n);
         frag = glGetUniformLocation(pipeline, frag_n);

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

bool Pass::init_pipeline()
{
   /* Handing the SPIR-V straight to the driver skips both SPIRV-Cross and
    * the driver's GLSL front end, which is most of the cost of bringing up
    * a multi-pass preset. Not every module can be expressed under the
    * OpenGL SPIR-V environment though, so this quietly falls back. */
   if (gl3_spirv_binary_supported())
   {
      /* The lowered push constant block needs a binding of its own. slang
       * only ever declares a single UBO, so one other slot always exists. */
      unsigned push_binding = (reflection.ubo_binding == 0) ? 1 : 0;

      if ((pipeline = gl3_spirv_link_program(
                  vertex_shader,   num_vertex_shader,
                  fragment_shader, num_fragment_shader,
                  push_binding)))
      {
         spirv_binary                       = true;

         locations.flat_ubo_vertex          = -1;
         locations.flat_ubo_fragment        = -1;
         locations.flat_push_vertex         = -1;
         locations.flat_push_fragment       = -1;

         /* Block bindings in a SPIR-V shader are immutable and cannot be
          * looked up by name, so they come from reflection rather than
          * glGetUniformBlockIndex(). */
         locations.buffer_index_ubo_vertex  =
            (reflection.ubo_size
             && (reflection.ubo_stage_mask & SLANG_STAGE_VERTEX_MASK))
            ? reflection.ubo_binding : GL_INVALID_INDEX;
         locations.buffer_index_ubo_fragment =
            (reflection.ubo_size
             && (reflection.ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK))
            ? reflection.ubo_binding : GL_INVALID_INDEX;

         locations.buffer_index_push_vertex =
            (reflection.push_constant_size
             && (reflection.push_constant_stage_mask & SLANG_STAGE_VERTEX_MASK))
            ? push_binding : GL_INVALID_INDEX;
         locations.buffer_index_push_fragment =
            (reflection.push_constant_size
             && (reflection.push_constant_stage_mask & SLANG_STAGE_FRAGMENT_MASK))
            ? push_binding : GL_INVALID_INDEX;
      }
   }

   if (!pipeline)
   {
      if (!(pipeline = gl3_cross_compile_program(
                  vertex_shader,   num_vertex_shader   * sizeof(uint32_t),
                  fragment_shader, num_fragment_shader * sizeof(uint32_t),
                  &locations, false)))
         return false;
   }

   free(uniforms);
   uniforms      = NULL;
   uniforms_size = 0;
   if (reflection.ubo_size)
   {
      if (!(uniforms = (uint8_t*)calloc(1, reflection.ubo_size)))
         return false;
      uniforms_size = reflection.ubo_size;
   }
   if (reflection.ubo_size)
      gl3_ubo_ring_init(&ubo_ring, reflection.ubo_size);

   free(push_constant_buffer);
   push_constant_buffer      = NULL;
   push_constant_buffer_size = 0;
   if (reflection.push_constant_size)
   {
      if (!(push_constant_buffer = (uint8_t*)calloc(1,
                  reflection.push_constant_size)))
         return false;
      push_constant_buffer_size = reflection.push_constant_size;
   }
   if (     locations.buffer_index_push_vertex   != GL_INVALID_INDEX
         || locations.buffer_index_push_fragment != GL_INVALID_INDEX)
   {
      /* A push constant block is packed std430, so its declared size need
       * not be a multiple of 16 the way a std140 block's is. Round up so
       * the buffer can never come up short of the block size the driver
       * derives from the SPIR-V offsets. */
      gl3_ubo_ring_init(&push_ring,
            (reflection.push_constant_size + 15) & ~((size_t)15));
   }

   reflect_parameter("MVP", reflection.semantics[SLANG_SEMANTIC_MVP]);
   reflect_parameter("OutputSize", reflection.semantics[SLANG_SEMANTIC_OUTPUT]);
   reflect_parameter("FinalViewportSize", reflection.semantics[SLANG_SEMANTIC_FINAL_VIEWPORT]);
   reflect_parameter("FrameCount", reflection.semantics[SLANG_SEMANTIC_FRAME_COUNT]);
   reflect_parameter("FrameDirection", reflection.semantics[SLANG_SEMANTIC_FRAME_DIRECTION]);
   reflect_parameter("FrameTimeDelta", reflection.semantics[SLANG_SEMANTIC_FRAME_TIME_DELTA]);
   reflect_parameter("OriginalFPS", reflection.semantics[SLANG_SEMANTIC_ORIGINAL_FPS]);
   reflect_parameter("Rotation", reflection.semantics[SLANG_SEMANTIC_ROTATION]);
   reflect_parameter("OriginalAspect", reflection.semantics[SLANG_SEMANTIC_CORE_ASPECT]);
   reflect_parameter("OriginalAspectRotated", reflection.semantics[SLANG_SEMANTIC_CORE_ASPECT_ROT]);
   reflect_parameter("TotalSubFrames", reflection.semantics[SLANG_SEMANTIC_TOTAL_SUBFRAMES]);
   reflect_parameter("CurrentSubFrame", reflection.semantics[SLANG_SEMANTIC_CURRENT_SUBFRAME]);
   reflect_parameter("Gyroscope", reflection.semantics[SLANG_SEMANTIC_GYROSCOPE]);
   reflect_parameter("Accelerometer", reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER]);
   reflect_parameter("AccelerometerRest", reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER_REST]);

   {
      slang_semantic_meta &g = reflection.semantics[SLANG_SEMANTIC_GYROSCOPE];
      slang_semantic_meta &a = reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER];
      slang_semantic_meta &r = reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER_REST];
      if (g.uniform || g.push_constant ||
          a.uniform || a.push_constant ||
          r.uniform || r.push_constant)
         input_state_get_ptr()->shader_uses_sensors = true;
   }

   reflect_parameter("OriginalSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL].data[0]);
   reflect_parameter("SourceSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_SOURCE].data[0]);
   reflect_parameter_array("OriginalHistorySize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY]);
   reflect_parameter_array("PassOutputSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT]);
   reflect_parameter_array("PassFeedbackSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK]);
   reflect_parameter_array("UserSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_USER]);
   for (size_t mi = 0; mi < common->texture_semantic_uniform_map.count; mi++)
   {
      const slang_texture_semantic_map_entry *ent =
         &common->texture_semantic_uniform_map.entries[mi];
      slang_texture_semantic_array &array =
         reflection.semantic_textures[ent->semantic];
      if (ent->index < array.size)
         reflect_parameter(ent->name, array.data[ent->index]);
   }

   {
      size_t f;
      for (f = 0; f < num_filtered_parameters; f++)
      {
         const struct Parameter *m = &parameters[filtered_parameters[f]];
         if (m->semantic_index < reflection.num_float_parameters)
            reflect_parameter(m->id,
                  reflection.semantic_float_parameters[m->semantic_index]);
      }
   }

   return true;
}

void Pass::set_pass_info(const gl3_filter_chain_pass_info &info)
{
   pass_info = info;
}

Size2D Pass::get_output_size(const Size2D &original,
      const Size2D &source) const
{
   float width  = 0.0f;
   float height = 0.0f;
   switch (pass_info.scale_type_x)
   {
      case GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL:
         width = float(original.width) * pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         width = float(source.width) * pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         width = (retroarch_get_rotation() % 2 ? curr_vp.height : curr_vp.width) * pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE:
         width = pass_info.scale_x;
         break;

      default:
         break;
   }

   switch (pass_info.scale_type_y)
   {
      case GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL:
         height = float(original.height) * pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         height = float(source.height) * pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         height = (retroarch_get_rotation() % 2 ? curr_vp.width : curr_vp.height) * pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE:
         height = pass_info.scale_y;
         break;

      default:
         break;
   }

   return { unsigned(roundf(width)), unsigned(roundf(height)) };
}

void Pass::end_frame()
{
   struct gl3_framebuffer *tmp = framebuffer;
   framebuffer                 = framebuffer_feedback;
   framebuffer_feedback        = tmp;
}

void Pass::build_semantic_vec4(uint8_t *data, slang_semantic semantic,
      unsigned width, unsigned height)
{
   slang_semantic_meta *refl = (slang_semantic_meta*)
      &reflection.semantics[semantic];

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
         float *_data = reinterpret_cast<float *>(data + refl->ubo_offset);
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
         float *_data = reinterpret_cast<float *>
               (push_constant_buffer + refl->push_constant_offset);
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }
}

void Pass::build_semantic_parameter(uint8_t *data, unsigned index, float value)
{
   slang_semantic_meta *refl = (slang_semantic_meta*)
      &reflection.semantic_float_parameters[index];

   /* We will have filtered out stale parameters. */
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
         *reinterpret_cast<float *>(data + refl->ubo_offset) = value;
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
         *reinterpret_cast<float *>(push_constant_buffer + refl->push_constant_offset) = value;
   }
}

void Pass::build_semantic_uint(uint8_t *data, slang_semantic semantic,
                               uint32_t value)
{
   slang_semantic_meta &refl = reflection.semantics[semantic];

   if (data && refl.uniform)
   {
      if (refl.location.ubo_vertex >= 0 || refl.location.ubo_fragment >= 0)
      {
         if (refl.location.ubo_vertex >= 0)
            glUniform1ui(refl.location.ubo_vertex, value);
         if (refl.location.ubo_fragment >= 0)
            glUniform1ui(refl.location.ubo_fragment, value);
      }
      else
         *reinterpret_cast<uint32_t *>(data + reflection.semantics[semantic].ubo_offset) = value;
   }

   if (refl.push_constant)
   {
      if (refl.location.push_vertex >= 0 || refl.location.push_fragment >= 0)
      {
         if (refl.location.push_vertex >= 0)
            glUniform1ui(refl.location.push_vertex, value);
         if (refl.location.push_fragment >= 0)
            glUniform1ui(refl.location.push_fragment, value);
      }
      else
         *reinterpret_cast<uint32_t *>(push_constant_buffer + refl.push_constant_offset) = value;
   }
}

void Pass::build_semantic_int(uint8_t *data, slang_semantic semantic,
                              int32_t value)
{
   slang_semantic_meta &refl = reflection.semantics[semantic];

   if (data && refl.uniform)
   {
      if (refl.location.ubo_vertex >= 0 || refl.location.ubo_fragment >= 0)
      {
         if (refl.location.ubo_vertex >= 0)
            glUniform1i(refl.location.ubo_vertex, value);
         if (refl.location.ubo_fragment >= 0)
            glUniform1i(refl.location.ubo_fragment, value);
      }
      else
         *reinterpret_cast<int32_t *>(data + reflection.semantics[semantic].ubo_offset) = value;
   }

   if (refl.push_constant)
   {
      if (refl.location.push_vertex >= 0 || refl.location.push_fragment >= 0)
      {
         if (refl.location.push_vertex >= 0)
            glUniform1i(refl.location.push_vertex, value);
         if (refl.location.push_fragment >= 0)
            glUniform1i(refl.location.push_fragment, value);
      }
      else
         *reinterpret_cast<int32_t *>(push_constant_buffer + refl.push_constant_offset) = value;
   }
}

void Pass::build_semantic_float(uint8_t *data, slang_semantic semantic,
                              float value)
{
   slang_semantic_meta &refl = reflection.semantics[semantic];

   if (data && refl.uniform)
   {
      if (refl.location.ubo_vertex >= 0 || refl.location.ubo_fragment >= 0)
      {
         if (refl.location.ubo_vertex >= 0)
            glUniform1f(refl.location.ubo_vertex, value);
         if (refl.location.ubo_fragment >= 0)
            glUniform1f(refl.location.ubo_fragment, value);
      }
      else
         *reinterpret_cast<float *>(data + reflection.semantics[semantic].ubo_offset) = value;
   }

   if (refl.push_constant)
   {
      if (refl.location.push_vertex >= 0 || refl.location.push_fragment >= 0)
      {
         if (refl.location.push_vertex >= 0)
            glUniform1f(refl.location.push_vertex, value);
         if (refl.location.push_fragment >= 0)
            glUniform1f(refl.location.push_fragment, value);
      }
      else
         *reinterpret_cast<float *>(push_constant_buffer + refl.push_constant_offset) = value;
   }
}

void Pass::build_semantic_vec3(uint8_t *data, slang_semantic semantic,
                              const float *values)
{
   slang_semantic_meta &refl = reflection.semantics[semantic];

   if (data && refl.uniform)
   {
      if (refl.location.ubo_vertex >= 0 || refl.location.ubo_fragment >= 0)
      {
         if (refl.location.ubo_vertex >= 0)
            glUniform3fv(refl.location.ubo_vertex, 1, values);
         if (refl.location.ubo_fragment >= 0)
            glUniform3fv(refl.location.ubo_fragment, 1, values);
      }
      else
         memcpy(data + refl.ubo_offset, values, 3 * sizeof(float));
   }

   if (refl.push_constant)
   {
      if (refl.location.push_vertex >= 0 || refl.location.push_fragment >= 0)
      {
         if (refl.location.push_vertex >= 0)
            glUniform3fv(refl.location.push_vertex, 1, values);
         if (refl.location.push_fragment >= 0)
            glUniform3fv(refl.location.push_fragment, 1, values);
      }
      else
         memcpy(push_constant_buffer + refl.push_constant_offset, values, 3 * sizeof(float));
   }
}

void Pass::build_semantic_texture(uint8_t *buffer,
      slang_texture_semantic semantic, const Texture &texture)
{
   build_semantic_texture_vec4(buffer, semantic,
         texture.texture.width, texture.texture.height);
   set_semantic_texture(semantic, texture);
}

void Pass::build_semantic_texture_array_vec4(uint8_t *data, slang_texture_semantic semantic,
      unsigned index, unsigned width, unsigned height)
{
   const slang_texture_semantic_array &arr =
      reflection.semantic_textures[semantic];
   const slang_texture_semantic_meta *refl;
   if (index >= arr.size)
      return;
   refl = &arr.data[index];

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
         float *_data = reinterpret_cast<float *>(data + refl->ubo_offset);
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
         float *_data = reinterpret_cast<float *>(push_constant_buffer + refl->push_constant_offset);
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }
}

void Pass::build_semantic_texture_vec4(uint8_t *data, slang_texture_semantic semantic,
      unsigned width, unsigned height)
{
   build_semantic_texture_array_vec4(data, semantic, 0, width, height);
}

bool Pass::init_feedback()
{
   if (final_pass)
      return false;

   gl3_framebuffer_delete(framebuffer_feedback);
   framebuffer_feedback = gl3_framebuffer_new(pass_info.rt_format,
         pass_info.max_levels);
   return true;
}

Pass::~Pass()
{
   if (pipeline != 0)
      glDeleteProgram(pipeline);
   /* the unique_ptrs these replaced were released by ~Pass itself */
   gl3_framebuffer_delete(framebuffer);
   gl3_framebuffer_delete(framebuffer_feedback);
   /* likewise ~UBORing, which ran as a member destructor */
   gl3_ubo_ring_free(&ubo_ring);
   gl3_ubo_ring_free(&push_ring);
   /* and the string / vector members these replaced */
   {
      size_t p;
      for (p = 0; p < num_parameters; p++)
         free(parameters[p].id);
   }
   free(parameters);
   free(filtered_parameters);
   free(pass_name);
   free(vertex_shader);
   free(fragment_shader);
   free(uniforms);
   free(push_constant_buffer);
   slang_reflection_free(&reflection);
}

void Pass::set_shader(GLenum stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   switch (stage)
   {
      case GL_VERTEX_SHADER:
         free(vertex_shader);
         vertex_shader     = NULL;
         num_vertex_shader = 0;
         if (spirv_words &&
               (vertex_shader = (uint32_t*)malloc(
                     spirv_words * sizeof(uint32_t))))
         {
            memcpy(vertex_shader, spirv, spirv_words * sizeof(uint32_t));
            num_vertex_shader = spirv_words;
         }
         break;
      case GL_FRAGMENT_SHADER:
         free(fragment_shader);
         fragment_shader     = NULL;
         num_fragment_shader = 0;
         if (spirv_words &&
               (fragment_shader = (uint32_t*)malloc(
                     spirv_words * sizeof(uint32_t))))
         {
            memcpy(fragment_shader, spirv, spirv_words * sizeof(uint32_t));
            num_fragment_shader = spirv_words;
         }
         break;
      default:
         break;
   }
}

void Pass::add_parameter(unsigned index, const char *id)
{
   struct Parameter *next = (struct Parameter*)realloc(parameters,
         (num_parameters + 1) * sizeof(*parameters));

   if (!next)
      return;

   parameters                        = next;
   parameters[num_parameters].id     = id ? strdup(id) : NULL;
   parameters[num_parameters].index  = index;
   parameters[num_parameters].semantic_index = (unsigned)num_parameters;
   num_parameters++;
}

void Pass::set_semantic_texture(slang_texture_semantic semantic,
      const Texture &texture)
{
   if (reflection.semantic_textures[semantic].data[0].texture)
   {
      unsigned binding = reflection.semantic_textures[semantic].data[0].binding;
      glActiveTexture(GL_TEXTURE0 + binding);
      glBindTexture(GL_TEXTURE_2D, texture.texture.image);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, convert_filter_to_mag_gl(texture.filter));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, convert_filter_to_min_gl(texture.filter, texture.mip_filter));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, address_to_gl(texture.address));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, address_to_gl(texture.address));
   }
}

void Pass::build_semantic_texture_array(uint8_t *buffer,
      slang_texture_semantic semantic, unsigned index, const Texture &texture)
{
   build_semantic_texture_array_vec4(buffer, semantic, index,
         texture.texture.width, texture.texture.height);

   if (index < reflection.semantic_textures[semantic].size &&
         reflection.semantic_textures[semantic].data[index].texture)
   {
      unsigned binding = reflection.semantic_textures[semantic].data[index].binding;
      glActiveTexture(GL_TEXTURE0 + binding);
      glBindTexture(GL_TEXTURE_2D, texture.texture.image);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, convert_filter_to_mag_gl(texture.filter));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, convert_filter_to_min_gl(texture.filter, texture.mip_filter));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, address_to_gl(texture.address));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, address_to_gl(texture.address));
   }
}

void Pass::build_semantics(uint8_t *buffer,
      const float *mvp, const Texture &original, const Texture &source)
{
   unsigned i;

   /* MVP */
   if (buffer && reflection.semantics[SLANG_SEMANTIC_MVP].uniform)
   {
      size_t offset = reflection.semantics[
         SLANG_SEMANTIC_MVP].ubo_offset;
      if (mvp)
         memcpy(buffer + offset,
               mvp, sizeof(float) * 16);
      else
         gl3_build_default_matrix(reinterpret_cast<float *>(
                  buffer + offset));
   }

   if (reflection.semantics[SLANG_SEMANTIC_MVP].push_constant)
   {
      size_t offset = reflection.semantics[
         SLANG_SEMANTIC_MVP].push_constant_offset;

      if (mvp)
         memcpy(push_constant_buffer + offset,
               mvp, sizeof(float) * 16);
      else
         gl3_build_default_matrix(reinterpret_cast<float *>(
                  push_constant_buffer + offset));
   }

   /* Output information */
   build_semantic_vec4(buffer, SLANG_SEMANTIC_OUTPUT,
                       current_framebuffer_size.width,
                       current_framebuffer_size.height);
   build_semantic_vec4(buffer, SLANG_SEMANTIC_FINAL_VIEWPORT,
                       unsigned(curr_vp.width),
                       unsigned(curr_vp.height));

   build_semantic_uint(buffer, SLANG_SEMANTIC_FRAME_COUNT,
                       frame_count_period
                       ? uint32_t(frame_count % frame_count_period)
                       : uint32_t(frame_count));

   build_semantic_int(buffer, SLANG_SEMANTIC_FRAME_DIRECTION,
                      frame_direction);

   build_semantic_uint(buffer, SLANG_SEMANTIC_FRAME_TIME_DELTA,
                      frame_time_delta);

   build_semantic_float(buffer, SLANG_SEMANTIC_ORIGINAL_FPS,
                      original_fps);

   build_semantic_uint(buffer, SLANG_SEMANTIC_ROTATION,
                      rotation);

   build_semantic_float(buffer, SLANG_SEMANTIC_CORE_ASPECT,
                      core_aspect);

   build_semantic_float(buffer, SLANG_SEMANTIC_CORE_ASPECT_ROT,
                      core_aspect_rot);

   build_semantic_uint(buffer, SLANG_SEMANTIC_TOTAL_SUBFRAMES,
                      total_subframes);
   build_semantic_uint(buffer, SLANG_SEMANTIC_CURRENT_SUBFRAME,
                      current_subframe);

   /* Sensor uniforms — per-frame snapshot cached
    * by input_driver_poll() on the main thread */
   {
      input_driver_state_t *input_st = input_state_get_ptr();
      build_semantic_vec3(buffer, SLANG_SEMANTIC_GYROSCOPE,
                        input_st->sensor_gyroscope_cache);
      build_semantic_vec3(buffer, SLANG_SEMANTIC_ACCELEROMETER,
                        input_st->sensor_accelerometer_cache);
      build_semantic_vec3(buffer, SLANG_SEMANTIC_ACCELEROMETER_REST,
                        input_st->sensor_accelerometer_rest);
   }

   /* Standard inputs */
   build_semantic_texture(buffer, SLANG_TEXTURE_SEMANTIC_ORIGINAL, original);
   build_semantic_texture(buffer, SLANG_TEXTURE_SEMANTIC_SOURCE, source);

   /* ORIGINAL_HISTORY[0] is an alias of ORIGINAL. */
   build_semantic_texture_array(buffer,
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0, original);

   /* Parameters. */
   for (i = 0; i < num_filtered_parameters; i++)
   {
      const struct Parameter *m = &parameters[filtered_parameters[i]];
      build_semantic_parameter(buffer, m->semantic_index,
            common->shader_preset->parameters[m->index].current);
   }

   /* Previous inputs. */
   for (i = 0; i < common->num_original_history; i++)
      build_semantic_texture_array(buffer,
            SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, i + 1,
            common->original_history[i]);

   /* Previous passes. */
   for (i = 0; i < common->num_pass_outputs; i++)
      build_semantic_texture_array(buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i,
            common->pass_outputs[i]);

   /* Feedback FBOs. */
   for (i = 0; i < common->num_framebuffer_feedback; i++)
      build_semantic_texture_array(buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i,
            common->framebuffer_feedback[i]);

   /* LUTs. */
   for (i = 0; i < common->num_luts; i++)
      build_semantic_texture_array(buffer,
            SLANG_TEXTURE_SEMANTIC_USER, i,
            common->luts[i].texture);
}

void Pass::build_commands(
      const Texture &original,
      const Texture &source,
      const gl3_viewport &vp,
      const float *mvp)
{
   curr_vp          = vp;
   Size2D size      = get_output_size(
         { original.texture.width, original.texture.height },
         { source.texture.width, source.texture.height });

   if (framebuffer &&
       (size.width  != framebuffer->size.width ||
        size.height != framebuffer->size.height))
      gl3_framebuffer_set_size(framebuffer, &size, 0);

   current_framebuffer_size = size;

   glUseProgram(pipeline);

   build_semantics(uniforms, mvp, original, source);

   if (locations.flat_ubo_vertex >= 0)
      glUniform4fv(locations.flat_ubo_vertex,
                   GLsizei((reflection.ubo_size + 15) / 16),
                   reinterpret_cast<const float *>(uniforms));

   if (locations.flat_ubo_fragment >= 0)
      glUniform4fv(locations.flat_ubo_fragment,
                   GLsizei((reflection.ubo_size + 15) / 16),
                   reinterpret_cast<const float *>(uniforms));

   if (locations.flat_push_vertex >= 0)
      glUniform4fv(locations.flat_push_vertex,
                   GLsizei((reflection.push_constant_size + 15) / 16),
                   reinterpret_cast<const float *>(push_constant_buffer));

   if (locations.flat_push_fragment >= 0)
      glUniform4fv(locations.flat_push_fragment,
                   GLsizei((reflection.push_constant_size + 15) / 16),
                   reinterpret_cast<const float *>(push_constant_buffer));

   if (!(      locations.buffer_index_ubo_vertex   == GL_INVALID_INDEX
            && locations.buffer_index_ubo_fragment == GL_INVALID_INDEX))
   {
      /* UBO Ring - update and bind */
      unsigned vertex_binding   = locations.buffer_index_ubo_vertex;
      unsigned fragment_binding = locations.buffer_index_ubo_fragment;
      const void *data          = uniforms;
      size_t _len               = reflection.ubo_size;
      GLuint id                 = ubo_ring.buffers[ubo_ring.buffer_index];

      glBindBuffer(GL_UNIFORM_BUFFER, id);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, _len, data);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
      if (vertex_binding != GL_INVALID_INDEX)
         glBindBufferBase(GL_UNIFORM_BUFFER, vertex_binding, id);
      if (     fragment_binding != GL_INVALID_INDEX
            && fragment_binding != vertex_binding)
         glBindBufferBase(GL_UNIFORM_BUFFER, fragment_binding, id);

      ubo_ring.buffer_index++;
      if (ubo_ring.buffer_index >= ubo_ring.num_buffers)
         ubo_ring.buffer_index = 0;
   }

   if (!(      locations.buffer_index_push_vertex   == GL_INVALID_INDEX
            && locations.buffer_index_push_fragment == GL_INVALID_INDEX))
   {
      /* Push constant ring - the GL_ARB_gl_spirv path carries the push
       * constant block in a uniform buffer of its own. */
      unsigned vertex_binding   = locations.buffer_index_push_vertex;
      unsigned fragment_binding = locations.buffer_index_push_fragment;
      const void *data          = push_constant_buffer;
      size_t _len               = reflection.push_constant_size;
      GLuint id                 = push_ring.buffers[push_ring.buffer_index];

      glBindBuffer(GL_UNIFORM_BUFFER, id);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, _len, data);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
      if (vertex_binding != GL_INVALID_INDEX)
         glBindBufferBase(GL_UNIFORM_BUFFER, vertex_binding, id);
      if (     fragment_binding != GL_INVALID_INDEX
            && fragment_binding != vertex_binding)
         glBindBufferBase(GL_UNIFORM_BUFFER, fragment_binding, id);

      push_ring.buffer_index++;
      if (push_ring.buffer_index >= push_ring.num_buffers)
         push_ring.buffer_index = 0;
   }

   /* The final pass is always executed inside
    * another render pass since the frontend will
    * want to overlay various things on top for
    * the passes that end up on-screen. */
   if (!final_pass && framebuffer->complete)
   {
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
   }

#ifdef GL3_ROLLING_SCANLINE_SIMULATION
   if (simulate_scanline)
   {
      glEnable(GL_SCISSOR_TEST);
   }
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

   if (final_pass)
   {
      glViewport(curr_vp.x, curr_vp.y,
                 curr_vp.width, curr_vp.height);
#ifdef GL3_ROLLING_SCANLINE_SIMULATION
      if (simulate_scanline)
      {
         glScissor(  curr_vp.x,
                     int32_t((float(curr_vp.height) / float(total_subframes))
                              * float(current_subframe - 1)),
                     curr_vp.width,
                     uint32_t(float(curr_vp.height) / float(total_subframes))
         );
      }
      else
      {
         glScissor(  curr_vp.x,     curr_vp.y,
                     curr_vp.width, curr_vp.height);
      }
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */
   }
   else
   {
      glViewport(0, 0, size.width, size.height);

#ifdef GL3_ROLLING_SCANLINE_SIMULATION
      if (simulate_scanline)
      {
         glScissor(  0,
                     int32_t((float(size.height) / float(total_subframes))
                              * float(current_subframe - 1)),
                     size.width,
                     uint32_t(float(size.height) / float(total_subframes))
         );
      }
      else
      {
         glScissor(0, 0, size.width, size.height);
      }
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */
   }

#if !defined(HAVE_OPENGLES)
   if (framebuffer && framebuffer->format == GL_SRGB8_ALPHA8)
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
   glBindBuffer(GL_ARRAY_BUFFER, common->quad_vbo);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         reinterpret_cast<void *>(uintptr_t(0)));
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         reinterpret_cast<void *>(uintptr_t(2 * sizeof(float))));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
#ifdef GL3_ROLLING_SCANLINE_SIMULATION
   if (simulate_scanline)
   {
      glDisable(GL_SCISSOR_TEST);
   }
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

#if !defined(HAVE_OPENGLES)
   glDisable(GL_FRAMEBUFFER_SRGB);
#endif

   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   if (!final_pass)
      if (framebuffer->levels > 1)
      {
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
         glBindTexture(GL_TEXTURE_2D, framebuffer->image);
         glGenerateMipmap(GL_TEXTURE_2D);
         glBindTexture(GL_TEXTURE_2D, 0);
      }
}

}

struct gl3_filter_chain
{
   gl3_shader::Pass **passes;
   size_t num_passes;
   gl3_filter_chain_pass_info *pass_info;
   size_t num_pass_info;
   struct gl3_shader::gl3_framebuffer *copy_framebuffer;
   gl3_shader::CommonResources common;
   gl3_filter_chain_texture input_texture;
   struct gl3_shader::gl3_framebuffer **original_history;
   size_t num_original_history;
   bool require_clear;
   bool alias_initialized;
};

static void gl3_chain_update_history_info(struct gl3_filter_chain *chain);
static void gl3_chain_update_feedback_info(struct gl3_filter_chain *chain);
static void gl3_chain_build_offscreen_passes(struct gl3_filter_chain *chain, const gl3_viewport &vp);
static void gl3_chain_end_frame(struct gl3_filter_chain *chain);
static void gl3_chain_build_viewport_pass(struct gl3_filter_chain *chain, const gl3_viewport &vp, const float *mvp);
static bool gl3_chain_init_history(struct gl3_filter_chain *chain);
static bool gl3_chain_init_feedback(struct gl3_filter_chain *chain);
static bool gl3_chain_init_alias(struct gl3_filter_chain *chain);
static void gl3_chain_set_pass_info(struct gl3_filter_chain *chain, unsigned pass, const gl3_filter_chain_pass_info &info);
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
static void gl3_chain_set_input_texture(struct gl3_filter_chain *chain, const gl3_filter_chain_texture &texture);
static bool gl3_chain_add_static_texture(struct gl3_filter_chain *chain, const struct gl3_shader::gl3_static_texture *texture);
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
      video_shader *shader);
static video_shader *gl3_chain_get_shader_preset(
      struct gl3_filter_chain *chain);
static void gl3_chain_free(struct gl3_filter_chain *chain);

/* CommonResources still has a constructor, so the chain is new'd rather
 * than calloc'd until that flattens too. */
static struct gl3_filter_chain *gl3_chain_new(unsigned num_passes)
{
   struct gl3_filter_chain *chain = new gl3_filter_chain();

   if (!chain)
      return NULL;

   chain->passes               = NULL;
   chain->num_passes           = 0;
   chain->pass_info            = NULL;
   chain->num_pass_info        = 0;
   chain->copy_framebuffer     = NULL;
   memset(&chain->input_texture, 0, sizeof(chain->input_texture));
   chain->original_history     = NULL;
   chain->num_original_history = 0;
   chain->require_clear        = false;
   chain->alias_initialized    = false;

   gl3_chain_set_num_passes(chain, num_passes);
   return chain;
}

static void gl3_chain_free(struct gl3_filter_chain *chain)
{
   size_t h;

   if (!chain)
      return;

   for (h = 0; h < chain->num_original_history; h++)
      gl3_shader::gl3_framebuffer_delete(chain->original_history[h]);
   free(chain->original_history);
   gl3_shader::gl3_framebuffer_delete(chain->copy_framebuffer);
   for (h = 0; h < chain->num_passes; h++)
      delete chain->passes[h];
   free(chain->passes);
   free(chain->pass_info);
   delete chain;
}

static void gl3_chain_set_shader_preset(struct gl3_filter_chain *chain,
      video_shader *shader)
{
   /* still new-allocated by the create paths, so this stays delete
    * until those move to calloc with the rest of the chain */
   if (chain->common.shader_preset != shader)
      delete chain->common.shader_preset;
   chain->common.shader_preset = shader;
}

static video_shader *gl3_chain_get_shader_preset(
      struct gl3_filter_chain *chain)
{
   return chain->common.shader_preset;
}



static void gl3_chain_update_history_info(struct gl3_filter_chain *chain)
{
   unsigned i;

   for (i = 0; i < chain->num_original_history; i++)
   {
      gl3_shader::Texture *source = (gl3_shader::Texture*)
         &chain->common.original_history[i];

      if (!source)
         continue;

      source->texture.image  = chain->original_history[i]->image;
      source->texture.width  = chain->original_history[i]->size.width;
      source->texture.height = chain->original_history[i]->size.height;
      source->filter         = chain->passes[0]->get_source_filter();
      source->mip_filter     = chain->passes[0]->get_mip_filter();
      source->address        = chain->passes[0]->get_address_mode();
   }
}

static void gl3_chain_update_feedback_info(struct gl3_filter_chain *chain)
{
   unsigned i;

   for (i = 0; i < chain->num_passes - 1; i++)
   {
      struct gl3_shader::gl3_framebuffer *fb = chain->passes[i]->get_feedback_framebuffer();
      if (!fb)
         continue;

      gl3_shader::Texture *source = (gl3_shader::Texture*)
         &chain->common.framebuffer_feedback[i];

      if (!source)
         continue;

      source->texture.image  = fb->image;
      source->texture.width  = fb->size.width;
      source->texture.height = fb->size.height;
      source->filter         = chain->passes[i]->get_source_filter();
      source->mip_filter     = chain->passes[i]->get_mip_filter();
      source->address        = chain->passes[i]->get_address_mode();
   }
}

static void gl3_chain_build_offscreen_passes(struct gl3_filter_chain *chain, const gl3_viewport &vp)
{
   unsigned i;

   /* First frame, make sure our history and feedback textures
    * are in a clean state. */
   if (chain->require_clear)
   {
      gl3_chain_clear_history_and_feedback(chain);
      chain->require_clear = false;
   }

   gl3_chain_update_history_info(chain);
   if (chain->common.num_framebuffer_feedback)
      gl3_chain_update_feedback_info(chain);

   const gl3_shader::Texture original = {
         chain->input_texture,
         chain->passes[0]->get_source_filter(),
         chain->passes[0]->get_mip_filter(),
         chain->passes[0]->get_address_mode(),
   };
   gl3_shader::Texture source = original;

   for (i = 0; i < chain->num_passes - 1; i++)
   {
      chain->passes[i]->build_commands(original, source, vp, nullptr);

      const struct gl3_shader::gl3_framebuffer *fb = chain->passes[i]->get_framebuffer();

      source.texture.image             = fb->image;
      source.texture.width             = fb->size.width;
      source.texture.height            = fb->size.height;
      source.filter                    = chain->passes[i + 1]->get_source_filter();
      source.mip_filter                = chain->passes[i + 1]->get_mip_filter();
      source.address                   = chain->passes[i + 1]->get_address_mode();

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
      struct gl3_shader::gl3_framebuffer *tmp =
         chain->original_history[chain->num_original_history - 1];
      chain->original_history[chain->num_original_history - 1] = NULL;

      if (chain->input_texture.width      != tmp->size.width  ||
            chain->input_texture.height     != tmp->size.height ||
            (chain->input_texture.format    != 0
             && chain->input_texture.format != tmp->format))
      {
         Size2D new_size;
         new_size.width  = chain->input_texture.width;
         new_size.height = chain->input_texture.height;
         gl3_framebuffer_set_size(tmp, &new_size, chain->input_texture.format);
      }

      if (tmp->complete)
         gl3_framebuffer_copy(
               tmp->framebuffer,
               chain->common.quad_program,
               chain->common.quad_vbo,
               chain->common.quad_loc.flat_ubo_vertex,
               tmp->size,
               chain->input_texture.image);

      /* Should ring buffer, but we don't have *that* many chain->passes. */
      for (h = chain->num_original_history - 1; h > 0; h--)
         chain->original_history[h] = chain->original_history[h - 1];
      chain->original_history[0] = tmp;
   }
}

static void gl3_chain_build_viewport_pass(struct gl3_filter_chain *chain, const gl3_viewport &vp, const float *mvp)
{
   unsigned i;
   /* First frame, make sure our history and
    * feedback textures are in a clean state. */
   if (chain->require_clear)
   {
      gl3_chain_clear_history_and_feedback(chain);
      chain->require_clear = false;
   }

   gl3_shader::Texture source;
   const gl3_shader::Texture original = {
         chain->input_texture,
         chain->passes[0]->get_source_filter(),
         chain->passes[0]->get_mip_filter(),
         chain->passes[0]->get_address_mode(),
   };

   if (chain->num_passes == 1)
   {
      source = {
            chain->input_texture,
            chain->passes[chain->num_passes - 1]->get_source_filter(),
            chain->passes[chain->num_passes - 1]->get_mip_filter(),
            chain->passes[chain->num_passes - 1]->get_address_mode(),
      };
   }
   else
   {
      const struct gl3_shader::gl3_framebuffer *fb = chain->passes[chain->num_passes - 2]
         ->get_framebuffer();
      source.texture.image           = fb->image;
      source.texture.width           = fb->size.width;
      source.texture.height          = fb->size.height;
      source.filter                  = chain->passes[chain->num_passes - 1]->get_source_filter();
      source.mip_filter              = chain->passes[chain->num_passes - 1]->get_mip_filter();
      source.address                 = chain->passes[chain->num_passes - 1]->get_address_mode();
   }

   chain->passes[chain->num_passes - 1]->build_commands(original, source, vp, mvp);

   /* For feedback FBOs, swap current and previous. */
   for (i = 0; i < chain->num_passes; i++)
   {
      struct gl3_shader::gl3_framebuffer *fb = chain->passes[i]->get_feedback_framebuffer();
      if (fb)
         chain->passes[i]->end_frame();
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
   gl3_shader::gl3_texture_array_resize(&chain->common.original_history,
         &chain->common.num_original_history, 0);

   for (i = 0; i < chain->num_passes; i++)
   {
      size_t _y = chain->passes[i]->get_reflection().semantic_textures[
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
   chain->original_history = (struct gl3_shader::gl3_framebuffer**)
      calloc(required_images, sizeof(*chain->original_history));
   if (!chain->original_history)
      return false;
   chain->num_original_history = required_images;
   if (!gl3_shader::gl3_texture_array_resize(&chain->common.original_history,
            &chain->common.num_original_history, required_images))
      return false;

   for (i = 0; i < required_images; i++)
   {
      chain->original_history[i] = gl3_shader::gl3_framebuffer_new(0, 1);
      if (!chain->original_history[i])
         return false;
   }

   RARCH_LOG("[GLCore] Using history of %u frames.\n", unsigned(required_images));

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

   gl3_shader::gl3_texture_array_resize(&chain->common.framebuffer_feedback,
         &chain->common.num_framebuffer_feedback, 0);

   /* Final pass cannot have feedback. */
   for (i = 0; i < chain->num_passes - 1; i++)
   {
      bool use_feedback = false;
      size_t q;
      for (q = 0; q < chain->num_passes; q++)
      {
         const slang_reflection &r          = chain->passes[q]->get_reflection();
         const slang_texture_semantic_array &feedbacks =
            r.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK];

         if (i < feedbacks.size && feedbacks.data[i].texture)
         {
            use_feedback  = true;
            use_feedbacks = true;
            break;
         }
      }

      if (use_feedback && !chain->passes[i]->init_feedback())
         return false;

      if (use_feedback)
         RARCH_LOG("[GLCore] Using framebuffer feedback for pass #%u.\n", i);
   }

   if (!use_feedbacks)
   {
      RARCH_LOG("[GLCore] Not using framebuffer feedback.\n");
      return true;
   }

   if (!gl3_shader::gl3_texture_array_resize(&chain->common.framebuffer_feedback,
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
      const char *name = chain->passes[i]->get_name();
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

static void gl3_chain_set_pass_info(struct gl3_filter_chain *chain, unsigned pass, const gl3_filter_chain_pass_info &info)
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
      delete chain->passes[i];
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

   if (!(chain->passes = (gl3_shader::Pass**)
            calloc(num_passes_, sizeof(*chain->passes))))
      return;

   for (i = 0; i < num_passes_; i++)
   {
      if (!(chain->passes[i] = new gl3_shader::Pass(i + 1 == num_passes_)))
         return;
      chain->passes[i]->set_common_resources(&chain->common);
      chain->passes[i]->set_pass_number(i);
      chain->num_passes++;
   }
}

static void gl3_chain_set_shader(struct gl3_filter_chain *chain, unsigned pass, GLenum stage, const uint32_t *spirv, size_t spirv_words)
{
   chain->passes[pass]->set_shader(stage, spirv, spirv_words);
}

static void gl3_chain_add_parameter(struct gl3_filter_chain *chain, unsigned pass,
      unsigned index, const char *id)
{
   chain->passes[pass]->add_parameter(index, id);
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
            string_is_empty(chain->passes[i]->get_name()) ?
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) :
            chain->passes[i]->get_name());

      chain->passes[i]->set_pass_info(chain->pass_info[i]);
      if (!chain->passes[i]->build())
         return false;
   }

   chain->require_clear = false;
   if (!gl3_chain_init_history(chain))
      return false;
   if (!gl3_chain_init_feedback(chain))
      return false;
   if (!gl3_shader::gl3_texture_array_resize(&chain->common.pass_outputs,
            &chain->common.num_pass_outputs, chain->num_passes))
      return false;
   return true;
}

static bool gl3_chain_init_single_pass(struct gl3_filter_chain *chain, unsigned pass_idx)
{
   if (pass_idx >= chain->num_passes)
      return false;

   RARCH_LOG("[GLCore] Building pass #%u (%s)\n", pass_idx,
         string_is_empty(chain->passes[pass_idx]->get_name()) ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) :
         chain->passes[pass_idx]->get_name());

   chain->passes[pass_idx]->set_pass_info(chain->pass_info[pass_idx]);
   if (!chain->passes[pass_idx]->build())
      return false;

   return true;
}

static bool gl3_chain_compile_full_pass(struct gl3_filter_chain *chain, unsigned pass_idx,
      glslang_filter_chain_filter default_filter)
{
   video_shader *shader = chain->common.shader_preset;
   if (!shader || pass_idx >= chain->num_passes)
      return false;

   /* For the extra opaque pass appended when last_pass_is_fbo,
    * the SPIRV was already set in create_deferred — just build. */
   if (pass_idx >= shader->passes)
      return gl3_chain_init_single_pass(chain, pass_idx);

   const video_shader_pass *pass      = &shader->pass[pass_idx];
   const video_shader_pass *next_pass =
      pass_idx + 1 < shader->passes
      ? &shader->pass[pass_idx + 1] : nullptr;

   /* ---- SPIRV cross-compile (CPU) ---- */
   glslang_output output;
   if (!glslang_compile_shader(pass->source.path, &output))
   {
      RARCH_ERR("[GLCore] Failed to compile shader: \"%s\".\n",
            pass->source.path);
      return false;
   }

   /* ---- Extract parameters ---- */
   for (size_t j = 0; j < output.meta.num_parameters; j++)
   {
      const glslang_parameter *meta_param = &output.meta.parameters[j];

      if (shader->num_parameters >= GFX_MAX_PARAMETERS)
      {
         RARCH_ERR("[GLCore] Exceeded maximum number of parameters (%u).\n",
               GFX_MAX_PARAMETERS);
         glslang_output_free(&output);
         return false;
      }

      video_shader_parameter *itr = NULL;
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
         video_shader_parameter *param =
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
   if (!string_is_empty(chain->passes[pass_idx]->get_name()))
   {
      chain->alias_initialized = false;
      if (!gl3_chain_init_alias_early(chain))
      {
         glslang_output_free(&output);
         return false;
      }
   }

   /* ---- Pass info (scale, filter, format) ---- */
   struct gl3_filter_chain_pass_info p_info;
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

   bool explicit_format = output.meta.rt_format != SLANG_FORMAT_UNKNOWN;

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
            gl3_shader::convert_glslang_format(output.meta.rt_format);
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
         gl3_shader::convert_glslang_format(output.meta.rt_format);

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
   if (!gl3_shader::gl3_texture_array_resize(&chain->common.pass_outputs,
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
      struct gl3_shader::gl3_framebuffer *fb = chain->passes[i]->get_feedback_framebuffer();
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

static void gl3_chain_set_input_texture(struct gl3_filter_chain *chain, const gl3_filter_chain_texture &texture)
{
   chain->input_texture = texture;

   /* Need a copy to remove padding.
    * GL HW render interface in libretro is kinda garbage now ... */
   if (chain->input_texture.padded_width  != chain->input_texture.width ||
       chain->input_texture.padded_height != chain->input_texture.height)
   {
      if (!chain->copy_framebuffer)
         chain->copy_framebuffer = gl3_shader::gl3_framebuffer_new(texture.format, 1);
      if (!chain->copy_framebuffer)
         return;

      if (chain->input_texture.width   != chain->copy_framebuffer->size.width  ||
          chain->input_texture.height  != chain->copy_framebuffer->size.height ||
          (chain->input_texture.format != 0                                   &&
           chain->input_texture.format != chain->copy_framebuffer->format))
      {
         Size2D copy_size;
         copy_size.width  = chain->input_texture.width;
         copy_size.height = chain->input_texture.height;
         gl3_shader::gl3_framebuffer_set_size(chain->copy_framebuffer, &copy_size,
               chain->input_texture.format);
      }

      if (chain->copy_framebuffer->complete)
         gl3_framebuffer_copy_partial(
               chain->copy_framebuffer->framebuffer,
               chain->common.quad_program,
               chain->common.quad_loc.flat_ubo_vertex,
               chain->copy_framebuffer->size,
               chain->input_texture.image,
               float(chain->input_texture.width)
               / chain->input_texture.padded_width,
               float(chain->input_texture.height)
               / chain->input_texture.padded_height);
      chain->input_texture.image = chain->copy_framebuffer->image;
   }
}

static bool gl3_chain_add_static_texture(struct gl3_filter_chain *chain, const struct gl3_shader::gl3_static_texture *texture)
{
   struct gl3_shader::gl3_static_texture *next =
      (struct gl3_shader::gl3_static_texture*)realloc(chain->common.luts,
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
      chain->passes[i]->set_frame_count(count);
}

static void gl3_chain_set_frame_count_period(struct gl3_filter_chain *chain, unsigned pass, unsigned period)
{
   chain->passes[pass]->set_frame_count_period(period);
}

static void gl3_chain_set_frame_direction(struct gl3_filter_chain *chain, int32_t direction)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      chain->passes[i]->set_frame_direction(direction);
}

static void gl3_chain_set_frame_time_delta(struct gl3_filter_chain *chain, uint32_t time_delta)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      chain->passes[i]->set_frame_time_delta(time_delta);
}

static void gl3_chain_set_original_fps(struct gl3_filter_chain *chain, float fps)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      chain->passes[i]->set_original_fps(fps);
}

static void gl3_chain_set_rotation(struct gl3_filter_chain *chain, uint32_t rot)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      chain->passes[i]->set_rotation(rot);
}

static void gl3_chain_set_core_aspect(struct gl3_filter_chain *chain, float coreaspect)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      chain->passes[i]->set_core_aspect(coreaspect);
}

static void gl3_chain_set_core_aspect_rot(struct gl3_filter_chain *chain, float coreaspectrot)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      chain->passes[i]->set_core_aspect_rot(coreaspectrot);
}


static void gl3_chain_set_shader_subframes(struct gl3_filter_chain *chain, uint32_t tot_subframes)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      chain->passes[i]->set_shader_subframes(tot_subframes);
}

static void gl3_chain_set_current_shader_subframe(struct gl3_filter_chain *chain, uint32_t cur_subframe)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      chain->passes[i]->set_current_shader_subframe(cur_subframe);
}

#ifdef GL3_ROLLING_SCANLINE_SIMULATION
static void gl3_chain_set_simulate_scanline(struct gl3_filter_chain *chain, bool simulate_scanline)
{
   unsigned i;
   for (i = 0; i < chain->num_passes; i++)
      chain->passes[i]->set_simulate_scanline(simulate_scanline);
}
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

static void gl3_chain_set_pass_name(struct gl3_filter_chain *chain, unsigned pass, const char *name)
{
   chain->passes[pass]->set_name(name);
}

static bool gl3_filter_chain_load_lut(
      gl3_filter_chain *chain,
      const video_shader_lut *shader,
      struct gl3_shader::gl3_static_texture *out)
{
   texture_image image;
   GLuint tex                      = 0;

   image.width                     = 0;
   image.height                    = 0;
   image.pixels                    = NULL;
   image.supports_rgba             = true;

   if (!image_texture_load(&image, shader->path))
      return false;

   unsigned levels = shader->mipmap ? glslang_num_miplevels(image.width, image.height) : 1;

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
      gl3_filter_chain *chain,
      video_shader *shader)
{
   unsigned i;
   for (i = 0; i < shader->luts; i++)
   {
      struct gl3_shader::gl3_static_texture image;
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
         gl3_shader::opaque_vert,
         sizeof(gl3_shader::opaque_vert) / sizeof(uint32_t));
   gl3_chain_set_shader(chain, 0, GL_FRAGMENT_SHADER,
         gl3_shader::opaque_frag,
         sizeof(gl3_shader::opaque_frag) / sizeof(uint32_t));

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
   unsigned i;
   video_shader *shader = new video_shader();
   if (!shader)
      return NULL;

   if (!video_shader_load_preset_into_shader(path, shader))
   {
      delete shader;
      return NULL;
   }

   bool last_pass_is_fbo = shader->pass[shader->passes - 1].fbo.flags &
      FBO_SCALE_FLAG_VALID;

   struct gl3_filter_chain *chain = gl3_chain_new(
         shader->passes + (last_pass_is_fbo ? 1 : 0));
   if (!chain)
      {
         delete shader;
         gl3_chain_free(chain);
         return NULL;
      }

   if (      shader->luts
         && !gl3_filter_chain_load_luts(chain, shader))
      {
         delete shader;
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
   glslang_include_cache_guard include_cache_guard;
   void *include_cache = include_cache_guard.handle;

   for (i = 0; i < shader->passes; i++)
   {
      glslang_output output;
      struct gl3_filter_chain_pass_info pass_info;
      const video_shader_pass *pass      = &shader->pass[i];
      const video_shader_pass *next_pass =
         i + 1 < shader->passes ? &shader->pass[i + 1] : nullptr;

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
         delete shader;
         gl3_chain_free(chain);
         return NULL;
      }
      }

      for (size_t j = 0; j < output.meta.num_parameters; j++)
      {
         const glslang_parameter *meta_param = &output.meta.parameters[j];

         if (shader->num_parameters >= GFX_MAX_PARAMETERS)
         {
            RARCH_ERR("[GLCore] Exceeded maximum number of parameters (%u).\n", GFX_MAX_PARAMETERS);
            glslang_output_free(&output);
            {
         delete shader;
         gl3_chain_free(chain);
         return NULL;
      }
         }

         video_shader_parameter *itr = NULL;
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
         delete shader;
         gl3_chain_free(chain);
         return NULL;
      }
            }
            gl3_chain_add_parameter(chain, i, (unsigned)(itr - shader->parameters), meta_param->id);
         }
         else
         {
            video_shader_parameter *param = &shader->parameters[shader->num_parameters];
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

      bool explicit_format = output.meta.rt_format != SLANG_FORMAT_UNKNOWN;

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
            pass_info.rt_format = gl3_shader::convert_glslang_format(output.meta.rt_format);
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

         pass_info.rt_format = gl3_shader::convert_glslang_format(output.meta.rt_format);
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
            gl3_shader::opaque_vert,
            sizeof(gl3_shader::opaque_vert) / sizeof(uint32_t));

      gl3_chain_set_shader(chain, shader->passes,
            GL_FRAGMENT_SHADER,
            gl3_shader::opaque_frag,
            sizeof(gl3_shader::opaque_frag) / sizeof(uint32_t));
   }

   gl3_chain_set_shader_preset(chain, shader);
   shader = NULL;   /* the chain owns it now */

   if (!gl3_chain_init(chain))
      {
         delete shader;
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
   video_shader *shader = new video_shader();
   if (!shader)
      return NULL;

   if (!video_shader_load_preset_into_shader(path, shader))
   {
      delete shader;
      return NULL;
   }

   bool last_pass_is_fbo = shader->pass[shader->passes - 1].fbo.flags &
      FBO_SCALE_FLAG_VALID;

   unsigned total_passes = shader->passes + (last_pass_is_fbo ? 1 : 0);
   struct gl3_filter_chain *chain = gl3_chain_new(total_passes);
   if (!chain)
      {
         delete shader;
         gl3_chain_free(chain);
         return NULL;
      }

   if (      shader->luts
         && !gl3_filter_chain_load_luts(chain, shader))
      {
         delete shader;
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
            gl3_shader::opaque_vert,
            sizeof(gl3_shader::opaque_vert) / sizeof(uint32_t));

      gl3_chain_set_shader(chain, shader->passes,
            GL_FRAGMENT_SHADER,
            gl3_shader::opaque_frag,
            sizeof(gl3_shader::opaque_frag) / sizeof(uint32_t));
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
         delete shader;
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
