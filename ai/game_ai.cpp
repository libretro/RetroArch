#include "game_ai.h"
#include <stdio.h>
#include <retro_assert.h>
#include <bitset>
#include <iostream>
#include <string>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#define GAME_AI_MAX_PLAYERS 2

#include "../deps/game_ai_lib/GameAI.h"


creategameai_t             CreateGameAI = nullptr;
GameAI *                   ga = nullptr;
volatile void *            g_ram_ptr = nullptr;
volatile int               g_ram_size = 0;
volatile signed short int  g_buttons_bits[GAME_AI_MAX_PLAYERS] = {0};
volatile int               g_frameCount = 0;
volatile char              game_ai_lib_path[1024] = {0};
std::string                g_game_name;
retro_log_printf_t         g_log = nullptr;

//======================================================
// Helper functions
//======================================================
extern "C" void game_ai_debug_log(int level, const char *fmt, ...)
{
   va_list vp;
   va_start(vp, fmt);

   if (g_log)
   {
      g_log((enum retro_log_level)level, fmt, vp);
   }

   va_end(vp);
}

void array_to_bits_16(volatile signed short & result, const bool b[16])
{
   for (int bit = 0; bit <= 15; bit++)
   {
      result |= b[bit] ? (1 << bit) : 0;
   }
}

//======================================================
// Interface to RA
//======================================================
extern "C" signed short int game_ai_input(unsigned int port, unsigned int device, unsigned int idx, unsigned int id, signed short int result)
{
   if (ga == nullptr)
      return 0;

   if (port < GAME_AI_MAX_PLAYERS)
      return g_buttons_bits[port];

   return 0;
}

extern "C" void game_ai_init()
{
   printf("GameAIManager::Init()\n");

   if (CreateGameAI == nullptr)
   {
#ifdef _WIN32
   HINSTANCE hinstLib;
   BOOL fFreeResult, fRunTimeLinkSuccess = FALSE;

   hinstLib = LoadLibrary(TEXT("game_ai.dll"));
   retro_assert(hinstLib);

   char full_module_path[MAX_PATH];
   DWORD dwLen = GetModuleFileNameA(hinstLib, static_cast<char*>(&full_module_path), MAX_PATH);

   if (hinstLib != NULL)
   {
      CreateGameAI = (creategameai_t) GetProcAddress(hinstLib, "CreateGameAI");

      retro_assert(CreateGameAI);
   }
#else
      void *myso = dlopen("libgame_ai.so", RTLD_NOW);
      retro_assert(myso);

      if(myso != NULL)
      {
         dlinfo(myso, RTLD_DI_ORIGIN, (void *) &game_ai_lib_path);

         CreateGameAI = reinterpret_cast<creategameai_t>(dlsym(myso, "CreateGameAI"));
         retro_assert(CreateGameAI);
      }
#endif
   }
}

extern "C" void game_ai_load(const char * name, void * ram_ptr, int ram_size, retro_log_printf_t log)
{
   printf("GameAIManager::Load\n");

   g_game_name = name;

   g_ram_ptr = ram_ptr;
   g_ram_size = ram_size;

   g_log = log;
}

extern "C" void game_ai_think(bool override_p1, bool override_p2, bool show_debug, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format)
{
   if (ga)
      ga->SetShowDebug(show_debug);

   if (ga == nullptr && g_ram_ptr != nullptr)
   {
      ga = CreateGameAI(g_game_name.c_str());
      retro_assert(ga);

      if (ga)
      {
         std::string data_path((char *)game_ai_lib_path);
         data_path += "/data/";
         data_path += g_game_name;

         ga->Init((void *) g_ram_ptr, g_ram_size);

         ga->SetDebugLog(game_ai_debug_log);
      }
   }

   if (g_frameCount >= 3)
   {
      if (ga)
      {
         bool b[16] = {0};

         g_buttons_bits[0]=0;
         g_buttons_bits[1]=0;

         if (override_p1)
         {
            ga->Think(b, 0, frame_data, frame_width, frame_height, frame_pitch, pixel_format);
            array_to_bits_16(g_buttons_bits[0], b);
         }

         if (override_p2)
         {
            ga->Think(b, 1, frame_data, frame_width, frame_height, frame_pitch, pixel_format);
            array_to_bits_16(g_buttons_bits[1], b);
         }
      }
      g_frameCount=0;
   }
   else
   {
      g_frameCount++;
   }
}
