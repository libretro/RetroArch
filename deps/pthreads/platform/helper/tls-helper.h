/*
 * tls-helper.h
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

#ifndef _TLS_HELPER_H_
#define _TLS_HELPER_H_

#include "../psp/pte_osal.h"

/* @todo document.. */

pte_osResult pteTlsGlobalInit(int maxEntries);
void * pteTlsThreadInit(void);

pte_osResult pteTlsAlloc(unsigned int *pKey);
void * pteTlsGetValue(void *pTlsThreadStruct, unsigned int index);
pte_osResult pteTlsSetValue(void *pTlsThreadStruct, unsigned int index, void * value);
pte_osResult pteTlsFree(unsigned int index);

void pteTlsThreadDestroy(void * pTlsThreadStruct);
void pteTlsGlobalDestroy(void);

#endif // _TLS_HELPER_H_
