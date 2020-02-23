/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gl_capabilities.h).
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

#ifndef _GL_CAPABILITIES_H
#define _GL_CAPABILITIES_H

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum gl_capability_enum
{
   GL_CAPS_NONE = 0,
   GL_CAPS_EGLIMAGE,
   GL_CAPS_SYNC,
   GL_CAPS_MIPMAP,
   GL_CAPS_VAO,
   GL_CAPS_FBO,
   GL_CAPS_ARGB8,
   GL_CAPS_DEBUG,
   GL_CAPS_PACKED_DEPTH_STENCIL,
   GL_CAPS_ES2_COMPAT,
   GL_CAPS_UNPACK_ROW_LENGTH,
   GL_CAPS_FULL_NPOT_SUPPORT,
   GL_CAPS_SRGB_FBO,
   GL_CAPS_SRGB_FBO_ES3,
   GL_CAPS_FP_FBO,
   GL_CAPS_BGRA8888,
   GL_CAPS_GLES3_SUPPORTED,
   GL_CAPS_TEX_STORAGE,
   GL_CAPS_TEX_STORAGE_EXT
};

bool gl_query_core_context_in_use(void);

void gl_query_core_context_set(bool set);

void gl_query_core_context_unset(void);

bool gl_query_extension(const char *ext);

bool gl_check_error(char **error_string);

bool gl_check_capability(enum gl_capability_enum enum_idx);

RETRO_END_DECLS

#endif
