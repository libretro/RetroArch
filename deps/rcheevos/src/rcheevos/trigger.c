#include "internal.h"

#include <stddef.h>
#if !defined( __CELLOS_LV2__) && !defined(__MWERKS__)
#include <memory.h>
#endif
#include <string.h>

void rc_parse_trigger_internal(rc_trigger_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condset_t** next;
  const char* aux;

  aux = *memaddr;
  next = &self->alternative;

  if (*aux == 's' || *aux == 'S') {
    self->requirement = 0;
  }
  else {
    self->requirement = rc_parse_condset(&aux, parse);

    if (parse->offset < 0) {
      return;
    }

    self->requirement->next = 0;
  }

  while (*aux == 's' || *aux == 'S') {
    aux++;
    *next = rc_parse_condset(&aux, parse);

    if (parse->offset < 0) {
      return;
    }

    next = &(*next)->next;
  }
  
  *next = 0;
  *memaddr = aux;

  self->measured_value = 0;
  self->measured_target = parse->measured_target;
  self->state = RC_TRIGGER_STATE_WAITING;
  self->has_hits = 0;
}

int rc_trigger_size(const char* memaddr) {
  rc_trigger_t* self;
  rc_parse_state_t parse;
  rc_init_parse_state(&parse, 0, 0, 0);

  self = RC_ALLOC(rc_trigger_t, &parse);
  rc_parse_trigger_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset;
}

rc_trigger_t* rc_parse_trigger(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  rc_trigger_t* self;
  rc_parse_state_t parse;
  rc_init_parse_state(&parse, buffer, L, funcs_ndx);
  
  self = RC_ALLOC(rc_trigger_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_trigger_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset >= 0 ? self : 0;
}

static void rc_reset_trigger_hitcounts(rc_trigger_t* self) {
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

int rc_evaluate_trigger(rc_trigger_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_eval_state_t eval_state;
  rc_condset_t* condset;
  int ret;
  char is_paused;

  /* previously triggered, do nothing - return INACTIVE so caller doesn't report a repeated trigger */
  if (self->state == RC_TRIGGER_STATE_TRIGGERED)
      return RC_TRIGGER_STATE_INACTIVE;

  rc_update_memref_values(self->memrefs, peek, ud);

  /* not yet active, only update the memrefs - so deltas are corrent when it becomes active */
  if (self->state == RC_TRIGGER_STATE_INACTIVE)
    return RC_TRIGGER_STATE_INACTIVE;

  /* process the trigger */
  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = ud;
  eval_state.L = L;

  ret = self->requirement != 0 ? rc_test_condset(self->requirement, &eval_state) : 1;
  condset = self->alternative;

  if (condset) {
    int sub = 0;

    do {
      sub |= rc_test_condset(condset, &eval_state);
      condset = condset->next;
    }
    while (condset != 0);

    ret &= sub;
  }

  self->measured_value = eval_state.measured_value;

  /* if the state is WAITING and the trigger is ready to fire, ignore it and reset the hit counts */
  /* otherwise, if the state is WAITING, proceed to activating the trigger */
  if (self->state == RC_TRIGGER_STATE_WAITING && ret) {
    rc_reset_trigger(self);
    self->has_hits = 0;
    return RC_TRIGGER_STATE_WAITING;
  }

  if (eval_state.was_reset) {
    /* if any ResetIf condition was true, reset the hit counts */
    rc_reset_trigger_hitcounts(self);

    /* if there were hit counts to clear, return RESET, but don't change the state */
    if (self->has_hits) {
      self->has_hits = 0;
      return RC_TRIGGER_STATE_RESET;
    }

    /* any hits that were tallied were just reset */
    eval_state.has_hits = 0;
  }
  else if (ret) {
    /* trigger was triggered */
    self->state = RC_TRIGGER_STATE_TRIGGERED;
    return RC_TRIGGER_STATE_TRIGGERED;
  }

  /* did not trigger this frame - update the information we'll need for next time */
  self->has_hits = eval_state.has_hits;

  /* check to see if the trigger is paused */
  is_paused = (self->requirement != NULL) ? self->requirement->is_paused : 0;
  if (!is_paused) {
    /* if the core is not paused, all alts must be paused to count as a paused trigger */
    is_paused = (self->alternative != NULL);
    for (condset = self->alternative; condset != NULL; condset = condset->next) {
      if (!condset->is_paused) {
        is_paused = 0;
        break;
      }
    }
  }

  self->state = is_paused ? RC_TRIGGER_STATE_PAUSED : RC_TRIGGER_STATE_ACTIVE;
  return self->state;
}

int rc_test_trigger(rc_trigger_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  /* for backwards compatibilty, rc_test_trigger always assumes the achievement is active */
  self->state = RC_TRIGGER_STATE_ACTIVE;

  return (rc_evaluate_trigger(self, peek, ud, L) == RC_TRIGGER_STATE_TRIGGERED);
}

void rc_reset_trigger(rc_trigger_t* self) {
  rc_reset_trigger_hitcounts(self);

  self->state = RC_TRIGGER_STATE_WAITING;
}
