/* gfx/drivers_shader/shader_gl3.c's filter chain creation with each
 * allocation it makes failed in turn: gl3_chain_new() gives a chain
 * with every pass it was asked for, or none, and what a failed one
 * made is freed (ASan, LSan).
 *
 * The file is compiled into this one (gl3_chain_new() is static) with
 * calloc() routed through the test; GL buffer calls go to no-op mocks
 * through the real glsym table, and the rest of what shader_gl3.c
 * links against comes from stubs.c. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers_shader/shader_gl3.h"
#include "../../../gfx/drivers_shader/slang_process.h"
#include "../../../gfx/drivers_shader/slang_blob.h"

static unsigned alloc_calls; /* calloc() calls so far */
static unsigned fail_at;     /* the one to fail, 1-based; 0 none */

static void *test_calloc(size_t n, size_t size)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return calloc(n, size);
}

static void *test_malloc(size_t size)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return malloc(size);
}

static void *test_realloc(void *p, size_t size)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return realloc(p, size);
}

/* Reflect the stock shader's Source sampler, with no uniform buffers.
 * With replay_reflect each pass also reads PassFeedback0 and
 * OriginalHistory1, on units 1 and 2. */
static slang_texture_semantic_meta texture_meta[SLANG_NUM_TEXTURE_SEMANTICS];
static slang_texture_semantic_meta history_meta[2];
static bool replay_reflect;
static bool test_reflection_init(slang_reflection *r)
{
   unsigned i;
   memset(r, 0, sizeof(*r));
   memset(texture_meta, 0, sizeof(texture_meta));
   memset(history_meta, 0, sizeof(history_meta));
   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      r->semantic_textures[i].data = &texture_meta[i];
      r->semantic_textures[i].size = 1;
      memset(&texture_meta[i].location, -1, sizeof(texture_meta[i].location));
   }
   if (replay_reflect)
   {
      r->semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].data =
         history_meta;
      r->semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size = 2;
      memset(&history_meta[0].location, -1, sizeof(history_meta[0].location));
      memset(&history_meta[1].location, -1, sizeof(history_meta[1].location));
   }
   return true;
}
static void test_reflection_free(slang_reflection *r)
{ memset(r, 0, sizeof(*r)); }
static bool test_reflect(const struct slang_blob *v,
      const struct slang_blob *f, slang_reflection *r)
{
   r->semantic_textures[SLANG_TEXTURE_SEMANTIC_SOURCE].data[0].texture = true;
   if (replay_reflect)
   {
      r->semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK].data[0].texture = true;
      r->semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK].data[0].binding = 1;
      r->semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].data[1].texture = true;
      r->semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].data[1].binding = 2;
   }
   return v && v->nwords && f && f->nwords;
}
static unsigned test_miplevels(unsigned w, unsigned h)
{
   unsigned levels = 1;
   unsigned side   = w > h ? w : h;
   while (side > 1)
   {
      side >>= 1;
      levels++;
   }
   return levels;
}

static unsigned next_texture = 1;
static int live_textures, live_framebuffers, live_buffers, live_shaders, live_programs;
static unsigned mipmaps, trilinear, storage_levels, draws;
static GLuint bound_texture, mip_texture;
/* What each draw saw: the textures on units 0-2, each texture's draws
 * into it so far (its generation), and what draws write. */
#define TEX_NAMES 4096
static unsigned active_unit;
static GLuint unit_texture[3];
static unsigned tex_gen[TEX_NAMES];
static GLuint fb_texture[TEX_NAMES];
static GLuint bound_fb;
static GLuint seen_texture[3];
static unsigned seen_gen[3];
static uint64_t seen_count;
static const uint64_t *watched_count;
static void APIENTRY mock_gen_textures(GLsizei n, GLuint *names)
{
   GLsizei i;
   for (i = 0; i < n; i++) names[i] = next_texture++;
   live_textures += n;
}
static void APIENTRY mock_delete_textures(GLsizei n, const GLuint *names)
{ (void)names; live_textures -= n; }
static void APIENTRY mock_bind_texture(GLenum target, GLuint name)
{
   (void)target;
   bound_texture = name;
   if (active_unit < 3)
      unit_texture[active_unit] = name;
}
static void APIENTRY mock_tex_parameter(GLenum target, GLenum pname, GLint value)
{
   (void)target;
   if (pname == GL_TEXTURE_MIN_FILTER && value == GL_LINEAR_MIPMAP_LINEAR
         && bound_texture == mip_texture && mip_texture)
      trilinear++;
}
static void APIENTRY mock_draw(GLenum mode, GLint first, GLsizei count)
{
   unsigned u;
   GLuint target = bound_fb < TEX_NAMES ? fb_texture[bound_fb] : 0;
   (void)mode;
   (void)first;
   (void)count;
   draws++;
   for (u = 0; u < 3; u++)
   {
      seen_texture[u] = unit_texture[u];
      seen_gen[u]     = unit_texture[u] < TEX_NAMES
         ? tex_gen[unit_texture[u]] : 0;
   }
   if (watched_count)
      seen_count = *watched_count;
   if (target && target < TEX_NAMES)
      tex_gen[target]++;
}

#define slang_reflection_init test_reflection_init
#define slang_reflection_free test_reflection_free
#define slang_reflect_pass test_reflect
#define glslang_num_miplevels test_miplevels
#define glGenTextures mock_gen_textures
#define glDeleteTextures mock_delete_textures
#define glBindTexture mock_bind_texture
#define glTexParameteri mock_tex_parameter
#define glDrawArrays mock_draw
bool test_image_load(struct texture_image *img, const char *path);
void test_image_free(struct texture_image *img);
#define image_texture_load test_image_load
#define image_texture_free test_image_free
#define calloc(n, size) test_calloc(n, size)
#define malloc(size) test_malloc(size)
#define realloc(p, size) test_realloc(p, size)
#include "../../../gfx/drivers_shader/shader_gl3.c"
#undef image_texture_load
#undef image_texture_free
#undef calloc
#undef malloc
#undef realloc

static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

static GLuint next_buffer = 1;
static void APIENTRY mock_gen_buffers(GLsizei n, GLuint *b)
{ GLsizei i; for (i = 0; i < n; i++) b[i] = next_buffer++; live_buffers += n; }
static void APIENTRY mock_bind_buffer(GLenum t, GLuint b) { (void)t; (void)b; }
static void APIENTRY mock_buffer_data(GLenum t, GLsizeiptr s,
      const void *d, GLenum u) { (void)t; (void)s; (void)d; (void)u; }
static void APIENTRY mock_delete_buffers(GLsizei n, const GLuint *b)
{ (void)b; live_buffers -= n; }
static void APIENTRY mock_gen_framebuffers(GLsizei n, GLuint *names)
{ GLsizei i; for (i = 0; i < n; i++) names[i] = next_buffer++; live_framebuffers += n; }
static void APIENTRY mock_delete_framebuffers(GLsizei n, const GLuint *names)
{ (void)names; live_framebuffers -= n; }
static void APIENTRY mock_bind_framebuffer(GLenum target, GLuint name)
{ (void)target; bound_fb = name; }
static GLenum APIENTRY mock_framebuffer_status(GLenum target)
{ (void)target; return GL_FRAMEBUFFER_COMPLETE; }
static void APIENTRY mock_framebuffer_texture(GLenum target, GLenum attach,
      GLenum textarget, GLuint texture, GLint level)
{
   (void)target;
   (void)attach;
   (void)textarget;
   (void)level;
   if (bound_fb < TEX_NAMES)
      fb_texture[bound_fb] = texture;
}
static void APIENTRY mock_storage(GLenum target, GLsizei levels, GLenum format,
      GLsizei width, GLsizei height)
{ (void)target; (void)format; (void)width; (void)height; storage_levels = levels; }
static void APIENTRY mock_mipmap(GLenum target)
{ (void)target; mipmaps++; mip_texture = bound_texture; }
static void APIENTRY mock_active_texture(GLenum target)
{ active_unit = target - GL_TEXTURE0; }
static void APIENTRY mock_attrib(GLuint index) { (void)index; }
static void APIENTRY mock_attrib_pointer(GLuint index, GLint size, GLenum type,
      GLboolean normalized, GLsizei stride, const void *p)
{ (void)index; (void)size; (void)type; (void)normalized; (void)stride; (void)p; }

/* Programs build: every compile and link succeeds */
static GLuint next_name = 1;
static GLuint APIENTRY mock_create(GLenum stage)
{ (void)stage; live_shaders++; return next_name++; }
static GLuint APIENTRY mock_create_program(void) { live_programs++; return next_name++; }
static void APIENTRY mock_delete_shader(GLuint o) { (void)o; live_shaders--; }
static void APIENTRY mock_delete_program(GLuint o) { (void)o; live_programs--; }
static void APIENTRY mock_shader_source(GLuint s, GLsizei c,
      const GLchar *const *str, const GLint *len)
{ (void)s; (void)c; (void)str; (void)len; }
static void APIENTRY mock_compile(GLuint s) { (void)s; }
static void APIENTRY mock_get_iv(GLuint o, GLenum pname, GLint *v)
{ (void)o; *v = (pname == GL_COMPILE_STATUS || pname == GL_LINK_STATUS) ? 1 : 0; }
static void APIENTRY mock_get_log(GLuint o, GLsizei n, GLsizei *len, GLchar *log)
{ (void)o; (void)n; if (len) *len = 0; if (log && n) log[0] = 0; }
static void APIENTRY mock_attach(GLuint p, GLuint s) { (void)p; (void)s; }
static void APIENTRY mock_bind_attrib(GLuint p, GLuint i, const GLchar *n)
{ (void)p; (void)i; (void)n; }
static void APIENTRY mock_use(GLuint p) { (void)p; }
static GLint APIENTRY mock_uniform_location(GLuint p, const GLchar *n)
{ (void)p; (void)n; return -1; }
static GLuint APIENTRY mock_uniform_block_index(GLuint p, const GLchar *n)
{ (void)p; (void)n; return GL_INVALID_INDEX; }

#define PASSES 3

static void check_gl_objects(void)
{
   CHECK(!live_textures && !live_framebuffers && !live_buffers
         && !live_shaders && !live_programs,
         "live GL objects: textures %d, framebuffers %d, buffers %d, shaders %d, programs %d",
         live_textures, live_framebuffers, live_buffers, live_shaders, live_programs);
}

static void render_shrink_chain(struct gl3_filter_chain *chain)
{
   struct gl3_filter_chain_texture texture;
   struct gl3_viewport vp;
   float mvp[16];
   memset(&texture, 0, sizeof(texture));
   texture.image  = 0x100000;
   texture.dims   = VIDEO_SCALE_PACK(256, 128);
   texture.format = GL_RGBA8;
   vp.pos         = 0;
   vp.dims        = VIDEO_SCALE_PACK(32, 16);
   gl3_build_default_matrix(mvp);
   mipmaps = trilinear = storage_levels = draws = 0;
   mip_texture = 0;
   gl3_filter_chain_set_input_texture(chain, &texture);
   gl3_filter_chain_build_offscreen_passes(chain, &vp);
   gl3_filter_chain_build_viewport_pass(chain, &vp, mvp);
   CHECK(mipmaps == 1 && storage_levels == 9 && trilinear && draws == 2,
         "shrink draw: %u mipmaps, %u levels, %u trilinear samples, %u draws",
         mipmaps, storage_levels, trilinear, draws);
   CHECK(chain->passes[0]->framebuffer->size_dims == texture.dims,
         "copy pass changed the source size");
}

/* A blob of the stock stage's words, as the compile output hands one
 * over. */
static struct slang_blob *stock_blob(const uint32_t *words, size_t len)
{
   uint32_t *copy = (uint32_t*)malloc(len);
   if (!copy)
      return NULL;
   memcpy(copy, words, len);
   return slang_blob_adopt_words(copy, len / sizeof(uint32_t));
}

/* @passes passes binding @vs/@fs by reference, as a preset's passes do
 * once the blob table hands the same stages to each of them, with the
 * text rslc printed for them. */
static bool chain_replay;
static struct gl3_filter_chain *chain_of(unsigned passes,
      struct slang_blob *vs, struct slang_blob *fs)
{
   struct gl3_filter_chain_pass_info pass_info;
   struct gl3_filter_chain *chain = gl3_chain_new(passes);
   unsigned i;
   if (!chain)
      return NULL;
   chain->replay = chain_replay;
   pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_x       = 1.0f;
   pass_info.scale_y       = 1.0f;
   pass_info.rt_format     = 0;
   pass_info.source_filter = GLSLANG_FILTER_CHAIN_NEAREST;
   pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_NEAREST;
   pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;
   pass_info.max_levels    = 0;
   for (i = 0; i < passes; i++)
   {
      gl3_chain_set_pass_info(chain, i, pass_info);
      gl3_chain_set_blob(chain, i, GL_VERTEX_SHADER, slang_blob_ref(vs));
      gl3_chain_set_blob(chain, i, GL_FRAGMENT_SHADER, slang_blob_ref(fs));
      gl3_pass_set_shader_text(chain->passes[i], GL_VERTEX_SHADER, "void main() {}\n");
      gl3_pass_set_shader_text(chain->passes[i], GL_FRAGMENT_SHADER, "void main() {}\n");
   }
   if (!gl3_chain_init(chain))
   {
      gl3_chain_free(chain);
      return NULL;
   }
   return chain;
}

/* Passes that bind the same two blobs share one linked program, within
 * a chain and across the chain being built and the one it replaces;
 * the program goes with the last pass that holds it. */
static void test_shared_programs(void)
{
   struct slang_blob *vs = stock_blob(spirv_opaque_vert, spirv_opaque_vert_size);
   struct slang_blob *fs = stock_blob(spirv_opaque_frag, spirv_opaque_frag_size);
   struct slang_blob *fs2 = stock_blob(spirv_opaque_frag, spirv_opaque_frag_size);
   struct gl3_filter_chain *live, *next;
   unsigned reused0 = gl3_programs.reused;
   /* every chain links a quad program of its own besides its passes' */
   int chains = 0;

   CHECK(vs && fs && fs2, "stock blobs");
   live = chain_of(3, vs, fs);
   chains++;
   CHECK(live && live_programs - chains == 1 && gl3_programs.n == 1,
         "three passes of one blob pair: %d pass programs, %u listed",
         live_programs - chains, (unsigned)gl3_programs.n);
   CHECK(gl3_programs.reused - reused0 == 2, "two of them took the first one's program");
   CHECK(live && live->passes[0]->pipeline == live->passes[2]->pipeline,
         "the passes draw with the same program");

   next = chain_of(2, vs, fs);
   chains++;
   CHECK(next && live_programs - chains == 1, "a chain built beside the live one links nothing");
   gl3_chain_free(live);
   chains--;
   CHECK(live_programs - chains == 1, "freeing the outgoing chain keeps the program the new one holds");
   live = next;

   next = chain_of(1, vs, fs2);
   chains++;
   CHECK(next && live_programs - chains == 2, "another fragment blob is another program");
   gl3_chain_free(live);
   chains--;
   CHECK(live_programs - chains == 1 && gl3_programs.n == 1, "the first program went with its last pass");
   gl3_chain_free(next);
   chains--;
   CHECK(live_programs == 0 && gl3_programs.n == 0 && !gl3_programs.v,
         "no pass, no program, no table");

   CHECK(retro_atomic_load_relaxed_int(&vs->refs) == 1 && retro_atomic_load_relaxed_int(&fs->refs) == 1 && retro_atomic_load_relaxed_int(&fs2->refs) == 1,
         "the programs' blob references went with them");
   slang_blob_unref(vs);
   slang_blob_unref(fs);
   slang_blob_unref(fs2);
   check_gl_objects();
}

/* LUT images decoded, by path */
static unsigned used_loads, unused_loads;
bool test_image_load(struct texture_image *img, const char *path)
{
   if (!strcmp(path, "used.png"))
      used_loads++;
   else
      unused_loads++;
   img->width  = 4;
   img->height = 4;
   img->pixels = (uint32_t*)calloc(16, sizeof(uint32_t));
   return img->pixels != NULL;
}
void test_image_free(struct texture_image *img)
{
   free(img->pixels);
   img->pixels = NULL;
}

/* A LUT no pass reads is named for the semantic map and never loaded;
 * one a pass reads is loaded once, however often the chain is built. */
static void test_unread_luts(void)
{
   static slang_texture_semantic_meta reads[1];
   struct gl3_filter_chain *chain = gl3_chain_new(1);
   struct video_shader *preset    = (struct video_shader*)calloc(1, sizeof(*preset));
   slang_texture_semantic_array *user;

   CHECK(chain && preset, "a chain and a preset");
   if (!chain || !preset)
   {
      free(preset);
      gl3_chain_free(chain);
      return;
   }
   preset->luts = 2;
   strcpy(preset->lut[0].id, "Used");
   strcpy(preset->lut[0].path, "used.png");
   strcpy(preset->lut[1].id, "Unused");
   strcpy(preset->lut[1].path, "unused.png");
   used_loads = unused_loads = 0;
   CHECK(gl3_filter_chain_load_luts(chain, preset), "the LUTs are named");
   gl3_chain_set_shader_preset(chain, preset);
   CHECK(chain->common.num_luts == 2 && !strcmp(chain->common.luts[1].id, "Unused"),
         "both LUTs are named");
   CHECK(!used_loads && !unused_loads, "nothing is loaded before the passes are reflected");

   memset(reads, 0, sizeof(reads));
   reads[0].texture = true;
   user       = &chain->passes[0]->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_USER];
   user->data = reads;
   user->size = 1;
   CHECK(gl3_chain_load_luts(chain), "the read LUT loads");
   CHECK(gl3_chain_load_luts(chain), "a rebuild loads nothing new");
   CHECK(used_loads == 1, "the read LUT is decoded once: %u", used_loads);
   CHECK(!unused_loads, "the unread LUT is never decoded: %u", unused_loads);
   CHECK(chain->common.luts[0].image && !chain->common.luts[1].image,
         "only the read LUT has a texture");
   user->data = NULL;
   user->size = 0;
   gl3_chain_free(chain);
   check_gl_objects();
}

static void test_shrink_chain(void)
{
   struct gl3_filter_chain *chain;
   unsigned allocs, k;

   alloc_calls = 0;
   fail_at     = 0;
   chain       = gl3_filter_chain_create_shrink();
   allocs      = alloc_calls;
   CHECK(chain && chain->num_passes == 2, "shrink chain must have two passes");
   if (chain)
      render_shrink_chain(chain);
   gl3_chain_free(chain);
   check_gl_objects();
   for (k = 1; k <= allocs; k++)
   {
      alloc_calls = 0;
      fail_at     = k;
      chain       = gl3_filter_chain_create_shrink();
      fail_at     = 0;
      CHECK(!chain || (chain->num_passes == 2 && chain->passes[0]->framebuffer),
            "shrink allocation %u of %u left an incomplete chain", k, allocs);
      gl3_chain_free(chain);
      check_gl_objects();
   }
}

/* The history copy, drawn into its framebuffer's texture. */
extern void (*test_framebuffer_copied)(unsigned fb_id);
static void framebuffer_copied(unsigned fb_id)
{
   if (fb_id < TEX_NAMES && fb_texture[fb_id] < TEX_NAMES)
      tex_gen[fb_texture[fb_id]]++;
}

/* One frame through @chain: its input @in, count @count, every pass,
 * and the frame ended. */
static void replay_frame(struct gl3_filter_chain *chain, GLuint in,
      uint64_t count)
{
   struct gl3_filter_chain_texture texture;
   struct gl3_viewport vp;
   float mvp[16];
   memset(&texture, 0, sizeof(texture));
   texture.image  = in;
   texture.dims   = VIDEO_SCALE_PACK(64, 32);
   texture.format = GL_RGBA8;
   vp.pos         = 0;
   vp.dims        = VIDEO_SCALE_PACK(64, 32);
   gl3_build_default_matrix(mvp);
   gl3_filter_chain_set_frame_count(chain, count);
   gl3_filter_chain_set_input_texture(chain, &texture);
   gl3_filter_chain_build_offscreen_passes(chain, &vp);
   mock_bind_framebuffer(GL_FRAMEBUFFER, 0);
   gl3_filter_chain_build_viewport_pass(chain, &vp, mvp);
   gl3_filter_chain_end_frame(chain);
}

/* A replay of the last frame's final pass, in a later frame and at
 * another size, reads what that pass read: the same textures - the
 * first pass's output, its feedback, the history - with nothing drawn
 * into them since, and that frame's count. Feedback does not swap. The
 * history keeps a spare slot, so the frame's end did not write over the
 * one the pass read. */
static void test_replay(void)
{
   GLuint   texture[3];
   unsigned gen[3], u;
   struct gl3_filter_chain *chain;
   struct gl3_framebuffer *fb, *feedback;
   struct gl3_viewport vp;
   float mvp[16];
   uint32_t words[] = { 0x07230203u, 0x00010000u };
   struct slang_blob *vs = stock_blob(words, sizeof(words));
   struct slang_blob *fs = stock_blob(words, sizeof(words));

   /* Created to replay: one history texture more than read. Any other
    * chain keeps only what its passes read. */
   replay_reflect = true;
   chain          = chain_of(2, vs, fs);
   CHECK(chain && chain->num_original_history == 1,
         "a chain made without replay keeps %u history framebuffers",
         chain ? (unsigned)chain->num_original_history : 0);
   gl3_chain_free(chain);
   chain_replay   = true;
   chain          = chain_of(2, vs, fs);
   chain_replay   = false;
   replay_reflect = false;
   slang_blob_unref(vs);
   slang_blob_unref(fs);
   CHECK(chain, "replay chain not built");
   if (!chain)
      return;
   watched_count           = &chain->passes[1]->frame_count;
   test_framebuffer_copied = framebuffer_copied;
   CHECK(chain->num_original_history == 2
         && chain->common.num_original_history == 1,
         "history: %u framebuffers for %u read",
         (unsigned)chain->num_original_history,
         (unsigned)chain->common.num_original_history);
   CHECK(!gl3_filter_chain_replay_ready(chain),
         "a chain with no frame drawn is ready to replay");
   replay_frame(chain, 0x1001, 1);
   replay_frame(chain, 0x1002, 2);
   for (u = 0; u < 3; u++)
   {
      texture[u] = seen_texture[u];
      gen[u]     = seen_gen[u];
   }
   CHECK(seen_count == 2, "frame 2 drew with count %u", (unsigned)seen_count);
   CHECK(gl3_filter_chain_replay_ready(chain), "no replay after a frame");

   fb       = chain->passes[0]->framebuffer;
   feedback = chain->passes[0]->framebuffer_feedback;
   gl3_filter_chain_set_frame_count(chain, 3);
   vp.pos  = 0;
   vp.dims = VIDEO_SCALE_PACK(80, 40);
   gl3_build_default_matrix(mvp);
   mock_bind_framebuffer(GL_FRAMEBUFFER, 0);
   draws = 0;
   gl3_filter_chain_build_viewport_pass_replay(chain, &vp, mvp);
   CHECK(draws == 1, "replay drew %u passes", draws);
   for (u = 0; u < 3; u++)
      CHECK(seen_texture[u] == texture[u] && seen_gen[u] == gen[u],
            "replay unit %u: texture %u gen %u, want %u gen %u", u,
            seen_texture[u], seen_gen[u], texture[u], gen[u]);
   CHECK(seen_count == 2, "replay drew with count %u, want 2",
         (unsigned)seen_count);
   CHECK(chain->passes[1]->frame_count == 3,
         "replay left the count at %u",
         (unsigned)chain->passes[1]->frame_count);
   CHECK(chain->passes[0]->framebuffer == fb
         && chain->passes[0]->framebuffer_feedback == feedback,
         "replay swapped the feedback buffers");
   watched_count           = NULL;
   test_framebuffer_copied = NULL;
   gl3_chain_free(chain);
}

int main(void)
{
   unsigned allocs, k;
   struct gl3_filter_chain *chain;

   __rglgen_glGenBuffers    = (RGLSYMGLGENBUFFERSPROC)mock_gen_buffers;
   __rglgen_glBindBuffer    = (RGLSYMGLBINDBUFFERPROC)mock_bind_buffer;
   __rglgen_glBufferData    = (RGLSYMGLBUFFERDATAPROC)mock_buffer_data;
   __rglgen_glDeleteBuffers = (RGLSYMGLDELETEBUFFERSPROC)mock_delete_buffers;
   __rglgen_glCreateShader       = (RGLSYMGLCREATESHADERPROC)mock_create;
   __rglgen_glDeleteShader       = (RGLSYMGLDELETESHADERPROC)mock_delete_shader;
   __rglgen_glShaderSource       = (RGLSYMGLSHADERSOURCEPROC)mock_shader_source;
   __rglgen_glCompileShader      = (RGLSYMGLCOMPILESHADERPROC)mock_compile;
   __rglgen_glGetShaderiv        = (RGLSYMGLGETSHADERIVPROC)mock_get_iv;
   __rglgen_glGetShaderInfoLog   = (RGLSYMGLGETSHADERINFOLOGPROC)mock_get_log;
   __rglgen_glCreateProgram      = (RGLSYMGLCREATEPROGRAMPROC)mock_create_program;
   __rglgen_glDeleteProgram      = (RGLSYMGLDELETEPROGRAMPROC)mock_delete_program;
   __rglgen_glAttachShader       = (RGLSYMGLATTACHSHADERPROC)mock_attach;
   __rglgen_glBindAttribLocation = (RGLSYMGLBINDATTRIBLOCATIONPROC)mock_bind_attrib;
   __rglgen_glLinkProgram        = (RGLSYMGLLINKPROGRAMPROC)mock_compile;
   __rglgen_glGetProgramiv       = (RGLSYMGLGETPROGRAMIVPROC)mock_get_iv;
   __rglgen_glGetProgramInfoLog  = (RGLSYMGLGETPROGRAMINFOLOGPROC)mock_get_log;
   __rglgen_glUseProgram         = (RGLSYMGLUSEPROGRAMPROC)mock_use;
   __rglgen_glGetUniformLocation = (RGLSYMGLGETUNIFORMLOCATIONPROC)mock_uniform_location;
   __rglgen_glGetUniformBlockIndex = (RGLSYMGLGETUNIFORMBLOCKINDEXPROC)mock_uniform_block_index;
   __rglgen_glGenFramebuffers       = (RGLSYMGLGENFRAMEBUFFERSPROC)mock_gen_framebuffers;
   __rglgen_glDeleteFramebuffers    = (RGLSYMGLDELETEFRAMEBUFFERSPROC)mock_delete_framebuffers;
   __rglgen_glBindFramebuffer       = (RGLSYMGLBINDFRAMEBUFFERPROC)mock_bind_framebuffer;
   __rglgen_glCheckFramebufferStatus = (RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC)mock_framebuffer_status;
   __rglgen_glFramebufferTexture2D  = (RGLSYMGLFRAMEBUFFERTEXTURE2DPROC)mock_framebuffer_texture;
   __rglgen_glTexStorage2D          = (RGLSYMGLTEXSTORAGE2DPROC)mock_storage;
   __rglgen_glGenerateMipmap        = (RGLSYMGLGENERATEMIPMAPPROC)mock_mipmap;
   __rglgen_glActiveTexture         = (RGLSYMGLACTIVETEXTUREPROC)mock_active_texture;
   __rglgen_glEnableVertexAttribArray = (RGLSYMGLENABLEVERTEXATTRIBARRAYPROC)mock_attrib;
   __rglgen_glDisableVertexAttribArray = (RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC)mock_attrib;
   __rglgen_glVertexAttribPointer   = (RGLSYMGLVERTEXATTRIBPOINTERPROC)mock_attrib_pointer;

   alloc_calls = 0;
   fail_at     = 0;
   chain       = gl3_chain_new(PASSES);
   allocs      = alloc_calls;
   CHECK(chain && chain->num_passes == PASSES, "with memory: %u passes",
         PASSES);
   gl3_chain_free(chain);

   for (k = 1; k <= allocs; k++)
   {
      alloc_calls = 0;
      fail_at     = k;
      chain       = gl3_chain_new(PASSES);
      fail_at     = 0;
      CHECK(!chain || chain->num_passes == PASSES,
            "allocation %u of %u failed and gl3_chain_new gave a chain "
            "of %u passes", k, allocs,
            chain ? (unsigned)chain->num_passes : 0);
      gl3_chain_free(chain);
   }

   test_shrink_chain();
   test_shared_programs();
   test_unread_luts();
   test_replay();

   if (failures)
   {
      printf("gl3_chain_alloc_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("gl3_chain_alloc_test: OK (%u allocations)\n", allocs);
   return 0;
}
