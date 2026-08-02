// C wrapper implementations for content-streamer C API
// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include "content-streamer.h"
#include "content-streamer-c.h"
#include <fcntl.h>
#include <unistd.h>

extern "C" {

StreamIO* file_stream_io_create(const char* filename) {
  return reinterpret_cast<StreamIO*>(new rgb_matrix::FileStreamIO(open(filename, O_RDONLY)));
}

void file_stream_io_delete(StreamIO* io) {
  delete reinterpret_cast<rgb_matrix::StreamIO*>(io);
}

ContentStreamReaderHandle content_stream_reader_create(StreamIO* io) {
  return reinterpret_cast<ContentStreamReaderHandle>(new rgb_matrix::StreamReader(reinterpret_cast<rgb_matrix::StreamIO*>(io)));
}

void content_stream_reader_destroy(ContentStreamReaderHandle reader) {
  delete reinterpret_cast<rgb_matrix::StreamReader*>(reader);
}

int content_stream_reader_get_next(ContentStreamReaderHandle reader, struct FrameCanvas* frame, uint32_t* hold_time_us) {
  auto r = reinterpret_cast<rgb_matrix::StreamReader*>(reader);
  return r->GetNext(reinterpret_cast<rgb_matrix::FrameCanvas*>(frame), hold_time_us) ? 1 : 0;
}

void content_stream_reader_rewind(ContentStreamReaderHandle reader) {
  auto r = reinterpret_cast<rgb_matrix::StreamReader*>(reader);
  r->Rewind();
}

// C API wrapper for canvas-aware compatibility check.
int file_stream_io_is_compatible_with_canvas(StreamIO* io, struct FrameCanvas* frame) {
  return rgb_matrix::StreamIOIsCompatibleWithCanvas(reinterpret_cast<rgb_matrix::StreamIO*>(io), reinterpret_cast<rgb_matrix::FrameCanvas*>(frame)) ? 1 : 0;
}

}  // extern "C"
