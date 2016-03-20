#ifndef __RARCHDB_MSGPACK_H__
#define __RARCHDB_MSGPACK_H__

#include <stdint.h>

#include <streams/file_stream.h>

struct rmsgpack_read_callbacks
{
   int (*read_nil        )(void *);
   int (*read_bool       )(int, void *);
   int (*read_int        )(int64_t, void *);
   int (*read_uint       )(uint64_t, void *);
   int (*read_string     )(char *, uint32_t, void *);
   int (*read_bin        )(void *, uint32_t, void *);
   int (*read_map_start  )(uint32_t, void *);
   int (*read_array_start)(uint32_t, void *);
};

int rmsgpack_write_array_header(RFILE *fd, uint32_t size);

int rmsgpack_write_map_header(RFILE *fd, uint32_t size);

int rmsgpack_write_string(RFILE *fd, const char *s, uint32_t len);

int rmsgpack_write_bin(RFILE *fd, const void *s, uint32_t len);

int rmsgpack_write_nil(RFILE *fd);

int rmsgpack_write_bool(RFILE *fd, int value);

int rmsgpack_write_int(RFILE *fd, int64_t value);

int rmsgpack_write_uint(RFILE *fd, uint64_t value );

int rmsgpack_read(RFILE *fd, struct rmsgpack_read_callbacks *callbacks, void *data);

#endif

