#include "internal.h"

#if !defined( __CELLOS_LV2__) && !defined(__MWERKS__)
#include <memory.h>
#endif
#include <string.h>

static void rc_parse_cond_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condition_t** next;
  int has_measured;
  int in_add_address;

  has_measured = 0;
  in_add_address = 0;
  self->expressions = 0;

  /* this largely duplicates rc_parse_condset, but we cannot call it directly, as we need to check the 
   * type of each condition as we go */
  self->conditions = RC_ALLOC(rc_condset_t, parse);
  self->conditions->next = 0;
  self->conditions->has_pause = 0;

  next = &self->conditions->conditions;
  for (;;) {
    *next = rc_parse_condition(memaddr, parse, in_add_address);

    if (parse->offset < 0) {
      return;
    }

    in_add_address = (*next)->type == RC_CONDITION_ADD_ADDRESS;

    switch ((*next)->type) {
      case RC_CONDITION_ADD_HITS:
      case RC_CONDITION_ADD_SOURCE:
      case RC_CONDITION_SUB_SOURCE:
      case RC_CONDITION_AND_NEXT:
      case RC_CONDITION_ADD_ADDRESS:
        /* combining flags are allowed */
        break;

      case RC_CONDITION_RESET_IF:
        /* ResetIf is allowed (primarily for rich presense - leaderboard will typically cancel instead of resetting) */
        break;

      case RC_CONDITION_MEASURED:
        if (has_measured) {
          parse->offset = RC_MULTIPLE_MEASURED;
          return;
        }
        has_measured = 1;
        if ((*next)->required_hits == 0 && (*next)->oper != RC_CONDITION_NONE)
          (*next)->required_hits = (unsigned)-1;
        break;

      default:
        /* non-combinding flags and PauseIf are not allowed */
        parse->offset = RC_INVALID_VALUE_FLAG;
        return;
    }

    (*next)->pause = 0;
    next = &(*next)->next;

    if (**memaddr != '_') {
      break;
    }

    (*memaddr)++;
  }

  *next = 0;

  if (!has_measured) {
    parse->offset = RC_MISSING_VALUE_MEASURED;
  }
}

void rc_parse_value_internal(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_expression_t** next;

  /* if it starts with a condition flag (M: A: B: C:), parse the conditions */
  if ((*memaddr)[1] == ':') {
    rc_parse_cond_value(self, memaddr, parse);
    return;
  }

  self->conditions = 0;
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

static int rc_evaluate_expr_value(rc_value_t* self, rc_eval_state_t* eval_state) {
  rc_expression_t* exp;
  int value, max;

  exp = self->expressions;
  max = rc_evaluate_expression(exp, eval_state);

  for (exp = exp->next; exp != 0; exp = exp->next) {
    value = rc_evaluate_expression(exp, eval_state);

    if (value > max) {
      max = value;
    }
  }

  return max;
}

int rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_eval_state_t eval_state;
  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = ud;
  eval_state.L = L;

  rc_update_memref_values(self->memrefs, peek, ud);

  if (self->expressions) {
    return rc_evaluate_expr_value(self, &eval_state);
  }

  rc_test_condset(self->conditions, &eval_state);
  return (int)eval_state.measured_value;
}
