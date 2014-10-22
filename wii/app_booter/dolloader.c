#include <gctypes.h>
#include "string.h"
#include "dolloader.h"
#include "sync.h"

#define ARENA1_HI_LIMIT 0x81800000

typedef struct _dolheade
{
   u32 text_pos[7];
   u32 data_pos[11];
   u32 text_start[7];
   u32 data_start[11];
   u32 text_size[7];
   u32 data_size[11];
   u32 bss_start;
   u32 bss_size;
   u32 entry_point;
} dolheader;

u32 load_dol_image(const void *dolstart)
{
	u32 i;
   dolheader *dolfile = NULL;
	if(!dolstart)
		return 0;

	dolfile = (dolheader *) dolstart;

	for (i = 0; i < 7; i++)
	{
		if ((!dolfile->text_size[i]) || (dolfile->text_start[i] < 0x100))
			continue;

		memcpy((void *) dolfile->text_start[i], dolstart + dolfile->text_pos[i], dolfile->text_size[i]);
		sync_before_exec((void *) dolfile->text_start[i], dolfile->text_size[i]);
	}

	for (i = 0; i < 11; i++)
	{
		if ((!dolfile->data_size[i]) || (dolfile->data_start[i] < 0x100))
			continue;

		memcpy((void *) dolfile->data_start[i], dolstart + dolfile->data_pos[i], dolfile->data_size[i]);
		sync_before_exec((void *) dolfile->data_start[i], dolfile->data_size[i]);
	}

	return dolfile->entry_point;
}
