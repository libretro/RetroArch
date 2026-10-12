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

/* Reflect the stock shader's Source sampler, with no uniform buffers. */
static slang_texture_semantic_meta texture_meta[SLANG_NUM_TEXTURE_SEMANTICS];
static bool test_reflection_init(slang_reflection *r)
{
   unsigned i;
   memset(r, 0, sizeof(*r));
   memset(texture_meta, 0, sizeof(texture_meta));
   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      r->semantic_textures[i].data = &texture_meta[i];
      r->semantic_textures[i].size = 1;
      memset(&texture_meta[i].location, -1, sizeof(texture_meta[i].location));
   }
   return true;
}
static void test_reflection_free(slang_reflection *r)
{ memset(r, 0, sizeof(*r)); }
static bool test_reflect(const uint32_t *v, size_t vn,
      const uint32_t *f, size_t fn, slang_reflection *r)
{
   r->semantic_textures[SLANG_TEXTURE_SEMANTIC_SOURCE].data[0].texture = true;
   return v && vn && f && fn;
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
static void APIENTRY mock_gen_textures(GLsizei n, GLuint *names)
{
   GLsizei i;
   for (i = 0; i < n; i++) names[i] = next_texture++;
   live_textures += n;
}
static void APIENTRY mock_delete_textures(GLsizei n, const GLuint *names)
{ (void)names; live_textures -= n; }
static void APIENTRY mock_bind_texture(GLenum target, GLuint name)
{ (void)target; bound_texture = name; }
static void APIENTRY mock_tex_parameter(GLenum target, GLenum pname, GLint value)
{
   (void)target;
   if (pname == GL_TEXTURE_MIN_FILTER && value == GL_LINEAR_MIPMAP_LINEAR
         && bound_texture == mip_texture && mip_texture)
      trilinear++;
}
static void APIENTRY mock_draw(GLenum mode, GLint first, GLsizei count)
{ (void)mode; (void)first; (void)count; draws++; }

#define slang_reflection_init test_reflection_init
#define slang_reflection_free test_reflection_free
#define slang_reflect_spirv test_reflect
#define glslang_num_miplevels test_miplevels
#define glGenTextures mock_gen_textures
#define glDeleteTextures mock_delete_textures
#define glBindTexture mock_bind_texture
#define glTexParameteri mock_tex_parameter
#define glDrawArrays mock_draw
#define calloc(n, size) test_calloc(n, size)
#define malloc(size) test_malloc(size)
#define realloc(p, size) test_realloc(p, size)
#include "../../../gfx/drivers_shader/shader_gl3.c"
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
{ (void)target; (void)name; }
static GLenum APIENTRY mock_framebuffer_status(GLenum target)
{ (void)target; return GL_FRAMEBUFFER_COMPLETE; }
static void APIENTRY mock_framebuffer_texture(GLenum target, GLenum attach,
      GLenum textarget, GLuint texture, GLint level)
{ (void)target; (void)attach; (void)textarget; (void)texture; (void)level; }
static void APIENTRY mock_storage(GLenum target, GLsizei levels, GLenum format,
      GLsizei width, GLsizei height)
{ (void)target; (void)format; (void)width; (void)height; storage_levels = levels; }
static void APIENTRY mock_mipmap(GLenum target)
{ (void)target; mipmaps++; mip_texture = bound_texture; }
static void APIENTRY mock_active_texture(GLenum target) { (void)target; }
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

   if (failures)
   {
      printf("gl3_chain_alloc_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("gl3_chain_alloc_test: OK (%u allocations)\n", allocs);
   return 0;
}
