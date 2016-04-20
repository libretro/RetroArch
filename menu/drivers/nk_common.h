/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
 *  Copyright (C) 2016      - Andrés Suárez
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

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

#include "../../deps/zahnrad/nuklear.h"
#include "../../deps/stb/stb_image.h"

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../../gfx/common/gl_common.h"
#endif

#define MAX_VERTEX_MEMORY     (512 * 1024)
#define MAX_ELEMENT_MEMORY    (128 * 1024)

#define NK_SYSTEM_TAB_END     NK_SYSTEM_TAB_SETTINGS

 struct nk_device
 {
   struct nk_buffer cmds;
   struct nk_draw_null_texture null;
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLuint vbo, vao, ebo;

   GLuint prog;
   GLuint vert_shdr;
   GLuint frag_shdr;

   GLint attrib_pos;
   GLint attrib_uv;
   GLint attrib_col;

   GLint uniform_proj;
   GLuint font_tex;
#endif
};

extern struct nk_font font;
extern struct nk_user_font usrfnt;
extern struct nk_allocator nk_alloc;
extern struct nk_device device;

struct nk_image nk_common_image_load(const char *filename);

char* nk_common_file_load(const char* path, size_t* size);

void nk_common_device_init(struct nk_device *dev);
struct nk_user_font nk_common_font(
   struct nk_device *dev,
   struct nk_font *font,
   const char *path,
   unsigned int font_height,
   const nk_rune *range);

void nk_common_device_shutdown(struct nk_device *dev);

void nk_common_device_draw(struct nk_device *dev,
   struct nk_context *ctx, int width, int height,
   enum nk_anti_aliasing AA);

void* nk_common_mem_alloc(nk_handle a, void *old, nk_size b);

void nk_common_mem_free(nk_handle unused, void *ptr);
