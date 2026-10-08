/* SPIRV-Cross, as shader_gl3.c calls it, stood in for: a source per
 * stage and no resources. Shared by the tests in this directory. */

#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <spirv_cross_c.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

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
