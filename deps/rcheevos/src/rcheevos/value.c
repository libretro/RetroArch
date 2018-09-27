#include "internal.h"

void rc_parse_value_internal(rc_value_t* self, int* ret, void* buffer, void* scratch, const char** memaddr, lua_State* L, int funcs_ndx) {
  rc_expression_t** next;

  next = &self->expressions;

  for (;;) {
    *next = rc_parse_expression(ret, buffer, scratch, memaddr, L, funcs_ndx);

    if (*ret < 0) {
      return;
    }

    next = &(*next)->next;

    if (**memaddr != '$') {
      break;
    }

    (*memaddr)++;
  }

  *next = 0;
}

int rc_value_size(const char* memaddr) {
  int ret;
  rc_value_t* self;
  rc_scratch_t scratch;

  ret = 0;
  self = RC_ALLOC(rc_value_t, 0, &ret, &scratch);
  rc_parse_value_internal(self, &ret, 0, &scratch, &memaddr, 0, 0);
  return ret;
}

rc_value_t* rc_parse_value(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  int ret;
  rc_value_t* self;
  rc_scratch_t scratch;
  
  ret = 0;
  self = RC_ALLOC(rc_value_t, buffer, &ret, &scratch);
  rc_parse_value_internal(self, &ret, buffer, 0, &memaddr, L, funcs_ndx);
  return ret >= 0 ? self : 0;
}

unsigned rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_expression_t* exp;
  unsigned value, max;

  exp = self->expressions;
  max = rc_evaluate_expression(exp, peek, ud, L);

  for (exp = exp->next; exp != 0; exp = exp->next) {
    value = rc_evaluate_expression(exp, peek, ud, L);

    if (value > max) {
      max = value;
    }
  }

  return max;
}
