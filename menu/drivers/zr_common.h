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


 #include "../../deps/zahnrad/zahnrad.h"
 #include "../../deps/stb/stb_image.h"

 #if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
 #include "../../gfx/common/gl_common.h"
 #endif

 #define MAX_VERTEX_MEMORY     (512 * 1024)
 #define MAX_ELEMENT_MEMORY    (128 * 1024)

 #define ZR_SYSTEM_TAB_END     ZR_SYSTEM_TAB_SETTINGS

 struct zr_device
 {
    struct zr_buffer cmds;
    struct zr_draw_null_texture null;
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

struct zr_font font;
struct zr_user_font usrfnt;
struct zr_allocator zr_alloc;
struct zr_device device;

struct zr_image zr_common_image_load(const char *filename);

char* zr_common_file_load(const char* path, size_t* size);

void zr_common_device_init(struct zr_device *dev);
struct zr_user_font zr_common_font(
   struct zr_device *dev,
   struct zr_font *font,
   const char *path,
   unsigned int font_height,
   const zr_rune *range);

void zr_common_device_shutdown(struct zr_device *dev);

void zr_common_device_draw(struct zr_device *dev,
   struct zr_context *ctx, int width, int height,
   enum zr_anti_aliasing AA);

void* zr_common_mem_alloc(zr_handle unused, size_t size);

void zr_common_mem_free(zr_handle unused, void *ptr);
