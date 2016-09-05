/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#ifndef _GL_CAPABILITIES_H
#define _GL_CAPABILITIES_H

#include <boolean.h>
#include <retro_common_api.h>

#include <glsym/glsym.h>

#if defined(HAVE_FBO) && defined(HAVE_PSGL)
#define glGenFramebuffers glGenFramebuffersOES
#define glBindFramebuffer glBindFramebufferOES
#define glFramebufferTexture2D glFramebufferTexture2DOES
#define glCheckFramebufferStatus glCheckFramebufferStatusOES
#define glDeleteFramebuffers glDeleteFramebuffersOES
#define glGenRenderbuffers glGenRenderbuffersOES
#define glBindRenderbuffer glBindRenderbufferOES
#define glFramebufferRenderbuffer glFramebufferRenderbufferOES
#define glRenderbufferStorage glRenderbufferStorageOES
#define glDeleteRenderbuffers glDeleteRenderbuffersOES
#endif

#if (!defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3))
#ifdef GL_PIXEL_PACK_BUFFER
#define HAVE_GL_ASYNC_READBACK
#endif
#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#elif defined(OSX_PPC)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#else
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0
#endif

#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#if defined(HAVE_OPENGLES2)
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#else
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#endif
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#elif defined(OSX_PPC)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_EXT
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_EXT
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_EXT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_OES
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_SCE
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_OES
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_OES
#else
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#endif

#ifdef OSX_PPC
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_OES
#else
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE
#endif

#if defined(HAVE_PSGL)
#define glGenerateMipmap glGenerateMipmapOES
#endif

#ifdef HAVE_FBO

#if defined(__APPLE__) || defined(HAVE_PSGL)
#define GL_RGBA32F GL_RGBA32F_ARB
#endif

#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_INTERNAL_FORMAT32 GL_ARGB_SCE
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB5 /* TODO: Verify if this is really 565 or just 555. */
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_RGB5
#elif defined(HAVE_OPENGLES)
/* Imgtec/SGX headers have this missing. */
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifdef IOS
/* Stupid Apple. */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA
#else
#define RARCH_GL_INTERNAL_FORMAT32 GL_BGRA_EXT
#endif
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA_EXT
#define RARCH_GL_TEXTURE_TYPE16 GL_RGB
#define RARCH_GL_FORMAT32 GL_UNSIGNED_BYTE
#define RARCH_GL_FORMAT16 GL_UNSIGNED_SHORT_5_6_5
#else
/* On desktop, we always use 32-bit. */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA8
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGBA8
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_UNSIGNED_INT_8_8_8_8_REV

/* GL_RGB565 internal format isn't in desktop GL 
 * until 4.1 core (ARB_ES2_compatibility).
 * Check for this. */
#ifndef GL_RGB565
#define GL_RGB565 0x8D62
#endif
#define RARCH_GL_INTERNAL_FORMAT16_565 GL_RGB565
#define RARCH_GL_TEXTURE_TYPE16_565 GL_RGB
#define RARCH_GL_FORMAT16_565 GL_UNSIGNED_SHORT_5_6_5
#endif

/* Platform specific workarounds/hacks. */
#if defined(__CELLOS_LV2__)
#define NO_GL_READ_PIXELS
#endif

#if defined(HAVE_OPENGL_MODERN) || defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2) || defined(HAVE_PSGL)

#ifndef NO_GL_FF_VERTEX
#define NO_GL_FF_VERTEX
#endif

#ifndef NO_GL_FF_MATRIX
#define NO_GL_FF_MATRIX
#endif

#endif

#if defined(HAVE_OPENGLES2) /* TODO: Figure out exactly what. */
#define NO_GL_CLAMP_TO_BORDER
#endif

#if defined(HAVE_OPENGLES)
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH  0x0CF2
#endif

#ifndef GL_SRGB_ALPHA_EXT
#define GL_SRGB_ALPHA_EXT 0x8C42
#endif
#endif

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
   GL_CAPS_BGRA8888
};

RETRO_BEGIN_DECLS

bool gl_check_error(char *error_string);

bool gl_query_core_context_in_use(void);

void gl_query_core_context_set(bool set);

void gl_query_core_context_unset(void);

bool gl_check_capability(enum gl_capability_enum enum_idx);

RETRO_END_DECLS

#endif
