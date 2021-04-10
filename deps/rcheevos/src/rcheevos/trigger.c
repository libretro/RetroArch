#include "internal.h"

#include <stddef.h>
#include <string.h> /* memset */

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

  if (!buffer || !memaddr)
    return 0;

  rc_init_parse_state(&parse, buffer, L, funcs_ndx);

  self = RC_ALLOC(rc_trigger_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_trigger_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return (parse.offset >= 0) ? self : 0;
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
  char is_primed;

  /* previously triggered, do nothing - return INACTIVE so caller doesn't report a repeated trigger */
  if (self->state == RC_TRIGGER_STATE_TRIGGERED)
    return RC_TRIGGER_STATE_INACTIVE;

  rc_update_memref_values(self->memrefs, peek, ud);

  /* not yet active, only update the memrefs - so deltas are correct when it becomes active */
  if (self->state == RC_TRIGGER_STATE_INACTIVE)
    return RC_TRIGGER_STATE_INACTIVE;

  /* process the trigger */
  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = ud;
  eval_state.L = L;

  if (self->requirement != NULL) {
    ret = rc_test_condset(self->requirement, &eval_state);
    is_paused = self->requirement->is_paused;
    is_primed = eval_state.primed;
  } else {
    ret = 1;
    is_paused = 0;
    is_primed = 1;
  }

  condset = self->alternative;
  if (condset) {
    int sub = 0;
    char sub_paused = 1;
    char sub_primed = 0;

    do {
      sub |= rc_test_condset(condset, &eval_state);
      sub_paused &= condset->is_paused;
      sub_primed |= eval_state.primed;

      condset = condset->next;
    } while (condset != 0);

    /* to trigger, the core must be true and at least one alt must be true */
    ret &= sub;
    is_primed &= sub_primed;

    /* if the core is not paused, all alts must be paused to count as a paused trigger */
    is_paused |= sub_paused;
  }

  /* if paused, the measured value may not be captured, keep the old value */
  if (!is_paused)
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

    /* if the measured value came from a hit count, reset it too */
    if (eval_state.measured_from_hits)
      self->measured_value = 0;

    /* if there were hit counts to clear, return RESET, but don't change the state */
    if (self->has_hits) {
      self->has_hits = 0;
      return RC_TRIGGER_STATE_RESET;
    }

    /* any hits that were tallied were just reset */
    eval_state.has_hits = 0;
    is_primed = 0;
  }
  else if (ret) {
    /* trigger was triggered */
    self->state = RC_TRIGGER_STATE_TRIGGERED;
    return RC_TRIGGER_STATE_TRIGGERED;
  }

  /* did not trigger this frame - update the information we'll need for next time */
  self->has_hits = eval_state.has_hits;

  if (is_paused) {
    self->state = RC_TRIGGER_STATE_PAUSED;
  }
  else if (is_primed) {
    self->state = RC_TRIGGER_STATE_PRIMED;
  }
  else {
    self->state = RC_TRIGGER_STATE_ACTIVE;
  }

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
  self->measured_value = 0;
  self->has_hits = 0;
}
