/* A slang pass translated for the Direct3D 10/11/12 drivers.
 *
 * The real slang_process() and glslang_util.c, with SPIRV-Cross built
 * the way the unity build carries it (griffin/griffin_cpp.cpp) for the
 * configuration the Makefile names. Only the GLSL to SPIR-V step is
 * stood in for: glslang_compile_spirv() hands back stock.slang's
 * stages, compiled once with glslangValidator (stock_spirv.h), and the
 * pass source is read from here rather than from a file.
 *
 * Every build that has a Direct3D 10, 11 or 12 driver has to turn a
 * slang pass into HLSL, whether or not it also has the Direct3D 9
 * HLSL driver: the pass must come back as HLSL source. A build with
 * neither has no HLSL emitter, and asking for HLSL must fail rather
 * than hand back something else.
 *
 * The stages, as compiled into stock_spirv.h:
 *
 *   #version 450
 *   layout(set = 0, binding = 0, std140) uniform UBO
 *   {
 *      mat4 MVP;
 *   } global;
 *   #pragma stage vertex
 *   layout(location = 0) in vec4 Position;
 *   layout(location = 1) in vec2 TexCoord;
 *   layout(location = 0) out vec2 vTexCoord;
 *   void main()
 *   {
 *      gl_Position = global.MVP * Position;
 *      vTexCoord   = TexCoord;
 *   }
 *   #pragma stage fragment
 *   layout(location = 0) in vec2 vTexCoord;
 *   layout(location = 0) out vec4 FragColor;
 *   layout(set = 0, binding = 2) uniform sampler2D Source;
 *   void main()
 *   {
 *      FragColor = texture(Source, vTexCoord);
 *   }
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include <boolean.h>

#include "gfx/video_shader_parse.h"
#include "gfx/drivers_shader/slang_process.h"
#include "gfx/drivers_shader/glslang_compile.h"

#include "stock_spirv.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* --- the GLSL to SPIR-V step --------------------------------------- */

static uint32_t *spirv_copy(const uint32_t *words, size_t n, size_t *len)
{
   uint32_t *out = (uint32_t*)malloc(n * sizeof(*out));
   if (!out)
      return NULL;
   memcpy(out, words, n * sizeof(*out));
   *len = n;
   return out;
}

bool glslang_compile_spirv(const char *source,
      enum glslang_compile_stage stage,
      uint32_t **spirv, size_t *spirv_len)
{
   (void)source;
   if (stage == GLSLANG_COMPILE_STAGE_VERTEX)
      *spirv = spirv_copy(stock_vert,
            sizeof(stock_vert) / sizeof(stock_vert[0]), spirv_len);
   else
      *spirv = spirv_copy(stock_frag,
            sizeof(stock_frag) / sizeof(stock_frag[0]), spirv_len);
   return *spirv != NULL;
}

/* --- the SPIR-V cache: always a miss, nothing kept ------------------ */

bool spirv_cache_compute_hash(const char *vertex_source,
      const char *fragment_source, char *hash_out)
{
   (void)vertex_source;
   (void)fragment_source;
   strcpy(hash_out, "0");
   return true;
}

bool spirv_cache_load(const char *hash, struct glslang_output *output)
{
   (void)hash;
   (void)output;
   return false;
}

bool spirv_cache_save(const char *hash, const struct glslang_output *output)
{
   (void)hash;
   (void)output;
   return true;
}

/* --- the frontend's logger ------------------------------------------ */

static char last_error[512];

void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
   (void)tag;
   (void)fmt;
   (void)ap;
}

void RARCH_LOG_BUFFER(uint8_t *data, size_t len)
{
   (void)data;
   (void)len;
}

void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(last_error, sizeof(last_error), fmt, ap);
   va_end(ap);
}

/* --- the pass source, read from here ------------------------------- */

static const char stock_slang[] =
      "#version 450\n"
      "layout(set = 0, binding = 0, std140) uniform UBO\n"
      "{\n"
      "   mat4 MVP;\n"
      "} global;\n"
      "#pragma stage vertex\n"
      "layout(location = 0) in vec4 Position;\n"
      "layout(location = 1) in vec2 TexCoord;\n"
      "layout(location = 0) out vec2 vTexCoord;\n"
      "void main()\n"
      "{\n"
      "   gl_Position = global.MVP * Position;\n"
      "   vTexCoord   = TexCoord;\n"
      "}\n"
      "#pragma stage fragment\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
      "void main()\n"
      "{\n"
      "   FragColor = texture(Source, vTexCoord);\n"
      "}\n";

bool video_shader_source_read(const char *ident, char **buf, int64_t *len)
{
   size_t n = sizeof(stock_slang) - 1;
   if (!ident || strcmp(ident, "stock.slang") || !buf)
      return false;
   if (!(*buf = (char*)malloc(n + 1)))
      return false;
   memcpy(*buf, stock_slang, n + 1);
   if (len)
      *len = (int64_t)n;
   return true;
}

bool video_shader_source_resolve(const char *parent, const char *name,
      char *s, size_t len)
{
   (void)parent;
   if (!name || !*name || !s || !len || strlen(name) >= len)
      return false;
   strcpy(s, name);
   return true;
}

const char *video_shader_source_ident_name(const char *ident)
{
   return (ident && *ident) ? ident : NULL;
}

bool video_shader_source_ident_is_slang(const char *ident)
{
   size_t n = ident ? strlen(ident) : 0;
   return n > 6 && !strcmp(ident + n - 6, ".slang");
}

/* --- the rest of the frontend --------------------------------------- */

void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category;
}

/* --- the lane ------------------------------------------------------- */

int main(void)
{
   static struct video_shader shader;
   semantics_map_t map;
   pass_semantics_t sem;
   float mvp[16];
   void *source_view = NULL;
   float source_size[4];
   bool ok;
   const char *vs;
   const char *ps;
#if defined(EXPECT_HLSL)
   const char *label = "HLSL wanted";
#else
   const char *label = "no HLSL emitter";
#endif

   memset(&map, 0, sizeof(map));
   memset(&sem, 0, sizeof(sem));
   map.textures[SLANG_TEXTURE_SEMANTIC_SOURCE].image      = &source_view;
   map.textures[SLANG_TEXTURE_SEMANTIC_SOURCE].size       = source_size;
   map.uniforms[SLANG_SEMANTIC_MVP]                       = mvp;

   shader.passes = 1;
   strcpy(shader.pass[0].source.path, "stock.slang");

   ok = slang_process(&shader, 0, RARCH_SHADER_HLSL, 50, &map, &sem);
   vs = shader.pass[0].source.string.vertex;
   ps = shader.pass[0].source.string.fragment;

#if defined(EXPECT_HLSL)
   if (!ok)
      printf("   slang_process failed: %s", last_error);
   CHECK(ok, "a slang pass did not translate to HLSL");
   CHECK(vs && strstr(vs, "SV_Position"),
         "the vertex stage did not come back as HLSL");
   CHECK(ps && strstr(ps, "SV_Target"),
         "the fragment stage did not come back as HLSL");
   CHECK(ps && !strstr(ps, "#version"),
         "the fragment stage came back as GLSL");
   CHECK(sem.texture_count == 1, "the pass's Source texture was not found");
#else
   CHECK(!ok, "HLSL was asked of a build with no HLSL emitter and "
         "the pass was accepted");
   CHECK(!vs && !ps, "a build with no HLSL emitter handed back source");
   CHECK(strstr(last_error, "HLSL") != NULL,
         "the failure did not say the HLSL backend is missing");
#endif

   free(shader.pass[0].source.string.vertex);
   free(shader.pass[0].source.string.fragment);
   free(sem.textures);
   {
      unsigned i;
      for (i = 0; i < SLANG_CBUFFER_MAX; i++)
         free(sem.cbuffers[i].uniforms);
   }

   printf("%s %s (%s)\n", failures ? "[FAIL]" : "[pass]",
         "slang_hlsl_backend", label);
   return failures ? 1 : 0;
}
