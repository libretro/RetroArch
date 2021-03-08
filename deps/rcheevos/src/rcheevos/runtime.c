#include "internal.h"

#include "../rhash/md5.h"

#include <stdlib.h>
#include <string.h>

#define RC_RICHPRESENCE_DISPLAY_BUFFER_SIZE 256

void rc_runtime_init(rc_runtime_t* self) {
  memset(self, 0, sizeof(rc_runtime_t));
  self->next_memref = &self->memrefs;
}

void rc_runtime_destroy(rc_runtime_t* self) {
  unsigned i;

  if (self->triggers) {
    for (i = 0; i < self->trigger_count; ++i)
      free(self->triggers[i].buffer);

    free(self->triggers);
    self->triggers = NULL;

    self->trigger_count = self->trigger_capacity = 0;
  }

  if (self->lboards) {
    free(self->lboards);
    self->lboards = NULL;

    self->lboard_count = self->lboard_capacity = 0;
  }

  while (self->richpresence) {
    rc_runtime_richpresence_t* previous = self->richpresence->previous;

    free(self->richpresence->buffer);
    free(self->richpresence);
    self->richpresence = previous;
  }

  if (self->richpresence_display_buffer) {
    free(self->richpresence_display_buffer);
    self->richpresence_display_buffer = NULL;
  }

  self->next_memref = 0;
  self->memrefs = 0;
}

static void rc_runtime_checksum(const char* memaddr, unsigned char* md5) {
  md5_state_t state;
  md5_init(&state);
  md5_append(&state, (unsigned char*)memaddr, (int)strlen(memaddr));
  md5_finish(&state, md5);
}

static void rc_runtime_deactivate_trigger_by_index(rc_runtime_t* self, unsigned index) {
  if (self->triggers[index].owns_memrefs) {
    /* if the trigger has one or more memrefs in its buffer, we can't free the buffer.
     * just null out the trigger so the runtime processor will skip it
     */
    rc_reset_trigger(self->triggers[index].trigger);
    self->triggers[index].trigger = NULL;
  }
  else {
    /* trigger doesn't own any memrefs, go ahead and free it, then replace it with the last trigger */
    free(self->triggers[index].buffer);

    if (--self->trigger_count > index)
      memcpy(&self->triggers[index], &self->triggers[self->trigger_count], sizeof(rc_runtime_trigger_t));
  }
}

void rc_runtime_deactivate_achievement(rc_runtime_t* self, unsigned id) {
  unsigned i;

  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].id == id && self->triggers[i].trigger != NULL)
      rc_runtime_deactivate_trigger_by_index(self, i);
  }
}

int rc_runtime_activate_achievement(rc_runtime_t* self, unsigned id, const char* memaddr, lua_State* L, int funcs_idx) {
  void* trigger_buffer;
  rc_trigger_t* trigger;
  rc_parse_state_t parse;
  unsigned char md5[16];
  int size;
  char owns_memref;
  unsigned i;

  if (memaddr == NULL)
    return RC_INVALID_MEMORY_OPERAND;

  rc_runtime_checksum(memaddr, md5);

  /* check to see if the id is already registered with an active trigger */
  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].id == id && self->triggers[i].trigger != NULL) {
      if (memcmp(self->triggers[i].md5, md5, 16) == 0) {
        /* if the checksum hasn't changed, we can reuse the existing item */
        rc_reset_trigger(self->triggers[i].trigger);
        return RC_OK;
      }

      /* checksum has changed, deactivate the the item */
      rc_runtime_deactivate_trigger_by_index(self, i);

      /* deactivate may reorder the list so we should continue from the current index. however, we
       * assume that only one trigger is active per id, so having found that, just stop scanning.
       */
      break;
    }
  }

  /* check to see if a disabled trigger for the specific id matches the trigger being registered */
  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].id == id && memcmp(self->triggers[i].md5, md5, 16) == 0) {
      /* retrieve the trigger pointer from the buffer */
      size = 0;
      trigger = (rc_trigger_t*)rc_alloc(self->triggers[i].buffer, &size, sizeof(rc_trigger_t), RC_ALIGNOF(rc_trigger_t), 0);
      self->triggers[i].trigger = trigger;

      rc_reset_trigger(trigger);
      return RC_OK;
    }
  }

  /* item has not been previously registered, determine how much space we need for it, and allocate it */
  size = rc_trigger_size(memaddr);
  if (size < 0)
    return size;

  trigger_buffer = malloc(size);
  if (!trigger_buffer)
    return RC_OUT_OF_MEMORY;

  /* populate the item, using the communal memrefs pool */
  rc_init_parse_state(&parse, trigger_buffer, L, funcs_idx);
  parse.first_memref = &self->memrefs;
  trigger = RC_ALLOC(rc_trigger_t, &parse);
  rc_parse_trigger_internal(trigger, &memaddr, &parse);
  rc_destroy_parse_state(&parse);

  if (parse.offset < 0) {
    free(trigger_buffer);
    *self->next_memref = NULL; /* disassociate any memrefs allocated by the failed parse */
    return parse.offset;
  }

  /* if at least one memref was allocated within the trigger, we can't free the buffer when the trigger is deactivated */
  owns_memref = (*self->next_memref != NULL);
  if (owns_memref) {
    /* advance through the new memrefs so we're ready for the next allocation */
    do {
      self->next_memref = &(*self->next_memref)->next;
    } while (*self->next_memref != NULL);
  }

  /* grow the trigger buffer if necessary */
  if (self->trigger_count == self->trigger_capacity) {
    self->trigger_capacity += 32;
    if (!self->triggers)
      self->triggers = (rc_runtime_trigger_t*)malloc(self->trigger_capacity * sizeof(rc_runtime_trigger_t));
    else
      self->triggers = (rc_runtime_trigger_t*)realloc(self->triggers, self->trigger_capacity * sizeof(rc_runtime_trigger_t));
  }

  /* assign the new trigger */
  self->triggers[self->trigger_count].id = id;
  self->triggers[self->trigger_count].trigger = trigger;
  self->triggers[self->trigger_count].buffer = trigger_buffer;
  self->triggers[self->trigger_count].serialized_size = 0;
  memcpy(self->triggers[self->trigger_count].md5, md5, 16);
  self->triggers[self->trigger_count].owns_memrefs = owns_memref;
  ++self->trigger_count;

  /* reset it, and return it */
  trigger->memrefs = NULL;
  rc_reset_trigger(trigger);
  return RC_OK;
}

rc_trigger_t* rc_runtime_get_achievement(const rc_runtime_t* self, unsigned id)
{
  unsigned i;

  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].id == id && self->triggers[i].trigger != NULL)
      return self->triggers[i].trigger;
  }

  return NULL;
}

static void rc_runtime_deactivate_lboard_by_index(rc_runtime_t* self, unsigned index) {
  if (self->lboards[index].owns_memrefs) {
    /* if the lboard has one or more memrefs in its buffer, we can't free the buffer.
     * just null out the lboard so the runtime processor will skip it
     */
    rc_reset_lboard(self->lboards[index].lboard);
    self->lboards[index].lboard = NULL;
  }
  else {
    /* lboard doesn't own any memrefs, go ahead and free it, then replace it with the last lboard */
    free(self->lboards[index].buffer);

    if (--self->lboard_count > index)
      memcpy(&self->lboards[index], &self->lboards[self->lboard_count], sizeof(rc_runtime_lboard_t));
  }
}

void rc_runtime_deactivate_lboard(rc_runtime_t* self, unsigned id) {
  unsigned i;

  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].id == id && self->lboards[i].lboard != NULL)
      rc_runtime_deactivate_lboard_by_index(self, i);
  }
}

int rc_runtime_activate_lboard(rc_runtime_t* self, unsigned id, const char* memaddr, lua_State* L, int funcs_idx) {
  void* lboard_buffer;
  unsigned char md5[16];
  rc_lboard_t* lboard;
  rc_parse_state_t parse;
  int size;
  char owns_memref;
  unsigned i;

  if (memaddr == 0)
    return RC_INVALID_MEMORY_OPERAND;

  rc_runtime_checksum(memaddr, md5);

  /* check to see if the id is already registered with an active lboard */
  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].id == id && self->lboards[i].lboard != NULL) {
      if (memcmp(self->lboards[i].md5, md5, 16) == 0) {
        /* if the checksum hasn't changed, we can reuse the existing item */
        rc_reset_lboard(self->lboards[i].lboard);
        return RC_OK;
      }

      /* checksum has changed, deactivate the the item */
      rc_runtime_deactivate_lboard_by_index(self, i);

      /* deactivate may reorder the list so we should continue from the current index. however, we
       * assume that only one trigger is active per id, so having found that, just stop scanning.
       */
      break;
    }
  }

  /* check to see if a disabled lboard for the specific id matches the lboard being registered */
  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].id == id && memcmp(self->lboards[i].md5, md5, 16) == 0) {
      /* retrieve the lboard pointer from the buffer */
      size = 0;
      lboard = (rc_lboard_t*)rc_alloc(self->lboards[i].buffer, &size, sizeof(rc_lboard_t), RC_ALIGNOF(rc_lboard_t), 0);
      self->lboards[i].lboard = lboard;

      rc_reset_lboard(lboard);
      return RC_OK;
    }
  }

  /* item has not been previously registered, determine how much space we need for it, and allocate it */
  size = rc_lboard_size(memaddr);
  if (size < 0)
    return size;

  lboard_buffer = malloc(size);
  if (!lboard_buffer)
    return RC_OUT_OF_MEMORY;

  /* populate the item, using the communal memrefs pool */
  rc_init_parse_state(&parse, lboard_buffer, L, funcs_idx);
  lboard = RC_ALLOC(rc_lboard_t, &parse);
  parse.first_memref = &self->memrefs;
  rc_parse_lboard_internal(lboard, memaddr, &parse);
  rc_destroy_parse_state(&parse);

  if (parse.offset < 0) {
    free(lboard_buffer);
    *self->next_memref = NULL; /* disassociate any memrefs allocated by the failed parse */
    return parse.offset;
  }

  /* if at least one memref was allocated within the trigger, we can't free the buffer when the trigger is deactivated */
  owns_memref = (*self->next_memref != NULL);
  if (owns_memref) {
    /* advance through the new memrefs so we're ready for the next allocation */
    do {
      self->next_memref = &(*self->next_memref)->next;
    } while (*self->next_memref != NULL);
  }

  /* grow the lboard buffer if necessary */
  if (self->lboard_count == self->lboard_capacity) {
    self->lboard_capacity += 16;
    if (!self->lboards)
      self->lboards = (rc_runtime_lboard_t*)malloc(self->lboard_capacity * sizeof(rc_runtime_lboard_t));
    else
      self->lboards = (rc_runtime_lboard_t*)realloc(self->lboards, self->lboard_capacity * sizeof(rc_runtime_lboard_t));
  }

  /* assign the new lboard */
  self->lboards[self->lboard_count].id = id;
  self->lboards[self->lboard_count].value = 0;
  self->lboards[self->lboard_count].lboard = lboard;
  self->lboards[self->lboard_count].buffer = lboard_buffer;
  memcpy(self->lboards[self->lboard_count].md5, md5, 16);
  self->lboards[self->lboard_count].owns_memrefs = owns_memref;
  ++self->lboard_count;

  /* reset it, and return it */
  lboard->memrefs = NULL;
  rc_reset_lboard(lboard);
  return RC_OK;
}

rc_lboard_t* rc_runtime_get_lboard(const rc_runtime_t* self, unsigned id)
{
  unsigned i;

  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].id == id && self->lboards[i].lboard != NULL)
      return self->lboards[i].lboard;
  }

  return NULL;
}

int rc_runtime_activate_richpresence(rc_runtime_t* self, const char* script, lua_State* L, int funcs_idx) {
  rc_richpresence_t* richpresence;
  rc_runtime_richpresence_t* previous;
  rc_richpresence_display_t* display;
  rc_parse_state_t parse;
  int size;

  if (script == NULL)
    return RC_MISSING_DISPLAY_STRING;

  size = rc_richpresence_size(script);
  if (size < 0)
    return size;

  if (!self->richpresence_display_buffer) {
    self->richpresence_display_buffer = (char*)malloc(RC_RICHPRESENCE_DISPLAY_BUFFER_SIZE * sizeof(char));
    if (!self->richpresence_display_buffer)
      return RC_OUT_OF_MEMORY;
  }
  self->richpresence_display_buffer[0] = '\0';

  previous = self->richpresence;
  if (previous) {
    if (!previous->owns_memrefs) {
      free(previous->buffer);
      previous = previous->previous;
    }
  }

  self->richpresence = (rc_runtime_richpresence_t*)malloc(sizeof(rc_runtime_richpresence_t));
  if (!self->richpresence)
    return RC_OUT_OF_MEMORY;

  self->richpresence->previous = previous;
  self->richpresence->owns_memrefs = 0;
  self->richpresence->buffer = malloc(size);

  if (!self->richpresence->buffer)
    return RC_OUT_OF_MEMORY;

  rc_init_parse_state(&parse, self->richpresence->buffer, L, funcs_idx);
  self->richpresence->richpresence = richpresence = RC_ALLOC(rc_richpresence_t, &parse);
  parse.first_memref = &self->memrefs;
  rc_parse_richpresence_internal(richpresence, script, &parse);
  rc_destroy_parse_state(&parse);

  if (parse.offset < 0) {
    free(self->richpresence->buffer);
    free(self->richpresence);
    self->richpresence = previous;
    *self->next_memref = NULL; /* disassociate any memrefs allocated by the failed parse */
    return parse.offset;
  }

  /* if at least one memref was allocated within the rich presence, we can't free the buffer when the rich presence is deactivated */
  self->richpresence->owns_memrefs = (*self->next_memref != NULL);
  if (self->richpresence->owns_memrefs) {
      /* advance through the new memrefs so we're ready for the next allocation */
      do {
          self->next_memref = &(*self->next_memref)->next;
      } while (*self->next_memref != NULL);
  }

  richpresence->memrefs = NULL;
  self->richpresence_update_timer = 0;

  if (!richpresence->first_display || !richpresence->first_display->display) {
    /* non-existant rich presence, treat like static empty string */
    *self->richpresence_display_buffer = '\0';
    self->richpresence->richpresence = NULL;
  }
  else if (richpresence->first_display->next || /* has conditional display strings */
      richpresence->first_display->display->next || /* has macros */
      richpresence->first_display->display->value.conditions) { /* is only a macro */
    /* dynamic rich presence - reset all of the conditions */
    display = richpresence->first_display;
    while (display != NULL) {
      rc_reset_trigger(&display->trigger);
      display = display->next;
    }

    rc_evaluate_richpresence(self->richpresence->richpresence, self->richpresence_display_buffer, RC_RICHPRESENCE_DISPLAY_BUFFER_SIZE - 1, NULL, NULL, L);
  }
  else {
    /* static rich presence - copy the static string */
    const char* src = richpresence->first_display->display->text;
    char* dst = self->richpresence_display_buffer;
    const char* end = dst + RC_RICHPRESENCE_DISPLAY_BUFFER_SIZE - 1;
    while (*src && dst < end)
      *dst++ = *src++;
    *dst = '\0';

    /* by setting self->richpresence to null, it won't be evaluated in do_frame() */
    self->richpresence = NULL;
  }

  return RC_OK;
}

const char* rc_runtime_get_richpresence(const rc_runtime_t* self)
{
  if (self->richpresence_display_buffer)
    return self->richpresence_display_buffer;

  return "";
}

void rc_runtime_do_frame(rc_runtime_t* self, rc_runtime_event_handler_t event_handler, rc_peek_t peek, void* ud, lua_State* L) {
  rc_runtime_event_t runtime_event;
  int i;

  runtime_event.value = 0;

  rc_update_memref_values(self->memrefs, peek, ud);

  for (i = self->trigger_count - 1; i >= 0; --i) {
    rc_trigger_t* trigger = self->triggers[i].trigger;
    int trigger_state;

    if (!trigger)
      continue;

    trigger_state = trigger->state;

    switch (rc_evaluate_trigger(trigger, peek, ud, L))
    {
      case RC_TRIGGER_STATE_RESET:
        runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_RESET;
        runtime_event.id = self->triggers[i].id;
        event_handler(&runtime_event);
        break;

      case RC_TRIGGER_STATE_TRIGGERED:
        runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED;
        runtime_event.id = self->triggers[i].id;
        event_handler(&runtime_event);
        break;

      case RC_TRIGGER_STATE_PAUSED:
        if (trigger_state != RC_TRIGGER_STATE_PAUSED) {
          runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED;
          runtime_event.id = self->triggers[i].id;
          event_handler(&runtime_event);
        }
        break;

      case RC_TRIGGER_STATE_PRIMED:
        if (trigger_state != RC_TRIGGER_STATE_PRIMED) {
          runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED;
          runtime_event.id = self->triggers[i].id;
          event_handler(&runtime_event);
        }
        break;

      case RC_TRIGGER_STATE_ACTIVE:
        if (trigger_state != RC_TRIGGER_STATE_ACTIVE) {
          runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED;
          runtime_event.id = self->triggers[i].id;
          event_handler(&runtime_event);
        }
        break;
    }
  }

  for (i = self->lboard_count - 1; i >= 0; --i) {
    rc_lboard_t* lboard = self->lboards[i].lboard;
    int lboard_state;

    if (!lboard)
      continue;

    lboard_state = lboard->state;
    switch (rc_evaluate_lboard(lboard, &runtime_event.value, peek, ud, L))
    {
      case RC_LBOARD_STATE_STARTED: /* leaderboard is running */
        if (lboard_state != RC_LBOARD_STATE_STARTED) {
          self->lboards[i].value = runtime_event.value;

          runtime_event.type = RC_RUNTIME_EVENT_LBOARD_STARTED;
          runtime_event.id = self->lboards[i].id;
          event_handler(&runtime_event);
        }
        else if (runtime_event.value != self->lboards[i].value) {
          self->lboards[i].value = runtime_event.value;

          runtime_event.type = RC_RUNTIME_EVENT_LBOARD_UPDATED;
          runtime_event.id = self->lboards[i].id;
          event_handler(&runtime_event);
        }
        break;

      case RC_LBOARD_STATE_CANCELED:
        if (lboard_state != RC_LBOARD_STATE_CANCELED) {
          runtime_event.type = RC_RUNTIME_EVENT_LBOARD_CANCELED;
          runtime_event.id = self->lboards[i].id;
          event_handler(&runtime_event);
        }
        break;

      case RC_LBOARD_STATE_TRIGGERED:
        if (lboard_state != RC_RUNTIME_EVENT_LBOARD_TRIGGERED) {
          runtime_event.type = RC_RUNTIME_EVENT_LBOARD_TRIGGERED;
          runtime_event.id = self->lboards[i].id;
          event_handler(&runtime_event);
        }
        break;
    }
  }

  if (self->richpresence && self->richpresence->richpresence) {
    if (self->richpresence_update_timer == 0) {
      /* generate into a temporary buffer so we don't get a partially updated string if it's read while its being updated */
      char buffer[RC_RICHPRESENCE_DISPLAY_BUFFER_SIZE];
      int len = rc_evaluate_richpresence(self->richpresence->richpresence, buffer, RC_RICHPRESENCE_DISPLAY_BUFFER_SIZE - 1, peek, ud, L);

      /* copy into the real buffer - write the 0 terminator first to ensure reads don't overflow the buffer */
      if (len > 0) {
        buffer[RC_RICHPRESENCE_DISPLAY_BUFFER_SIZE - 1] = '\0';
        memcpy(self->richpresence_display_buffer, buffer, RC_RICHPRESENCE_DISPLAY_BUFFER_SIZE);
      }

      /* schedule the next update for 60 frames later - most systems use a 60 fps framerate (some use more 50 or 75)
       * since we're only sending to the server every two minutes, that's only every 7200 frames while active, which
       * is evenly divisible by 50, 60, and 75.
       */
      self->richpresence_update_timer = 59;
    }
    else {
      self->richpresence_update_timer--;
    }
  }
}

void rc_runtime_reset(rc_runtime_t* self) {
  unsigned i;

  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].trigger)
      rc_reset_trigger(self->triggers[i].trigger);
  }

  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].lboard)
      rc_reset_lboard(self->lboards[i].lboard);
  }

  if (self->richpresence) {
    rc_richpresence_display_t* display = self->richpresence->richpresence->first_display;
    while (display != 0) {
      rc_reset_trigger(&display->trigger);
      display = display->next;
    }
  }
}
