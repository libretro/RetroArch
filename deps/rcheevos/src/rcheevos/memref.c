#include "internal.h"

#include <stdlib.h> /* malloc/realloc */
#include <string.h> /* memcpy */

rc_memref_value_t* rc_alloc_memref_value(rc_parse_state_t* parse, unsigned address, char size, char is_bcd) {
  rc_memref_value_t** next_memref_value;
  rc_memref_value_t* memref_value;
  rc_memref_t* memref;
  int i;

  if (!parse->first_memref) {
    /* sizing mode - have to track unique address/size/bcd combinations */
    for (i = 0; i < parse->scratch.memref_count; ++i) {
      memref = &parse->scratch.memref[i];
      if (memref->address == address && memref->size == size && memref->is_bcd == is_bcd) {
        return &parse->scratch.obj.memref_value;
      }
    }

    /* resize unique tracking buffer if necessary */
    if (parse->scratch.memref_count == parse->scratch.memref_size) {
      if (parse->scratch.memref == parse->scratch.memref_buffer) {
        parse->scratch.memref_size += 16;
        memref = (rc_memref_t*)malloc(parse->scratch.memref_size * sizeof(parse->scratch.memref_buffer[0]));
        if (memref) {
          parse->scratch.memref = memref;
          memcpy(memref, parse->scratch.memref_buffer, parse->scratch.memref_count * sizeof(parse->scratch.memref_buffer[0]));
        }
        else {
          parse->offset = RC_OUT_OF_MEMORY;
          return 0;
        }
      } 
      else {
        parse->scratch.memref_size += 32;
        memref = (rc_memref_t*)realloc(parse->scratch.memref, parse->scratch.memref_size * sizeof(parse->scratch.memref_buffer[0]));
        if (memref) {
          parse->scratch.memref = memref;
        }
        else {
          parse->offset = RC_OUT_OF_MEMORY;
          return 0;
        }
      }
    }

    /* add new unique tracking entry */
    if (parse->scratch.memref) {
      memref = &parse->scratch.memref[parse->scratch.memref_count++];
      memref->address = address;
      memref->size = size;
      memref->is_bcd = is_bcd;
    }

    /* allocate memory but don't actually populate, as it might overwrite the self object referencing the rc_memref_value_t */
    return RC_ALLOC(rc_memref_value_t, parse);
  } 
  
  /* construction mode - find or create the appropriate rc_memref_value_t */
  next_memref_value = parse->first_memref;
  while (*next_memref_value) {
    memref_value = *next_memref_value;
    if (memref_value->memref.address == address && memref_value->memref.size == size && memref_value->memref.is_bcd == is_bcd) {
      return memref_value;
    }

    next_memref_value = &memref_value->next;
  }

  memref_value = RC_ALLOC(rc_memref_value_t, parse);
  memref_value->memref.address = address;
  memref_value->memref.size = size;
  memref_value->memref.is_bcd = is_bcd;
  memref_value->value = 0;
  memref_value->previous = 0;
  memref_value->prior = 0;
  memref_value->next = 0;

  *next_memref_value = memref_value;

  return memref_value;
}

static unsigned rc_memref_get_value(rc_memref_t* self, rc_peek_t peek, void* ud) {
  unsigned value;

  switch (self->size)
  {
    case RC_MEMSIZE_BIT_0:
      value = (peek(self->address, 1, ud) >> 0) & 1;
      break;

    case RC_MEMSIZE_BIT_1:
      value = (peek(self->address, 1, ud) >> 1) & 1;
      break;

    case RC_MEMSIZE_BIT_2:
      value = (peek(self->address, 1, ud) >> 2) & 1;
      break;

    case RC_MEMSIZE_BIT_3:
      value = (peek(self->address, 1, ud) >> 3) & 1;
      break;

    case RC_MEMSIZE_BIT_4:
      value = (peek(self->address, 1, ud) >> 4) & 1;
      break;

    case RC_MEMSIZE_BIT_5:
      value = (peek(self->address, 1, ud) >> 5) & 1;
      break;

    case RC_MEMSIZE_BIT_6:
      value = (peek(self->address, 1, ud) >> 6) & 1;
      break;

    case RC_MEMSIZE_BIT_7:
      value = (peek(self->address, 1, ud) >> 7) & 1;
      break;

    case RC_MEMSIZE_LOW:
      value = peek(self->address, 1, ud) & 0x0f;
      break;

    case RC_MEMSIZE_HIGH:
      value = (peek(self->address, 1, ud) >> 4) & 0x0f;
      break;

    case RC_MEMSIZE_8_BITS:
      value = peek(self->address, 1, ud);

      if (self->is_bcd) {
         value = ((value >> 4) & 0x0f) * 10 + (value & 0x0f);
      }

      break;

    case RC_MEMSIZE_16_BITS:
      value = peek(self->address, 2, ud);

      if (self->is_bcd) {
         value = ((value >> 12) & 0x0f) * 1000
               + ((value >> 8) & 0x0f) * 100
               + ((value >> 4) & 0x0f) * 10
               + ((value >> 0) & 0x0f) * 1;
      }

      break;

    case RC_MEMSIZE_24_BITS:
      value = peek(self->address, 4, ud);

      if (self->is_bcd) {
        value = ((value >> 20) & 0x0f) * 100000
              + ((value >> 16) & 0x0f) * 10000
              + ((value >> 12) & 0x0f) * 1000
              + ((value >> 8) & 0x0f) * 100
              + ((value >> 4) & 0x0f) * 10
              + ((value >> 0) & 0x0f) * 1;
      }

      break;

    case RC_MEMSIZE_32_BITS:
      value = peek(self->address, 4, ud);

      if (self->is_bcd) {
        value = ((value >> 28) & 0x0f) * 10000000
              + ((value >> 24) & 0x0f) * 1000000
              + ((value >> 20) & 0x0f) * 100000
              + ((value >> 16) & 0x0f) * 10000
              + ((value >> 12) & 0x0f) * 1000
              + ((value >> 8) & 0x0f) * 100
              + ((value >> 4) & 0x0f) * 10
              + ((value >> 0) & 0x0f) * 1;
      }

      break;

    default:
      value = 0;
      break;
  }

  return value;
}

void rc_update_memref_values(rc_memref_value_t* memref, rc_peek_t peek, void* ud) {
  while (memref) {
    memref->previous = memref->value;
    memref->value = rc_memref_get_value(&memref->memref, peek, ud);
    if (memref->value != memref->previous)
      memref->prior = memref->previous;

    memref = memref->next;
  }
}

void rc_init_parse_state_memrefs(rc_parse_state_t* parse, rc_memref_value_t** memrefs)
{
  parse->first_memref = memrefs;
  *memrefs = 0;
}
