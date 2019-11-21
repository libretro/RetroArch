#include "internal.h"

#include <stdlib.h> /* malloc/realloc */
#include <string.h> /* memcpy */

#define MEMREF_PLACEHOLDER_ADDRESS 0xFFFFFFFF

static rc_memref_value_t* rc_alloc_memref_value_sizing_mode(rc_parse_state_t* parse, unsigned address, char size, char is_bcd, char is_indirect) {
  rc_memref_t* memref;
  int i;

  /* indirect address always creates two new entries; don't bother tracking them */
  if (is_indirect) {
    RC_ALLOC(rc_memref_value_t, parse);
    return RC_ALLOC(rc_memref_value_t, parse);
  }

  memref = NULL;

  /* have to track unique address/size/bcd combinations - use scratch.memref for sizing mode */
  for (i = 0; i < parse->scratch.memref_count; ++i) {
    memref = &parse->scratch.memref[i];
    if (memref->address == address && memref->size == size && memref->is_bcd == is_bcd) {
      return &parse->scratch.obj.memref_value;
    }
  }

  /* no match found - resize unique tracking buffer if necessary */
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
    memref->is_indirect = is_indirect;
  }
  
  /* allocate memory but don't actually populate, as it might overwrite the self object referencing the rc_memref_value_t */
  return RC_ALLOC(rc_memref_value_t, parse);
}

static rc_memref_value_t* rc_alloc_memref_value_constuct_mode(rc_parse_state_t* parse, unsigned address, char size, char is_bcd, char is_indirect) {
  rc_memref_value_t** next_memref_value;
  rc_memref_value_t* memref_value;
  rc_memref_value_t* indirect_memref_value;
  
  if (!is_indirect) {
    /* attempt to find an existing rc_memref_value_t */
    next_memref_value = parse->first_memref;
    while (*next_memref_value) {
      memref_value = *next_memref_value;
      if (!memref_value->memref.is_indirect && memref_value->memref.address == address && 
          memref_value->memref.size == size && memref_value->memref.is_bcd == is_bcd) {
        return memref_value;
      }

      next_memref_value = &memref_value->next;
    }
  }
  else {
    /* indirect address always creates two new entries - one for the original address, and one for 
       the indirect dereference - just skip ahead to the end of the list */
    next_memref_value = parse->first_memref;
    while (*next_memref_value) {
      next_memref_value = &(*next_memref_value)->next;
    }
  }

  /* no match found, create a new entry */
  memref_value = RC_ALLOC(rc_memref_value_t, parse);
  memref_value->memref.address = address;
  memref_value->memref.size = size;
  memref_value->memref.is_bcd = is_bcd;
  memref_value->memref.is_indirect = is_indirect;
  memref_value->value = 0;
  memref_value->previous = 0;
  memref_value->prior = 0;
  memref_value->next = 0;

  *next_memref_value = memref_value;

  /* also create the indirect deference entry for indirect references */
  if (is_indirect) {
    indirect_memref_value = RC_ALLOC(rc_memref_value_t, parse);
    indirect_memref_value->memref.address = MEMREF_PLACEHOLDER_ADDRESS;
    indirect_memref_value->memref.size = size;
    indirect_memref_value->memref.is_bcd = is_bcd;
    indirect_memref_value->memref.is_indirect = 1;
    indirect_memref_value->value = 0;
    indirect_memref_value->previous = 0;
    indirect_memref_value->prior = 0;
    indirect_memref_value->next = 0;

    memref_value->next = indirect_memref_value;
  }

  return memref_value;
}

rc_memref_value_t* rc_alloc_memref_value(rc_parse_state_t* parse, unsigned address, char size, char is_bcd, char is_indirect) {
  if (!parse->first_memref)
    return rc_alloc_memref_value_sizing_mode(parse, address, size, is_bcd, is_indirect);

  return rc_alloc_memref_value_constuct_mode(parse, address, size, is_bcd, is_indirect);
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
      /* peek 4 bytes - don't expect the caller to understand 24-bit numbers */
      value = peek(self->address, 4, ud);

      if (self->is_bcd) {
        value = ((value >> 20) & 0x0f) * 100000
              + ((value >> 16) & 0x0f) * 10000
              + ((value >> 12) & 0x0f) * 1000
              + ((value >> 8) & 0x0f) * 100
              + ((value >> 4) & 0x0f) * 10
              + ((value >> 0) & 0x0f) * 1;
      } else {
        value &= 0x00FFFFFF;
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

void rc_update_memref_value(rc_memref_value_t* memref, rc_peek_t peek, void* ud) {
  memref->previous = memref->value;
  memref->value = rc_memref_get_value(&memref->memref, peek, ud);
  if (memref->value != memref->previous)
    memref->prior = memref->previous;
}

void rc_update_memref_values(rc_memref_value_t* memref, rc_peek_t peek, void* ud) {
  while (memref) {
    if (memref->memref.address != MEMREF_PLACEHOLDER_ADDRESS)
      rc_update_memref_value(memref, peek, ud);
    memref = memref->next;
  }
}

void rc_init_parse_state_memrefs(rc_parse_state_t* parse, rc_memref_value_t** memrefs) {
  parse->first_memref = memrefs;
  *memrefs = 0;
}

rc_memref_value_t* rc_get_indirect_memref(rc_memref_value_t* memref, rc_eval_state_t* eval_state) {
  unsigned new_address;

  if (eval_state->add_address == 0)
    return memref;

  if (!memref->memref.is_indirect)
    return memref;

  new_address = memref->memref.address + eval_state->add_address;

  /* an extra rc_memref_value_t is allocated for offset calculations */
  memref = memref->next;

  /* if the adjusted address has changed, update the record */
  if (memref->memref.address != new_address) {
    memref->memref.address = new_address;
    rc_update_memref_value(memref, eval_state->peek, eval_state->peek_userdata);
  }

  return memref;
}
