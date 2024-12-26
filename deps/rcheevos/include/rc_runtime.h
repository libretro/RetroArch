#ifndef RC_RUNTIME_H
#define RC_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rc_error.h"

#include <stddef.h>
#include <stdint.h>

/*****************************************************************************\
| Forward Declarations (defined in rc_runtime_types.h)                        |
\*****************************************************************************/

#ifndef RC_RUNTIME_TYPES_H /* prevents pedantic redefinition error */

typedef struct lua_State lua_State;

typedef struct rc_trigger_t rc_trigger_t;
typedef struct rc_lboard_t rc_lboard_t;
typedef struct rc_richpresence_t rc_richpresence_t;
typedef struct rc_memref_t rc_memref_t;
typedef struct rc_value_t rc_value_t;

#endif

/*****************************************************************************\
| Callbacks                                                                   |
\*****************************************************************************/

/**
 * Callback used to read num_bytes bytes from memory starting at address. If
 * num_bytes is greater than 1, the value is read in little-endian from
 * memory.
 */
typedef uint32_t(*rc_runtime_peek_t)(uint32_t address, uint32_t num_bytes, void* ud);

/*****************************************************************************\
| Runtime                                                                     |
\*****************************************************************************/

typedef struct rc_runtime_trigger_t {
  uint32_t id;
  rc_trigger_t* trigger;
  void* buffer;
  rc_memref_t* invalid_memref;
  uint8_t md5[16];
  int32_t serialized_size;
  uint8_t owns_memrefs;
}
rc_runtime_trigger_t;

typedef struct rc_runtime_lboard_t {
  uint32_t id;
  int32_t value;
  rc_lboard_t* lboard;
  void* buffer;
  rc_memref_t* invalid_memref;
  uint8_t md5[16];
  uint32_t serialized_size;
  uint8_t owns_memrefs;
}
rc_runtime_lboard_t;

typedef struct rc_runtime_richpresence_t {
  rc_richpresence_t* richpresence;
  void* buffer;
  struct rc_runtime_richpresence_t* previous;
  uint8_t md5[16];
  uint8_t owns_memrefs;
}
rc_runtime_richpresence_t;

typedef struct rc_runtime_t {
  rc_runtime_trigger_t* triggers;
  uint32_t trigger_count;
  uint32_t trigger_capacity;

  rc_runtime_lboard_t* lboards;
  uint32_t lboard_count;
  uint32_t lboard_capacity;

  rc_runtime_richpresence_t* richpresence;

  rc_memref_t* memrefs;
  rc_memref_t** next_memref;

  rc_value_t* variables;
  rc_value_t** next_variable;

  uint8_t owns_self;
}
rc_runtime_t;

rc_runtime_t* rc_runtime_alloc(void);
void rc_runtime_init(rc_runtime_t* runtime);
void rc_runtime_destroy(rc_runtime_t* runtime);

int rc_runtime_activate_achievement(rc_runtime_t* runtime, uint32_t id, const char* memaddr, lua_State* L, int funcs_idx);
void rc_runtime_deactivate_achievement(rc_runtime_t* runtime, uint32_t id);
rc_trigger_t* rc_runtime_get_achievement(const rc_runtime_t* runtime, uint32_t id);
int rc_runtime_get_achievement_measured(const rc_runtime_t* runtime, uint32_t id, unsigned* measured_value, unsigned* measured_target);
int rc_runtime_format_achievement_measured(const rc_runtime_t* runtime, uint32_t id, char *buffer, size_t buffer_size);

int rc_runtime_activate_lboard(rc_runtime_t* runtime, uint32_t id, const char* memaddr, lua_State* L, int funcs_idx);
void rc_runtime_deactivate_lboard(rc_runtime_t* runtime, uint32_t id);
rc_lboard_t* rc_runtime_get_lboard(const rc_runtime_t* runtime, uint32_t id);
int rc_runtime_format_lboard_value(char* buffer, int size, int32_t value, int format);


int rc_runtime_activate_richpresence(rc_runtime_t* runtime, const char* script, lua_State* L, int funcs_idx);
int rc_runtime_get_richpresence(const rc_runtime_t* runtime, char* buffer, size_t buffersize, rc_runtime_peek_t peek, void* peek_ud, lua_State* L);

enum {
  RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, /* from WAITING, PAUSED, or PRIMED to ACTIVE */
  RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_RESET,
  RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED,
  RC_RUNTIME_EVENT_LBOARD_STARTED,
  RC_RUNTIME_EVENT_LBOARD_CANCELED,
  RC_RUNTIME_EVENT_LBOARD_UPDATED,
  RC_RUNTIME_EVENT_LBOARD_TRIGGERED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED,
  RC_RUNTIME_EVENT_LBOARD_DISABLED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_PROGRESS_UPDATED
};

typedef struct rc_runtime_event_t {
  uint32_t id;
  int32_t value;
  uint8_t type;
}
rc_runtime_event_t;

typedef void (*rc_runtime_event_handler_t)(const rc_runtime_event_t* runtime_event);

void rc_runtime_do_frame(rc_runtime_t* runtime, rc_runtime_event_handler_t event_handler, rc_runtime_peek_t peek, void* ud, lua_State* L);
void rc_runtime_reset(rc_runtime_t* runtime);

typedef int (*rc_runtime_validate_address_t)(uint32_t address);
void rc_runtime_validate_addresses(rc_runtime_t* runtime, rc_runtime_event_handler_t event_handler, rc_runtime_validate_address_t validate_handler);
void rc_runtime_invalidate_address(rc_runtime_t* runtime, uint32_t address);

int rc_runtime_progress_size(const rc_runtime_t* runtime, lua_State* L);
int rc_runtime_serialize_progress(void* buffer, const rc_runtime_t* runtime, lua_State* L);
int rc_runtime_deserialize_progress(rc_runtime_t* runtime, const uint8_t* serialized, lua_State* L);

#ifdef __cplusplus
}
#endif

#endif /* RC_RUNTIME_H */
