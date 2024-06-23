#pragma once




#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif


EXTERNC void game_ai_test(unsigned port, unsigned device, unsigned idx, unsigned id);
EXTERNC void game_ai_init();