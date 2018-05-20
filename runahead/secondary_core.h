#ifndef __SECONDARY_CORE_H__
#define __SECONDARY_CORE_H__

#include <stddef.h>
#include <boolean.h>

#include <retro_common_api.h>

#include "../core_type.h"

RETRO_BEGIN_DECLS

bool secondary_core_run_no_input_polling(void);
bool secondary_core_deserialize(const void *buffer, int size);
bool secondary_core_ensure_exists(void);
void secondary_core_destroy(void);
void set_last_core_type(enum rarch_core_type type);
void remember_controller_port_device(long port, long device);
void clear_controller_port_map(void);
void secondary_core_set_variable_update(void);

RETRO_END_DECLS

#endif
