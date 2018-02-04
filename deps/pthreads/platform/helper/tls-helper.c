/*
 * tls-helper.c
 *
 * Description:
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads-embedded (PTE) - POSIX Threads Library for embedded systems
 *      Copyright(C) 2008 Jason Schmidlapp
 *
 *      Contact Email: jschmidlapp@users.sourceforge.net
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <stdio.h>
#include <stdlib.h>

#include "tls-helper.h"

static int *keysUsed;

/* We don't protect this - it's only written on startup */
static int maxTlsValues;

pte_osMutexHandle globalTlsLock;

pte_osResult pteTlsGlobalInit(int maxEntries)
{
   int i;
   pte_osResult result;

   pte_osMutexCreate(&globalTlsLock);

   keysUsed = (int *) malloc(maxEntries * sizeof(int));

   if (keysUsed != NULL)
   {
      for (i=0;i<maxEntries;i++)
         keysUsed[i] = 0;

      maxTlsValues = maxEntries;

      result = PTE_OS_OK;
   }
   else
      result = PTE_OS_NO_RESOURCES;

   return result;
}


void * pteTlsThreadInit(void)
{
   int i;
   void **pTlsStruct = (void **) malloc(maxTlsValues * sizeof(void*));

   /* PTE library assumes that keys are initialized to zero */
   for (i=0; i<maxTlsValues;i++)
      pTlsStruct[i] = 0;

   return (void *) pTlsStruct;
}


pte_osResult pteTlsAlloc(unsigned int *pKey)
{
   int i;
   pte_osResult result = PTE_OS_NO_RESOURCES;

   pte_osMutexLock(globalTlsLock);

   for (i=0;i<maxTlsValues;i++)
   {
      if (keysUsed[i] == 0)
      {
         keysUsed[i] = 1;

         *pKey = i+1;
         result = PTE_OS_OK;
         break;
      }
   }

   pte_osMutexUnlock(globalTlsLock);

   return result;
}


void * pteTlsGetValue(void *pTlsThreadStruct, unsigned int index)
{
   void **pTls = (void **) pTlsThreadStruct;

   if (keysUsed[index-1])
   {
      if (pTls != NULL)
         return pTls[index-1];
   }
   return NULL;
}


pte_osResult pteTlsSetValue(void *pTlsThreadStruct, unsigned int index, void * value)
{
   pte_osResult result;
   void ** pTls = (void **) pTlsThreadStruct;

   if (pTls != NULL)
   {
      pTls[index-1] = value;
      result = PTE_OS_OK;
   }
   else
      result = PTE_OS_INVALID_PARAM;

   return result;

}

pte_osResult pteTlsFree(unsigned int index)
{
   pte_osResult result;

   if (keysUsed != NULL)
   {
      pte_osMutexLock(globalTlsLock);

      keysUsed[index-1] = 0;

      pte_osMutexUnlock(globalTlsLock);

      result = PTE_OS_OK;
   }
   else
      result = PTE_OS_GENERAL_FAILURE;

   return result;
}

void pteTlsThreadDestroy(void * pTlsThreadStruct)
{
  free(pTlsThreadStruct);
}

void pteTlsGlobalDestroy(void)
{
  pte_osMutexDelete(globalTlsLock);
  free(keysUsed);
}
