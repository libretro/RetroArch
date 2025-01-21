#include "game_ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <retro_assert.h>

#include "../deps/game_ai_lib/GameAI.h"

#define GAME_AI_MAX_PLAYERS 2

void *                     ga = NULL;
volatile void *            g_ram_ptr = NULL;
volatile int               g_ram_size = 0;
volatile signed short int  g_buttons_bits[GAME_AI_MAX_PLAYERS] = {0};
volatile int               g_frameCount = 0;
volatile char              game_ai_lib_path[1024] = {0};
volatile char              g_game_name[1024] = {0};
retro_log_printf_t         g_log = NULL;

/* GameAI Lib API*/
create_game_ai_t              create_game_ai = NULL;
game_ai_lib_init_t            game_ai_lib_init = NULL;
game_ai_lib_think_t           game_ai_lib_think = NULL;
game_ai_lib_set_show_debug_t  game_ai_lib_set_show_debug = NULL;
game_ai_lib_set_debug_log_t   game_ai_lib_set_debug_log = NULL;

/* Helper functions */
void game_ai_debug_log(int level, const char *fmt, ...)
{
   va_list vp;
   va_start(vp, fmt);

   if (g_log)
   {
      g_log((enum retro_log_level)level, fmt, vp);
   }

   va_end(vp);
}

void array_to_bits_16(volatile signed short * result, const bool b[16])
{
   for (int bit = 0; bit <= 15; bit++)
   {
      *result |= b[bit] ? (1 << bit) : 0;
   }
}

/* Interface to RA */

extern signed short int game_ai_input(unsigned int port, unsigned int device, unsigned int idx, unsigned int id, signed short int result)
{
   if (ga == NULL)
      return 0;

   if (port < GAME_AI_MAX_PLAYERS)
      return g_buttons_bits[port];

   return 0;
}

extern void game_ai_init()
{
   if (create_game_ai == NULL)
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
      create_game_ai = (create_game_ai_t) GetProcAddress(hinstLib, "create_game_ai");
      retro_assert(create_game_ai);

      game_ai_lib_init = (game_ai_lib_init_t) GetProcAddress(hinstLib, "game_ai_lib_init");
      retro_assert(game_ai_lib_init);

      game_ai_lib_think = (game_ai_lib_think_t) GetProcAddress(hinstLib, "game_ai_lib_think");
      retro_assert(game_ai_lib_think);

      game_ai_lib_set_show_debug = (game_ai_lib_set_show_debug_t) GetProcAddress(hinstLib, "game_ai_lib_set_show_debug");
      retro_assert(game_ai_lib_set_show_debug);

      game_ai_lib_set_debug_log = (game_ai_lib_set_debug_log_t) GetProcAddress(hinstLib, "game_ai_lib_set_debug_log");
      retro_assert(game_ai_lib_set_debug_log);
   }
#else
      void *myso = dlopen("libgame_ai.so", RTLD_NOW);
      retro_assert(myso);

      if(myso != NULL)
      {
         dlinfo(myso, RTLD_DI_ORIGIN, (void *) &game_ai_lib_path);

         create_game_ai = (create_game_ai_t)(dlsym(myso, "create_game_ai"));
         retro_assert(create_game_ai);

         game_ai_lib_init = (game_ai_lib_init_t)(dlsym(myso, "game_ai_lib_init"));
         retro_assert(game_ai_lib_init);

         game_ai_lib_think = (game_ai_lib_think_t)(dlsym(myso, "game_ai_lib_think"));
         retro_assert(game_ai_lib_think);

         game_ai_lib_set_show_debug = (game_ai_lib_set_show_debug_t)(dlsym(myso, "game_ai_lib_set_show_debug"));
         retro_assert(game_ai_lib_set_show_debug);

         game_ai_lib_set_debug_log  = (game_ai_lib_set_debug_log_t)(dlsym(myso, "game_ai_lib_set_debug_log"));
         retro_assert(game_ai_lib_set_debug_log);
      }
#endif
   }
}

extern void game_ai_load(const char * name, void * ram_ptr, int ram_size, retro_log_printf_t log)
{
   strcpy((char *) &g_game_name[0], name);

   g_ram_ptr = ram_ptr;
   g_ram_size = ram_size;

   g_log = log;
}

extern void game_ai_think(bool override_p1, bool override_p2, bool show_debug, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format)
{
   if (ga)
      game_ai_lib_set_show_debug(ga, show_debug);

   if (ga == NULL && g_ram_ptr != NULL)
   {
      ga = create_game_ai((char *) &g_game_name[0]);
      retro_assert(ga);

      if (ga)
      {
         char data_path[1024] = {0};
         strcpy(&data_path[0], (char *)game_ai_lib_path);
         strcat(&data_path[0], "/data/");
         strcat(&data_path[0], (char *)g_game_name);

         game_ai_lib_init(ga, (void *) g_ram_ptr, g_ram_size);
         game_ai_lib_set_debug_log(ga, game_ai_debug_log);
      }
   }

   if (g_frameCount >= (GAMEAI_SKIPFRAMES - 1))
   {
      if (ga)
      {
         bool b[GAMEAI_MAX_BUTTONS] = {0};

         g_buttons_bits[0]=0;
         g_buttons_bits[1]=0;

         if (override_p1)
         {
            game_ai_lib_think(ga, b, 0, frame_data, frame_width, frame_height, frame_pitch, pixel_format);
            array_to_bits_16(&g_buttons_bits[0], b);
         }

         if (override_p2)
         {
            game_ai_lib_think(ga, b, 1, frame_data, frame_width, frame_height, frame_pitch, pixel_format);
            array_to_bits_16(&g_buttons_bits[1], b);
         }
      }
      g_frameCount=0;
   }
   else
   {
      g_frameCount++;
   }
}
