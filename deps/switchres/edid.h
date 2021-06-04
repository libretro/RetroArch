/**************************************************************

   edid.h - Basic EDID generation
   (based on edid.S: EDID data template by Carsten Emde)

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#ifndef __EDID_H__
#define __EDID_H__

//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef struct edid_block
{
	char b[128];
/*  char ext1[128];
    char ext2[128];
    char ext3[128];*/
} edid_block;

//============================================================
//  PROTOTYPES
//============================================================

int edid_from_modeline(modeline *mode, monitor_range *range, char *name, edid_block *edid);

#endif
