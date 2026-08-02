/* C API for content-streamer (C wrapper for content-streamer.h) */
#ifndef CONTENT_STREAMER_C_H
#define CONTENT_STREAMER_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque type for StreamReader */
typedef void* ContentStreamReaderHandle;

/* Forward declarations */
struct FrameCanvas;
struct StreamIO;

/* Stream reader control */
ContentStreamReaderHandle content_stream_reader_create(struct StreamIO* io);
void content_stream_reader_destroy(ContentStreamReaderHandle reader);
int content_stream_reader_get_next(ContentStreamReaderHandle reader, struct FrameCanvas* frame, uint32_t* hold_time_us);
void content_stream_reader_rewind(ContentStreamReaderHandle reader);

/* FileStreamIO management */
struct StreamIO* file_stream_io_create(const char* filename);
void file_stream_io_delete(struct StreamIO* io);

int file_stream_io_is_compatible_with_canvas(struct StreamIO* io, struct FrameCanvas* frame);

#ifdef __cplusplus
}
#endif

#endif
