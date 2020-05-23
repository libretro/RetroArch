#include "internal.h"

#include <stdlib.h>
#include <string.h>

void* rc_alloc(void* pointer, int* offset, int size, int alignment, rc_scratch_t* scratch) {
  void* ptr;

  *offset = (*offset + alignment - 1) & ~(alignment - 1);

  if (pointer != 0) {
    ptr = (void*)((char*)pointer + *offset);
  }
  else if (scratch != 0) {
    ptr = &scratch->obj;
  }
  else {
    ptr = 0;
  }

  *offset += size;
  return ptr;
}

char* rc_alloc_str(rc_parse_state_t* parse, const char* text, int length) {
  char* ptr;

  ptr = (char*)rc_alloc(parse->buffer, &parse->offset, length + 1, RC_ALIGNOF(char), 0);
  if (ptr) {
    memcpy(ptr, text, length);
    ptr[length] = '\0';
  }

  return ptr;
}

void rc_init_parse_state(rc_parse_state_t* parse, void* buffer, lua_State* L, int funcs_ndx)
{
  parse->offset = 0;
  parse->L = L;
  parse->funcs_ndx = funcs_ndx;
  parse->buffer = buffer;
  parse->scratch.memref = parse->scratch.memref_buffer;
  parse->scratch.memref_size = sizeof(parse->scratch.memref_buffer) / sizeof(parse->scratch.memref_buffer[0]);
  parse->scratch.memref_count = 0;
  parse->first_memref = 0;
  parse->measured_target = 0;
}

void rc_destroy_parse_state(rc_parse_state_t* parse)
{
  if (parse->scratch.memref != parse->scratch.memref_buffer)
    free(parse->scratch.memref);
}
