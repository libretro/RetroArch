#include "internal.h"

#include <stdlib.h>

rc_condition_t* rc_parse_condition(const char** memaddr, rc_parse_state_t* parse, int is_indirect) {
  rc_condition_t* self;
  const char* aux;
  int ret2;
  int can_modify = 0;

  aux = *memaddr;
  self = RC_ALLOC(rc_condition_t, parse);
  self->current_hits = 0;

  if (*aux != 0 && aux[1] == ':') {
    switch (*aux) {
      case 'p': case 'P': self->type = RC_CONDITION_PAUSE_IF; break;
      case 'r': case 'R': self->type = RC_CONDITION_RESET_IF; break;
      case 'a': case 'A': self->type = RC_CONDITION_ADD_SOURCE; can_modify = 1; break;
      case 'b': case 'B': self->type = RC_CONDITION_SUB_SOURCE; can_modify = 1; break;
      case 'c': case 'C': self->type = RC_CONDITION_ADD_HITS; break;
      case 'n': case 'N': self->type = RC_CONDITION_AND_NEXT; break;
      case 'o': case 'O': self->type = RC_CONDITION_OR_NEXT; break;
      case 'm': case 'M': self->type = RC_CONDITION_MEASURED; break;
      case 'q': case 'Q': self->type = RC_CONDITION_MEASURED_IF; break;
      case 'i': case 'I': self->type = RC_CONDITION_ADD_ADDRESS; can_modify = 1; break;
      case 't': case 'T': self->type = RC_CONDITION_TRIGGER; break;
      default: parse->offset = RC_INVALID_CONDITION_TYPE; return 0;
    }

    aux += 2;
  }
  else {
    self->type = RC_CONDITION_STANDARD;
  }

  ret2 = rc_parse_operand(&self->operand1, &aux, 1, is_indirect, parse);

  if (ret2 < 0) {
    parse->offset = ret2;
    return 0;
  }

  if (self->operand1.type == RC_OPERAND_FP) {
    parse->offset = can_modify ? RC_INVALID_FP_OPERAND : RC_INVALID_COMPARISON;
    return 0;
  }

  switch (*aux++) {
    case '=':
      self->oper = RC_OPERATOR_EQ;
      aux += *aux == '=';
      break;
    
    case '!':
      if (*aux++ != '=') {
        /* fall through */
    default:
        parse->offset = RC_INVALID_OPERATOR;
        return 0;
      }

      self->oper = RC_OPERATOR_NE;
      break;
    
    case '<':
      self->oper = RC_OPERATOR_LT;

      if (*aux == '=') {
        self->oper = RC_OPERATOR_LE;
        aux++;
      }

      break;
    
    case '>':
      self->oper = RC_OPERATOR_GT;

      if (*aux == '=') {
        self->oper = RC_OPERATOR_GE;
        aux++;
      }

      break;

    case '*':
      self->oper = RC_OPERATOR_MULT;
      break;

    case '/':
      self->oper = RC_OPERATOR_DIV;
      break;

    case '&':
      self->oper = RC_OPERATOR_AND;
      break;

    case '_':
    case ')':
    case '\0':
      self->oper = RC_OPERATOR_NONE;
      self->operand2.type = RC_OPERAND_CONST;
      self->operand2.value.num = 1;
      self->required_hits = 0;
      *memaddr = aux - 1;
      return self;
  }

  switch (self->oper) {
    case RC_OPERATOR_MULT:
    case RC_OPERATOR_DIV:
    case RC_OPERATOR_AND:
      /* modifying operators are only valid on modifying statements */
      if (!can_modify) {
        parse->offset = RC_INVALID_OPERATOR;
        return 0;
      }
      break;

    default:
      /* comparison operators are not valid on modifying statements */
      if (can_modify) {
        switch (self->type) {
          case RC_CONDITION_ADD_SOURCE:
          case RC_CONDITION_SUB_SOURCE:
          case RC_CONDITION_ADD_ADDRESS:
            /* prevent parse errors on legacy achievements where a condition was present before changing the type */
            self->oper = RC_OPERATOR_NONE;
            break;

          default:
            parse->offset = RC_INVALID_OPERATOR;
            return 0;
        }
      }
      break;
  }

  ret2 = rc_parse_operand(&self->operand2, &aux, 1, is_indirect, parse);

  if (ret2 < 0) {
    parse->offset = ret2;
    return 0;
  }

  if (self->oper == RC_OPERATOR_NONE) {
    /* if operator is none, explicitly clear out the right side */
    self->operand2.type = RC_INVALID_CONST_OPERAND;
    self->operand2.value.num = 0;
  }

  if (!can_modify && self->operand2.type == RC_OPERAND_FP) {
    parse->offset = RC_INVALID_COMPARISON;
    return 0;
  }

  if (*aux == '(') {
    char* end;
    self->required_hits = (unsigned)strtoul(++aux, &end, 10);

    if (end == aux || *end != ')') {
      parse->offset = RC_INVALID_REQUIRED_HITS;
      return 0;
    }

    aux = end + 1;
  }
  else if (*aux == '.') {
    char* end;
    self->required_hits = (unsigned)strtoul(++aux, &end, 10);

    if (end == aux || *end != '.') {
      parse->offset = RC_INVALID_REQUIRED_HITS;
      return 0;
    }

    aux = end + 1;
  }
  else {
    self->required_hits = 0;
  }

  *memaddr = aux;
  return self;
}

int rc_test_condition(rc_condition_t* self, rc_eval_state_t* eval_state) {
  unsigned value1 = rc_evaluate_operand(&self->operand1, eval_state) + eval_state->add_value;
  unsigned value2 = rc_evaluate_operand(&self->operand2, eval_state);

  switch (self->oper) {
    case RC_OPERATOR_EQ: return value1 == value2;
    case RC_OPERATOR_NE: return value1 != value2;
    case RC_OPERATOR_LT: return value1 < value2;
    case RC_OPERATOR_LE: return value1 <= value2;
    case RC_OPERATOR_GT: return value1 > value2;
    case RC_OPERATOR_GE: return value1 >= value2;
    case RC_OPERATOR_NONE: return 1;
    default: return 1;
  }
}

int rc_evaluate_condition_value(rc_condition_t* self, rc_eval_state_t* eval_state) {
  unsigned value = rc_evaluate_operand(&self->operand1, eval_state);

  switch (self->oper) {
    case RC_OPERATOR_MULT:
      if (self->operand2.type == RC_OPERAND_FP)
        value = (int)((double)value * self->operand2.value.dbl);
      else
        value *= rc_evaluate_operand(&self->operand2, eval_state);
      break;

    case RC_OPERATOR_DIV:
      if (self->operand2.type == RC_OPERAND_FP)
      {
        if (self->operand2.value.dbl == 0.0)
          value = 0;
        else
          value = (int)((double)value / self->operand2.value.dbl);
      }
      else
      {
        unsigned value2 = rc_evaluate_operand(&self->operand2, eval_state);
        if (value2 == 0)
          value = 0;
        else
          value /= value2;
      }
      break;

    case RC_OPERATOR_AND:
      value &= rc_evaluate_operand(&self->operand2, eval_state);
      break;
  }

  return value;
}
