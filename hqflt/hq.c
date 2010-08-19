/* This source code is Copyright (C) 2006-2009 by WolfWings. */
/*      It is, however, released under the ISC license.      */
/*             en.wikipedia.org/wiki/ISC_Licence             */
/* --------------------------------------------------------- */
/* It is a formula-level rederiviation of the HQ2x technique */
/* and is, thus, not a derivative work of the original code, */
/* only the original equations behind the code.              */

#include <stdlib.h>
#include <string.h>

#include "pastlib.h"

static const uint8_t blends_2x[14*4] = {
/*	 5  2  4  1 */

	 8, 4, 4, 0,
	 8, 4, 0, 4,
	 8, 0, 4, 4,
	16, 0, 0, 0,
	12, 4, 0, 0,
	12, 0, 4, 0,
	12, 0, 0, 4,

	 4, 6, 6, 0, /* Interp9 */
	12, 2, 2, 0, /* Interp7 */
	14, 1, 1, 0, /* Interp10*/
	10, 4, 2, 0, /* Interp6 */
	10, 2, 4, 0, /* Interp6 */
	 4, 6, 6, 0, /* Added for secondary blending used in HQ4x */
	 8, 4, 4, 0, /* Added for secondary blending used in HQ4x */
}; 	

static const uint8_t blends_4x[14*16] = {
/*	 5  2  4  1	 5  2  4  1	 5  2  4  1	 5  2  4  1 */

	 8, 4, 4, 0,	10, 4, 2, 0,	10, 2, 4, 0,	12, 2, 2, 0, /* Needs to be split. B */
	10, 0, 0, 6,	10, 4, 0, 2,	12, 0, 0, 4,	14, 0, 0, 2,
	10, 0, 0, 6,	10, 0, 4, 2,	12, 0, 0, 4,	14, 0, 0, 2,
	16, 0, 0, 0,	16, 0, 0, 0,	16, 0, 0, 0,	16, 0, 0, 0, /* Solid colors */
	10, 6, 0, 0,	10, 6, 0, 0,	14, 2, 0, 0,	14, 2, 0, 0,
	10, 0, 6, 0,	14, 0, 2, 0,	10, 0, 6, 0,	14, 0, 2, 0,
	10, 0, 0, 6,	12, 0, 0, 4,	12, 0, 0, 4,	14, 0, 0, 2,
	 0, 8, 8, 0,	 4, 8, 4, 0,	 0, 6,10, 0,	12, 2, 2, 0, /* Needs to be split. A */
	 8, 4, 4, 0,	12, 4, 0, 0,	12, 0, 4, 0,	16, 0, 0, 0,
	 8, 4, 4, 0,	16, 0, 0, 0,	16, 0, 0, 0,	16, 0, 0, 0,
	12, 4, 0, 0,	 4,12, 0, 0,	10, 0, 4, 0,	14, 0, 2, 0,
	12, 0, 4, 0,	10, 4, 0, 0,	 4, 0,12, 0,	14, 2, 0, 0,
	 0, 8, 8, 0,	 0,10, 6, 0,	 4, 4, 8, 0,	12, 2, 2, 0, /* Needs to be split. A */
	 0, 8, 8, 0,	 8, 8, 0, 0,	 8, 0, 8, 0,	16, 0, 0, 0, /* Needs to be split. B */
};

static const uint8_t tree_hq[0x800] = {
/*	    6     6     6     6     6     6     6     6
 *	       2  2        2  2        2  2        2  2
 *	             4  4  4  4              4  4  4  4
 *	                         8  8  8  8  8  8  8  8
 */
	 0, 0, 2, 2, 1, 1,13,13, 0, 0, 2, 2, 1, 1,13, 8, /*      */
	 0, 0, 2, 2, 1, 1,12, 6, 0, 0, 2, 2, 1, 1, 8, 8, /* 3    */
	 0, 0, 5,10, 4, 4,13,13, 0, 0, 5, 5,11, 4,13,13, /*  1   */
	 0, 0, 5,10, 4, 4,12,13, 0, 0, 5, 5,11, 4,13,13, /* 31   */
	 0, 0, 2, 2, 1, 1, 7, 8, 0, 0, 2, 2, 1, 1, 6, 8, /*   7  */
	 0, 0, 2, 2, 1, 1, 8, 6, 0, 0, 2, 2, 1, 1, 6, 6, /* 3 7  */
	 0, 0, 5,10, 4, 4, 7,13, 0, 0, 5, 5,11, 4,13,13, /*  17  */
	 0, 0, 5,10, 4, 4, 9, 9, 0, 0, 5,10,11,11, 9, 9, /* 317  */
	 0, 0, 2, 2, 1, 1,13, 8, 0, 0, 2, 2, 1, 1, 8, 8, /*    9 */
	 0, 0, 2, 2, 1, 1,12, 8, 0, 0, 2, 2, 1, 1, 8, 6, /* 3  9 */
	 0, 0, 5, 5, 4, 4,13,13, 0, 0, 5, 5, 4, 4,13,13, /*  1 9 */
	 0, 0, 5, 5, 4, 4,12,13, 0, 0, 5, 5, 4, 4,12,13, /* 31 9 */
	 0, 0, 2, 2, 1, 1, 7, 8, 0, 0, 2, 2, 1, 1, 8, 6, /*   79 */
	 0, 0, 2, 2, 1, 1, 8, 6, 0, 0, 2, 2, 1, 1, 6, 6, /* 3 79 */
	 0, 0, 5, 5, 4, 4, 7, 7, 0, 0, 5, 5, 4, 4,13,13, /*  179 */
	 0, 0, 5, 5, 4, 4, 9, 9, 0, 0, 5, 5, 4, 4, 9, 9, /* 3179 */
	/* DIFF26               */
	 0, 0, 2, 2, 1, 1,13,13, 0, 0, 2, 2, 1, 1,13, 8,
	 0, 0, 2, 2, 1, 1,12, 6, 0, 0, 2, 2, 1, 1, 8, 8,
	 0, 0, 5, 5, 4, 4,13,13, 0, 0, 5, 5,11, 4,13,13,
	 0, 0, 5, 5, 4, 4,12,13, 0, 0, 5, 5,11, 4,13,13,
	 0, 0, 2, 2, 1, 1, 7, 8, 0, 0, 2, 2, 1, 1, 6, 8,
	 0, 0, 2, 2, 1, 1, 8, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 7,13, 0, 0, 5, 5,11, 4,13,13,
	 0, 0, 5, 5, 4, 4, 9, 9, 0, 0, 5, 5,11,11, 9, 9,
	 0, 0, 2, 2, 1, 1,13, 8, 0, 0, 2, 2, 1, 1, 8, 8,
	 0, 0, 2, 2, 1, 1,12, 8, 0, 0, 2, 2, 1, 1, 8, 6,
	 0, 0, 5, 5, 4, 4,13,13, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5, 5, 4, 4,12,13, 0, 0, 5, 5, 4, 4,12,13,
	 0, 0, 2, 2, 1, 1, 7, 8, 0, 0, 2, 2, 1, 1, 8, 6,
	 0, 0, 2, 2, 1, 1, 8, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 7, 7, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5, 5, 4, 4, 9, 9, 0, 0, 5, 5, 4, 4, 9, 9,
	/*        DIFF24        */
	 0, 0, 2, 2, 1, 1, 6, 3, 0, 0, 2, 2, 1, 1, 3, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5,10, 4, 4, 3, 3, 0, 0, 5, 5,11, 4, 3, 3,
	 0, 0, 5,10, 4, 4, 3, 3, 0, 0, 5, 5,11, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5,10, 4, 4, 3, 3, 0, 0, 5, 5,11, 4, 3, 3,
	 0, 0, 5,10, 4, 4, 3, 3, 0, 0, 5,10,11,11, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	/* DIFF26 DIFF24        */
	 0, 0, 2, 2, 1, 1, 6, 3, 0, 0, 2, 2, 1, 1, 3, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5,11, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5,11, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5,11, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5,11,11, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	/*               DIFF84 */
	 0, 0, 2, 2, 1, 1,13,13, 0, 0, 2, 2, 1, 1,13, 8,
	 0, 0, 2, 2, 1, 1,12, 6, 0, 0, 2, 2, 1, 1, 8, 8,
	 0, 0, 5,10, 4, 4,13,13, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5,10, 4, 4,12,13, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 2, 2, 1, 1, 7, 8, 0, 0, 2, 2, 1, 1, 6, 8,
	 0, 0, 2, 2, 1, 1, 8, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5,10, 4, 4, 7,13, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5,10, 4, 4, 9, 9, 0, 0, 5,10, 4, 4, 9, 9,
	 0, 0, 2, 2, 1, 1,13, 8, 0, 0, 2, 2, 1, 1, 8, 8,
	 0, 0, 2, 2, 1, 1,12, 8, 0, 0, 2, 2, 1, 1, 8, 6,
	 0, 0, 5, 5, 4, 4,13,13, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5, 5, 4, 4,12,13, 0, 0, 5, 5, 4, 4,12,13,
	 0, 0, 2, 2, 1, 1, 7, 8, 0, 0, 2, 2, 1, 1, 8, 6,
	 0, 0, 2, 2, 1, 1, 8, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 7, 7, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5, 5, 4, 4, 9, 9, 0, 0, 5, 5, 4, 4, 9, 9,
	/* DIFF26        DIFF84 */
	 0, 0, 2, 2, 1, 1,13,13, 0, 0, 2, 2, 1, 1,13, 8,
	 0, 0, 2, 2, 1, 1,12, 6, 0, 0, 2, 2, 1, 1, 8, 8,
	 0, 0, 5, 5, 4, 4,13,13, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5, 5, 4, 4,12,13, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 2, 2, 1, 1, 7, 8, 0, 0, 2, 2, 1, 1, 6, 8,
	 0, 0, 2, 2, 1, 1, 8, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 7,13, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5, 5, 4, 4, 9, 9, 0, 0, 5, 5, 4, 4, 9, 9,
	 0, 0, 2, 2, 1, 1,13, 8, 0, 0, 2, 2, 1, 1, 8, 8,
	 0, 0, 2, 2, 1, 1,12, 8, 0, 0, 2, 2, 1, 1, 8, 6,
	 0, 0, 5, 5, 4, 4,13,13, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5, 5, 4, 4,12,13, 0, 0, 5, 5, 4, 4,12,13,
	 0, 0, 2, 2, 1, 1, 7, 8, 0, 0, 2, 2, 1, 1, 8, 6,
	 0, 0, 2, 2, 1, 1, 8, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 7, 7, 0, 0, 5, 5, 4, 4,13,13,
	 0, 0, 5, 5, 4, 4, 9, 9, 0, 0, 5, 5, 4, 4, 9, 9,
	/*        DIFF24 DIFF84 */
	 0, 0, 2, 2, 1, 1, 6, 3, 0, 0, 2, 2, 1, 1, 3, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5,10, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5,10, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5,10, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5,10, 4, 4, 3, 3, 0, 0, 5,10, 4, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	/* DIFF26 DIFF24 DIFF84 */
	 0, 0, 2, 2, 1, 1, 6, 3, 0, 0, 2, 2, 1, 1, 3, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 2, 2, 1, 1, 6, 6, 0, 0, 2, 2, 1, 1, 6, 6,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3,
	 0, 0, 5, 5, 4, 4, 3, 3, 0, 0, 5, 5, 4, 4, 3, 3};

#define RB(y,u,v) (((y)&0x3FF)<<22)|(((u)&0x3FF)<<11)|((v)&0x3FF)
#define GO(y,v) ((((y)+64)&0x3FF)<<22)|(16<<11)|(((v)+16)&0x3FF)
static const uint32_t DiffTable[128] = {
RB(-85, 160,  80), RB(-83, 155,  77), RB(-80, 150,  75), RB(-77, 145,  72),
RB(-75, 140,  70), RB(-72, 135,  67), RB(-69, 130,  65), RB(-67, 125,  62),
RB(-64, 120,  60), RB(-61, 115,  57), RB(-59, 110,  55), RB(-56, 105,  52),
RB(-53, 100,  50), RB(-51,  95,  47), RB(-48,  90,  45), RB(-45,  85,  42),
RB(-43,  80,  40), RB(-40,  75,  37), RB(-37,  70,  35), RB(-35,  65,  32),
RB(-32,  60,  30), RB(-29,  55,  27), RB(-27,  50,  25), RB(-24,  45,  22),
RB(-21,  40,  20), RB(-19,  35,  17), RB(-16,  30,  15), RB(-13,  25,  12),
RB(-11,  20,  10), RB( -8,  15,   7), RB( -5,  10,   5), RB( -3,   5,   2),
RB(  0,   0,   0), RB(  2,  -5,  -2), RB(  5, -10,  -5), RB(  7, -15,  -7),
RB( 10, -20, -10), RB( 13, -25, -12), RB( 15, -30, -15), RB( 18, -35, -17),
RB( 21, -40, -20), RB( 23, -45, -22), RB( 26, -50, -25), RB( 29, -55, -27),
RB( 31, -60, -30), RB( 34, -65, -32), RB( 37, -70, -35), RB( 39, -75, -37),
RB( 42, -80, -40), RB( 45, -85, -42), RB( 47, -90, -45), RB( 50, -95, -47),
RB( 53,-100, -50), RB( 55,-105, -52), RB( 58,-110, -55), RB( 61,-115, -57),
RB( 63,-120, -60), RB( 66,-125, -62), RB( 69,-130, -65), RB( 71,-135, -67),
RB( 74,-140, -70), RB( 77,-145, -72), RB( 79,-150, -75), RB( 82,-155, -77),
GO(-84,     -160), GO(-81,     -155), GO(-78,     -150), GO(-76,     -145),
GO(-73,     -140), GO(-70,     -135), GO(-68,     -130), GO(-65,     -125),
GO(-63,     -120), GO(-60,     -115), GO(-57,     -110), GO(-55,     -105),
GO(-52,     -100), GO(-49,      -95), GO(-47,      -90), GO(-44,      -85),
GO(-42,      -80), GO(-39,      -75), GO(-36,      -70), GO(-34,      -65),
GO(-31,      -60), GO(-28,      -55), GO(-26,      -50), GO(-23,      -45),
GO(-21,      -40), GO(-18,      -35), GO(-15,      -30), GO(-13,      -25),
GO(-10,      -20), GO( -7,      -15), GO( -5,      -10), GO( -2,       -5),
GO(  0,        0), GO(  2,        4), GO(  5,        9), GO(  7,       14),
GO( 10,       19), GO( 13,       24), GO( 15,       29), GO( 18,       34),
GO( 21,       39), GO( 23,       44), GO( 26,       49), GO( 28,       54),
GO( 31,       59), GO( 34,       64), GO( 36,       69), GO( 39,       74),
GO( 42,       79), GO( 44,       84), GO( 47,       89), GO( 49,       94),
GO( 52,       99), GO( 55,      104), GO( 57,      109), GO( 60,      114),
GO( 63,      119), GO( 65,      124), GO( 68,      129), GO( 70,      134),
GO( 73,      139), GO( 76,      144), GO( 78,      149), GO( 81,      154)};

inline static int ExpandedDiff (uint32_t c0, uint32_t c1) {
   uint32_t r, g;
   g   = c0;
   g  += 0x4008020;
   g  -= c1;
   r   = DiffTable[(int)((unsigned char)g) +   0];
   r  ^= (0x3FF << 11);
   g >>= 10;
   r  += DiffTable[(int)((unsigned char)g) +   0];
   g >>= 11;
   r  += DiffTable[(int)((unsigned char)g) +  64];
   r  &= 0xE01F03E0;
   return r;
}

#define DIFF56 0x001
#define DIFF52 0x002
#define DIFF54 0x004
#define DIFF58 0x008
#define DIFF53 0x010
#define DIFF51 0x020
#define DIFF57 0x040
#define DIFF59 0x080
#define DIFF26 0x100
#define DIFF24 0x200
#define DIFF84 0x400
#define DIFF86 0x800

/* lastLineDiffs is previous line's 52, 53, 26, all >> 1 */

static uint8_t lastLineDiffs[__PAST_LIBRARY_WIDTH];

#define RotatePattern(x) ((((x) & 0x777) << 1) | (((x) & 0x888) >> 3))

void ProcessHQ2x(const pixel * restrict in, pixel * restrict out) {
	signed int y, x;
	unsigned int pattern, newpattern;
	uint32_t pixels[9];
	int prevline, nextline;

	memset(lastLineDiffs, 0, sizeof(lastLineDiffs));
	prevline = 1;
	nextline = 1 + __PAST_LIBRARY_WIDTH;
	y = __PAST_LIBRARY_HEIGHT - 1;

	do {
		pixels[1-1] =
		pixels[2-1] = RGBUnpack(in[prevline-1]); /* Pixel 2 */
		pixels[3-1] = RGBUnpack(in[prevline  ]); /* Pixel 3 */

		pixels[4-1] =
		pixels[5-1] = RGBUnpack(in[         0]); /* Pixel 5 */
		pixels[6-1] = RGBUnpack(in[         1]); /* Pixel 6 */

		pixels[7-1] =
		pixels[8-1] = RGBUnpack(in[nextline-1]); /* Pixel 8 */
		pixels[9-1] = RGBUnpack(in[nextline  ]); /* Pixel 9 */

		pattern = 0;

		x = __PAST_LIBRARY_WIDTH - 1;
		do {
			newpattern = 0;
			if (pattern & DIFF26) newpattern |= DIFF51;
			if (pattern & DIFF56) newpattern |= DIFF54;
			if (pattern & DIFF86) newpattern |= DIFF57;
			if (pattern & DIFF53) newpattern |= DIFF24;
			if (pattern & DIFF59) newpattern |= DIFF84;

			if (lastLineDiffs[x] & (DIFF52 >> 1)) newpattern |= DIFF58;
			if (lastLineDiffs[x] & (DIFF26 >> 1)) newpattern |= DIFF59;
			if (lastLineDiffs[x] & (DIFF53 >> 1)) newpattern |= DIFF86;

			pattern = newpattern;

			if (ExpandedDiff(pixels[5-1], pixels[2-1])) pattern |= DIFF52;
			if (ExpandedDiff(pixels[5-1], pixels[3-1])) pattern |= DIFF53;
			if (ExpandedDiff(pixels[5-1], pixels[6-1])) pattern |= DIFF56;
			if (ExpandedDiff(pixels[2-1], pixels[6-1])) pattern |= DIFF26;
			lastLineDiffs[x] = pattern >> 1;

			newpattern = tree_hq[pattern & 0x7FF];
			pattern = RotatePattern(pattern);
			out[                0] =
				RGBPack(((pixels[5-1] * blends_2x[(newpattern * 4) + 0]) +
				         (pixels[2-1] * blends_2x[(newpattern * 4) + 1]) +
				         (pixels[4-1] * blends_2x[(newpattern * 4) + 2]) +
				         (pixels[1-1] * blends_2x[(newpattern * 4) + 3])) / 16);

			newpattern = tree_hq[pattern & 0x7FF];
			pattern = RotatePattern(pattern);
			out[                1] =
				RGBPack(((pixels[5-1] * blends_2x[(newpattern * 4) + 0]) +
				         (pixels[6-1] * blends_2x[(newpattern * 4) + 1]) +
				         (pixels[2-1] * blends_2x[(newpattern * 4) + 2]) +
				         (pixels[3-1] * blends_2x[(newpattern * 4) + 3])) / 16);

			newpattern = tree_hq[pattern & 0x7FF];
			pattern = RotatePattern(pattern);
			out[(__PAST_LIBRARY_WIDTH*2)+1] =
				RGBPack(((pixels[5-1] * blends_2x[(newpattern * 4) + 0]) +
				         (pixels[8-1] * blends_2x[(newpattern * 4) + 1]) +
				         (pixels[6-1] * blends_2x[(newpattern * 4) + 2]) +
				         (pixels[9-1] * blends_2x[(newpattern * 4) + 3])) / 16);

			newpattern = tree_hq[pattern & 0x7FF];
			pattern = RotatePattern(pattern);
			out[(__PAST_LIBRARY_WIDTH*2)  ] =
				RGBPack(((pixels[5-1] * blends_2x[(newpattern * 4) + 0]) +
				         (pixels[4-1] * blends_2x[(newpattern * 4) + 1]) +
				         (pixels[8-1] * blends_2x[(newpattern * 4) + 2]) +
				         (pixels[7-1] * blends_2x[(newpattern * 4) + 3])) / 16);

			out += 2;
			in++;
			x--;

			if (x == 0) {
				pixels[1-1] = pixels[2-1];
				pixels[2-1] = pixels[3-1];
				pixels[4-1] = pixels[5-1];
				pixels[5-1] = pixels[6-1];
				pixels[7-1] = pixels[8-1];
				pixels[8-1] = pixels[9-1];
				continue;
			}

			pixels[1-1] = pixels[2-1];
			pixels[2-1] = pixels[3-1];
			pixels[3-1] = RGBUnpack(in[prevline]); /* Pixel 3 */
			pixels[4-1] = pixels[5-1];
			pixels[5-1] = pixels[6-1];
			pixels[6-1] = RGBUnpack(in[1       ]); /* Pixel 6 */
			pixels[7-1] = pixels[8-1];
			pixels[8-1] = pixels[9-1];
			pixels[9-1] = RGBUnpack(in[nextline]); /* Pixel 9 */
		} while (x >= 0);

		prevline = 1 - __PAST_LIBRARY_WIDTH;
		out += (__PAST_LIBRARY_WIDTH * 2);

		y--;
		if (y > 0) {
			continue;
		}

		nextline = 0;
	} while (y >= 0);
}

void ProcessHQ4x(const pixel * restrict in, pixel * restrict out) {
	signed int y, x;
	unsigned int pattern, newpattern;
	uint32_t pixels[9];
	int prevline, nextline;

	memset(lastLineDiffs, 0, sizeof(lastLineDiffs));
	prevline = 1;
	nextline = 1 + __PAST_LIBRARY_WIDTH;
	y = __PAST_LIBRARY_HEIGHT - 1;

	do {
		pixels[1-1] =
		pixels[2-1] = RGBUnpack(in[prevline-1]); /* Pixel 2 */
		pixels[3-1] = RGBUnpack(in[prevline  ]); /* Pixel 3 */

		pixels[4-1] =
		pixels[5-1] = RGBUnpack(in[         0]); /* Pixel 5 */
		pixels[6-1] = RGBUnpack(in[         1]); /* Pixel 6 */

		pixels[7-1] =
		pixels[8-1] = RGBUnpack(in[nextline-1]); /* Pixel 8 */
		pixels[9-1] = RGBUnpack(in[nextline  ]); /* Pixel 9 */

		pattern = 0;

		x = __PAST_LIBRARY_WIDTH - 1;
		do {
			newpattern = 0;
			if (pattern & DIFF26) newpattern |= DIFF51;
			if (pattern & DIFF56) newpattern |= DIFF54;
			if (pattern & DIFF86) newpattern |= DIFF57;
			if (pattern & DIFF53) newpattern |= DIFF24;
			if (pattern & DIFF59) newpattern |= DIFF84;

			if (lastLineDiffs[x] & (DIFF52 >> 1)) newpattern |= DIFF58;
			if (lastLineDiffs[x] & (DIFF26 >> 1)) newpattern |= DIFF59;
			if (lastLineDiffs[x] & (DIFF53 >> 1)) newpattern |= DIFF86;

			pattern = newpattern;

			if (ExpandedDiff(pixels[5-1], pixels[2-1])) pattern |= DIFF52;
			if (ExpandedDiff(pixels[5-1], pixels[3-1])) pattern |= DIFF53;
			if (ExpandedDiff(pixels[5-1], pixels[6-1])) pattern |= DIFF56;
			if (ExpandedDiff(pixels[2-1], pixels[6-1])) pattern |= DIFF26;
			lastLineDiffs[x] = pattern >> 1;

			newpattern = tree_hq[pattern & 0x7FF];
			pattern = RotatePattern(pattern);
			out[                           0] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x0]) +
				         (pixels[2-1] * blends_4x[(newpattern * 16) + 1 + 0x0]) +
				         (pixels[4-1] * blends_4x[(newpattern * 16) + 2 + 0x0]) +
				         (pixels[1-1] * blends_4x[(newpattern * 16) + 3 + 0x0])) / 16);
			out[                           1] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x4]) +
				         (pixels[2-1] * blends_4x[(newpattern * 16) + 1 + 0x4]) +
				         (pixels[4-1] * blends_4x[(newpattern * 16) + 2 + 0x4]) +
				         (pixels[1-1] * blends_4x[(newpattern * 16) + 3 + 0x4])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0x4)  ] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x8]) +
				         (pixels[2-1] * blends_4x[(newpattern * 16) + 1 + 0x8]) +
				         (pixels[4-1] * blends_4x[(newpattern * 16) + 2 + 0x8]) +
				         (pixels[1-1] * blends_4x[(newpattern * 16) + 3 + 0x8])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0x4)+1] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0xC]) +
				         (pixels[2-1] * blends_4x[(newpattern * 16) + 1 + 0xC]) +
				         (pixels[4-1] * blends_4x[(newpattern * 16) + 2 + 0xC]) +
				         (pixels[1-1] * blends_4x[(newpattern * 16) + 3 + 0xC])) / 16);

			newpattern = tree_hq[pattern & 0x7FF];
			pattern = RotatePattern(pattern);
			out[                           3] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x0]) +
				         (pixels[6-1] * blends_4x[(newpattern * 16) + 1 + 0x0]) +
				         (pixels[2-1] * blends_4x[(newpattern * 16) + 2 + 0x0]) +
				         (pixels[3-1] * blends_4x[(newpattern * 16) + 3 + 0x0])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0x4)+3] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x4]) +
				         (pixels[6-1] * blends_4x[(newpattern * 16) + 1 + 0x4]) +
				         (pixels[2-1] * blends_4x[(newpattern * 16) + 2 + 0x4]) +
				         (pixels[3-1] * blends_4x[(newpattern * 16) + 3 + 0x4])) / 16);
			out[                           2] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x8]) +
				         (pixels[6-1] * blends_4x[(newpattern * 16) + 1 + 0x8]) +
				         (pixels[2-1] * blends_4x[(newpattern * 16) + 2 + 0x8]) +
				         (pixels[3-1] * blends_4x[(newpattern * 16) + 3 + 0x8])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0x4)+2] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0xC]) +
				         (pixels[6-1] * blends_4x[(newpattern * 16) + 1 + 0xC]) +
				         (pixels[2-1] * blends_4x[(newpattern * 16) + 2 + 0xC]) +
				         (pixels[3-1] * blends_4x[(newpattern * 16) + 3 + 0xC])) / 16);

			newpattern = tree_hq[pattern & 0x7FF];
			pattern = RotatePattern(pattern);
			out[(__PAST_LIBRARY_WIDTH*0xC)+3] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x0]) +
				         (pixels[8-1] * blends_4x[(newpattern * 16) + 1 + 0x0]) +
				         (pixels[6-1] * blends_4x[(newpattern * 16) + 2 + 0x0]) +
				         (pixels[9-1] * blends_4x[(newpattern * 16) + 3 + 0x0])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0xC)+2] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x4]) +
				         (pixels[8-1] * blends_4x[(newpattern * 16) + 1 + 0x4]) +
				         (pixels[6-1] * blends_4x[(newpattern * 16) + 2 + 0x4]) +
				         (pixels[9-1] * blends_4x[(newpattern * 16) + 3 + 0x4])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0x8)+3] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x8]) +
				         (pixels[8-1] * blends_4x[(newpattern * 16) + 1 + 0x8]) +
				         (pixels[6-1] * blends_4x[(newpattern * 16) + 2 + 0x8]) +
				         (pixels[9-1] * blends_4x[(newpattern * 16) + 3 + 0x8])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0x8)+2] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0xC]) +
				         (pixels[8-1] * blends_4x[(newpattern * 16) + 1 + 0xC]) +
				         (pixels[6-1] * blends_4x[(newpattern * 16) + 2 + 0xC]) +
				         (pixels[9-1] * blends_4x[(newpattern * 16) + 3 + 0xC])) / 16);

			newpattern = tree_hq[pattern & 0x7FF];
			pattern = RotatePattern(pattern);
			out[(__PAST_LIBRARY_WIDTH*0xC)  ] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x0]) +
				         (pixels[4-1] * blends_4x[(newpattern * 16) + 1 + 0x0]) +
				         (pixels[8-1] * blends_4x[(newpattern * 16) + 2 + 0x0]) +
				         (pixels[7-1] * blends_4x[(newpattern * 16) + 3 + 0x0])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0x8)  ] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x4]) +
				         (pixels[4-1] * blends_4x[(newpattern * 16) + 1 + 0x4]) +
				         (pixels[8-1] * blends_4x[(newpattern * 16) + 2 + 0x4]) +
				         (pixels[7-1] * blends_4x[(newpattern * 16) + 3 + 0x4])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0xC)+1] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0x8]) +
				         (pixels[4-1] * blends_4x[(newpattern * 16) + 1 + 0x8]) +
				         (pixels[8-1] * blends_4x[(newpattern * 16) + 2 + 0x8]) +
				         (pixels[7-1] * blends_4x[(newpattern * 16) + 3 + 0x8])) / 16);
			out[(__PAST_LIBRARY_WIDTH*0x8)+1] =
				RGBPack(((pixels[5-1] * blends_4x[(newpattern * 16) + 0 + 0xC]) +
				         (pixels[4-1] * blends_4x[(newpattern * 16) + 1 + 0xC]) +
				         (pixels[8-1] * blends_4x[(newpattern * 16) + 2 + 0xC]) +
				         (pixels[7-1] * blends_4x[(newpattern * 16) + 3 + 0xC])) / 16);

			out += 4;
			in++;
			x--;

			if (x == 0) {
				pixels[1-1] = pixels[2-1];
				pixels[2-1] = pixels[3-1];
				pixels[4-1] = pixels[5-1];
				pixels[5-1] = pixels[6-1];
				pixels[7-1] = pixels[8-1];
				pixels[8-1] = pixels[9-1];
				continue;
			}

			pixels[1-1] = pixels[2-1];
			pixels[2-1] = pixels[3-1];
			pixels[3-1] = RGBUnpack(in[prevline]); /* Pixel 3 */
			pixels[4-1] = pixels[5-1];
			pixels[5-1] = pixels[6-1];
			pixels[6-1] = RGBUnpack(in[1       ]); /* Pixel 6 */
			pixels[7-1] = pixels[8-1];
			pixels[8-1] = pixels[9-1];
			pixels[9-1] = RGBUnpack(in[nextline]); /* Pixel 9 */
		} while (x >= 0);

		prevline = 1 - __PAST_LIBRARY_WIDTH;
		out += (__PAST_LIBRARY_WIDTH * 0xC);

		y--;
		if (y > 0) {
			continue;
		}

		nextline = 0;
	} while (y >= 0);
}
