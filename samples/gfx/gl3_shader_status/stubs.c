/* What gfx/drivers_shader/shader_gl3.c links against beyond the code
 * under test: none of it is reached by gl3_cross_compile_program, so
 * each is an empty stand-in. Declared here without the headers, as
 * only their names have to resolve. */

#include <stddef.h>
#include <stdint.h>

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }

const char *msg_hash_to_str(unsigned id) { (void)id; return ""; }
void input_driver_set_shader_uses_sensors(int on) { (void)on; }
int  input_driver_read_sensor_snapshot(void *s) { (void)s; return 0; }
int  video_shader_load_preset_into_shader(const char *p, void *s)
{ (void)p; (void)s; return 0; }
void gl3_framebuffer_copy(void) { }
void gl3_framebuffer_copy_partial(void) { }
uint32_t gl3_get_cross_compiler_target_version(void) { return 430; }
int  gl3_spirv_binary_supported(void) { return 0; }
void spirv_opengl_lower(void) { }
int  image_texture_load(void *t, const char *p) { (void)t; (void)p; return 0; }
void image_texture_free(void *t) { (void)t; }

int  glslang_compile_shader(void) { return 0; }
int  glslang_compile_shader_cached(void) { return 0; }
const char *glslang_format_to_string(int f) { (void)f; return ""; }
void *glslang_include_cache_new(void) { return NULL; }
void glslang_include_cache_free(void *c) { (void)c; }
unsigned glslang_num_miplevels(unsigned w, unsigned h) { (void)w; (void)h; return 1; }
void glslang_output_free(void *o) { (void)o; }
int  slang_reflect_spirv(void) { return 0; }
void slang_reflection_free(void *r) { (void)r; }
void slang_reflection_init(void *r) { (void)r; }
void slang_semantic_name_map_free(void *m) { (void)m; }
int  slang_semantic_name_map_set_unique(void) { return 0; }
void slang_texture_semantic_name_map_free(void *m) { (void)m; }
int  slang_texture_semantic_name_map_set_unique(void) { return 0; }

#ifdef STUB_GL11
void glBindTexture(void) { }
void glClear(void) { }
void glClearColor(void) { }
void glColorMask(void) { }
void glDeleteTextures(void) { }
void glDisable(void) { }
void glDrawArrays(void) { }
void glEnable(void) { }
void glGenTextures(void) { }
unsigned glGetError(void) { return 0; }
void glPixelStorei(void) { }
void glScissor(void) { }
void glTexParameteri(void) { }
void glTexSubImage2D(void) { }
void glViewport(void) { }
#endif
