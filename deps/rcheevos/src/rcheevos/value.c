#include "rc_internal.h"

#include <string.h> /* memset */
#include <ctype.h> /* isdigit */

static void rc_parse_cond_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condset_t** next_clause;

  next_clause = &self->conditions;

  do
  {
    parse->measured_target = 0; /* passing is_value=1 should prevent any conflicts, but clear it out anyway */
    *next_clause = rc_parse_condset(memaddr, parse, 1);
    if (parse->offset < 0) {
      return;
    }

    if (**memaddr == 'S' || **memaddr == 's') {
      /* alt groups not supported */
      parse->offset = RC_INVALID_VALUE_FLAG;
    }
    else if (parse->measured_target == 0) {
      parse->offset = RC_MISSING_VALUE_MEASURED;
    }
    else if (**memaddr == '$') {
      /* maximum of */
      ++(*memaddr);
      next_clause = &(*next_clause)->next;
      continue;
    }

    break;
  } while (1);

  (*next_clause)->next = 0;
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
  self->conditions->is_paused = 0;

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
          if (*buffer_ptr == '-' || *buffer_ptr == '+')
            ++buffer_ptr; /* ignore sign */

          /* if it looks like a floating point number, add the 'f' prefix */
          while (isdigit((unsigned char)*buffer_ptr))
            ++buffer_ptr;
          if (*buffer_ptr == '.')
            *ptr++ = 'f';
          break;

        default:
          *ptr++ = **memaddr;
          break;
      }

      ++(*memaddr);
    } while (!end_of_clause);

    buffer_ptr = buffer;
    cond = rc_parse_condition(&buffer_ptr, parse, 0);
    if (parse->offset < 0)
      return;

    if (*buffer_ptr) {
      /* whatever we copied as a single condition was not fully consumed */
      parse->offset = RC_INVALID_COMPARISON;
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
        (*next_clause)->is_paused = 0;
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

  self->name = "(unnamed)";
  self->value.value = self->value.prior = 0;
  self->value.changed = 0;
  self->next = 0;
}

int rc_value_size(const char* memaddr) {
  rc_value_t* self;
  rc_parse_state_t parse;
  rc_memref_t* first_memref;
  rc_init_parse_state(&parse, 0, 0, 0);
  rc_init_parse_state_memrefs(&parse, &first_memref);

  self = RC_ALLOC(rc_value_t, &parse);
  rc_parse_value_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset;
}

rc_value_t* rc_parse_value(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  rc_value_t* self;
  rc_parse_state_t parse;

  if (!buffer || !memaddr)
    return NULL;

  rc_init_parse_state(&parse, buffer, L, funcs_ndx);

  self = RC_ALLOC(rc_value_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_value_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return (parse.offset >= 0) ? self : NULL;
}

int rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_eval_state_t eval_state;
  rc_condset_t* condset;
  int result = 0;
  int paused = 1;

  rc_update_memref_values(self->memrefs, peek, ud);

  for (condset = self->conditions; condset != NULL; condset = condset->next) {
    memset(&eval_state, 0, sizeof(eval_state));
    eval_state.peek = peek;
    eval_state.peek_userdata = ud;
    eval_state.L = L;

    rc_test_condset(condset, &eval_state);

    if (condset->is_paused)
      continue;

    if (eval_state.was_reset) {
      /* if any ResetIf condition was true, reset the hit counts
       * NOTE: ResetIf only affects the current condset when used in values!
       */
      rc_reset_condset(condset);

      /* if the measured value came from a hit count, reset it too */
      if (eval_state.measured_from_hits)
        eval_state.measured_value = 0;
    }

    if (paused) {
      /* capture the first valid measurement */
      result = (int)eval_state.measured_value;
      paused = 0;
    }
    else {
      /* multiple condsets are currently only used for the MAX_OF operation.
       * only keep the condset's value if it's higher than the current highest value.
       */
      if ((int)eval_state.measured_value > result)
        result = (int)eval_state.measured_value;
    }
  }

  if (!paused) {
    /* if not paused, store the value so that it's available when paused. */
    rc_update_memref_value(&self->value, result);
  }
  else {
    /* when paused, the Measured value will not be captured, use the last captured value. */
    result = self->value.value;
  }

  return result;
}

void rc_reset_value(rc_value_t* self) {
  rc_condset_t* condset = self->conditions;
  while (condset != NULL) {
    rc_reset_condset(condset);
    condset = condset->next;
  }

  self->value.value = self->value.prior = 0;
  self->value.changed = 0;
}

void rc_init_parse_state_variables(rc_parse_state_t* parse, rc_value_t** variables) {
  parse->variables = variables;
  *variables = 0;
}

rc_value_t* rc_alloc_helper_variable(const char* memaddr, int memaddr_len, rc_parse_state_t* parse)
{
  rc_value_t** variables = parse->variables;
  rc_value_t* value;
  const char* name;
  unsigned measured_target;

  while ((value = *variables) != NULL) {
    if (strncmp(value->name, memaddr, memaddr_len) == 0 && value->name[memaddr_len] == 0)
      return value;

    variables = &value->next;
  }

  value = RC_ALLOC_SCRATCH(rc_value_t, parse);
  memset(&value->value, 0, sizeof(value->value));
  value->value.size = RC_MEMSIZE_VARIABLE;
  value->memrefs = NULL;

  /* capture name before calling parse as parse will update memaddr pointer */
  name = rc_alloc_str(parse, memaddr, memaddr_len);
  if (!name)
    return NULL;

  /* the helper variable likely has a Measured condition. capture the current measured_target so we can restore it
   * after generating the variable so the variable's Measured target doesn't conflict with the rest of the trigger. */
  measured_target = parse->measured_target;

  /* disable variable resolution when defining a variable to prevent infinite recursion */
  variables = parse->variables;
  parse->variables = NULL;
  rc_parse_value_internal(value, &memaddr, parse);
  parse->variables = variables;

  /* restore the measured target */
  parse->measured_target = measured_target;

  /* store name after calling parse as parse will set name to (unnamed) */
  value->name = name;

  /* append the new variable to the end of the list (have to re-evaluate in case any others were added) */
  while (*variables != NULL)
    variables = &(*variables)->next;
  *variables = value;

  return value;
}

void rc_update_variables(rc_value_t* variable, rc_peek_t peek, void* ud, lua_State* L) {
  while (variable) {
    rc_evaluate_value(variable, peek, ud, L);
    variable = variable->next;
  }
}
