#include "internal.h"

rc_expression_t* rc_parse_expression(int* ret, void* buffer, rc_scratch_t* scratch, const char** memaddr, lua_State* L, int funcs_ndx) {
  rc_expression_t* self;
  rc_term_t** next;

  self = RC_ALLOC(rc_expression_t, buffer, ret, scratch);
  next = &self->terms;

  for (;;) {
    *next = rc_parse_term(ret, buffer, scratch, memaddr, L, funcs_ndx);

    if (*ret < 0) {
      return 0;
    }

    next = &(*next)->next;

    if (**memaddr != '_') {
      break;
    }

    (*memaddr)++;
  }

  *next = 0;
  return self;
}

unsigned rc_evaluate_expression(rc_expression_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_term_t* term;
  unsigned value;

  value = 0;

  for (term = self->terms; term != 0; term = term->next) {
    value += rc_evaluate_term(term, peek, ud, L);
  }

  return value;
}
