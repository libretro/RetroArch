/* The frontend symbols gfx/drivers/gl2.c refers to; none of them runs
 * on the readback path. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../record/record_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_thread_wrapper.h"
#include "../../../gfx/font_driver.h"
#include "../../../gfx/drivers_shader/shader_glsl.h"
#include "../../../menu/menu_driver.h"
#include <formats/image.h>
#include <glsym/glsym.h>

void RARCH_LOG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); printf("      [log] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("      [warn] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); printf("      [err] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }

static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
retro_time_t cpu_features_get_time_usec(void) { return 0; }
uint64_t cpu_features_get(void) { return 0; }
uint32_t runloop_get_flags(void) { return 0; }
static recording_state_t stub_recording;
recording_state_t *recording_state_get_ptr(void) { return &stub_recording; }
const char *path_get_extension(const char *path) { (void)path; return ""; }

void font_driver_render_msg(void *data, const char *msg, size_t msg_len,
      const struct font_params *params, void *font_data)
{ (void)data; (void)msg; (void)msg_len; (void)params; (void)font_data; }
void font_driver_resolve_params(const struct font_params *params,
      font_params_resolved_t *out) { (void)params; (void)out; }
int font_renderer_create_default(const font_renderer_driver_t **drv,
      void **handle, const char *font_path, unsigned font_size,
      enum font_atlas_format fmt)
{ (void)drv; (void)handle; (void)font_path; (void)font_size; (void)fmt; return 0; }
int font_renderer_get_message_width(const font_renderer_driver_t *renderer,
      void *renderer_data, const char *msg, size_t msg_len, float scale)
{ (void)renderer; (void)renderer_data; (void)msg; (void)msg_len; (void)scale; return 0; }

void image_texture_free(struct texture_image *img) { (void)img; }
bool image_texture_load(struct texture_image *img, const char *path)
{ (void)img; (void)path; return false; }
void image_yuv_coefficients(unsigned flags, float c[6]) { (void)flags; memset(c, 0, 6 * sizeof(float)); }

void menu_driver_frame(bool menu_is_alive, video_frame_info_t *video_info)
{ (void)menu_is_alive; (void)video_info; }

void rglgen_resolve_symbols(rglgen_proc_address_t proc) { (void)proc; }

void video_context_driver_free(void) { }
bool video_context_driver_get_flags(gfx_ctx_flags_t *flags) { (void)flags; return false; }
const gfx_ctx_driver_t *video_context_driver_init_first(void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major, unsigned minor,
      bool hw_render_ctx, void **ctx_data)
{ (void)data; (void)ident; (void)api; (void)major; (void)minor; (void)hw_render_ctx; (void)ctx_data; return NULL; }
bool video_context_driver_set(const gfx_ctx_driver_t *data) { (void)data; return false; }
bool video_context_driver_set_flags(gfx_ctx_flags_t *flags) { (void)flags; return false; }
bool video_coord_array_append(video_coord_array_t *ca,
      const video_coords_t *coords, unsigned count)
{ (void)ca; (void)coords; (void)count; return false; }
void video_driver_attach_windows(unsigned screens, unsigned restore)
{ (void)screens; (void)restore; }
void video_driver_cached_frame(void) { }
void video_driver_cached_frame_retire(void) { }
uint32_t video_driver_get_disp_flags(void) { return 0; }
struct retro_hw_render_callback *video_driver_get_hw_context(void)
{ static struct retro_hw_render_callback cb; return &cb; }
unsigned video_driver_get_output_dims(void) { return 0; }
void video_driver_modify_disp_flags(uint32_t set_bits, uint32_t clear_bits) { (void)set_bits; (void)clear_bits; }
void video_driver_publish_window_layout(unsigned window,
      unsigned generation, const video_views_layout_t *layout,
      const video_views_map_t *core_map, unsigned core_dims,
      unsigned window_dims)
{ (void)window; (void)generation; (void)layout; (void)core_map; (void)core_dims; (void)window_dims; }
void video_driver_set_output_dims(unsigned dims) { (void)dims; }
bool video_driver_test_all_flags(enum display_flags testflag) { (void)testflag; return false; }
bool video_driver_thread_wrapper_active(void) { return false; }
void video_driver_update_viewport(struct video_viewport *vp, bool force_full,
      bool keep_aspect, bool y_down)
{ (void)vp; (void)force_full; (void)keep_aspect; (void)y_down; }
unsigned video_driver_window_generation(void) { return 0; }
void video_driver_window_layout_screens(const video_windows_frame_t *frame,
      unsigned screens, unsigned dims, video_views_layout_t *out)
{ (void)frame; (void)screens; (void)dims; (void)out; }
const char *video_shader_get_current_shader_preset(void) { return NULL; }
enum rarch_shader_type video_shader_get_type_from_ext(const char *ext, bool *is_preset)
{ (void)ext; (void)is_preset; return RARCH_SHADER_NONE; }
const char *video_shader_type_to_str(enum rarch_shader_type type) { (void)type; return ""; }
void video_thread_call_on_waiter(void (*fn)(void *data), void *data) { fn(data); }
bool video_thread_hw_allowed(void) { return false; }
uint64_t video_thread_swap_count(void) { return 0; }
uintptr_t video_thread_texture_handle(void *data, custom_command_method_t func)
{ (void)data; (void)func; return 0; }
void video_windows_steps(video_windows_held_t *held,
      const video_windows_frame_t *frame, unsigned *retire,
      unsigned *create)
{ (void)held; (void)frame; *retire = 0; *create = 0; }

const shader_backend_t gl_glsl_backend = { 0 };
bool gl_glsl_draw_mesh(void *shader_data, unsigned vbo, unsigned ibo,
      unsigned count, bool strip, const float *mvp, const float *tint)
{ (void)shader_data; (void)vbo; (void)ibo; (void)count; (void)strip; (void)mvp; (void)tint; return false; }
void gl_glsl_set_context_type(bool core_profile, unsigned major, unsigned minor)
{ (void)core_profile; (void)major; (void)minor; }
size_t video_driver_set_gpu_api_version_string(const char *str) { (void)str; return 0; }
