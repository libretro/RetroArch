/* Symbols gfx_display.c references on paths this test never takes.
 * No RetroArch header: the signatures do not matter, the calls never
 * happen. */
#include <stddef.h>

void *config_get_ptr(void) { return NULL; }
void *data_transfer_complete(void) { return NULL; }
void *data_transfer_free(void) { return NULL; }
void *data_transfer_iterate(void) { return NULL; }
void *data_transfer_open_prefix(void) { return NULL; }
void *data_transfer_ptr(void) { return NULL; }
void *fill_pathname_join(void) { return NULL; }
void *font_driver_init_first(void) { return NULL; }
void *image_transfer_free(void) { return NULL; }
void *image_transfer_get_gpu_layout(void) { return NULL; }
void *image_transfer_is_10bit(void) { return NULL; }
void *image_transfer_is_valid(void) { return NULL; }
void *image_transfer_iterate(void) { return NULL; }
void *image_transfer_new(void) { return NULL; }
void *image_transfer_process(void) { return NULL; }
void *image_transfer_set_buffer_ptr(void) { return NULL; }
void *image_transfer_set_want_10bit(void) { return NULL; }
void *image_transfer_start(void) { return NULL; }
void *input_osk_native_active(void) { return NULL; }
void *video_context_driver_get_metrics(void) { return NULL; }
void *video_driver_get_ident(void) { return NULL; }
void *video_driver_get_ptr(void) { return NULL; }
void *video_driver_has_windowed(void) { return NULL; }
void *video_state_get_ptr(void) { return NULL; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
