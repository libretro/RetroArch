#include "rc_runtime.h"
#include "rc_internal.h"

#include "../rhash/md5.h"

#include <string.h>

#define RC_RUNTIME_MARKER            0x0A504152 /* RAP\n */

#define RC_RUNTIME_CHUNK_MEMREFS     0x4645524D /* MREF */
#define RC_RUNTIME_CHUNK_ACHIEVEMENT 0x56484341 /* ACHV */

#define RC_RUNTIME_CHUNK_DONE        0x454E4F44 /* DONE */

typedef struct rc_runtime_progress_t {
  rc_runtime_t* runtime;

  int offset;
  unsigned char* buffer;

  int chunk_size_offset;

  lua_State* L;
} rc_runtime_progress_t;

#define RC_TRIGGER_STATE_UNUPDATED 0x7F

#define RC_MEMREF_FLAG_CHANGED_THIS_FRAME 0x00010000

#define RC_COND_FLAG_IS_TRUE                            0x00000001
#define RC_COND_FLAG_OPERAND1_IS_INDIRECT_MEMREF        0x00010000
#define RC_COND_FLAG_OPERAND1_MEMREF_CHANGED_THIS_FRAME 0x00020000
#define RC_COND_FLAG_OPERAND2_IS_INDIRECT_MEMREF        0x00100000
#define RC_COND_FLAG_OPERAND2_MEMREF_CHANGED_THIS_FRAME 0x00200000

static void rc_runtime_progress_write_uint(rc_runtime_progress_t* progress, unsigned value)
{
  if (progress->buffer) {
    progress->buffer[progress->offset + 0] = value & 0xFF; value >>= 8;
    progress->buffer[progress->offset + 1] = value & 0xFF; value >>= 8;
    progress->buffer[progress->offset + 2] = value & 0xFF; value >>= 8;
    progress->buffer[progress->offset + 3] = value & 0xFF;
  }

  progress->offset += 4;
}

static unsigned rc_runtime_progress_read_uint(rc_runtime_progress_t* progress)
{
  unsigned value = progress->buffer[progress->offset + 0] |
      (progress->buffer[progress->offset + 1] << 8) |
      (progress->buffer[progress->offset + 2] << 16) |
      (progress->buffer[progress->offset + 3] << 24);

  progress->offset += 4;
  return value;
}

static void rc_runtime_progress_write_md5(rc_runtime_progress_t* progress, unsigned char* md5)
{
  if (progress->buffer)
    memcpy(&progress->buffer[progress->offset], md5, 16);

  progress->offset += 16;
}

static int rc_runtime_progress_match_md5(rc_runtime_progress_t* progress, unsigned char* md5)
{
  int result = 0;
  if (progress->buffer)
    result = (memcmp(&progress->buffer[progress->offset], md5, 16) == 0);

  progress->offset += 16;

  return result;
}

static void rc_runtime_progress_start_chunk(rc_runtime_progress_t* progress, unsigned chunk_id)
{
  rc_runtime_progress_write_uint(progress, chunk_id);

  progress->chunk_size_offset = progress->offset;

  progress->offset += 4;
}

static void rc_runtime_progress_end_chunk(rc_runtime_progress_t* progress)
{
  unsigned length;
  int offset;

  progress->offset = (progress->offset + 3) & ~0x03; /* align to 4 byte boundary */

  if (progress->buffer) {
    /* ignore chunk size field when calculating chunk size */
    length = (unsigned)(progress->offset - progress->chunk_size_offset - 4);

    /* temporarily update the write pointer to write the chunk size field */
    offset = progress->offset;
    progress->offset = progress->chunk_size_offset;
    rc_runtime_progress_write_uint(progress, length);
    progress->offset = offset;
  }
}

static void rc_runtime_progress_init(rc_runtime_progress_t* progress, rc_runtime_t* runtime, lua_State* L)
{
  memset(progress, 0, sizeof(rc_runtime_progress_t));
  progress->runtime = runtime;
  progress->L = L;
}

static int rc_runtime_progress_write_memrefs(rc_runtime_progress_t* progress)
{
  rc_memref_t* memref = progress->runtime->memrefs;
  unsigned int flags = 0;

  rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_MEMREFS);

  if (!progress->buffer) {
    while (memref) {
      progress->offset += 16;
      memref = memref->next;
    }
  }
  else {
    while (memref) {
      flags = memref->value.size;
      if (memref->value.changed)
        flags |= RC_MEMREF_FLAG_CHANGED_THIS_FRAME;

      rc_runtime_progress_write_uint(progress, memref->address);
      rc_runtime_progress_write_uint(progress, flags);
      rc_runtime_progress_write_uint(progress, memref->value.value);
      rc_runtime_progress_write_uint(progress, memref->value.prior);

      memref = memref->next;
    }
  }

  rc_runtime_progress_end_chunk(progress);
  return RC_OK;
}

static int rc_runtime_progress_read_memrefs(rc_runtime_progress_t* progress)
{
  unsigned entries;
  unsigned address, flags, value, prior;
  char size;
  rc_memref_t* memref;
  rc_memref_t* first_unmatched_memref = progress->runtime->memrefs;

  /* re-read the chunk size to determine how many memrefs are present */
  progress->offset -= 4;
  entries = rc_runtime_progress_read_uint(progress) / 16;

  while (entries != 0) {
    address = rc_runtime_progress_read_uint(progress);
    flags = rc_runtime_progress_read_uint(progress);
    value = rc_runtime_progress_read_uint(progress);
    prior = rc_runtime_progress_read_uint(progress);

    size = flags & 0xFF;

    memref = first_unmatched_memref;
    while (memref) {
      if (memref->address == address && memref->value.size == size) {
        memref->value.value = value;
        memref->value.changed = (flags & RC_MEMREF_FLAG_CHANGED_THIS_FRAME) ? 1 : 0;
        memref->value.prior = prior;

        if (memref == first_unmatched_memref)
          first_unmatched_memref = memref->next;

        break;
      }

      memref = memref->next;
    }

    --entries;
  }

  return RC_OK;
}

static int rc_runtime_progress_is_indirect_memref(rc_operand_t* oper)
{
  switch (oper->type)
  {
    case RC_OPERAND_CONST:
    case RC_OPERAND_FP:
    case RC_OPERAND_LUA:
      return 0;

    default:
      return oper->value.memref->value.is_indirect;
  }
}

static int rc_runtime_progress_write_condset(rc_runtime_progress_t* progress, rc_condset_t* condset)
{
  rc_condition_t* cond;
  unsigned flags;

  rc_runtime_progress_write_uint(progress, condset->is_paused);

  cond = condset->conditions;
  while (cond) {
    flags = 0;
    if (cond->is_true)
      flags |= RC_COND_FLAG_IS_TRUE;

    if (rc_runtime_progress_is_indirect_memref(&cond->operand1)) {
      flags |= RC_COND_FLAG_OPERAND1_IS_INDIRECT_MEMREF;
      if (cond->operand1.value.memref->value.changed)
        flags |= RC_COND_FLAG_OPERAND1_MEMREF_CHANGED_THIS_FRAME;
    }

    if (rc_runtime_progress_is_indirect_memref(&cond->operand2)) {
      flags |= RC_COND_FLAG_OPERAND2_IS_INDIRECT_MEMREF;
      if (cond->operand2.value.memref->value.changed)
        flags |= RC_COND_FLAG_OPERAND2_MEMREF_CHANGED_THIS_FRAME;
    }

    rc_runtime_progress_write_uint(progress, cond->current_hits);
    rc_runtime_progress_write_uint(progress, flags);

    if (flags & RC_COND_FLAG_OPERAND1_IS_INDIRECT_MEMREF) {
      rc_runtime_progress_write_uint(progress, cond->operand1.value.memref->value.value);
      rc_runtime_progress_write_uint(progress, cond->operand1.value.memref->value.prior);
    }

    if (flags & RC_COND_FLAG_OPERAND2_IS_INDIRECT_MEMREF) {
      rc_runtime_progress_write_uint(progress, cond->operand2.value.memref->value.value);
      rc_runtime_progress_write_uint(progress, cond->operand2.value.memref->value.prior);
    }

    cond = cond->next;
  }

  return RC_OK;
}

static int rc_runtime_progress_read_condset(rc_runtime_progress_t* progress, rc_condset_t* condset)
{
  rc_condition_t* cond;
  unsigned flags;

  condset->is_paused = (char)rc_runtime_progress_read_uint(progress);

  cond = condset->conditions;
  while (cond) {
    cond->current_hits = rc_runtime_progress_read_uint(progress);
    flags = rc_runtime_progress_read_uint(progress);

    cond->is_true = (flags & RC_COND_FLAG_IS_TRUE) ? 1 : 0;

    if (flags & RC_COND_FLAG_OPERAND1_IS_INDIRECT_MEMREF) {
      if (!rc_operand_is_memref(&cond->operand1)) /* this should never happen, but better safe than sorry */
        return RC_INVALID_STATE;

      cond->operand1.value.memref->value.value = rc_runtime_progress_read_uint(progress);
      cond->operand1.value.memref->value.prior = rc_runtime_progress_read_uint(progress);
      cond->operand1.value.memref->value.changed = (flags & RC_COND_FLAG_OPERAND1_MEMREF_CHANGED_THIS_FRAME) ? 1 : 0;
    }

    if (flags & RC_COND_FLAG_OPERAND2_IS_INDIRECT_MEMREF) {
      if (!rc_operand_is_memref(&cond->operand2)) /* this should never happen, but better safe than sorry */
        return RC_INVALID_STATE;

      cond->operand2.value.memref->value.value = rc_runtime_progress_read_uint(progress);
      cond->operand2.value.memref->value.prior = rc_runtime_progress_read_uint(progress);
      cond->operand2.value.memref->value.changed = (flags & RC_COND_FLAG_OPERAND2_MEMREF_CHANGED_THIS_FRAME) ? 1 : 0;
    }

    cond = cond->next;
  }

  return RC_OK;
}

static int rc_runtime_progress_write_trigger(rc_runtime_progress_t* progress, rc_trigger_t* trigger)
{
  rc_condset_t* condset;
  int result;

  rc_runtime_progress_write_uint(progress, trigger->state);
  rc_runtime_progress_write_uint(progress, trigger->measured_value);

  if (trigger->requirement) {
    result = rc_runtime_progress_write_condset(progress, trigger->requirement);
    if (result != RC_OK)
      return result;
  }

  condset = trigger->alternative;
  while (condset) {
    result = rc_runtime_progress_write_condset(progress, condset);
    if (result != RC_OK)
      return result;

    condset = condset->next;
  }

  return RC_OK;
}

static int rc_runtime_progress_read_trigger(rc_runtime_progress_t* progress, rc_trigger_t* trigger)
{
  rc_condset_t* condset;
  int result;

  trigger->state = (char)rc_runtime_progress_read_uint(progress);
  trigger->measured_value = rc_runtime_progress_read_uint(progress);

  if (trigger->requirement) {
    result = rc_runtime_progress_read_condset(progress, trigger->requirement);
    if (result != RC_OK)
      return result;
  }

  condset = trigger->alternative;
  while (condset) {
    result = rc_runtime_progress_read_condset(progress, condset);
    if (result != RC_OK)
      return result;

    condset = condset->next;
  }

  return RC_OK;
}

static int rc_runtime_progress_write_achievements(rc_runtime_progress_t* progress)
{
  unsigned i;
  int offset = 0;
  int result;

  for (i = 0; i < progress->runtime->trigger_count; ++i) {
    rc_runtime_trigger_t* runtime_trigger = &progress->runtime->triggers[i];
    if (!runtime_trigger->trigger)
      continue;

    /* don't store state for inactive or triggered achievements */
    if (!rc_trigger_state_active(runtime_trigger->trigger->state))
      continue;

    if (!progress->buffer) {
      if (runtime_trigger->serialized_size) {
        progress->offset += runtime_trigger->serialized_size;
        continue;
      }

      offset = progress->offset;
    }

    rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_ACHIEVEMENT);
    rc_runtime_progress_write_uint(progress, runtime_trigger->id);
    rc_runtime_progress_write_md5(progress, runtime_trigger->md5);

    result = rc_runtime_progress_write_trigger(progress, runtime_trigger->trigger);
    if (result != RC_OK)
      return result;

    rc_runtime_progress_end_chunk(progress);

    if (!progress->buffer)
      runtime_trigger->serialized_size = progress->offset - offset;
  }

  return RC_OK;
}

static int rc_runtime_progress_read_achievement(rc_runtime_progress_t* progress)
{
  unsigned id = rc_runtime_progress_read_uint(progress);
  unsigned i;

  for (i = 0; i < progress->runtime->trigger_count; ++i) {
    rc_runtime_trigger_t* runtime_trigger = &progress->runtime->triggers[i];
    if (runtime_trigger->id == id && runtime_trigger->trigger != NULL) {
      /* ignore triggered and waiting achievements */
      if (runtime_trigger->trigger->state == RC_TRIGGER_STATE_UNUPDATED) {
        /* only update state if definition hasn't changed (md5 matches) */
        if (rc_runtime_progress_match_md5(progress, runtime_trigger->md5))
          return rc_runtime_progress_read_trigger(progress, runtime_trigger->trigger);
        break;
      }
    }
  }

  return RC_OK;
}

static int rc_runtime_progress_serialize_internal(rc_runtime_progress_t* progress)
{
  md5_state_t state;
  unsigned char md5[16];
  int result;

  rc_runtime_progress_write_uint(progress, RC_RUNTIME_MARKER);

  if ((result = rc_runtime_progress_write_memrefs(progress)) != RC_OK)
    return result;

  if ((result = rc_runtime_progress_write_achievements(progress)) != RC_OK)
    return result;

  rc_runtime_progress_write_uint(progress, RC_RUNTIME_CHUNK_DONE);
  rc_runtime_progress_write_uint(progress, 16);

  if (progress->buffer) {
    md5_init(&state);
    md5_append(&state, progress->buffer, progress->offset);
    md5_finish(&state, md5);
  }

  rc_runtime_progress_write_md5(progress, md5);

  return RC_OK;
}

int rc_runtime_progress_size(const rc_runtime_t* runtime, lua_State* L)
{
  rc_runtime_progress_t progress;
  int result;

  rc_runtime_progress_init(&progress, (rc_runtime_t*)runtime, L);

  result = rc_runtime_progress_serialize_internal(&progress);
  if (result != RC_OK)
    return result;

  return progress.offset;
}

int rc_runtime_serialize_progress(void* buffer, const rc_runtime_t* runtime, lua_State* L)
{
  rc_runtime_progress_t progress;

  rc_runtime_progress_init(&progress, (rc_runtime_t*)runtime, L);
  progress.buffer = (unsigned char*)buffer;

  return rc_runtime_progress_serialize_internal(&progress);
}

int rc_runtime_deserialize_progress(rc_runtime_t* runtime, const unsigned char* serialized, lua_State* L)
{
  rc_runtime_progress_t progress;
  md5_state_t state;
  unsigned char md5[16];
  unsigned chunk_id;
  unsigned chunk_size;
  unsigned next_chunk_offset;
  unsigned i;
  int result = RC_OK;

  rc_runtime_progress_init(&progress, runtime, L);
  progress.buffer = (unsigned char*)serialized;

  if (rc_runtime_progress_read_uint(&progress) != RC_RUNTIME_MARKER) {
    rc_runtime_reset(runtime);
    return RC_INVALID_STATE;
  }

  for (i = 0; i < runtime->trigger_count; ++i) {
    rc_runtime_trigger_t* runtime_trigger = &runtime->triggers[i];
    if (runtime_trigger->trigger) {
      /* don't update state for inactive or triggered achievements */
      if (rc_trigger_state_active(runtime_trigger->trigger->state))
      {
        /* mark active achievements as unupdated. anything that's still unupdated
         * after deserializing the progress will be reset to waiting */
        runtime_trigger->trigger->state = RC_TRIGGER_STATE_UNUPDATED;
      }
    }
  }

  do {
    chunk_id = rc_runtime_progress_read_uint(&progress);
    chunk_size = rc_runtime_progress_read_uint(&progress);
    next_chunk_offset = progress.offset + chunk_size;

    switch (chunk_id)
    {
      case RC_RUNTIME_CHUNK_MEMREFS:
        result = rc_runtime_progress_read_memrefs(&progress);
        break;

      case RC_RUNTIME_CHUNK_ACHIEVEMENT:
        result = rc_runtime_progress_read_achievement(&progress);
        break;

      case RC_RUNTIME_CHUNK_DONE:
        md5_init(&state);
        md5_append(&state, progress.buffer, progress.offset);
        md5_finish(&state, md5);
        if (!rc_runtime_progress_match_md5(&progress, md5))
          result = RC_INVALID_STATE;
        break;

      default:
        if (chunk_size & 0xFFFF0000)
          result = RC_INVALID_STATE; /* assume unknown chunk > 64KB is invalid */
        break;
    }

    progress.offset = next_chunk_offset;
  } while (result == RC_OK && chunk_id != RC_RUNTIME_CHUNK_DONE);

  if (result != RC_OK) {
    rc_runtime_reset(runtime);
  }
  else {
    for (i = 0; i < runtime->trigger_count; ++i) {
      rc_trigger_t* trigger = runtime->triggers[i].trigger;
      if (trigger && trigger->state == RC_TRIGGER_STATE_UNUPDATED)
        rc_reset_trigger(trigger);
    }
  }

  return result;
}
