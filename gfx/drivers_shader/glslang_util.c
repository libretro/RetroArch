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

static void get_include_file(
      const char *line, char *include_file, size_t len)
{
   char *end   = NULL;
   char *start = (char*)strchr(line, '\"');

   if (!start)
      return;

   start++;
   end = (char*)strchr(start, '\"');

   if (!end)
      return;

   *end = '\0';
   strlcpy(include_file, start, len);
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
      const char *name, const char **names,
      unsigned *index)
{
   unsigned i = 0;
   while (*names)
   {
      const char                        *n = *names;
      enum slang_texture_semantic semantic = (enum slang_texture_semantic)(i);

      if (slang_texture_semantic_is_array(semantic))
      {
         size_t baselen = strlen(n);
         int        cmp = strncmp(n, name, baselen);

         if (cmp == 0)
         {
            *index = (unsigned)strtoul(name + baselen, NULL, 0);
            return semantic;
         }
      }
      else if (string_is_equal(name, n))
      {
         *index = 0;
         return semantic;
      }

      i++;
      names++;
   }
   return SLANG_INVALID_TEXTURE_SEMANTIC;
}

/* inj_to_define
 * check if str is in format inj_char KEY = VALUE
 * (spaces are not mandatory)
 * returns true if str is in that format and fills "value" ans "key" accordingly
*/
bool inj_to_define(char *str, char *key, char *value, char inj_char) {
   // remove spaces tabs and double quotes
   int i = 0, j = 0;
   while (str[i] != '\0') {
      if (str[i] != ' ' && str[i] != '"' && str[i] != '\t') {
      str[j++] = str[i];
      }
      i++;
   }
   str[j] = '\0';

   // Does it start with inj_char
   if (*str != inj_char) return false;

   // Is it in format KEY=VALUE now?
   char *eq_pos = strchr(str, '=');
   if (!eq_pos) return false;

   // Extract KEY,VALUE
   *eq_pos = '\0';             // Substitute '=' with \0 to divide key and value
   strcpy(key, str + 1);       // copy key skipping inj_char
   strcpy(value, eq_pos + 1);  // copy value

   return true;
}

bool inject(const char *inject_file, struct string_list *output, const char *inject_prefix){
   
   /* Read inject file contents */
   union string_list_elem_attr attr;
   uint8_t *buf              = NULL;
   int64_t buf_len           = 0;
   struct string_list lines  = {0};
   bool    ret               = false;

   if (filestream_read_file(inject_file, (void**)&buf, &buf_len)) {
      string_list_initialize(&lines);  
      ret = string_separate_noalloc(&lines, (char*)buf, "\n");
   }

   if (buf) 
      free(buf);
   if (!ret) 
      return false;
   if (lines.size < 1)
      goto error;
	
   /* Cycle through its lines searching for matching format (*KEY = VALUE) */
   for (size_t j = 0; j < lines.size; j++) {
      char *line   = lines.elems[j].data;
      char inj_key[100];
      char inj_value[100];
      char tmp[350];  //size of: #undef $inj_key \n #define $inj_key $inj_value 
      
      /* Inject them */
      if (inj_to_define(line, inj_key, inj_value, *inject_prefix )) { 
         snprintf(tmp, sizeof(tmp), "#undef %s", inj_key);
         RARCH_LOG("[shader]: Injected #undef %s\n", inj_key);
         if (!string_list_append(output, tmp, attr)) goto error;
         
         snprintf(tmp, sizeof(tmp), "#define %s %s", inj_key, inj_value);
         if (!string_list_append(output, tmp, attr)) goto error;       
         RARCH_LOG("[shader]: Injected #define %s %s\n", inj_key, inj_value);
      }
         
   }
      
   return true;
   
   error:
      string_list_deinitialize(&lines);
      return false;
}

bool glslang_read_shader_file(const char *path,
      struct string_list *output, bool root_file, const char *preset_path)
{
   size_t i;
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   const char *basename      = NULL;
   uint8_t *buf              = NULL;
   int64_t buf_len           = 0;
   struct string_list lines  = {0};
   bool    ret               = false;

   tmp[0] = '\0';
   attr.i = 0;

   /* Sanity check */
   if (string_is_empty(path) || !output)
      return false;

   basename      = path_basename_nocompression(path);

   if (string_is_empty(basename))
      return false;

   /* Read file contents */
   if (!filestream_read_file(path, (void**)&buf, &buf_len))
   {
      RARCH_ERR("[slang]: Failed to open shader file: \"%s\".\n", path);
      return false;
   }

   if (buf_len > 0)
   {
      /* Remove Windows '\r' chars if we encounter them */
      string_remove_all_chars((char*)buf, '\r');

      /* Split into lines
       * (Blank lines must be included) */
      string_list_initialize(&lines);
      ret = string_separate_noalloc(&lines, (char*)buf, "\n");
   }

   /* Buffer is no longer required - clean up */
   if (buf)
      free(buf);

   /* Sanity check */
   if (!ret)
      return false;

   if (lines.size < 1)
      goto error;

   /* If this is the 'parent' shader file, ensure that first
    * line is a 'VERSION' string */
   if (root_file)
   {
      const char *line = lines.elems[0].data;

      if (strncmp("#version ", line, STRLEN_CONST("#version ")))
      {
         RARCH_ERR("[slang]: First line of the shader must contain a valid "
               "#version string.\n");
         goto error;
      }

      if (!string_list_append(output, line, attr))
         goto error;

      /* Allows us to use #line to make dealing with shader 
       * errors easier.
       * This is supported by glslang, but since we always 
       * use glslang statically, this is fine. */
      if (!string_list_append(output,
               "#extension GL_GOOGLE_cpp_style_line_directive : require",
               attr))
         goto error;
   }

   /* At least VIM treats the first line as line #1,
    * so offset everything by one. */
   snprintf(tmp, sizeof(tmp), "#line %u \"%s\"", root_file ? 2 : 1, basename);
   if (!string_list_append(output, tmp, attr))
      goto error;

   /* Loop through lines of file */
   for (i = root_file ? 1 : 0; i < lines.size; i++)
   {
      const char *line   = lines.elems[i].data;
      
      const char inject_prefix[] = "*" ;
      const char preset_defines_keyword[] = "#pragma inject_preset_code";
      
      /* Check for injection directives*/
      if (!strncmp(preset_defines_keyword, line, STRLEN_CONST(preset_defines_keyword)) && preset_path != NULL )
      {
         /* From preset path */
         inject(preset_path, output, inject_prefix);
         /* ...and from inc file */
         /* Strip existing extension and append .inc */
         char *inc_file_path = strdup(preset_path); 
         char *p_ptr = strrchr(inc_file_path, '.');
         if (p_ptr != NULL) *p_ptr = '\0';
         strcat(inc_file_path, ".inc");
         if (filestream_exists(inc_file_path)) //Does the inc file exist?
            inject(inc_file_path, output, inject_prefix); //Inject
      }
      
      /* Check for 'include' statements */
      if (!strncmp("#include ", line, STRLEN_CONST("#include ")))
      {
         char include_file[PATH_MAX_LENGTH];
         char include_path[PATH_MAX_LENGTH];

         include_file[0] = '\0';
         include_path[0] = '\0';

         /* Build include file path */
         get_include_file(line, include_file, sizeof(include_file));

         if (string_is_empty(include_file))
         {
            RARCH_ERR("[slang]: Invalid include statement \"%s\".\n", line);
            goto error;
         }

         fill_pathname_resolve_relative(
               include_path, path, include_file, sizeof(include_path));

         /* Parse include file */
         if (!glslang_read_shader_file(include_path, output, false, preset_path))
            goto error;

         /* After including a file, use line directive
          * to pull it back to current file. */
         snprintf(tmp, sizeof(tmp), "#line %u \"%s\"",
               (unsigned)(i + 1), basename);
         if (!string_list_append(output, tmp, attr))
            goto error;
      }
      else if (!strncmp("#endif", line, STRLEN_CONST("#endif")) ||
               !strncmp("#pragma", line, STRLEN_CONST("#pragma")))
      {
         /* #line seems to be ignored if preprocessor tests fail,
          * so we should reapply #line after each #endif.
          * Add extra offset here since we're setting #line
          * for the line after this one.
          */
         if (!string_list_append(output, line, attr))
            goto error;
         snprintf(tmp, sizeof(tmp), "#line %u \"%s\"",
               (unsigned)(i + 2), basename);
         if (!string_list_append(output, tmp, attr))
            goto error;
      }
      else
         if (!string_list_append(output, line, attr))
            goto error;
   }

   /* 
      if (preset_path != NULL) {
      RARCH_LOG("RAW SHADER------------------------------------------\n");
      for (size_t i = 0; i < output -> size; i++)
      {
         const char *line = output -> elems[i].data;
         RARCH_LOG("    %s\n", line);
      }
      RARCH_LOG("/RAW SHADER------------------------------------------/\n");
   }
   
   */
   string_list_deinitialize(&lines);
   return true;

error:
   string_list_deinitialize(&lines);
   return false;

}

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
   return glslang_formats[fmt];
}

enum glslang_format glslang_find_format(const char *fmt)
{
#undef FMT
#define FMT(x) if (string_is_equal(fmt, #x)) return SLANG_FORMAT_ ## x
   FMT(R8_UNORM);
   FMT(R8_UINT);
   FMT(R8_SINT);
   FMT(R8G8_UNORM);
   FMT(R8G8_UINT);
   FMT(R8G8_SINT);
   FMT(R8G8B8A8_UNORM);
   FMT(R8G8B8A8_UINT);
   FMT(R8G8B8A8_SINT);
   FMT(R8G8B8A8_SRGB);

   FMT(A2B10G10R10_UNORM_PACK32);
   FMT(A2B10G10R10_UINT_PACK32);

   FMT(R16_UINT);
   FMT(R16_SINT);
   FMT(R16_SFLOAT);
   FMT(R16G16_UINT);
   FMT(R16G16_SINT);
   FMT(R16G16_SFLOAT);
   FMT(R16G16B16A16_UINT);
   FMT(R16G16B16A16_SINT);
   FMT(R16G16B16A16_SFLOAT);

   FMT(R32_UINT);
   FMT(R32_SINT);
   FMT(R32_SFLOAT);
   FMT(R32G32_UINT);
   FMT(R32G32_SINT);
   FMT(R32G32_SFLOAT);
   FMT(R32G32B32A32_UINT);
   FMT(R32G32B32A32_SINT);
   FMT(R32G32B32A32_SFLOAT);

   return SLANG_FORMAT_UNKNOWN;
}

unsigned glslang_num_miplevels(unsigned width, unsigned height)
{
   unsigned size   = MAX(width, height);
   unsigned levels = 0;
   while (size)
   {
      levels++;
      size >>= 1;
   }
   return levels;
}
