#include "internal.h"

void* rc_alloc(void* pointer, int* offset, int size, int alignment, rc_scratch_t* scratch) {
  void* ptr;

  *offset = (*offset + alignment - 1) & ~(alignment - 1);

  if (pointer != 0) {
    ptr = (void*)((char*)pointer + *offset);
  }
  else {
    ptr = scratch;
  }

  *offset += size;
  return ptr;
}
