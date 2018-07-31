#include "internal.h"

void* rc_alloc(void* pointer, int* offset, int size, rc_scratch_t* scratch) {
  void* ptr;

  *offset = (*offset + RC_ALIGNMENT - 1) & -RC_ALIGNMENT;

  if (pointer != 0) {
    ptr = (void*)((char*)pointer + *offset);
  }
  else {
    ptr = scratch;
  }

  *offset += size;
  return ptr;
}
