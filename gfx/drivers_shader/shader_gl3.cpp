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

#include "shader_gl3.h"
#include "glslang_util.h"
#include "glslang_util_cxx.h"

#include <vector>
#include <memory>
#include <functional>
#include <utility>
#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <formats/image.h>
#include <retro_miscellaneous.h>

#include "slang_reflection.h"
#include "slang_reflection.hpp"
#include "spirv_glsl.hpp"

#include "../common/gl3_defines.h"

#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../msg_hash.h"

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

static void gl3_framebuffer_copy(
      GLuint fb_id,
      GLuint quad_program,
      GLuint quad_vbo,
      GLint flat_ubo_vertex,
      struct Size2D size,
      GLuint image)
{
   glBindFramebuffer(GL_FRAMEBUFFER, fb_id);
   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, image);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glViewport(0, 0, size.width, size.height);
   glClear(GL_COLOR_BUFFER_BIT);

   glUseProgram(quad_program);
   if (flat_ubo_vertex >= 0)
   {
      static float mvp[16] = { 
                                2.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 2.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 2.0f, 0.0f,
                               -1.0f,-1.0f, 0.0f, 1.0f
                             };
      glUniform4fv(flat_ubo_vertex, 4, mvp);
   }

   /* Draw quad */
   glDisable(GL_CULL_FACE);
   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void *)((uintptr_t)(0)));
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void *)((uintptr_t)(2 * sizeof(float))));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);

   glUseProgram(0);
   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void gl3_framebuffer_copy_partial(
      GLuint fb_id,
      GLuint quad_program, 
      GLint flat_ubo_vertex,
      struct Size2D size,
      GLuint image,
      float rx, float ry)
{
   GLuint vbo;
   const float quad_data[16] = {
      0.0f, 0.0f, 0.0f, 0.0f,
      1.0f, 0.0f, rx,   0.0f,
      0.0f, 1.0f, 0.0f, ry,
      1.0f, 1.0f, rx,   ry,
   };

   glBindFramebuffer(GL_FRAMEBUFFER, fb_id);
   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, image);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glViewport(0, 0, size.width, size.height);
   glClear(GL_COLOR_BUFFER_BIT);

   glUseProgram(quad_program);
   if (flat_ubo_vertex >= 0)
   {
      static float mvp[16] = { 
                                2.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 2.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 2.0f, 0.0f,
                               -1.0f,-1.0f, 0.0f, 1.0f
                             };
      glUniform4fv(flat_ubo_vertex, 4, mvp);
   }
   glDisable(GL_CULL_FACE);
   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);

   /* A bit crude, but heeeey. */
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);

   glBufferData(GL_ARRAY_BUFFER, sizeof(quad_data), quad_data, GL_STREAM_DRAW);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void *)((uintptr_t)(0)));
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void *)((uintptr_t)(2 * sizeof(float))));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDeleteBuffers(1, &vbo);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glUseProgram(0);
   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static GLuint gl3_compile_shader(GLenum stage, const char *source)
{
   GLint status;
   GLuint shader   = glCreateShader(stage);
   const char *ptr = source;

   glShaderSource(shader, 1, &ptr, NULL);
   glCompileShader(shader);

   glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

   if (!status)
   {
      GLint length;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
      if (length > 0)
      {
         char *info_log = (char*)malloc(length);

         if (info_log)
         {
            glGetShaderInfoLog(shader, length, &length, info_log);
            RARCH_ERR("[GLCore]: Failed to compile shader: %s\n", info_log);
            free(info_log);
            glDeleteShader(shader);
            return 0;
         }
      }
   }

   return shader;
}

static uint32_t gl3_get_cross_compiler_target_version(void)
{
   const char *version = (const char*)glGetString(GL_VERSION);
   unsigned major      = 0;
   unsigned minor      = 0;

#ifdef HAVE_OPENGLES3
   if (!version || sscanf(version, "OpenGL ES %u.%u", &major, &minor) != 2)
      return 300;
   
   if (major == 2 && minor == 0)
      return 100;
#else
   if (!version || sscanf(version, "%u.%u", &major, &minor) != 2)
      return 150;

   if (major == 3)
   {
      switch (minor)
      {
         case 2:
            return 150;
         case 1:
            return 140;
         case 0:
            return 130;
      }
   }
   else if (major == 2)
   {
      switch (minor)
      {
         case 1:
            return 120;
         case 0:
            return 110;
      }
   }
#endif

   return 100 * major + 10 * minor;
}


GLuint gl3_cross_compile_program(
      const uint32_t *vertex, size_t vertex_size,
      const uint32_t *fragment, size_t fragment_size,
      gl3_buffer_locations *loc, bool flatten)
{
   GLuint program = 0;
   try
   {
      spirv_cross::ShaderResources vertex_resources;
      spirv_cross::ShaderResources fragment_resources;
      spirv_cross::CompilerGLSL vertex_compiler(vertex, vertex_size / 4);
      spirv_cross::CompilerGLSL fragment_compiler(fragment, fragment_size / 4);
      spirv_cross::CompilerGLSL::Options opts;
#ifdef HAVE_OPENGLES3
      opts.es                               = true;
#else
      opts.es                               = false;
#endif
      opts.version                          = gl3_get_cross_compiler_target_version();
      opts.fragment.default_float_precision = spirv_cross::CompilerGLSL::Options::Precision::Highp;
      opts.fragment.default_int_precision   = spirv_cross::CompilerGLSL::Options::Precision::Highp;
      opts.enable_420pack_extension         = false;

      vertex_compiler.set_common_options(opts);
      fragment_compiler.set_common_options(opts);

      vertex_resources                      = vertex_compiler.get_shader_resources();
      fragment_resources                    = fragment_compiler.get_shader_resources();

      for (auto &res : vertex_resources.stage_inputs)
      {
         uint32_t location = vertex_compiler.get_decoration(res.id, spv::DecorationLocation);
         char loc_buf[64];
         snprintf(loc_buf, sizeof(loc_buf), "RARCH_ATTRIBUTE_%d", location);
         vertex_compiler.set_name(res.id, loc_buf);
         vertex_compiler.unset_decoration(res.id, spv::DecorationLocation);
      }

      for (auto &res : vertex_resources.stage_outputs)
      {
         uint32_t location = vertex_compiler.get_decoration(res.id, spv::DecorationLocation);
         char loc_buf[64];
         snprintf(loc_buf, sizeof(loc_buf), "RARCH_VARYING_%d", location);
         vertex_compiler.set_name(res.id, loc_buf);
         vertex_compiler.unset_decoration(res.id, spv::DecorationLocation);
      }

      for (auto &res : fragment_resources.stage_inputs)
      {
         uint32_t location = fragment_compiler.get_decoration(res.id, spv::DecorationLocation);
         char loc_buf[64];
         snprintf(loc_buf, sizeof(loc_buf), "RARCH_VARYING_%d", location);
         fragment_compiler.set_name(res.id, loc_buf);
         fragment_compiler.unset_decoration(res.id, spv::DecorationLocation);
      }

      if (vertex_resources.push_constant_buffers.size() > 1)
      {
         RARCH_ERR("[GLCore]: Cannot have more than one push constant buffer.\n");
         return 0;
      }

      for (auto &res : vertex_resources.push_constant_buffers)
      {
         vertex_compiler.set_name(res.id, "RARCH_PUSH_VERTEX_INSTANCE");
         vertex_compiler.set_name(res.base_type_id, "RARCH_PUSH_VERTEX");
      }

      if (vertex_resources.uniform_buffers.size() > 1)
      {
         RARCH_ERR("[GLCore]: Cannot have more than one uniform buffer.\n");
         return 0;
      }

      for (auto &res : vertex_resources.uniform_buffers)
      {
         if (flatten)
            vertex_compiler.flatten_buffer_block(res.id);
         vertex_compiler.set_name(res.id, "RARCH_UBO_VERTEX_INSTANCE");
         vertex_compiler.set_name(res.base_type_id, "RARCH_UBO_VERTEX");
         vertex_compiler.unset_decoration(res.id, spv::DecorationDescriptorSet);
         vertex_compiler.unset_decoration(res.id, spv::DecorationBinding);
      }

      if (fragment_resources.push_constant_buffers.size() > 1)
      {
         RARCH_ERR("[GLCore]: Cannot have more than one push constant block.\n");
         return 0;
      }

      for (auto &res : fragment_resources.push_constant_buffers)
      {
         fragment_compiler.set_name(res.id, "RARCH_PUSH_FRAGMENT_INSTANCE");
         fragment_compiler.set_name(res.base_type_id, "RARCH_PUSH_FRAGMENT");
      }

      if (fragment_resources.uniform_buffers.size() > 1)
      {
         RARCH_ERR("[GLCore]: Cannot have more than one uniform buffer.\n");
         return 0;
      }

      for (auto &res : fragment_resources.uniform_buffers)
      {
         if (flatten)
            fragment_compiler.flatten_buffer_block(res.id);
         fragment_compiler.set_name(res.id, "RARCH_UBO_FRAGMENT_INSTANCE");
         fragment_compiler.set_name(res.base_type_id, "RARCH_UBO_FRAGMENT");
         fragment_compiler.unset_decoration(res.id, spv::DecorationDescriptorSet);
         fragment_compiler.unset_decoration(res.id, spv::DecorationBinding);
      }

      std::vector<uint32_t> texture_binding_fixups;
      for (auto &res : fragment_resources.sampled_images)
      {
         uint32_t binding = fragment_compiler.get_decoration(res.id, spv::DecorationBinding);
         char loc_buf[64];
         snprintf(loc_buf, sizeof(loc_buf), "RARCH_TEXTURE_%d", binding);
         fragment_compiler.set_name(res.id, loc_buf);
         fragment_compiler.unset_decoration(res.id, spv::DecorationDescriptorSet);
         fragment_compiler.unset_decoration(res.id, spv::DecorationBinding);
         texture_binding_fixups.push_back(binding);
      }

      auto vertex_source     = vertex_compiler.compile();
      auto fragment_source   = fragment_compiler.compile();
      GLuint vertex_shader   = gl3_compile_shader(GL_VERTEX_SHADER, vertex_source.c_str());
      GLuint fragment_shader = gl3_compile_shader(GL_FRAGMENT_SHADER, fragment_source.c_str());

#if 0
      RARCH_LOG("[GLCore]: Vertex shader:\n========\n%s\n=======\n", vertex_source.c_str());
      RARCH_LOG("[GLCore]: Fragment shader:\n========\n%s\n=======\n", fragment_source.c_str());
#endif

      if (!vertex_shader || !fragment_shader)
      {
         RARCH_ERR("[GLCore]: One or more shaders failed to compile.\n");
         if (vertex_shader)
            glDeleteShader(vertex_shader);
         if (fragment_shader)
            glDeleteShader(fragment_shader);
         return 0;
      }

      program = glCreateProgram();
      glAttachShader(program, vertex_shader);
      glAttachShader(program, fragment_shader);
      for (auto &res : vertex_resources.stage_inputs)
      {
         char loc_buf[64];
         uint32_t _loc = vertex_compiler.get_decoration(res.id, spv::DecorationLocation);
         snprintf(loc_buf, sizeof(loc_buf), "RARCH_ATTRIBUTE_%d", _loc);
         glBindAttribLocation(program, _loc, loc_buf);
      }
      glLinkProgram(program);
      glDeleteShader(vertex_shader);
      glDeleteShader(fragment_shader);

      GLint status;
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
               RARCH_ERR("[GLCore]: Failed to link program: %s\n", info_log);
               free(info_log);
               glDeleteProgram(program);
               return 0;
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
      for (auto &binding : texture_binding_fixups)
      {
         char loc_buf[64];
         snprintf(loc_buf, sizeof(loc_buf), "RARCH_TEXTURE_%d", binding);
         GLint location = glGetUniformLocation(program, loc_buf);
         if (location >= 0)
            glUniform1i(location, binding);
      }

      glUseProgram(0);
   }
   catch (const std::exception &e)
   {
      RARCH_ERR("[GLCore]: Failed to cross compile program: %s\n", e.what());
      if (program != 0)
         glDeleteProgram(program);
      return 0;
   }

   return program;
}

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

static GLenum address_to_gl(glslang_filter_chain_address type)
{
   switch (type)
   {
#ifdef HAVE_OPENGLES3
      case GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_BORDER:
#if 0
         RARCH_WARN("[GLCore]: No CLAMP_TO_BORDER in GLES3. Falling back to edge clamp.\n");
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

class StaticTexture
{
public:
   StaticTexture(std::string id,
                 GLuint image,
                 unsigned width, unsigned height,
                 bool linear,
                 bool mipmap,
                 glslang_filter_chain_address address);
   ~StaticTexture();

   StaticTexture(StaticTexture&&) = delete;
   void operator=(StaticTexture&&) = delete;

   void set_id(std::string name) { id = std::move(name); }
   const std::string &get_id() const { return id; }
   const Texture &get_texture() const { return texture; }

private:
   std::string id;
   GLuint image;
   Texture texture;
};

StaticTexture::StaticTexture(std::string id_, GLuint image_,
      unsigned width, unsigned height, bool linear, bool mipmap,
      glslang_filter_chain_address address)
   : id(std::move(id_)), image(image_)
{
   GLenum gl_address         = address_to_gl(address);

   texture.filter            = GLSLANG_FILTER_CHAIN_NEAREST;
   texture.mip_filter        = GLSLANG_FILTER_CHAIN_NEAREST;
   texture.address           = address;
   texture.texture.width     = width;
   texture.texture.height    = height;
   texture.texture.format    = 0;
   texture.texture.image     = image;

   if (linear)
   {
      texture.filter         = GLSLANG_FILTER_CHAIN_LINEAR;
      if (mipmap)
         texture.mip_filter  = GLSLANG_FILTER_CHAIN_LINEAR;
   }

   glBindTexture(GL_TEXTURE_2D, image);
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

StaticTexture::~StaticTexture()
{
   if (image != 0)
      glDeleteTextures(1, &image);
}

struct CommonResources
{
   CommonResources();
   ~CommonResources();

   std::vector<Texture> original_history;
   std::vector<Texture> framebuffer_feedback;
   std::vector<Texture> pass_outputs;
   std::vector<std::unique_ptr<StaticTexture>> luts;

   std::unordered_map<std::string, slang_texture_semantic_map> texture_semantic_map;
   std::unordered_map<std::string, slang_texture_semantic_map> texture_semantic_uniform_map;
   std::unique_ptr<video_shader> shader_preset;

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
   if (quad_program != 0)
      glDeleteProgram(quad_program);
   if (quad_vbo != 0)
      glDeleteBuffers(1, &quad_vbo);
}

class Framebuffer
{
public:
   Framebuffer(GLenum format, unsigned max_levels);

   ~Framebuffer();
   Framebuffer(Framebuffer&&) = delete;
   void operator=(Framebuffer&&) = delete;

   void set_size(const Size2D &size, GLenum format = 0);

   const Size2D &get_size() const { return size; }
   GLenum get_format() const { return format; }
   GLuint get_image() const { return image; }
   GLuint get_framebuffer() const { return framebuffer; }

   bool is_complete() const { return complete; }

   unsigned get_levels() const { return levels; }

private:
   GLuint image = 0;
   Size2D size;
   GLenum format;
   unsigned max_levels;
   unsigned levels = 0;

   GLuint framebuffer = 0;

   void init();
   bool complete = false;
};

Framebuffer::Framebuffer(GLenum format_, unsigned max_levels_)
   : size({1, 1}), format(format_), max_levels(max_levels_)
{
   glGenFramebuffers(1, &framebuffer);

   /* Need to bind to create */
   glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   if (format == 0)
      format = GL_RGBA8;
}

void Framebuffer::set_size(const Size2D &size_, GLenum format_)
{
   size = size_;
   if (format_ != 0)
      format = format_;

   init();
}

void Framebuffer::init()
{
   GLenum status;

   glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
   if (image != 0)
   {
      glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
      glDeleteTextures(1, &image);
   }

   glGenTextures(1, &image);
   glBindTexture(GL_TEXTURE_2D, image);

   if (size.width == 0)
      size.width = 1;
   if (size.height == 0)
      size.height = 1;

   levels = glslang_num_miplevels(size.width, size.height);
   if (max_levels < levels)
      levels = max_levels;
   if (levels == 0)
      levels = 1;

   glTexStorage2D(GL_TEXTURE_2D, levels,
                  format,
                  size.width, size.height);

   glFramebufferTexture2D(GL_FRAMEBUFFER,
         GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);

   status   = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   complete = true;

   if (status != GL_FRAMEBUFFER_COMPLETE)
   {
      complete = false;

      switch (status)
      {
         case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            RARCH_ERR("[GLCore]: Incomplete attachment.\n");
            break;

         case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            RARCH_ERR("[GLCore]: Incomplete, missing attachment.\n");
            break;

         case GL_FRAMEBUFFER_UNSUPPORTED:
            {
               unsigned levels;

               RARCH_ERR("[GLCore]: Unsupported FBO, falling back to RGBA8.\n");

               glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
               glDeleteTextures(1, &image);
               glGenTextures(1, &image);
               glBindTexture(GL_TEXTURE_2D, image);

               levels = glslang_num_miplevels(size.width, size.height);
               if (max_levels < levels)
                  levels = max_levels;
               glTexStorage2D(GL_TEXTURE_2D, levels,
                     GL_RGBA8,
                     size.width, size.height);
               glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
               complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            }
            break;
      }
   }

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glBindTexture(GL_TEXTURE_2D, 0);
}

Framebuffer::~Framebuffer()
{
   if (framebuffer != 0)
      glDeleteFramebuffers(1, &framebuffer);
   if (image != 0)
      glDeleteTextures(1, &image);
}

class UBORing
{
public:
   ~UBORing();
   std::vector<GLuint> buffers;
   unsigned buffer_index = 0;
};

UBORing::~UBORing()
{
   glDeleteBuffers((GLsizei)buffers.size(), buffers.data());
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

   const Framebuffer &get_framebuffer() const
   {
      return *framebuffer;
   }

   Framebuffer *get_feedback_framebuffer()
   {
      return framebuffer_feedback.get();
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

   void set_rotation(uint32_t rot)
   {
      rotation = rot;
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
#endif // GL3_ROLLING_SCANLINE_SIMULATION  

   void set_name(const char *name)
   {
      pass_name = name;
   }

   const std::string &get_name() const
   {
      return pass_name;
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

   void add_parameter(unsigned parameter_index, const std::string &id);

   void end_frame();
   void allocate_buffers();

private:
   bool final_pass;

   Size2D get_output_size(const Size2D &original_size,
                          const Size2D &max_source) const;

   GLuint pipeline                 = 0;
   CommonResources *common         = nullptr;

   Size2D current_framebuffer_size = {};
   gl3_viewport current_viewport;
   gl3_filter_chain_pass_info pass_info;

   std::vector<uint32_t> vertex_shader;
   std::vector<uint32_t> fragment_shader;
   std::unique_ptr<Framebuffer> framebuffer;
   std::unique_ptr<Framebuffer> framebuffer_feedback;

   bool init_pipeline();

   void set_semantic_texture(slang_texture_semantic semantic,
         const Texture &texture);

   slang_reflection reflection;

   std::vector<uint8_t> uniforms;

   void build_semantics(uint8_t *buffer,
                        const float *mvp,
                        const Texture &original, const Texture &source);
   void build_semantic_vec4(uint8_t *data, slang_semantic semantic,
                            unsigned width, unsigned height);
   void build_semantic_uint(uint8_t *data,
         slang_semantic semantic, uint32_t value);
   void build_semantic_int(uint8_t *data,
         slang_semantic semantic, int32_t value);
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
   uint32_t rotation = 0;
   unsigned pass_number = 0;
   uint32_t total_subframes = 1;
   uint32_t current_subframe = 1;
#ifdef GL3_ROLLING_SCANLINE_SIMULATION  
   bool simulate_scanline = false;
#endif // GL3_ROLLING_SCANLINE_SIMULATION  

   size_t ubo_offset = 0;
   std::string pass_name;

   struct Parameter
   {
      std::string id;
      unsigned index;
      unsigned semantic_index;
   };

   std::vector<Parameter> parameters;
   std::vector<Parameter> filtered_parameters;
   std::vector<uint8_t> push_constant_buffer;
   gl3_buffer_locations locations = {};
   UBORing ubo_ring;

   void reflect_parameter(const std::string &name, slang_semantic_meta &meta);
   void reflect_parameter(const std::string &name, slang_texture_semantic_meta &meta);
   void reflect_parameter_array(const char *name, std::vector<slang_texture_semantic_meta> &meta);
};

bool Pass::build()
{
   std::unordered_map<std::string, slang_semantic_map> semantic_map;
   unsigned i;
   unsigned j = 0;

   framebuffer.reset();
   framebuffer_feedback.reset();

   if (!final_pass)
      framebuffer = std::unique_ptr<Framebuffer>(
            new Framebuffer(pass_info.rt_format, pass_info.max_levels));

   for (i = 0; i < parameters.size(); i++)
   {
      if (!slang_set_unique_map(semantic_map, parameters[i].id,
               slang_semantic_map{ SLANG_SEMANTIC_FLOAT_PARAMETER, j }))
         return false;
      j++;
   }

   reflection                              = slang_reflection{};
   reflection.pass_number                  = pass_number;
   reflection.texture_semantic_map         = &common->texture_semantic_map;
   reflection.texture_semantic_uniform_map = &common->texture_semantic_uniform_map;
   reflection.semantic_map                 = &semantic_map;

   if (!slang_reflect_spirv(vertex_shader, fragment_shader, &reflection))
      return false;

   /* Filter out parameters which we will never use anyways. */
   filtered_parameters.clear();

   for (i = 0; i < reflection.semantic_float_parameters.size(); i++)
   {
      if (reflection.semantic_float_parameters[i].uniform ||
          reflection.semantic_float_parameters[i].push_constant)
         filtered_parameters.push_back(parameters[i]);
   }

   if (!init_pipeline())
      return false;

   return true;
}

void Pass::reflect_parameter(const std::string &name, slang_semantic_meta &meta)
{
   if (meta.uniform)
   {
      int vert = glGetUniformLocation(pipeline, (std::string("RARCH_UBO_VERTEX_INSTANCE.") + name).c_str());
      int frag = glGetUniformLocation(pipeline, (std::string("RARCH_UBO_FRAGMENT_INSTANCE.") + name).c_str());

      if (vert >= 0)
         meta.location.ubo_vertex = vert;
      if (frag >= 0)
         meta.location.ubo_fragment = frag;
   }

   if (meta.push_constant)
   {
      int vert = glGetUniformLocation(pipeline, (std::string("RARCH_PUSH_VERTEX_INSTANCE.") + name).c_str());
      int frag = glGetUniformLocation(pipeline, (std::string("RARCH_PUSH_FRAGMENT_INSTANCE.") + name).c_str());

      if (vert >= 0)
         meta.location.push_vertex = vert;
      if (frag >= 0)
         meta.location.push_fragment = frag;
   }
}

void Pass::reflect_parameter(const std::string &name, slang_texture_semantic_meta &meta)
{
   if (meta.uniform)
   {
      int vert = glGetUniformLocation(pipeline, (std::string("RARCH_UBO_VERTEX_INSTANCE.") + name).c_str());
      int frag = glGetUniformLocation(pipeline, (std::string("RARCH_UBO_FRAGMENT_INSTANCE.") + name).c_str());

      if (vert >= 0)
         meta.location.ubo_vertex = vert;
      if (frag >= 0)
         meta.location.ubo_fragment = frag;
   }

   if (meta.push_constant)
   {
      int vert = glGetUniformLocation(pipeline, (std::string("RARCH_PUSH_VERTEX_INSTANCE.") + name).c_str());
      int frag = glGetUniformLocation(pipeline, (std::string("RARCH_PUSH_FRAGMENT_INSTANCE.") + name).c_str());

      if (vert >= 0)
         meta.location.push_vertex = vert;
      if (frag >= 0)
         meta.location.push_fragment = frag;
   }
}

void Pass::reflect_parameter_array(const char *name, std::vector<slang_texture_semantic_meta> &meta)
{
   size_t i;
   for (i = 0; i < meta.size(); i++)
   {
      char n[128];
      snprintf(n, sizeof(n), "%s%u", name, (unsigned)i);
      slang_texture_semantic_meta *m = (slang_texture_semantic_meta*)&meta[i];

      if (m->uniform)
      {
         int vert, frag;
         char vert_n[256];
         char frag_n[256];
         snprintf(vert_n, sizeof(vert_n), "RARCH_UBO_VERTEX_INSTANCE.%s", n);
         snprintf(frag_n, sizeof(frag_n), "RARCH_UBO_FRAGMENT_INSTANCE.%s", n);
         vert = glGetUniformLocation(pipeline, vert_n);
         frag = glGetUniformLocation(pipeline, frag_n);

         if (vert >= 0)
            m->location.ubo_vertex = vert;
         if (frag >= 0)
            m->location.ubo_fragment = frag;
      }

      if (m->push_constant)
      {
         int vert, frag;
         char vert_n[256];
         char frag_n[256];
         snprintf(vert_n, sizeof(vert_n), "RARCH_PUSH_VERTEX_INSTANCE.%s", n);
         snprintf(frag_n, sizeof(frag_n), "RARCH_PUSH_FRAGMENT_INSTANCE.%s", n);
         vert = glGetUniformLocation(pipeline, vert_n);
         frag = glGetUniformLocation(pipeline, frag_n);

         if (vert >= 0)
            m->location.push_vertex = vert;
         if (frag >= 0)
            m->location.push_fragment = frag;
      }
   }
}

bool Pass::init_pipeline()
{
   pipeline = gl3_cross_compile_program(
         vertex_shader.data(),   vertex_shader.size()   * sizeof(uint32_t),
         fragment_shader.data(), fragment_shader.size() * sizeof(uint32_t),
         &locations, false);

   if (!pipeline)
      return false;

   uniforms.resize(reflection.ubo_size);
   if (reflection.ubo_size)
   {
      unsigned i;
      size_t    size = reflection.ubo_size;
      unsigned count = 16;

      ubo_ring.buffers.resize(count);
      glGenBuffers(count, ubo_ring.buffers.data());

      for (i = 0; i < ubo_ring.buffers.size(); i++)
      {
         glBindBuffer(GL_UNIFORM_BUFFER, ubo_ring.buffers[i]);
         glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STREAM_DRAW);
      }

      glBindBuffer(GL_UNIFORM_BUFFER, 0);
   }
   push_constant_buffer.resize(reflection.push_constant_size);

   reflect_parameter("MVP", reflection.semantics[SLANG_SEMANTIC_MVP]);
   reflect_parameter("OutputSize", reflection.semantics[SLANG_SEMANTIC_OUTPUT]);
   reflect_parameter("FinalViewportSize", reflection.semantics[SLANG_SEMANTIC_FINAL_VIEWPORT]);
   reflect_parameter("FrameCount", reflection.semantics[SLANG_SEMANTIC_FRAME_COUNT]);
   reflect_parameter("FrameDirection", reflection.semantics[SLANG_SEMANTIC_FRAME_DIRECTION]);
   reflect_parameter("Rotation", reflection.semantics[SLANG_SEMANTIC_ROTATION]);
   reflect_parameter("TotalSubFrames", reflection.semantics[SLANG_SEMANTIC_TOTAL_SUBFRAMES]);
   reflect_parameter("CurrentSubFrame", reflection.semantics[SLANG_SEMANTIC_CURRENT_SUBFRAME]);

   reflect_parameter("OriginalSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL][0]);
   reflect_parameter("SourceSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_SOURCE][0]);
   reflect_parameter_array("OriginalHistorySize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY]);
   reflect_parameter_array("PassOutputSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT]);
   reflect_parameter_array("PassFeedbackSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK]);
   reflect_parameter_array("UserSize", reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_USER]);
   for (auto &m : common->texture_semantic_uniform_map)
   {
      auto &array = reflection.semantic_textures[m.second.semantic];
      if (m.second.index < array.size())
         reflect_parameter(m.first, array[m.second.index]);
   }

   for (auto &m : filtered_parameters)
      if (m.semantic_index < reflection.semantic_float_parameters.size())
         reflect_parameter(m.id, reflection.semantic_float_parameters[m.semantic_index]);

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
         width = (retroarch_get_rotation() % 2 ? current_viewport.height : current_viewport.width) * pass_info.scale_x;
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
         height = (retroarch_get_rotation() % 2 ? current_viewport.width : current_viewport.height) * pass_info.scale_y;
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
   swap(framebuffer, framebuffer_feedback);
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
               (push_constant_buffer.data() + refl->push_constant_offset);
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
         *reinterpret_cast<float *>(push_constant_buffer.data() + refl->push_constant_offset) = value;
   }
}

void Pass::build_semantic_uint(uint8_t *data, slang_semantic semantic,
                               uint32_t value)
{
   auto &refl = reflection.semantics[semantic];

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
         *reinterpret_cast<uint32_t *>(push_constant_buffer.data() + refl.push_constant_offset) = value;
   }
}

void Pass::build_semantic_int(uint8_t *data, slang_semantic semantic,
                              int32_t value)
{
   auto &refl = reflection.semantics[semantic];

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
         *reinterpret_cast<int32_t *>(push_constant_buffer.data() + refl.push_constant_offset) = value;
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
   auto &refl = reflection.semantic_textures[semantic];
   if (index >= refl.size())
      return;

   if (data && refl[index].uniform)
   {
      if (refl[index].location.ubo_vertex >= 0 || refl[index].location.ubo_fragment >= 0)
      {
         float v4[4];
         v4[0] = (float)(width);
         v4[1] = (float)(height);
         v4[2] = 1.0f / (float)(width);
         v4[3] = 1.0f / (float)(height);
         if (refl[index].location.ubo_vertex >= 0)
            glUniform4fv(refl[index].location.ubo_vertex, 1, v4);
         if (refl[index].location.ubo_fragment >= 0)
            glUniform4fv(refl[index].location.ubo_fragment, 1, v4);
      }
      else
      {
         float *_data = reinterpret_cast<float *>(data + refl[index].ubo_offset);
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }

   if (refl[index].push_constant)
   {
      if (refl[index].location.push_vertex >= 0 || refl[index].location.push_fragment >= 0)
      {
         float v4[4];
         v4[0] = (float)(width);
         v4[1] = (float)(height);
         v4[2] = 1.0f / (float)(width);
         v4[3] = 1.0f / (float)(height);
         if (refl[index].location.push_vertex >= 0)
            glUniform4fv(refl[index].location.push_vertex, 1, v4);
         if (refl[index].location.push_fragment >= 0)
            glUniform4fv(refl[index].location.push_fragment, 1, v4);
      }
      else
      {
         float *_data = reinterpret_cast<float *>(push_constant_buffer.data() + refl[index].push_constant_offset);
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

   framebuffer_feedback = std::unique_ptr<Framebuffer>(
         new Framebuffer(pass_info.rt_format, pass_info.max_levels));
   return true;
}

Pass::~Pass()
{
   if (pipeline != 0)
      glDeleteProgram(pipeline);
}

void Pass::set_shader(GLenum stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   switch (stage)
   {
      case GL_VERTEX_SHADER:
         vertex_shader.clear();
         vertex_shader.insert(end(vertex_shader),
               spirv, spirv + spirv_words);
         break;
      case GL_FRAGMENT_SHADER:
         fragment_shader.clear();
         fragment_shader.insert(end(fragment_shader),
               spirv, spirv + spirv_words);
         break;
      default:
         break;
   }
}

void Pass::add_parameter(unsigned index, const std::string &id)
{
   parameters.push_back({ id, index, unsigned(parameters.size()) });
}

void Pass::set_semantic_texture(slang_texture_semantic semantic,
      const Texture &texture)
{
   if (reflection.semantic_textures[semantic][0].texture)
   {
      unsigned binding = reflection.semantic_textures[semantic][0].binding;
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

   if (index < reflection.semantic_textures[semantic].size() &&
         reflection.semantic_textures[semantic][index].texture)
   {
      unsigned binding = reflection.semantic_textures[semantic][index].binding;
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
         memcpy(push_constant_buffer.data() + offset,
               mvp, sizeof(float) * 16);
      else
         gl3_build_default_matrix(reinterpret_cast<float *>(
                  push_constant_buffer.data() + offset));
   }

   /* Output information */
   build_semantic_vec4(buffer, SLANG_SEMANTIC_OUTPUT,
                       current_framebuffer_size.width,
                       current_framebuffer_size.height);
   build_semantic_vec4(buffer, SLANG_SEMANTIC_FINAL_VIEWPORT,
                       unsigned(current_viewport.width),
                       unsigned(current_viewport.height));

   build_semantic_uint(buffer, SLANG_SEMANTIC_FRAME_COUNT,
                       frame_count_period 
                       ? uint32_t(frame_count % frame_count_period) 
                       : uint32_t(frame_count));

   build_semantic_int(buffer, SLANG_SEMANTIC_FRAME_DIRECTION,
                      frame_direction);

   build_semantic_uint(buffer, SLANG_SEMANTIC_ROTATION,
                      rotation);

   build_semantic_uint(buffer, SLANG_SEMANTIC_TOTAL_SUBFRAMES,
                      total_subframes);
   build_semantic_uint(buffer, SLANG_SEMANTIC_CURRENT_SUBFRAME,
                      current_subframe);

   /* Standard inputs */
   build_semantic_texture(buffer, SLANG_TEXTURE_SEMANTIC_ORIGINAL, original);
   build_semantic_texture(buffer, SLANG_TEXTURE_SEMANTIC_SOURCE, source);

   /* ORIGINAL_HISTORY[0] is an alias of ORIGINAL. */
   build_semantic_texture_array(buffer,
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0, original);

   /* Parameters. */
   for (i = 0; i < filtered_parameters.size(); i++)
      build_semantic_parameter(buffer,
            filtered_parameters[i].semantic_index,
            common->shader_preset->parameters[
            filtered_parameters[i].index].current);

   /* Previous inputs. */
   for (i = 0; i < common->original_history.size(); i++)
      build_semantic_texture_array(buffer,
            SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, i + 1,
            common->original_history[i]);

   /* Previous passes. */
   for (i = 0; i < common->pass_outputs.size(); i++)
      build_semantic_texture_array(buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i,
            common->pass_outputs[i]);

   /* Feedback FBOs. */
   for (i = 0; i < common->framebuffer_feedback.size(); i++)
      build_semantic_texture_array(buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i,
            common->framebuffer_feedback[i]);

   /* LUTs. */
   for (i = 0; i < common->luts.size(); i++)
      build_semantic_texture_array(buffer,
            SLANG_TEXTURE_SEMANTIC_USER, i,
            common->luts[i]->get_texture());
}

void Pass::build_commands(
      const Texture &original,
      const Texture &source,
      const gl3_viewport &vp,
      const float *mvp)
{
   current_viewport = vp;
   Size2D size      = get_output_size(
         { original.texture.width, original.texture.height },
         { source.texture.width, source.texture.height });

   if (framebuffer &&
       (size.width  != framebuffer->get_size().width ||
        size.height != framebuffer->get_size().height))
      framebuffer->set_size(size);

   current_framebuffer_size = size;

   glUseProgram(pipeline);

   build_semantics(uniforms.data(), mvp, original, source);

   if (locations.flat_ubo_vertex >= 0)
      glUniform4fv(locations.flat_ubo_vertex,
                   GLsizei((reflection.ubo_size + 15) / 16),
                   reinterpret_cast<const float *>(uniforms.data()));

   if (locations.flat_ubo_fragment >= 0)
      glUniform4fv(locations.flat_ubo_fragment,
                   GLsizei((reflection.ubo_size + 15) / 16),
                   reinterpret_cast<const float *>(uniforms.data()));

   if (locations.flat_push_vertex >= 0)
      glUniform4fv(locations.flat_push_vertex,
                   GLsizei((reflection.push_constant_size + 15) / 16),
                   reinterpret_cast<const float *>(push_constant_buffer.data()));

   if (locations.flat_push_fragment >= 0)
      glUniform4fv(locations.flat_push_fragment,
                   GLsizei((reflection.push_constant_size + 15) / 16),
                   reinterpret_cast<const float *>(push_constant_buffer.data()));

   if (!(      locations.buffer_index_ubo_vertex   == GL_INVALID_INDEX 
            && locations.buffer_index_ubo_fragment == GL_INVALID_INDEX))
   {
      /* UBO Ring - update and bind */
      unsigned vertex_binding   = locations.buffer_index_ubo_vertex;
      unsigned fragment_binding = locations.buffer_index_ubo_fragment;
      const void *data          = uniforms.data();
      size_t size               = reflection.ubo_size;
      GLuint id                 = ubo_ring.buffers[ubo_ring.buffer_index];

      glBindBuffer(GL_UNIFORM_BUFFER, id);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
      if (vertex_binding != GL_INVALID_INDEX)
         glBindBufferBase(GL_UNIFORM_BUFFER, vertex_binding, id);
      if (fragment_binding != GL_INVALID_INDEX)
         glBindBufferBase(GL_UNIFORM_BUFFER, fragment_binding, id);

      ubo_ring.buffer_index++;
      if (ubo_ring.buffer_index >= ubo_ring.buffers.size())
         ubo_ring.buffer_index = 0;
   }

   /* The final pass is always executed inside
    * another render pass since the frontend will
    * want to overlay various things on top for
    * the passes that end up on-screen. */
   if (!final_pass && framebuffer->is_complete())
   {
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->get_framebuffer());
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
   }

#ifdef GL3_ROLLING_SCANLINE_SIMULATION 
   if (simulate_scanline)
   {
      glEnable(GL_SCISSOR_TEST);
   }
#endif // GL3_ROLLING_SCANLINE_SIMULATION 

   if (final_pass)
   {
      glViewport(current_viewport.x, current_viewport.y,
                 current_viewport.width, current_viewport.height);
#ifdef GL3_ROLLING_SCANLINE_SIMULATION  
      if (simulate_scanline)
      {
         glScissor(  current_viewport.x,
                     int32_t((float(current_viewport.height) / float(total_subframes)) 
                              * float(current_subframe - 1)),
                     current_viewport.width,
                     uint32_t(float(current_viewport.height) / float(total_subframes))
         );
      }
      else
      {
         glScissor(  current_viewport.x, current_viewport.y, 
                     current_viewport.width, current_viewport.height);
      }  
#endif // GL3_ROLLING_SCANLINE_SIMULATION  
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
#endif // GL3_ROLLING_SCANLINE_SIMULATION  
   }

#if !defined(HAVE_OPENGLES)
   if (framebuffer && framebuffer->get_format() == GL_SRGB8_ALPHA8)
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
#endif // GL3_ROLLING_SCANLINE_SIMULATION 

#if !defined(HAVE_OPENGLES)
   glDisable(GL_FRAMEBUFFER_SRGB);
#endif

   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   if (!final_pass)
      if (framebuffer->get_levels() > 1)
      {
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
         glBindTexture(GL_TEXTURE_2D, framebuffer->get_image());
         glGenerateMipmap(GL_TEXTURE_2D);
         glBindTexture(GL_TEXTURE_2D, 0);
      }
}

}

struct gl3_filter_chain
{
public:
   gl3_filter_chain(unsigned num_passes) { set_num_passes(num_passes); }

   inline void set_shader_preset(std::unique_ptr<video_shader> shader)
   {
      common.shader_preset = std::move(shader);
   }

   inline video_shader *get_shader_preset()
   {
      return common.shader_preset.get();
   }

   void set_pass_info(unsigned pass,
                      const gl3_filter_chain_pass_info &info);
   void set_shader(unsigned pass, GLenum stage,
                   const uint32_t *spirv, size_t spirv_words);

   bool init();

   void set_input_texture(const gl3_filter_chain_texture &texture);
   void build_offscreen_passes(const gl3_viewport &vp);
   void build_viewport_pass(const gl3_viewport &vp, const float *mvp);
   void end_frame();

   void set_frame_count(uint64_t count);
   void set_frame_count_period(unsigned pass, unsigned period);
   void set_frame_direction(int32_t direction);
   void set_rotation(uint32_t rot);
   void set_shader_subframes(uint32_t tot_subframes);
   void set_current_shader_subframe(uint32_t cur_subframe);
#ifdef GL3_ROLLING_SCANLINE_SIMULATION 
   void set_simulate_scanline(bool simulate);
#endif // GL3_ROLLING_SCANLINE_SIMULATION 
   void set_pass_name(unsigned pass, const char *name);

   void add_static_texture(std::unique_ptr<gl3_shader::StaticTexture> texture);
   void add_parameter(unsigned pass, unsigned parameter_index, const std::string &id);
   void set_num_passes(unsigned passes);

private:
   std::vector<std::unique_ptr<gl3_shader::Pass>> passes;
   std::vector<gl3_filter_chain_pass_info> pass_info;
   std::vector<std::vector<std::function<void ()>>> deferred_calls;
   std::unique_ptr<gl3_shader::Framebuffer> copy_framebuffer;
   gl3_shader::CommonResources common;

   gl3_filter_chain_texture input_texture = {};

   bool init_history();
   bool init_feedback();
   bool init_alias();
   std::vector<std::unique_ptr<gl3_shader::Framebuffer>> original_history;
   bool require_clear = false;
   void clear_history_and_feedback();
   void update_feedback_info();
   void update_history_info();
};


void gl3_filter_chain::update_history_info()
{
   unsigned i;

   for (i = 0; i < original_history.size(); i++)
   {
      gl3_shader::Texture *source = (gl3_shader::Texture*)
         &common.original_history[i];

      if (!source)
         continue;

      source->texture.image  = original_history[i]->get_image();
      source->texture.width  = original_history[i]->get_size().width;
      source->texture.height = original_history[i]->get_size().height;
      source->filter         = passes.front()->get_source_filter();
      source->mip_filter     = passes.front()->get_mip_filter();
      source->address        = passes.front()->get_address_mode();
   }
}

void gl3_filter_chain::update_feedback_info()
{
   unsigned i;

   for (i = 0; i < passes.size() - 1; i++)
   {
      gl3_shader::Framebuffer *fb = passes[i]->get_feedback_framebuffer();
      if (!fb)
         continue;

      gl3_shader::Texture *source = (gl3_shader::Texture*)
         &common.framebuffer_feedback[i];

      if (!source)
         continue;

      source->texture.image  = fb->get_image();
      source->texture.width  = fb->get_size().width;
      source->texture.height = fb->get_size().height;
      source->filter         = passes[i]->get_source_filter();
      source->mip_filter     = passes[i]->get_mip_filter();
      source->address        = passes[i]->get_address_mode();
   }
}

void gl3_filter_chain::build_offscreen_passes(const gl3_viewport &vp)
{
   unsigned i;

   /* First frame, make sure our history and feedback textures 
    * are in a clean state. */
   if (require_clear)
   {
      clear_history_and_feedback();
      require_clear = false;
   }

   update_history_info();
   if (!common.framebuffer_feedback.empty())
      update_feedback_info();

   const gl3_shader::Texture original = {
         input_texture,
         passes.front()->get_source_filter(),
         passes.front()->get_mip_filter(),
         passes.front()->get_address_mode(),
   };
   gl3_shader::Texture source = original;

   for (i = 0; i < passes.size() - 1; i++)
   {
      passes[i]->build_commands(original, source, vp, nullptr);

      const gl3_shader::Framebuffer &fb   = passes[i]->get_framebuffer();

      source.texture.image             = fb.get_image();
      source.texture.width             = fb.get_size().width;
      source.texture.height            = fb.get_size().height;
      source.filter                    = passes[i + 1]->get_source_filter();
      source.mip_filter                = passes[i + 1]->get_mip_filter();
      source.address                   = passes[i + 1]->get_address_mode();

      common.pass_outputs[i]           = source;
   }
}

void gl3_filter_chain::end_frame()
{
   /* If we need to keep old frames, copy it after fragment is complete.
    * TODO: We can improve pipelining by figuring out which
    * pass is the last that reads from
    * the history and dispatch the copy earlier. */
   if (!original_history.empty())
   {
      /* Update history */
      std::unique_ptr<gl3_shader::Framebuffer> tmp;
      std::unique_ptr<gl3_shader::Framebuffer> &back = original_history.back();
      swap(back, tmp);

      if (input_texture.width      != tmp->get_size().width  ||
            input_texture.height     != tmp->get_size().height ||
            (input_texture.format    != 0 
             && input_texture.format != tmp->get_format()))
         tmp->set_size({ input_texture.width, input_texture.height }, input_texture.format);

      if (tmp->is_complete())
         gl3_framebuffer_copy(
               tmp->get_framebuffer(),
               common.quad_program,
               common.quad_vbo,
               common.quad_loc.flat_ubo_vertex,
               tmp->get_size(),
               input_texture.image);

      /* Should ring buffer, but we don't have *that* many passes. */
      move_backward(begin(original_history), end(original_history) - 1, end(original_history));
      swap(original_history.front(), tmp);
   }
}

void gl3_filter_chain::build_viewport_pass(
      const gl3_viewport &vp, const float *mvp)
{
   unsigned i;
   /* First frame, make sure our history and 
    * feedback textures are in a clean state. */
   if (require_clear)
   {
      clear_history_and_feedback();
      require_clear = false;
   }

   gl3_shader::Texture source;
   const gl3_shader::Texture original = {
         input_texture,
         passes.front()->get_source_filter(),
         passes.front()->get_mip_filter(),
         passes.front()->get_address_mode(),
   };

   if (passes.size() == 1)
   {
      source = {
            input_texture,
            passes.back()->get_source_filter(),
            passes.back()->get_mip_filter(),
            passes.back()->get_address_mode(),
      };
   }
   else
   {
      const gl3_shader::Framebuffer &fb = passes[passes.size() - 2]
         ->get_framebuffer();
      source.texture.image           = fb.get_image();
      source.texture.width           = fb.get_size().width;
      source.texture.height          = fb.get_size().height;
      source.filter                  = passes.back()->get_source_filter();
      source.mip_filter              = passes.back()->get_mip_filter();
      source.address                 = passes.back()->get_address_mode();
   }

   passes.back()->build_commands(original, source, vp, mvp);

   /* For feedback FBOs, swap current and previous. */
   for (i = 0; i < passes.size(); i++)
   {
      gl3_shader::Framebuffer *fb = passes[i]->get_feedback_framebuffer();
      if (fb)
         passes[i]->end_frame();
   }
}

bool gl3_filter_chain::init_history()
{
   unsigned i;
   size_t required_images = 0;

   original_history.clear();
   common.original_history.clear();

   for (i = 0; i < passes.size(); i++)
      required_images =
            std::max(required_images,
                passes[i]->get_reflection().semantic_textures[
                SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size());

   if (required_images < 2)
   {
      RARCH_LOG("[GLCore]: Not using frame history.\n");
      return true;
   }

   /* We don't need to store array element #0,
    * since it's aliased with the actual original. */
   required_images--;
   original_history.reserve(required_images);
   common.original_history.resize(required_images);

   for (i = 0; i < required_images; i++)
      original_history.emplace_back(new gl3_shader::Framebuffer(0, 1));

   RARCH_LOG("[GLCore]: Using history of %u frames.\n", unsigned(required_images));

   /* On first frame, we need to clear the textures to
    * a known state, but we need
    * a command buffer for that, so just defer to first frame.
    */
   require_clear = true;
   return true;
}

bool gl3_filter_chain::init_feedback()
{
   unsigned i;
   bool use_feedbacks = false;

   common.framebuffer_feedback.clear();

   /* Final pass cannot have feedback. */
   for (i = 0; i < passes.size() - 1; i++)
   {
      bool use_feedback = false;
      for (auto &pass : passes)
      {
         auto &r          = pass->get_reflection();
         auto &feedbacks  = r.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK];

         if (i < feedbacks.size() && feedbacks[i].texture)
         {
            use_feedback  = true;
            use_feedbacks = true;
            break;
         }
      }

      if (use_feedback && !passes[i]->init_feedback())
         return false;

      if (use_feedback)
         RARCH_LOG("[GLCore]: Using framebuffer feedback for pass #%u.\n", i);
   }

   if (!use_feedbacks)
   {
      RARCH_LOG("[GLCore]: Not using framebuffer feedback.\n");
      return true;
   }

   common.framebuffer_feedback.resize(passes.size() - 1);
   require_clear = true;
   return true;
}

bool gl3_filter_chain::init_alias()
{
   int i;
    
   common.texture_semantic_map.clear();
   common.texture_semantic_uniform_map.clear();

   for (i = 0; i < (int)passes.size(); i++)
   {
      unsigned j;
      const std::string name = passes[i]->get_name();
      if (name.empty())
         continue;

      j = (unsigned)(&passes[i] - passes.data());

      if (!slang_set_unique_map(common.texture_semantic_map, name,
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, j }))
         return false;

      if (!slang_set_unique_map(common.texture_semantic_uniform_map,
               name + "Size",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, j }))
         return false;

      if (!slang_set_unique_map(common.texture_semantic_map,
               name + "Feedback",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, j }))
         return false;

      if (!slang_set_unique_map(common.texture_semantic_uniform_map,
               name + "FeedbackSize",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, j }))
         return false;
   }

   for (i = 0; i < (int)common.luts.size(); i++)
   {
      unsigned j = (unsigned)(&common.luts[i] - common.luts.data());
      if (!slang_set_unique_map(common.texture_semantic_map,
               common.luts[i]->get_id(),
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_USER, j }))
         return false;

      if (!slang_set_unique_map(common.texture_semantic_uniform_map,
               common.luts[i]->get_id() + "Size",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_USER, j }))
         return false;
   }

   return true;
}

void gl3_filter_chain::set_pass_info(unsigned pass, const gl3_filter_chain_pass_info &info)
{
   if (pass >= pass_info.size())
      pass_info.resize(pass + 1);
   pass_info[pass] = info;
}

void gl3_filter_chain::set_num_passes(unsigned num_passes)
{
   unsigned i;

   pass_info.resize(num_passes);
   passes.reserve(num_passes);

   for (i = 0; i < num_passes; i++)
   {
      passes.emplace_back(new gl3_shader::Pass(i + 1 == num_passes));
      passes.back()->set_common_resources(&common);
      passes.back()->set_pass_number(i);
   }
}

void gl3_filter_chain::set_shader(unsigned pass, GLenum stage, const uint32_t *spirv, size_t spirv_words)
{
   passes[pass]->set_shader(stage, spirv, spirv_words);
}

void gl3_filter_chain::add_parameter(unsigned pass,
      unsigned index, const std::string &id)
{
   passes[pass]->add_parameter(index, id);
}

bool gl3_filter_chain::init()
{
   unsigned i;

   if (!init_alias())
      return false;

   for (i = 0; i < passes.size(); i++)
   {
      RARCH_LOG("[slang]: Building pass #%u (%s)\n", i,
            passes[i]->get_name().empty() ?
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) :
            passes[i]->get_name().c_str());

      passes[i]->set_pass_info(pass_info[i]);
      if (!passes[i]->build())
         return false;
   }

   require_clear = false;
   if (!init_history())
      return false;
   if (!init_feedback())
      return false;
   common.pass_outputs.resize(passes.size());
   return true;
}

void gl3_filter_chain::clear_history_and_feedback()
{
   unsigned i;
   for (i = 0; i < original_history.size(); i++)
   {
      if (original_history[i]->is_complete())
      {
         GLuint id = original_history[i]->get_framebuffer();
         glBindFramebuffer(GL_FRAMEBUFFER, id);
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
         glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
         glClear(GL_COLOR_BUFFER_BIT);
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }
   }
   for (i = 0; i < passes.size(); i++)
   {
      gl3_shader::Framebuffer *fb = passes[i]->get_feedback_framebuffer();
      if (fb && fb->is_complete())
      {
         GLuint id = fb->get_framebuffer();
         glBindFramebuffer(GL_FRAMEBUFFER, id);
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
         glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
         glClear(GL_COLOR_BUFFER_BIT);
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }
   }
}

void gl3_filter_chain::set_input_texture(
      const gl3_filter_chain_texture &texture)
{
   input_texture = texture;

   /* Need a copy to remove padding.
    * GL HW render interface in libretro is kinda garbage now ... */
   if (input_texture.padded_width  != input_texture.width ||
       input_texture.padded_height != input_texture.height)
   {
      if (!copy_framebuffer)
         copy_framebuffer.reset(new gl3_shader::Framebuffer(texture.format, 1));

      if (input_texture.width   != copy_framebuffer->get_size().width  ||
          input_texture.height  != copy_framebuffer->get_size().height ||
          (input_texture.format != 0                                   &&
           input_texture.format != copy_framebuffer->get_format()))
         copy_framebuffer->set_size({ input_texture.width, input_texture.height }, input_texture.format);

      if (copy_framebuffer->is_complete())
         gl3_framebuffer_copy_partial(
               copy_framebuffer->get_framebuffer(),
               common.quad_program,
               common.quad_loc.flat_ubo_vertex,
               copy_framebuffer->get_size(),
               input_texture.image,
               float(input_texture.width) 
               / input_texture.padded_width,
               float(input_texture.height) 
               / input_texture.padded_height);
      input_texture.image = copy_framebuffer->get_image();
   }
}

void gl3_filter_chain::add_static_texture(std::unique_ptr<gl3_shader::StaticTexture> texture)
{
   common.luts.push_back(std::move(texture));
}

void gl3_filter_chain::set_frame_count(uint64_t count)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_frame_count(count);
}

void gl3_filter_chain::set_frame_count_period(unsigned pass, unsigned period)
{
   passes[pass]->set_frame_count_period(period);
}

void gl3_filter_chain::set_frame_direction(int32_t direction)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_frame_direction(direction);
}

void gl3_filter_chain::set_rotation(uint32_t rot)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_rotation(rot);
}

void gl3_filter_chain::set_shader_subframes(uint32_t tot_subframes)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_shader_subframes(tot_subframes);
}

void gl3_filter_chain::set_current_shader_subframe(uint32_t cur_subframe)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_current_shader_subframe(cur_subframe);
}

#ifdef GL3_ROLLING_SCANLINE_SIMULATION  
void gl3_filter_chain::set_simulate_scanline(bool simulate_scanline)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_simulate_scanline(simulate_scanline);
}
#endif // GL3_ROLLING_SCANLINE_SIMULATION  

void gl3_filter_chain::set_pass_name(unsigned pass, const char *name)
{
   passes[pass]->set_name(name);
}

static std::unique_ptr<gl3_shader::StaticTexture> gl3_filter_chain_load_lut(
      gl3_filter_chain *chain,
      const video_shader_lut *shader)
{
   texture_image image;
   GLuint tex                      = 0;

   image.width                     = 0;
   image.height                    = 0;
   image.pixels                    = NULL;
   image.supports_rgba             = true;

   if (!image_texture_load(&image, shader->path))
      return {};

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

   return std::unique_ptr<gl3_shader::StaticTexture>(new gl3_shader::StaticTexture(shader->id,
            tex, image.width, image.height,
            shader->filter != RARCH_FILTER_NEAREST,
            levels > 1,
            rarch_wrap_to_address(shader->wrap)));
}

static bool gl3_filter_chain_load_luts(
      gl3_filter_chain *chain,
      video_shader *shader)
{
   unsigned i;
   for (i = 0; i < shader->luts; i++)
   {
      std::unique_ptr<gl3_shader::StaticTexture> image = gl3_filter_chain_load_lut(chain, &shader->lut[i]);
      if (!image)
      {
         RARCH_ERR("[GLCore]: Failed to load LUT \"%s\".\n", shader->lut[i].path);
         return false;
      }

      chain->add_static_texture(std::move(image));
   }

   return true;
}

gl3_filter_chain_t *gl3_filter_chain_create_default(
      glslang_filter_chain_filter filter)
{
   struct gl3_filter_chain_pass_info pass_info;

   std::unique_ptr<gl3_filter_chain> chain{ new gl3_filter_chain(1) };
   if (!chain)
      return nullptr;

   pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_x       = 1.0f;
   pass_info.scale_y       = 1.0f;
   pass_info.rt_format     = 0;
   pass_info.source_filter = filter;
   pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_NEAREST;
   pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;
   pass_info.max_levels    = 0;

   chain->set_pass_info(0, pass_info);

   chain->set_shader(0, GL_VERTEX_SHADER,
         gl3_shader::opaque_vert,
         sizeof(gl3_shader::opaque_vert) / sizeof(uint32_t));
   chain->set_shader(0, GL_FRAGMENT_SHADER,
         gl3_shader::opaque_frag,
         sizeof(gl3_shader::opaque_frag) / sizeof(uint32_t));

   if (!chain->init())
      return nullptr;

   return chain.release();
}

gl3_filter_chain_t *gl3_filter_chain_create_from_preset(
      const char *path, glslang_filter_chain_filter filter)
{
   unsigned i;
   std::unique_ptr<video_shader> shader{ new video_shader() };
   if (!shader)
      return nullptr;

   if (!video_shader_load_preset_into_shader(path, shader.get()))
      return nullptr;

   bool last_pass_is_fbo = shader->pass[shader->passes - 1].fbo.flags &
      FBO_SCALE_FLAG_VALID;

   std::unique_ptr<gl3_filter_chain> chain{ new gl3_filter_chain(shader->passes + (last_pass_is_fbo ? 1 : 0)) };
   if (!chain)
      return nullptr;

   if (      shader->luts 
         && !gl3_filter_chain_load_luts(chain.get(), shader.get()))
      return nullptr;

   shader->num_parameters = 0;

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

      if (!glslang_compile_shader(pass->source.path, &output))
      {
         RARCH_ERR("[GLCore]: Failed to compile shader: \"%s\".\n",
               pass->source.path);
         return nullptr;
      }

      for (auto &meta_param : output.meta.parameters)
      {
         if (shader->num_parameters >= GFX_MAX_PARAMETERS)
         {
            RARCH_ERR("[GLCore]: Exceeded maximum number of parameters (%u).\n", GFX_MAX_PARAMETERS);
            return nullptr;
         }

         auto itr = std::find_if(shader->parameters, shader->parameters + shader->num_parameters,
               [&](const video_shader_parameter &param)
               {
                  return meta_param.id == param.id;
               });

         if (itr != shader->parameters + shader->num_parameters)
         {
            /* Allow duplicate #pragma parameter, but
             * only if they are exactly the same. */
            if (meta_param.desc    != itr->desc    ||
                meta_param.initial != itr->initial ||
                meta_param.minimum != itr->minimum ||
                meta_param.maximum != itr->maximum ||
                meta_param.step    != itr->step)
            {
               RARCH_ERR("[GLCore]: Duplicate parameters found for \"%s\", but arguments do not match.\n",
                     itr->id);
               return nullptr;
            }
            chain->add_parameter(i, (unsigned)(itr - shader->parameters), meta_param.id);
         }
         else
         {
            video_shader_parameter *param = &shader->parameters[shader->num_parameters];
            strlcpy(param->id, meta_param.id.c_str(), sizeof(param->id));
            strlcpy(param->desc, meta_param.desc.c_str(), sizeof(param->desc));
            param->initial = meta_param.initial;
            param->minimum = meta_param.minimum;
            param->maximum = meta_param.maximum;
            param->step    = meta_param.step;
            chain->add_parameter(i, shader->num_parameters, meta_param.id);
            shader->num_parameters++;
         }
      }

      chain->set_shader(i,
            GL_VERTEX_SHADER,
            output.vertex.data(),
            output.vertex.size());

      chain->set_shader(i,
            GL_FRAGMENT_SHADER,
            output.fragment.data(),
            output.fragment.size());

      chain->set_frame_count_period(i, pass->frame_count_mod);

      if (!output.meta.name.empty())
         chain->set_pass_name(i, output.meta.name.c_str());

      /* Preset overrides. */
      if (*pass->alias)
         chain->set_pass_name(i, pass->alias);

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
               RARCH_WARN("[slang]: Using explicit format for last pass in chain,"
                     " but it is not rendered to framebuffer, using swapchain format instead.\n");
         }
         else
         {
            pass_info.rt_format = gl3_shader::convert_glslang_format(output.meta.rt_format);
            RARCH_LOG("[slang]: Using render target format %s for pass output #%u.\n",
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

         pass_info.rt_format = gl3_shader::convert_glslang_format(output.meta.rt_format);
         RARCH_LOG("[slang]: Using render target format %s for pass output #%u.\n",
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

      chain->set_pass_info(i, pass_info);
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

      chain->set_pass_info(shader->passes, pass_info);

      chain->set_shader(shader->passes,
            GL_VERTEX_SHADER,
            gl3_shader::opaque_vert,
            sizeof(gl3_shader::opaque_vert) / sizeof(uint32_t));

      chain->set_shader(shader->passes,
            GL_FRAGMENT_SHADER,
            gl3_shader::opaque_frag,
            sizeof(gl3_shader::opaque_frag) / sizeof(uint32_t));
   }

   chain->set_shader_preset(std::move(shader));

   if (!chain->init())
      return nullptr;

   return chain.release();
}

struct video_shader *gl3_filter_chain_get_preset(
      gl3_filter_chain_t *chain) { return chain->get_shader_preset(); }
void gl3_filter_chain_free(gl3_filter_chain_t *chain) { delete chain; }

void gl3_filter_chain_set_shader(
      gl3_filter_chain_t *chain,
      unsigned pass,
      GLenum shader_stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   chain->set_shader(pass, shader_stage, spirv, spirv_words);
}

void gl3_filter_chain_set_pass_info(
      gl3_filter_chain_t *chain,
      unsigned pass,
      const struct gl3_filter_chain_pass_info *info)
{
   chain->set_pass_info(pass, *info);
}

bool gl3_filter_chain_init(gl3_filter_chain_t *chain)
{
   return chain->init();
}

void gl3_filter_chain_set_input_texture(
      gl3_filter_chain_t *chain,
      const struct gl3_filter_chain_texture *texture)
{
   chain->set_input_texture(*texture);
}

void gl3_filter_chain_set_frame_count(
      gl3_filter_chain_t *chain,
      uint64_t count)
{
   chain->set_frame_count(count);
}

void gl3_filter_chain_set_frame_direction(
      gl3_filter_chain_t *chain,
      int32_t direction)
{
   chain->set_frame_direction(direction);
}

void gl3_filter_chain_set_rotation(
      gl3_filter_chain_t *chain,
      uint32_t rot)
{
   chain->set_rotation(rot);
}

void gl3_filter_chain_set_shader_subframes(
      gl3_filter_chain_t *chain,
      uint32_t tot_subframes)
{
   chain->set_shader_subframes(tot_subframes);
}

void gl3_filter_chain_set_current_shader_subframe(
      gl3_filter_chain_t *chain,
      uint32_t cur_subframe)
{
   chain->set_current_shader_subframe(cur_subframe);
}

#ifdef GL3_ROLLING_SCANLINE_SIMULATION  
void gl3_filter_chain_set_simulate_scanline(
      gl3_filter_chain_t *chain,
      bool simulate_scanline)
{
   chain->set_simulate_scanline(simulate_scanline);
}
#endif // GL3_ROLLING_SCANLINE_SIMULATION  

void gl3_filter_chain_set_frame_count_period(
      gl3_filter_chain_t *chain,
      unsigned pass,
      unsigned period)
{
   chain->set_frame_count_period(pass, period);
}

void gl3_filter_chain_set_pass_name(
      gl3_filter_chain_t *chain,
      unsigned pass,
      const char *name)
{
   chain->set_pass_name(pass, name);
}

void gl3_filter_chain_build_offscreen_passes(
      gl3_filter_chain_t *chain,
      const gl3_viewport *vp)
{
   chain->build_offscreen_passes(*vp);
}

void gl3_filter_chain_build_viewport_pass(
      gl3_filter_chain_t *chain,
      const gl3_viewport *vp, const float *mvp)
{
   chain->build_viewport_pass(*vp, mvp);
}

void gl3_filter_chain_end_frame(gl3_filter_chain_t *chain)
{
   chain->end_frame();
}
