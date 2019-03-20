#include "internal.h"

#include <memory.h>

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

char* rc_alloc_str(void* pointer, int* offset, const char* text, int length) {
  char* ptr;

  ptr = (char*)rc_alloc(pointer, offset, length + 1, RC_ALIGNOF(char), 0);
  if (ptr) {
    memcpy(ptr, text, length);
    ptr[length] = '\0';
  }

  return ptr;
}
