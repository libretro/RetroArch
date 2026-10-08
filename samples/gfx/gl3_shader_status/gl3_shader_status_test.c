/* gfx/drivers_shader/shader_gl3.c's program build against a driver
 * that fails a compile or a link.
 *
 * A failed compile or link is a failure whether or not the driver wrote
 * an info log: gl3_cross_compile_program() returns 0, and every shader
 * and program it created is deleted. Each case runs with no log, with a
 * log, and - for the one that succeeds - checks a program comes back.
 *
 * The GL entry points are the real glsym table pointed at mocks that
 * track live objects; SPIRV-Cross is stood in for here, handing back a
 * source for each stage and no resources. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <boolean.h>
#include <glsym/glsym.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <spirv_cross_c.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "gfx/drivers_shader/shader_gl3.h"

static unsigned failures = 0;
static unsigned err_lines = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

void RARCH_ERR(const char *fmt, ...) { (void)fmt; err_lines++; }

/* ---- the driver ---- */
static GLint    fail_vertex;     /* the vertex stage does not compile  */
static GLint    fail_fragment;   /* the fragment stage does not compile */
static GLint    fail_link;
static GLint    log_len;         /* what INFO_LOG_LENGTH says          */
static int      live_shaders;
static int      live_programs;
static GLuint   next_name = 1;
static GLenum   stage_of[64];

static GLuint APIENTRY mock_create_shader(GLenum stage)
{
   GLuint n = next_name++;
   if (n < 64)
      stage_of[n] = stage;
   live_shaders++;
   return n;
}
static void APIENTRY mock_delete_shader(GLuint s) { if (s) live_shaders--; }
static void APIENTRY mock_shader_source(GLuint s, GLsizei c,
      const GLchar *const *src, const GLint *len)
{ (void)s; (void)c; (void)src; (void)len; }
static void APIENTRY mock_compile_shader(GLuint s) { (void)s; }
static void APIENTRY mock_get_shaderiv(GLuint s, GLenum pname, GLint *v)
{
   if (pname == GL_COMPILE_STATUS)
      *v = (s < 64 && stage_of[s] == GL_VERTEX_SHADER)
         ? !fail_vertex : !fail_fragment;
   else if (pname == GL_INFO_LOG_LENGTH)
      *v = log_len;
}
static void APIENTRY mock_get_shader_log(GLuint s, GLsizei n, GLsizei *len,
      GLchar *log)
{
   (void)s;
   if (n > 0)
      log[0] = '\0';
   if (len)
      *len = 0;
}
static GLuint APIENTRY mock_create_program(void)
{
   live_programs++;
   return next_name++;
}
static void APIENTRY mock_delete_program(GLuint p) { if (p) live_programs--; }
static void APIENTRY mock_attach_shader(GLuint p, GLuint s) { (void)p; (void)s; }
static void APIENTRY mock_bind_attrib(GLuint p, GLuint i, const GLchar *n)
{ (void)p; (void)i; (void)n; }
static void APIENTRY mock_link_program(GLuint p) { (void)p; }
static void APIENTRY mock_get_programiv(GLuint p, GLenum pname, GLint *v)
{
   (void)p;
   if (pname == GL_LINK_STATUS)
      *v = !fail_link;
   else if (pname == GL_INFO_LOG_LENGTH)
      *v = log_len;
}
static void APIENTRY mock_get_program_log(GLuint p, GLsizei n, GLsizei *len,
      GLchar *log)
{
   (void)p;
   if (n > 0)
      log[0] = '\0';
   if (len)
      *len = 0;
}
static void APIENTRY mock_use_program(GLuint p) { (void)p; }

static void mock_install(void)
{
   __rglgen_glCreateShader      = (RGLSYMGLCREATESHADERPROC)mock_create_shader;
   __rglgen_glDeleteShader      = (RGLSYMGLDELETESHADERPROC)mock_delete_shader;
   __rglgen_glShaderSource      = (RGLSYMGLSHADERSOURCEPROC)mock_shader_source;
   __rglgen_glCompileShader     = (RGLSYMGLCOMPILESHADERPROC)mock_compile_shader;
   __rglgen_glGetShaderiv       = (RGLSYMGLGETSHADERIVPROC)mock_get_shaderiv;
   __rglgen_glGetShaderInfoLog  = (RGLSYMGLGETSHADERINFOLOGPROC)mock_get_shader_log;
   __rglgen_glCreateProgram     = (RGLSYMGLCREATEPROGRAMPROC)mock_create_program;
   __rglgen_glDeleteProgram     = (RGLSYMGLDELETEPROGRAMPROC)mock_delete_program;
   __rglgen_glAttachShader      = (RGLSYMGLATTACHSHADERPROC)mock_attach_shader;
   __rglgen_glBindAttribLocation= (RGLSYMGLBINDATTRIBLOCATIONPROC)mock_bind_attrib;
   __rglgen_glLinkProgram       = (RGLSYMGLLINKPROGRAMPROC)mock_link_program;
   __rglgen_glGetProgramiv      = (RGLSYMGLGETPROGRAMIVPROC)mock_get_programiv;
   __rglgen_glGetProgramInfoLog = (RGLSYMGLGETPROGRAMINFOLOGPROC)mock_get_program_log;
   __rglgen_glUseProgram        = (RGLSYMGLUSEPROGRAMPROC)mock_use_program;
}

/* ---- SPIRV-Cross: a source per stage, no resources ---- */
static int dummy;

spvc_result spvc_context_create(spvc_context *c)
{ *c = (spvc_context)&dummy; return SPVC_SUCCESS; }
void spvc_context_destroy(spvc_context c) { (void)c; }
const char *spvc_context_get_last_error_string(spvc_context c)
{ (void)c; return ""; }
spvc_result spvc_context_parse_spirv(spvc_context c, const SpvId *s,
      size_t n, spvc_parsed_ir *ir)
{ (void)c; (void)s; (void)n; *ir = (spvc_parsed_ir)&dummy; return SPVC_SUCCESS; }
spvc_result spvc_context_create_compiler(spvc_context c, spvc_backend b,
      spvc_parsed_ir ir, spvc_capture_mode m, spvc_compiler *out)
{ (void)c; (void)b; (void)ir; (void)m; *out = (spvc_compiler)&dummy; return SPVC_SUCCESS; }
spvc_result spvc_compiler_create_shader_resources(spvc_compiler c,
      spvc_resources *r)
{ (void)c; *r = (spvc_resources)&dummy; return SPVC_SUCCESS; }
spvc_result spvc_compiler_create_compiler_options(spvc_compiler c,
      spvc_compiler_options *o)
{ (void)c; *o = (spvc_compiler_options)&dummy; return SPVC_SUCCESS; }
spvc_result spvc_compiler_options_set_bool(spvc_compiler_options o,
      spvc_compiler_option opt, spvc_bool v)
{ (void)o; (void)opt; (void)v; return SPVC_SUCCESS; }
spvc_result spvc_compiler_options_set_uint(spvc_compiler_options o,
      spvc_compiler_option opt, unsigned v)
{ (void)o; (void)opt; (void)v; return SPVC_SUCCESS; }
spvc_result spvc_compiler_install_compiler_options(spvc_compiler c,
      spvc_compiler_options o)
{ (void)c; (void)o; return SPVC_SUCCESS; }
spvc_result spvc_resources_get_resource_list_for_type(spvc_resources r,
      spvc_resource_type t, const spvc_reflected_resource **list,
      size_t *count)
{ (void)r; (void)t; *list = NULL; *count = 0; return SPVC_SUCCESS; }
spvc_result spvc_compiler_compile(spvc_compiler c, const char **src)
{ (void)c; *src = "void main() {}\n"; return SPVC_SUCCESS; }
unsigned spvc_compiler_get_decoration(spvc_compiler c, SpvId id,
      SpvDecoration d)
{ (void)c; (void)id; (void)d; return 0; }
void spvc_compiler_set_name(spvc_compiler c, SpvId id, const char *n)
{ (void)c; (void)id; (void)n; }
void spvc_compiler_unset_decoration(spvc_compiler c, SpvId id,
      SpvDecoration d)
{ (void)c; (void)id; (void)d; }
spvc_result spvc_compiler_flatten_buffer_block(spvc_compiler c,
      spvc_variable_id id)
{ (void)c; (void)id; return SPVC_SUCCESS; }

/* ---- the cases ---- */
static const uint32_t spirv[4];

static void run(const char *name, GLint vertex, GLint fragment, GLint link,
      GLint log, int expect_program)
{
   GLuint prog;
   fail_vertex   = vertex;
   fail_fragment = fragment;
   fail_link     = link;
   log_len       = log;
   live_shaders  = 0;
   live_programs = 0;
   err_lines     = 0;

   prog = gl3_cross_compile_program(spirv, sizeof(spirv),
         spirv, sizeof(spirv), NULL, true);

   if (expect_program)
   {
      CHECK(prog != 0, name);
      CHECK(live_programs == 1, "the program built is the one left");
      if (prog)
         mock_delete_program(prog);
   }
   else
   {
      CHECK(prog == 0, name);
      CHECK(live_programs == 0, "a failed build leaves no program");
      CHECK(err_lines > 0, "a failed build says so");
   }
   CHECK(live_shaders == 0, "no shader outlives the build");
   printf("      %-44s %s\n", name, prog ? "program" : "no program");
}

int main(void)
{
   mock_install();

   run("builds",                              0, 0, 0, 0, 1);
   run("vertex compile fails, no log",        1, 0, 0, 0, 0);
   run("vertex compile fails, with a log",    1, 0, 0, 8, 0);
   run("fragment compile fails, no log",      0, 1, 0, 0, 0);
   run("fragment compile fails, with a log",  0, 1, 0, 8, 0);
   run("link fails, no log",                  0, 0, 1, 0, 0);
   run("link fails, empty log (length 1)",    0, 0, 1, 1, 0);
   run("link fails, with a log",              0, 0, 1, 8, 0);

   if (failures)
   {
      printf("[fail] gl3_shader_status_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] gl3_shader_status_test\n");
   return 0;
}
