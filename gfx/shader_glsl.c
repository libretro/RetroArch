/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "../boolean.h"
#include <string.h>
#include "../general.h"
#include "shader_glsl.h"
#include "../compat/strl.h"
#include "../compat/posix_string.h"
#include "state_tracker.h"
#include "../dynamic.h"

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#elif defined(HAVE_PSGL)
#include <PSGL/psgl.h>
#include <PSGL/psglu.h>
#include <GLES/glext.h>
#elif defined(HAVE_OPENGL_MODERN)
#include <EGL/egl.h>
#include <GL3/gl3.h>
#include <GL3/gl3ext.h>
#elif defined(HAVE_OPENGLES2)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#elif defined(HAVE_OPENGLES1)
#include <GLES/gl.h>
#include <GLES/glext.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include "gfx_context.h"
#include <stdlib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gl_common.h"
#include "image.h"

static PFNGLCREATEPROGRAMPROC pglCreateProgram = NULL;
static PFNGLUSEPROGRAMPROC pglUseProgram = NULL;
static PFNGLCREATESHADERPROC pglCreateShader = NULL;
static PFNGLDELETESHADERPROC pglDeleteShader = NULL;
static PFNGLSHADERSOURCEPROC pglShaderSource = NULL;
static PFNGLCOMPILESHADERPROC pglCompileShader = NULL;
static PFNGLATTACHSHADERPROC pglAttachShader = NULL;
static PFNGLDETACHSHADERPROC pglDetachShader = NULL;
static PFNGLLINKPROGRAMPROC pglLinkProgram = NULL;
static PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = NULL;
static PFNGLUNIFORM1IPROC pglUniform1i = NULL;
static PFNGLUNIFORM1FPROC pglUniform1f = NULL;
static PFNGLUNIFORM2FVPROC pglUniform2fv = NULL;
static PFNGLUNIFORM4FVPROC pglUniform4fv = NULL;
static PFNGLUNIFORMMATRIX4FVPROC pglUniformMatrix4fv = NULL;
static PFNGLGETSHADERIVPROC pglGetShaderiv = NULL;
static PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog = NULL;
static PFNGLGETPROGRAMIVPROC pglGetProgramiv = NULL;
static PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog = NULL;
static PFNGLDELETEPROGRAMPROC pglDeleteProgram = NULL;
static PFNGLGETATTACHEDSHADERSPROC pglGetAttachedShaders = NULL;
static PFNGLGETATTRIBLOCATIONPROC pglGetAttribLocation = NULL;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = NULL;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC pglDisableVertexAttribArray = NULL;
static PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = NULL;

#define MAX_PROGRAMS 16
#define MAX_TEXTURES 8
#define MAX_VARIABLES 256
#define PREV_TEXTURES 7

enum filter_type
{
   RARCH_GL_NOFORCE,
   RARCH_GL_LINEAR,
   RARCH_GL_NEAREST
};

static bool glsl_enable = false;
static bool glsl_modern = false;
static GLuint gl_program[MAX_PROGRAMS] = {0};
static enum filter_type gl_filter_type[MAX_PROGRAMS] = {RARCH_GL_NOFORCE};
static struct gl_fbo_scale gl_scale[MAX_PROGRAMS];
static unsigned gl_num_programs = 0;
static unsigned active_index = 0;

static GLuint gl_teximage[MAX_TEXTURES];
static unsigned gl_teximage_cnt = 0;
static char gl_teximage_uniforms[MAX_TEXTURES][64];

static state_tracker_t *gl_state_tracker = NULL;
static struct state_tracker_uniform_info gl_tracker_info[MAX_VARIABLES];
static unsigned gl_tracker_info_cnt = 0;
static char gl_tracker_script[PATH_MAX];
static char gl_tracker_script_class[64];
static xmlChar *gl_script_program = NULL;

static GLint gl_attribs[PREV_TEXTURES + 1 + 4 + MAX_PROGRAMS];
static unsigned gl_attrib_index = 0;

struct shader_program
{
   char *vertex;
   char *fragment;
   enum filter_type filter;

   float scale_x;
   float scale_y;
   unsigned abs_x;
   unsigned abs_y;
   enum gl_scale_type type_x;
   enum gl_scale_type type_y;

   bool valid_scale;
};

static const char *stock_vertex_legacy =
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
   "   gl_TexCoord[0] = gl_MultiTexCoord0;\n"
   "   color = gl_Color;\n"
   "}";

static const char *stock_fragment_legacy =
   "uniform sampler2D rubyTexture;\n"
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_FragColor = color * texture2D(rubyTexture, gl_TexCoord[0].xy);\n"
   "}";

static const char *stock_vertex_modern =
   "attribute vec2 rubyTexCoord;\n"
   "attribute vec2 rubyVertexCoord;\n"
   "attribute vec4 rubyColor;\n"
   "uniform mat4 rubyMVPMatrix;\n"
   "varying vec2 tex_coord;\n"
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_Position = rubyMVPMatrix * vec4(rubyVertexCoord, 0.0, 1.0);\n"
   "   tex_coord = rubyTexCoord;\n"
   "   color = rubyColor;\n"
   "}";

static const char *stock_fragment_modern =
   "uniform sampler2D rubyTexture;\n"
   "varying vec2 tex_coord;\n"
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_FragColor = color * texture2D(rubyTexture, tex_coord);\n"
   "}";


static bool get_xml_attrs(struct shader_program *prog, xmlNodePtr ptr)
{
   prog->scale_x = 1.0;
   prog->scale_y = 1.0;
   prog->type_x = prog->type_y = RARCH_SCALE_INPUT;
   prog->valid_scale = false;

   // Check if shader forces a certain texture filtering.
   xmlChar *attr = xmlGetProp(ptr, (const xmlChar*)"filter");
   if (attr)
   {
      if (strcmp((const char*)attr, "nearest") == 0)
      {
         prog->filter = RARCH_GL_NEAREST;
         RARCH_LOG("XML: Shader forces GL_NEAREST.\n");
      }
      else if (strcmp((const char*)attr, "linear") == 0)
      {
         prog->filter = RARCH_GL_LINEAR;
         RARCH_LOG("XML: Shader forces GL_LINEAR.\n");
      }
      else
         RARCH_WARN("XML: Invalid property for filter.\n");

      xmlFree(attr);
   }
   else
      prog->filter = RARCH_GL_NOFORCE;

   // Check for scaling attributes *lots of code <_<*
   xmlChar *attr_scale = xmlGetProp(ptr, (const xmlChar*)"scale");
   xmlChar *attr_scale_x = xmlGetProp(ptr, (const xmlChar*)"scale_x");
   xmlChar *attr_scale_y = xmlGetProp(ptr, (const xmlChar*)"scale_y");
   xmlChar *attr_size = xmlGetProp(ptr, (const xmlChar*)"size");
   xmlChar *attr_size_x = xmlGetProp(ptr, (const xmlChar*)"size_x");
   xmlChar *attr_size_y = xmlGetProp(ptr, (const xmlChar*)"size_y");
   xmlChar *attr_outscale = xmlGetProp(ptr, (const xmlChar*)"outscale");
   xmlChar *attr_outscale_x = xmlGetProp(ptr, (const xmlChar*)"outscale_x");
   xmlChar *attr_outscale_y = xmlGetProp(ptr, (const xmlChar*)"outscale_y");

   unsigned x_attr_cnt = 0, y_attr_cnt = 0;

   if (attr_scale)
   {
      float scale = strtod((const char*)attr_scale, NULL);
      prog->scale_x = scale;
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_x = prog->type_y = RARCH_SCALE_INPUT;
      RARCH_LOG("Got scale attr: %.1f\n", scale);
      x_attr_cnt++;
      y_attr_cnt++;
   }

   if (attr_scale_x)
   {
      float scale = strtod((const char*)attr_scale_x, NULL);
      prog->scale_x = scale;
      prog->valid_scale = true;
      prog->type_x = RARCH_SCALE_INPUT;
      RARCH_LOG("Got scale_x attr: %.1f\n", scale);
      x_attr_cnt++;
   }

   if (attr_scale_y)
   {
      float scale = strtod((const char*)attr_scale_y, NULL);
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_y = RARCH_SCALE_INPUT;
      RARCH_LOG("Got scale_y attr: %.1f\n", scale);
      y_attr_cnt++;
   }
   
   if (attr_size)
   {
      prog->abs_x = prog->abs_y = strtoul((const char*)attr_size, NULL, 0);
      prog->valid_scale = true;
      prog->type_x = prog->type_y = RARCH_SCALE_ABSOLUTE;
      RARCH_LOG("Got size attr: %u\n", prog->abs_x);
      x_attr_cnt++;
      y_attr_cnt++;
   }

   if (attr_size_x)
   {
      prog->abs_x = strtoul((const char*)attr_size_x, NULL, 0);
      prog->valid_scale = true;
      prog->type_x = RARCH_SCALE_ABSOLUTE;
      RARCH_LOG("Got size_x attr: %u\n", prog->abs_x);
      x_attr_cnt++;
   }

   if (attr_size_y)
   {
      prog->abs_y = strtoul((const char*)attr_size_y, NULL, 0);
      prog->valid_scale = true;
      prog->type_y = RARCH_SCALE_ABSOLUTE;
      RARCH_LOG("Got size_y attr: %u\n", prog->abs_y);
      y_attr_cnt++;
   }

   if (attr_outscale)
   {
      float scale = strtod((const char*)attr_outscale, NULL);
      prog->scale_x = scale;
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_x = prog->type_y = RARCH_SCALE_VIEWPORT;
      RARCH_LOG("Got outscale attr: %.1f\n", scale);
      x_attr_cnt++;
      y_attr_cnt++;
   }

   if (attr_outscale_x)
   {
      float scale = strtod((const char*)attr_outscale_x, NULL);
      prog->scale_x = scale;
      prog->valid_scale = true;
      prog->type_x = RARCH_SCALE_VIEWPORT;
      RARCH_LOG("Got outscale_x attr: %.1f\n", scale);
      x_attr_cnt++;
   }

   if (attr_outscale_y)
   {
      float scale = strtod((const char*)attr_outscale_y, NULL);
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_y = RARCH_SCALE_VIEWPORT;
      RARCH_LOG("Got outscale_y attr: %.1f\n", scale);
      y_attr_cnt++;
   }

   if (attr_scale)
      xmlFree(attr_scale);
   if (attr_scale_x)
      xmlFree(attr_scale_x);
   if (attr_scale_y)
      xmlFree(attr_scale_y);
   if (attr_size)
      xmlFree(attr_size);
   if (attr_size_x)
      xmlFree(attr_size_x);
   if (attr_size_y)
      xmlFree(attr_size_y);
   if (attr_outscale)
      xmlFree(attr_outscale);
   if (attr_outscale_x)
      xmlFree(attr_outscale_x);
   if (attr_outscale_y)
      xmlFree(attr_outscale_y);

   if (x_attr_cnt > 1)
      return false;
   if (y_attr_cnt > 1)
      return false;

   return true;
}

static bool get_texture_image(const char *shader_path, xmlNodePtr ptr)
{
   if (gl_teximage_cnt >= MAX_TEXTURES)
   {
      RARCH_WARN("Too many texture images. Ignoring ...\n");
      return true;
   }

   bool linear = true;
   xmlChar *filename = xmlGetProp(ptr, (const xmlChar*)"file");
   xmlChar *filter = xmlGetProp(ptr, (const xmlChar*)"filter");
   xmlChar *id = xmlGetProp(ptr, (const xmlChar*)"id");
   char *last = NULL;
   struct texture_image img;

   if (!id)
   {
      RARCH_ERR("Could not find ID in texture.\n");
      goto error;
   }

   if (!filename)
   {
      RARCH_ERR("Could not find filename in texture.\n");
      goto error;
   }

   if (filter && strcmp((const char*)filter, "nearest") == 0)
      linear = false;

   char tex_path[PATH_MAX];
   strlcpy(tex_path, shader_path, sizeof(tex_path));

   last = strrchr(tex_path, '/');
   if (!last) last = strrchr(tex_path, '\\');
   if (last) last[1] = '\0';

   strlcat(tex_path, (const char*)filename, sizeof(tex_path));

   RARCH_LOG("Loading texture image from: \"%s\" ...\n", tex_path);
   if (!texture_image_load(tex_path, &img))
   {
      RARCH_ERR("Failed to load texture image from: \"%s\"\n", tex_path);
      goto error;
   }

   strlcpy(gl_teximage_uniforms[gl_teximage_cnt], (const char*)id, sizeof(gl_teximage_uniforms[0]));

   glGenTextures(1, &gl_teximage[gl_teximage_cnt]);

   pglActiveTexture(GL_TEXTURE0 + gl_teximage_cnt + 1);
   glBindTexture(GL_TEXTURE_2D, gl_teximage[gl_teximage_cnt]);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);

   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGBA, img.width, img.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, img.pixels);

   pglActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, 0);
   free(img.pixels);

   xmlFree(filename);
   xmlFree(id);
   if (filter)
      xmlFree(filter);

   gl_teximage_cnt++;

   return true;

error:
   if (filename)
      xmlFree(filename);
   if (filter)
      xmlFree(filter);
   if (filter)
      xmlFree(id);
   return false;
}

#ifdef HAVE_PYTHON
static bool get_script(const char *path, xmlNodePtr ptr)
{
   if (*gl_tracker_script || gl_script_program)
   {
      RARCH_ERR("Script already imported.\n");
      return false;
   }

   xmlChar *script_class = xmlGetProp(ptr, (const xmlChar*)"class");
   if (script_class)
   {
      strlcpy(gl_tracker_script_class, (const char*)script_class, sizeof(gl_tracker_script_class));
      xmlFree(script_class);
   }

   xmlChar *language = xmlGetProp(ptr, (const xmlChar*)"language");
   if (!language || strcmp((const char*)language, "python") != 0)
   {
      RARCH_ERR("Script language is not Python.\n");
      if (language)
         xmlFree(language);
      return false;
   }

   if (language)
      xmlFree(language);

   xmlChar *src = xmlGetProp(ptr, (const xmlChar*)"src");
   if (src)
   {
      strlcpy(gl_tracker_script, path, sizeof(gl_tracker_script));
      char *dir_ptr = strrchr(gl_tracker_script, '/');
      if (!dir_ptr) dir_ptr = strrchr(gl_tracker_script, '\\');
      if (dir_ptr) dir_ptr[1] = '\0';
      strlcat(gl_tracker_script, (const char*)src, sizeof(gl_tracker_script));

      xmlFree(src);
   }
   else
   {
      xmlChar *script = xmlNodeGetContent(ptr);
      if (!script)
      {
         RARCH_ERR("No content in script.\n");
         return false;
      }
      gl_script_program = script;
   }

   return true;
}
#endif

static bool get_import_value(xmlNodePtr ptr)
{
   bool ret = true;
   if (gl_tracker_info_cnt >= MAX_VARIABLES)
   {
      RARCH_ERR("Too many import variables ...\n");
      return false;
   }

   xmlChar *id = xmlGetProp(ptr, (const xmlChar*)"id");
   xmlChar *semantic = xmlGetProp(ptr, (const xmlChar*)"semantic");
   xmlChar *wram = xmlGetProp(ptr, (const xmlChar*)"wram");
   xmlChar *input = xmlGetProp(ptr, (const xmlChar*)"input_slot");
   xmlChar *bitmask = xmlGetProp(ptr, (const xmlChar*)"mask");
   xmlChar *bitequal = xmlGetProp(ptr, (const xmlChar*)"equal");

   unsigned memtype;
   enum state_tracker_type tracker_type;
   enum state_ram_type ram_type = RARCH_STATE_NONE;
   uint32_t addr = 0;
   unsigned mask_value = 0;
   unsigned mask_equal = 0;

   if (!semantic || !id)
   {
      RARCH_ERR("No semantic or ID for import value.\n");
      ret = false;
      goto end;
   }

   if (strcmp((const char*)semantic, "capture") == 0)
      tracker_type = RARCH_STATE_CAPTURE;
   else if (strcmp((const char*)semantic, "capture_previous") == 0)
      tracker_type = RARCH_STATE_CAPTURE_PREV;
   else if (strcmp((const char*)semantic, "transition") == 0)
      tracker_type = RARCH_STATE_TRANSITION;
   else if (strcmp((const char*)semantic, "transition_count") == 0)
      tracker_type = RARCH_STATE_TRANSITION_COUNT;
   else if (strcmp((const char*)semantic, "transition_previous") == 0)
      tracker_type = RARCH_STATE_TRANSITION_PREV;
#ifdef HAVE_PYTHON
   else if (strcmp((const char*)semantic, "python") == 0)
      tracker_type = RARCH_STATE_PYTHON;
#endif
   else
   {
      RARCH_ERR("Invalid semantic for import value.\n");
      ret = false;
      goto end;
   }

#ifdef HAVE_PYTHON
   if (tracker_type != RARCH_STATE_PYTHON)
#endif
   {
      if (input) 
      {
         unsigned slot = strtoul((const char*)input, NULL, 0);
         switch (slot)
         {
            case 1:
               ram_type = RARCH_STATE_INPUT_SLOT1;
               break;
            case 2:
               ram_type = RARCH_STATE_INPUT_SLOT2;
               break;

            default:
               RARCH_ERR("Invalid input slot for import.\n");
               ret = false;
               goto end;
         }
      }
      else if (wram)
      {
         addr = strtoul((const char*)wram, NULL, 16);
         ram_type = RARCH_STATE_WRAM;
      }
      else
      {
         RARCH_ERR("No RAM address specificed for import value.\n");
         ret = false;
         goto end;
      }
   }

   switch (ram_type)
   {
      case RARCH_STATE_WRAM:
         memtype = RETRO_MEMORY_SYSTEM_RAM;
         break;

      default:
         memtype = -1u;
   }

   if ((memtype != -1u) && (addr >= pretro_get_memory_size(memtype)))
   {
      RARCH_ERR("Address out of bounds.\n");
      ret = false;
      goto end;
   }

   if (bitmask)
      mask_value = strtoul((const char*)bitmask, NULL, 16);
   if (bitequal)
      mask_equal = strtoul((const char*)bitequal, NULL, 16);

   strlcpy(gl_tracker_info[gl_tracker_info_cnt].id, (const char*)id, sizeof(gl_tracker_info[0].id));
   gl_tracker_info[gl_tracker_info_cnt].addr = addr;
   gl_tracker_info[gl_tracker_info_cnt].type = tracker_type;
   gl_tracker_info[gl_tracker_info_cnt].ram_type = ram_type;
   gl_tracker_info[gl_tracker_info_cnt].mask = mask_value;
   gl_tracker_info[gl_tracker_info_cnt].equal = mask_equal;
   gl_tracker_info_cnt++;

end:
   if (id) xmlFree(id);
   if (semantic) xmlFree(semantic);
   if (wram) xmlFree(wram);
   if (input) xmlFree(input);
   if (bitmask) xmlFree(bitmask);
   if (bitequal) xmlFree(bitequal);
   return ret;
}

static unsigned get_xml_shaders(const char *path, struct shader_program *prog, size_t size)
{
   LIBXML_TEST_VERSION;

   xmlParserCtxtPtr ctx = xmlNewParserCtxt();
   if (!ctx)
   {
      RARCH_ERR("Failed to load libxml2 context.\n");
      return false;
   }

   RARCH_LOG("Loading XML shader: %s\n", path);
   xmlDocPtr doc = xmlCtxtReadFile(ctx, path, NULL, 0);
   xmlNodePtr head = NULL;
   xmlNodePtr cur = NULL;
   unsigned num = 0;

   if (!doc)
   {
      RARCH_ERR("Failed to parse XML file: %s\n", path);
      goto error;
   }

   if (ctx->valid == 0)
   {
      RARCH_ERR("Cannot validate XML shader: %s\n", path);
      goto error;
   }

   head = xmlDocGetRootElement(doc);

   for (cur = head; cur; cur = cur->next)
   {
      if (cur->type != XML_ELEMENT_NODE)
         continue;
      if (strcmp((const char*)cur->name, "shader") != 0)
         continue;

      xmlChar *attr;
      attr = xmlGetProp(cur, (const xmlChar*)"language");
      if (attr && strcmp((const char*)attr, "GLSL") != 0)
      {
         xmlFree(attr);
         continue;
      }

      if (attr)
         xmlFree(attr);

      attr        = xmlGetProp(cur, (const xmlChar*)"style");
      glsl_modern = attr && (strcmp((const char*)attr, "GLES2") == 0);
      if (attr)
         xmlFree(attr);

      if (glsl_modern)
         RARCH_LOG("[GL]: Shader reports a GLES2 style shader.\n");
      break;
   }

   if (!cur) // We couldn't find any GLSL shader :(
      goto error;

   memset(prog, 0, sizeof(struct shader_program) * size);

   // Iterate to check if we find fragment and/or vertex shaders.
   for (cur = cur->children; cur && num < size; cur = cur->next)
   {
      if (cur->type != XML_ELEMENT_NODE)
         continue;

      xmlChar *content = xmlNodeGetContent(cur);
      if (!content)
         continue;

      if (strcmp((const char*)cur->name, "vertex") == 0)
      {
         if (prog[num].vertex)
         {
            RARCH_ERR("Cannot have more than one vertex shader in a program.\n");
            xmlFree(content);
            goto error;
         }

         prog[num].vertex = (char*)content;
      }
      else if (strcmp((const char*)cur->name, "fragment") == 0)
      {
         if (glsl_modern && !prog[num].vertex)
         {
            RARCH_ERR("Modern GLSL was chosen and vertex shader was not provided. This is an error.\n");
            xmlFree(content);
            goto error;
         }

         prog[num].fragment = (char*)content;
         if (!get_xml_attrs(&prog[num], cur))
         {
            RARCH_ERR("XML shader attributes do not comply with specifications.\n");
            goto error;
         }
         num++;
      }
      else if (strcmp((const char*)cur->name, "texture") == 0)
      {
         if (!get_texture_image(path, cur))
         {
            RARCH_ERR("Texture image failed to load.\n");
            goto error;
         }
      }
      else if (strcmp((const char*)cur->name, "import") == 0)
      {
         if (!get_import_value(cur))
         {
            RARCH_ERR("Import value is invalid.\n");
            goto error;
         }
      }
#ifdef HAVE_PYTHON
      else if (strcmp((const char*)cur->name, "script") == 0)
      {
         if (!get_script(path, cur))
         {
            RARCH_ERR("Script is invalid.\n");
            goto error;
         }
      }
#endif
   }

   if (num == 0)
   {
      RARCH_ERR("Couldn't find vertex shader nor fragment shader in XML file.\n");
      goto error;
   }

   xmlFreeDoc(doc);
   xmlFreeParserCtxt(ctx);
   return num;

error:
   RARCH_ERR("Failed to load XML shader ...\n");
   if (doc)
      xmlFreeDoc(doc);
   xmlFreeParserCtxt(ctx);
   return 0;
}

static void print_shader_log(GLuint obj)
{
   GLint info_len = 0;
   GLint max_len;

   pglGetShaderiv(obj, GL_INFO_LOG_LENGTH, &max_len);

   if (max_len == 0)
      return;

   char *info_log = (char*)malloc(max_len);
   if (!info_log)
      return;

   pglGetShaderInfoLog(obj, max_len, &info_len, info_log);

   if (info_len > 0)
      RARCH_LOG("Shader log: %s\n", info_log);

   free(info_log);
}

static void print_linker_log(GLuint obj)
{
   GLint info_len = 0;
   GLint max_len;

   pglGetProgramiv(obj, GL_INFO_LOG_LENGTH, &max_len);

   if (max_len == 0)
      return;

   char *info_log = (char*)malloc(max_len);
   if (!info_log)
      return;

   pglGetProgramInfoLog(obj, max_len, &info_len, info_log);

   if (info_len > 0)
      RARCH_LOG("Linker log: %s\n", info_log);

   free(info_log);
}

static bool compile_shader(GLuint shader, const char *program)
{
   pglShaderSource(shader, 1, &program, 0);
   pglCompileShader(shader);

   GLint status;
   pglGetShaderiv(shader, GL_COMPILE_STATUS, &status);
   print_shader_log(shader);

   return status == GL_TRUE;
}

static bool link_program(GLuint prog)
{
   pglLinkProgram(prog);

   GLint status;
   pglGetProgramiv(prog, GL_LINK_STATUS, &status);
   print_linker_log(prog);

   if (status == GL_TRUE)
   {
      pglUseProgram(prog);
      return true;
   }
   else
      return false;
}

static bool compile_programs(GLuint *gl_prog, struct shader_program *progs, size_t num)
{
   for (unsigned i = 0; i < num; i++)
   {
      gl_prog[i] = pglCreateProgram();

      if (gl_prog[i] == 0)
      {
         RARCH_ERR("Failed to create GL program #%u.\n", i);
         return false;
      }

      if (progs[i].vertex)
      {
         RARCH_LOG("Found GLSL vertex shader.\n");
         GLuint shader = pglCreateShader(GL_VERTEX_SHADER);
         if (!compile_shader(shader, progs[i].vertex))
         {
            RARCH_ERR("Failed to compile vertex shader #%u\n", i);
            return false;
         }

         pglAttachShader(gl_prog[i], shader);
         free(progs[i].vertex);
      }

      if (progs[i].fragment)
      {
         RARCH_LOG("Found GLSL fragment shader.\n");
         GLuint shader = pglCreateShader(GL_FRAGMENT_SHADER);
         if (!compile_shader(shader, progs[i].fragment))
         {
            RARCH_ERR("Failed to compile fragment shader #%u\n", i);
            return false;
         }

         pglAttachShader(gl_prog[i], shader);
         free(progs[i].fragment);
      }

      if (progs[i].vertex || progs[i].fragment)
      {
         RARCH_LOG("Linking GLSL program.\n");
         if (!link_program(gl_prog[i]))
         {
            RARCH_ERR("Failed to link program #%u\n", i);
            return false;
         }

         GLint location = pglGetUniformLocation(gl_prog[i], "rubyTexture");
         pglUniform1i(location, 0);
         pglUseProgram(0);
      }
   }

   return true;
}

static void gl_glsl_reset_attrib(void)
{
   for (unsigned i = 0; i < gl_attrib_index; i++)
      pglDisableVertexAttribArray(gl_attribs[i]);
   gl_attrib_index = 0;
}

#define LOAD_GL_SYM(SYM) if (!pgl##SYM) { \
   gfx_ctx_proc_t sym = gfx_ctx_get_proc_address("gl" #SYM); \
   memcpy(&(pgl##SYM), &sym, sizeof(sym)); \
}

bool gl_glsl_init(const char *path)
{
   // Load shader functions.
   LOAD_GL_SYM(CreateProgram);
   LOAD_GL_SYM(UseProgram);
   LOAD_GL_SYM(CreateShader);
   LOAD_GL_SYM(DeleteShader);
   LOAD_GL_SYM(ShaderSource);
   LOAD_GL_SYM(CompileShader);
   LOAD_GL_SYM(AttachShader);
   LOAD_GL_SYM(DetachShader);
   LOAD_GL_SYM(LinkProgram);
   LOAD_GL_SYM(GetUniformLocation);
   LOAD_GL_SYM(Uniform1i);
   LOAD_GL_SYM(Uniform1f);
   LOAD_GL_SYM(Uniform2fv);
   LOAD_GL_SYM(Uniform4fv);
   LOAD_GL_SYM(UniformMatrix4fv);
   LOAD_GL_SYM(GetShaderiv);
   LOAD_GL_SYM(GetShaderInfoLog);
   LOAD_GL_SYM(GetProgramiv);
   LOAD_GL_SYM(GetProgramInfoLog);
   LOAD_GL_SYM(DeleteProgram);
   LOAD_GL_SYM(GetAttachedShaders);
   LOAD_GL_SYM(GetAttribLocation);
   LOAD_GL_SYM(EnableVertexAttribArray);
   LOAD_GL_SYM(DisableVertexAttribArray);
   LOAD_GL_SYM(VertexAttribPointer);

   RARCH_LOG("Checking GLSL shader support ...\n");
   bool shader_support = pglCreateProgram && pglUseProgram && pglCreateShader
      && pglDeleteShader && pglShaderSource && pglCompileShader && pglAttachShader
      && pglDetachShader && pglLinkProgram && pglGetUniformLocation
      && pglUniform1i && pglUniform1f && pglUniform2fv && pglUniform4fv && pglUniformMatrix4fv
      && pglGetShaderiv && pglGetShaderInfoLog && pglGetProgramiv && pglGetProgramInfoLog 
      && pglDeleteProgram && pglGetAttachedShaders
      && pglGetAttribLocation && pglEnableVertexAttribArray && pglDisableVertexAttribArray
      && pglVertexAttribPointer;

   if (!shader_support)
   {
      RARCH_ERR("GLSL shaders aren't supported by your OpenGL driver.\n");
      return false;
   }

   struct shader_program progs[MAX_PROGRAMS];
   unsigned num_progs = get_xml_shaders(path, progs, MAX_PROGRAMS - 1);

   if (num_progs == 0)
   {
      RARCH_ERR("Couldn't find any valid shaders in XML file.\n");
      return false;
   }

   struct shader_program stock_prog = {0};
   stock_prog.vertex   = strdup(glsl_modern ? stock_vertex_modern   : stock_vertex_legacy);
   stock_prog.fragment = strdup(glsl_modern ? stock_fragment_modern : stock_fragment_legacy);

   if (!compile_programs(&gl_program[0], &stock_prog, 1))
   {
      RARCH_ERR("GLSL stock programs failed to compile.\n");
      return false;
   }

   for (unsigned i = 0; i < num_progs; i++)
   {
      gl_filter_type[i + 1]   = progs[i].filter;
      gl_scale[i + 1].type_x  = progs[i].type_x;
      gl_scale[i + 1].type_y  = progs[i].type_y;
      gl_scale[i + 1].scale_x = progs[i].scale_x;
      gl_scale[i + 1].scale_y = progs[i].scale_y;
      gl_scale[i + 1].abs_x   = progs[i].abs_x;
      gl_scale[i + 1].abs_y   = progs[i].abs_y;
      gl_scale[i + 1].valid   = progs[i].valid_scale;
   }

   if (!compile_programs(&gl_program[1], progs, num_progs))
      return false;

   // RetroArch custom two-pass with two different files.
   if (num_progs == 1 && *g_settings.video.second_pass_shader && g_settings.video.render_to_texture)
   {
      unsigned secondary_progs = get_xml_shaders(g_settings.video.second_pass_shader, progs, 1);
      if (secondary_progs == 1)
      {
         compile_programs(&gl_program[2], progs, 1);
         num_progs++;
      }
      else
      {
         RARCH_ERR("Did not find valid shader in secondary shader file.\n");
         return false;
      }
   }

   //if (!gl_check_error())
   //   RARCH_WARN("Detected GL error.\n");

   if (gl_tracker_info_cnt > 0)
   {
      struct state_tracker_info info = {0};
      info.wram      = (uint8_t*)pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
      info.info      = gl_tracker_info;
      info.info_elem = gl_tracker_info_cnt;

#ifdef HAVE_PYTHON
      if (*gl_tracker_script)
         info.script = gl_tracker_script;
      else if (gl_script_program)
         info.script = (const char*)gl_script_program;
      else
         info.script = NULL;

      info.script_class   = *gl_tracker_script_class ? gl_tracker_script_class : NULL;
      info.script_is_file = *gl_tracker_script;
#endif

      gl_state_tracker = state_tracker_init(&info);
      if (!gl_state_tracker)
         RARCH_WARN("Failed to init state tracker.\n");
   }
   
   glsl_enable = true;
   gl_num_programs = num_progs;
   gl_program[gl_num_programs + 1] = gl_program[0];

   gl_glsl_reset_attrib();

   return true;
}

void gl_glsl_deinit(void)
{
   if (glsl_enable)
   {
      pglUseProgram(0);
      for (unsigned i = 0; i <= gl_num_programs; i++)
      {
         if (gl_program[i] == 0)
            continue;

         GLsizei count;
         GLuint shaders[2];

         pglGetAttachedShaders(gl_program[i], 2, &count, shaders);
         for (GLsizei j = 0; j < count; j++)
         {
            pglDetachShader(gl_program[i], shaders[j]);
            pglDeleteShader(shaders[j]);
         }

         pglDeleteProgram(gl_program[i]);
      }

      glDeleteTextures(gl_teximage_cnt, gl_teximage);
      gl_teximage_cnt = 0;
      memset(gl_teximage_uniforms, 0, sizeof(gl_teximage_uniforms));
   }

   memset(gl_program, 0, sizeof(gl_program));
   glsl_enable  = false;
   active_index = 0;

   gl_tracker_info_cnt = 0;
   memset(gl_tracker_info, 0, sizeof(gl_tracker_info));
   memset(gl_tracker_script, 0, sizeof(gl_tracker_script));
   memset(gl_tracker_script_class, 0, sizeof(gl_tracker_script_class));

   if (gl_script_program)
   {
      xmlFree(gl_script_program);
      gl_script_program = NULL;
   }

   if (gl_state_tracker)
   {
      state_tracker_free(gl_state_tracker);
      gl_state_tracker = NULL;
   }

   gl_glsl_reset_attrib();
}

void gl_glsl_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info, 
      const struct gl_tex_info *prev_info, 
      const struct gl_tex_info *fbo_info, unsigned fbo_info_cnt)
{
   // We enforce a certain layout for our various texture types in the texunits.
   // - Regular SNES frame (rubyTexture) (always bound).
   // - LUT textures (always bound).
   // - Original texture (always bound if meaningful).
   // - FBO textures (always bound if available).
   // - Previous textures.

   if (!glsl_enable || (gl_program[active_index] == 0))
      return;

   GLint location;

   float inputSize[2] = {(float)width, (float)height};
   location = pglGetUniformLocation(gl_program[active_index], "rubyInputSize");
   pglUniform2fv(location, 1, inputSize);

   float outputSize[2] = {(float)out_width, (float)out_height};
   location = pglGetUniformLocation(gl_program[active_index], "rubyOutputSize");
   pglUniform2fv(location, 1, outputSize);

   float textureSize[2] = {(float)tex_width, (float)tex_height};
   location = pglGetUniformLocation(gl_program[active_index], "rubyTextureSize");
   pglUniform2fv(location, 1, textureSize);

   location = pglGetUniformLocation(gl_program[active_index], "rubyFrameCount");
   pglUniform1i(location, frame_count);

   location = pglGetUniformLocation(gl_program[active_index], "rubyFrameDirection");
   pglUniform1i(location, g_extern.frame_is_reverse ? -1 : 1);

   for (unsigned i = 0; i < gl_teximage_cnt; i++)
   {
      location = pglGetUniformLocation(gl_program[active_index], gl_teximage_uniforms[i]);
      pglUniform1i(location, i + 1);
   }

   unsigned texunit = gl_teximage_cnt + 1;

   // Set original texture unless we're in first pass (pointless).
   if (active_index > 1)
   {
      // Bind original texture.
      pglActiveTexture(GL_TEXTURE0 + texunit);

      location = pglGetUniformLocation(gl_program[active_index], "rubyOrigTexture");
      pglUniform1i(location, texunit++);
      glBindTexture(GL_TEXTURE_2D, info->tex);

      location = pglGetUniformLocation(gl_program[active_index], "rubyOrigTextureSize");
      pglUniform2fv(location, 1, info->tex_size);
      location = pglGetUniformLocation(gl_program[active_index], "rubyOrigInputSize");
      pglUniform2fv(location, 1, info->input_size);

      // Pass texture coordinates.
      location = pglGetAttribLocation(gl_program[active_index], "rubyOrigTexCoord");
      if (location >= 0)
      {
         pglEnableVertexAttribArray(location);
         pglVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, 0, info->coord);
         gl_attribs[gl_attrib_index++] = location;
      }

      // Bind new texture in the chain.
      if (fbo_info_cnt > 0)
      {
         pglActiveTexture(GL_TEXTURE0 + texunit + fbo_info_cnt - 1);
         glBindTexture(GL_TEXTURE_2D, fbo_info[fbo_info_cnt - 1].tex);
      }

      // Bind FBO textures.
      for (unsigned i = 0; i < fbo_info_cnt; i++)
      {
         char attrib_buf[64];

         snprintf(attrib_buf, sizeof(attrib_buf), "rubyPass%uTexture", i + 1);
         location = pglGetUniformLocation(gl_program[active_index], attrib_buf);
         pglUniform1i(location, texunit++);

         snprintf(attrib_buf, sizeof(attrib_buf), "rubyPass%uTextureSize", i + 1);
         location = pglGetUniformLocation(gl_program[active_index], attrib_buf);
         pglUniform2fv(location, 1, fbo_info[i].tex_size);

         snprintf(attrib_buf, sizeof(attrib_buf), "rubyPass%uInputSize", i + 1);
         location = pglGetUniformLocation(gl_program[active_index], attrib_buf);
         pglUniform2fv(location, 1, fbo_info[i].input_size);

         snprintf(attrib_buf, sizeof(attrib_buf), "rubyPass%uTexCoord", i + 1);
         location = pglGetAttribLocation(gl_program[active_index], attrib_buf);
         if (location >= 0)
         {
            pglEnableVertexAttribArray(location);
            pglVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, 0, fbo_info[i].coord);
            gl_attribs[gl_attrib_index++] = location;
         }
      }
   }
   else
   {
      // First pass, so unbind everything to avoid collitions.
      // Unbind ORIG.
      pglActiveTexture(GL_TEXTURE0 + texunit);
      glBindTexture(GL_TEXTURE_2D, 0);

      GLuint base_tex = texunit + 1;
      // Unbind any lurking FBO passes.
      // Rendering to a texture that is bound to a texture unit
      // sounds very shaky ... ;)
      for (unsigned i = 0; i < gl_num_programs; i++)
      {
         pglActiveTexture(GL_TEXTURE0 + base_tex + i);
         glBindTexture(GL_TEXTURE_2D, 0);
      }
   }

   // Set previous textures. Only bind if they're actually used.
   for (unsigned i = 0; i < PREV_TEXTURES; i++)
   {
      char attr_buf_tex[64];
      char attr_buf_tex_size[64];
      char attr_buf_input_size[64];
      char attr_buf_coord[64];
      static const char *prev_names[PREV_TEXTURES] = {
         "Prev",
         "Prev1",
         "Prev2",
         "Prev3",
         "Prev4",
         "Prev5",
         "Prev6",
      };

      snprintf(attr_buf_tex,        sizeof(attr_buf_tex),        "ruby%sTexture",     prev_names[i]);
      snprintf(attr_buf_tex_size,   sizeof(attr_buf_tex_size),   "ruby%sTextureSize", prev_names[i]);
      snprintf(attr_buf_input_size, sizeof(attr_buf_input_size), "ruby%sInputSize",   prev_names[i]);
      snprintf(attr_buf_coord,      sizeof(attr_buf_coord),      "ruby%sTexCoord",    prev_names[i]);

      location = pglGetUniformLocation(gl_program[active_index], attr_buf_tex);
      if (location >= 0)
      {
         pglActiveTexture(GL_TEXTURE0 + texunit);
         glBindTexture(GL_TEXTURE_2D, prev_info[i].tex);
         pglUniform1i(location, texunit++);
      }

      location = pglGetUniformLocation(gl_program[active_index], attr_buf_tex_size);
      pglUniform2fv(location, 1, prev_info[i].tex_size);
      location = pglGetUniformLocation(gl_program[active_index], attr_buf_input_size);
      pglUniform2fv(location, 1, prev_info[i].input_size);

      // Pass texture coordinates.
      location = pglGetAttribLocation(gl_program[active_index], attr_buf_coord);
      if (location >= 0)
      {
         pglEnableVertexAttribArray(location);
         pglVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, 0, prev_info[i].coord);
         gl_attribs[gl_attrib_index++] = location;
      }
   }

   pglActiveTexture(GL_TEXTURE0);

   if (gl_state_tracker)
   {
      static struct state_tracker_uniform info[MAX_VARIABLES];
      static unsigned cnt = 0;

      if (active_index == 1)
         cnt = state_get_uniform(gl_state_tracker, info, MAX_VARIABLES, frame_count);

      for (unsigned i = 0; i < cnt; i++)
      {
         location = pglGetUniformLocation(gl_program[active_index], info[i].id);
         pglUniform1f(location, info[i].value);
      }
   }
}

bool gl_glsl_set_mvp(const math_matrix *mat)
{
   if (!glsl_enable || !glsl_modern)
      return false;

   int loc = pglGetUniformLocation(gl_program[active_index], "rubyMVPMatrix");
   if (loc >= 0)
      pglUniformMatrix4fv(loc, 1, GL_FALSE, mat->data);
   return true;
}

bool gl_glsl_set_coords(const struct gl_coords *coords)
{
   if (!glsl_enable || !glsl_modern)
      return false;

   if (coords->tex_coord)
   {
      int loc = pglGetAttribLocation(gl_program[active_index], "rubyTexCoord");
      if (loc >= 0)
      {
         pglEnableVertexAttribArray(loc);
         pglVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, coords->tex_coord);
         gl_attribs[gl_attrib_index++] = loc;
      }
   }

   if (coords->vertex)
   {
      int loc = pglGetAttribLocation(gl_program[active_index], "rubyVertexCoord");
      if (loc >= 0)
      {
         pglEnableVertexAttribArray(loc);
         pglVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, coords->vertex);
         gl_attribs[gl_attrib_index++] = loc;
      }
   }

   if (coords->color)
   {
      int loc = pglGetAttribLocation(gl_program[active_index], "rubyColor");
      if (loc >= 0)
      {
         pglEnableVertexAttribArray(loc);
         pglVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, 0, coords->color);
         gl_attribs[gl_attrib_index++] = loc;
      }
   }

   if (coords->lut_tex_coord)
   {
      int loc = pglGetAttribLocation(gl_program[active_index], "rubyLUTTexCoord");
      if (loc >= 0)
      {
         pglEnableVertexAttribArray(loc);
         pglVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, coords->lut_tex_coord);
         gl_attribs[gl_attrib_index++] = loc;
      }
   }

   return true;
}

void gl_glsl_use(unsigned index)
{
   if (glsl_enable)
   {
      gl_glsl_reset_attrib();

      active_index = index;
      pglUseProgram(gl_program[index]);
   }
}

unsigned gl_glsl_num(void)
{
   return gl_num_programs;
}

bool gl_glsl_filter_type(unsigned index, bool *smooth)
{
   if (!glsl_enable)
      return false;

   switch (gl_filter_type[index])
   {
      case RARCH_GL_NOFORCE:
         return false;

      case RARCH_GL_NEAREST:
         *smooth = false;
         return true;

      case RARCH_GL_LINEAR:
         *smooth = true;
         return true;

      default:
         return false;
   }
}

void gl_glsl_shader_scale(unsigned index, struct gl_fbo_scale *scale)
{
   if (glsl_enable)
      *scale = gl_scale[index];
   else
      scale->valid = false;
}
