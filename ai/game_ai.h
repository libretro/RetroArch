#pragma once

#include <libretro.h>

signed short int game_ai_input(unsigned int port, unsigned int device, unsigned int idx, unsigned int id, signed short int result);
void game_ai_init();
void game_ai_shutdown();
void game_ai_load(const char * name, void * ram_ptr, int ram_size, retro_log_printf_t log);
void game_ai_think(bool override_p1, bool override_p2, bool show_debug, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format);