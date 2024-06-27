#pragma once


#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif


EXTERNC signed short int game_ai_input(unsigned int port, unsigned int device, unsigned int idx, unsigned int id, signed short int result);
EXTERNC void game_ai_init();
EXTERNC void game_ai_load(const char * name, void * ram_ptr, int ram_size);
EXTERNC void game_ai_think();