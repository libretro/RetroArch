/** This code is licensed to you under the terms of the GNU GPL, version 2;
    see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt */

/****************************************************************************
* SSARAM
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gccore.h>

#include "ssaram.h"

static char aramfix[2048] ATTRIBUTE_ALIGN(32);

#define ARAMSTART  0x8000
#define ARAM_READ  1
#define ARAM_WRITE 0

/****************************************************************************
* ARAMPut
*
* This version of ARAMPut fixes up those segments which are misaligned.
* The IPL rules for DOLs is that they must contain 32byte aligned sections with
* 32byte aligned lengths.
*
* The reality is that most homebrew DOLs don't adhere to this.
****************************************************************************/
void ARAMPut(char *src, char *dst, int len)
{
  u32 misalignaddress;
  u32 misalignedbytes;
  u32 misalignedbytestodo;

  int i, block;
  int offset = 0;

  /*** Check destination alignment ***/
  if ((u32) dst & 0x1f)
  {
    /*** Retrieve previous 32 byte section ***/
    misalignaddress = ((u32) dst & ~0x1f);
    misalignedbytestodo = 32 - ((u32) dst & 0x1f);
    misalignedbytes = ((u32) dst & 0x1f);
    ARAMFetch(aramfix, (char *) misalignaddress, 32);

    /*** Update from source ***/
    memcpy(aramfix + misalignedbytes, src, misalignedbytestodo);

    /*** Put it back ***/
    DCFlushRange(aramfix, 32);
    AR_StartDMA(ARAM_WRITE, (u32) aramfix, (u32) dst & ~0x1f, 32);
    while (AR_GetDMAStatus());

    /*** Update pointers ***/
    src += misalignedbytestodo;
    len -= misalignedbytestodo;
    dst = (char *) (((u32) dst & ~0x1f) + 32);
  }

  /*** Move 2k blocks - saves aligning source buffer ***/
  block = (len >> 11);
  for (i = 0; i < block; i++)
  {
    memcpy(aramfix, src + offset, 2048);
    DCFlushRange(aramfix, 2048);
    AR_StartDMA(ARAM_WRITE, (u32) aramfix, (u32) dst + offset, 2048);
    while (AR_GetDMAStatus());
    offset += 2048;
  }

  /*** Clean up remainder ***/
  len &= 0x7ff;
  if (len)
  {
    block = len & 0x1f;		/*** Is length aligned ? ***/
    memcpy(aramfix, src + offset, len & ~0x1f);
    DCFlushRange(aramfix, len & ~0x1f);
    AR_StartDMA(ARAM_WRITE, (u32) aramfix, (u32) dst + offset, len & ~0x1f);
    while (AR_GetDMAStatus());

    if (block)
    {
      offset += len & ~0x1f;
      misalignedbytes = len & 0x1f;

      /*** Do same shuffle as destination alignment ***/
      ARAMFetch(aramfix, dst + offset, 32);
      memcpy(aramfix, src + offset, misalignedbytes);
      DCFlushRange(aramfix, 32);
      AR_StartDMA(ARAM_WRITE, (u32) aramfix, (u32) dst + offset, 32);
      while (AR_GetDMAStatus());
    }
  }
}

/****************************************************************************
* ARAMFetch
****************************************************************************/
void ARAMFetch(char *dst, char *src, int len)
{
    DCInvalidateRange(dst, len);
    AR_StartDMA(ARAM_READ, (u32) dst, (u32) src, len);
    while (AR_GetDMAStatus());
}
