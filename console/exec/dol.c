// this code was contributed by shagkur of the devkitpro team, thx!

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <gccore.h>
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

uint32_t load_dol_image (void *dolstart)
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

      return dolfile->entry_point;
   }

   return 0;
}

