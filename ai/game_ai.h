#pragma once


#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include <libretro.h>

EXTERNC signed short int game_ai_input(unsigned int port, unsigned int device, unsigned int idx, unsigned int id, signed short int result);
EXTERNC void game_ai_init();
EXTERNC void game_ai_load(const char * name, void * ram_ptr, int ram_size, retro_log_printf_t log);
EXTERNC void game_ai_think(bool override_p1, bool override_p2, bool show_debug, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch);