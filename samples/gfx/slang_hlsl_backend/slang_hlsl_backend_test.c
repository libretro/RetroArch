/* A slang pass translated for the Direct3D 10/11/12 drivers.
 *
 * The real slang_process() and glslang_util.c, with rslc compiling the
 * pass and printing its HLSL; only the pass source is read from here
 * rather than from a file. The pass must come back as HLSL source, and
 * a preset's simulated SourceSize reaches the first pass only.
 *
 * The stages:
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

#include "gfx/video_defines.h"


static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* --- the SPIR-V cache: always a miss, nothing kept ------------------ */

/* A key that tells the lanes' sources apart, since the blob table
 * lists live stages under it. */
static unsigned long key_sum(const char *s, unsigned long h)
{
   while (*s)
      h = h * 33 + (unsigned char)*s++;
   return h;
}

bool spirv_cache_compute_hash(const char *vertex_source,
      const char *fragment_source, const char *compiler, char *hash_out)
{
   sprintf(hash_out, "%08lx%08lx%08lx%08lx",
         key_sum(vertex_source, 5381) & 0xfffffffful,
         key_sum(fragment_source, 5381) & 0xfffffffful,
         key_sum(compiler, 5381) & 0xfffffffful,
         (unsigned long)(strlen(vertex_source) + strlen(fragment_source))
            & 0xfffffffful);
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

/* A pass that reads SourceSize. */
static const char sized_slang[] =
      "#version 450\n"
      "layout(set = 0, binding = 0, std140) uniform UBO\n"
      "{\n"
      "   mat4 MVP;\n"
      "} global;\n"
      "layout(push_constant) uniform Push\n"
      "{\n"
      "   vec4 SourceSize;\n"
      "} params;\n"
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
      "   FragColor = texture(Source,\n"
      "         vTexCoord * params.SourceSize.zw * params.SourceSize.xy);\n"
      "}\n";

/* A pass that declares a history frame and its own feedback and reads
 * neither. */
static const char unread_slang[] =
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
      "layout(set = 0, binding = 3) uniform sampler2D OriginalHistory3;\n"
      "layout(set = 0, binding = 4) uniform sampler2D PassFeedback0;\n"
      "vec4 never() { return texture(OriginalHistory3, vTexCoord); }\n"
      "void main()\n"
      "{\n"
      "   FragColor = texture(Source, vTexCoord);\n"
      "}\n";

bool video_shader_source_read(const char *ident, char **buf, int64_t *len)
{
   const char *src;
   size_t n;
   if (!ident || !buf)
      return false;
   if (!strcmp(ident, "stock.slang"))
      src = stock_slang;
   else if (!strcmp(ident, "sized.slang"))
      src = sized_slang;
   else if (!strcmp(ident, "unread.slang"))
      src = unread_slang;
   else
      return false;
   n = strlen(src);
   if (!(*buf = (char*)malloc(n + 1)))
      return false;
   memcpy(*buf, src, n + 1);
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

/* --- the simulated SourceSize ------------------------------------- */

static const uniform_sem_t *find_uniform(const pass_semantics_t *sem,
      const char *id)
{
   unsigned i;
   int j;
   for (i = 0; i < SLANG_CBUFFER_MAX; i++)
      for (j = 0; j < sem->cbuffers[i].uniform_count; j++)
         if (!strcmp(sem->cbuffers[i].uniforms[j].id, id))
            return &sem->cbuffers[i].uniforms[j];
   return NULL;
}

static void free_pass(struct video_shader *shader, unsigned n,
      pass_semantics_t *sem)
{
   unsigned i;
   free(shader->pass[n].source.string.vertex);
   free(shader->pass[n].source.string.fragment);
   shader->pass[n].source.string.vertex   = NULL;
   shader->pass[n].source.string.fragment = NULL;
   free(sem->textures);
   for (i = 0; i < SLANG_CBUFFER_MAX; i++)
      free(sem->cbuffers[i].uniforms);
   memset(sem, 0, sizeof(*sem));
}

/* A preset's simulated size reaches the first pass's SourceSize, and
 * only the first pass's. The D3D drivers copy the semantics a pass was
 * reflected into and clear the original, and Metal copies its
 * uniforms, so what the uniform reads must outlive that copy. */
static void check_simulated_source_size(void)
{
   static struct video_shader shader;
   semantics_map_t map;
   pass_semantics_t sem, kept;
   const uniform_sem_t *u;
   const float *v;
   float mvp[16];
   void *source_view = NULL;
   float source_size[4] = { 1024.0f, 768.0f, 1.0f / 1024.0f, 1.0f / 768.0f };

   memset(&map, 0, sizeof(map));
   memset(&sem, 0, sizeof(sem));
   map.textures[SLANG_TEXTURE_SEMANTIC_SOURCE].image = &source_view;
   map.textures[SLANG_TEXTURE_SEMANTIC_SOURCE].size  = source_size;
   map.uniforms[SLANG_SEMANTIC_MVP]                  = mvp;

   shader.passes          = 2;
   shader.sim_source_dims = VIDEO_SCALE_PACK(320, 240);
   strcpy(shader.pass[0].source.path, "sized.slang");
   strcpy(shader.pass[1].source.path, "sized.slang");

   CHECK(slang_process(&shader, 0, RARCH_SHADER_HLSL, 50, &map, &sem),
         "the SourceSize pass did not translate");
   u = find_uniform(&sem, "SourceSize");
   CHECK(u != NULL, "the first pass has no SourceSize uniform");
   if (u)
   {
      CHECK(u->data != (void*)source_size,
            "the first pass reads the real size with a simulated one set");
      /* As d3d10's deferred loader does: copy, then clear the original. */
      kept = sem;
      memset(&sem, 0xa5, sizeof(sem));
      u = find_uniform(&kept, "SourceSize");
      v = u ? (const float*)u->data : NULL;
      CHECK(v && v[0] == 320.0f && v[1] == 240.0f
            && v[2] == 1.0f / 320.0f && v[3] == 1.0f / 240.0f,
            "the simulated SourceSize did not survive the semantics' copy");
      free_pass(&shader, 0, &kept);
   }
   else
      free_pass(&shader, 0, &sem);

   CHECK(slang_process(&shader, 1, RARCH_SHADER_HLSL, 50, &map, &sem),
         "the second SourceSize pass did not translate");
   u = find_uniform(&sem, "SourceSize");
   CHECK(u && u->data == (void*)source_size,
         "a later pass was told the simulated size");
   free_pass(&shader, 1, &sem);

   shader.sim_source_dims = 0;
   CHECK(slang_process(&shader, 0, RARCH_SHADER_HLSL, 50, &map, &sem),
         "the SourceSize pass did not translate without a simulated size");
   u = find_uniform(&sem, "SourceSize");
   CHECK(u && u->data == (void*)source_size,
         "the first pass lost its real size with none simulated");
   free_pass(&shader, 0, &sem);
}

/* Samplers the pass never reads are left out: no texture to bind, no
 * history frame kept, no feedback buffer. */
static void check_unread_samplers(void)
{
   static struct video_shader shader;
   semantics_map_t map;
   pass_semantics_t sem;
   float mvp[16];
   void *views[4] = { NULL, NULL, NULL, NULL };
   float sizes[4][4];
   unsigned i;

   memset(&map, 0, sizeof(map));
   memset(&sem, 0, sizeof(sem));
   map.textures[SLANG_TEXTURE_SEMANTIC_SOURCE].image           = &views[0];
   map.textures[SLANG_TEXTURE_SEMANTIC_SOURCE].size            = sizes[0];
   map.textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].image = &views[1];
   map.textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size  = sizes[1];
   map.textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK].image    = &views[2];
   map.textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK].size     = sizes[2];
   map.uniforms[SLANG_SEMANTIC_MVP]                            = mvp;

   shader.passes = 1;
   strcpy(shader.pass[0].source.path, "unread.slang");
   CHECK(slang_process(&shader, 0, RARCH_SHADER_HLSL, 50, &map, &sem),
         "the pass with unread samplers did not translate");
   CHECK(sem.texture_count == 1, "an unread sampler is bound");
   CHECK(shader.history_size == 0, "an unread history frame is kept");
   CHECK(!shader.pass[0].feedback, "an unread feedback buffer is kept");
   free(shader.pass[0].source.string.vertex);
   free(shader.pass[0].source.string.fragment);
   free(sem.textures);
   for (i = 0; i < SLANG_CBUFFER_MAX; i++)
      free(sem.cbuffers[i].uniforms);
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

   free(shader.pass[0].source.string.vertex);
   free(shader.pass[0].source.string.fragment);
   free(sem.textures);
   {
      unsigned i;
      for (i = 0; i < SLANG_CBUFFER_MAX; i++)
         free(sem.cbuffers[i].uniforms);
   }

   check_simulated_source_size();
   check_unread_samplers();

   printf("%s slang_hlsl_backend\n", failures ? "[FAIL]" : "[pass]");
   return failures ? 1 : 0;
}
