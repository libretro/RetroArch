/* Copyright (C) 2026 The RetroArch team. MIT license. */
#ifndef LIBRETRO_RWMA_STREAM_H
#define LIBRETRO_RWMA_STREAM_H
#include <formats/rwma.h>
RETRO_BEGIN_DECLS
typedef struct rwma_stream rwma_stream;
#define RWMA_STREAM_WAIT 1
#define RWMA_STREAM_END 2
/* Input address and total size stay stable until close. avail is the
 * resident prefix; zero means fully resident. No file I/O is performed. */
int rwma_stream_open(rwma_stream **out, const uint8_t *data, size_t size, size_t avail);
void rwma_stream_close(rwma_stream *stream);
void rwma_stream_set_avail(rwma_stream *stream, size_t avail);
int rwma_stream_next(rwma_stream *stream, rwma_frame *frame);
void rwma_stream_rewind(rwma_stream *stream);
void rwma_stream_info(const rwma_stream *stream, unsigned *channels, unsigned *rate);
size_t rwma_stream_tell(const rwma_stream *stream);
size_t rwma_stream_read_end(const rwma_stream *stream);
uint32_t rwma_stream_channel_mask(const rwma_stream *stream);
RETRO_END_DECLS
#endif
