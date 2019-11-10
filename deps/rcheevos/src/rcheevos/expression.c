#include "internal.h"

rc_expression_t* rc_parse_expression(const char** memaddr, rc_parse_state_t* parse) {
  rc_expression_t* self;
  rc_term_t** next;

  self = RC_ALLOC(rc_expression_t, parse);
  next = &self->terms;

  for (;;) {
    *next = rc_parse_term(memaddr, 0, parse);

    if (parse->offset < 0) {
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

int rc_evaluate_expression(rc_expression_t* self, rc_eval_state_t* eval_state) {
  rc_term_t* term;
  int value;

  value = 0;

  for (term = self->terms; term != 0; term = term->next) {
    value += rc_evaluate_term(term, eval_state);
  }

  return value;
}
