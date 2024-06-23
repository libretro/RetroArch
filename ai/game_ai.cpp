#include "game_ai.h"
#include <stdio.h>


extern "C" void game_ai_test(unsigned port, unsigned device, unsigned idx, unsigned id)
{
   printf("AI INPUT %d, %d, %d, %d\n", port, device, idx, id);

   return;
}

extern "C" void game_ai_init()
{
   printf("GAME AI INIT");

}