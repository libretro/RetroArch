#include "../general.h"
#include "rarch_sdl_input.h"

#ifndef _LINUXRAW_INPUT_H
#define _LINUXRAW_INPUT_H

typedef struct linuxraw_input
{
	sdl_input_t *sdl;
	bool state[0x80];
} linuxraw_input_t;

#endif
