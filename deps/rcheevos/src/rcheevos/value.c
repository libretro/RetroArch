#include "internal.h"

void rc_parse_value_internal(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_expression_t** next;

  next = &self->expressions;

  for (;;) {
    *next = rc_parse_expression(memaddr, parse);

    if (parse->offset < 0) {
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
  rc_value_t* self;
  rc_parse_state_t parse;
  rc_init_parse_state(&parse, 0, 0, 0);

  self = RC_ALLOC(rc_value_t, &parse);
  rc_parse_value_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset;
}

rc_value_t* rc_parse_value(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  rc_value_t* self;
  rc_parse_state_t parse;
  rc_init_parse_state(&parse, buffer, L, funcs_ndx);
  
  self = RC_ALLOC(rc_value_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_value_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset >= 0 ? self : 0;
}

unsigned rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_expression_t* exp;
  unsigned value, max;

  rc_update_memref_values(self->memrefs, peek, ud);

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
