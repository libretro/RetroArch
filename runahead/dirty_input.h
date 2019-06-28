#ifndef __DIRTY_INPUT_H___
#define __DIRTY_INPUT_H___

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

int16_t input_state_get_last(unsigned port,
   unsigned device, unsigned index, unsigned id);

RETRO_END_DECLS

#endif
