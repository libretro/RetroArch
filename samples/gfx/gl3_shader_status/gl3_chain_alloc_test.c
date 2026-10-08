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

static unsigned alloc_calls; /* calloc() calls so far */
static unsigned fail_at;     /* the one to fail, 1-based; 0 none */

static void *test_calloc(size_t n, size_t size)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return calloc(n, size);
}

#define calloc(n, size) test_calloc(n, size)
#include "../../../gfx/drivers_shader/shader_gl3.c"
#undef calloc

static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

static GLuint next_buffer = 1;
static void APIENTRY mock_gen_buffers(GLsizei n, GLuint *b)
{ GLsizei i; for (i = 0; i < n; i++) b[i] = next_buffer++; }
static void APIENTRY mock_bind_buffer(GLenum t, GLuint b) { (void)t; (void)b; }
static void APIENTRY mock_buffer_data(GLenum t, GLsizeiptr s,
      const void *d, GLenum u) { (void)t; (void)s; (void)d; (void)u; }
static void APIENTRY mock_delete_buffers(GLsizei n, const GLuint *b)
{ (void)n; (void)b; }

/* Programs build: every compile and link succeeds */
static GLuint next_name = 1;
static GLuint APIENTRY mock_create(GLenum stage) { (void)stage; return next_name++; }
static GLuint APIENTRY mock_create_program(void) { return next_name++; }
static void APIENTRY mock_delete(GLuint o) { (void)o; }
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

int main(void)
{
   unsigned allocs, k;
   struct gl3_filter_chain *chain;

   __rglgen_glGenBuffers    = (RGLSYMGLGENBUFFERSPROC)mock_gen_buffers;
   __rglgen_glBindBuffer    = (RGLSYMGLBINDBUFFERPROC)mock_bind_buffer;
   __rglgen_glBufferData    = (RGLSYMGLBUFFERDATAPROC)mock_buffer_data;
   __rglgen_glDeleteBuffers = (RGLSYMGLDELETEBUFFERSPROC)mock_delete_buffers;
   __rglgen_glCreateShader       = (RGLSYMGLCREATESHADERPROC)mock_create;
   __rglgen_glDeleteShader       = (RGLSYMGLDELETESHADERPROC)mock_delete;
   __rglgen_glShaderSource       = (RGLSYMGLSHADERSOURCEPROC)mock_shader_source;
   __rglgen_glCompileShader      = (RGLSYMGLCOMPILESHADERPROC)mock_compile;
   __rglgen_glGetShaderiv        = (RGLSYMGLGETSHADERIVPROC)mock_get_iv;
   __rglgen_glGetShaderInfoLog   = (RGLSYMGLGETSHADERINFOLOGPROC)mock_get_log;
   __rglgen_glCreateProgram      = (RGLSYMGLCREATEPROGRAMPROC)mock_create_program;
   __rglgen_glDeleteProgram      = (RGLSYMGLDELETEPROGRAMPROC)mock_delete;
   __rglgen_glAttachShader       = (RGLSYMGLATTACHSHADERPROC)mock_attach;
   __rglgen_glBindAttribLocation = (RGLSYMGLBINDATTRIBLOCATIONPROC)mock_bind_attrib;
   __rglgen_glLinkProgram        = (RGLSYMGLLINKPROGRAMPROC)mock_compile;
   __rglgen_glGetProgramiv       = (RGLSYMGLGETPROGRAMIVPROC)mock_get_iv;
   __rglgen_glGetProgramInfoLog  = (RGLSYMGLGETPROGRAMINFOLOGPROC)mock_get_log;
   __rglgen_glUseProgram         = (RGLSYMGLUSEPROGRAMPROC)mock_use;
   __rglgen_glGetUniformLocation = (RGLSYMGLGETUNIFORMLOCATIONPROC)mock_uniform_location;
   __rglgen_glGetUniformBlockIndex = (RGLSYMGLGETUNIFORMBLOCKINDEXPROC)mock_uniform_block_index;

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

   if (failures)
   {
      printf("gl3_chain_alloc_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("gl3_chain_alloc_test: OK (%u allocations)\n", allocs);
   return 0;
}
