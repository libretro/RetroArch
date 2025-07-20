#ifndef __UINT32S_INDEX__H
#define __UINT32S_INDEX__H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <boolean.h>
#include <retro_common_api.h>

struct uint32s_bucket
{
   uint32_t len; /* if < 4, contents is idxs. */
   union {
     uint32_t idxs[3];
     struct {
       uint32_t cap;
       uint32_t *idxs;
     } vec;
   } contents;
};
struct uint32s_frame_addition {
   uint64_t frame_counter;
   uint32_t first_index; /* lowest index added on this frame */
};
struct uint32s_index
{
   size_t object_size; /* measured in ints */
   struct uint32s_bucket *index; /* an rhmap of buckets for value->index lookup */
   uint32_t **objects;   /* an rbuf of the actual buffers */
   uint32_t *counts;   /* an rbuf of the times each object was used */
   struct uint32s_frame_addition *additions; /* an rbuf of addition info */
};
typedef struct uint32s_index uint32s_index_t;

struct uint32s_insert_result
{
   uint32_t index;
   bool is_new;
};
typedef struct uint32s_insert_result uint32s_insert_result_t;

RETRO_BEGIN_DECLS

uint32s_index_t *uint32s_index_new(size_t object_size);
/* Does not take ownership of object */
uint32s_insert_result_t uint32s_index_insert(uint32s_index_t *index, uint32_t *object, uint64_t frame);
/* Does take ownership, requires idx is the exact next index and object not in index */
bool uint32s_index_insert_exact(uint32s_index_t *index, uint32_t idx, uint32_t *object, uint64_t frame);
/* Does not grant ownership of return value */
uint32_t *uint32s_index_get(uint32s_index_t *index, uint32_t which);
void uint32s_index_free(uint32s_index_t *index);

/* goes backwards from end of additions */
void uint32s_index_remove_after(uint32s_index_t *index, uint64_t frame);
/* removes all data from index */
void uint32s_index_clear(uint32s_index_t *index);
uint32_t uint32s_index_count(uint32s_index_t *index);
#if DEBUG
void uint32s_index_print_count_data(uint32s_index_t *index);
#endif
RETRO_END_DECLS

#endif /* __UINT32S_INDEX__H */
