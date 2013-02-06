/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include "../file.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(IOS)
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#elif defined(__APPLE__) // Because they like to be "oh, so, special".
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

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/tree.h>
#else
#define RXML_LIBXML2_COMPAT
#include "../compat/rxml/rxml.h"
#endif

#include "gl_common.h"
#include "image.h"

#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGL_MODERN) || defined(__APPLE__)
#define pglCreateProgram glCreateProgram
#define pglUseProgram glUseProgram
#define pglCreateShader glCreateShader
#define pglDeleteShader glDeleteShader
#define pglShaderSource glShaderSource
#define pglCompileShader glCompileShader
#define pglAttachShader glAttachShader
#define pglDetachShader glDetachShader
#define pglLinkProgram glLinkProgram
#define pglGetUniformLocation glGetUniformLocation
#define pglUniform1i glUniform1i
#define pglUniform1f glUniform1f
#define pglUniform2fv glUniform2fv
#define pglUniform4fv glUniform4fv
#define pglUniformMatrix4fv glUniformMatrix4fv
#define pglGetShaderiv glGetShaderiv
#define pglGetShaderInfoLog glGetShaderInfoLog
#define pglGetProgramiv glGetProgramiv
#define pglGetProgramInfoLog glGetProgramInfoLog
#define pglDeleteProgram glDeleteProgram
#define pglGetAttachedShaders glGetAttachedShaders
#define pglGetAttribLocation glGetAttribLocation
#define pglEnableVertexAttribArray glEnableVertexAttribArray
#define pglDisableVertexAttribArray glDisableVertexAttribArray
#define pglVertexAttribPointer glVertexAttribPointer
#else
static PFNGLCREATEPROGRAMPROC pglCreateProgram;
static PFNGLUSEPROGRAMPROC pglUseProgram;
static PFNGLCREATESHADERPROC pglCreateShader;
static PFNGLDELETESHADERPROC pglDeleteShader;
static PFNGLSHADERSOURCEPROC pglShaderSource;
static PFNGLCOMPILESHADERPROC pglCompileShader;
static PFNGLATTACHSHADERPROC pglAttachShader;
static PFNGLDETACHSHADERPROC pglDetachShader;
static PFNGLLINKPROGRAMPROC pglLinkProgram;
static PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation;
static PFNGLUNIFORM1IPROC pglUniform1i;
static PFNGLUNIFORM1FPROC pglUniform1f;
static PFNGLUNIFORM2FVPROC pglUniform2fv;
static PFNGLUNIFORM4FVPROC pglUniform4fv;
static PFNGLUNIFORMMATRIX4FVPROC pglUniformMatrix4fv;
static PFNGLGETSHADERIVPROC pglGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog;
static PFNGLGETPROGRAMIVPROC pglGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog;
static PFNGLDELETEPROGRAMPROC pglDeleteProgram;
static PFNGLGETATTACHEDSHADERSPROC pglGetAttachedShaders;
static PFNGLGETATTRIBLOCATIONPROC pglGetAttribLocation;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC pglDisableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer;
#endif

#ifdef HAVE_OPENGLES2
#define BORDER_FUNC GL_CLAMP_TO_EDGE
#else
#define BORDER_FUNC GL_CLAMP_TO_BORDER
#endif

#define MAX_VARIABLES 256
#define MAX_TEXTURES 8
#define PREV_TEXTURES 7

enum filter_type
{
   RARCH_GL_NOFORCE,
   RARCH_GL_LINEAR,
   RARCH_GL_NEAREST
};

static bool glsl_enable;
static bool glsl_modern;
static GLuint gl_program[RARCH_GLSL_MAX_SHADERS];
static enum filter_type gl_filter_type[RARCH_GLSL_MAX_SHADERS];
static struct gl_fbo_scale gl_scale[RARCH_GLSL_MAX_SHADERS];
static unsigned gl_num_programs;
static unsigned active_index;

static GLuint gl_teximage[MAX_TEXTURES];
static unsigned gl_teximage_cnt;
static char gl_teximage_uniforms[MAX_TEXTURES][64];

static state_tracker_t *gl_state_tracker;
static struct state_tracker_uniform_info gl_tracker_info[MAX_VARIABLES];
static unsigned gl_tracker_info_cnt;
static char gl_tracker_script_class[64];

static char *gl_script_program;

static GLint gl_attribs[PREV_TEXTURES + 1 + 4 + RARCH_GLSL_MAX_SHADERS];
static unsigned gl_attrib_index;

static gfx_ctx_proc_t (*glsl_get_proc_address)(const char*);

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

struct shader_uniforms_frame
{
   int texture;
   int input_size;
   int texture_size;
   int tex_coord;
};

struct shader_uniforms
{
   int mvp;
   int tex_coord;
   int vertex_coord;
   int color;
   int lut_tex_coord;

   int input_size;
   int output_size;
   int texture_size;

   int frame_count;
   int frame_direction;

   int lut_texture[MAX_TEXTURES];
   
   struct shader_uniforms_frame orig;
   struct shader_uniforms_frame pass[RARCH_GLSL_MAX_SHADERS];
   struct shader_uniforms_frame prev[PREV_TEXTURES];
};

static struct shader_uniforms gl_uniforms[RARCH_GLSL_MAX_SHADERS];

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
   "#ifdef GL_ES\n"
   "precision mediump float;\n"
   "#endif\n"
   "uniform sampler2D rubyTexture;\n"
   "varying vec2 tex_coord;\n"
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_FragColor = color * texture2D(rubyTexture, tex_coord);\n"
   "}";

static bool xml_get_prop(char *buf, size_t size, xmlNodePtr node, const char *prop)
{
   if (!size)
      return false;

   xmlChar *p = xmlGetProp(node, (const xmlChar*)prop);
   if (p)
   {
      bool ret = strlcpy(buf, (const char*)p, size) < size;
      xmlFree(p);
      return ret;
   }
   else
   {
      *buf = '\0';
      return false;
   }
}

static char *xml_get_content(xmlNodePtr node)
{
   xmlChar *content = xmlNodeGetContent(node);
   if (!content)
      return NULL;

   char *ret = strdup((const char*)content);
   xmlFree(content);
   return ret;
}

static char *xml_replace_if_file(char *content, const char *path, xmlNodePtr node, const char *src_prop)
{
   char prop[64];
   if (!xml_get_prop(prop, sizeof(prop), node, src_prop))
      return content;

   free(content);
   content = NULL;

   char shader_path[PATH_MAX];
   fill_pathname_resolve_relative(shader_path, path, (const char*)prop, sizeof(shader_path));

   RARCH_LOG("Loading external source from \"%s\".\n", shader_path);
   if (read_file(shader_path, (void**)&content) >= 0)
      return content;
   else
      return NULL;
}


static bool get_xml_attrs(struct shader_program *prog, xmlNodePtr ptr)
{
   prog->scale_x = 1.0;
   prog->scale_y = 1.0;
   prog->type_x = prog->type_y = RARCH_SCALE_INPUT;
   prog->valid_scale = false;

   // Check if shader forces a certain texture filtering.
   char attr[64];
   if (xml_get_prop(attr, sizeof(attr), ptr, "filter"))
   {
      if (strcmp(attr, "nearest") == 0)
      {
         prog->filter = RARCH_GL_NEAREST;
         RARCH_LOG("XML: Shader forces GL_NEAREST.\n");
      }
      else if (strcmp(attr, "linear") == 0)
      {
         prog->filter = RARCH_GL_LINEAR;
         RARCH_LOG("XML: Shader forces GL_LINEAR.\n");
      }
      else
         RARCH_WARN("XML: Invalid property for filter.\n");
   }
   else
      prog->filter = RARCH_GL_NOFORCE;

   // Check for scaling attributes *lots of code <_<*
   char attr_scale[64], attr_scale_x[64], attr_scale_y[64];
   char attr_size[64], attr_size_x[64], attr_size_y[64];
   char attr_outscale[64], attr_outscale_x[64], attr_outscale_y[64];

   xml_get_prop(attr_scale, sizeof(attr_scale), ptr, "scale");
   xml_get_prop(attr_scale_x, sizeof(attr_scale_x), ptr, "scale_x");
   xml_get_prop(attr_scale_y, sizeof(attr_scale_y), ptr, "scale_y");
   xml_get_prop(attr_size, sizeof(attr_size), ptr, "size");
   xml_get_prop(attr_size_x, sizeof(attr_size_x), ptr, "size_x");
   xml_get_prop(attr_size_y, sizeof(attr_size_y), ptr, "size_y");
   xml_get_prop(attr_outscale, sizeof(attr_outscale), ptr, "outscale");
   xml_get_prop(attr_outscale_x, sizeof(attr_outscale_x), ptr, "outscale_x");
   xml_get_prop(attr_outscale_y, sizeof(attr_outscale_y), ptr, "outscale_y");

   unsigned x_attr_cnt = 0, y_attr_cnt = 0;

   if (*attr_scale)
   {
      float scale = strtod(attr_scale, NULL);
      prog->scale_x = scale;
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_x = prog->type_y = RARCH_SCALE_INPUT;
      RARCH_LOG("Got scale attr: %.1f\n", scale);
      x_attr_cnt++;
      y_attr_cnt++;
   }

   if (*attr_scale_x)
   {
      float scale = strtod(attr_scale_x, NULL);
      prog->scale_x = scale;
      prog->valid_scale = true;
      prog->type_x = RARCH_SCALE_INPUT;
      RARCH_LOG("Got scale_x attr: %.1f\n", scale);
      x_attr_cnt++;
   }

   if (*attr_scale_y)
   {
      float scale = strtod(attr_scale_y, NULL);
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_y = RARCH_SCALE_INPUT;
      RARCH_LOG("Got scale_y attr: %.1f\n", scale);
      y_attr_cnt++;
   }
   
   if (*attr_size)
   {
      prog->abs_x = prog->abs_y = strtoul(attr_size, NULL, 0);
      prog->valid_scale = true;
      prog->type_x = prog->type_y = RARCH_SCALE_ABSOLUTE;
      RARCH_LOG("Got size attr: %u\n", prog->abs_x);
      x_attr_cnt++;
      y_attr_cnt++;
   }

   if (*attr_size_x)
   {
      prog->abs_x = strtoul(attr_size_x, NULL, 0);
      prog->valid_scale = true;
      prog->type_x = RARCH_SCALE_ABSOLUTE;
      RARCH_LOG("Got size_x attr: %u\n", prog->abs_x);
      x_attr_cnt++;
   }

   if (*attr_size_y)
   {
      prog->abs_y = strtoul(attr_size_y, NULL, 0);
      prog->valid_scale = true;
      prog->type_y = RARCH_SCALE_ABSOLUTE;
      RARCH_LOG("Got size_y attr: %u\n", prog->abs_y);
      y_attr_cnt++;
   }

   if (*attr_outscale)
   {
      float scale = strtod(attr_outscale, NULL);
      prog->scale_x = scale;
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_x = prog->type_y = RARCH_SCALE_VIEWPORT;
      RARCH_LOG("Got outscale attr: %.1f\n", scale);
      x_attr_cnt++;
      y_attr_cnt++;
   }

   if (*attr_outscale_x)
   {
      float scale = strtod(attr_outscale_x, NULL);
      prog->scale_x = scale;
      prog->valid_scale = true;
      prog->type_x = RARCH_SCALE_VIEWPORT;
      RARCH_LOG("Got outscale_x attr: %.1f\n", scale);
      x_attr_cnt++;
   }

   if (*attr_outscale_y)
   {
      float scale = strtod(attr_outscale_y, NULL);
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_y = RARCH_SCALE_VIEWPORT;
      RARCH_LOG("Got outscale_y attr: %.1f\n", scale);
      y_attr_cnt++;
   }

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
   char filename[PATH_MAX];
   char filter[64];
   char id[64];
   xml_get_prop(filename, sizeof(filename), ptr, "file");
   xml_get_prop(filter, sizeof(filter), ptr, "filter");
   xml_get_prop(id, sizeof(id), ptr, "id");
   struct texture_image img;

   if (!*id)
   {
      RARCH_ERR("Could not find ID in texture.\n");
      return false;
   }

   if (!*filename)
   {
      RARCH_ERR("Could not find filename in texture.\n");
      return false;
   }

   if (strcmp(filter, "nearest") == 0)
      linear = false;

   char tex_path[PATH_MAX];
   fill_pathname_resolve_relative(tex_path, shader_path, (const char*)filename, sizeof(tex_path));

   RARCH_LOG("Loading texture image from: \"%s\" ...\n", tex_path);

   if (!texture_image_load(tex_path, &img))
   {
      RARCH_ERR("Failed to load texture image from: \"%s\"\n", tex_path);
      return false;
   }

   strlcpy(gl_teximage_uniforms[gl_teximage_cnt], (const char*)id, sizeof(gl_teximage_uniforms[0]));

   glGenTextures(1, &gl_teximage[gl_teximage_cnt]);

   pglActiveTexture(GL_TEXTURE0 + gl_teximage_cnt + 1);
   glBindTexture(GL_TEXTURE_2D, gl_teximage[gl_teximage_cnt]);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, BORDER_FUNC);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, BORDER_FUNC);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);

   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glTexImage2D(GL_TEXTURE_2D,
         0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
         img.width, img.height, 0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         RARCH_GL_FORMAT32, img.pixels);

   pglActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, 0);
   free(img.pixels);

   gl_teximage_cnt++;

   return true;
}

#ifdef HAVE_PYTHON
static bool get_script(const char *path, xmlNodePtr ptr)
{
   if (gl_script_program)
   {
      RARCH_ERR("Script already imported.\n");
      return false;
   }

   char script_class[64];
   xml_get_prop(script_class, sizeof(script_class), ptr, "class");
   if (*script_class)
      strlcpy(gl_tracker_script_class, script_class, sizeof(gl_tracker_script_class));

   char language[64];
   xml_get_prop(language, sizeof(language), ptr, "language");
   if (strcmp(language, "python") != 0)
   {
      RARCH_ERR("Script language is not Python.\n");
      return false;
   }

   char *script = xml_get_content(ptr);
   if (!script)
      return false;

   gl_script_program = xml_replace_if_file(script, path, ptr, "src"); 
   if (!gl_script_program)
   {
      RARCH_ERR("Cannot find Python script.\n");
      return false;
   }

   return true;
}
#endif

static bool get_import_value(xmlNodePtr ptr)
{
   if (gl_tracker_info_cnt >= MAX_VARIABLES)
   {
      RARCH_ERR("Too many import variables ...\n");
      return false;
   }

   char id[64], semantic[64], wram[64], input[64], bitmask[64], bitequal[64];
   xml_get_prop(id, sizeof(id), ptr, "id");
   xml_get_prop(semantic, sizeof(semantic), ptr, "semantic");
   xml_get_prop(wram, sizeof(wram), ptr, "wram");
   xml_get_prop(input, sizeof(input), ptr, "input_slot");
   xml_get_prop(bitmask, sizeof(bitmask), ptr, "mask");
   xml_get_prop(bitequal, sizeof(bitequal), ptr, "equal");

   unsigned memtype;
   enum state_tracker_type tracker_type;
   enum state_ram_type ram_type = RARCH_STATE_NONE;
   uint32_t addr = 0;
   unsigned mask_value = 0;
   unsigned mask_equal = 0;

   if (!*semantic || !*id)
   {
      RARCH_ERR("No semantic or ID for import value.\n");
      return false;
   }

   if (strcmp(semantic, "capture") == 0)
      tracker_type = RARCH_STATE_CAPTURE;
   else if (strcmp(semantic, "capture_previous") == 0)
      tracker_type = RARCH_STATE_CAPTURE_PREV;
   else if (strcmp(semantic, "transition") == 0)
      tracker_type = RARCH_STATE_TRANSITION;
   else if (strcmp(semantic, "transition_count") == 0)
      tracker_type = RARCH_STATE_TRANSITION_COUNT;
   else if (strcmp(semantic, "transition_previous") == 0)
      tracker_type = RARCH_STATE_TRANSITION_PREV;
#ifdef HAVE_PYTHON
   else if (strcmp(semantic, "python") == 0)
      tracker_type = RARCH_STATE_PYTHON;
#endif
   else
   {
      RARCH_ERR("Invalid semantic for import value.\n");
      return false;
   }

#ifdef HAVE_PYTHON
   if (tracker_type != RARCH_STATE_PYTHON)
#endif
   {
      if (*input) 
      {
         unsigned slot = strtoul(input, NULL, 0);
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
               return false;
         }
      }
      else if (*wram)
      {
         addr = strtoul(wram, NULL, 16);
         ram_type = RARCH_STATE_WRAM;
      }
      else
      {
         RARCH_ERR("No RAM address specificed for import value.\n");
         return false;
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
      return false;
   }

   if (*bitmask)
      mask_value = strtoul(bitmask, NULL, 16);
   if (*bitequal)
      mask_equal = strtoul(bitequal, NULL, 16);

   strlcpy(gl_tracker_info[gl_tracker_info_cnt].id, id, sizeof(gl_tracker_info[0].id));
   gl_tracker_info[gl_tracker_info_cnt].addr = addr;
   gl_tracker_info[gl_tracker_info_cnt].type = tracker_type;
   gl_tracker_info[gl_tracker_info_cnt].ram_type = ram_type;
   gl_tracker_info[gl_tracker_info_cnt].mask = mask_value;
   gl_tracker_info[gl_tracker_info_cnt].equal = mask_equal;
   gl_tracker_info_cnt++;

   return true;
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

#ifdef HAVE_LIBXML2
   if (ctx->valid == 0)
   {
      RARCH_ERR("Cannot validate XML shader: %s\n", path);
      goto error;
   }
#endif

   head = xmlDocGetRootElement(doc);

   for (cur = head; cur; cur = cur->next)
   {
      if (cur->type != XML_ELEMENT_NODE)
         continue;
      if (strcmp((const char*)cur->name, "shader") != 0)
         continue;

      char attr[64];
      xml_get_prop(attr, sizeof(attr), cur, "language");
      if (strcmp(attr, "GLSL") != 0)
         continue;

      xml_get_prop(attr, sizeof(attr), cur, "style");
      glsl_modern = strcmp(attr, "GLES2") == 0;

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

      char *content = xml_get_content(cur);
      if (!content)
         continue;

      if (strcmp((const char*)cur->name, "vertex") == 0)
      {
         if (prog[num].vertex)
         {
            RARCH_ERR("Cannot have more than one vertex shader in a program.\n");
            free(content);
            goto error;
         }

         content = xml_replace_if_file(content, path, cur, "src");
         if (!content)
         {
            RARCH_ERR("Shader source file was provided, but failed to read.\n");
            goto error;
         }

         prog[num].vertex = content;
      }
      else if (strcmp((const char*)cur->name, "fragment") == 0)
      {
         if (glsl_modern && !prog[num].vertex)
         {
            RARCH_ERR("Modern GLSL was chosen and vertex shader was not provided. This is an error.\n");
            free(content);
            goto error;
         }

         content = xml_replace_if_file(content, path, cur, "src");
         if (!content)
         {
            RARCH_ERR("Shader source file was provided, but failed to read.\n");
            goto error;
         }

         prog[num].fragment = content;
         if (!get_xml_attrs(&prog[num], cur))
         {
            RARCH_ERR("XML shader attributes do not comply with specifications.\n");
            goto error;
         }
         num++;
      }
      else if (strcmp((const char*)cur->name, "texture") == 0)
      {
         free(content);
         if (!get_texture_image(path, cur))
         {
            RARCH_ERR("Texture image failed to load.\n");
            goto error;
         }
      }
      else if (strcmp((const char*)cur->name, "import") == 0)
      {
         free(content);
         if (!get_import_value(cur))
         {
            RARCH_ERR("Import value is invalid.\n");
            goto error;
         }
      }
#ifdef HAVE_PYTHON
      else if (strcmp((const char*)cur->name, "script") == 0)
      {
         free(content);
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
   bool ret = true;

   for (unsigned i = 0; i < num; i++)
   {
      gl_prog[i] = pglCreateProgram();

      if (gl_prog[i] == 0)
      {
         RARCH_ERR("Failed to create GL program #%u.\n", i);
         ret = false;
         goto end;
      }

      if (progs[i].vertex)
      {
         RARCH_LOG("Found GLSL vertex shader.\n");
         GLuint shader = pglCreateShader(GL_VERTEX_SHADER);
         if (!compile_shader(shader, progs[i].vertex))
         {
            RARCH_ERR("Failed to compile vertex shader #%u\n", i);
            ret = false;
            goto end;
         }

         pglAttachShader(gl_prog[i], shader);
      }

      if (progs[i].fragment)
      {
         RARCH_LOG("Found GLSL fragment shader.\n");
         GLuint shader = pglCreateShader(GL_FRAGMENT_SHADER);
         if (!compile_shader(shader, progs[i].fragment))
         {
            RARCH_ERR("Failed to compile fragment shader #%u\n", i);
            ret = false;
            goto end;
         }

         pglAttachShader(gl_prog[i], shader);
      }

      if (progs[i].vertex || progs[i].fragment)
      {
         RARCH_LOG("Linking GLSL program.\n");
         if (!link_program(gl_prog[i]))
         {
            RARCH_ERR("Failed to link program #%u\n", i);
            ret = false;
            goto end;
         }

         GLint location = pglGetUniformLocation(gl_prog[i], "rubyTexture");
         pglUniform1i(location, 0);
         pglUseProgram(0);
      }
   }

end:
   for (unsigned i = 0; i < num; i++)
   {
      free(progs[i].vertex);
      free(progs[i].fragment);
      progs[i].vertex   = NULL;
      progs[i].fragment = NULL;
   }

   return ret;
}

static void gl_glsl_reset_attrib(void)
{
   for (unsigned i = 0; i < gl_attrib_index; i++)
      pglDisableVertexAttribArray(gl_attribs[i]);
   gl_attrib_index = 0;
}

static void find_uniforms_frame(GLuint prog, struct shader_uniforms_frame *frame, const char *base)
{
   char texture[64];
   char texture_size[64];
   char input_size[64];
   char tex_coord[64];

   snprintf(texture, sizeof(texture), "%s%s", base, "Texture");
   snprintf(texture_size, sizeof(texture_size), "%s%s", base, "TextureSize");
   snprintf(input_size, sizeof(input_size), "%s%s", base, "InputSize");
   snprintf(tex_coord, sizeof(tex_coord), "%s%s", base, "TexCoord");

   frame->texture      = pglGetUniformLocation(prog, texture);
   frame->texture_size = pglGetUniformLocation(prog, texture_size);
   frame->input_size   = pglGetUniformLocation(prog, input_size);
   frame->tex_coord    = pglGetAttribLocation(prog, tex_coord);
}

static void find_uniforms(GLuint prog, struct shader_uniforms *uni)
{
   pglUseProgram(prog);

   uni->mvp           = pglGetUniformLocation(prog, "rubyMVPMatrix");
   uni->tex_coord     = pglGetAttribLocation(prog, "rubyTexCoord");
   uni->vertex_coord  = pglGetAttribLocation(prog, "rubyVertexCoord");
   uni->color         = pglGetAttribLocation(prog, "rubyColor");
   uni->lut_tex_coord = pglGetAttribLocation(prog, "rubyLUTTexCoord");

   uni->input_size    = pglGetUniformLocation(prog, "rubyInputSize");
   uni->output_size   = pglGetUniformLocation(prog, "rubyOutputSize");
   uni->texture_size  = pglGetUniformLocation(prog, "rubyTextureSize");

   uni->frame_count     = pglGetUniformLocation(prog, "rubyFrameCount");
   uni->frame_direction = pglGetUniformLocation(prog, "rubyFrameDirection");

   for (unsigned i = 0; i < gl_teximage_cnt; i++)
      uni->lut_texture[i] = pglGetUniformLocation(prog, gl_teximage_uniforms[i]);

   find_uniforms_frame(prog, &uni->orig, "rubyOrig");

   char frame_base[64];
   for (unsigned i = 0; i < RARCH_GLSL_MAX_SHADERS; i++)
   {
      snprintf(frame_base, sizeof(frame_base), "rubyPass%u", i + 1);
      find_uniforms_frame(prog, &uni->pass[i], frame_base);
   }

   find_uniforms_frame(prog, &uni->prev[0], "rubyPrev");
   for (unsigned i = 1; i < PREV_TEXTURES; i++)
   {
      snprintf(frame_base, sizeof(frame_base), "rubyPrev%u", i);
      find_uniforms_frame(prog, &uni->prev[i], frame_base);
   }

   pglUseProgram(0);
}

static void gl_glsl_delete_shader(GLuint prog)
{
   GLsizei count;
   GLuint shaders[2] = {0};

   pglGetAttachedShaders(prog, 2, &count, shaders);
   for (GLsizei i = 0; i < count; i++)
   {
      pglDetachShader(prog, shaders[i]);
      pglDeleteShader(shaders[i]);
   }

   pglDeleteProgram(prog);
}

static bool gl_glsl_load_shader(unsigned index, const char *path)
{
   pglUseProgram(0);

   if (gl_program[index] != gl_program[0])
   {
      gl_glsl_delete_shader(gl_program[index]);
      gl_program[index] = 0;
   }

   if (path)
   {
      struct shader_program prog = {0};
      unsigned progs = get_xml_shaders(path, &prog, 1);
      if (progs != 1)
         return false;

      if (!compile_programs(&gl_program[index], &prog, 1))
      {
         RARCH_ERR("Failed to compile shader: %s.\n", path);
         return false;
      }

      find_uniforms(gl_program[index], &gl_uniforms[index]);
   }
   else
   {
      gl_program[index]  = gl_program[0];
      gl_uniforms[index] = gl_uniforms[0];
   }

   pglUseProgram(gl_program[active_index]);
   return true;
}

// Platforms with broken get_proc_address.
// Assume functions are available without proc_address.
#undef LOAD_GL_SYM
#define LOAD_GL_SYM(SYM) if (!pgl##SYM) { \
   gfx_ctx_proc_t sym = glsl_get_proc_address("gl" #SYM); \
   memcpy(&(pgl##SYM), &sym, sizeof(sym)); \
}

bool gl_glsl_init(const char *path)
{
#if !defined(HAVE_OPENGLES2) && !defined(HAVE_OPENGL_MODERN) && !defined(__APPLE__)
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
#endif

   unsigned num_progs = 0;
   struct shader_program progs[RARCH_GLSL_MAX_SHADERS] = {{0}};
   if (path)
   {
      num_progs = get_xml_shaders(path, progs, RARCH_GLSL_MAX_SHADERS - 1);

      if (num_progs == 0)
      {
         RARCH_ERR("Couldn't find any valid shaders in XML file.\n");
         return false;
      }
   }
   else
   {
      RARCH_WARN("[GL]: Stock GLSL shaders will be used.\n");
      num_progs = 1;
      progs[0].vertex   = strdup(stock_vertex_modern);
      progs[0].fragment = strdup(stock_fragment_modern);
      glsl_modern       = true;
   }

#ifdef HAVE_OPENGLES2
   if (!glsl_modern)
   {
      RARCH_ERR("[GL]: GLES context is used, but shader is not modern. Cannot use it.\n");
      return false;
   }
#endif

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
         if (!compile_programs(&gl_program[2], progs, 1))
         {
            RARCH_ERR("Failed to compile second pass shader.\n");
            return false;
         }

         num_progs++;
      }
      else
      {
         RARCH_ERR("Did not find exactly one valid shader in secondary shader file.\n");
         return false;
      }
   }

   for (unsigned i = 0; i <= num_progs; i++)
      find_uniforms(gl_program[i], &gl_uniforms[i]);

#ifdef GLSL_DEBUG
   if (!gl_check_error())
      RARCH_WARN("Detected GL error in GLSL.\n");
#endif

   if (gl_tracker_info_cnt > 0)
   {
      struct state_tracker_info info = {0};
      info.wram      = (uint8_t*)pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
      info.info      = gl_tracker_info;
      info.info_elem = gl_tracker_info_cnt;

#ifdef HAVE_PYTHON
      info.script = gl_script_program;
      info.script_class   = *gl_tracker_script_class ? gl_tracker_script_class : NULL;
      info.script_is_file = false;
#endif

      gl_state_tracker = state_tracker_init(&info);
      if (!gl_state_tracker)
         RARCH_WARN("Failed to init state tracker.\n");
   }
   
   glsl_enable                      = true;
   gl_num_programs                  = num_progs;
   gl_program[gl_num_programs + 1]  = gl_program[0];
   gl_uniforms[gl_num_programs + 1] = gl_uniforms[0];

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
         if (gl_program[i] == 0 || (i && gl_program[i] == gl_program[0]))
            continue;

         gl_glsl_delete_shader(gl_program[i]);
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
   memset(gl_tracker_script_class, 0, sizeof(gl_tracker_script_class));

   free(gl_script_program);
   gl_script_program = NULL;

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
   // - Regular frame (rubyTexture) (always bound).
   // - LUT textures (always bound).
   // - Original texture (always bound if meaningful).
   // - FBO textures (always bound if available).
   // - Previous textures.

   if (!glsl_enable || (gl_program[active_index] == 0))
      return;

   const struct shader_uniforms *uni = &gl_uniforms[active_index];

   float input_size[2] = {(float)width, (float)height};
   float output_size[2] = {(float)out_width, (float)out_height};
   float texture_size[2] = {(float)tex_width, (float)tex_height};

   if (uni->input_size >= 0)
      pglUniform2fv(uni->input_size, 1, input_size);

   if (uni->output_size >= 0)
      pglUniform2fv(uni->output_size, 1, output_size);

   if (uni->texture_size >= 0)
      pglUniform2fv(uni->texture_size, 1, texture_size);

   if (uni->frame_count >= 0)
      pglUniform1i(uni->frame_count, frame_count);

   if (uni->frame_direction >= 0)
      pglUniform1i(uni->frame_direction, g_extern.frame_is_reverse ? -1 : 1);

   for (unsigned i = 0; i < gl_teximage_cnt; i++)
   {
      if (uni->lut_texture[i] >= 0)
         pglUniform1i(uni->lut_texture[i], i + 1);
   }

   unsigned texunit = gl_teximage_cnt + 1;

   // Set original texture unless we're in first pass (pointless).
   if (active_index > 1)
   {
      if (uni->orig.texture >= 0)
      {
         // Bind original texture.
         pglActiveTexture(GL_TEXTURE0 + texunit);
         pglUniform1i(uni->orig.texture, texunit);
         glBindTexture(GL_TEXTURE_2D, info->tex);
      }

      texunit++;

      if (uni->orig.texture_size >= 0)
         pglUniform2fv(uni->orig.texture_size, 1, info->tex_size);

      if (uni->orig.input_size >= 0)
         pglUniform2fv(uni->orig.input_size, 1, info->input_size);

      // Pass texture coordinates.
      if (uni->orig.tex_coord >= 0)
      {
         int loc = uni->orig.tex_coord;
         pglEnableVertexAttribArray(loc);
         pglVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, info->coord);
         gl_attribs[gl_attrib_index++] = loc;
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
         if (uni->pass[i].texture)
            pglUniform1i(uni->pass[i].texture, texunit);

         texunit++;

         if (uni->pass[i].texture_size >= 0)
            pglUniform2fv(uni->pass[i].texture_size, 1, fbo_info[i].tex_size);

         if (uni->pass[i].input_size >= 0)
            pglUniform2fv(uni->pass[i].input_size, 1, fbo_info[i].input_size);

         if (uni->pass[i].tex_coord >= 0)
         {
            int loc = uni->pass[i].tex_coord;
            pglEnableVertexAttribArray(loc);
            pglVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, fbo_info[i].coord);
            gl_attribs[gl_attrib_index++] = loc;
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
      if (uni->prev[i].texture >= 0)
      {
         pglActiveTexture(GL_TEXTURE0 + texunit);
         glBindTexture(GL_TEXTURE_2D, prev_info[i].tex);
         pglUniform1i(uni->prev[i].texture, texunit++);
      }

      texunit++;

      if (uni->prev[i].texture_size >= 0)
         pglUniform2fv(uni->prev[i].texture_size, 1, prev_info[i].tex_size);

      if (uni->prev[i].input_size >= 0)
         pglUniform2fv(uni->prev[i].input_size, 1, prev_info[i].input_size);

      // Pass texture coordinates.
      if (uni->prev[i].tex_coord >= 0)
      {
         int loc = uni->prev[i].tex_coord;
         pglEnableVertexAttribArray(loc);
         pglVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, prev_info[i].coord);
         gl_attribs[gl_attrib_index++] = loc; 
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
         int location = pglGetUniformLocation(gl_program[active_index], info[i].id);
         pglUniform1f(location, info[i].value);
      }
   }
}

bool gl_glsl_set_mvp(const math_matrix *mat)
{
   if (!glsl_enable || !glsl_modern)
      return false;

   int loc = gl_uniforms[active_index].mvp;
   if (loc >= 0)
      pglUniformMatrix4fv(loc, 1, GL_FALSE, mat->data);

   return true;
}

bool gl_glsl_set_coords(const struct gl_coords *coords)
{
   if (!glsl_enable || !glsl_modern)
      return false;

   const struct shader_uniforms *uni = &gl_uniforms[active_index];
   if (uni->tex_coord >= 0)
   {
      int loc = uni->tex_coord;
      pglEnableVertexAttribArray(loc);
      pglVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, coords->tex_coord);
      gl_attribs[gl_attrib_index++] = loc;
   }

   if (uni->vertex_coord >= 0)
   {
      int loc = uni->vertex_coord;
      pglEnableVertexAttribArray(loc);
      pglVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, coords->vertex);
      gl_attribs[gl_attrib_index++] = loc;
   }

   if (uni->color >= 0)
   {
      int loc = uni->color;
      pglEnableVertexAttribArray(loc);
      pglVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, 0, coords->color);
      gl_attribs[gl_attrib_index++] = loc;
   }

   if (uni->lut_tex_coord >= 0)
   {
      int loc = uni->lut_tex_coord;
      pglEnableVertexAttribArray(loc);
      pglVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, coords->lut_tex_coord);
      gl_attribs[gl_attrib_index++] = loc;
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

void gl_glsl_set_get_proc_address(gfx_ctx_proc_t (*proc)(const char*))
{
   glsl_get_proc_address = proc;
}

const gl_shader_backend_t gl_glsl_backend = {
   gl_glsl_init,
   gl_glsl_deinit,
   gl_glsl_set_params,
   gl_glsl_use,
   gl_glsl_num,
   gl_glsl_filter_type,
   gl_glsl_shader_scale,
   gl_glsl_set_coords,
   gl_glsl_set_mvp,

   gl_glsl_load_shader,
   RARCH_SHADER_GLSL,
};

