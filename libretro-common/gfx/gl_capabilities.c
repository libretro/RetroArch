/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gl_capabilities.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include <boolean.h>

#include <glsym/glsym.h>

#include <gfx/gl_capabilities.h>

static bool gl_core_context       = false;

bool gl_query_core_context_in_use(void)
{
   return gl_core_context;
}

void gl_query_core_context_set(bool set)
{
   gl_core_context     =  set;
}

void gl_query_core_context_unset(void)
{
   gl_core_context = false;
}

bool gl_query_extension(const char *ext)
{
   bool ret = false;

   if (gl_query_core_context_in_use())
   {
#ifdef GL_NUM_EXTENSIONS
      GLint i;
      GLint exts = 0;
      glGetIntegerv(GL_NUM_EXTENSIONS, &exts);
      for (i = 0; i < exts; i++)
      {
         const char *str = (const char*)glGetStringi(GL_EXTENSIONS, i);
         if (str && strstr(str, ext))
         {
            ret = true;
            break;
         }
      }
#endif
   }
   else
   {
      const char *str = (const char*)glGetString(GL_EXTENSIONS);
      ret = str && strstr(str, ext);
   }

   return ret;
}

bool gl_check_error(char **error_string)
{
   int error = glGetError();
   switch (error)
   {
      case GL_INVALID_ENUM:
         *error_string = strdup("GL: Invalid enum.");
         break;
      case GL_INVALID_VALUE:
         *error_string = strdup("GL: Invalid value.");
         break;
      case GL_INVALID_OPERATION:
         *error_string = strdup("GL: Invalid operation.");
         break;
      case GL_OUT_OF_MEMORY:
         *error_string = strdup("GL: Out of memory.");
         break;
      case GL_NO_ERROR:
         return true;
      default:
         *error_string = strdup("Non specified GL error.");
         break;
   }

   return false;
}

bool gl_check_capability(enum gl_capability_enum enum_idx)
{
   unsigned major       = 0;
   unsigned minor       = 0;
   const char *vendor   = (const char*)glGetString(GL_VENDOR);
   const char *renderer = (const char*)glGetString(GL_RENDERER);
   const char *version  = (const char*)glGetString(GL_VERSION);
#ifdef HAVE_OPENGLES
   if (version && sscanf(version, "OpenGL ES %u.%u", &major, &minor) != 2)
#else
   if (version && sscanf(version, "%u.%u", &major, &minor) != 2)
#endif
      major = minor = 0;

   (void)vendor;
   (void)renderer;

   switch (enum_idx)
   {
      case GL_CAPS_GLES3_SUPPORTED:
#if defined(HAVE_OPENGLES)
         if (major >= 3)
            return true;
#endif
         break;
      case GL_CAPS_EGLIMAGE:
#if defined(HAVE_EGL) && defined(HAVE_OPENGLES)
         if (glEGLImageTargetTexture2DOES != NULL)
            return true;
#endif
         break;
      case GL_CAPS_SYNC:
#ifdef HAVE_OPENGLES
         if (major >= 3)
            return true;
#else
         if (gl_query_extension("ARB_sync") &&
               glFenceSync && glDeleteSync && glClientWaitSync)
            return true;
#endif
         break;
      case GL_CAPS_MIPMAP:
         {
            static bool extension_queried = false;
            static bool extension         = false;

            if (!extension_queried)
            {
               extension         = gl_query_extension("ARB_framebuffer_object");
               extension_queried = true;
            }

            if (extension)
               return true;
         }
         break;
      case GL_CAPS_VAO:
#ifndef HAVE_OPENGLES
         if (!gl_query_core_context_in_use() && !gl_query_extension("ARB_vertex_array_object"))
            return false;

         if (glGenVertexArrays && glBindVertexArray && glDeleteVertexArrays)
            return true;
#endif
         break;
      case GL_CAPS_FBO:
#if defined(HAVE_PSGL) || defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
         return true;
#else
         if (     !gl_query_core_context_in_use()
               && !gl_query_extension("ARB_framebuffer_object")
               && !gl_query_extension("EXT_framebuffer_object"))
            return false;

         if (gl_query_extension("ARB_framebuffer_object"))
            return true;

         if (gl_query_extension("EXT_framebuffer_object"))
            return true;

         if (major >= 3)
            return true;
         break;
#endif
      case GL_CAPS_ARGB8:
#ifdef HAVE_OPENGLES
         if (gl_query_extension("OES_rgb8_rgba8")
               || gl_query_extension("ARM_rgba8")
                  || major >= 3)
            return true;
#else
         /* TODO/FIXME - implement this for non-GLES? */
#endif
         break;
      case GL_CAPS_DEBUG:
         if (gl_query_extension("KHR_debug"))
            return true;
#ifndef HAVE_OPENGLES
         if (gl_query_extension("ARB_debug_output"))
            return true;
#endif
         break;
      case GL_CAPS_PACKED_DEPTH_STENCIL:
         if (major >= 3)
            return true;
         if (gl_query_extension("OES_packed_depth_stencil"))
            return true;
         if (gl_query_extension("EXT_packed_depth_stencil"))
            return true;
         break;
      case GL_CAPS_ES2_COMPAT:
#ifndef HAVE_OPENGLES
         /* ATI card detected, skipping check for GL_RGB565 support... */
         if (vendor && renderer && (strstr(vendor, "ATI") || strstr(renderer, "ATI")))
            return false;

         if (gl_query_extension("ARB_ES2_compatibility"))
            return true;
#endif
         break;
      case GL_CAPS_UNPACK_ROW_LENGTH:
#ifdef HAVE_OPENGLES
         if (major >= 3)
            return true;

         /* Extension GL_EXT_unpack_subimage, can copy textures faster
          * than using UNPACK_ROW_LENGTH */
         if (gl_query_extension("GL_EXT_unpack_subimage"))
            return true;
#endif
         break;
      case GL_CAPS_FULL_NPOT_SUPPORT:
         if (major >= 3)
            return true;
#ifdef HAVE_OPENGLES
         if (gl_query_extension("ARB_texture_non_power_of_two") ||
               gl_query_extension("OES_texture_npot"))
            return true;
#else
         {
            GLint max_texture_size = 0;
            GLint max_native_instr = 0;
            /* try to detect actual npot support. might fail for older cards. */
            bool  arb_npot         = gl_query_extension("ARB_texture_non_power_of_two");
            bool  arb_frag_program = gl_query_extension("ARB_fragment_program");

            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

#ifdef GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB
            if (arb_frag_program && glGetProgramivARB)
               glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
                     GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, &max_native_instr);
#endif

            if (arb_npot && arb_frag_program &&
                  (max_texture_size >= 8192) && (max_native_instr >= 4096))
               return true;
         }
#endif
         break;
      case GL_CAPS_SRGB_FBO_ES3:
#ifdef HAVE_OPENGLES
         if (major >= 3)
            return true;
#else
         break;
#endif
      case GL_CAPS_SRGB_FBO:
#if defined(HAVE_OPENGLES)
         if (major >= 3 || gl_query_extension("EXT_sRGB"))
            return true;
#endif
         if (gl_check_capability(GL_CAPS_FBO))
         {
            if (   gl_query_core_context_in_use() ||
                  (gl_query_extension("EXT_texture_sRGB")
                   && gl_query_extension("ARB_framebuffer_sRGB"))
               )
               return true;
         }
         break;
      case GL_CAPS_FP_FBO:
         if (gl_check_capability(GL_CAPS_FBO))
         {
            /* Float FBO is core in 3.2. */
            if (gl_query_core_context_in_use() || gl_query_extension("ARB_texture_float") || gl_query_extension("OES_texture_float_linear"))
               return true;
         }
         break;
      case GL_CAPS_BGRA8888:
#ifdef HAVE_OPENGLES
         /* There are both APPLE and EXT variants. */
         if (gl_query_extension("BGRA8888"))
            return true;
#else
         return true;
#endif
         break;
      case GL_CAPS_TEX_STORAGE:
#ifdef HAVE_OPENGLES
         if (major >= 3)
            return true;
#else
         if (vendor && strstr(vendor, "ATI Technologies"))
            return false;
         if (gl_query_extension("ARB_texture_storage"))
            return true;
#endif
         break;
      case GL_CAPS_TEX_STORAGE_EXT:
#ifdef TARGET_OS_IPHONE
           /* Not working on iOS */
           return false;
#else
         if (gl_query_extension("EXT_texture_storage"))
            return true;
#endif
         break;
      case GL_CAPS_NONE:
      default:
         break;
   }

   return false;
}
