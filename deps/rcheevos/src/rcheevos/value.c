#include "internal.h"

#include <string.h> /* memset */
#include <ctype.h> /* isdigit */

static void rc_parse_cond_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condition_t** next;
  int has_measured;
  int in_add_address;

  has_measured = 0;
  in_add_address = 0;

  /* this largely duplicates rc_parse_condset, but we cannot call it directly, as we need to check the
   * type of each condition as we go */
  self->conditions = RC_ALLOC(rc_condset_t, parse);
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
        if ((*next)->required_hits == 0 && (*next)->oper != RC_OPERATOR_NONE)
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

  if (!has_measured) {
    parse->offset = RC_MISSING_VALUE_MEASURED;
  }

  if (parse->buffer) {
    *next = 0;
    self->conditions->next = 0;
  }
}

void rc_parse_legacy_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condition_t** next;
  rc_condset_t** next_clause;
  rc_condition_t* cond;
  char buffer[64] = "A:";
  const char* buffer_ptr;
  char* ptr;
  int end_of_clause;

  /* convert legacy format into condset */
  self->conditions = RC_ALLOC(rc_condset_t, parse);
  self->conditions->has_pause = 0;

  next = &self->conditions->conditions;
  next_clause = &self->conditions->next;

  for (;;) {
    ptr = &buffer[2];
    end_of_clause = 0;

    do {
      switch (**memaddr) {
        case '_': /* add next */
        case '$': /* maximum of */
        case '\0': /* end of string */
        case ':': /* end of leaderboard clause */
        case ')': /* end of rich presence macro */
          end_of_clause = 1;
          *ptr = '\0';
          break;

        case '*':
          *ptr++ = '*';

          buffer_ptr = *memaddr + 1;
          if (*buffer_ptr == '-') {
            /* negative value automatically needs prefix, 'f' handles both float and digits, so use it */
            *ptr++ = 'f';
          }
          else {
            /* if it looks like a floating point number, add the 'f' prefix */
            while (isdigit((unsigned char)*buffer_ptr))
              ++buffer_ptr;
            if (*buffer_ptr == '.')
              *ptr++ = 'f';
          }
          break;

        default:
          *ptr++ = **memaddr;
          break;
      }

      ++(*memaddr);
    } while (!end_of_clause);

    buffer_ptr = buffer;
    cond = rc_parse_condition(&buffer_ptr, parse, 0);
    if (parse->offset < 0) {
      return;
    }

    switch (cond->oper) {
      case RC_OPERATOR_MULT:
      case RC_OPERATOR_DIV:
      case RC_OPERATOR_AND:
      case RC_OPERATOR_NONE:
        break;

      default:
        parse->offset = RC_INVALID_OPERATOR;
        return;
    }

    cond->pause = 0;
    *next = cond;

    switch ((*memaddr)[-1]) {
      case '_': /* add next */
        next = &cond->next;
        break;

      case '$': /* max of */
        cond->type = RC_CONDITION_MEASURED;
        cond->next = 0;
        *next_clause = RC_ALLOC(rc_condset_t, parse);
        (*next_clause)->has_pause = 0;
        next = &(*next_clause)->conditions;
        next_clause = &(*next_clause)->next;
        break;

      default: /* end of valid string */
        --(*memaddr); /* undo the increment we performed when copying the string */
        cond->type = RC_CONDITION_MEASURED;
        cond->next = 0;
        *next_clause = 0;
        return;
    }
  }
}

void rc_parse_value_internal(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  /* if it starts with a condition flag (M: A: B: C:), parse the conditions */
  if ((*memaddr)[1] == ':') {
    rc_parse_cond_value(self, memaddr, parse);
  }
  else {
    rc_parse_legacy_value(self, memaddr, parse);
  }
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

  if (!buffer || !memaddr)
    return 0;

  rc_init_parse_state(&parse, buffer, L, funcs_ndx);

  self = RC_ALLOC(rc_value_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_value_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return (parse.offset >= 0) ? self : 0;
}

int rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_eval_state_t eval_state;
  rc_condset_t* condset;
  int result = 0;

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = ud;
  eval_state.L = L;

  rc_update_memref_values(self->memrefs, peek, ud);

  rc_test_condset(self->conditions, &eval_state);
  result = (int)eval_state.measured_value;

  condset = self->conditions->next;
  while (condset != NULL) {
    rc_test_condset(condset, &eval_state);
    if ((int)eval_state.measured_value > result)
      result = (int)eval_state.measured_value;

    condset = condset->next;
  }

  return result;
}
