#include <stdio.h>

#include <string.h>
#include <fat.h>
#include <gctypes.h>
#include <ogc/cache.h>
#include <ogc/lwp_threads.h>
#include <ogc/system.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>

#define EXECUTE_ADDR ((uint8_t *) 0x91800000)
#define BOOTER_ADDR ((uint8_t *) 0x93000000)
#define ARGS_ADDR ((uint8_t *) 0x93200000)

extern uint8_t _binary_wii_app_booter_app_booter_bin_start[];
extern uint8_t _binary_wii_app_booter_app_booter_bin_end[];
#define booter_start _binary_wii_app_booter_app_booter_bin_start
#define booter_end _binary_wii_app_booter_app_booter_bin_end

#include "../../retroarch_logger.h"
#include "../../file.h"

#ifdef IS_SALAMANDER
char gx_rom_path[PATH_MAX];
#endif

static void dol_copy_argv_path(const char *dolpath, const char *argpath)
{
   char tmp[PATH_MAX];
   size_t len, t_len;
   struct __argv *argv = (struct __argv *) ARGS_ADDR;
   memset(ARGS_ADDR, 0, sizeof(struct __argv));
   char *cmdline = (char *) ARGS_ADDR + sizeof(struct __argv);
   argv->argvMagic = ARGV_MAGIC;
   argv->commandLine = cmdline;
   len = 0;

   // a device-less fullpath
   if (dolpath[0] == '/')
   {
      char *dev = strchr(__system_argv->argv[0], ':');
      t_len = dev - __system_argv->argv[0] + 1;
      memcpy(cmdline, __system_argv->argv[0], t_len);
      len += t_len;
   }
   // a relative path
   else if (strstr(dolpath, "sd:/") != dolpath && strstr(dolpath, "usb:/") != dolpath &&
       strstr(dolpath, "carda:/") != dolpath && strstr(dolpath, "cardb:/") != dolpath)
   {
      fill_pathname_parent_dir(tmp, __system_argv->argv[0], sizeof(tmp));
      t_len = strlen(tmp);
      memcpy(cmdline, tmp, t_len);
      len += t_len;
   }

   t_len = strlen(dolpath);
   memcpy(cmdline + len, dolpath, t_len);
   len += t_len;
   cmdline[len++] = 0;

   // file must be split into two parts, the path and the actual filename
   // done to be compatible with loaders
   if (argpath && strrchr(argpath, '/') != NULL)
   {
      // basedir
      fill_pathname_parent_dir(tmp, argpath, sizeof(tmp));
      t_len = strlen(tmp);
      memcpy(cmdline + len, tmp, t_len);
      len += t_len;
      cmdline[len++] = 0;

      // filename
      char *name = strrchr(argpath, '/') + 1;
      t_len = strlen(name);
      memcpy(cmdline + len, name, t_len);
      len += t_len;
      cmdline[len++] = 0;
   }

   cmdline[len++] = 0;
   argv->length = len;
   DCFlushRange(ARGS_ADDR, sizeof(struct __argv) + argv->length);
}

// WARNING: after we move any data into EXECUTE_ADDR, we can no longer use any
// heap memory and are restricted to the stack only
void system_exec_wii(const char *path, bool should_load_game)
{
   char game_path[PATH_MAX];

   RARCH_LOG("Attempt to load executable: [%s] %d.\n", path, sizeof(game_path));

   // copy heap info into stack so it survives us moving the .dol into MEM2
   if (should_load_game)
   {
#ifdef IS_SALAMANDER
      strlcpy(game_path, gx_rom_path, sizeof(game_path));
#else
      strlcpy(game_path, g_extern.fullpath, sizeof(game_path));
#endif
   }

   FILE * fp = fopen(path, "rb");
   if (fp == NULL)
   {
      RARCH_ERR("Could not open DOL file %s.\n", path);
      return;
   }

   fseek(fp, 0, SEEK_END);
   size_t size = ftell(fp);
   fseek(fp, 0, SEEK_SET);

   // try to allocate a buffer for it. if we can't, fail
   void *dol = malloc(size);
   if (!dol)
   {
      RARCH_ERR("Could not execute DOL file %s.\n", path);
      fclose(fp);
      return;
   }

   fread(dol, 1, size, fp);
   fclose(fp);

   fatUnmount("carda:");
   fatUnmount("cardb:");
   fatUnmount("sd:");
   fatUnmount("usb:");
   __io_wiisd.shutdown();
   __io_usbstorage.shutdown();

   // luckily for us, newlib's memmove doesn't allocate a seperate buffer for
   // copying in situations of overlap, so it's safe to do this
   memmove(EXECUTE_ADDR, dol, size);
   DCFlushRange(EXECUTE_ADDR, size);

   dol_copy_argv_path(path, should_load_game ? game_path : NULL);

   size_t booter_size = booter_end - booter_start;
   memcpy(BOOTER_ADDR, booter_start, booter_size);
   DCFlushRange(BOOTER_ADDR, booter_size);

   RARCH_LOG("jumping to %08x\n", (unsigned) BOOTER_ADDR);
   SYS_ResetSystem(SYS_SHUTDOWN,0,0);
   __lwp_thread_stopmultitasking((void (*)(void)) BOOTER_ADDR);
}
