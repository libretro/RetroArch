#include "internal.h"

rc_term_t* rc_parse_term(int* ret, void* buffer, rc_scratch_t* scratch, const char** memaddr, lua_State* L, int funcs_ndx) {
  rc_term_t* self;
  const char* aux;
  int ret2;

  aux = *memaddr;
  self = RC_ALLOC(rc_term_t, buffer, ret, scratch);
  self->invert = 0;

  ret2 = rc_parse_operand(&self->operand1, &aux, 0, L, funcs_ndx);

  if (ret2 < 0) {
    *ret = ret2;
    return 0;
  }

  if (*aux == '*') {
    aux++;

    if (*aux == '~') {
      aux++;
      self->invert = 1;
    }

    ret2 = rc_parse_operand(&self->operand2, &aux, 0, L, funcs_ndx);

    if (ret2 < 0) {
      *ret = ret2;
      return 0;
    }

    if (self->invert) {
      switch (self->operand2.size) {
        case RC_OPERAND_BIT_0:
        case RC_OPERAND_BIT_1:
        case RC_OPERAND_BIT_2:
        case RC_OPERAND_BIT_3:
        case RC_OPERAND_BIT_4:
        case RC_OPERAND_BIT_5:
        case RC_OPERAND_BIT_6:
        case RC_OPERAND_BIT_7:
          /* invert is already 1 */
          break;

        case RC_OPERAND_LOW:
        case RC_OPERAND_HIGH:
          self->invert = 0xf;
          break;
        
        case RC_OPERAND_8_BITS:
          self->invert = 0xffU;
          break;

        case RC_OPERAND_16_BITS:
          self->invert = 0xffffU;
          break;

        case RC_OPERAND_24_BITS:
          self->invert = 0xffffffU;
          break;

        case RC_OPERAND_32_BITS:
          self->invert = 0xffffffffU;
          break;
      }
    }
  }
  else {
    self->operand2.type = RC_OPERAND_FP;
    self->operand2.size = RC_OPERAND_8_BITS;
    self->operand2.fp_value = 1.0;
  }

  *memaddr = aux;
  return self;
}

unsigned rc_evaluate_term(rc_term_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  unsigned value = rc_evaluate_operand(&self->operand1, peek, ud, L);

  if (self->operand2.type != RC_OPERAND_FP) {
    return value * (rc_evaluate_operand(&self->operand2, peek, ud, L) ^ self->invert);
  }

  return (unsigned)(value * self->operand2.fp_value);
}
