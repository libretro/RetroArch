#include "internal.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#ifndef RC_DISABLE_LUA

#ifdef __cplusplus
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>

#ifdef __cplusplus
}
#endif

#endif /* RC_DISABLE_LUA */

static int rc_parse_operand_lua(rc_operand_t* self, const char** memaddr, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
#ifndef RC_DISABLE_LUA
  const char* id;
#endif

  if (*aux++ != '@') {
    return RC_INVALID_LUA_OPERAND;
  }

  if (!isalpha(*aux)) {
    return RC_INVALID_LUA_OPERAND;
  }

#ifndef RC_DISABLE_LUA
  id = aux;
#endif

  while (isalnum(*aux) || *aux == '_') {
    aux++;
  }

#ifndef RC_DISABLE_LUA

  if (parse->L != 0) {
    if (!lua_istable(parse->L, parse->funcs_ndx)) {
      return RC_INVALID_LUA_OPERAND;
    }

    lua_pushlstring(parse->L, id, aux - id);
    lua_gettable(parse->L, parse->funcs_ndx);

    if (!lua_isfunction(parse->L, -1)) {
      lua_pop(parse->L, 1);
      return RC_INVALID_LUA_OPERAND;
    }

    self->value.luafunc = luaL_ref(parse->L, LUA_REGISTRYINDEX);
  }

#endif /* RC_DISABLE_LUA */

  self->type = RC_OPERAND_LUA;
  *memaddr = aux;
  return RC_OK;
}

static int rc_parse_operand_memory(rc_operand_t* self, const char** memaddr, rc_parse_state_t* parse, int is_indirect) {
  const char* aux = *memaddr;
  char* end;
  unsigned long address;
  char is_bcd = 0;
  char size;

  switch (*aux++) {
    case 'd': case 'D':
      self->type = RC_OPERAND_DELTA;
      break;

    case 'b': case 'B':
      self->type = RC_OPERAND_ADDRESS;
      is_bcd = 1;
      break;

    case 'p': case 'P':
      self->type = RC_OPERAND_PRIOR;
      break;

    default:
      self->type = RC_OPERAND_ADDRESS;
      aux--;
      break;
  }

  if (*aux++ != '0') {
    return RC_INVALID_MEMORY_OPERAND;
  }

  if (*aux != 'x' && *aux != 'X') {
    return RC_INVALID_MEMORY_OPERAND;
  }

  aux++;

  switch (*aux++) {
    case 'm': case 'M': size = RC_MEMSIZE_BIT_0; break;
    case 'n': case 'N': size = RC_MEMSIZE_BIT_1; break;
    case 'o': case 'O': size = RC_MEMSIZE_BIT_2; break;
    case 'p': case 'P': size = RC_MEMSIZE_BIT_3; break;
    case 'q': case 'Q': size = RC_MEMSIZE_BIT_4; break;
    case 'r': case 'R': size = RC_MEMSIZE_BIT_5; break;
    case 's': case 'S': size = RC_MEMSIZE_BIT_6; break;
    case 't': case 'T': size = RC_MEMSIZE_BIT_7; break;
    case 'l': case 'L': size = RC_MEMSIZE_LOW; break;
    case 'u': case 'U': size = RC_MEMSIZE_HIGH; break;
    case 'h': case 'H': size = RC_MEMSIZE_8_BITS; break;
    case 'w': case 'W': size = RC_MEMSIZE_24_BITS; break;
    case 'x': case 'X': size = RC_MEMSIZE_32_BITS; break;

    default: /* fall through */
      aux--;
    case ' ':
      size = RC_MEMSIZE_16_BITS;
      break;
  }

  address = strtoul(aux, &end, 16);

  if (end == aux) {
    return RC_INVALID_MEMORY_OPERAND;
  }

  if (address > 0xffffffffU) {
    address = 0xffffffffU;
  }

  self->value.memref = rc_alloc_memref_value(parse, address, size, is_bcd, is_indirect);
  if (parse->offset < 0)
    return parse->offset;

  *memaddr = end;
  return RC_OK;
}

static int rc_parse_operand_trigger(rc_operand_t* self, const char** memaddr, int is_indirect, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
  char* end;
  int ret;
  unsigned long value;

  switch (*aux) {
    case 'h': case 'H':
      if (aux[2] == 'x' || aux[2] == 'X') {
        /* H0x1234 is a typo - either H1234 or 0xH1234 was probably meant */
        return RC_INVALID_CONST_OPERAND;
      }

      value = strtoul(++aux, &end, 16);

      if (end == aux) {
        return RC_INVALID_CONST_OPERAND;
      }

      if (value > 0xffffffffU) {
        value = 0xffffffffU;
      }

      self->type = RC_OPERAND_CONST;
      self->value.num = (unsigned)value;

      aux = end;
      break;
    
    case '0':
      if (aux[1] == 'x' || aux[1] == 'X') {
        /* fall through */
    default:
        ret = rc_parse_operand_memory(self, &aux, parse, is_indirect);

        if (ret < 0) {
          return ret;
        }

        break;
      }

      /* fall through */
    case '+': case '-':
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
      value = strtoul(aux, &end, 10);
      
      if (end == aux) {
        return RC_INVALID_CONST_OPERAND;
      }

      if (value > 0xffffffffU) {
        value = 0xffffffffU;
      }

      self->type = RC_OPERAND_CONST;
      self->value.num = (unsigned)value;

      aux = end;
      break;
    
    case '@':
      ret = rc_parse_operand_lua(self, &aux, parse);

      if (ret < 0) {
        return ret;
      }

      break;
  }

  *memaddr = aux;
  return RC_OK;
}

static int rc_parse_operand_term(rc_operand_t* self, const char** memaddr, int is_indirect, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
  char* end;
  int ret;
  unsigned long value;
  long svalue;

  switch (*aux) {
    case 'h': case 'H':
      value = strtoul(++aux, &end, 16);

      if (end == aux) {
        return RC_INVALID_CONST_OPERAND;
      }

      if (value > 0xffffffffU) {
        value = 0xffffffffU;
      }

      self->type = RC_OPERAND_CONST;
      self->value.num = (unsigned)value;

      aux = end;
      break;
    
    case 'v': case 'V':
      svalue = strtol(++aux, &end, 10);

      if (end == aux) {
        return RC_INVALID_CONST_OPERAND;
      }

      if (svalue > 0xffffffffU) {
        svalue = 0xffffffffU;
      }

      self->type = RC_OPERAND_CONST;
      self->value.num = (unsigned)svalue;

      aux = end;
      break;
    
    case '0':
      if (aux[1] == 'x' || aux[1] == 'X') {
        /* fall through */
    default:
        ret = rc_parse_operand_memory(self, &aux, parse, is_indirect);

        if (ret < 0) {
          return ret;
        }

        break;
      }

      /* fall through */
    case '.':
    case '+': case '-':
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
      self->value.dbl = strtod(aux, &end);

      if (end == aux) {
        return RC_INVALID_FP_OPERAND;
      }

      if (floor(self->value.dbl) == self->value.dbl) {
        self->type = RC_OPERAND_CONST;
        self->value.num = (unsigned)floor(self->value.dbl);
      }
      else {
        self->type = RC_OPERAND_FP;
      }
      aux = end;
      break;
    
    case '@':
      ret = rc_parse_operand_lua(self, &aux, parse);

      if (ret < 0) {
        return ret;
      }

      break;
  }

  *memaddr = aux;
  return RC_OK;
}

int rc_parse_operand(rc_operand_t* self, const char** memaddr, int is_trigger, int is_indirect, rc_parse_state_t* parse) {
  if (is_trigger) {
    return rc_parse_operand_trigger(self, memaddr, is_indirect, parse);
  }
  else {
    return rc_parse_operand_term(self, memaddr, is_indirect, parse);
  }
}

#ifndef RC_DISABLE_LUA

typedef struct {
  rc_peek_t peek;
  void* ud;
}
rc_luapeek_t;

static int rc_luapeek(lua_State* L) {
  unsigned address = (unsigned)luaL_checkinteger(L, 1);
  unsigned num_bytes = (unsigned)luaL_checkinteger(L, 2);
  rc_luapeek_t* luapeek = (rc_luapeek_t*)lua_touserdata(L, 3);

  unsigned value = luapeek->peek(address, num_bytes, luapeek->ud);

  lua_pushinteger(L, value);
  return 1;
}

#endif /* RC_DISABLE_LUA */

unsigned rc_evaluate_operand(rc_operand_t* self, rc_eval_state_t* eval_state) {
#ifndef RC_DISABLE_LUA
  rc_luapeek_t luapeek;
#endif /* RC_DISABLE_LUA */

  unsigned value = 0;

  switch (self->type) {
    case RC_OPERAND_CONST:
      value = self->value.num;
      break;

    case RC_OPERAND_FP:
      /* This is handled by rc_evaluate_expression. */
      return 0;
    
    case RC_OPERAND_LUA:
#ifndef RC_DISABLE_LUA

      if (eval_state->L != 0) {
        lua_rawgeti(eval_state->L, LUA_REGISTRYINDEX, self->value.luafunc);
        lua_pushcfunction(eval_state->L, rc_luapeek);

        luapeek.peek = eval_state->peek;
        luapeek.ud = eval_state->peek_userdata;

        lua_pushlightuserdata(eval_state->L, &luapeek);
        
        if (lua_pcall(eval_state->L, 2, 1, 0) == LUA_OK) {
          if (lua_isboolean(eval_state->L, -1)) {
            value = lua_toboolean(eval_state->L, -1);
          }
          else {
            value = (unsigned)lua_tonumber(eval_state->L, -1);
          }
        }

        lua_pop(eval_state->L, 1);
      }

#endif /* RC_DISABLE_LUA */

      break;

    case RC_OPERAND_ADDRESS:
      value = rc_get_indirect_memref(self->value.memref, eval_state)->value;
      break;

    case RC_OPERAND_DELTA:
      value = rc_get_indirect_memref(self->value.memref, eval_state)->previous;
      break;

    case RC_OPERAND_PRIOR:
      value = rc_get_indirect_memref(self->value.memref, eval_state)->prior;
      break;
  }

  return value;
}
