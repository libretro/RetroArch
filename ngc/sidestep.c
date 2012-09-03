/** This code is licensed to you under the terms of the GNU GPL, version 2;
    see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt */

/****************************************************************************
* SideStep DOL Loading
*
* This module runs a DOL file from Auxilliary RAM. This removes any memory
* issues which might occur - and also means you can easily overwrite yourself!
*
* softdev March 2007
***************************************************************************/
#ifndef HW_RVL
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <network.h>
#include <smb.h>

#include "sidestep.h"
#include "ssaram.h"
#include "../retroarch_logger.h"

#define ARAMSTART 0x8000

/*** A global or two ***/
static DOLHEADER *dolhdr;
static u32 minaddress = 0;
static u32 maxaddress = 0;
static char dol_readbuf[2048];

typedef int (*BOOTSTUB) (u32 entrypoint, u32 dst, u32 src, int len, u32 invlen, u32 invaddress);

/*--- Auxilliary RAM Support ----------------------------------------------*/
/****************************************************************************
* ARAMStub
*
* This is an assembly routine and should only be called through ARAMRun
* *DO NOT CALL DIRECTLY!*
****************************************************************************/
static void ARAMStub(void)
{
	/*** The routine expects to receive
             R3 = entrypoint
             R4 = Destination in main RAM
             R5 = Source from ARAM
             R6 = Data length
             R7 = Invalidate Length / 32
             R8 = Invalidate Start Address
        ***/

    asm("mtctr 7");
    asm("Invalidate:");
    asm("dcbi 0,8");
    asm("addi 8,8,32");
    asm("bdnz Invalidate");

    asm("lis 8,0xcc00");
    asm("ori 8,8,0x3004");
    asm("lis 7,0");
    asm("stw 7,0(8)");

    asm("mfmsr 8");
    asm("ori 8,8,2");
    asm("rlwinm 8,8,0,17,15");
    asm("mtmsr 8");

    asm("lis  7,0xcc00");
    asm("ori 7,7,0x5020");
    asm("stw 4,0(7)");		/*** Store Memory Address ***/
    asm("stw 5,4(7)");		/*** Store ARAM Address ***/
    asm("stw 6,8(7)");		/*** Store Length ***/

    asm("lis  7,0xcc00");
    asm("ori 7,7,0x500a");
    asm("WaitDMA:");
    asm("lhz 5,0(7)");
    asm("andi. 5,5,0x200");
    asm("cmpwi 5,5,0");
    asm("bne WaitDMA");		/*** Wait DMA Complete ***/

    /*** Update exceptions ***/
    asm("lis 8,0x8000");
    asm("lis 5,0x4c00");
    asm("ori 5,5,0x64");
    asm("stw 5,0x100(8)");
    asm("stw 5,0x200(8)");
    asm("stw 5,0x300(8)");
    asm("stw 5,0x400(8)");
    asm("stw 5,0x500(8)");
    asm("stw 5,0x600(8)");
    asm("stw 5,0x700(8)");
    asm("stw 5,0x800(8)");
    asm("stw 5,0x900(8)");
    asm("stw 5,0xC00(8)");
    asm("stw 5,0xD00(8)");
    asm("stw 5,0xF00(8)");
    asm("stw 5,0x1300(8)");
    asm("stw 5,0x1400(8)");
    asm("stw 5,0x1700(8)");

    /*** Flush it all again ***/
    asm("lis 7,0x30");
    asm("lis 8,0x8000");
    asm("mtctr 7");
    asm("flush:");
    asm("dcbst 0,8");
    asm("sync");
    asm("icbi 0,8");
    asm("addi 8,8,8");
    asm("bdnz flush");
    asm("isync");

    /*** Fix ints ***/
    asm("mfmsr 8");
    asm("rlwinm 8,8,0,17,15");
    asm("mtmsr 8");

    asm("mfmsr 8");
    asm("ori 8,8,8194");
    asm("mtmsr 8");

    /*** Party! ***/
    asm("mtlr 3");
    asm("blr");			/*** Boot DOL ***/

}

/****************************************************************************
* ARAMRun
*
* This actually runs the new DOL ... eventually ;)
****************************************************************************/
void ARAMRun(u32 entrypoint, u32 dst, u32 src, u32 len)
{
  char *p;
  char *s = (char *) ARAMStub;
  BOOTSTUB stub;

  /*** Shutdown libOGC ***/
  SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);

  /*** Copy ARAMStub to 81300000 ***/
  if (dst + len < 0x81300000) p = (void *) 0x81300000;
  else p = (void *) 0x80003100;
  memcpy(p, s, 256); /*** Way too much - but who cares ***/

  /*** Round length to 32 bytes ***/
  if (len & 0x1f) len = (len & ~0x1f) + 0x20;
 
  /*** Flush everything! ***/
  DCFlushRange((void *) 0x80000000, 0x1800000);

  /*** Boot the bugger :D ***/
  stub = (BOOTSTUB) p;
  RARCH_LOG("Launching relocated stub at %08x\n", (unsigned) p);
  stub((u32) entrypoint, dst, src, len | 0x80000000, len >> 5, dst);
}

/****************************************************************************
* ARAMClear
*
* To make life easy, just clear out the Auxilliary RAM completely.
****************************************************************************/
static void ARAMClear(void)
{
  int i;
  char *buffer = memalign(32, 2048); /*** A little 2k buffer ***/

  memset(buffer, 0, 2048);
  DCFlushRange(buffer, 2048);

  for (i = ARAMSTART; i < 0x1000000; i += 2048)
  {
    ARAMPut(buffer, (char *) i, 2048);
    while (AR_GetDMAStatus());
  }

  free(buffer);
}

/*--- DOL Decoding functions -----------------------------------------------*/
/****************************************************************************
* DOLMinMax
*
* Calculate the DOL minimum and maximum memory addresses
****************************************************************************/
static void DOLMinMax(DOLHEADER * dol)
{
  int i;

  maxaddress = 0;
  minaddress = 0x87100000;

  /*** Go through DOL sections ***/
  /*** Text sections ***/
  for (i = 0; i < MAXTEXTSECTION; i++)
  {
    if (dol->textAddress[i] && dol->textLength[i])
    {
      if (dol->textAddress[i] < minaddress)
        minaddress = dol->textAddress[i];
      if ((dol->textAddress[i] + dol->textLength[i]) > maxaddress) 
        maxaddress = dol->textAddress[i] + dol->textLength[i];
    }
  }

  /*** Data sections ***/
  for (i = 0; i < MAXDATASECTION; i++)
  {
    if (dol->dataAddress[i] && dol->dataLength[i])
    {
      if (dol->dataAddress[i] < minaddress)
        minaddress = dol->dataAddress[i];
      if ((dol->dataAddress[i] + dol->dataLength[i]) > maxaddress)
        maxaddress = dol->dataAddress[i] + dol->dataLength[i];
    }
  }

  /*** And of course, any BSS section ***/
  if (dol->bssAddress)
  {
    if ((dol->bssAddress + dol->bssLength) > maxaddress)
      maxaddress = dol->bssAddress + dol->bssLength;
  }

  /*** Some OLD dols, Xrick in particular, require ~128k clear memory ***/
  maxaddress += 0x20000;

  RARCH_LOG("Min Address: %08x     Max Address: %08x\n", minaddress, maxaddress);
}

/****************************************************************************
* DOLtoARAM
*
* Moves the DOL from main memory to ARAM, positioning as it goes
*
* Pass in a memory pointer to a previously loaded DOL
****************************************************************************/
int DOLtoARAM(const char *dol_name)
{
  u32 sizeinbytes;
  int i, j;
  static DOLHEADER dolhead;
  FILE *f = fopen(dol_name, "rb");
  
  if (!f)
  {
    RARCH_ERR("Could not open\"%s\"\n", dol_name);
    return 0;
  }

  fread(&dolhead, 1, sizeof(DOLHEADER), f);
  /*** Make sure ARAM subsystem is alive! ***/
  AR_Init(NULL, 0); /*** No stack - we need it all ***/
  ARAMClear();

  /*** Get DOL header ***/
  dolhdr = (DOLHEADER *) &dolhead;

  /*** First, does this look like a DOL? ***/
  if (dolhdr->textOffset[0] != DOLHDRLENGTH)
  {
    RARCH_ERR("\"%s\" is not a .dol file\n", dol_name);
    return 0;
  }

  /*** Get DOL stats ***/
  DOLMinMax(dolhdr);
  sizeinbytes = maxaddress - minaddress;

  /*** Move all DOL sections into ARAM ***/
  /*** Move text sections ***/
  for (i = 0; i < MAXTEXTSECTION; i++)
  {
    /*** This may seem strange, but in developing d0lLZ we found some with section addresses with zero length ***/
    if (dolhdr->textAddress[i] && dolhdr->textLength[i])
    {
      fseek(f, dolhdr->textOffset[i], SEEK_SET);
      unsigned count = dolhdr->textLength[i] / sizeof(dol_readbuf);
      for (j = 0; j < count; j++)
      {
        fread(dol_readbuf, 1, sizeof(dol_readbuf), f);
        ARAMPut(dol_readbuf, (char *) ((dolhdr->textAddress[i] - minaddress) + (sizeof(dol_readbuf) * j) + ARAMSTART),
                sizeof(dol_readbuf));
      }
      unsigned remaining = dolhdr->textLength[i] % sizeof(dol_readbuf);
      if (remaining)
      {
        fread(dol_readbuf, 1, remaining, f);
        ARAMPut(dol_readbuf, (char *) ((dolhdr->textAddress[i] - minaddress) + (sizeof(dol_readbuf) * count) + ARAMSTART),
                remaining);
      }
    }
  }

  /*** Move data sections ***/
  for (i = 0; i < MAXDATASECTION; i++)
  {
    if (dolhdr->dataAddress[i] && dolhdr->dataLength[i])
    {
      fseek(f, dolhdr->dataOffset[i], SEEK_SET);
      unsigned count = dolhdr->dataLength[i] / sizeof(dol_readbuf);
      for (j = 0; j < count; j++)
      {
        fread(dol_readbuf, 1, sizeof(dol_readbuf), f);
        ARAMPut(dol_readbuf, (char *) ((dolhdr->dataAddress[i] - minaddress) + (sizeof(dol_readbuf) * j) + ARAMSTART),
                sizeof(dol_readbuf));
      }
      unsigned remaining = dolhdr->dataLength[i] % sizeof(dol_readbuf);
      if (remaining)
      {
        fread(dol_readbuf, 1, remaining, f);
        ARAMPut(dol_readbuf, (char *) ((dolhdr->dataAddress[i] - minaddress) + (sizeof(dol_readbuf) * count) + ARAMSTART),
                remaining);
      }
    }
  }

  fclose(f);

  /*** Now go run it ***/
  ARAMRun(dolhdr->entryPoint, minaddress, ARAMSTART, sizeinbytes);

  /*** Will never return ***/
  return 1;
}
#endif
