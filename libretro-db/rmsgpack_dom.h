#ifndef __RARCHDB_MSGPACK_DOM_H__
#define __RARCHDB_MSGPACK_DOM_H__

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum rmsgpack_dom_type {
	RDT_NULL = 0,
	RDT_BOOL,
	RDT_UINT,
	RDT_INT,
	RDT_STRING,
	RDT_BINARY,
	RDT_MAP,
	RDT_ARRAY
};

struct rmsgpack_dom_value {
	enum rmsgpack_dom_type type;
	union {
		uint64_t uint_;
		int64_t int_;
		struct
      {
			uint32_t len;
			char * buff;
		} string;

		struct
      {
			uint32_t len;
			char * buff;
		} binary;

		int bool_;

		struct
      {
			uint32_t len;
			struct rmsgpack_dom_pair * items;
		} map;

		struct
      {
			uint32_t len;
			struct rmsgpack_dom_value * items;
		} array;
	} val;
};

struct rmsgpack_dom_pair {
	struct rmsgpack_dom_value key;
	struct rmsgpack_dom_value value;
};

void rmsgpack_dom_value_print(struct rmsgpack_dom_value * obj);
void rmsgpack_dom_value_free(struct rmsgpack_dom_value * v);
int rmsgpack_dom_value_cmp(
        const struct rmsgpack_dom_value * a,
        const struct rmsgpack_dom_value * b
);

struct rmsgpack_dom_value * rmsgpack_dom_value_map_value(
        const struct rmsgpack_dom_value * map,
        const struct rmsgpack_dom_value * key
);

int rmsgpack_dom_read(
        FILE *fp,
        struct rmsgpack_dom_value * out
);
int rmsgpack_dom_write(
        FILE *fp,
        const struct rmsgpack_dom_value * obj
);

int rmsgpack_dom_read_into(FILE *fp, ...);

#ifdef __cplusplus
}
#endif

#endif
