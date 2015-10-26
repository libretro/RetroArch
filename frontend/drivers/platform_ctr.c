/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Ali Bouhlel
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>

#include <3ds.h>

#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <file/file_list.h>
#endif

#include "../../general.h"
#include "retroarch.h"

#ifdef IS_SALAMANDER
#include "../../file_ext.h"
#else
#include "../../menu/menu.h"
#endif

const char* elf_path_cst = "sdmc:/retroarch/test.3dsx";

#ifndef DEBUG_HOLD
void wait_for_input(void);
#define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0)
#define DEBUG_VAR(X) printf( "%-20s: 0x%08X\n", #X, (u32)(X))
#define DEBUG_VAR64(X) printf( #X"\r\t\t\t\t : 0x%016llX\n", (u64)(X))
#endif

#define CTR_APPMEMALLOC_PTR ((u32*)0x1FF80040)

extern char* fake_heap_start;
extern char* fake_heap_end;
u32 __linear_heap;
u32 __heapBase;
extern u32 __linear_heap_size;
extern u32 __heap_size;
extern u32 __linear_heap_size_hbl;
extern u32 __heap_size_hbl;

extern void (*__system_retAddr)(void);

void __destroy_handle_list(void);
void __appExit();
void __libc_fini_array(void);

void __system_allocateHeaps() {
   extern unsigned int __service_ptr;
   u32 tmp=0;
   int64_t mem_used;
   u32 mem_available;
   u32 app_memory;

   svcGetSystemInfo(&mem_used, 0, 1);

   if(__service_ptr)
   {
      app_memory = mem_used + __linear_heap_size_hbl + __heap_size_hbl;
      app_memory = app_memory < 0x04000000 ? 0x04000000 : app_memory;
   }
   else
      app_memory = *CTR_APPMEMALLOC_PTR;

   mem_available = (app_memory - mem_used - __linear_heap_size - 0x10000) & 0xFFFFF000;

   __heap_size = __heap_size > mem_available ?  mem_available : __heap_size;
   __heap_size = __heap_size > 0x6000000 ? 0x6000000 : __heap_size;

//   __linear_heap_size = (app_memory - mem_used - __heap_size - 0x10000) & 0xFFFFF000;

	// Allocate the application heap
	__heapBase = 0x08000000;
	svcControlMemory(&tmp, __heapBase, 0x0, __heap_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);

	// Allocate the linear heap
	svcControlMemory(&__linear_heap, 0x0, 0x0, __linear_heap_size, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);
	// Set up newlib heap
	fake_heap_start = (char*)__heapBase;
	fake_heap_end = fake_heap_start + __heap_size;
}

void __attribute__((noreturn)) __libctru_exit(int rc)
{
	u32 tmp=0;

	// Unmap the linear heap
	svcControlMemory(&tmp, __linear_heap, 0x0, __linear_heap_size, MEMOP_FREE, 0x0);

	// Unmap the application heap
	svcControlMemory(&tmp, __heapBase, 0x0, __heap_size, MEMOP_FREE, 0x0);

	// Close some handles
	__destroy_handle_list();

	// Jump to the loader if it provided a callback
	if (__system_retAddr)
		__system_retAddr();

	// Since above did not jump, end this process
	svcExitProcess();
}



static void frontend_ctr_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   (void)args;

#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   global_t *global  = global_get_ptr();
   global->log_file = fopen("sdmc:/retroarch/retroarch-log.txt", "w");
#endif
#endif

   fill_pathname_basedir(g_defaults.dir.port, elf_path_cst, sizeof(g_defaults.dir.port));
   RARCH_LOG("port dir: [%s]\n", g_defaults.dir.port);

   fill_pathname_join(g_defaults.dir.core_assets, g_defaults.dir.port,
         "downloads", sizeof(g_defaults.dir.core_assets));
   fill_pathname_join(g_defaults.dir.assets, g_defaults.dir.port,
         "media", sizeof(g_defaults.dir.assets));
   fill_pathname_join(g_defaults.dir.core, g_defaults.dir.port,
         "cores", sizeof(g_defaults.dir.core));
   fill_pathname_join(g_defaults.dir.core_info, g_defaults.dir.port,
         "cores", sizeof(g_defaults.dir.core_info));
   fill_pathname_join(g_defaults.dir.savestate, g_defaults.dir.core,
         "savestates", sizeof(g_defaults.dir.savestate));
   fill_pathname_join(g_defaults.dir.sram, g_defaults.dir.core,
         "savefiles", sizeof(g_defaults.dir.sram));
   fill_pathname_join(g_defaults.dir.system, g_defaults.dir.core,
         "system", sizeof(g_defaults.dir.system));
   fill_pathname_join(g_defaults.dir.playlist, g_defaults.dir.core,
         "playlists", sizeof(g_defaults.dir.playlist));
   fill_pathname_join(g_defaults.dir.remap, g_defaults.dir.port,
         "remaps", sizeof(g_defaults.dir.remap));
   fill_pathname_join(g_defaults.path.config, g_defaults.dir.port,
         "retroarch.cfg", sizeof(g_defaults.path.config));
   
   *argc = 0;

#ifndef IS_SALAMANDER
#if 0
   if (argv[1] && (argv[1][0] != '\0'))
   {
      static char path[PATH_MAX_LENGTH];
      struct rarch_main_wrap *args = NULL;

      *path = '\0';
      args = (struct rarch_main_wrap*)params_data;

      if (args)
      {
         strlcpy(path, argv[1], sizeof(path));

         args->touched        = true;
         args->no_content     = false;
         args->verbose        = false;
         args->config_path    = NULL;
         args->sram_path      = NULL;
         args->state_path     = NULL;
         args->content_path   = path;
         args->libretro_path  = NULL;

         RARCH_LOG("argv[0]: %s\n", argv[0]);
         RARCH_LOG("argv[1]: %s\n", argv[1]);
         RARCH_LOG("argv[2]: %s\n", argv[2]);

         RARCH_LOG("Auto-start game %s.\n", argv[1]);
      }
   }
#endif
#endif
}

static void frontend_ctr_deinit(void *data)
{
   extern PrintConsole* currentConsole;
   Handle lcd_handle;
   u8 not_2DS;
   (void)data;
#ifndef IS_SALAMANDER
   global_t *global   = global_get_ptr();
   global->verbosity = true;

#ifdef HAVE_FILE_LOGGER
   if (global->log_file)
      fclose(global->log_file);
   global->log_file = NULL;
#endif

   if(gfxBottomFramebuffers[0] == (u8*)currentConsole->frameBuffer)
      wait_for_input();

   CFGU_GetModelNintendo2DS(&not_2DS);
   if(not_2DS && srvGetServiceHandle(&lcd_handle, "gsp::Lcd") >= 0)
   {
      u32 *cmdbuf = getThreadCommandBuffer();
      cmdbuf[0] = 0x00110040;
      cmdbuf[1] = 2;
      svcSendSyncRequest(lcd_handle);
      svcCloseHandle(lcd_handle);
   }

   exitCfgu();
   csndExit();
   gfxExit();

#if 0
   sdmcExit();
   fsExit();
   hidExit();
   aptExit();
   srvExit();
#endif
#endif
}

static void frontend_ctr_shutdown(bool unused)
{
   (void)unused;
}

#define PRINTFPOS(X,Y) "\x1b["#X";"#Y"H"
#define PRINTFPOS_STR(X,Y) "\x1b["X";"Y"H"

static void frontend_ctr_init(void *data)
{
#ifndef IS_SALAMANDER
   (void)data;
   global_t *global   = global_get_ptr();
   global->verbosity = true;

#if 0
   srvInit();
   aptInit();
   hidInit();
   fsInit();
   sdmcInit();

   APT_SetAppCpuTimeLimit(NULL, 80);
   gfxInitDefault();
#endif
   osSetSpeedupEnable(true);
   gfxInit(GSP_BGR8_OES,GSP_RGB565_OES,false);
   csndInit();
   initCfgu();
   gfxSet3D(false);
   consoleInit(GFX_BOTTOM, NULL);
#endif
}


static int frontend_ctr_get_rating(void)
{
   return 3;
}

bool select_pressed = false;

void wait_for_input(void)
{
   printf("\n\nPress Start.\n\n");
   fflush(stdout);

   while(aptMainLoop())
   {
      u32 kDown;

      hidScanInput();

      kDown = hidKeysDown();

      if (kDown & KEY_START)
         break;

      if (kDown & KEY_SELECT)
         exit(0);
#if 0
      select_pressed = true;
#endif

      retro_sleep(1);
   }
}

int usleep (useconds_t us)
{
   svcSleepThread((int64_t)us * 1000);
}

long sysconf(int name)
{
   switch(name)
   {
   case _SC_NPROCESSORS_ONLN:
      return 2;
   }

   return -1;
}


enum frontend_architecture frontend_ctr_get_architecture(void)
{
   return FRONTEND_ARCH_ARM;
}

static int frontend_ctr_parse_drive_list(void *data)
{
   file_list_t *list = (file_list_t*)data;

#ifndef IS_SALAMANDER
   if (!list)
      return -1;

   menu_entries_push(list,
         "sdmc:/", "", MENU_FILE_DIRECTORY, 0, 0);
#endif

   return 0;
}

frontend_ctx_driver_t frontend_ctx_ctr = {
   frontend_ctr_get_environment_settings,
   frontend_ctr_init,
   frontend_ctr_deinit,
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_ctr_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_ctr_get_rating,
   NULL,                         /* load_content */
   frontend_ctr_get_architecture,
   NULL,                         /* get_powerstate */
   frontend_ctr_parse_drive_list,
   "ctr",
};
