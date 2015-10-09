/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>

#include <formats/jsonsax.h>
#include <retro_log.h>

#include "cheevos.h"
#include "dynamic.h"

enum
{
  VAR_SIZE_BIT_0,
  VAR_SIZE_BIT_1,
  VAR_SIZE_BIT_2,
  VAR_SIZE_BIT_3,
  VAR_SIZE_BIT_4,
  VAR_SIZE_BIT_5,
  VAR_SIZE_BIT_6,
  VAR_SIZE_BIT_7,
  VAR_SIZE_NIBBLE_LOWER,
  VAR_SIZE_NIBBLE_UPPER,
  /* Byte, */
  VAR_SIZE_EIGHT_BITS, /* =Byte, */
  VAR_SIZE_SIXTEEN_BITS,
  VAR_SIZE_THIRTYTWO_BITS,

  VAR_SIZE_LAST
}; /* var_t.size */

enum
{
  VAR_TYPE_ADDRESS,     /* compare to the value of a live address in RAM */
  VAR_TYPE_VALUE_COMP,  /* a number. assume 32 bit */
  VAR_TYPE_DELTA_MEM,   /* the value last known at this address. */
  VAR_TYPE_DYNAMIC_VAR, /* a custom user-set variable */

  VAR_TYPE_LAST
}; /* var_t.type */

enum
{
  COND_OP_EQUALS,
  COND_OP_LESS_THAN,
  COND_OP_LESS_THAN_OR_EQUAL,
  COND_OP_GREATER_THAN,
  COND_OP_GREATER_THAN_OR_EQUAL,
  COND_OP_NOT_EQUAL_TO,

  COND_OP_LAST
}; /* cheevos_cond_t.op */

enum
{
  COND_TYPE_STANDARD,
  COND_TYPE_PAUSE_IF,
  COND_TYPE_RESET_IF,

  COND_TYPE_LAST
}; /* cheevos_cond_t.type */

enum
{
	DIRTY_TITLE			  = 1 << 0,
	DIRTY_DESC			  = 1 << 1,
	DIRTY_POINTS		  = 1 << 2,
	DIRTY_AUTHOR		  = 1 << 3,
	DIRTY_ID			    = 1 << 4,
	DIRTY_BADGE			  = 1 << 5,
	DIRTY_CONDITIONS  = 1 << 6,
	DIRTY_VOTES			  = 1 << 7,
	DIRTY_DESCRIPTION	= 1 << 8,

	DIRTY__ALL			  = ( 1 << 9 ) - 1
};

typedef struct
{
  unsigned size;
  unsigned type;
  unsigned bank_id;
  unsigned value;
  unsigned previous;
}
var_t;

typedef struct
{
  unsigned type;
  unsigned req_hits;
  unsigned curr_hits;

  var_t    source;
  unsigned op;
  var_t    target;
} cheevos_cond_t;

typedef struct
{
  cheevos_cond_t*  conds;
  unsigned count;

  const char* expression;
}
condset_t;

typedef struct
{
  unsigned    id;
  const char* title;
  const char* description;
  const char* author;
  const char* badge;
  unsigned    points;
  unsigned    dirty;
  int         active;
  int         modified;

  condset_t*  condsets;
  unsigned    count;
}
cheevo_t;

typedef struct
{
  cheevo_t* cheevos;
  unsigned  count;
}
cheevoset_t;

cheevos_config_t cheevos_config =
{
  /* enable          */ 1,
  /* test_unofficial */ 0,
};

#ifdef HAVE_CHEEVOS

static cheevoset_t core_cheevos = { NULL, 0 };
static cheevoset_t unofficial_cheevos = { NULL, 0 };

#ifdef HAVE_SMW_CHEEVOS

/*****************************************************************************
Achievements for SNES' Super Mario World.
*****************************************************************************/

static const char* smw_json = "{\"Success\":true,\"PatchData\":{\"ID\":228,\"Title\":\"Super Mario World\",\"ConsoleID\":3,\"ForumTopicID\":135,\"Flags\":0,\"ImageIcon\":\"/Images/000284.png\",\"ImageTitle\":\"/Images/000021.png\",\"ImageIngame\":\"/Images/000022.png\",\"ImageBoxArt\":\"/Images/000138.png\",\"Publisher\":\"Nintendo\",\"Developer\":\"Nintendo EAD\",\"Genre\":\"Platforming\",\"Released\":\"JP 1990 , NA 1991 Europe 1992\",\"IsFinal\":false,\"ConsoleName\":\"SNES\",\"RichPresencePatch\":\"Lookup:LevelName\r\n0x0=Title Screen\r\n0x14=Yellow Switch Palace\r\n0x28=Yoshi's House\r\n0x29=Yoshi's Island 1\r\n0x2a=Yoshi's Island 2\r\n0x27=Yoshi's Island 3\r\n0x26=Yoshi's Island 4\r\n0x25=#1 Iggy's Castle\r\n0x15=Donut Plains 1\r\n0x9=Donut Plains 2\r\n0x8=Green Switch Palace\r\n0x4=Donut Ghost House\r\n0x3=Top Secret Area\r\n0x5=Donut Plains 3\r\n0x6=Donut Plains 4\r\n0x7=#2 Morton's Castle\r\n0xa=Donut Secret 1\r\n0x13=Donut Secret House\r\n0x2f=Donut Secret 2\r\n0x3e=Vanilla Dome 1\r\n0x3c=Vanilla Dome 2\r\n0x3f=Red Switch Palace\r\n0x2b=Vanilla Ghost House\r\n0x2e=Vanilla Dome 3\r\n0x3d=Vanilla Dome 4\r\n0x40=#3 Lemmy's Castle\r\n0x2d=Vanilla Secret 1\r\n0x1=Vanilla Secret 2\r\n0x2=Vanilla Secret 3\r\n0xb=Vanilla Fortress\r\n0xc=Butter Bridge 1\r\n0xd=Butter Bridge 2\r\n0xe=#4 Ludwig's Castle\r\n0xf=Cheese Bridge Area\r\n0x10=Cookie Mountain\r\n0x11=Soda Lake\r\n0x41=Forest Ghost House\r\n0x42=Forest of Illusion 1\r\n0x43=Forest of Illusion 4\r\n0x44=Forest of Illusion 2\r\n0x45=Blue Switch Palace\r\n0x46=Forest Secret Area\r\n0x47=Forest of Illusion 3\r\n0x1f=Forest Fortress\r\n0x20=#5 Roy's Castle\r\n0x21=Choco-Ghost House\r\n0x22=Chocolate Island 1\r\n0x23=Chocolate Island 3\r\n0x24=Chocolate Island 2\r\n0x1b=Chocolate Fortress\r\n0x1d=Chocolate Island 4\r\n0x1c=Chocolate Island 5\r\n0x1a=#6 Wendy's Castle\r\n0x18=Sunken Ghost Ship\r\n0x3b=Chocolate Secret\r\n0x3a=Valley of Bowser 1\r\n0x39=Valley of Bowser 2\r\n0x38=Valley Ghost House\r\n0x37=Valley of Bowser 3\r\n0x33=Valley of Bowser 4\r\n0x34=#7 Larry's Castle\r\n0x35=Valley Fortress\r\n0x31=Front Door\r\n0x32=Back Door\r\n0x58=Star World 1\r\n0x54=Star World 2\r\n0x56=Star World 3\r\n0x59=Star World 4\r\n0x5a=Star World 5\r\n0x4e=Gnarly\r\n0x4f=Tubular\r\n0x50=Way Cool\r\n0x51=Awesome\r\n0x4c=Groovy\r\n0x4b=Mondo\r\n0x4a=Outrageous\r\n0x49=Funky\r\n\r\nFormat:Lives\r\nFormatType=VALUE\r\n\r\nDisplay:\r\n@LevelName(0xh0013bf), @Lives(0xh0dbe_v+1) live(s)\",\"Achievements\":[{\"ID\":4874,\"MemAddr\":\"0xH000019=2\",\"Title\":\"I Believe I Can Fly\",\"Description\":\"Collect a feather\",\"Points\":5,\"Author\":\"UNHchabo\",\"Modified\":1434153343,\"Created\":1391908064,\"BadgeName\":\"05506\",\"Flags\":3},{\"ID\":4933,\"MemAddr\":\"0xH0018c2>0\",\"Title\":\"Floating Through the Clouds\",\"Description\":\"Hijack a Lakitu cloud\",\"Points\":10,\"Author\":\"UNHchabo\",\"Modified\":1441035935,\"Created\":1392010935,\"BadgeName\":\"00085\",\"Flags\":5},{\"ID\":4934,\"MemAddr\":\"0xH0013c7=4\",\"Title\":\"Yellow Yoshi\",\"Description\":\"Ride a yellow Yoshi\",\"Points\":10,\"Author\":\"UNHchabo\",\"Modified\":1410734527,\"Created\":1392011140,\"BadgeName\":\"00085\",\"Flags\":5},{\"ID\":4935,\"MemAddr\":\"0xH0013c7=6\",\"Title\":\"Blue Yoshi\",\"Description\":\"Ride a blue Yoshi\",\"Points\":10,\"Author\":\"UNHchabo\",\"Modified\":1410735517,\"Created\":1392011155,\"BadgeName\":\"00085\",\"Flags\":5},{\"ID\":4936,\"MemAddr\":\"0xH0013c7=8\",\"Title\":\"Red Yoshi\",\"Description\":\"Ride a red Yoshi\",\"Points\":10,\"Author\":\"UNHchabo\",\"Modified\":1410735526,\"Created\":1392011190,\"BadgeName\":\"00085\",\"Flags\":5},{\"ID\":4937,\"MemAddr\":\"0xH000dbe>=98_0xH000019=1\",\"Title\":\"Mushroom Collector\",\"Description\":\"Get 99 lives, and become Super Mario\",\"Points\":10,\"Author\":\"UNHchabo\",\"Modified\":1392018537,\"Created\":1392018537,\"BadgeName\":\"00085\",\"Flags\":5},{\"ID\":5756,\"MemAddr\":\"0xH0013c5>0\",\"Title\":\"Shoot the Moon!\",\"Description\":\"Collect a 3-Up\",\"Points\":10,\"Author\":\"UNHchabo\",\"Modified\":1393710657,\"Created\":1393710657,\"BadgeName\":\"05505\",\"Flags\":5},{\"ID\":341,\"MemAddr\":\"0x1420=5\",\"Title\":\"Unleash the Dragon\",\"Description\":\"Collect 5 Dragon Coins in a level\",\"Points\":10,\"Author\":\"Scott\",\"Modified\":1367266589,\"Created\":1367266583,\"BadgeName\":\"00549\",\"Flags\":3},{\"ID\":342,\"MemAddr\":\"0xH000dc1=1_0xH0013bf>0\",\"Title\":\"Giddy Up!\",\"Description\":\"Catch a ride with a friend\",\"Points\":10,\"Author\":\"Scott\",\"Modified\":1376974455,\"Created\":1367266931,\"BadgeName\":\"00550\",\"Flags\":3},{\"ID\":340,\"MemAddr\":\"0xH0dbf=99\",\"Title\":\"Rich Mario\",\"Description\":\"Collect 99 coins!\",\"Points\":10,\"Author\":\"Scott\",\"Modified\":1367254980,\"Created\":1367254976,\"BadgeName\":\"00547\",\"Flags\":3},{\"ID\":1706,\"MemAddr\":\"0xH001900=80\",\"Title\":\"Maximum Finish\",\"Description\":\"Cross the finish line at the end of the stage and collect the max 50 stars\",\"Points\":10,\"Author\":\"jackolantern\",\"Modified\":1372762549,\"Created\":1372674230,\"BadgeName\":\"02014\",\"Flags\":3},{\"ID\":2246,\"MemAddr\":\"0xH000f31=0(20)_R:0xH000f31!=0_0xH000f32=0(20)_R:0xH000f32!=0_0xH000f33=0(20)_R:0xH000f33!=0_0xH000dbe>d0xH000dbe(8)_0xH001411=1(20)_R:0xH001411=0\",\"Title\":\"Perfect Bonus Stage\",\"Description\":\"Score 8 Extra Lives on the Bonus Game\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376611582,\"Created\":1376582613,\"BadgeName\":\"02739\",\"Flags\":3},{\"ID\":2199,\"MemAddr\":\"0xH001f28=1_0xH001f27=1_0xH001f29=1_0xH001f2a=1\",\"Title\":\"Filling all the blocks in\",\"Description\":\"Hit the buttons in all four coloured switch palaces.\",\"Points\":20,\"Author\":\"jackolantern\",\"Modified\":1376552794,\"Created\":1376514114,\"BadgeName\":\"02720\",\"Flags\":3},{\"ID\":2253,\"MemAddr\":\"0xH0013bf=37_0xH000dd5=1\",\"Title\":\"Iggy Koopa\",\"Description\":\"Defeat Iggy Koopa of Castle #1\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1425959554,\"Created\":1376616356,\"BadgeName\":\"00562\",\"Flags\":3},{\"ID\":347,\"MemAddr\":\"0xH0013bf=7_0xH000dd5=1\",\"Title\":\"Morton Koopa Jr\",\"Description\":\"Defeat Morton Koopa Jr of Castle #2\",\"Points\":10,\"Author\":\"Scott\",\"Modified\":1425959561,\"Created\":1367322700,\"BadgeName\":\"00562\",\"Flags\":3},{\"ID\":2261,\"MemAddr\":\"0xH0013bf=64_0xH000dd5=1\",\"Title\":\"Lemmy Koopa\",\"Description\":\"Defeat Lemmy Koopa of Castle #3\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1425959153,\"Created\":1376652522,\"BadgeName\":\"00562\",\"Flags\":3},{\"ID\":2262,\"MemAddr\":\"0xH0013bf=14_0xH000dd5=1\",\"Title\":\"Ludwig von Koopa\",\"Description\":\"Defeat Ludwig von Koopa of Castle #4\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1425959133,\"Created\":1376653163,\"BadgeName\":\"00562\",\"Flags\":3},{\"ID\":2306,\"MemAddr\":\"0xH0013bf=32_0xH000dd5=1\",\"Title\":\"Roy Koopa\",\"Description\":\"Defeat Roy Koopa of Castle #5\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1425956637,\"Created\":1376938808,\"BadgeName\":\"00562\",\"Flags\":3},{\"ID\":2309,\"MemAddr\":\"0xH0013bf=26_0xH000906=1.400._R:0xH0013bf!=26\",\"Title\":\"Wendy O. Koopa\",\"Description\":\"Defeat Wendy O. Koopa of Castle #6\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1390938088,\"Created\":1376939582,\"BadgeName\":\"00562\",\"Flags\":3},{\"ID\":2342,\"MemAddr\":\"0xH0013bf=52_0xH000dd5=1\",\"Title\":\"Larry Koopa\",\"Description\":\"Defeat Larry Koopa of Castle #7\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1425959095,\"Created\":1376970283,\"BadgeName\":\"00562\",\"Flags\":3},{\"ID\":2250,\"MemAddr\":\"0xH0013bf=11(20)_0xH000906=1(20)_R:0xH001411!=1\",\"Title\":\"Reznor\",\"Description\":\"Defeat the Reznor atop Vanilla Dome\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376615087,\"Created\":1376615073,\"BadgeName\":\"02742\",\"Flags\":3},{\"ID\":2307,\"MemAddr\":\"0xH0013bf=31(20)_0xH000906=1(20)_R:0xH001411!=1\",\"Title\":\"Reznor Again?\",\"Description\":\"Defeat the Reznor in the clearing of the Forest of Illusion\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376938847,\"Created\":1376938811,\"BadgeName\":\"02742\",\"Flags\":3},{\"ID\":2308,\"MemAddr\":\"0xH0013bf=27(20)_0xH000906=1(20)_R:0xH001411!=1\",\"Title\":\"Reznor, do you ever give up?\",\"Description\":\"Defeat the Reznor at the center of Chocolate Island\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376938850,\"Created\":1376938815,\"BadgeName\":\"02742\",\"Flags\":3},{\"ID\":2338,\"MemAddr\":\"0xH0013bf=53(20)_0xH000906=1(20)_R:0xH001411!=1\",\"Title\":\"Reznor...\",\"Description\":\"Defeat the Reznor in Bowsers Valley\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376969439,\"Created\":1376969412,\"BadgeName\":\"02742\",\"Flags\":3},{\"ID\":2275,\"MemAddr\":\"0xH0013bf=49_R:0xH0013bf!=49_0xH0013f9=3_R:0xH0013f9!=3_0xH0013ef=1_R:0xH0013ef!=1\",\"Title\":\"King Bowser Koopa\",\"Description\":\"Beat Bowser and Save the Princess (Front Door!)\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1390856575,\"Created\":1376742802,\"BadgeName\":\"02764\",\"Flags\":3},{\"ID\":2276,\"MemAddr\":\"0xH0013bf=49_R:0xH0013bf!=49_0xH0013f9=3_R:0xH0013f9!=3_0xH0013ef=1_R:0xH0013ef!=1_0xH000019=0_R:0xH000019!=0\",\"Title\":\"Baby's First Kiss\",\"Description\":\"Get the Princess Kiss as Little Mario (Front Door!)\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1390856597,\"Created\":1376742805,\"BadgeName\":\"02765\",\"Flags\":3},{\"ID\":2277,\"MemAddr\":\"0xH0013bf=49_R:0xH0013bf!=49_0xH0013f9=3_R:0xH0013f9!=3_0xH0013ef=1_R:0xH0013ef!=1_0xH000019=3_R:0xH000019!=3\",\"Title\":\"Burning Bowser\",\"Description\":\"Get the Princess Kiss as Fire Mario (Front Door!)\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1390856617,\"Created\":1376742808,\"BadgeName\":\"02766\",\"Flags\":3},{\"ID\":2278,\"MemAddr\":\"0xH0013bf=49_R:0xH0013bf!=49_0xH0013f9=3_R:0xH0013f9!=3_0xH0013ef=1_R:0xH0013ef!=1_0xH000019=2_R:0xH000019!=2\",\"Title\":\"Flying Finish\",\"Description\":\"Get the Princess Kiss as Cape Mario (Front Door!)\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1390856673,\"Created\":1376742811,\"BadgeName\":\"02767\",\"Flags\":3},{\"ID\":2299,\"MemAddr\":\"0xH000020=168_R:0xH000020!=168_0xH00001e=248_R:0xH00001e!=248_0xH0013c3=0_R:0xH0013c3!=0_0xH0013bf=19_R:0xH0013bf!=19_0xH001411=0\",\"Title\":\"The Big Boo\",\"Description\":\"Defeat the Big Boo in Donut Secret House\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1378767596,\"Created\":1376918022,\"BadgeName\":\"02797\",\"Flags\":3},{\"ID\":2298,\"MemAddr\":\"0xH0013c3=6\",\"Title\":\"To the Stars!\",\"Description\":\"Reach the Star Road\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376918072,\"Created\":1376918019,\"BadgeName\":\"02798\",\"Flags\":3},{\"ID\":2264,\"MemAddr\":\"0xH000007=114_R:0xH000007!=114_0xH000008=121_R:0xH000008!=121_0xH0013c3=3_R:0xH0013c3!=3_0xH001411=0\",\"Title\":\"I could've sworn...\",\"Description\":\"Get lost in the Forest of Illusion\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376941372,\"Created\":1376657732,\"BadgeName\":\"02753\",\"Flags\":3},{\"ID\":2305,\"MemAddr\":\"0xH00001e=169_0xH000020=28_0xH0013c3=0_0xH001411=0\",\"Title\":\"Chocolate Donut\",\"Description\":\"Walk in a circle on Chocolate Island\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376945561,\"Created\":1376938805,\"BadgeName\":\"02806\",\"Flags\":3},{\"ID\":2300,\"MemAddr\":\"0xH0013c3=5\",\"Title\":\"Mario's Special Place\",\"Description\":\"Get to the challenging Special Stages\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376918077,\"Created\":1376918026,\"BadgeName\":\"02800\",\"Flags\":3},{\"ID\":2302,\"MemAddr\":\"0xH0013c3=5(1)_0xH0013c3=1(1)_R:0xH0013c3=6_R:0xH0013c3=0_R:0xH001f79=0\",\"Title\":\"Change of Scenery\",\"Description\":\"Clear the Special Zone and change the seasons in Mushroom Kingdom\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376929315,\"Created\":1376929303,\"BadgeName\":\"02803\",\"Flags\":3},{\"ID\":2252,\"MemAddr\":\"0xH0013f3=1(20)_0xH001411=1(20)_R:0xH001411=0_0xH000019=0(20)_R:0xH000019!=0(20)\",\"Title\":\"Too Much Pasta\",\"Description\":\"Let Little Mario load up on mama Luigi's Famous P(asta)-gas!\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376651934,\"Created\":1376615082,\"BadgeName\":\"02744\",\"Flags\":3},{\"ID\":2251,\"MemAddr\":\"0xH0013f3=1(20)_0xH001411=1(20)_R:0xH001411=0_0xH000019!=0_R:0xH000019=0\",\"Title\":\"Another kind of flying\",\"Description\":\"Send Mario flying with P-gas\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376617016,\"Created\":1376615078,\"BadgeName\":\"02743\",\"Flags\":3},{\"ID\":2263,\"MemAddr\":\"0xH001697=12_0xH0013bf=66_R:0xH001697=0\",\"Title\":\"Bother the Wigglers\",\"Description\":\"Jump on yellow Wigglers in Forest of Illusion 12 times in a row for a surprise!\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1379674609,\"Created\":1376655155,\"BadgeName\":\"02752\",\"Flags\":3},{\"ID\":2274,\"MemAddr\":\"0xH0013bf=49_R:0xH0013bf!=49_0xH0013f9=3_R:0xH0013f9!=3_0xH001f2e=11_R:0xH001f2e!=11_0xH0013ef=1_R:0xH0013ef!=1\",\"Title\":\"Shortest Route\",\"Description\":\"Clear the fewest stages possible and beat the game\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1376742834,\"Created\":1376742799,\"BadgeName\":\"02768\",\"Flags\":3},{\"ID\":2297,\"MemAddr\":\"0xH001f2e=0.1._0xH001f2e=1.1._0xH001f2e=2.1._0xH001f2e=3.1._0xH001f2e=4.1._0xH001f2e=5.1._0xH001f2e=6.1._0xH001f2e=7.1._0xH001f2e=8.1._0xH001f2e=9.1._0xH001f2e=10.1._0xH001f2e=11.1._0xH0013f9=3_R:0xH000dbe<d0xH000dbe_0xH0013bf=49_R:0xH001f79=0_R:0xH001f2e<d0xH001f2e_0xH0013ef=1\",\"Title\":\"Starman Challenge\",\"Description\":\"Clear the game without dying (One session)\",\"Points\":20,\"Author\":\"Jaarl\",\"Modified\":1438387321,\"Created\":1376918015,\"BadgeName\":\"02772\",\"Flags\":3},{\"ID\":2345,\"MemAddr\":\"0xH0013ef=1_0xH0013f9=3_0xH0013bf=50\",\"Title\":\"Backdooring Bowser\",\"Description\":\"Beat Bowser through the Back Door\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1390856810,\"Created\":1376973534,\"BadgeName\":\"02842\",\"Flags\":3},{\"ID\":2303,\"MemAddr\":\"R:0xH001411=1_R:0xH0013c3!=5_0xH0013c3=5(7200)\",\"Title\":\"That oh so familiar tune\",\"Description\":\"Find the secret in the Special Zone\",\"Points\":10,\"Author\":\"Jaarl\",\"Modified\":1379674614,\"Created\":1376929305,\"BadgeName\":\"02804\",\"Flags\":3},{\"ID\":2304,\"MemAddr\":\"0xH001f2e=96(1)_0xH001f2e=95(1)\",\"Title\":\"All Exits\",\"Description\":\"100% Clear the game\",\"Points\":25,\"Author\":\"Jaarl\",\"Modified\":1376929320,\"Created\":1376929308,\"BadgeName\":\"02805\",\"Flags\":3},{\"ID\":2985,\"MemAddr\":\"0x0018f2=3598_0xH0013bf=31\",\"Title\":\"The Investigator\",\"Description\":\"Access secret area in the forest fortress\",\"Points\":10,\"Author\":\"mrvsonic87\",\"Modified\":1380239596,\"Created\":1379656738,\"BadgeName\":\"03447\",\"Flags\":3}],\"Leaderboards\":[]}}";

#endif /* HAVE_SMW_CHEEVOS */

/*****************************************************************************
Supporting functions.
*****************************************************************************/

static uint32_t djb2( const char* str, size_t length )
{
  const unsigned char* aux = (const unsigned char*)str;
  const unsigned char* end = aux + length;
  uint32_t hash = 5381;

  while ( aux < end )
  {
    hash = ( hash << 5 ) + hash + *aux++;
  }

  return hash;
}

/*****************************************************************************
Count number of achievements in a JSON file.
*****************************************************************************/

typedef struct
{
  int      in_cheevos;
  uint32_t field_hash;
  unsigned core_count;
  unsigned unofficial_count;
}
countud_t;

static int count__json_end_array( void* userdata )
{
  countud_t* ud = (countud_t*)userdata;
  ud->in_cheevos = 0;
  return 0;
}

static int count__json_key( void* userdata, const char* name, size_t length )
{
  countud_t* ud = (countud_t*)userdata;
  ud->field_hash = djb2( name, length );

  if ( ud->field_hash == 0x69749ae1U /* Achievements */ )
  {
    ud->in_cheevos = 1;
  }

  return 0;
}

static int count__json_number( void* userdata, const char* number, size_t length )
{
  countud_t* ud = (countud_t*)userdata;

  if ( ud->in_cheevos && ud->field_hash == 0x0d2e96b2U /* Flags */ )
  {
    long flags = strtol( number, NULL, 10 );

    if ( flags == 3 ) /* core achievements */
    {
      ud->core_count++;
    }
    else if ( flags == 5 ) /* unofficial achievements */
    {
      ud->unofficial_count++;
    }
  }

  return 0;
}

static int count_cheevos( const char* json, unsigned* core_count, unsigned* unofficial_count )
{
  static const jsonsax_handlers_t handlers =
  {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    count__json_end_array,
    count__json_key,
    NULL,
    NULL,
    count__json_number,
    NULL,
    NULL
  };

  countud_t ud;
  ud.in_cheevos = 0;
  ud.core_count = 0;
  ud.unofficial_count = 0;

  int res = jsonsax_parse( json, &handlers, (void*)&ud );

  *core_count = ud.core_count;
  *unofficial_count = ud.unofficial_count;

  return res;
}

/*****************************************************************************
Parse the MemAddr field.
*****************************************************************************/

static unsigned prefix_to_comp_size( char prefix )
{
  /* Careful not to use ABCDEF here, this denotes part of an actual variable! */

  switch( toupper( prefix ) )
  {
    case 'M': return VAR_SIZE_BIT_0;
    case 'N': return VAR_SIZE_BIT_1;
    case 'O': return VAR_SIZE_BIT_2;
    case 'P': return VAR_SIZE_BIT_3;
    case 'Q': return VAR_SIZE_BIT_4;
    case 'R': return VAR_SIZE_BIT_5;
    case 'S': return VAR_SIZE_BIT_6;
    case 'T': return VAR_SIZE_BIT_7;
    case 'L': return VAR_SIZE_NIBBLE_LOWER;
    case 'U': return VAR_SIZE_NIBBLE_UPPER;
    case 'H': return VAR_SIZE_EIGHT_BITS;
    case 'X': return VAR_SIZE_THIRTYTWO_BITS;
    default:
    case ' ': return VAR_SIZE_SIXTEEN_BITS;
  }
}

static unsigned read_hits( const char** memaddr )
{
  const char* str = *memaddr;
  char* end;
  unsigned num_hits = 0;

  if ( *str == '(' || *str == '.' )
  {
    num_hits = strtol( str + 1, &end, 10 );
    str = end + 1;
  }

  *memaddr = str;
  return num_hits;
}

static unsigned parse_operator( const char** memaddr )
{
  const char* str = *memaddr;
  unsigned char op;

  if ( *str == '=' && str[ 1 ] == '=' )
  {
    op = COND_OP_EQUALS;
    str += 2;
  }
  else if ( *str == '=' )
  {
    op = COND_OP_EQUALS;
    str++;
  }
  else if ( *str == '!' && str[ 1 ] == '=' )
  {
    op = COND_OP_NOT_EQUAL_TO;
    str += 2;
  }
  else if ( *str == '<' && str[ 1 ] == '=' )
  {
    op = COND_OP_LESS_THAN_OR_EQUAL;
    str += 2;
  }
  else if ( *str == '<' )
  {
    op = COND_OP_LESS_THAN;
    str++;
  }
  else if ( *str == '>' && str[ 1 ] == '=' )
  {
    op = COND_OP_GREATER_THAN_OR_EQUAL;
    str += 2;
  }
  else if ( *str == '>' )
  {
    op = COND_OP_GREATER_THAN;
    str++;
  }
  else
  {
    /* TODO log the exception */
    op = COND_OP_EQUALS;
  }

  *memaddr = str;
  return op;
}

static void parse_var( var_t* var, const char** memaddr )
{
  const char* str = *memaddr;
  char* end;
  unsigned base = 16;

  if ( toupper( *str ) == 'D' && str[ 1 ] == '0' && toupper( str[ 2 ] ) == 'X' )
  {
    /* d0x + 4 hex digits */
    str += 3;
    var->type = VAR_TYPE_DELTA_MEM;
  }
  else if ( *str == '0' && toupper( str[ 1 ] ) == 'X' )
  {
    /* 0x + 4 hex digits */
    str += 2;
    var->type = VAR_TYPE_ADDRESS;
  }
  else
  {
    var->type = VAR_TYPE_VALUE_COMP;

    if ( toupper( *str ) == 'H' )
    {
      str++;
    }
    else
    {
      base = 10;
    }
  }

  if ( var->type != VAR_TYPE_VALUE_COMP )
  {
    var->size = prefix_to_comp_size( *str );

    if ( var->size != VAR_SIZE_SIXTEEN_BITS )
    {
      str++;
    }
  }

  var->value = strtol( str, &end, base );
  *memaddr = end;
}

static void parse_cond( cheevos_cond_t* cond, const char** memaddr )
{
  const char* str = *memaddr;

  if ( *str == 'R' && str[ 1 ] == ':' )
  {
    cond->type = COND_TYPE_RESET_IF;
    str += 2;
  }
  else if ( *str == 'P' && str[ 1 ] == ':' )
  {
    cond->type = COND_TYPE_PAUSE_IF;
    str += 2;
  }
  else
  {
    cond->type = COND_TYPE_STANDARD;
  }

  parse_var( &cond->source, &str );
  cond->op = parse_operator( &str );
  parse_var( &cond->target, &str );
  cond->curr_hits = 0;
  cond->req_hits = read_hits( &str );

  *memaddr = str;
}

static unsigned count_cond_sets( const char* memaddr )
{
  unsigned count = 0;
  cheevos_cond_t   cond;

  do
  {
    do
    {
      while( *memaddr == ' ' || *memaddr == '_' || *memaddr == '|' || *memaddr == 'S' )
      {
        memaddr++; /* Skip any chars up til the start of the achievement condition */
      }

      parse_cond( &cond, &memaddr );
    }
    while( *memaddr == '_' || *memaddr == 'R' || *memaddr == 'P' ); /* AND, ResetIf, PauseIf */

    count++;
  }
  while( *memaddr == 'S' ); /* Repeat for all subconditions if they exist */

  return count;
}

static unsigned count_conds_in_set( const char* memaddr, unsigned set )
{
  unsigned index = 0;
  unsigned count = 0;
  cheevos_cond_t   cond;

  do
  {
    do
    {
      while ( *memaddr == ' ' || *memaddr == '_' || *memaddr == '|' || *memaddr == 'S' )
      {
        memaddr++; /* Skip any chars up til the start of the achievement condition */
      }

      parse_cond( &cond, &memaddr );

      if ( index == set )
      {
        count++;
      }
    }
    while ( *memaddr == '_' || *memaddr == 'R' || *memaddr == 'P' ); /* AND, ResetIf, PauseIf */
  }
  while( *memaddr == 'S' ); /* Repeat for all subconditions if they exist */

  return count;
}

static void parse_memaddr( cheevos_cond_t* cond, const char* memaddr )
{
  do
  {
    do
    {
      while( *memaddr == ' ' || *memaddr == '_' || *memaddr == '|' || *memaddr == 'S' )
      {
        memaddr++; /* Skip any chars up til the start of the achievement condition */
      }

      parse_cond( cond++, &memaddr );
    }
    while( *memaddr == '_' || *memaddr == 'R' || *memaddr == 'P' ); /* AND, ResetIf, PauseIf */
  }
  while( *memaddr == 'S' ); /* Repeat for all subconditions if they exist */
}

/*****************************************************************************
Load achievements from a JSON string.
*****************************************************************************/

typedef struct
{
  const char* string;
  size_t      length;
}
field_t;

typedef struct
{
  int      in_cheevos;
  field_t* field;
  unsigned core_count;
  unsigned unofficial_count;

  field_t id, memaddr, title, desc, points, author, modified, created, badge, flags;
}
readud_t;

static inline const char* dupstr( const field_t* field )
{
  char* string = (char*)malloc( field->length + 1 );

  if ( string )
  {
    memcpy( (void*)string, (void*)field->string, field->length );
    string[ field->length ] = 0;
  }

  return string;
}

static int new_cheevo( readud_t* ud )
{
  int flags = strtol( ud->flags.string, NULL, 10 );
  cheevo_t* cheevo;

  if ( flags == 3 )
  {
    cheevo = core_cheevos.cheevos + ud->core_count++;
  }
  else
  {
    cheevo = unofficial_cheevos.cheevos + ud->unofficial_count++;
  }

  cheevo->id = strtol( ud->id.string, NULL, 10 );
  cheevo->title = dupstr( &ud->title );
  cheevo->description = dupstr( &ud->desc );
  cheevo->author = dupstr( &ud->author );
  cheevo->badge = dupstr( &ud->badge );
  cheevo->points = strtol( ud->points.string, NULL, 10 );
  cheevo->active = flags == 3;
  cheevo->modified = 0;

  if ( !cheevo->title || !cheevo->description || !cheevo->author || !cheevo->badge )
  {
    return -1;
  }

  cheevo->count = count_cond_sets( ud->memaddr.string );
  cheevo->condsets = (condset_t*)malloc( cheevo->count * sizeof( condset_t ) );

  if ( !cheevo->condsets )
  {
    return -1;
  }

  if ( cheevo->count )
  {
    const condset_t* end = cheevo->condsets + cheevo->count;
    unsigned set = 0;
    condset_t* condset;

    for ( condset = cheevo->condsets; condset < end; condset++ )
    {
      condset->count = count_conds_in_set( ud->memaddr.string, set++ );

      if ( condset->count )
      {
        condset->conds = (cheevos_cond_t*)malloc( condset->count * sizeof( cheevos_cond_t ) );

        if ( !condset->conds )
        {
          return -1;
        }

        condset->expression = dupstr( &ud->memaddr );
        parse_memaddr( condset->conds, ud->memaddr.string );
      }
      else
      {
        condset->conds = NULL;
      }
    }
  }

  return 0;
}

static int read__json_key( void* userdata, const char* name, size_t length )
{
  readud_t* ud = (readud_t*)userdata;
  uint32_t hash = djb2( name, length );

  ud->field = NULL;

  if ( hash == 0x69749ae1U /* Achievements */ )
  {
    ud->in_cheevos = 1;
  }
  else if ( ud->in_cheevos )
  {
    switch ( hash )
    {
    case 0x005973f2U: /* ID          */ ud->field = &ud->id;       break;
    case 0x1e76b53fU: /* MemAddr     */ ud->field = &ud->memaddr;  break;
    case 0x0e2a9a07U: /* Title       */ ud->field = &ud->title;    break;
    case 0xe61a1f69U: /* Description */ ud->field = &ud->desc;     break;
    case 0xca8fce22U: /* Points      */ ud->field = &ud->points;   break;
    case 0xa804edb8U: /* Author      */ ud->field = &ud->author;   break;
    case 0xdcea4fe6U: /* Modified    */ ud->field = &ud->modified; break;
    case 0x3a84721dU: /* Created     */ ud->field = &ud->created;  break;
    case 0x887685d9U: /* BadgeName   */ ud->field = &ud->badge;    break;
    case 0x0d2e96b2U: /* Flags       */ ud->field = &ud->flags;    break;
    }
  }

  return 0;
}

static int read__json_string( void* userdata, const char* string, size_t length )
{
  readud_t* ud = (readud_t*)userdata;

  if ( ud->field )
  {
    ud->field->string = string;
    ud->field->length = length;
  }

  return 0;
}

static int read__json_number( void* userdata, const char* number, size_t length )
{
  readud_t* ud = (readud_t*)userdata;

  if ( ud->field )
  {
    ud->field->string = number;
    ud->field->length = length;
  }

  return 0;
}

static int read__json_end_object( void* userdata )
{
  readud_t* ud = (readud_t*)userdata;

  if ( ud->in_cheevos )
  {
    return new_cheevo( ud );
  }

  return 0;
}

static int read__json_end_array( void* userdata )
{
  readud_t* ud = (readud_t*)userdata;
  ud->in_cheevos = 0;
  return 0;
}

int cheevos_load( const char* json )
{
  static const jsonsax_handlers_t handlers =
  {
    NULL,
    NULL,
    NULL,
    read__json_end_object,
    NULL,
    read__json_end_array,
    read__json_key,
    NULL,
    read__json_string,
    read__json_number,
    NULL,
    NULL
  };

  /* Count the number of achievements in the JSON file. */

  unsigned core_count, unofficial_count;

  if ( count_cheevos( json, &core_count, &unofficial_count ) != JSONSAX_OK )
  {
    return -1;
  }

  /* Allocate the achievements. */

  core_cheevos.cheevos = (cheevo_t*)malloc( core_count * sizeof( cheevo_t ) );
  core_cheevos.count = core_count;

  unofficial_cheevos.cheevos = (cheevo_t*)malloc( unofficial_count * sizeof( cheevo_t ) );
  unofficial_cheevos.count = unofficial_count;

  if ( !core_cheevos.cheevos || !unofficial_cheevos.cheevos )
  {
    free( (void*)core_cheevos.cheevos );
    free( (void*)unofficial_cheevos.cheevos );

    return -1;
  }

  /* Load the achievements. */

  readud_t ud;
  ud.in_cheevos = 0;
  ud.field = NULL;
  ud.core_count = 0;
  ud.unofficial_count = 0;

  if ( jsonsax_parse( json, &handlers, (void*)&ud ) == JSONSAX_OK )
  {
    return 0;
  }

  cheevos_unload();
  return -1;
}

/*****************************************************************************
Test all the achievements (call once per frame).
*****************************************************************************/

static unsigned get_var_value( var_t* var )
{
  unsigned previous = var->previous;
  unsigned live_val = 0;
  uint8_t* memory;

  if ( var->type == VAR_TYPE_VALUE_COMP )
  {
    return var->value;
  }

  if ( var->type == VAR_TYPE_ADDRESS || var->type == VAR_TYPE_DELTA_MEM )
  {
    /* TODO Check with Scott if the bank id is needed */
    /* memory = (uint8_t*)core.retro_get_memory_data( var->bank_id ); */
    memory = (uint8_t*)core.retro_get_memory_data( RETRO_MEMORY_SYSTEM_RAM );
    live_val = memory[ var->value ];

    if ( var->size >= VAR_SIZE_BIT_0 && var->size <= VAR_SIZE_BIT_7 )
    {
      live_val = ( live_val & ( 1 << ( var->size - VAR_SIZE_BIT_0 ) ) ) != 0;
    }
    else if ( var->size == VAR_SIZE_NIBBLE_LOWER )
    {
      live_val &= 0x0f;
    }
    else if ( var->size == VAR_SIZE_NIBBLE_UPPER )
    {
      live_val = ( live_val >> 4 ) & 0x0f;
    }
    else if ( var->size == VAR_SIZE_EIGHT_BITS )
    {
      /* nothing */
    }
    else if ( var->size == VAR_SIZE_SIXTEEN_BITS )
    {
      live_val |= memory[ var->value + 1 ] << 8;
    }
    else if ( var->size == VAR_SIZE_THIRTYTWO_BITS )
    {
      live_val |= memory[ var->value + 1 ] << 8;
      live_val |= memory[ var->value + 2 ] << 16;
      live_val |= memory[ var->value + 3 ] << 24;
    }

    if ( var->type == VAR_TYPE_DELTA_MEM )
    {
      var->previous = live_val;
      return previous;
    }

    return live_val;
  }

  /* We shouldn't get here... */
  return 0;
}

static int test_condition( cheevos_cond_t* cond )
{
  unsigned sval = get_var_value( &cond->source );
  unsigned tval = get_var_value( &cond->target );

  switch ( cond->op )
  {
  case COND_OP_EQUALS:
    return sval == tval;

  case COND_OP_LESS_THAN:
    return sval < tval;

  case COND_OP_LESS_THAN_OR_EQUAL:
    return sval <= tval;

  case COND_OP_GREATER_THAN:
    return sval > tval;

  case COND_OP_GREATER_THAN_OR_EQUAL:
    return sval >= tval;

  case COND_OP_NOT_EQUAL_TO:
    return sval != tval;

  default:
    return 1;
  }
}

static int test_cond_set( const condset_t* condset, int* dirty_conds, int* reset_read, int match_any )
{
  int cond_valid = 0;
  int set_valid = 1;
  int pause_active = 0;
  const cheevos_cond_t* end = condset->conds + condset->count;
  cheevos_cond_t* cond;

  /* Now, read all Pause conditions, and if any are true, do not process further (retain old state) */
  for ( cond = condset->conds; cond < end; cond++ )
  {
    if ( cond->type == COND_TYPE_PAUSE_IF )
    {
      /* Reset by default, set to 1 if hit! */
      cond->curr_hits = 0;

      if ( test_condition( cond ) )
      {
        cond->curr_hits = 1;
        *dirty_conds = 1;

        /* Early out: this achievement is paused, do not process any further! */
        return 0;
      }
    }
  }

  /* Read all standard conditions, and process as normal: */
  for ( cond = condset->conds; cond < end; cond++ )
  {
    if ( cond->type == COND_TYPE_PAUSE_IF || cond->type == COND_TYPE_RESET_IF )
    {
      continue;
    }

    if ( cond->req_hits != 0 && cond->curr_hits >= cond->req_hits )
    {
      continue;
    }

    cond_valid = test_condition( cond );

    if ( cond_valid )
    {
      cond->curr_hits++;
      *dirty_conds = 1;

      /* Process this logic, if this condition is true: */
      if ( cond->req_hits == 0 )
      {
        /* Not a hit-based requirement: ignore any additional logic! */
      }
      else if ( cond->curr_hits >= cond->req_hits )
      {
        /* Not entirely valid yet! */
        cond_valid = 0;
      }

      if ( match_any )
      {
        break;
      }
    }

    /* Sequential or non-sequential? */
    set_valid &= cond_valid;
  }

  /* Now, ONLY read reset conditions! */
  for ( cond = condset->conds; cond < end; cond++ )
  {
    if ( cond->type == COND_TYPE_RESET_IF )
    {
      cond_valid = test_condition( cond );

      if ( cond_valid )
      {
        *reset_read = 1; /* Resets all hits found so far */
        set_valid = 0;   /* Cannot be valid if we've hit a reset condition. */
        break;           /* No point processing any further reset conditions. */
      }
    }
  }

  return set_valid;
}

static int reset_cond_set( condset_t* condset, int deltas )
{
  int dirty = 0;
  const cheevos_cond_t* end = condset->conds + condset->count;
  cheevos_cond_t* cond;

  if ( deltas )
  {
    for ( cond = condset->conds; cond < end; cond++ )
    {
      dirty |= cond->curr_hits != 0;
      cond->curr_hits = 0;

      cond->source.previous = cond->source.value;
      cond->target.previous = cond->target.value;
    }
  }
  else
  {
    for ( cond = condset->conds; cond < end; cond++ )
    {
      dirty |= cond->curr_hits != 0;
      cond->curr_hits = 0;
    }
  }

  return dirty;
}

static int test_cheevo( cheevo_t* cheevo )
{
  int dirty_conds = 0;
  int reset_read = 0;
  int ret_val = 0;
  int dirty;
  int ret_val_sub_cond = cheevo->count == 1;
  condset_t* condset = cheevo->condsets;
  const condset_t* end = condset + cheevo->count;

  if ( condset < end )
  {
    ret_val = test_cond_set( condset, &dirty_conds, &reset_read, 0 );
    if ( ret_val ) RARCH_LOG( "%s\n", condset->expression );
    condset++;
  }

  while ( condset < end )
  {
    int res = test_cond_set( condset, &dirty_conds, &reset_read, 0 );
    ret_val_sub_cond |= res;
    if ( res ) RARCH_LOG( "%s\n", condset->expression );
    condset++;
  }

  if ( dirty_conds )
  {
    cheevo->dirty |= dirty_conds;
  }

  if ( reset_read )
  {
    dirty = 0;

    for ( condset = cheevo->condsets; condset < end; condset++ )
    {
      dirty |= reset_cond_set( condset, 0 );
    }

    if ( dirty )
    {
      cheevo->dirty |= DIRTY_CONDITIONS;
    }
  }

  return ret_val && ret_val_sub_cond;
}

static void test_cheevo_set( const cheevoset_t* set )
{
  const cheevo_t* end = set->cheevos + set->count;
  cheevo_t* cheevo;

  for ( cheevo = set->cheevos; cheevo < end; cheevo++ )
  {
    if ( cheevo->active && test_cheevo( cheevo ) )
    {
      RARCH_LOG( "ACHIEVEMENT! %s\n", cheevo->title );
      RARCH_LOG( "ACHIEVEMENT! %s\n", cheevo->description );
      cheevo->active = 0;
    }
  }
}

void cheevos_test( void )
{
#ifdef HAVE_SMW_CHEEVOS
  static int init = 1;

  if ( init )
  {
    cheevos_load( smw_json );
    init = 0;
  }
#endif

  if ( cheevos_config.enable )
  {
    test_cheevo_set( &core_cheevos );

    if ( cheevos_config.test_unofficial )
    {
      test_cheevo_set( &unofficial_cheevos );
    }
  }
}

/*****************************************************************************
Free the loaded achievements.
*****************************************************************************/

static void free_condset( const condset_t* set )
{
  free( (void*)set->conds );
}

static void free_cheevo( const cheevo_t* cheevo )
{
  free( (void*)cheevo->title );
  free( (void*)cheevo->description );
  free( (void*)cheevo->author );
  free( (void*)cheevo->badge );
  free_condset( cheevo->condsets );
  free( (void*)cheevo );
}

static void free_cheevo_set( const cheevoset_t* set )
{
  const cheevo_t* cheevo = set->cheevos;
  const cheevo_t* end = cheevo + set->count;

  while ( cheevo < end )
  {
    free_cheevo( cheevo++ );
  }

  free( (void*)set->cheevos );
}

void cheevos_unload( void )
{
  free_cheevo_set( &core_cheevos );
  free_cheevo_set( &unofficial_cheevos );
}

#else /* HAVE_CHEEVOS */

int cheevos_load( const char* json )
{
  return -1;
}

void cheevos_test( void )
{
}

void cheevos_unload( void )
{
}

#endif /* HAVE_CHEEVOS */
