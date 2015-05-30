#ifndef __RARCHDB_MSGPACK_H__
#define __RARCHDB_MSGPACK_H__

#include <stdio.h>
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
        FILE *fp,
        uint32_t size
);
int rmsgpack_write_map_header(
        FILE *fp,
        uint32_t size
);
int rmsgpack_write_string(
        FILE *fp,
        const char * s,
        uint32_t len
);
int rmsgpack_write_bin(
        FILE *fp,
        const void * s,
        uint32_t len
);
int rmsgpack_write_nil(FILE *fp);
int rmsgpack_write_bool(
        FILE *fp,
        int value
);
int rmsgpack_write_int(
        FILE *fp,
        int64_t value
);
int rmsgpack_write_uint(
        FILE *fp,
        uint64_t value
);

int rmsgpack_read(
        FILE *fp,
        struct rmsgpack_read_callbacks * callbacks,
        void * data
);

#endif

