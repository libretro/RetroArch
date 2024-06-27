#include "game_ai.h"
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include <bitset>
#include <iostream>
#include <string>

#include "../../stable-retro-scripts/ef_lib/GameAI.h"


static creategameai_t CreateGameAI = nullptr;
static GameAI * ga = nullptr;
static volatile void * g_ram_ptr = nullptr;
static volatile int g_ram_size = 0;
static volatile signed short int g_buttons_bits = 0;
static volatile int g_frameCount = 0;
static volatile char game_ai_lib_path[1024];
static std::string g_game_name;

extern "C" signed short int game_ai_input(unsigned int port, unsigned int device, unsigned int idx, unsigned int id, signed short int result)
{
   if(ga == nullptr)
      return 0;

   return g_buttons_bits;
}

extern "C" void game_ai_init()
{
   //printf("GAME AI INIT");

   if(CreateGameAI == nullptr)
   {
      void *myso = dlopen("libgame_ai.so", RTLD_NOW);
      assert(myso);

      dlinfo(myso, RTLD_DI_ORIGIN, (void *) &game_ai_lib_path);

      CreateGameAI = reinterpret_cast<creategameai_t>(dlsym(myso, "CreateGameAI"));
      assert(CreateGameAI);
   }
}

extern "C" void game_ai_load(const char * name, void * ram_ptr, int ram_size)
{
   g_game_name = "NHL941on1-Genesis";

   g_ram_ptr = ram_ptr;
   g_ram_size = ram_size;
}

extern "C" void game_ai_think() 
{
   if(ga == nullptr && g_ram_ptr != nullptr)
   {
      ga = CreateGameAI(g_game_name.c_str());
      assert(ga);

      std::string data_path((char *)game_ai_lib_path);
      data_path += "/data/NHL941on1-Genesis";

      ga->Init(data_path.c_str(), (void *) g_ram_ptr, g_ram_size);
   }

   if (g_frameCount >= 3)
	{
		if(ga)
		{
         bool b[16] = {0};
			ga->Think(b);

         g_buttons_bits=0;

         for(int bit=0; bit<=15; bit++){
            g_buttons_bits |= b[bit] ? (1 << bit) : 0;
		   }
      }
		
		g_frameCount=0;
	}
	else
   {
		g_frameCount++;
	}
}

