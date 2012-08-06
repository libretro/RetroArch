// this code was contributed by shagkur of the devkitpro team, thx!

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <gccore.h>
#include <gctypes.h>
#include <ogcsys.h>

#include "../../retroarch_logger.h"

typedef struct _dolheader
{
   uint32_t text_pos[7];
   uint32_t data_pos[11];
   uint32_t text_start[7];
   uint32_t data_start[11];
   uint32_t text_size[7];
   uint32_t data_size[11];
   uint32_t bss_start;
   uint32_t bss_size;
   uint32_t entry_point;
} dolheader;

static char dol_argv_commandline[1024];

uint32_t *load_dol_image (void *dolstart)
{
   uint32_t i;
   dolheader *dolfile;

   if(dolstart)
   {
      dolfile = (dolheader *) dolstart;
      for(i = 0; i < 7; i++)
      {
         if ((!dolfile->text_size[i]) || (dolfile->text_start[i] < 0x100))
            continue;

         RARCH_LOG("loading text section %u @ 0x%08x (0x%08x bytes)\n",
         i, dolfile->text_start[i], dolfile->text_size[i]);

         ICInvalidateRange ((void *) dolfile->text_start[i],
         dolfile->text_size[i]);

         memmove ((void *) dolfile->text_start[i], dolstart+dolfile->text_pos[i],
         dolfile->text_size[i]);
      }

      for(i = 0; i < 11; i++)
      {
         if ((!dolfile->data_size[i]) || (dolfile->data_start[i] < 0x100))
            continue;

         RARCH_LOG("loading data section %u @ 0x%08x (0x%08x bytes)\n", i,
         dolfile->data_start[i], dolfile->data_size[i]);

         memmove ((void*) dolfile->data_start[i], dolstart+dolfile->data_pos[i],
         dolfile->data_size[i]);

         DCFlushRangeNoSync ((void *) dolfile->data_start[i], dolfile->data_size[i]);
      }

      RARCH_LOG("clearing bss\n");

      memset ((void *) dolfile->bss_start, 0, dolfile->bss_size);
      DCFlushRange((void *) dolfile->bss_start, dolfile->bss_size);

      return (uint32_t *) dolfile->entry_point;
   }

   return NULL;
}

// NOTE: this does not update the path to point to the new loading .dol file.
// we only ues it for keeping the current path anyway.
void dol_copy_argv(struct __argv *argv)
{
   memcpy(dol_argv_commandline, __system_argv->commandLine, __system_argv->length);
   DCFlushRange((void *) dol_argv_commandline, sizeof(dol_argv_commandline));
   argv->argvMagic = ARGV_MAGIC;
   argv->commandLine = dol_argv_commandline;
   argv->length = __system_argv->length;
   DCFlushRange((void *) argv, sizeof(struct __argv));
}
