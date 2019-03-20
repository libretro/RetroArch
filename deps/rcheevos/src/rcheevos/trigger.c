#include "internal.h"

#include <stddef.h>

void rc_parse_trigger_internal(rc_trigger_t* self, int* ret, void* buffer, rc_scratch_t* scratch, const char** memaddr, lua_State* L, int funcs_ndx) {
  rc_condset_t** next;
  const char* aux;

  aux = *memaddr;
  next = &self->alternative;

  if (*aux == 's' || *aux == 'S') {
    self->requirement = 0;
  }
  else {
    self->requirement = rc_parse_condset(ret, buffer, scratch, &aux, L, funcs_ndx);

    if (*ret < 0) {
      return;
    }

    self->requirement->next = 0;
  }

  while (*aux == 's' || *aux == 'S') {
    aux++;
    *next = rc_parse_condset(ret, buffer, scratch, &aux, L, funcs_ndx);

    if (*ret < 0) {
      return;
    }

    next = &(*next)->next;
  }
  
  *next = 0;
  *memaddr = aux;
}

int rc_trigger_size(const char* memaddr) {
  int ret;
  rc_trigger_t* self;
  rc_scratch_t scratch;

  ret = 0;
  self = RC_ALLOC(rc_trigger_t, 0, &ret, &scratch);
  rc_parse_trigger_internal(self, &ret, 0, &scratch, &memaddr, 0, 0);
  return ret;
}

rc_trigger_t* rc_parse_trigger(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  int ret;
  rc_trigger_t* self;
  rc_scratch_t scratch;
  
  ret = 0;
  self = RC_ALLOC(rc_trigger_t, buffer, &ret, &scratch);
  rc_parse_trigger_internal(self, &ret, buffer, 0, &memaddr, L, funcs_ndx);
  return ret >= 0 ? self : 0;
}

int rc_test_trigger(rc_trigger_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  int ret, reset;
  rc_condset_t* condset;

  reset = 0;
  ret = self->requirement != 0 ? rc_test_condset(self->requirement, &reset, peek, ud, L) : 1;
  condset = self->alternative;

  if (condset) {
    int sub = 0;

    do {
      sub |= rc_test_condset(condset, &reset, peek, ud, L);
      condset = condset->next;
    }
    while (condset != 0);

    ret &= sub && !reset;
  }

  if (reset) {
    rc_reset_trigger(self);
  }

  return ret;
}

void rc_reset_trigger(rc_trigger_t* self) {
  rc_condset_t* condset;

  if (self->requirement != 0) {
    rc_reset_condset(self->requirement);
  }

  condset = self->alternative;

  while (condset != 0) {
    rc_reset_condset(condset);
    condset = condset->next;
  }
}
