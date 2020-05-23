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

const char* rc_error_str(int ret)
{
  switch (ret) {
    case RC_OK: return "OK";
    case RC_INVALID_LUA_OPERAND: return "Invalid Lua operand";
    case RC_INVALID_MEMORY_OPERAND: return "Invalid memory operand";
    case RC_INVALID_CONST_OPERAND: return "Invalid constant operand";
    case RC_INVALID_FP_OPERAND: return "Invalid floating-point operand";
    case RC_INVALID_CONDITION_TYPE: return "Invalid condition type";
    case RC_INVALID_OPERATOR: return "Invalid operator";
    case RC_INVALID_REQUIRED_HITS: return "Invalid required hits";
    case RC_DUPLICATED_START: return "Duplicated start condition";
    case RC_DUPLICATED_CANCEL: return "Duplicated cancel condition";
    case RC_DUPLICATED_SUBMIT: return "Duplicated submit condition";
    case RC_DUPLICATED_VALUE: return "Duplicated value expression";
    case RC_DUPLICATED_PROGRESS: return "Duplicated progress expression";
    case RC_MISSING_START: return "Missing start condition";
    case RC_MISSING_CANCEL: return "Missing cancel condition";
    case RC_MISSING_SUBMIT: return "Missing submit condition";
    case RC_MISSING_VALUE: return "Missing value expression";
    case RC_INVALID_LBOARD_FIELD: return "Invalid field in leaderboard";
    case RC_MISSING_DISPLAY_STRING: return "Missing display string";
    case RC_OUT_OF_MEMORY: return "Out of memory";
    case RC_INVALID_VALUE_FLAG: return "Invalid flag in value expression";
    case RC_MISSING_VALUE_MEASURED: return "Missing measured flag in value expression";
    case RC_MULTIPLE_MEASURED: return "Multiple measured targets";
    case RC_INVALID_MEASURED_TARGET: return "Invalid measured target";
    case RC_INVALID_COMPARISON: return "Invalid comparison";
    case RC_INVALID_STATE: return "Invalid state";

    default: return "Unknown error";
  }
}
