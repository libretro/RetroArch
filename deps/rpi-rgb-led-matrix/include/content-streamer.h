// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
//
// Abstractions to read and write FrameCanvas objects to streams. This allows
// you to create canned streams of content with minimal overhead at runtime
// to play with extreme pixel-throughput which also minimizes overheads in
// the Pi to avoid stuttering or brightness glitches.
//
// The disadvantage is, that this represents the full expanded internal
// representation of a frame, so is very large memory wise.
//
// These abstractions are used in util/led-image-viewer.cc to read and
// write such animations to disk. It is also used in util/video-viewer.cc
// to write a version to disk that then can be played with the led-image-viewer.

#ifndef RPI_CONTENT_STREAMER_H
#define RPI_CONTENT_STREAMER_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <string>

namespace rgb_matrix {
class FrameCanvas;

// An abstraction of a data stream. Two implementations exist for files and
// an in-memory representation, but this allows your own implementation, e.g.
// reading from a socket.
class StreamIO {
public:
  virtual ~StreamIO() {}

  // Rewind stream.
  virtual void Rewind() = 0;

  // Read bytes into buffer at current position of stream.
  // Similar to Posix behavior that allows short reads.
  virtual ssize_t Read(void *buf, size_t count) = 0;

  // Write bytes from buffer. Similar to Posix behavior that allows short
  // writes.
  virtual ssize_t Append(const void *buf, size_t count) = 0;
};

class FileStreamIO : public StreamIO {
public:
  explicit FileStreamIO(int fd);
  ~FileStreamIO();

  void Rewind() final;
  ssize_t Read(void *buf, size_t count) final;
  ssize_t Append(const void *buf, size_t count) final;

private:
  const int fd_;
};

// Storing a stream in memory. Owns the memory.
class MemStreamIO : public StreamIO {
public:
  void Rewind() final;
  ssize_t Read(void *buf, size_t count) final;
  ssize_t Append(const void *buf, size_t count) final;

private:
  std::string buffer_;  // super simplistic.
  size_t pos_;
};

// Just a view around the memory, possibly a memory mapped file.
class MemMapViewInput : public StreamIO {
public:
  MemMapViewInput(int fd);
  ~MemMapViewInput();

  // Since mmmap() might fail, this tells us if it was successful.
  bool IsInitialized() const { return buffer_ != nullptr; }

  void Rewind() final;
  ssize_t Read(void *buf, size_t count) final;

  // No append, this is purely read-only.
  ssize_t Append(const void *buf, size_t count) final { return -1; }

private:
  char *buffer_;
  char *end_;
  char *pos_;
};

class StreamWriter {
public:
  // Does not take ownership of StreamIO
  StreamWriter(StreamIO *io);

  // Stream out given canvas at the given time. "hold_time_us" indicates
  // for how long this frame is to be shown in microseconds.
  bool Stream(const FrameCanvas &frame, uint32_t hold_time_us);

private:
  void WriteFileHeader(const FrameCanvas &frame, size_t len);

  StreamIO *const io_;
  bool header_written_;
};

class StreamReader {
public:
  // Does not take ownership of StreamIO
  StreamReader(StreamIO *io);
  ~StreamReader();

  // Go back to the beginning.
  void Rewind();

  // Get next frame and its timestamp. Returns 'false' if there is an error
  // or end of stream reached..
  bool GetNext(FrameCanvas *frame, uint32_t* hold_time_us);

private:
  enum State {
    STREAM_AT_BEGIN,
    STREAM_READING,
    STREAM_ERROR,
  };
  bool ReadFileHeader(const FrameCanvas &frame);

  StreamIO *io_;
  size_t frame_buf_size_;
  State state_;

  char *header_frame_buffer_;
};
// Helpers for external C bridge wrappers.
bool StreamIOIsCompatibleWithCanvas(StreamIO* io, FrameCanvas* frame);
}  // namespace rgb_matrix
#endif