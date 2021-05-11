/**************************************************************

   edid.c - Basic EDID generation
   (based on edid.S: EDID data template by Carsten Emde)

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <stdio.h>
#include <string.h>
#include "switchres.h"
#include "edid.h"

//============================================================
//  edid_from_modeline
//============================================================

int edid_from_modeline(modeline *mode, monitor_range *range, char *name, edid_block *edid)
{
	if (!edid) return 0;

	// header
	edid->b[0] = 0x00;
	edid->b[1] = 0xff;
	edid->b[2] = 0xff;
	edid->b[3] = 0xff;
	edid->b[4] = 0xff;
	edid->b[5] = 0xff;
	edid->b[6] = 0xff;
	edid->b[7] = 0x00;

	// Manufacturer ID = "SWR"
	edid->b[8] = 0x4e;
	edid->b[9] = 0xf2;

	// Manufacturer product code
	edid->b[10] = 0x00;
	edid->b[11] = 0x00;

	// Serial number
	edid->b[12] = 0x00;
	edid->b[13] = 0x00;
	edid->b[14] = 0x00;
	edid->b[15] = 0x00;

	// Week of manufacture
	edid->b[16] = 5;

	// Year of manufacture
	edid->b[17] = 2021 - 1990;

	// EDID version and revision
	edid->b[18] = 1;
	edid->b[19] = 3;

	// video params
	edid->b[20] = 0x6d;

	// Maximum H & V size in cm
	edid->b[21] = 48;
	edid->b[22] = 36;

	// Gamma
	edid->b[23] = 120;

	// Display features
	edid->b[24] = 0x0A;

	// Chromacity coordinates;
	edid->b[25] = 0x5e;
	edid->b[26] = 0xc0;
	edid->b[27] = 0xa4;
	edid->b[28] = 0x59;
	edid->b[29] = 0x4a;
	edid->b[30] = 0x98;
	edid->b[31] = 0x25;
	edid->b[32] = 0x20;
	edid->b[33] = 0x50;
	edid->b[34] = 0x54;

	// Established timings
	edid->b[35] = 0x00;
	edid->b[36] = 0x00;
	edid->b[37] = 0x00;

	// Standard timing information
	edid->b[38] = 0x01;
	edid->b[39] = 0x01;
	edid->b[40] = 0x01;
	edid->b[41] = 0x01;
	edid->b[42] = 0x01;
	edid->b[43] = 0x01;
	edid->b[44] = 0x01;
	edid->b[45] = 0x01;
	edid->b[46] = 0x01;
	edid->b[47] = 0x01;
	edid->b[48] = 0x01;
	edid->b[49] = 0x01;
	edid->b[50] = 0x01;
	edid->b[51] = 0x01;
	edid->b[52] = 0x01;
	edid->b[53] = 0x01;

	// Pixel clock in 10 kHz units. (0.-655.35 MHz, little-endian)
	edid->b[54] = (mode->pclock / 10000) & 0xff;
	edid->b[55] = (mode->pclock / 10000) >> 8;

	int h_active = mode->hactive;
	int h_blank = mode->htotal - mode->hactive;
	int h_offset = mode->hbegin - mode->hactive;
	int h_pulse = mode->hend - mode->hbegin;

	int v_active = mode->vactive;
	int v_blank = (int)mode->vtotal - mode->vactive;
	int v_offset = mode->vbegin - mode->vactive;
	int v_pulse = mode->vend - mode->vbegin;

	// Horizontal active pixels 8 lsbits (0-4095)
	edid->b[56] = h_active & 0xff;

	// Horizontal blanking pixels 8 lsbits (0-4095)
	edid->b[57] = h_blank & 0xff;

	// Bits 7-4 Horizontal active pixels 4 msbits
	// Bits 3-0 Horizontal blanking pixels 4 msbits
	edid->b[58] = (((h_active >> 8) & 0x0f) << 4) + ((h_blank >> 8) & 0x0f);

	// Vertical active lines 8 lsbits (0-4095)
	edid->b[59] = v_active & 0xff;

	// Vertical blanking lines 8 lsbits (0-4095)
	edid->b[60] = v_blank & 0xff;

	// Bits 7-4 Vertical active lines 4 msbits
	// Bits 3-0 Vertical blanking lines 4 msbits
	edid->b[61] = (((v_active >> 8) & 0x0f) << 4) + ((v_blank >> 8) & 0x0f);

	// Horizontal sync offset pixels 8 lsbits (0-1023) From blanking start
	edid->b[62] = h_offset & 0xff;

	// Horizontal sync pulse width pixels 8 lsbits (0-1023)
	edid->b[63] = h_pulse & 0xff;

	// Bits 7-4 Vertical sync offset lines 4 lsbits 0-63)
	// Bits 3-0 Vertical sync pulse width lines 4 lsbits 0-63)
	edid->b[64] = ((v_offset & 0x0f) << 4) + (v_pulse & 0x0f);

	// Bits 7-6     Horizontal sync offset pixels 2 msbits
	// Bits 5-4     Horizontal sync pulse width pixels 2 msbits
	// Bits 3-2     Vertical sync offset lines 2 msbits
	// Bits 1-0     Vertical sync pulse width lines 2 msbits
	edid->b[65] = (((h_offset >> 8) & 0x03) << 6) +
			   (((h_pulse >> 8) & 0x03) << 4) +
			   (((v_offset >> 8) & 0x03) << 2) +
			   ((v_pulse >> 8) & 0x03);

	// Horizontal display size, mm, 8 lsbits (0-4095 mm, 161 in)
	edid->b[66] = 485 & 0xff;

	// Vertical display size, mm, 8 lsbits (0-4095 mm, 161 in)
	edid->b[67] = 364 & 0xff;

	// Bits 7-4 Horizontal display size, mm, 4 msbits
	// Bits 3-0 Vertical display size, mm, 4 msbits
	edid->b[68] = (((485 >> 8) & 0x0f) << 4) + ((364 >> 8) & 0x0f);

	// Horizontal border pixels (each side; total is twice this)
	edid->b[69] = 0;

	// Vertical border lines (each side; total is twice this)
	edid->b[70] = 0;

	// Features bitmap
	edid->b[71] = ((mode->interlace & 0x01) << 7) + 0x18 + (mode->vsync << 2) + (mode->hsync << 2);


	// Descriptor: monitor serial number
	edid->b[72] = 0;
	edid->b[73] = 0;
	edid->b[74] = 0;
	edid->b[75] = 0xff;
	edid->b[76] = 0;
	edid->b[77] = 'S';
	edid->b[78] = 'w';
	edid->b[79] = 'i';
	edid->b[80] = 't';
	edid->b[81] = 'c';
	edid->b[82] = 'h';
	edid->b[83] = 'r';
	edid->b[84] = 'e';
	edid->b[85] = 's';
	edid->b[86] = '2';
	edid->b[87] = '0';
	edid->b[88] = '0';
	edid->b[89] = 0x0a;

	// Descriptor: monitor range limits
	edid->b[90] = 0;
	edid->b[91] = 0;
	edid->b[92] = 0;
	edid->b[93] = 0xfd;
	edid->b[94] = 0;
	edid->b[95] = ((int)range->vfreq_min) & 0xff;
	edid->b[96] = ((int)range->vfreq_max) & 0xff;
	edid->b[97] = ((int)range->hfreq_min / 1000) & 0xff;
	edid->b[98] = ((int)range->hfreq_max / 1000) & 0xff;
	edid->b[99] = 0xff;
	edid->b[100] = 0;
	edid->b[101] = 0x0a;
	edid->b[102] = 0x20;
	edid->b[103] = 0x20;
	edid->b[104] = 0x20;
	edid->b[105] = 0x20;
	edid->b[106] = 0x20;
	edid->b[107] = 0x20;

	// Descriptor: text
	edid->b[108] = 0;
	edid->b[109] = 0;
	edid->b[110] = 0;
	edid->b[111] = 0xfc;
	edid->b[112] = 0;
	snprintf(&edid->b[113], 13, "%s", name);
	edid->b[125] = 0x0a;

	// Extensions to follow
	edid->b[126] = 0;

	// Compute checksum
	char checksum = 0;
	int i;
	for (i = 0; i <= 126; i++)
		checksum += edid->b[i];
	edid->b[127] = 256 - checksum;

	return 1;
}
