/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2017 - Hans-Kristian Arntzen
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

#include <string.h>
#include <stdlib.h>

#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "glslang_util.h"
#include "../../verbosity.h"

/* -------------------------------------------------------------------
 * shader_line_buf implementation
 * ------------------------------------------------------------------- */

#define SHADER_LINE_BUF_INITIAL_DATA_CAP  4096
#define SHADER_LINE_BUF_INITIAL_LINES_CAP 128

bool shader_line_buf_init(struct shader_line_buf *buf)
{
   size_t offsets_bytes = SHADER_LINE_BUF_INITIAL_LINES_CAP * sizeof(size_t);
   /* Allocate data and offsets in one block: [data | offsets] */
   char *block = (char*)malloc(SHADER_LINE_BUF_INITIAL_DATA_CAP + offsets_bytes);
   if (!block)
      return false;

   buf->data          = block;
   buf->len           = 0;
   buf->cap           = SHADER_LINE_BUF_INITIAL_DATA_CAP;

   buf->line_offsets  = (size_t*)(block + SHADER_LINE_BUF_INITIAL_DATA_CAP);
   buf->num_lines     = 0;
   buf->lines_cap     = SHADER_LINE_BUF_INITIAL_LINES_CAP;
   buf->_single_alloc = true;
   return true;
}

void shader_line_buf_free(struct shader_line_buf *buf)
{
   if (buf->_single_alloc)
   {
      free(buf->data);   /* frees the whole block */
   }
   else
   {
      if (buf->data)
         free(buf->data);
      if (buf->line_offsets)
         free(buf->line_offsets);
   }
   buf->data          = NULL;
   buf->line_offsets  = NULL;
   buf->len           = 0;
   buf->cap           = 0;
   buf->num_lines     = 0;
   buf->lines_cap     = 0;
   buf->_single_alloc = false;
}

static bool shader_line_buf_append_batch(struct shader_line_buf *buf,
      const char *const *lines, const size_t *lens, size_t count)
{
   size_t i;
   size_t total_data  = 0;

   /* Pre-calculate total bytes needed for all lines */
   for (i = 0; i < count; i++)
      total_data += lens[i] + 1; /* +1 for null terminator */

   /* Single data-capacity check */
   {
      size_t needed = buf->len + total_data;
      if (needed > buf->cap)
      {
         size_t new_cap = buf->cap;
         char  *tmp;
         while (new_cap < needed)
            new_cap *= 2;
         if (buf->_single_alloc)
         {
            size_t *old_offsets = buf->line_offsets;
            size_t  off_bytes  = buf->lines_cap * sizeof(size_t);
            tmp = (char*)malloc(new_cap);
            if (!tmp)
               return false;
            memcpy(tmp, buf->data, buf->len);
            buf->line_offsets = (size_t*)malloc(off_bytes);
            if (!buf->line_offsets)
            {
               free(tmp);
               buf->line_offsets = old_offsets;
               return false;
            }
            memcpy(buf->line_offsets, old_offsets, buf->num_lines * sizeof(size_t));
            free(buf->data); /* frees old combined block */
            buf->data          = tmp;
            buf->cap           = new_cap;
            buf->_single_alloc = false;
         }
         else
         {
            tmp = (char*)realloc(buf->data, new_cap);
            if (!tmp)
               return false;
            buf->data = tmp;
            buf->cap  = new_cap;
         }
      }
   }

   /* Single line_offsets capacity check */
   if (buf->num_lines + count > buf->lines_cap)
   {
      size_t  new_lcap = buf->lines_cap;
      while (new_lcap < buf->num_lines + count)
         new_lcap *= 2;
      if (buf->_single_alloc)
      {
         size_t *new_off = (size_t*)malloc(new_lcap * sizeof(size_t));
         if (!new_off)
            return false;
         memcpy(new_off, buf->line_offsets, buf->num_lines * sizeof(size_t));
         /* Don't free line_offsets—it's part of combined block;
          * data still points to start of the block. */
         /* Actually, we must split: allocate data separately too. */
         {
            char *new_data = (char*)malloc(buf->cap);
            if (!new_data)
            {
               free(new_off);
               return false;
            }
            memcpy(new_data, buf->data, buf->len);
            free(buf->data); /* frees combined block */
            buf->data = new_data;
         }
         buf->line_offsets  = new_off;
         buf->lines_cap     = new_lcap;
         buf->_single_alloc = false;
      }
      else
      {
         size_t *tmp = (size_t*)realloc(buf->line_offsets,
               new_lcap * sizeof(size_t));
         if (!tmp)
            return false;
         buf->line_offsets = tmp;
         buf->lines_cap   = new_lcap;
      }
   }

   /* Bulk copy all lines — no further checks needed */
   for (i = 0; i < count; i++)
   {
      buf->line_offsets[buf->num_lines++] = buf->len;
      memcpy(buf->data + buf->len, lines[i], lens[i]);
      buf->len              += lens[i];
      buf->data[buf->len++]  = '\0';
   }
   return true;
}

static bool shader_line_buf_append(struct shader_line_buf *buf,
      const char *line, size_t line_len)
{
   return shader_line_buf_append_batch(buf, &line, &line_len, 1);
}

const char *shader_line_buf_get(const struct shader_line_buf *buf, size_t index)
{
   return buf->data + buf->line_offsets[index];
}

/* -------------------------------------------------------------------
 * Original helper functions (unchanged)
 * ------------------------------------------------------------------- */

static char *slang_get_include_file(char *line, size_t len)
{
   char *end   = NULL;
   char *start = (char*)memchr(line, '\"', len);
   if (!start)
      return NULL;

   start++;
   len -= (size_t)(start - line);

   if (!(end = (char*)memchr(start, '\"', len)))
      return NULL;

   *end = '\0';
   return start;
}

bool slang_texture_semantic_is_array(enum slang_texture_semantic sem)
{
   switch (sem)
   {
      case SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY:
      case SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT:
      case SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK:
      case SLANG_TEXTURE_SEMANTIC_USER:
         return true;

      default:
         break;
   }

   return false;
}

enum slang_texture_semantic slang_name_to_texture_semantic_array(
      const char *name, const char **names, unsigned *index)
{
   unsigned i;

   for (i = 0; *names; i++, names++)
   {
      enum slang_texture_semantic semantic = (enum slang_texture_semantic)i;
      const char *n = name;
      const char *s = *names;

      /* Compare characters until the name string is exhausted */
      while (*s && *n == *s)
      {
         n++;
         s++;
      }

      /* If we didn't reach the end of *names, it's not a match */
      if (*s)
         continue;

      if (slang_texture_semantic_is_array(semantic))
      {
         /* Prefix matched — remainder is the array index */
         *index = (unsigned)strtoul(n, NULL, 10);
         return semantic;
      }
      else
      {
         /* Exact match required — name must also be exhausted */
         if (*n == '\0')
         {
            *index = 0;
            return semantic;
         }
      }
   }
   return SLANG_INVALID_TEXTURE_SEMANTIC;
}

/* -------------------------------------------------------------------
 * Static feature-define strings (compile-time constants)
 * ------------------------------------------------------------------- */
static const char _define_originalaspect[] = "#define _HAS_ORIGINALASPECT_UNIFORMS";
static const char _define_frametime[]      = "#define _HAS_FRAMETIME_UNIFORMS";
static const char _define_sensor[]         = "#define _HAS_SENSOR_UNIFORMS";
static const char _ext_line_directive[]    = "#extension GL_GOOGLE_cpp_style_line_directive : require";

#define DEFINE_ORIGINALASPECT_LEN (sizeof(_define_originalaspect) - 1)
#define DEFINE_FRAMETIME_LEN      (sizeof(_define_frametime) - 1)
#define DEFINE_SENSOR_LEN         (sizeof(_define_sensor) - 1)
#define EXT_LINE_DIRECTIVE_LEN    (sizeof(_ext_line_directive) - 1)

static bool emit_feature_defines(struct shader_line_buf *output)
{
   static const char *const lines[3] = {
      _define_originalaspect,
      _define_frametime,
      _define_sensor
   };
   static const size_t lens[3] = {
      DEFINE_ORIGINALASPECT_LEN,
      DEFINE_FRAMETIME_LEN,
      DEFINE_SENSOR_LEN
   };
   return shader_line_buf_append_batch(output, lines, lens, 3);
}

/* Build a "#line N \"file\"" directive into tmp using a precomputed suffix.
 * Returns the total length written. */
static size_t build_line_directive(char *tmp, size_t tmp_size,
      unsigned lineno, const char *suffix, size_t suffix_len)
{
   size_t n = (size_t)snprintf(tmp, tmp_size, "#line %u", lineno);
   if (n + suffix_len < tmp_size)
   {
      memcpy(tmp + n, suffix, suffix_len);
      n += suffix_len;
      tmp[n] = '\0';
   }
   return n;
}

/* -------------------------------------------------------------------
 * glslang_read_shader_file
 * ------------------------------------------------------------------- */
bool glslang_read_shader_file(const char *path,
      struct shader_line_buf *output, bool root_file, bool is_optional)
{
   char tmp[PATH_MAX_LENGTH];
   char line_suffix[PATH_MAX_LENGTH]; /* precomputed: " \"basename\"" */
   size_t line_suffix_len = 0;
   const char *basename      = NULL;
   uint8_t *buf              = NULL;
   int64_t buf_len           = 0;
   bool    ret               = false;

   tmp[0] = '\0';

   /* Sanity check */
   if (!path || path[0] == '\0' || !output)
      return false;

   basename = path_basename_nocompression(path);

   if (!basename || basename[0] == '\0')
      return false;

   /* Precompute the #line directive suffix: ' "basename"'
    * so the inner loop only needs to write the line number. */
   line_suffix_len = (size_t)snprintf(line_suffix, sizeof(line_suffix),
         " \"%s\"", basename);

   /* Read file contents */
   if (!filestream_read_file(path, (void**)&buf, &buf_len))
   {
      if (!is_optional)
         RARCH_ERR("[Slang] Failed to open shader file: \"%s\".\n", path);
      return false;
   }

   if (buf_len <= 0)
      goto cleanup;

   /* Null-terminate the buffer so we can work with it as a C string.
    * \r removal is folded into the line-scanning loop below to avoid
    * a separate O(n) pass over the data. */
   ((char*)buf)[buf_len] = '\0';

   {
      char *cursor     = (char*)buf;
      size_t line_idx  = 0;
      size_t line_len  = 0;
      bool first_line  = true;

      /* If this is the 'parent' shader file and a slang file,
       * ensure that first line is a 'VERSION' string */
      bool check_version = root_file
            && (strcmp(path_get_extension(path), "slang") == 0);

      while (*cursor != '\0')
      {
         char saved;
         char *line_start;
         char *newline;
         /* Strip \r from this line in-place while finding newline/end */
         char *rd = cursor;
         char *wr = cursor;
         while (*rd != '\n' && *rd != '\0')
         {
            if (*rd != '\r')
               *wr++ = *rd;
            rd++;
         }
         line_start = cursor;
         newline    = wr;    /* points to where the logical line ends */
         saved      = *rd;   /* save the real delimiter (\n or \0)   */
         /* Shift the delimiter to the compacted position */
         *wr        = *rd;

         /* Temporarily terminate the line */
         *newline = '\0';

         if (first_line)
         {
            size_t line_len = 0;
            first_line      = false;

            if (check_version)
            {
               if (strncmp("#version ", line_start, sizeof("#version ")-1))
               {
                  RARCH_ERR("[Slang] First line of the shader must contain a valid "
                        "#version string.\n");
                  goto cleanup;
               }

               if (!shader_line_buf_append(output, line_start,
                        (size_t)(newline - line_start)))
                  goto cleanup;

               /* Allows us to use #line to make dealing with shader
                * errors easier. */
               if (!shader_line_buf_append(output,
                     _ext_line_directive, EXT_LINE_DIRECTIVE_LEN))
                  goto cleanup;

               if (!emit_feature_defines(output))
                  goto cleanup;

               line_len = build_line_directive(tmp, sizeof(tmp),
                     2u, line_suffix, line_suffix_len);
               if (!shader_line_buf_append(output, tmp, line_len))
                  goto cleanup;

               /* Advance past this line in the real buffer */
               cursor = (saved == '\n') ? rd + 1 : rd;
               line_idx++;
               continue;
            }

            /* Non-root or non-slang: emit feature defines + #line once */
            if (!emit_feature_defines(output))
               goto cleanup;

            line_len = build_line_directive(tmp, sizeof(tmp),
                  root_file ? 2u : 1u, line_suffix, line_suffix_len);
            if (!shader_line_buf_append(output, tmp, line_len))
               goto cleanup;
         }

         /* Skip line 0 (the #version line) for root slang files —
          * already handled above */
         if (root_file && check_version && line_idx == 0)
         {
            cursor = (saved == '\n') ? rd + 1 : rd;
            line_idx++;
            continue;
         }

         /* Process the line */
         {
            size_t cur_line_len   = (size_t)(newline - line_start);
            bool include_optional = (cur_line_len >= sizeof("#pragma include_optional ")-1)
                  && !memcmp("#pragma include_optional ", line_start,
                        sizeof("#pragma include_optional ")-1);

            if (  ((cur_line_len >= sizeof("#include ")-1)
                  && !memcmp("#include ", line_start, sizeof("#include ")-1))
                  || include_optional)
            {
               char include_path[PATH_MAX_LENGTH];
               char *include_file = slang_get_include_file(
                     line_start, cur_line_len);

               if (!include_file || include_file[0] == '\0')
               {
                  RARCH_ERR("[Slang] Invalid include statement \"%s\".\n",
                        line_start);
                  goto cleanup;
               }

               include_path[0] = '\0';
               fill_pathname_resolve_relative(
                     include_path, path, include_file, sizeof(include_path));

               if (!glslang_read_shader_file(include_path, output,
                     false, include_optional))
               {
                  if (!include_optional)
                     goto cleanup;
                  RARCH_LOG("[Slang] Optional include not found \"%s\".\n",
                        include_path);
               }

               line_len = build_line_directive(tmp, sizeof(tmp),
                     (unsigned)(line_idx + 1), line_suffix, line_suffix_len);
               if (!shader_line_buf_append(output, tmp, line_len))
                  goto cleanup;
            }
            else if (  ((cur_line_len >= sizeof("#endif")-1)
                     && !memcmp("#endif", line_start, sizeof("#endif")-1))
                    || ((cur_line_len >= sizeof("#pragma")-1)
                     && !memcmp("#pragma", line_start, sizeof("#pragma")-1)))
            {
               if (!shader_line_buf_append(output, line_start, cur_line_len))
                  goto cleanup;
               line_len = build_line_directive(tmp, sizeof(tmp),
                     (unsigned)(line_idx + 2), line_suffix, line_suffix_len);
               if (!shader_line_buf_append(output, tmp, line_len))
                  goto cleanup;
            }
            else
            {
               if (!shader_line_buf_append(output, line_start, cur_line_len))
                  goto cleanup;
            }

            cursor = (saved == '\n') ? rd + 1 : rd;
            line_idx++;
            continue;
         }

         goto cleanup;
      }
   }

   ret = true;

cleanup:
   if (buf)
      free(buf);
   return ret;
}

/* -------------------------------------------------------------------
 * Format helpers
 * ------------------------------------------------------------------- */

const char *glslang_format_to_string(enum glslang_format fmt)
{
   static const char *glslang_formats[] = {
      "UNKNOWN",

      "R8_UNORM",
      "R8_UINT",
      "R8_SINT",
      "R8G8_UNORM",
      "R8G8_UINT",
      "R8G8_SINT",
      "R8G8B8A8_UNORM",
      "R8G8B8A8_UINT",
      "R8G8B8A8_SINT",
      "R8G8B8A8_SRGB",

      "A2B10G10R10_UNORM_PACK32",
      "A2B10G10R10_UINT_PACK32",

      "R16_UINT",
      "R16_SINT",
      "R16_SFLOAT",
      "R16G16_UINT",
      "R16G16_SINT",
      "R16G16_SFLOAT",
      "R16G16B16A16_UINT",
      "R16G16B16A16_SINT",
      "R16G16B16A16_SFLOAT",

      "R32_UINT",
      "R32_SINT",
      "R32_SFLOAT",
      "R32G32_UINT",
      "R32G32_SINT",
      "R32G32_SFLOAT",
      "R32G32B32A32_UINT",
      "R32G32B32A32_SINT",
      "R32G32B32A32_SFLOAT",
   };
   if ((unsigned)fmt >= sizeof(glslang_formats) / sizeof(glslang_formats[0]))
      return glslang_formats[0]; /* "UNKNOWN" */
   return glslang_formats[fmt];
}

enum glslang_format glslang_find_format(const char *fmt)
{
#undef FMT
#define FMT(x) do { \
   static const char s[] = #x; \
   if (memcmp(fmt, s, sizeof(s) - 1) == 0 && fmt[sizeof(s) - 1] == '\0') \
      return SLANG_FORMAT_ ## x; \
} while(0)

   switch (fmt[0])
   {
      case 'R':
         switch (fmt[1])
         {
            case '8':
               if (fmt[2] == '_')
               {
                  FMT(R8_UNORM);
                  FMT(R8_UINT);
                  FMT(R8_SINT);
               }
               else /* R8G8... */
               {
                  if (fmt[4] == '_') /* R8G8_xxx */
                  {
                     FMT(R8G8_UNORM);
                     FMT(R8G8_UINT);
                     FMT(R8G8_SINT);
                  }
                  else /* R8G8B8A8_xxx */
                  {
                     FMT(R8G8B8A8_UNORM);
                     FMT(R8G8B8A8_UINT);
                     FMT(R8G8B8A8_SINT);
                     FMT(R8G8B8A8_SRGB);
                  }
               }
               break;
            case '1': /* R16... */
               if (fmt[3] == '_')
               {
                  FMT(R16_UINT);
                  FMT(R16_SINT);
                  FMT(R16_SFLOAT);
               }
               else /* R16G16... */
               {
                  if (fmt[5] == '_') /* R16G16_xxx */
                  {
                     FMT(R16G16_UINT);
                     FMT(R16G16_SINT);
                     FMT(R16G16_SFLOAT);
                  }
                  else /* R16G16B16A16_xxx */
                  {
                     FMT(R16G16B16A16_UINT);
                     FMT(R16G16B16A16_SINT);
                     FMT(R16G16B16A16_SFLOAT);
                  }
               }
               break;
            case '3': /* R32... */
               if (fmt[3] == '_')
               {
                  FMT(R32_UINT);
                  FMT(R32_SINT);
                  FMT(R32_SFLOAT);
               }
               else
               {
                  if (fmt[5] == '_') /* R32G32_xxx */
                  {
                     FMT(R32G32_UINT);
                     FMT(R32G32_SINT);
                     FMT(R32G32_SFLOAT);
                  }
                  else /* R32G32B32A32_xxx */
                  {
                     FMT(R32G32B32A32_UINT);
                     FMT(R32G32B32A32_SINT);
                     FMT(R32G32B32A32_SFLOAT);
                  }
               }
               break;
         }
         break;
      case 'A': /* A2B10G10R10... */
         FMT(A2B10G10R10_UNORM_PACK32);
         FMT(A2B10G10R10_UINT_PACK32);
         break;
   }
   return SLANG_FORMAT_UNKNOWN;
}

unsigned glslang_num_miplevels(unsigned width, unsigned height)
{
   unsigned size = MAX(width, height);
   if (!size)
      return 0;
#if defined(__GNUC__) || defined(__clang__)
   return (unsigned)(8 * sizeof(unsigned)) - (unsigned)__builtin_clz(size);
#elif defined(_MSC_VER) && _MSC_VER >= 1300
   {
      unsigned long idx;
      _BitScanReverse(&idx, size);
      return (unsigned)idx + 1u;
   }
#else
   {
      unsigned levels = 0;
      if (size >= 0x10000u) { levels += 16; size >>= 16; }
      if (size >= 0x100u)   { levels +=  8; size >>=  8; }
      if (size >= 0x10u)    { levels +=  4; size >>=  4; }
      if (size >= 0x4u)     { levels +=  2; size >>=  2; }
      if (size >= 0x2u)     { levels +=  1; size >>=  1; }
      if (size)               levels +=  1;
      return levels;
   }
#endif
}
