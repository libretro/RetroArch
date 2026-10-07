/* Symbols image_texture.c references on its file decode, which this
 * test never takes: the stub task queue hands images over whole. No
 * RetroArch header: the signatures do not matter, the calls never
 * happen. */
#include <stddef.h>

void *data_transfer_complete(void) { return NULL; }
void *data_transfer_free(void) { return NULL; }
void *data_transfer_iterate(void) { return NULL; }
void *data_transfer_open_prefix(void) { return NULL; }
void *data_transfer_ptr(void) { return NULL; }
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
