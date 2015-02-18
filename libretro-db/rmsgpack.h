#ifndef __RARCHDB_MSGPACK_H__
#define __RARCHDB_MSGPACK_H__

#include <stdint.h>

struct rmsgpack_read_callbacks {
	int (* read_nil)(void *);
	int (* read_bool)(
	        int,
	        void *
	);
	int (* read_int)(
	        int64_t,
	        void *
	);
	int (* read_uint)(
	        uint64_t,
	        void *
	);
	int (* read_string)(
	        char *,
	        uint32_t,
	        void *
	);
	int (* read_bin)(
	        void *,
	        uint32_t,
	        void *
	);
	int (* read_map_start)(
	        uint32_t,
	        void *
	);
	int (* read_array_start)(
	        uint32_t,
	        void *
	);
};


int rmsgpack_write_array_header(
        int fd,
        uint32_t size
);
int rmsgpack_write_map_header(
        int fd,
        uint32_t size
);
int rmsgpack_write_string(
        int fd,
        const char * s,
        uint32_t len
);
int rmsgpack_write_bin(
        int fd,
        const void * s,
        uint32_t len
);
int rmsgpack_write_nil(int fd);
int rmsgpack_write_bool(
        int fd,
        int value
);
int rmsgpack_write_int(
        int fd,
        int64_t value
);
int rmsgpack_write_uint(
        int fd,
        uint64_t value
);

int rmsgpack_read(
        int fd,
        struct rmsgpack_read_callbacks * callbacks,
        void * data
);

#endif

