/* What gfx/drivers_shader/shader_vulkan.c links against beyond the
 * chain creation under test: none of it is reached there, so each is an empty stand-in, declared without
 * the headers as only the names have to resolve. */

#include <stddef.h>
#include <stdint.h>

void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

const char *msg_hash_to_str(unsigned id) { (void)id; return ""; }
void input_driver_set_shader_uses_sensors(int on) { (void)on; }
int  input_driver_read_sensor_snapshot(void *s) { (void)s; return 0; }
int  video_shader_load_preset_into_shader(const char *p, void *s)
{ (void)p; (void)s; return 0; }
uint32_t video_driver_get_disp_flags(void) { return 0; }
int  image_texture_load(void *t, const char *p) { (void)t; (void)p; return 0; }
void image_texture_free(void *t) { (void)t; }

int  glslang_compile_shader(void) { return 0; }
int  glslang_compile_shader_cached(void) { return 0; }
const char *glslang_format_to_string(int f) { (void)f; return ""; }
void *glslang_include_cache_new(void) { return NULL; }
void glslang_include_cache_free(void *c) { (void)c; }
unsigned glslang_num_miplevels(unsigned w, unsigned h) { (void)w; (void)h; return 1; }
void glslang_output_free(void *o) { (void)o; }
void glslang_output_init(void *o) { (void)o; }
int  slang_reflect_spirv(void) { return 0; }
void slang_reflection_free(void *r) { (void)r; }
void slang_reflection_init(void *r) { (void)r; }
void slang_semantic_name_map_free(void *m) { (void)m; }
int  slang_semantic_name_map_set_unique(void) { return 0; }
void slang_texture_semantic_name_map_free(void *m) { (void)m; }
int  slang_texture_semantic_name_map_set_unique(void) { return 0; }

void vulkan_debug_mark_image(void) { }
void vulkan_debug_mark_memory(void) { }
int  vulkan_is_hdr10_format(int f) { (void)f; return 0; }
