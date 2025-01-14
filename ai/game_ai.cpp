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

#include "GameAILibAPI.h"

class GameAIManager {
public:
   GameAIManager();

   signed short int  Input(unsigned int port, unsigned int device, unsigned int idx, unsigned int id, signed short int result);
   void              Init();
   void              Load(const char * name, void * ram_ptr, int ram_size, retro_log_printf_t log);
   void              Think(bool override_p1, bool override_p2, bool show_debug, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format);

private:
   creategameai_t             CreateGameAI;
   GameAI *                   ga;
   volatile void *            g_ram_ptr;
   volatile int               g_ram_size;
   volatile signed short int  g_buttons_bits[GAME_AI_MAX_PLAYERS];
   volatile int               g_frameCount;
   volatile char              game_ai_lib_path[1024];
   std::string                g_game_name;

};

GameAIManager        gameAIMgr;
retro_log_printf_t   g_log;

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
// GameAIManager::GameAIManager
//======================================================
GameAIManager::GameAIManager()
{
   CreateGameAI = nullptr;
   ga = nullptr;
   g_ram_ptr = nullptr;
   g_ram_size = 0;
   g_buttons_bits[0] = 0;
   g_buttons_bits[1] = 0;
   g_frameCount = 0;
   game_ai_lib_path[0]='\0';
   g_log = nullptr;
}

//======================================================
// GameAIManager::Input
//======================================================
signed short int GameAIManager::Input(unsigned int port, unsigned int device, unsigned int idx, unsigned int id, signed short int result)
{
   if (ga == nullptr)
      return 0;

   if (port < GAME_AI_MAX_PLAYERS)
      return g_buttons_bits[port];

   return 0;
}

//======================================================
// GameAIManager::Init
//======================================================
void GameAIManager::Init()
{
   if (CreateGameAI == nullptr)
   {
#ifdef _WIN32
   HINSTANCE hinstLib; 
   BOOL fFreeResult, fRunTimeLinkSuccess = FALSE;

   hinstLib = LoadLibrary(TEXT("game_ai.dll"));
   assert(hinstLib);

   char full_module_path[MAX_PATH];
   DWORD dwLen = GetModuleFileNameA(hinstLib, static_cast<char*>(&full_module_path), MAX_PATH);

   if (hinstLib != NULL) 
   { 
      CreateGameAI = (creategameai_t) GetProcAddress(hinstLib, "CreateGameAI"); 

      assert(CreateGameAI);
   } 
#else
      void *myso = dlopen("libgame_ai.so", RTLD_NOW);
      assert(myso);

      dlinfo(myso, RTLD_DI_ORIGIN, (void *) &game_ai_lib_path);

      CreateGameAI = reinterpret_cast<creategameai_t>(dlsym(myso, "CreateGameAI"));
      assert(CreateGameAI);
#endif
   }
}

//======================================================
// GameAIManager::Load
//======================================================
void GameAIManager::Load(const char *name, void *ram_ptr, int ram_size, retro_log_printf_t log)
{
   g_game_name = name;

   g_ram_ptr = ram_ptr;
   g_ram_size = ram_size;

   g_log = log;
}

//======================================================
// GameAIManager::Think
//======================================================
void GameAIManager::Think(bool override_p1, bool override_p2, bool show_debug, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format)
{
   if (ga)
      ga->SetShowDebug(show_debug);

   if (ga == nullptr && g_ram_ptr != nullptr)
   {
      ga = CreateGameAI(g_game_name.c_str());
      assert(ga);

      std::string data_path((char *)game_ai_lib_path);
      data_path += "/data/";
      data_path += g_game_name;

      ga->Init((void *) g_ram_ptr, g_ram_size);

      ga->SetDebugLog(game_ai_debug_log);
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

//======================================================
// Interface to RA
//======================================================
extern "C" signed short int game_ai_input(unsigned int port, unsigned int device, unsigned int idx, unsigned int id, signed short int result)
{
   return gameAIMgr.Input(port,device, idx, id, result);
}

extern "C" void game_ai_init()
{
   gameAIMgr.Init();
}

extern "C" void game_ai_load(const char * name, void * ram_ptr, int ram_size, retro_log_printf_t log)
{
   gameAIMgr.Load(name, ram_ptr, ram_size, log);
}

extern "C" void game_ai_think(bool override_p1, bool override_p2, bool show_debug, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format) 
{
   gameAIMgr.Think(override_p1, override_p2, show_debug, frame_data, frame_width, frame_height, frame_pitch, pixel_format);
}

