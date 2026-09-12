#ifndef GFX_VIDEO_RECORD_H
#define GFX_VIDEO_RECORD_H

#include <stdint.h>
#include <boolean.h>

/* Video thread only: BGR24, bottom up. Never redraws the cached frame. */
typedef bool (*video_record_read_t)(void *data, uint8_t *buffer);

video_record_read_t gl2_get_record_read(void);
video_record_read_t vulkan_get_record_read(void);

#endif
