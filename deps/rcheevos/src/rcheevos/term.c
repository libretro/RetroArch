#include "internal.h"

rc_term_t* rc_parse_term(const char** memaddr, int is_indirect, rc_parse_state_t* parse) {
  rc_term_t* self;
  const char* aux;
  char size;
  int ret2;

  aux = *memaddr;
  self = RC_ALLOC(rc_term_t, parse);
  self->invert = 0;

  ret2 = rc_parse_operand(&self->operand1, &aux, 0, is_indirect, parse);

  if (ret2 < 0) {
    parse->offset = ret2;
    return 0;
  }

  if (*aux == '*') {
    aux++;

    if (*aux == '~') {
      aux++;
      self->invert = 1;
    }

    ret2 = rc_parse_operand(&self->operand2, &aux, 0, is_indirect, parse);

    if (ret2 < 0) {
      parse->offset = ret2;
      return 0;
    }

    if (self->invert) {
      switch (self->operand2.type) {
        case RC_OPERAND_ADDRESS:
        case RC_OPERAND_DELTA:
        case RC_OPERAND_PRIOR:
          size = self->operand2.value.memref->memref.size;
          break;
        default:
          size = RC_MEMSIZE_32_BITS;
          break;
      }

      switch (size) {
        case RC_MEMSIZE_BIT_0:
        case RC_MEMSIZE_BIT_1:
        case RC_MEMSIZE_BIT_2:
        case RC_MEMSIZE_BIT_3:
        case RC_MEMSIZE_BIT_4:
        case RC_MEMSIZE_BIT_5:
        case RC_MEMSIZE_BIT_6:
        case RC_MEMSIZE_BIT_7:
          /* invert is already 1 */
          break;

        case RC_MEMSIZE_LOW:
        case RC_MEMSIZE_HIGH:
          self->invert = 0xf;
          break;
        
        case RC_MEMSIZE_8_BITS:
          self->invert = 0xffU;
          break;

        case RC_MEMSIZE_16_BITS:
          self->invert = 0xffffU;
          break;

        case RC_MEMSIZE_24_BITS:
          self->invert = 0xffffffU;
          break;

        case RC_MEMSIZE_32_BITS:
          self->invert = 0xffffffffU;
          break;
      }
    }
  }
  else {
    self->operand2.type = RC_OPERAND_CONST;
    self->operand2.value.num = 1;
  }

  *memaddr = aux;
  return self;
}

int rc_evaluate_term(rc_term_t* self, rc_eval_state_t* eval_state) {
  /* Operands are usually memory references and are always retrieved as unsigned. The floating
   * point operand is signed, and will automatically make the result signed. Otherwise, multiply
   * by the secondary operand (which is usually 1) and cast to signed.
   */
  unsigned value = rc_evaluate_operand(&self->operand1, eval_state);

  if (self->operand2.type != RC_OPERAND_FP) {
    return (int)(value * (rc_evaluate_operand(&self->operand2, eval_state) ^ self->invert));
  }

  return (int)((double)value * self->operand2.value.dbl);
}
